// Arpeggiator and step sequencer: turns held keys into a stepped note pattern.
// Behaviour follows ../reverse-engineer/docs/spectral_architecture.md ("Arpeggiator and step
// sequencer"). Provisional, our own choices where the original is not yet measured: the gate is
// step length x note length x 2; swing lengthens even steps and shortens odd ones by swing/2;
// rests do not consume a note.
//
// Real-time safe: fixed arrays only; no allocation, locks or files.
#pragma once

#include <array>

#include "patch.h"

namespace partials {

class Arpeggiator {
public:
    struct Event {
        bool on = true;
        int pitch = 60;
        float velocity = 1.0f;
    };
    static constexpr int kMaxEvents = 64;

    void configure(const ArpParams& p);
    void updateParams(const ArpParams& p) {
        p_ = p;
        if (step_ >= p_.length) step_ = 0;
        if (chordOctave_ >= p_.octaves) chordOctave_ = 0;
    }
    bool generatesNotes() const { return p_.mode >= 2; }
    bool runs() const { return p_.mode >= 1; }
    void keyDown(int pitch, float velocity);
    void keyUp(int pitch);
    void allUp();
    // Advances by frames; writes note events (at most kMaxEvents) and returns their count.
    int advance(int frames, double bpm, double sampleRate, Event* out);

    // Current step values, as matrix sources: velocity /127, transpose /48, length 0..1.
    float stepVelocity() const { return stepVelocity_; }
    float stepTranspose() const { return stepTranspose_; }
    float stepLength() const { return stepLength_; }

private:
    struct Held {
        int pitch;
        float velocity;
        unsigned order;
    };
    int buildList(int* pitches, float* velocities) const;  // the note list for this mode, expanded by octaves
    int nextIndex(int n);
    void triggerStep(Event* out, int& count);
    void releaseSounding(Event* out, int& count);

    ArpParams p_{};
    std::array<Held, 32> held_{};
    int heldCount_ = 0;
    unsigned orderCounter_ = 0;
    bool running_ = false;
    double pos_ = 0.0;         // samples into the current step
    double stepSamples_ = 1.0, gateSamples_ = 0.0;
    bool gateOpen_ = false;
    int step_ = 0, index_ = -1, dir_ = 1, chordOctave_ = 0;
    std::array<int, 32> sounding_{};
    int soundingCount_ = 0;
    unsigned rng_ = 22222u;
    float lastVelocity_ = 1.0f;
    float stepVelocity_ = 0.0f, stepTranspose_ = 0.0f, stepLength_ = 0.0f;
};

}  // namespace partials
