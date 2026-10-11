#pragma once
#include "first_person.h"
namespace mmvr {
// Change direction only: projectiles still originate at the held item's socket.
inline Matrix HeadAimedPose(Matrix socket, const Matrix& head, bool enabled) {
    if (enabled && head.m[3][3] != 0)
        for (int row=0; row<3; ++row)
            for (int col=0; col<3; ++col) socket.m[row][col]=head.m[row][col];
    return socket;
}

inline XrVector3f CalibrateBowAim(XrVector3f direction, float yawDegrees, float pitchDegrees) {
    const float yaw = std::atan2(direction.x, direction.z) + yawDegrees * .01745329252f;
    const float pitch =
        std::clamp(std::atan2(direction.y, std::hypot(direction.x, direction.z)) + pitchDegrees * .01745329252f,
                   -1.553343f, 1.553343f);
    return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
}
// A short, time-based filter for micro-tremor only. Deliberate turns bypass it.
class BowAimFilter {
    XrVector3f previous{};
    double time = -1;
public:
    void Reset() { time = -1; }
    XrVector3f Update(XrVector3f target, double now, bool active) {
        float dot = previous.x*target.x + previous.y*target.y + previous.z*target.z;
        double dt = now-time;
        if (active && time >= 0 && dt >= 0 && dt < .15 && dot > .9961947f) {
            float a = 1.f-std::exp(-float(dt)/.012f);
            target = {previous.x+(target.x-previous.x)*a,
                      previous.y+(target.y-previous.y)*a,
                      previous.z+(target.z-previous.z)*a};
            float n = std::sqrt(target.x*target.x+target.y*target.y+target.z*target.z);
            if (n > .00001f) { target.x/=n; target.y/=n; target.z/=n; }
        }
        previous=target;time=active?now:-1;
        return target;
    }
};
// Rotate the complete bow-hand mesh about its grip, retaining roll and scale.
// Native bow limbs span X; the shooting axis is +Y. Controller aim and grip
// poses are distinct on OpenXR, so they must not independently drive the mesh.
inline Matrix AlignBowModel(Matrix model, XrVector3f forward) {
    XrVector3f x{model.m[0][0],model.m[0][1],model.m[0][2]};
    float scale[3]{};
    for(int row=0;row<3;++row) scale[row]=std::sqrt(model.m[row][0]*model.m[row][0]+model.m[row][1]*model.m[row][1]+model.m[row][2]*model.m[row][2]);
    float dot=x.x*forward.x+x.y*forward.y+x.z*forward.z;
    x={x.x-dot*forward.x,x.y-dot*forward.y,x.z-dot*forward.z};
    float length=std::sqrt(x.x*x.x+x.y*x.y+x.z*x.z);
    if(length<.000001f) {
        x=std::abs(forward.y)<.9f?XrVector3f{0,1,0}:XrVector3f{1,0,0};
        dot=x.x*forward.x+x.y*forward.y+x.z*forward.z;
        x={x.x-dot*forward.x,x.y-dot*forward.y,x.z-dot*forward.z};
        length=std::sqrt(x.x*x.x+x.y*x.y+x.z*x.z);
    }
    x={x.x/length,x.y/length,x.z/length};
    XrVector3f z{x.y*forward.z-x.z*forward.y,x.z*forward.x-x.x*forward.z,x.x*forward.y-x.y*forward.x};
    
    // Retain authored handedness even for a large direction correction.
    float determinant=(model.m[0][1]*model.m[1][2]-model.m[0][2]*model.m[1][1])*model.m[2][0]
        +(model.m[0][2]*model.m[1][0]-model.m[0][0]*model.m[1][2])*model.m[2][1]
        +(model.m[0][0]*model.m[1][1]-model.m[0][1]*model.m[1][0])*model.m[2][2];
    for(int k=0;k<3;++k) {model.m[0][k]=(&x.x)[k]*scale[0];model.m[1][k]=(&forward.x)[k]*scale[1];model.m[2][k]=(&z.x)[k]*scale[2]*(determinant<0?-1.f:1.f);}
    return model;
}
// Fixed controller-local calibration: no world-up projection or Euler-angle
// reconstruction, so moving behind the shoulder cannot flip the bow's roll.
inline Matrix RigidBowModel(const Matrix& model, const Matrix& grip, float yaw, float pitch) {
    constexpr float radians=.01745329252f;
    const float halfPitch=pitch*radians*.5f;
    auto trim=Multiply(PoseMatrix({{std::sin(halfPitch),0,0,std::cos(halfPitch)},{0,0,0}}),YawPose(yaw*radians));
    const Matrix local{{{0,1,0,0},{0,0,-1,0},{-1,0,0,0},{0,0,0,1}}};
    auto result=Multiply(Multiply(local,trim),grip);
    const float determinant=(model.m[0][1]*model.m[1][2]-model.m[0][2]*model.m[1][1])*model.m[2][0]
        +(model.m[0][2]*model.m[1][0]-model.m[0][0]*model.m[1][2])*model.m[2][1]
        +(model.m[0][0]*model.m[1][1]-model.m[0][1]*model.m[1][0])*model.m[2][2];
    for(int row=0;row<3;++row) {
        float scale=std::sqrt(model.m[row][0]*model.m[row][0]+model.m[row][1]*model.m[row][1]+model.m[row][2]*model.m[row][2]);
        if(row==2 && determinant<0) scale=-scale;
        for(int k=0;k<3;++k) result.m[row][k]*=scale;
    }
    for(int k=0;k<3;++k) result.m[3][k]=model.m[3][k];
    return result;
}
// Small draw-hand corrections relative to the nocking position. Capture in
// bow-local coordinates so moving/turning the whole rig preserves the aim.
// Never rotate the held bow, bypass draw eligibility or replace head aiming.
class BowDrawHandAim {
    bool captured = false;
    float baseX = 0, baseZ = 0;
public:
    void Reset() { captured = false; }
    XrVector3f Update(const Matrix& model, XrVector3f delta, float trackingScale,
                     bool active, bool enabled) {
        XrVector3f axes[3];
        for (int row=0;row<3;++row) {
            const float length=std::hypot(model.m[row][0],model.m[row][1],model.m[row][2]);
            if (!std::isfinite(length) || length < 1e-7f) { Reset(); return {}; }
            axes[row]={model.m[row][0]/length,model.m[row][1]/length,model.m[row][2]/length};
        }
        const auto forward=axes[1];
        if (!active || !enabled || !std::isfinite(trackingScale) || trackingScale <= .05f ||
            !std::isfinite(delta.x) || !std::isfinite(delta.y) || !std::isfinite(delta.z)) {
            Reset(); return forward;
        }
        auto project=[&](int row) { return (delta.x*axes[row].x+delta.y*axes[row].y+delta.z*axes[row].z)/(40.f*trackingScale); };
        const float x=project(0), z=project(2);
        if (!captured) { baseX=x; baseZ=z; captured=true; return forward; }
        float dx=x-baseX, dz=z-baseZ;
        const float lateral=std::hypot(dx,dz);
        // Five millimetres of slack prevents small tremors at full draw from
        // steering the arrow. The maximum correction is fifteen degrees.
        if (lateral <= .005f) return forward;
        const float along=std::max(.10f,project(1));
        const float gain=std::min(lateral-.005f,along*.2679491924f)/lateral;
        dx*=gain; dz*=gain;
        XrVector3f result{forward.x*along+axes[0].x*dx+axes[2].x*dz,
                         forward.y*along+axes[0].y*dx+axes[2].y*dz,
                         forward.z*along+axes[0].z*dx+axes[2].z*dz};
        const float length=std::hypot(result.x,result.y,result.z);
        return {result.x/length,result.y/length,result.z/length};
    }
};
// Native high-detail arrow: tip X=-396, nock X=2001, scale 0.01.
constexpr float ArrowTipX = -396.f, ArrowNockX = 2001.f, ArrowLength = (ArrowNockX - ArrowTipX) * .01f;
// Add 20 cm only to the wooden shaft; neither arrowhead nor feathers stretch.
constexpr float HeldArrowExtension = 800.f;
constexpr float HeldArrowTipX = ArrowTipX-HeldArrowExtension;
inline float HeldArrowVertexX(float x) { return x <= 68.f ? x-HeldArrowExtension : x; }
inline float HeldArrowDrawLimit(float worldDistance, float gripScale=1.f) {
    // The relaxed string is 395 model units behind the grip. Keep the tip
    // another 2 game units ahead of the grip, even at maximum physical draw.
    return std::clamp(worldDistance,0.f,ArrowLength+HeldArrowExtension*.01f-3.95f*gripScale-2.f);
}
inline float LimitedArrowDraw(float worldDistance) {
    return std::clamp(worldDistance, 0.f, ArrowLength - 2.f);
}
inline Matrix ArrowPose(XrVector3f direction, XrVector3f nock, float trackingScale = 1.f) {
    auto m = YawPose(0);
    float horizontal = std::hypot(direction.x, direction.z);
    XrVector3f side = horizontal > .00001f ? XrVector3f{ direction.z / horizontal, 0, -direction.x / horizontal }
                                           : XrVector3f{ 1, 0, 0 };
    XrVector3f up{ side.y * direction.z - side.z * direction.y, side.z * direction.x - side.x * direction.z,
                   side.x * direction.y - side.y * direction.x };
    for (int k = 0; k < 3; ++k) {
        m.m[0][k] = -(&direction.x)[k] * (.01f * trackingScale);
        m.m[1][k] = (&up.x)[k] * (.01f * trackingScale);
        m.m[2][k] = -(&side.x)[k] * (.01f * trackingScale);
        m.m[3][k] = (&nock.x)[k] + (&direction.x)[k] * ArrowNockX * (.01f * trackingScale);
    }
    return m;
}
// Keep near markers modest and let apparent size decrease over distance.
// Above the far threshold a small angular floor preserves readability.
inline float ReticleWorldScale(float distance) {
    if (!std::isfinite(distance)) return 1.f;
    return std::max(1.f, std::max(0.f, distance) * .0012f);
}
// Shift the whole marker plane in front of the hit surface, including its corners.
inline void LiftReticle(Matrix& pose, XrVector3f normal) {
    float radius = 0;
    for (int row = 0; row < 2; ++row)
        radius += 3.f * std::abs(pose.m[row][0] * normal.x + pose.m[row][1] * normal.y + pose.m[row][2] * normal.z);
    for (int k = 0; k < 3; ++k)
        pose.m[3][k] += (&normal.x)[k] * (radius + 1.f);
}
} // namespace mmvr
