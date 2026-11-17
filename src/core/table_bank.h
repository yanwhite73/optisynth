// Pre-rendered single-cycle tables for one spectrum: filter levels x band-limit tiers.
//
// Level 0 is the unfiltered spectrum. Levels 1..216 apply the filter curve shifted by one
// semitone per level, so a cutoff sweep becomes a crossfade between neighbouring levels.
// Each level holds 21 band-limit tiers; tier t contains harmonics 1..maxHarmonic and is a
// decimated copy of the full 2048-sample cycle. See ../../docs/design.md.
#pragma once

#include <array>
#include <vector>

#include "spectrum.h"

namespace partials {

constexpr int kTableSize = 2048;
constexpr int kLevels = 217;
constexpr int kMaxLevel = kLevels - 1;
constexpr int kTiers = 21;
constexpr int kLevelStride = 12288;          // the 21 tiers of one level, back to back
constexpr double kBandLimitDivisor = 4.04;   // top partial stays below sampleRate / 4.04

struct Tier {
    int maxHarmonic;  // highest harmonic in this tier
    int step;         // decimation: every step-th sample of the full cycle
    int length;       // kTableSize / step
    int offset;       // position inside the level
};

const std::array<Tier, kTiers>& tiers();

// Lowest tier (most harmonics) whose top harmonic stays below sampleRate / 4.04 for a
// fundamental of freqOverSampleRate cycles per sample.
int chooseTier(double freqOverSampleRate);

// Filter gain (linear) for a harmonic at a given level (1..216). Exposed for tests.
float filterGain(const std::array<float, kCurvePoints>& curveAmplitude, int harmonic, int level);

class TableBank {
public:
    // The full unfiltered waveform's peak is capped at one, retaining that gain across
    // filter levels and band-limit tiers. A and B are built/normalised independently.
    // withFilterLevels = false builds only level 0 (filter bypassed), which is much cheaper.
    void build(const Spectrum& spectrum, const FilterCurve& curve, bool withFilterLevels);

    int levelCount() const { return levelCount_; }
    const float* table(int level, int tier) const {
        return data_.data() + static_cast<std::size_t>(level) * kLevelStride + tiers()[tier].offset;
    }

private:
    std::vector<float> data_;
    int levelCount_ = 0;
};

}  // namespace partials
