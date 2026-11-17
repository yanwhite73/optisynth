// AHDSFR envelope: Attack, Hold, Decay, Sustain, Fade, Release.
//
// Segment shapes and Fade are measured on the original (8 November); see envelope.cpp.
#pragma once

namespace partials {

struct EnvelopeParams {
    float attack = 0.0f, hold = 0.0f, decay = 0.0f, sustain = 1.0f, fade = 0.0f, release = 0.01f;
    float attackCurve = 1.0f, decayCurve = 1.0f, fadeCurve = 1.0f, releaseCurve = 1.0f;
    bool ignoreNoteOff = false;
};

class Envelope {
public:
    void start(const EnvelopeParams& p);
    void updateParams(const EnvelopeParams& p);  // allocation-free; preserve level and stage progress
    void noteOff();
    // Advances by dt seconds and returns the new level (0..1).
    float advance(float dt);
    bool finished() const { return stage_ == Stage::Done; }
    float level() const { return level_; }

private:
    enum class Stage { Attack, Hold, Decay, Sustain, Fade, Release, Done };
    void enter(Stage s);

    EnvelopeParams p_{};
    Stage stage_ = Stage::Done;
    float t_ = 0.0f;          // time in the current stage
    float level_ = 0.0f;
    float from_ = 0.0f;       // level at the start of the current stage
    float anchor_ = 0.0f;     // phase at the latest live edit (remaining curve is rebased here)
};

}  // namespace partials
