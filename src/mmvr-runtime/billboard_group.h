#pragma once
#include "first_person.h"
#include <cmath>
#include <cstring>
namespace mmvr {
// Frame-owned root registration; the interpreter supplies the same sampled
// root used by its limb replacements before either eye is replayed.
struct BillboardGroupRoot {
    const void* address = nullptr;
    Matrix native{}, visual{};
    bool interpolated = false;
    void Sample(const float* replacement) noexcept {
        visual = native;
        interpolated = false;
        if (!replacement) return;
        Matrix sampled;
        std::memcpy(&sampled, replacement, sizeof(sampled));
        for (const auto& row : sampled.m)
            for (float value : row) if (!std::isfinite(value)) return;
        Matrix inverse;
        if (!InverseAffine(sampled, inverse)) return;
        visual = sampled;
        interpolated = true;
    }
};
}
