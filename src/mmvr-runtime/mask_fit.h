#pragma once
#include "projection.h"
namespace mmvr {
inline Matrix CaptainMaskOrientation() {
    return Multiply(PoseMatrix({{.707106781f,0,0,.707106781f},{0,0,0}}),
                    PoseMatrix({{0,1,0,0},{0,0,0}}));
}
inline XrPosef CaptainMaskWearContact(XrPosef grip, float size) {
    // Skull-face center minus the corrected hood-base anchor, in native units.
    // Track the visible face during wearing; worn-mask retrieval keeps its slot.
    const auto pose=Multiply(CaptainMaskOrientation(),PoseMatrix(grip));
    const float scale=size/54.f;
    grip.position.x+=(22*pose.m[1][0]+25*pose.m[2][0])*scale;
    grip.position.y+=(22*pose.m[1][1]+25*pose.m[2][1])*scale;
    grip.position.z+=(22*pose.m[1][2]+25*pose.m[2][2])*scale;
    return grip;
}
}
