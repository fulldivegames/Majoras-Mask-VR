#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mmvr {
struct Pad {
    uint16_t buttons = 0;
    int8_t x = 0, y = 0;
    bool active = false;
    int8_t rightX = 0, rightY = 0;
    bool goronRoll = false, goronRollOverride = false;
};
inline Pad ItemWheelInput(Pad pad, bool selecting) {
    if (selecting) {
        pad.buttons = 0;
        pad.rightX = pad.rightY = 0;
        pad.goronRoll = pad.goronRollOverride = false;
    }
    return pad;
}
inline int8_t Axis(float value, float other) {
    const float length = std::sqrt(value * value + other * other);
    if (length <= .18f)
        return 0;
    const float scale = std::min(1.f, (length - .18f) / .82f) / length;
    return static_cast<int8_t>(std::lround(std::clamp(value * scale, -1.f, 1.f) * 85.f));
}
inline uint16_t CButtons(float x, float y) {
    return (x < -.55f ? 2 : 0) | (x > .55f ? 1 : 0) | (y > .55f ? 8 : 0) | (y < -.55f ? 4 : 0);
}
inline Pad NativeChoiceInput(Pad pad, float lx, float ly, float rx, float ry, bool confirm, bool cancel, bool owlMap = false) {
    const bool left = std::hypot(lx, ly) >= std::hypot(rx, ry);
    const float x = left ? lx : rx, y = left ? ly : ry;
    // The native owl map is a horizontal ring. Up means previous (left),
    // down means next (right); other native choice screens retain both axes.
    pad.x = owlMap && std::abs(y) > std::abs(x) ? Axis(-y, x) : Axis(x, y);
    pad.y = owlMap ? 0 : Axis(y, x);
    pad.rightX = pad.rightY = 0;
    pad.buttons = (confirm ? 0x8000 : 0) | (cancel ? 0x4000 : 0);
    return pad;
}
// Five native notes; B and Y both remain cancel rather than changing pitch.
inline uint16_t InstrumentButtons(float lx, float ly, float rx, float ry, bool a, bool x, bool b, bool y) {
    return CButtons(lx, ly) | CButtons(rx, ry) | ((a || x) ? 0x8000 : 0) | ((b || y) ? 0x4000 : 0);
}
struct TriggerHold {
    bool held = false;
    bool Update(float value, bool enabled) {
        if (!enabled || value < .25f)
            held = false;
        else if (value > .65f)
            held = true;
        return held;
    }
};
// A binding/context change must observe release before rolling. This prevents
// a trigger held across menus, dialogue or a transformation from starting a roll.
struct GoronRollHold {
    TriggerHold button;
    int source = -1;
    bool active = false, armed = false;
    bool Update(float value, int selectedSource, bool enabled) {
        if (!enabled) {
            active = armed = false;
            source = -1;
            return button.Update(0, false);
        }
        if (!active || source != selectedSource) {
            button.Update(0, false);
            armed = value < .25f;
            active = true;
            source = selectedSource;
        }
        armed |= value < .25f;
        return button.Update(value, armed);
    }
};
// Page left is native Z (not L, which opens the developer inventory editor).
// A trigger held on menu entry must be released before it can change pages.
struct PauseTriggers {
    TriggerHold left, right;
    bool active = false, armedLeft = false, armedRight = false;
    uint16_t Update(float l, float r, bool enabled) {
        if (!enabled) {
            active = false;
            armedLeft = armedRight = false;
            left.Update(0, false);
            right.Update(0, false);
            return 0;
        }
        if (!active) {
            active = true;
            armedLeft = l < .25f;
            armedRight = r < .25f;
        }
        armedLeft |= l < .25f;
        armedRight |= r < .25f;
        bool a = left.Update(l, armedLeft), b = right.Update(r, armedRight);
        return a == b ? 0 : a ? 0x2000 : 0x10;
    }
};
// Left upper face button is lock-on in gameplay; in instruments/pause it keeps
// its native L mapping. Triggers are reserved for physical actions and menu tabs.
inline uint16_t LeftUpperButton(bool pressed, bool gameplay, bool ocarina, bool pause, bool climbing) {
    if (!pressed)
        return 0;
    if (ocarina || pause || !gameplay)
        return 0x20;
    return climbing ? 0 : 0x2000;
}
// Native third-person controls keep item use on C-down and preserve native
// shield/target state machines. Menus and instruments own their own bindings.
inline uint16_t ThirdPersonButtons(float leftTrigger, float leftGrip, float rightTrigger, bool enabled) {
    if (!enabled) return 0;
    return (leftTrigger > .65f ? 0x10 : 0) |
           (leftGrip > .65f ? 0x2000 : 0) |
           (rightTrigger > .65f ? 4 : 0);
}
// Preserve short presses between native game ticks. Clear on focus/session loss.
struct PadLatch {
    Pad held{};
    uint16_t pending = 0, delivered = 0;
    bool pendingRoll = false, deliveredRoll = false;
    void Update(Pad next) {
        if (!next.active) {
            held = {};
            pending = 0;
            delivered = 0;
            pendingRoll = deliveredRoll = false;
            return;
        }
        if (!next.goronRollOverride) pendingRoll = deliveredRoll = false;
        else pendingRoll |= next.goronRoll && !held.goronRoll;
        pending |= next.buttons & ~held.buttons;
        held = next;
    }
    void ClearButtons() {
        held.buttons = 0;
        pending = delivered = 0;
        held.goronRoll = held.goronRollOverride = false;
        pendingRoll = deliveredRoll = false;
    }
    Pad Consume() {
        auto result = held;
        // A second short press needs a delivered release between native ticks.
        const uint16_t releaseFirst = pending & delivered;
        result.buttons = (result.buttons | pending) & ~releaseFirst;
        pending &= releaseFirst;
        delivered = result.buttons;
        const bool releaseRollFirst = pendingRoll && deliveredRoll;
        result.goronRoll = (result.goronRoll || pendingRoll) && !releaseRollFirst;
        pendingRoll &= releaseRollFirst;
        deliveredRoll = result.goronRoll;
        return result;
    }
};
} // namespace mmvr
