#include "table_bank.h"

#include <algorithm>
#include <cmath>

namespace partials {

namespace {

constexpr double kTwoPi = 6.283185307179586;

// Band-limit tiers, from the architecture doc: the top harmonic falls about a third of an
// octave per tier, and tables shrink as fewer harmonics need fewer samples.
constexpr int kTierMaxHarmonic[kTiers] = {256, 203, 161, 128, 101, 80, 64, 50, 40, 32, 25,
                                          20,  16,  12,  10,  8,   6,  4,  3,  2,  1};
constexpr int kTierStep[kTiers] = {1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 32, 32, 32, 32};

std::array<Tier, kTiers> makeTiers() {
    std::array<Tier, kTiers> t{};
    int offset = 0;
    for (int i = 0; i < kTiers; ++i) {
        t[i] = {kTierMaxHarmonic[i], kTierStep[i], kTableSize / kTierStep[i], offset};
        offset += t[i].length;
    }
    return t;
}

// Each harmonic's pitch above the fundamental: the semitone at or just above it, and a
// weight that places it between that semitone and the one below.
struct HarmonicPitch {
    int semitone;
    float weight;  // 1 when the harmonic sits exactly on `semitone`, towards 0 just above semitone-1
};

std::array<HarmonicPitch, kSpectrumSize> makePitches() {
    std::array<HarmonicPitch, kSpectrumSize> p{};
    const double semitoneRatio = std::pow(2.0, 1.0 / 12.0);
    for (int h = 1; h <= kHarmonics; ++h) {
        int n = static_cast<int>(std::ceil(12.0 * std::log2(static_cast<double>(h)) - 1e-9));
        double above = std::pow(2.0, n / 12.0) / h;  // 1 .. semitoneRatio
        double w = 1.0 - (above - 1.0) / (semitoneRatio - 1.0);
        p[h] = {std::clamp(n, 0, 120), static_cast<float>(std::clamp(w, 0.0, 1.0))};
    }
    return p;
}

const std::array<HarmonicPitch, kSpectrumSize>& pitches() {
    static const auto p = makePitches();
    return p;
}

}  // namespace

const std::array<Tier, kTiers>& tiers() {
    static const auto t = makeTiers();
    return t;
}

int chooseTier(double freqOverSampleRate) {
    const double limit = 1.0 / kBandLimitDivisor;
    for (int t = 0; t < kTiers; ++t) {
        if (kTierMaxHarmonic[t] * freqOverSampleRate < limit) return t;
    }
    return kTiers - 1;
}

float filterGain(const std::array<float, kCurvePoints>& curve, int harmonic, int level) {
    // At level L a harmonic reads curve index (its semitone + L - 96), clamped to 0..120,
    // which maps to points 60..180. Index 60 is the curve centre, so the fundamental sits on
    // the centre at level 156.
    const HarmonicPitch hp = pitches()[harmonic];
    const int index = hp.semitone + level - 96;
    constexpr int offset = kCurveCentre - 60;  // index range 0..120 maps to points 60..180
    if (index < 0) return curve[offset];
    if (index > 120) return curve[offset + 120];
    if (index == 0) return curve[offset];
    return (1.0f - hp.weight) * curve[offset + index - 1] + hp.weight * curve[offset + index];
}

void TableBank::build(const Spectrum& spectrumIn, const FilterCurve& curveIn, bool withFilterLevels) {
    Spectrum spectrum = spectrumIn;
    FilterCurve curve = curveIn;
    spectrum.normalise();
    curve.normalise();

    levelCount_ = withFilterLevels ? kLevels : 1;
    data_.assign(static_cast<std::size_t>(levelCount_) * kLevelStride, 0.0f);

    std::array<float, kSpectrumSize> amp{};
    for (int h = 1; h <= kHarmonics; ++h) amp[h] = spectrum.amplitude(h);
    std::array<float, kCurvePoints> curveAmp{};
    for (int i = 0; i < kCurvePoints; ++i) curveAmp[i] = curve.amplitude(i);

    // One unit-amplitude cycle per audible harmonic, with its phase; reused for every level.
    std::vector<float> rows(static_cast<std::size_t>(kSpectrumSize) * kTableSize, 0.0f);
    std::array<float, kTableSize> unfiltered{};
    for (int h = 1; h <= kHarmonics; ++h) {
        if (amp[h] <= 0.0f) continue;
        float* row = rows.data() + static_cast<std::size_t>(h) * kTableSize;
        const double ph = kTwoPi * spectrum.phase[h];
        for (int n = 0; n < kTableSize; ++n) {
            row[n] = static_cast<float>(std::sin(kTwoPi * h * n / kTableSize + ph));
            unfiltered[n] += amp[h] * row[n];
        }
    }

    // Measured on sparse, rich and phase-shifted spectra: each A/B waveform is scaled
    // down to a unit peak, independently, before filtering and band limiting. Retain this
    // one gain for every level/tier; renormalising each would undo filter attenuation.
    // Waves already below unity (e.g. a truncated square series) are not boosted.
    float peak = 1.0f;
    for (float x : unfiltered) peak = std::max(peak, std::fabs(x));
    for (float& a : amp) a /= peak;

    const auto& tierList = tiers();
    for (int level = 0; level < levelCount_; ++level) {
        float* base = data_.data() + static_cast<std::size_t>(level) * kLevelStride;
        float* full = base + tierList[0].offset;  // tier 0 doubles as the summing buffer
        int h = 1;
        for (int t = kTiers - 1; t >= 0; --t) {
            for (; h <= tierList[t].maxHarmonic; ++h) {
                float a = amp[h];
                if (a <= 0.0f) continue;
                if (level > 0) a *= filterGain(curveAmp, h, level);
                if (std::fabs(a) < 1e-7f) continue;
                const float* row = rows.data() + static_cast<std::size_t>(h) * kTableSize;
                for (int n = 0; n < kTableSize; ++n) full[n] += a * row[n];
            }
            if (t == 0) break;
            float* dst = base + tierList[t].offset;
            for (int k = 0; k < tierList[t].length; ++k) dst[k] = full[k * tierList[t].step];
        }
    }
}

}  // namespace partials
