#include "arp.h"

#include <algorithm>
#include <cmath>

namespace partials {

void Arpeggiator::configure(const ArpParams& p) {
    p_ = p;
    heldCount_ = 0;
    running_ = false;
    soundingCount_ = 0;
    gateOpen_ = false;
    step_ = 0;
    index_ = -1;
    dir_ = 1;
}

void Arpeggiator::keyDown(int pitch, float velocity) {
    for (int i = 0; i < heldCount_; ++i) {
        if (held_[static_cast<std::size_t>(i)].pitch == pitch) return;
    }
    if (heldCount_ >= static_cast<int>(held_.size())) return;
    held_[static_cast<std::size_t>(heldCount_++)] = {pitch, velocity, orderCounter_++};
    lastVelocity_ = velocity;
}

void Arpeggiator::keyUp(int pitch) {
    for (int i = 0; i < heldCount_; ++i) {
        if (held_[static_cast<std::size_t>(i)].pitch == pitch) {
            held_[static_cast<std::size_t>(i)] = held_[static_cast<std::size_t>(heldCount_ - 1)];
            --heldCount_;
            return;
        }
    }
}

void Arpeggiator::allUp() { heldCount_ = 0; }

int Arpeggiator::buildList(int* pitches, float* velocities) const {
    std::array<Held, 32> base{};
    for (int i = 0; i < heldCount_; ++i) base[static_cast<std::size_t>(i)] = held_[static_cast<std::size_t>(i)];
    if (p_.mode == 10) {  // as played
        std::sort(base.begin(), base.begin() + heldCount_, [](const Held& a, const Held& b) { return a.order < b.order; });
    } else {
        std::sort(base.begin(), base.begin() + heldCount_, [](const Held& a, const Held& b) { return a.pitch < b.pitch; });
    }
    int n = 0;
    for (int o = 0; o < p_.octaves; ++o) {
        for (int i = 0; i < heldCount_ && n < 128; ++i) {
            pitches[n] = base[static_cast<std::size_t>(i)].pitch + 12 * o;
            velocities[n] = base[static_cast<std::size_t>(i)].velocity;
            ++n;
        }
    }
    return n;
}

int Arpeggiator::nextIndex(int n) {
    // Measured on the original (8 November): every mode starts on the first note of the list
    // (lowest, or first played), then moves. Down-based modes move downwards from there, so
    // down wraps to the top and down/up bounces straight off the bottom.
    if (n <= 1) return index_ = 0;
    if (index_ < 0) {
        dir_ = (p_.mode == 3 || p_.mode == 6 || p_.mode == 7) ? -1 : 1;
        return index_ = 0;
    }
    index_ = std::min(index_, n - 1);
    switch (p_.mode) {
        case 3:  // down
            index_ = (index_ - 1 + n) % n;
            break;
        case 4:  // up/down and down/up: bounce, ends not repeated
        case 6:
            index_ += dir_;
            if (index_ >= n) { index_ = std::max(0, n - 2); dir_ = -1; }
            if (index_ < 0) { index_ = std::min(1, n - 1); dir_ = 1; }
            break;
        case 5:  // up/down+ and down/up+: bounce, ends repeated
        case 7:
            index_ += dir_;
            if (index_ >= n) { index_ = n - 1; dir_ = -1; }
            if (index_ < 0) { index_ = 0; dir_ = 1; }
            break;
        case 8:  // random
            rng_ = rng_ * 1664525u + 1013904223u;
            index_ = static_cast<int>((rng_ >> 8) % static_cast<unsigned>(n));
            break;
        default:  // up, as played
            index_ = (index_ + 1) % n;
            break;
    }
    return index_;
}

void Arpeggiator::releaseSounding(Event* out, int& count) {
    for (int i = 0; i < soundingCount_ && count < kMaxEvents; ++i)
        out[count++] = {false, sounding_[static_cast<std::size_t>(i)], 0.0f};
    soundingCount_ = 0;
    gateOpen_ = false;
}

void Arpeggiator::triggerStep(Event* out, int& count) {
    const ArpStep& st = p_.steps[static_cast<std::size_t>(step_)];
    step_ = (step_ + 1) % std::max(1, p_.length);
    if (st.velocity != 128) stepVelocity_ = st.velocity / 127.0f;
    stepTranspose_ = st.transpose / 48.0f;
    stepLength_ = st.length;
    const double gate = std::clamp(st.length * p_.noteLength * 2.0, 0.02, 1.0);
    if (p_.mode == 1) return;  // mod-only: the pattern drives the matrix sources only

    if (st.velocity == 128) {  // hold: the previous note continues through this step
        if (soundingCount_ > 0) {
            gateOpen_ = true;
            gateSamples_ = pos_ + stepSamples_ * gate;
        }
        return;
    }
    if (st.velocity == 0) {  // rest
        releaseSounding(out, count);
        return;
    }
    int pitches[128];
    float vels[128];
    const int n = buildList(pitches, vels);
    if (n == 0) return;
    int chosen[32];
    float chosenVel[32];
    int k = 0;
    if (p_.mode == 9) {  // chord: every held note, one octave step per pattern step
        const int heldN = std::min(heldCount_, 32);
        for (int i = 0; i < heldN; ++i) {
            chosen[k] = pitches[i] + 12 * chordOctave_;
            chosenVel[k++] = vels[i];
        }
        chordOctave_ = (chordOctave_ + 1) % std::max(1, p_.octaves);
    } else {
        int idx = st.advance > 0 ? ((index_ < 0 ? 0 : index_) + st.advance) % n : nextIndex(n);
        index_ = idx;
        chosen[k] = pitches[idx];
        chosenVel[k++] = vels[idx];
    }
    const bool legato = st.legato && soundingCount_ > 0;
    if (!legato) releaseSounding(out, count);
    int prev[32];
    const int prevCount = soundingCount_;
    for (int i = 0; i < prevCount; ++i) prev[i] = sounding_[static_cast<std::size_t>(i)];
    soundingCount_ = 0;
    for (int i = 0; i < k && count < kMaxEvents; ++i) {
        const int pitch = std::clamp(chosen[i] + st.transpose, 0, 127);
        const float velocity = std::clamp((1.0f - p_.veloBlend) * st.velocity / 127.0f + p_.veloBlend * chosenVel[i], 0.0f, 1.0f);
        out[count++] = {true, pitch, velocity};
        sounding_[static_cast<std::size_t>(soundingCount_++)] = pitch;
    }
    for (int i = 0; i < prevCount && count < kMaxEvents; ++i) out[count++] = {false, prev[i], 0.0f};  // legato: new first
    gateOpen_ = true;
    gateSamples_ = stepSamples_ * gate;
}

int Arpeggiator::advance(int frames, double bpm, double sampleRate, Event* out) {
    int count = 0;
    if (!generatesNotes() && !runs()) return 0;
    const double beat = 60.0 / std::max(20.0, bpm) * sampleRate;
    const double base = std::max(1.0, p_.resolutionBeats * beat);
    if (heldCount_ == 0) {
        if (running_) {
            releaseSounding(out, count);
            running_ = false;
            step_ = 0;
            index_ = -1;
            chordOctave_ = 0;
        }
        return count;
    }
    if (!running_) {
        running_ = true;
        pos_ = 0.0;
        step_ = 0;
        index_ = -1;
        stepSamples_ = base * (1.0 + 0.5 * p_.swing);
        triggerStep(out, count);
    }
    for (int i = 0; i < frames; ++i) {
        pos_ += 1.0;
        if (gateOpen_ && pos_ >= gateSamples_) releaseSounding(out, count);
        if (pos_ >= stepSamples_) {
            pos_ -= stepSamples_;
            // swing: even steps long, odd steps short (step_ is already the next step's index)
            stepSamples_ = base * (step_ % 2 == 0 ? 1.0 + 0.5 * p_.swing : 1.0 - 0.5 * p_.swing);
            triggerStep(out, count);
        }
    }
    return count;
}

}  // namespace partials
