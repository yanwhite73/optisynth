// Original 48 kHz rack DSP. See docs/research/modules/GATE_AND_LOOP_DELAY.md.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace optisynth {

class OpticalGateChannel {
public:
    float process(float input, float control, float strike, float keyStrike, float decay) noexcept {
        if (strike > .1f && !strikeHigh) excitation = std::max(excitation, std::min(strike, 1.f));
        strikeHigh = strike > .1f;
        excitation = std::max(excitation, keyStrike);
        const float target = std::clamp(control + excitation, 0.f, 1.f);
        excitation *= .988f;
        // Fast illumination and a slower, damping-dependent recovery couple
        // loudness and cutoff. A held Strike fires once; Control sustains.
        opening += (target > opening ? .012f : decay) * (target - opening);
        const float cutoff = 40.f + 17000.f * opening * opening;
        const float omega = cutoff * (6.2831853f / 48000.f);
        filtered += (omega / (1.f + omega)) * (input - filtered);
        return opening * filtered;
    }
    static float recovery(float damp) noexcept {
        const float seconds = .32f * std::exp2(-5.f * std::clamp(damp, 0.f, 1.f));
        return -std::expm1(-1.f / (48000.f * seconds));
    }
private:
    float excitation = 0, opening = 0, filtered = 0;
    bool strikeHigh = false;
};

class ClockDelayChannel {
public:
    // Eight quarter notes at the minimum rack tempo (20 BPM).
    static constexpr int capacity = 24 * 48000 + 1;
    ClockDelayChannel() : memory(capacity, 0.f) {}

    void configure(int samples, bool hold, bool reverse) noexcept {
        samples = std::clamp(samples, 1, capacity - 1);
        const int nextDirection = reverse ? -1 : 1;
        if (hold == held && nextDirection == direction && (held || length == samples)) return;
        fadeFrom = lastWet;
        fade = 96;
        if (hold && !held) {
            loopLength = length > 0 ? length : samples;
            loopStart = wrap(write - direction * loopLength);
            capturedDirection = direction;
            phase = 0;
        }
        if (held || hold) {
            if (nextDirection != direction)
                phase = wrapPhase(phase - direction * capturedDirection);
            if (!hold)
                write = wrap(wrap(loopStart + capturedDirection * phase) + nextDirection * samples);
        } else if (nextDirection != direction) {
            // The first reversed repeat starts at the most recently recorded sample.
            write = wrap(write - direction * (samples + 1));
        }
        length = samples;
        direction = nextDirection;
        held = hold;
    }

    float process(float input, float feed, float feedback) noexcept {
        float wet;
        if (held) {
            wet = memory[wrap(loopStart + capturedDirection * phase)];
            phase = wrapPhase(phase + direction * capturedDirection);
        } else {
            wet = memory[wrap(write - direction * length)];
            memory[write] = std::clamp(input * feed + wet * feedback, -4.f, 4.f);
            write = wrap(write + direction);
        }
        if (fade > 0) {
            wet += (fadeFrom - wet) * (float(fade) / 96.f);
            --fade;
        }
        lastWet = wet;
        return wet;
    }
private:
    static int wrap(int pos) noexcept {
        if (pos >= capacity) return pos - capacity;
        if (pos < 0) return pos + capacity;
        return pos;
    }
    int wrapPhase(int pos) const noexcept {
        if (pos < 0) return loopLength - 1;
        return pos == loopLength ? 0 : pos;
    }
    std::vector<float> memory;
    int write = 0, length = 0, direction = 1, capturedDirection = 1;
    int loopStart = 0, loopLength = 1, phase = 0, fade = 0;
    float fadeFrom = 0, lastWet = 0;
    bool held = false;
};
}
