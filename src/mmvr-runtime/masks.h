#pragma once
#include "projection.h"
#include "mask_fit.h"
#include <cstdint>
namespace mmvr {
inline XrPosef MaskFacePose(const XrPosef& grip, const XrPosef& aim, float height) {
    auto pose = grip;
    pose.orientation = aim.orientation;
    auto basis = PoseMatrix(aim);
    pose.position.x += basis.m[1][0] * height * .5f;
    pose.position.y += basis.m[1][1] * height * .5f;
    pose.position.z += basis.m[1][2] * height * .5f;
    return pose;
}
// The grip reaches a single small slot just below/in front of the headset.
// The same volume is used to put a mask on and to pick a worn mask up.
inline float MaskSlotDistance(const XrPosef& grip, const XrPosef& head) {
    auto local = Multiply(PoseMatrix(grip), InversePose(PoseMatrix(head)));
    float x = local.m[3][0], y = local.m[3][1] + .12f, z = local.m[3][2] + .14f;
    return std::sqrt(x * x + y * y + z * z);
}
inline bool InMaskFaceSlot(const XrPosef& grip, const XrPosef& head, float radius) {
    const float distance = MaskSlotDistance(grip, head);
    return std::isfinite(distance) && distance <= std::clamp(radius, .12f, .28f);
}
struct MaskGesture {
    bool carrying = false, armed = false, removing = false;
    uint64_t epoch = 0;
    double nearSince = -1, lastTime = 0;
    XrVector3f previous{};
    bool have = false;
    float grabbedDistance = 0;
    bool wheelHeld = false, wheelSeen = false, wheelOutside = false;
    void Cancel() {
        carrying = false;
        wheelHeld = wheelSeen = wheelOutside = false;
        armed = false;
        nearSince = -1;
        have = false;
    }
    void HoldFromWheel() {
        Cancel();
        carrying = wheelHeld = true;
        removing = false;
    }
    bool UpdateWheel(double time, uint64_t generation, bool allowed, bool tracked, float trigger,
                     const XrPosef& hand, const XrPosef& head, float faceDistance) {
        if (!allowed || !tracked || !std::isfinite(time) || !std::isfinite(trigger) ||
            !std::isfinite(MaskSlotDistance(hand, head))) {
            Cancel();
            return false;
        }
        // First sample establishes the tracking epoch. Selection while already
        // at the face must leave the face slot before it can wear the mask.
        if (!wheelSeen) {
            epoch = generation;
            lastTime = time;
            previous = hand.position;
            wheelSeen = true;
            armed = trigger < .25f;
            wheelOutside = !InMaskFaceSlot(hand, head, faceDistance);
            return false;
        }
        if (time == lastTime) return false;
        float dx=hand.position.x-previous.x, dy=hand.position.y-previous.y, dz=hand.position.z-previous.z;
        if (epoch != generation || time < lastTime || time-lastTime > .15 ||
            !std::isfinite(dx+dy+dz) || dx*dx+dy*dy+dz*dz > .25f*.25f) {
            Cancel();
            return false;
        }
        lastTime = time;
        previous = hand.position;
        if (trigger < .25f) armed = true;
        if (armed && trigger > .7f) {
            Cancel(); // Dismiss, including at the face; never also equip.
            return false;
        }
        if (!InMaskFaceSlot(hand, head, faceDistance)) {
            wheelOutside = true;
            nearSince = -1;
        } else if (wheelOutside) {
            if (nearSince < 0) nearSince = time;
            if (time-nearSince >= .08) {
                Cancel();
                return true;
            }
        }
        return false;
    }
    bool Update(double time, uint64_t generation, bool allowed, bool tracked, float trigger, bool worn,
                const XrPosef& hand, const XrPosef& head, float faceDistance, float removeDistance) {
        if (!allowed || !tracked || epoch != generation) {
            Cancel();
            epoch = generation;
            return false;
        }
        if (have && time == lastTime)
            return false;
        float dx = hand.position.x - previous.x, dy = hand.position.y - previous.y, dz = hand.position.z - previous.z;
        if (!std::isfinite(time) || !std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz) ||
            (have && (time <= lastTime || time - lastTime > .15 || dx * dx + dy * dy + dz * dz > .25f * .25f))) {
            Cancel();
            return false;
        }
        previous = hand.position;
        lastTime = time;
        have = true;
        auto local = Multiply(PoseMatrix(hand), InversePose(PoseMatrix(head)));
        float distance =
            std::sqrt(local.m[3][0] * local.m[3][0] + local.m[3][1] * local.m[3][1] + local.m[3][2] * local.m[3][2]);
        bool near = InMaskFaceSlot(hand, head, faceDistance);
        if (trigger < .25f && !carrying) {
            armed = true;
            return false;
        }
        if (trigger > .7f && !carrying && armed) {
            // Consume every press, including a miss. Moving a held trigger into the slot
            // must never turn that old press into a new grab.
            armed = false;
            if (!worn || near) {
                carrying = true;
                removing = worn;
                grabbedDistance = distance;
                nearSince = -1;
                have = false;
            }
        }
        if (!carrying)
            return false;
        if (near) {
            if (nearSince < 0)
                nearSince = time;
        } else
            nearSince = -1;
        if (trigger < .25f) {
            bool qualified = removing ? distance > removeDistance && distance > grabbedDistance + .08f : near;
            carrying = false;
            armed = true;
            nearSince = -1;
            return qualified;
        }
        return false;
    }
};
} // namespace mmvr
