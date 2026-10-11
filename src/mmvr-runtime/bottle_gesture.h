#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace mmvr {
enum class BottleGestureAction { None, Drink, Pour };

// Physical metres and headset time: independent of native animation/render rate.
// A bottle selected at the mouth must leave it before the first sip. Likewise,
// pouring requires an upright sample, then inversion and a deliberate reversal.
struct BottleUseGesture {
    uint64_t epoch = 0;
    double lastTime = -1, nearSince = -1, invertedSince = -1, strokeSince = -1;
    bool mouthArmed = false, pourArmed = false, fired = false;
    std::array<float, 3> anchor{};
    std::array<float, 3> previous{};
    int axis = -1, direction = 0;

    void Reset() { *this = {}; }
    BottleGestureAction Update(double time, uint64_t generation, bool allowed,
                              bool drinkable, bool pourable, bool atMouth,
                              float openingUp, const std::array<float, 3>& position) {
        if (!allowed || !std::isfinite(time) || !std::isfinite(openingUp) ||
            !std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2])) {
            Reset();
            return BottleGestureAction::None;
        }
        if (lastTime >= 0 && generation == epoch && time == lastTime)
            return BottleGestureAction::None;
        const float jump = std::hypot(position[0] - previous[0], position[1] - previous[1],
                                      position[2] - previous[2]);
        if (lastTime < 0 || generation != epoch || time < lastTime || time - lastTime > .15 || jump > .25f) {
            Reset();
            epoch = generation;
            lastTime = time;
            anchor = position;
            previous = position;
            mouthArmed = !atMouth;
            pourArmed = openingUp > .25f;
            return BottleGestureAction::None;
        }
        lastTime = time;
        previous = position;
        if (!atMouth) {
            mouthArmed = true;
            nearSince = -1;
        }
        if (openingUp > .25f) pourArmed = true;
        if (fired) return BottleGestureAction::None;
        if (drinkable && mouthArmed && atMouth) {
            if (nearSince < 0) nearSince = time;
            if (time - nearSince >= .25) {
                fired = true;
                return BottleGestureAction::Drink;
            }
        }
        if (!pourable || !pourArmed || openingUp > -.5f) {
            invertedSince = strokeSince = -1;
            direction = 0;
            axis = -1;
            anchor = position;
            return BottleGestureAction::None;
        }
        if (invertedSince < 0) {
            invertedSince = time;
            anchor = position;
            return BottleGestureAction::None;
        }
        if (time - invertedSince < .12) {
            anchor = position;
            return BottleGestureAction::None;
        }
        if (strokeSince >= 0 && time - strokeSince > .8) {
            direction = 0;
            axis = -1;
            strokeSince = -1;
            anchor = position;
        }
        int candidate = axis;
        if (candidate < 0) {
            candidate = 0;
            for (int i = 1; i < 3; ++i)
                if (std::abs(position[i] - anchor[i]) > std::abs(position[candidate] - anchor[candidate]))
                    candidate = i;
        }
        const float delta = position[candidate] - anchor[candidate];
        if (std::abs(delta) < .025f) return BottleGestureAction::None;
        const int sign = delta > 0 ? 1 : -1;
        if (direction && sign != direction && time > strokeSince &&
            std::abs(delta) / float(time - strokeSince) >= .25f) {
            fired = true;
            return BottleGestureAction::Pour;
        }
        axis = candidate;
        direction = sign;
        strokeSince = time;
        anchor = position;
        return BottleGestureAction::None;
    }
};
} // namespace mmvr
