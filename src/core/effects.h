// Effects for the six FX slots. Our own DSP, following the structures documented in
// ../reverse-engineer/docs/spectral_architecture.md ("Effects"). Where a published design
// exists we use it (the reverb is Freeverb-style, with Jezar's public-domain tunings).
//
// Real-time rules: prepare() allocates (call it at load time); process() and reset() never
// allocate, lock or touch files.
#pragma once

#include <map>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "effect_controls.h"

namespace partials {

using EffectParams = std::map<std::string, float>;

class Effect {
public:
    virtual ~Effect() = default;
    virtual void prepare(double sampleRate, const EffectParams& params) = 0;
    virtual void setTempo(double bpm) { bpm_ = bpm > 1.0 ? bpm : 120.0; }
    // Optional host position at the start of the next process call, in quarter notes.
    // Effects without transport synchronisation ignore it. No allocation or locking.
    virtual void setSongPosition(double) {}
    virtual void process(float* left, float* right, int frames) = 0;  // in place
    virtual void reset() = 0;
    virtual void setWet(float) {}  // already-clamped realtime control, no allocation
    // Physical values in this type's canonical local order (effect_controls.h).
    // Engine supplies smoothed, clamped values. Never allocate or reset history.
    virtual void setControls(const float*) = 0;
    void updateControls(const float* values) {
        if(std::equal(values,values+controlCount_,lastControls_.begin()))return;
        setControls(values);
        std::copy(values,values+controlCount_,lastControls_.begin());
    }
    virtual void inheritState(Effect&) {}  // same-type edit: transfer history, keep new coefficients

protected:
    double bpm_ = 120.0;
    void initialiseControls(const EffectParams& p,std::string_view type) {
        std::array<float,10> values{};
        const int offset=effectControlOffset(type);
        for(int i=offset;i<kEffectControls&&type==effectControlSpecs[i].type;++i){
            const auto& s=effectControlSpecs[i];auto it=p.find(s.key);
            const float v=it==p.end()||!std::isfinite(it->second)?s.initial:it->second;
            values[i-offset]=std::clamp(v,s.minimum,s.maximum);
            controlCount_=i-offset+1;
        }
        setControls(values.data());
        lastControls_=values;
    }
    std::array<float,10> lastControls_{};
    int controlCount_=0;
};

// All 13 types are implemented; an unknown type returns nullptr and the slot passes audio
// through unchanged.
std::unique_ptr<Effect> makeEffect(const std::string& type);
bool effectImplemented(const std::string& type);

// Our measured steady-state approximation of the original output dynamics. Stereo linked,
// finite and bounded even during transients; prepare/reset/process never allocate.
class OutputStage {
public:
    void prepare(double sampleRate);
    void reset() { envelope_ = 0.0; }
    void process(double& left, double& right);
private:
    double envelope_ = 0.0, release_ = 0.0;
};

// A delay line with a fractional (linearly interpolated) read.
class DelayLine {
public:
    void init(int maxSamples);
    void clear();
    void inheritHistory(DelayLine& old) {
        if(size_==old.size_) {buf_.swap(old.buf_);std::swap(write_,old.write_);return;}
        // New storage was prepared on the loader. Preserve chronological samples even
        // when a reverb modulation depth changes the required delay capacity.
        write_=0;
        for(int age=1;age<=std::min(size_,old.size_);++age)
            buf_[static_cast<std::size_t>(size_-age)]=old.buf_[static_cast<std::size_t>((old.write_+old.size_-age)%old.size_)];
    }
    void push(float x) {
        buf_[static_cast<std::size_t>(write_)] = x;
        if (++write_ == size_) write_ = 0;
    }
    // delay in samples, at least 1, at most size - 2
    float read(double delay) const;
    int capacity() const { return size_ > 3 ? size_ - 2 : 1; }  // safe even before init()

private:
    std::vector<float> buf_;
    int size_ = 0, write_ = 0;
};

}  // namespace partials
