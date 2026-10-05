#pragma once
// Private, one-shot comparison of the actual native cue display list. The
// caller supplies the same eye/camera state for both replays; no test geometry
// or alternate material is created here.
#include "native_bounds_dx11_test.h"
#include "fairy_mask_cue.h"
#include <array>
#include <fstream>
#include <functional>
#include <limits>

namespace mmvr {
struct NativeFairyCuePixelRegion {
    int left = 0, top = 0, right = 0, bottom = 0;
    bool Contains(unsigned x, unsigned y) const {
        return int(x) >= left && int(x) < right && int(y) >= top && int(y) < bottom;
    }
};

inline NativeFairyCuePixelRegion NativeFairyCueRegion(const FairyMaskCuePoint& point,
                                                     const XrView& eye, const XrPosef& head,
                                                     unsigned width, unsigned height) {
    if (!point.valid) throw std::runtime_error("Missing binocular cue point");
    const auto toEye = Multiply(PoseMatrix(head), InversePose(PoseMatrix(eye.pose)));
    const float l = std::tan(eye.fov.angleLeft), r = std::tan(eye.fov.angleRight);
    const float d = std::tan(eye.fov.angleDown), u = std::tan(eye.fov.angleUp);
    float lowX = std::numeric_limits<float>::infinity(), lowY = lowX;
    float highX = -lowX, highY = -lowX;
    for (float x : {-FairyMaskCueHalfSize, FairyMaskCueHalfSize})
        for (float y : {-FairyMaskCueHalfSize, FairyMaskCueHalfSize}) {
            const XrVector3f p{point.position.x + x, point.position.y + y, point.position.z};
            float q[3]{};
            for (int c = 0; c < 3; ++c)
                q[c] = p.x*toEye.m[0][c] + p.y*toEye.m[1][c] + p.z*toEye.m[2][c] + toEye.m[3][c];
            if (!(q[2] < -.05f)) throw std::runtime_error("Cue pixel region behind eye");
            const float px = (q[0]/-q[2] - l)/(r-l)*width;
            const float py = (u - q[1]/-q[2])/(u-d)*height;
            if (!std::isfinite(px) || !std::isfinite(py)) throw std::runtime_error("Invalid cue pixel region");
            lowX = std::min(lowX, px); highX = std::max(highX, px);
            lowY = std::min(lowY, py); highY = std::max(highY, py);
        }
    // Include raster edge/AA coverage for each expected sparkle region.
    // The oracle still compares actual RGB, not projected geometry.
    NativeFairyCuePixelRegion region{
        std::max(0, int(std::floor(lowX))-3), std::max(0, int(std::floor(lowY))-3),
        std::min(int(width), int(std::ceil(highX))+3), std::min(int(height), int(std::ceil(highY))+3)};
    if (region.left >= region.right || region.top >= region.bottom)
        throw std::runtime_error("Cue pixel region outside framebuffer");
    return region;
}

inline void NativeFairyCueSaveImage(const std::string& path, unsigned width, unsigned height,
                                    const std::vector<unsigned char>& pixels) {
    std::ofstream image(path, std::ios::binary | std::ios::trunc);
    image << "P6\n" << width << ' ' << height << "\n255\n";
    for (size_t i = 0; i < pixels.size(); i += 4)
        image.write(reinterpret_cast<const char*>(pixels.data()+i), 3);
    if (!image) throw std::runtime_error("Cannot save cue comparison image");
}

inline void CaptureNativeFairyCuePixelsDX11(ID3D11Device* device, ID3D11DeviceContext* context,
                                           ID3D11Texture2D* texture, const std::function<void(bool)>& draw,
                                           const std::function<void(int, bool)>& prepare,
                                           const std::array<XrView, 2>& eyes, const XrPosef& head,
                                           const std::array<FairyMaskCuePoint, FairyMaskCueCount>& points,
                                           const std::string& label, unsigned bindings, unsigned cueMask) {
    struct EyeResult {
        size_t changed = 0, visible = 0, centralChanged = 0, outsideChanged = 0, worldNonUniform = 0;
        std::array<size_t, FairyMaskCueCount> regionChanged{}, regionVisible{};
        std::array<NativeFairyCuePixelRegion, FairyMaskCueCount> regions{};
        bool passed = false;
    };
    std::array<EyeResult, 2> results{};
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    bool passed = false;
    const char* reason = "capture-error";
    try {
        if (!draw || bindings != FairyMaskCueCount || cueMask != ((1u << FairyMaskCueCount)-1))
            throw std::runtime_error("Current native display list lacks the full unique cue binding set");
        if (!ColorFamily(desc.Format) || desc.Width < 160 || desc.Height < 120 ||
            desc.Width > 8192 || desc.Height > 8192 || size_t(desc.Width)*desc.Height > 16*1024*1024 ||
            desc.ArraySize != 1 || desc.MipLevels != 1)
            throw std::runtime_error("Cue comparison requires a real game framebuffer");
        for (int eye = 0; eye < 2; ++eye) {
            auto& result = results[eye];
            for (int cue = 0; cue < FairyMaskCueCount; ++cue)
                result.regions[cue] = NativeFairyCueRegion(points[cue], eyes[eye], head, desc.Width, desc.Height);
            prepare(eye, false);
            draw(false);
            const auto baseline = NativeBoundsReadPixelsDX11(device, context, texture);
            prepare(eye, true);
            draw(false);
            const auto candidate = NativeBoundsReadPixelsDX11(device, context, texture);
            if (baseline.size() != candidate.size() || baseline.size() != size_t(desc.Width)*desc.Height*4)
                throw std::runtime_error("Cue comparison image size changed");
            const auto prefix = label + "-eye" + std::to_string(eye);
            NativeFairyCueSaveImage(prefix+"-baseline.ppm", desc.Width, desc.Height, baseline);
            NativeFairyCueSaveImage(prefix+"-candidate.ppm", desc.Width, desc.Height, candidate);
            std::vector<unsigned char> difference(candidate.size(), 0);
            const NativeFairyCuePixelRegion center{int(desc.Width*3/10), int(desc.Height*3/10),
                                                   int(desc.Width*7/10), int(desc.Height*7/10)};
            for (unsigned y = 0; y < desc.Height; ++y) for (unsigned x = 0; x < desc.Width; ++x) {
                const size_t i = (size_t(y)*desc.Width+x)*4;
                const bool changed = baseline[i] != candidate[i] || baseline[i+1] != candidate[i+1] ||
                                     baseline[i+2] != candidate[i+2];
                const bool visible = changed && (candidate[i] || candidate[i+1] || candidate[i+2]);
                result.worldNonUniform += baseline[i] != baseline[0] || baseline[i+1] != baseline[1] ||
                                          baseline[i+2] != baseline[2];
                if (!changed) continue;
                ++result.changed; result.visible += visible;
                result.centralChanged += center.Contains(x, y);
                bool expected = false;
                for (int cue = 0; cue < FairyMaskCueCount; ++cue) if (result.regions[cue].Contains(x, y)) {
                    ++result.regionChanged[cue]; result.regionVisible[cue] += visible; expected = true;
                }
                result.outsideChanged += !expected;
                for (int c = 0; c < 3; ++c)
                    difference[i+c] = static_cast<unsigned char>(std::abs(int(candidate[i+c])-int(baseline[i+c])));
            }
            NativeFairyCueSaveImage(prefix+"-difference.ppm", desc.Width, desc.Height, difference);
            result.passed = result.changed > 0 && result.visible > 0 && result.centralChanged == 0 &&
                            result.outsideChanged == 0 && result.worldNonUniform > 0;
            for (int cue = 0; cue < FairyMaskCueCount; ++cue)
                result.passed &= result.regionChanged[cue] > 0 && result.regionVisible[cue] > 0;
        }
        passed = results[0].passed && results[1].passed;
        reason = passed ? "completed" : "peripheral-pixels-or-center-check";
    } catch (const std::exception& error) {
        std::ofstream(label+"-error.txt") << error.what() << '\n';
    }
    std::ofstream report(label+".json", std::ios::trunc);
    report << "{\"passed\":" << (passed ? "true" : "false") << ",\"reason\":\"" << reason
           << "\",\"width\":" << desc.Width << ",\"height\":" << desc.Height
           << ",\"nativeBindings\":" << bindings << ",\"cueIndexMask\":" << cueMask << ",\"eyes\":[";
    for (int eye = 0; eye < 2; ++eye) {
        if (eye) report << ',';
        const auto& result = results[eye];
        report << "{\"eye\":" << eye << ",\"passed\":" << (result.passed ? "true" : "false")
               << ",\"changedPixels\":" << result.changed << ",\"changedNonzeroPixels\":" << result.visible
               << ",\"centralChangedPixels\":" << result.centralChanged
               << ",\"outsideExpectedPixels\":" << result.outsideChanged
               << ",\"baselineNonUniformPixels\":" << result.worldNonUniform << ",\"regions\":[";
        for (int cue = 0; cue < FairyMaskCueCount; ++cue) {
            if (cue) report << ',';
            const auto& region = result.regions[cue];
            report << "{\"cue\":" << cue << ",\"left\":" << region.left << ",\"top\":" << region.top
                   << ",\"right\":" << region.right << ",\"bottom\":" << region.bottom
                   << ",\"changedPixels\":" << result.regionChanged[cue]
                   << ",\"changedNonzeroPixels\":" << result.regionVisible[cue] << '}';
        }
        report << "]}";
    }
    report << "]}";
}
} // namespace mmvr
