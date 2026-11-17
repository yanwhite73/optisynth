#include "envelope.h"

#include <algorithm>
#include <cmath>

namespace partials {

namespace {

// Segment shape, measured on the original (8 November): curve 1 is linear; other values follow
// y = (e^(kx) - 1) / (e^k - 1) with k = 8.25 (curve - 1), so 0.2 is fast (concave) and 1.8 slow.
float shape(float x, float c) {
    x = std::clamp(x, 0.0f, 1.0f);
    const float k = 8.25f * (c - 1.0f);
    if (std::fabs(k) < 1e-3f) return x;
    // Keep the measured float calculation in its safe range for old preset recall.
    if(k<80.f)return std::expm1(k * x) / std::expm1(k);
    return static_cast<float>(std::expm1(static_cast<double>(k)*x)/std::expm1(static_cast<double>(k)));
}
float rise(float x, float c,float anchor=0) {
    if(anchor<=0)return shape(x,c);
    const double a=shape(anchor,c),b=shape(x,c);
    return static_cast<float>(std::clamp((b-a)/std::max(1.e-12,1.-a),0.,1.));
}
float fall(float x, float c,float anchor=0) { return 1.0f - rise(x,c,anchor); }

}  // namespace

void Envelope::start(const EnvelopeParams& p) {
    p_ = p;
    level_ = 0.0f;
    enter(Stage::Attack);
}

void Envelope::noteOff() {
    if (p_.ignoreNoteOff || stage_ == Stage::Done || stage_ == Stage::Release) return;
    enter(Stage::Release);
}

void Envelope::updateParams(const EnvelopeParams& p) {
    float before=0,after=0,curveBefore=1,curveAfter=1;
    bool rebase=false;
    switch(stage_) {
        case Stage::Attack: before=p_.attack;after=p.attack;curveBefore=p_.attackCurve;curveAfter=p.attackCurve;break;
        case Stage::Hold: before=p_.hold;after=p.hold;break;
        case Stage::Decay: before=p_.decay;after=p.decay;curveBefore=p_.decayCurve;curveAfter=p.decayCurve;rebase=p.sustain!=p_.sustain;break;
        case Stage::Release: before=p_.release;after=p.release;curveBefore=p_.releaseCurve;curveAfter=p.releaseCurve;break;
        default:break;
    }
    if(before!=after||curveBefore!=curveAfter||rebase) {
        anchor_=before>0?std::clamp(t_/before,0.f,1.f):0.f;
        t_=anchor_*after;from_=level_;
    }
    if(stage_==Stage::Fade && p.sustain!=p_.sustain && p_.sustain>0)
        level_=std::clamp(level_*p.sustain/p_.sustain,0.f,1.f);
    p_=p;
}

void Envelope::enter(Stage s) {
    stage_ = s;
    t_ = 0.0f;
    from_ = level_;
    anchor_ = 0.0f;
}

float Envelope::advance(float dt) {
    // Zero-length stages pass straight through, so loop until time is spent in a stage.
    for (int guard = 0; guard < 8; ++guard) {
        switch (stage_) {
            case Stage::Attack:
                if (p_.attack <= 0.0f) { level_ = 1.0f; enter(Stage::Hold); continue; }
                t_ += dt;
                level_ = from_ + (1.0f - from_) * rise(t_ / p_.attack, p_.attackCurve,anchor_);
                if (t_ >= p_.attack) { level_ = 1.0f; enter(Stage::Hold); }
                return level_;
            case Stage::Hold:
                if (p_.hold <= 0.0f || (t_ += dt) >= p_.hold) { enter(Stage::Decay); continue; }
                return level_;
            case Stage::Decay:
                if (p_.decay <= 0.0f) { level_ = p_.sustain; enter(Stage::Sustain); continue; }
                t_ += dt;
                level_ = p_.sustain + (from_ - p_.sustain) * fall(t_ / p_.decay, p_.decayCurve,anchor_);
                if (t_ >= p_.decay) { level_ = p_.sustain; enter(Stage::Sustain); }
                return level_;
            case Stage::Sustain:
                if(level_!=p_.sustain){
                    level_ += (p_.sustain-level_)*static_cast<float>(-std::expm1(-dt/.004f));
                    if(std::abs(level_-p_.sustain)<1.e-6f)level_=p_.sustain;
                }
                if (p_.fade != 0.0f) { enter(Stage::Fade); continue; }
                if (p_.ignoreNoteOff) { enter(Stage::Release); continue; }
                return level_;
            case Stage::Fade:
                if(p_.fade==0.f){enter(Stage::Sustain);continue;}
                // Fade, measured: a slope of about Fade dB per second while the key is held,
                // upwards for positive values, capped at full level (inferred from two probes).
                level_ = std::clamp(level_ * std::pow(10.0f, p_.fade * dt / 20.0f), 0.0f, 1.0f);
                if (level_ < 1e-5f) { level_ = 0.0f; enter(Stage::Done); }
                return level_;
            case Stage::Release:
                if (p_.release <= 0.0f) { level_ = 0.0f; enter(Stage::Done); return level_; }
                t_ += dt;
                level_ = from_ * fall(t_ / p_.release, p_.releaseCurve,anchor_);
                if (t_ >= p_.release) { level_ = 0.0f; enter(Stage::Done); }
                return level_;
            case Stage::Done:
                level_ = 0.0f;
                return level_;
        }
    }
    return level_;
}

}  // namespace partials
