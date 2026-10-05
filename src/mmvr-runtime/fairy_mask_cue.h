#pragma once
#include "projection.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace mmvr {
// One common point in headset-local metres, visible in both actual eye frusta.
// Do not place a separate sparkle at the same image pixel in each eye: that
// would flatten its depth and disagree on asymmetric or canted headsets.
inline constexpr float FairyMaskCueDepth = .8f;
inline constexpr float FairyMaskCueHalfSize = .018f;
inline constexpr int FairyMaskCueCount = 15;
inline constexpr unsigned FairyMaskCueBindingMask = (1u << FairyMaskCueCount) - 1u;
struct FairyMaskCueDescriptor {
    int side;
    float height, inset;
    double phase, rate; // Cycles and cycles per second.
    uint8_t peak;
};
// Irregular heights/insets keep a loose peripheral scatter. Each point has
// its own fade phase, pace and peak without consuming native gameplay RNG.
inline constexpr std::array<FairyMaskCueDescriptor, FairyMaskCueCount> FairyMaskCueDescriptors = {{
    {-1,-.237f,.103f,.17,.243,112}, {1,.219f,.088f,.63,.317,124},
    {-1,.079f,.117f,.37,.281,104}, {1,-.144f,.096f,.79,.353,118},
    {-1,.226f,.083f,.48,.229,120}, {1,-.009f,.119f,.24,.397,108},
    {-1,-.052f,.092f,.71,.263,116}, {1,.139f,.107f,.42,.337,102},
    {-1,-.174f,.111f,.83,.373,122}, {1,-.224f,.081f,.29,.251,110},
    {-1,.017f,.099f,.56,.419,106}, {1,.061f,.113f,.76,.293,114},
    {-1,.151f,.086f,.22,.359,100}, {1,-.078f,.104f,.68,.277,119},
    {-1,-.109f,.118f,.33,.311,98}
}};
// Retain the existing height catalog for callers transitioning to the common
// descriptor/index mapping.
inline constexpr auto FairyMaskCueHeights = [] {
    std::array<float, FairyMaskCueCount> heights{};
    for (int i=0;i<FairyMaskCueCount;++i) heights[i]=FairyMaskCueDescriptors[i].height;
    return heights;
}();
inline uint8_t FairyMaskCueAlpha(int index, double seconds) {
    if (index<0 || index>=FairyMaskCueCount || !std::isfinite(seconds)) return 0;
    const auto& cue=FairyMaskCueDescriptors[index];
    const double phase=std::fmod(seconds*cue.rate+cue.phase,1.);
    const double pulse=.5-.5*std::cos(6.2831853071795864769*phase);
    return uint8_t(std::clamp(std::lround(cue.peak*pulse*pulse),0l,124l));
}
struct FairyMaskCuePoint {
    XrVector3f position{};
    bool valid = false;
};
inline bool FairyMaskCuePoseValid(const XrPosef& pose) {
    const auto& q = pose.orientation;
    const float length = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    return std::isfinite(length) && std::abs(length-1.f) < .02f &&
           std::isfinite(pose.position.x) && std::isfinite(pose.position.y) && std::isfinite(pose.position.z);
}
inline FairyMaskCuePoint BinocularFairyMaskCuePoint(const std::array<XrView, 2>& eyes,
                                                   const XrPosef& head, int side, float height,
                                                   float inwardInset = .08f) {
    if ((side != -1 && side != 1) || !std::isfinite(height) || !FairyMaskCuePoseValid(head) ||
        !std::isfinite(inwardInset) || inwardInset<0 || inwardInset>.5f) return {};
    float low = -8.f*FairyMaskCueDepth, high = 8.f*FairyMaskCueDepth;
    for (const auto& eye : eyes) {
        const auto& f = eye.fov;
        if (!FairyMaskCuePoseValid(eye.pose) || !std::isfinite(f.angleLeft) || !std::isfinite(f.angleRight) ||
            !std::isfinite(f.angleDown) || !std::isfinite(f.angleUp) ||
            !(f.angleLeft < 0 && f.angleRight > 0 && f.angleDown < 0 && f.angleUp > 0) ||
            f.angleLeft <= -1.55f || f.angleRight >= 1.55f || f.angleDown <= -1.55f || f.angleUp >= 1.55f) return {};
        float l=std::tan(f.angleLeft), r=std::tan(f.angleRight), d=std::tan(f.angleDown), u=std::tan(f.angleUp);
        // Keep the small sprite inside the common frustum instead of clipping
        // one eye. The extra inward offset below avoids edge flicker on turns.
        const float mx=(r-l)*.025f, my=(u-d)*.025f;
        l+=mx; r-=mx; d+=my; u-=my;
        const auto transform=Multiply(PoseMatrix(head), InversePose(PoseMatrix(eye.pose)));
        float a[3], b[3];
        for (int c=0;c<3;++c) {
            a[c]=transform.m[0][c];
            b[c]=height*transform.m[1][c]-FairyMaskCueDepth*transform.m[2][c]+transform.m[3][c];
        }
        // At fixed head-local y/z each frustum plane is A*x+B >= 0.
        // Their intersection gives the exact common horizontal interval in
        // constant work, including IPD, eye translation and canted eye poses.
        const auto& y=transform.m[1];
        const std::array<std::array<float,3>,5> planes={{{a[0]+l*a[2],b[0]+l*b[2],y[0]+l*y[2]},
            {-a[0]-r*a[2],-b[0]-r*b[2],-y[0]-r*y[2]}, {a[1]+d*a[2],b[1]+d*b[2],y[1]+d*y[2]},
            {-a[1]-u*a[2],-b[1]-u*b[2],-y[1]-u*y[2]}, {-a[2],-b[2]-.05f,-y[2]}}};
        for (auto plane : planes) {
            // Constrain all four corners of the head-facing quad, even for
            // narrow or canted views, rather than just its center.
            plane[1]-=FairyMaskCueHalfSize*(std::abs(plane[0])+std::abs(plane[2]));
            if (std::abs(plane[0]) < 1e-6f) { if (plane[1]<0) return {}; }
            else if (plane[0]>0) low=std::max(low,-plane[1]/plane[0]);
            else high=std::min(high,-plane[1]/plane[0]);
        }
    }
    if (!std::isfinite(low) || !std::isfinite(high) || high-low<.04f) return {};
    // The projection edge can extend beyond the headset's visible lens area.
    // Keep the indicator peripheral but noticeably inside that edge.
    const float inset=(high-low)*inwardInset;
    return {{side<0 ? low+inset : high-inset, height, -FairyMaskCueDepth},true};
}
inline FairyMaskCuePoint FairyMaskCuePointForIndex(const std::array<XrView, 2>& eyes,
                                                  const XrPosef& head, int index) {
    if (index<0 || index>=FairyMaskCueCount) return {};
    const auto& cue=FairyMaskCueDescriptors[index];
    return BinocularFairyMaskCuePoint(eyes,head,cue.side,cue.height,cue.inset);
}
} // namespace mmvr
