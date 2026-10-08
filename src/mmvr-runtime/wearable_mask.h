#pragma once
#include "settings.h"
#include <openxr/openxr.h>
#include <array>
#include <string_view>

namespace mmvr {
struct WearableMaskAsset { int item; const char* file; };
// Native ItemId, not PLAYER_MASK or inventory-slot indices. Transformation
// masks and the Giant's Mask deliberately have no wearer-view art.
inline constexpr WearableMaskAsset WearableMaskAssets[]{
    {0x36,"truth"}, {0x37,"kafei-mask"}, {0x38,"all-night"},
    {0x39,"bunny"}, {0x3a,"keaton"}, {0x3b,"garo"},
    {0x3c,"romani"}, {0x3d,"circus"}, {0x3e,"postman"},
    {0x3f,"couple"}, {0x40,"great-fairy"}, {0x41,"gibdo"},
    {0x42,"don-gero"}, {0x43,"kamaro"}, {0x44,"captain"},
    {0x45,"stone"}, {0x46,"bremen"}, {0x47,"blast"}, {0x48,"scents"}
};
inline const WearableMaskAsset* FindWearableMask(int item) {
    for (const auto& asset : WearableMaskAssets) if (asset.item == item) return &asset;
    return nullptr;
}
inline int WearableMaskItem(const Settings& settings, int worn, bool context) {
    return context && settings.Get(Setting::WearableMaskOverlay) > .5f && FindWearableMask(worn) ? worn : -1;
}
// One angular canvas for the binocular field. The same world-facing ray has
// the same UV in either eye, including asymmetric runtime frusta. This is a
// headset-local overlay, with no world-space depth or controller attachment.
inline float WearableMaskAperture(int item) {
    // Preserve the three approved headset fits; bring other rims into view.
    return item < 0 || item == 0x39 || item == 0x3e || item == 0x47 ? 1.f : .78f;
}
inline std::array<float,4> WearableMaskUv(const XrFovf& eye, const XrFovf& left, const XrFovf& right, int item = -1) {
    const float l=std::tan(std::min(left.angleLeft,right.angleLeft));
    const float r=std::tan(std::max(left.angleRight,right.angleRight));
    const float b=std::tan(std::min(left.angleDown,right.angleDown));
    const float t=std::tan(std::max(left.angleUp,right.angleUp));
    if (!std::isfinite(l+r+b+t) || r-l < .001f || t-b < .001f) return {0,0,1,1};
    std::array<float,4> uv{std::clamp((std::tan(eye.angleLeft)-l)/(r-l),0.f,1.f),
            std::clamp((t-std::tan(eye.angleUp))/(t-b),0.f,1.f),
            std::clamp((std::tan(eye.angleRight)-l)/(r-l),0.f,1.f),
            std::clamp((t-std::tan(eye.angleDown))/(t-b),0.f,1.f)};
    for (auto& v : uv) v = .5f + (v-.5f)/WearableMaskAperture(item);
    return uv;
}
} // namespace mmvr
