#include "spectrum.h"

#include <cmath>

namespace partials {

namespace {

template <std::size_t N>
void normaliseDb(std::array<float, N>& db) {
    float peak = -140.0f;
    for (float v : db) {
        if (v > peak) peak = v;
    }
    for (float& v : db) {
        if (v > -95.999f) v -= peak;
    }
}

}  // namespace

float dbToAmplitude(float db) {
    if (db <= kSilentDb + 1.0f) return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

Spectrum::Spectrum() { db.fill(kSilentDb); }

Spectrum Spectrum::sine() {
    Spectrum s;
    s.db[1] = 0.0f;
    return s;
}

void Spectrum::normalise() { normaliseDb(db); }

FilterCurve FilterCurve::flat() {
    FilterCurve c;
    c.db.fill(0.0f);
    return c;
}

void FilterCurve::normalise() { normaliseDb(db); }

}  // namespace partials
