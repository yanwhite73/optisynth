// Harmonic spectra and spectral filter curves, held in dB.
#pragma once

#include <array>

namespace partials {

constexpr int kHarmonics = 256;               // harmonics 1..256; entry 0 is not played
constexpr int kSpectrumSize = kHarmonics + 1;
constexpr int kCurvePoints = 241;             // filter curve: one point per semitone
constexpr int kCurveCentre = 120;             // the point that sits at the cutoff frequency
constexpr float kSilentDb = -200.0f;          // marks a silent entry

// dB to linear amplitude; silent entries give 0.
float dbToAmplitude(float db);

struct Spectrum {
    std::array<float, kSpectrumSize> db{};     // level per harmonic
    std::array<float, kSpectrumSize> phase{};  // start phase per harmonic, in cycles

    Spectrum();
    static Spectrum sine();  // harmonic 1 at 0 dB, the rest silent

    // Moves the loudest entry to 0 dB. Entries at or below -96 dB (the floor of the
    // stored level format) keep their value, which matches the original's behaviour.
    void normalise();
    float amplitude(int harmonic) const { return dbToAmplitude(db[harmonic]); }
};

struct FilterCurve {
    std::array<float, kCurvePoints> db{};

    static FilterCurve flat();  // every point at 0 dB
    void normalise();           // same rule as Spectrum::normalise
    float amplitude(int point) const { return dbToAmplitude(db[point]); }
};

}  // namespace partials
