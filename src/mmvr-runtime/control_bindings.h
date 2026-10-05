#pragma once
#include "settings.h"
#include "controller_profiles.h"
#include <array>
#include <cmath>
namespace mmvr {
inline constexpr int ControlCount = 13;
inline bool StickControl(int action) {
    return action == 9 || action == 10;
}
inline Setting ControlSetting(int action) {
    return Setting(int(Setting::BindA) + action);
}
inline bool BindingSetting(int id) {
    return id >= int(Setting::BindA) && id <= int(Setting::BindRightTrigger);
}
inline bool ValidControlBinding(int action, int source) {
    return action >= 0 && action < ControlCount && source >= 0 && source < ControlCount &&
           StickControl(action) == StickControl(source);
}
// Include inputs synthesized from pads/digital grips, not only direct action bindings.
// With no active profile, allow offline configuration using the standard names.
inline bool ControlAvailable(int source, const compat::Profile* left, const compat::Profile* right) {
    if (source < 0 || source >= ControlCount) return false;
    const bool isRight = source == 0 || source == 1 || source == 7 || source == 8 || source == 10 || source == 12;
    const auto* profile = isRight ? right : left;
    if (!profile) return true;
    auto has = [&](int action) {
        return std::any_of(profile->bindings.begin(), profile->bindings.end(),
                           [&](const auto& binding) { return binding.action == action; });
    };
    if (source == 6 || source == 7) return has(source) || has(19 + (isRight ? 1 : 0));
    if (profile->layout == compat::Layout::Wand) {
        if (source == 0 || source == 1) return has(26);
        if (source == 2 || source == 4 || source == 5) return has(25);
        if (source == 9 || source == 10) return has(isRight ? 22 : 21) && has(isRight ? 24 : 23);
    }
    if (profile->layout == compat::Layout::Mixed && source <= 3) return has(isRight ? 26 : 25);
    if (profile->layout == compat::Layout::Index && source == 4) return has(23) && has(27);
    return has(source);
}
inline int ControlSource(const Settings& settings, int action) {
    return int(settings.Get(ControlSetting(action)));
}
// Rolling may share a face button, grip or trigger. Menu/recenter and stick
// navigation stay available so a roll binding cannot strand the player.
inline bool ValidGoronRollBinding(int source) {
    return (source >= 0 && source <= 3) || source == 6 || source == 7 ||
           source == 11 || source == 12 || source == ControlCount;
}
inline int GoronRollSource(const Settings& settings) {
    const int source = int(settings.Get(Setting::GoronRollBinding));
    return ValidGoronRollBinding(source) ? source : ControlCount;
}
inline int NextGoronRollBinding(int source, int direction) {
    do { source = (source + (direction > 0 ? 1 : ControlCount)) % (ControlCount + 1); }
    while (!ValidGoronRollBinding(source));
    return source;
}
struct ControlVector {
    float x = 0, y = 0;
};
struct ControlSample {
    std::array<float, ControlCount> value{};
    ControlVector sticks[2]{};
    bool Neutral() const {
        for (int i = 0; i < ControlCount; ++i)
            if (!StickControl(i) && value[i] > .25f)
                return false;
        for (auto stick : sticks)
            if (std::abs(stick.x) > .3f || std::abs(stick.y) > .3f)
                return false;
        return true;
    }
};
inline ControlSample RemapControls(const Settings& settings, const ControlSample& physical) {
    ControlSample result;
    for (int i = 0; i < ControlCount; ++i) {
        int source = ControlSource(settings, i);
        if (StickControl(i))
            result.sticks[i - 9] = physical.sticks[source - 9];
        else
            result.value[i] = physical.value[source];
    }
    return result;
}
inline ControlSample GoronRollControls(const Settings& settings, const ControlSample& physical, bool ownsRoll) {
    auto result = RemapControls(settings, physical);
    const int source = GoronRollSource(settings);
    if (ownsRoll && source < ControlCount) {
        // Interact/confirm A retains its native context. The roll action is
        // independent; suppress other actions that share its physical input.
        for (int action : {1, 2, 3, 6, 7, 11, 12})
            if (ControlSource(settings, action) == source) result.value[action] = 0;
    }
    return result;
}
// A roll-owned physical press stays claimed until its real release, including
// after dialogue, face-slot entry, focus loss or a binding change. Otherwise
// restoring a masked 0 to a still-held 1 creates a false item/attack press.
struct GoronRollRouting {
    std::array<bool, ControlCount> releaseRequired{};
    int owner = -1;
    bool Claims(int source) const {
        return source >= 0 && source < ControlCount &&
               (owner == source || releaseRequired[source]);
    }
    void EndOwnership() { owner = -1; }
    ControlSample Update(const Settings& settings, const ControlSample& physical, bool ownsRoll) {
        const int source = GoronRollSource(settings);
        owner = ownsRoll && source < ControlCount ? source : -1;
        for (int input = 0; input < ControlCount; ++input)
            if (std::isfinite(physical.value[input]) && physical.value[input] < .25f)
                releaseRequired[input] = false;
        if (owner >= 0 && !(physical.value[owner] < .25f)) releaseRequired[owner] = true;
        auto result = RemapControls(settings, physical);
        // A stays native interact/confirm. Menu and recenter remain independent
        // physical inputs in UI, while unrelated gameplay inputs stay available.
        for (int action : {1, 2, 3, 6, 7, 11, 12})
            if (Claims(ControlSource(settings, action))) result.value[action] = 0;
        return result;
    }
};
inline int BindingConflict(const Settings& settings, int action, int source) {
    for (int i = 0; i < ControlCount; ++i)
        if (i != action && StickControl(i) == StickControl(action) && ControlSource(settings, i) == source)
            return i;
    return -1;
}
template <class Change> inline void AssignControl(const Settings& settings, int action, int source, Change change) {
    if (!ValidControlBinding(action, source))
        return;
    const int previous = ControlSource(settings, action);
    // Swap occupied inputs rather than silently disabling the previous action.
    for (int i = 0; i < ControlCount; ++i)
        if (i != action && StickControl(i) == StickControl(action) && ControlSource(settings, i) == source)
            change(ControlSetting(i), float(previous));
    change(ControlSetting(action), float(source));
}
inline const char* ControlName(int source, const compat::Profile* left = nullptr,
                               const compat::Profile* right = nullptr) {
    static const char* standard[] = { "Right A",           "Right B",          "Left X",      "Left Y",
                                      "Left menu",         "Left stick click", "Left grip",   "Right grip",
                                      "Right stick click", "Left stick",       "Right stick", "Left trigger",
                                      "Right trigger" };
    if (source == ControlCount)
        return "Same as interact";
    if (source < 0 || source >= ControlCount)
        return "Unavailable";
    const bool isRight = source == 0 || source == 1 || source == 7 || source == 8 || source == 10 || source == 12;
    auto profile = isRight ? right : left;
    if (!profile)
        return standard[source];
    if (profile->layout == compat::Layout::Index || std::strstr(profile->path, "xr-4_controller")) {
        if (source == 2)
            return "Left A";
        if (source == 3)
            return "Left B";
        if (source == 4 && profile->layout == compat::Layout::Index)
            return "Left pad pressure";
    }
    if (profile->layout == compat::Layout::Wand || profile->layout == compat::Layout::Mixed) {
        if (source == 0)
            return "Right pad up/center";
        if (source == 1)
            return "Right pad down";
        if (source == 8)
            return "Right menu";
        if (source == 2)
            return profile->layout == compat::Layout::Wand ? "Left pad up" : "Left pad center/down";
        if (source == 3)
            return profile->layout == compat::Layout::Wand ? "Left menu" : "Left pad up";
        if (profile->layout == compat::Layout::Wand) {
            if (source == 4)
                return "Left pad down";
            if (source == 5)
                return "Left pad center";
            if (source == 9)
                return "Left trackpad";
            if (source == 10)
                return "Right trackpad";
        }
    }
    if (std::strstr(profile->path, "frame_controller")) {
        if (source == 2)
            return "Left D-pad down";
        if (source == 3)
            return "Left D-pad up";
        if (source == 4)
            return "Left view";
    }
    return standard[source];
}
// UI uses the original controls while rebinding, so remapping confirm/cancel or
// navigation cannot strand the player. No action is applied until confirmed.
struct BindingEditor {
    enum Phase { Closed, Release, Listen, ReviewRelease, Review };
    Phase phase = Closed;
    int action = -1, source = -1;
    float seconds = 0;
    void Begin(int selected) {
        action = selected;
        source = -1;
        seconds = 0;
        phase = Release;
    }
    void Cancel() {
        phase = Closed;
        action = source = -1;
        seconds = 0;
    }
    bool Active() const {
        return phase != Closed;
    }
    // Return 1 to commit, -1 on cancel/timeout, 0 while waiting.
    int Update(const ControlSample& input, float dt) {
        if (!Active())
            return 0;
        seconds += std::clamp(dt, 0.f, .1f);
        if (seconds > 20) {
            Cancel();
            return -1;
        }
        if (phase == Release || phase == ReviewRelease) {
            if (input.Neutral())
                phase = phase == Release ? Listen : Review;
            return 0;
        }
        if (phase == Listen) {
            if (StickControl(action) && input.value[1] > .75f) {
                Cancel();
                return -1;
            }
            int found = -1;
            for (int i = 0; i < ControlCount; ++i) {
                if (action == ControlCount && !ValidGoronRollBinding(i)) continue;
                if (StickControl(i) != StickControl(action))
                    continue;
                const auto stick = StickControl(i) ? input.sticks[i - 9] : ControlVector{};
                bool pressed =
                    StickControl(i) ? std::max(std::abs(stick.x), std::abs(stick.y)) > .75f : input.value[i] > .75f;
                if (pressed) {
                    if (found >= 0)
                        return 0;
                    found = i;
                }
            }
            if (found >= 0) {
                source = found;
                phase = ReviewRelease;
                seconds = 0;
            }
            return 0;
        }
        if (phase == Review) {
            if (input.value[1] > .75f) {
                Cancel();
                return -1;
            }
            if (input.value[0] > .75f)
                return 1;
        }
        return 0;
    }
};
inline BindingEditor& GetBindingEditor() {
    static BindingEditor editor;
    return editor;
}
} // namespace mmvr
