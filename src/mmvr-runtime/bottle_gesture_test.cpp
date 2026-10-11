#include "bottle_gesture.h"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <string>

int main() {
    unsigned checks = 0;
    const auto check = [&](bool ok) { ++checks; if (!ok) throw std::runtime_error("bottle gesture check failed " + std::to_string(checks)); };
    using Action = mmvr::BottleGestureAction;
    for (int hz : {20, 30, 72, 80, 90, 120, 144}) {
        mmvr::BottleUseGesture g;
        double time = 10;
        auto update = [&](bool near, float up, float x, bool drink, bool pour, bool allowed = true, uint64_t epoch = 1) {
            time += 1. / hz;
            return g.Update(time, epoch, allowed, drink, pour, near, up, {x, 0, 0});
        };
        // Selection at the mouth, inversion alone, and steady holding do nothing.
        for (int i = 0; i < hz; ++i) check(update(true, 1, 0, true, false) == Action::None);
        check(update(false, 1, 0, true, false) == Action::None);
        int drinks = 0;
        for (int i = 0; i < hz; ++i) drinks += update(true, .2f, 0, true, false) == Action::Drink;
        check(drinks == 1);
        for (int i = 0; i < hz; ++i) check(update(true, .2f, 0, true, false) == Action::None);
        g.Reset();
        check(update(false, 1, 0, false, true) == Action::None);
        for (int i = 0; i < hz; ++i) check(update(false, -1, 0, false, true) == Action::None);
        for (int i = 0; i < hz; ++i) check(update(false, -1, .0001f * (i % 2), false, true) == Action::None);
        check(update(false, -1, .06f, false, true) == Action::None);
        check(update(false, -1, 0, false, true) == Action::Pour);
        for (int i = 0; i < hz; ++i) check(update(false, -1, i % 2 ? .06f : 0, false, true) == Action::None);
        // Upright shaking cannot dump; drink and release categories do not cross.
        g.Reset();
        for (int i = 0; i < hz; ++i) check(update(true, 1, i % 2 ? .06f : 0, false, true) == Action::None);
        g.Reset();
        for (int i = 0; i < hz; ++i) check(update(false, -1, i % 2 ? .06f : 0, false, true) == Action::None);
        // Losing eligibility, tracking epoch, or a discontinuous pose cannot complete a sip.
        for (int boundary = 0; boundary < 4; ++boundary) {
            g.Reset(); update(false, 1, 0, true, false);
            for (int i = 0; i < hz / 5; ++i) check(update(true, 1, 0, true, false) == Action::None);
            if (boundary == 0) update(true, 1, 0, true, false, false);
            if (boundary == 1) update(true, 1, 0, true, false, true, 2);
            if (boundary == 2) { time += .3; update(true, 1, 0, true, false); }
            if (boundary == 3) update(true, 1, 1, true, false);
            for (int i = 0; i < hz; ++i) check(update(true, 1, 0, true, false) == Action::None);
        }
        // Milk's next sip needs a fresh departure from the face after native recovery.
        g.Reset(); update(true, 1, 0, true, false);
        check(update(true, 1, 0, true, false) == Action::None);
    }
    mmvr::BottleUseGesture g;
    check(g.Update(std::numeric_limits<double>::quiet_NaN(), 1, true, true, true, true, -1, {}) == Action::None);
    std::cout << "PASS " << checks << " bottle gesture checks\n";
}
