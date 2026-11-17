// Shared Spectral table-reading primitives. Prepared table pointers only: no
// allocation, graph lookup or patch parsing. Used by the compatibility voice;
// new generator modules can use the same sampling without inheriting Voice.
#pragma once
#include "table_bank.h"
#include <algorithm>

namespace partials {

inline float readSpectralTable(const float* table, int length, double position) {
    int i0 = static_cast<int>(position);
    const float fraction = static_cast<float>(position - i0);
    i0 &= length - 1;
    const int i1 = (i0 + 1) & (length - 1);
    return table[i0] + (table[i1] - table[i0]) * fraction;
}

// Position is already phase-modulated, wrapped and symmetry-warped. Keep the
// original operation ordering so extraction does not change saved-preset audio.
inline float readSpectralPair(const float* aLow, const float* aHigh,
                              const float* bLow, const float* bHigh,
                              int length, double position, float mix,
                              bool useFilter, float levelFraction) {
    float a = 0.0f, b = 0.0f;
    if (mix < 1.0f) {
        a = readSpectralTable(aLow, length, position);
        if (useFilter && levelFraction > 0.0f)
            a += (readSpectralTable(aHigh, length, position) - a) * levelFraction;
    }
    if (mix > 0.0f) {
        b = readSpectralTable(bLow, length, position);
        if (useFilter && levelFraction > 0.0f)
            b += (readSpectralTable(bHigh, length, position) - b) * levelFraction;
    }
    return a + (b - a) * mix;
}

inline double warpSpectralPhase(double phase, double symmetry) {
    const double s = std::clamp(symmetry, 0.01, 0.99) * kTableSize;
    const double half = kTableSize / 2.0;
    return phase < s ? phase * half / s : half + (phase - s) * half / (kTableSize - s);
}

} // namespace partials
