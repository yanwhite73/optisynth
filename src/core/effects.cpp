#include "effects.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace partials {

namespace {

constexpr double kPi = 3.141592653589793;

float param(const EffectParams& p, const char* key, float def) {
    auto it = p.find(key);
    return it == p.end() || !std::isfinite(it->second) ? def : it->second;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

template <std::size_t N>
double interpolate(double v, const double (&x)[N], const double (&y)[N]) {
    if (v <= x[0]) return y[0];
    for (std::size_t i = 1; i < N; ++i)
        if (v <= x[i]) return y[i-1] + (y[i]-y[i-1]) * (v-x[i-1]) / (x[i]-x[i-1]);
    return y[N-1];
}

// Delay time in ms, from a tempo-synced value in beats when one is set.
double timeMs(const EffectParams& p, const char* msKey, const char* syncKey, double bpm, float defMs) {
    const float beats = param(p, syncKey, 0.0f);
    if (beats > 0.0f) return beats * 60000.0 / bpm;
    return param(p, msKey, defMs);
}

// Triangle LFO, -1..1.
struct Tri {
    double phase = 0.0, inc = 0.0, initialPhase = 0.0;
    void setPhase(double p) { phase=initialPhase=p; }
    void setOffset(double p) { phase+=p-initialPhase;phase-=std::floor(phase);initialPhase=p; }
    void inheritPhase(const Tri& old) { phase=old.phase+initialPhase-old.initialPhase;phase-=std::floor(phase); }
    void setRate(double hz, double sr) { inc = std::max(0.0, hz) / sr; }
    float next() {
        phase += inc;
        if (phase >= 1.0) phase -= std::floor(phase);
        return static_cast<float>(phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
    }
};

struct OnePoleLP {
    float a = 1.0f, y = 0.0f;
    void set(double hz, double sr) {
        a = hz >= 0.49 * sr ? 1.0f : static_cast<float>(1.0 - std::exp(-2.0 * kPi * std::max(hz, 1.0) / sr));
    }
    float operator()(float x) { return y += a * (x - y); }
};

struct OnePoleHP {
    OnePoleLP lp;
    bool off = true;
    void set(double hz, double sr) {
        off = hz <= 20.5;
        lp.set(hz, sr);
    }
    float operator()(float x) { return off ? x : x - lp(x); }
};

float mix(float dry, float wet, float amount) { return dry + (wet - dry) * amount; }

struct SlewTime {
    double value=-1,sampleRate=0,blend=1;
    double next(double target,double sr) {
        if(sampleRate!=sr){sampleRate=sr;blend=-std::expm1(-1./(.005*sr));}
        if(value<0)value=target;
        else value+=(target-value)*blend;
        return value;
    }
};

// ---------------------------------------------------------------------------------------
// Reverb: per channel 8 parallel damped combs, then 2..6 all-pass diffusers (Freeverb-style).
class Reverb final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::reverb;
        preFreeMs_=v[predelay_ms];
        for(auto& ch:comb_)for(auto& k:ch){
            k.fb=static_cast<float>(std::pow(10.,-3.*k.len/(v[time_s]*sr_)));
            k.damp=v[damping];k.modDepth=modulation_?v[mod_depth]*static_cast<float>(sr_/44100.):0.f;
        }
        setTempo(bpm_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Reverb*>(&source);if(!old)return;preTime_=old->preTime_;
        pre_.inheritHistory(old->pre_);
        for(int c=0;c<2;++c){
            for(int i=0;i<8;++i){auto& a=comb_[c][i];auto& b=old->comb_[c][i];a.line.inheritHistory(b.line);a.store=b.store;a.lfo.inheritPhase(b.lfo);}
            for(int i=0;i<6;++i){ap_[c][i].buf.swap(old->ap_[c][i].buf);ap_[c][i].idx=old->ap_[c][i].idx;}
        }
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        static const int kComb[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int kAll[6] = {556, 441, 341, 225, 179, 131};
        const double scale = sr / 44100.0;
        const double t60 = clampf(param(p, "time_s", 2.5f), 0.1f, 50.0f);
        const float damping = clampf(param(p, "damping", 0.15f), 0.0f, 0.95f);
        const bool mod = param(p, "modulation", 0.0f) > 0.5f;
        const float modDepth = mod ? clampf(param(p, "mod_depth", 10.0f), 0.0f, 20.0f) * static_cast<float>(scale) : 0.0f;
        diffusors_ = static_cast<int>(clampf(param(p, "diffusors", 4.0f), 2.0f, 6.0f));
        wet_ = clampf(param(p, "wet", 0.25f), 0.0f, 1.0f);
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < 8; ++i) {
                Comb& k = comb_[c][i];
                k.len = (kComb[i] + (c ? 23 : 0)) * scale;
                k.line.init(static_cast<int>(k.len + 20*scale + 8));
                k.fb = static_cast<float>(std::pow(10.0, -3.0 * k.len / (t60 * sr)));
                k.damp = damping;
                k.modDepth = modDepth;
                k.lfo.setRate(0.3 + 0.11 * i + 0.05 * c, sr);
                k.lfo.setPhase(0.13 * i + 0.5 * c);
            }
            for (int i = 0; i < 6; ++i) {
                ap_[c][i].buf.assign(static_cast<std::size_t>((kAll[i] + (c ? 23 : 0)) * scale) + 1, 0.0f);
                ap_[c][i].idx = 0;
            }
        }
        const double preMs = timeMs(p, "predelay_ms", "predelay_beats", bpm_, 25.0f);
        pre_.init(static_cast<int>(2.0 * sr) + 8);
        preSamples_ = std::clamp(preMs * sr / 1000.0, 1.0, static_cast<double>(pre_.capacity()));
        params_ = p;
        predelayBeats_=param(p,"predelay_beats",0);modulation_=param(p,"modulation",0)>.5f;
        initialiseControls(p,"reverb");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);
        preSamples_=std::clamp((predelayBeats_>0?predelayBeats_*60000./bpm_:preFreeMs_)*sr_/1000.,1.,static_cast<double>(pre_.capacity()));
    }
    void reset() override {
        for (auto& ch : comb_)
            for (auto& k : ch) {
                k.line.clear();
                k.store = 0.0f;
            }
        for (auto& ch : ap_)
            for (auto& a : ch) std::fill(a.buf.begin(), a.buf.end(), 0.0f);
        pre_.clear();
    }
    void process(float* l, float* r, int n) override {
        for (int s = 0; s < n; ++s) {
            const float in = pre_.read(preTime_.next(preSamples_,sr_));
            pre_.push((l[s] + r[s]) * 0.015f);
            float out[2];
            for (int c = 0; c < 2; ++c) {
                float acc = 0.0f;
                for (auto& k : comb_[c]) {
                    const double d = k.len + (k.modDepth > 0.0f ? k.modDepth * 0.5 * (k.lfo.next() + 1.0f) : 0.0);
                    const float y = k.line.read(d);
                    k.store = y * (1.0f - k.damp) + k.store * k.damp;
                    k.line.push(in + k.store * k.fb);
                    acc += y;
                }
                for (int i = 0; i < diffusors_; ++i) {
                    AllPass& a = ap_[c][i];
                    const float b = a.buf[static_cast<std::size_t>(a.idx)];
                    a.buf[static_cast<std::size_t>(a.idx)] = acc + b * 0.5f;
                    acc = b - acc;
                    if (++a.idx == static_cast<int>(a.buf.size())) a.idx = 0;
                }
                out[c] = acc * 3.0f;
            }
            l[s] = mix(l[s], out[0], wet_);
            r[s] = mix(r[s], out[1], wet_);
        }
    }

private:
    SlewTime preTime_;
    float preFreeMs_=25,predelayBeats_=0; bool modulation_=false;
    struct Comb {
        DelayLine line;
        double len = 0.0;
        float fb = 0.0f, damp = 0.0f, store = 0.0f, modDepth = 0.0f;
        Tri lfo;
    };
    struct AllPass {
        std::vector<float> buf;
        int idx = 0;
    };
    double sr_ = 48000.0, preSamples_ = 1.0;
    std::array<std::array<Comb, 8>, 2> comb_{};
    std::array<std::array<AllPass, 6>, 2> ap_{};
    DelayLine pre_;
    int diffusors_ = 4;
    float wet_ = 0.25f;
    EffectParams params_;
};

// ---------------------------------------------------------------------------------------
// Stereo delay: separate left and right times, feedback through low and high cut filters
// (inside or after the loop), optional cross-feedback, stereo width.
class StereoDelay final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::delay_stereo;
        timeLms_=v[time_l_ms];timeRms_=v[time_r_ms];fb_=v[feedback];width_=1+v[stereo];
        for(int c=0;c<2;++c){hp_[c].set(v[highpass_hz],sr_);lp_[c].set(v[lowpass_hz],sr_);}
        setTempo(bpm_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<StereoDelay*>(&source);if(!old)return;leftTime_=old->leftTime_;rightTime_=old->rightTime_;
        dl_.inheritHistory(old->dl_);dr_.inheritHistory(old->dr_);
        for(int c=0;c<2;++c){hp_[c].lp.y=old->hp_[c].lp.y;lp_[c].y=old->lp_[c].y;}
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        params_ = p;
        for (auto* d : {&dl_, &dr_}) d->init(static_cast<int>(8.0 * sr) + 8);
        fb_ = clampf(param(p, "feedback", 0.4f), 0.0f, 0.99f);
        cross_ = param(p, "cross", 0.0f) > 0.5f;
        inLoop_ = param(p, "filters_in_feedback", 1.0f) > 0.5f;
        width_ = 1.0f + clampf(param(p, "stereo", 0.0f), -1.0f, 1.0f);
        wet_ = clampf(param(p, "wet", 0.3f), 0.0f, 1.0f);
        for (int c = 0; c < 2; ++c) {
            hp_[c].set(param(p, "highpass_hz", 20.0f), sr);
            lp_[c].set(param(p, "lowpass_hz", 20480.0f), sr);
        }
        setTempo(bpm_);
        syncL_=param(p,"sync_l_beats",0);syncR_=param(p,"sync_r_beats",0);
        initialiseControls(p,"delay_stereo");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);const double cap=dl_.capacity();
        tl_=std::clamp((syncL_>0?syncL_*60000./bpm_:timeLms_)*sr_/1000.,1.,cap);
        tr_=std::clamp((syncR_>0?syncR_*60000./bpm_:timeRms_)*sr_/1000.,1.,cap);
    }
    void reset() override {  // no allocation: clear state, keep coefficients
        dl_.clear();
        dr_.clear();
        for (int c = 0; c < 2; ++c) {
            hp_[c].lp.y = 0.0f;
            lp_[c].y = 0.0f;
        }
    }
    void process(float* l, float* r, int n) override {
        for (int s = 0; s < n; ++s) {
            float a = dl_.read(leftTime_.next(tl_,sr_)), b = dr_.read(rightTime_.next(tr_,sr_));
            float fa = lp_[0](hp_[0](a)), fb = lp_[1](hp_[1](b));
            const float la = inLoop_ ? fa : a, lb = inLoop_ ? fb : b;
            dl_.push(l[s] + fb_ * (cross_ ? lb : la));
            dr_.push(r[s] + fb_ * (cross_ ? la : lb));
            const float m = 0.5f * (fa + fb), d = 0.5f * (fa - fb) * width_;
            l[s] = mix(l[s], m + d, wet_);
            r[s] = mix(r[s], m - d, wet_);
        }
    }

private:
    SlewTime leftTime_,rightTime_;
    float timeLms_=300,timeRms_=450,syncL_=0,syncR_=0;
    double sr_ = 48000.0, tl_ = 1.0, tr_ = 1.0;
    DelayLine dl_, dr_;
    OnePoleHP hp_[2];
    OnePoleLP lp_[2];
    float fb_ = 0.4f, width_ = 1.0f, wet_ = 0.3f;
    bool cross_ = false, inLoop_ = true;
    EffectParams params_;
};

// ---------------------------------------------------------------------------------------
// Modulated delay: one line per channel, a triangle LFO on the time, feedback through filters.
class ModDelay final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::delay_mod;
        freeMs_=v[time_ms];fb_=v[feedback];depthMs_=v[mod_depth]*10;rateHz_=v[mod_rate_hz];stereo_=v[stereo];
        for(int c=0;c<2;++c){hp_[c].set(v[highpass_hz],sr_);lp_[c].set(v[lowpass_hz],sr_);}
        lfo_[1].setOffset(.25*(1+stereo_));setTempo(bpm_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<ModDelay*>(&source);if(!old)return;baseTime_=old->baseTime_;
        dl_.inheritHistory(old->dl_);dr_.inheritHistory(old->dr_);
        for(int c=0;c<2;++c){hp_[c].lp.y=old->hp_[c].lp.y;lp_[c].y=old->lp_[c].y;lfo_[c].inheritPhase(old->lfo_[c]);}
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        params_ = p;
        for (auto* d : {&dl_, &dr_}) d->init(static_cast<int>(8.9 * sr) + 8);
        fb_ = clampf(param(p, "feedback", 0.4f), 0.0f, 0.99f);
        depthMs_ = clampf(param(p, "mod_depth", 0.0f), 0.0f, 1.0f) * 10.0f;
        stereo_ = clampf(param(p, "stereo", 0.0f), -1.0f, 1.0f);
        wet_ = clampf(param(p, "wet", 0.3f), 0.0f, 1.0f);
        for (int c = 0; c < 2; ++c) {
            hp_[c].set(param(p, "highpass_hz", 20.0f), sr);
            lp_[c].set(param(p, "lowpass_hz", 20480.0f), sr);
        }
        lfo_[1].setPhase(0.25 * (1.0 + stereo_));
        setTempo(bpm_);
        sync_=param(p,"sync_beats",0);modSync_=param(p,"mod_sync_beats",0);
        initialiseControls(p,"delay_mod");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);baseMs_=sync_>0?sync_*60000./bpm_:freeMs_;
        for(auto& o:lfo_)o.setRate(modSync_>0?bpm_/60./modSync_:rateHz_,sr_);
    }
    void reset() override {
        dl_.clear();
        dr_.clear();
    }
    void process(float* l, float* r, int n) override {
        const double cap = dl_.capacity();
        for (int s = 0; s < n; ++s) {
            const double base=baseTime_.next(baseMs_,sr_);
            const double tl = std::clamp((base + depthMs_ * lfo_[0].next()) * sr_ / 1000.0, 1.0, cap);
            const double tr = std::clamp((base * (1.0 + 0.1 * stereo_) + depthMs_ * lfo_[1].next()) * sr_ / 1000.0, 1.0, cap);
            const float a = lp_[0](hp_[0](dl_.read(tl))), b = lp_[1](hp_[1](dr_.read(tr)));
            dl_.push(l[s] + fb_ * a);
            dr_.push(r[s] + fb_ * b);
            l[s] = mix(l[s], a, wet_);
            r[s] = mix(r[s], b, wet_);
        }
    }

private:
    SlewTime baseTime_;
    float freeMs_=300,rateHz_=.5f,sync_=0,modSync_=0;
    double sr_ = 48000.0, baseMs_ = 300.0;
    DelayLine dl_, dr_;
    OnePoleHP hp_[2];
    OnePoleLP lp_[2];
    Tri lfo_[2];
    float fb_ = 0.4f, depthMs_ = 0.0f, stereo_ = 0.0f, wet_ = 0.3f;
    EffectParams params_;
};

// ---------------------------------------------------------------------------------------
// Ping-pong: a centre (pre) delay feeds left and right delays that alternate.
class PingPong final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::delay_pingpong;
        timeLms_=v[time_l_ms];timeRms_=v[time_r_ms];timeCms_=v[time_c_ms];
        fb_=v[feedback];balance_=v[balance];panL_=v[pan_l];panR_=v[pan_r];setTempo(bpm_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<PingPong*>(&source);if(!old)return;preTime_=old->preTime_;leftTime_=old->leftTime_;rightTime_=old->rightTime_;
        dc_.inheritHistory(old->dc_);dl_.inheritHistory(old->dl_);dr_.inheritHistory(old->dr_);
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        params_ = p;
        for (auto* d : {&dc_, &dl_, &dr_}) d->init(static_cast<int>(8.0 * sr) + 8);
        fb_ = clampf(param(p, "feedback", 0.4f), 0.0f, 0.99f);
        balance_ = clampf(param(p, "balance", 0.5f), 0.0f, 1.0f);
        panL_ = clampf(param(p, "pan_l", -1.0f), -1.0f, 1.0f);
        panR_ = clampf(param(p, "pan_r", 1.0f), -1.0f, 1.0f);
        wet_ = clampf(param(p, "wet", 0.3f), 0.0f, 1.0f);
        setTempo(bpm_);
        syncL_=param(p,"sync_l_beats",0);syncR_=param(p,"sync_r_beats",0);syncC_=param(p,"sync_c_beats",0);
        initialiseControls(p,"delay_pingpong");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);const double cap=dl_.capacity();
        tl_=std::clamp((syncL_>0?syncL_*60000./bpm_:timeLms_)*sr_/1000.,1.,cap);
        tr_=std::clamp((syncR_>0?syncR_*60000./bpm_:timeRms_)*sr_/1000.,1.,cap);
        tc_=std::clamp((syncC_>0?syncC_*60000./bpm_:timeCms_)*sr_/1000.,1.,cap);
    }
    void reset() override {
        dc_.clear();
        dl_.clear();
        dr_.clear();
    }
    void process(float* l, float* r, int n) override {
        auto pan = [](float x, float p, float& ol, float& orr) {
            const double a = (p + 1.0) * kPi / 4.0;
            ol += x * static_cast<float>(std::cos(a));
            orr += x * static_cast<float>(std::sin(a));
        };
        for (int s = 0; s < n; ++s) {
            const float pre = dc_.read(preTime_.next(tc_,sr_)), a = dl_.read(leftTime_.next(tl_,sr_)), b = dr_.read(rightTime_.next(tr_,sr_));
            dc_.push(0.5f * (l[s] + r[s]));
            dl_.push(pre + fb_ * b);
            dr_.push(a);
            float wl = (1.0f - balance_) * pre * 0.7071f, wr = wl;
            pan(balance_ * a, panL_, wl, wr);
            pan(balance_ * b, panR_, wl, wr);
            l[s] = mix(l[s], wl, wet_);
            r[s] = mix(r[s], wr, wet_);
        }
    }

private:
    SlewTime preTime_,leftTime_,rightTime_;
    float timeLms_=250,timeRms_=250,timeCms_=0,syncL_=0,syncR_=0,syncC_=0;
    double sr_ = 48000.0, tc_ = 1.0, tl_ = 1.0, tr_ = 1.0;
    DelayLine dc_, dl_, dr_;
    float fb_ = 0.4f, balance_ = 0.5f, panL_ = -1.0f, panR_ = 1.0f, wet_ = 0.3f;
    EffectParams params_;
};

// ---------------------------------------------------------------------------------------
// Chorus: per channel two taps swept by triangle LFOs at slightly different rates.
class Chorus final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::chorus;
        timeMs_=std::clamp(v[time_ms],.5f,40.f);
        for(int c=0;c<2;++c){
            const double rate=std::clamp(v[rate_hz],.01f,10.f);
            lfo_[c][0].setRate(rate,sr_);lfo_[c][1].setRate(rate*(1+.39*v[rate_spread]+.1),sr_);
            lfo_[c][0].setOffset(c*.5*v[stereo]);lfo_[c][1].setOffset(.5+c*.5*v[stereo]);
        }
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Chorus*>(&source);if(!old)return;baseTime_=old->baseTime_;
        dl_.inheritHistory(old->dl_);dr_.inheritHistory(old->dr_);
        for(int c=0;c<2;++c)for(int i=0;i<2;++i)lfo_[c][i].inheritPhase(old->lfo_[c][i]);
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        for (auto* d : {&dl_, &dr_}) d->init(static_cast<int>(0.1 * sr) + 8);
        timeMs_ = clampf(param(p, "time_ms", 8.0f), 0.5f, 40.0f);
        const double rate = clampf(param(p, "rate_hz", 0.5f), 0.01f, 10.0f);
        const float spread = clampf(param(p, "rate_spread", 0.3f), 0.0f, 1.0f);
        const float stereo = clampf(param(p, "stereo", 0.5f), 0.0f, 1.0f);
        invert_ = param(p, "invert", 0.0f) > 0.5f ? -1.0f : 1.0f;
        wet_ = clampf(param(p, "wet", 0.5f), 0.0f, 1.0f);
        for (int c = 0; c < 2; ++c) {
            lfo_[c][0].setRate(rate, sr);
            lfo_[c][1].setRate(rate * (1.0 + 0.39 * spread + 0.1), sr);
            lfo_[c][0].setPhase(c * 0.5 * stereo);
            lfo_[c][1].setPhase(0.5 + c * 0.5 * stereo);
        }
        
        initialiseControls(p,"chorus");
    }
    void reset() override {
        dl_.clear();
        dr_.clear();
    }
    void process(float* l, float* r, int n) override {
        DelayLine* d[2] = {&dl_, &dr_};
        float* io[2] = {l, r};
        const double cap = dl_.capacity();
        for (int s = 0; s < n; ++s) {
            const double base=baseTime_.next(timeMs_,sr_);
            for (int c = 0; c < 2; ++c) {
                float acc = 0.0f;
                for (auto& o : lfo_[c]) {
                    const double t = base * (1.0 + 0.5 * o.next()) * sr_ / 1000.0;
                    acc += d[c]->read(std::clamp(t, 1.0, cap));
                }
                d[c]->push(io[c][s]);
                io[c][s] = mix(io[c][s], 0.5f * acc * invert_, wet_);
            }
        }
    }

private:
    SlewTime baseTime_;
    double sr_ = 48000.0;
    DelayLine dl_, dr_;
    Tri lfo_[2][2];
    float timeMs_ = 8.0f, invert_ = 1.0f, wet_ = 0.5f;
};

// ---------------------------------------------------------------------------------------
// Phaser: 1..24 first-order all-pass stages per channel, swept by a triangle LFO.
class Phaser final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::phaser;
        centre_=v[center_hz];depth_=v[depth];fb_=v[feedback]*feedbackSign_;rateHz_=v[rate_hz];
        lfo_[1].setOffset(.5*v[stereo]);setTempo(bpm_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Phaser*>(&source);if(!old)return;
        z_=old->z_;
        for(int c=0;c<2;++c){last_[c]=old->last_[c];lfo_[c].inheritPhase(old->lfo_[c]);countdown_[c]=0;}
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        params_ = p;
        stages_ = static_cast<int>(clampf(param(p, "stages", 6.0f), 1.0f, 24.0f));
        centre_ = clampf(param(p, "center_hz", 800.0f), 50.0f, 12000.0f);
        depth_ = clampf(param(p, "depth", 0.5f), 0.0f, 1.0f);
        fb_ = clampf(param(p, "feedback", 0.0f), 0.0f, 0.95f) * (param(p, "fb_invert", 0.0f) > 0.5f ? -1.0f : 1.0f);
        wet_ = clampf(param(p, "wet", 0.5f), 0.0f, 1.0f);
        lfo_[1].setPhase(0.5 * clampf(param(p, "stereo", 0.5f), 0.0f, 1.0f));
        setTempo(bpm_);
        sync_=param(p,"sync_beats",0);feedbackSign_=param(p,"fb_invert",0)>.5f?-1.f:1.f;
        initialiseControls(p,"phaser");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);for(auto& o:lfo_)o.setRate(sync_>0?bpm_/60./sync_:rateHz_,sr_);
    }
    void reset() override {
        for (auto& ch : z_) ch.fill(0.0f);
        last_[0] = last_[1] = 0.0f;
    }
    void process(float* l, float* r, int n) override {
        float* io[2] = {l, r};
        for (int s = 0; s < n; ++s) {
            for (int c = 0; c < 2; ++c) {
                const float m = lfo_[c].next();
                if (countdown_[c]-- <= 0) {  // sweep coefficient at control rate (every 16 samples)
                    countdown_[c] = 15;
                    const double f = centre_ * std::pow(2.0, 3.0 * depth_ * m);
                    const double t = std::tan(kPi * std::min(f, 0.45 * sr_) / sr_);
                    coef_[c] = static_cast<float>((t - 1.0) / (t + 1.0));
                }
                const float a = coef_[c];
                float x = io[c][s] + fb_ * last_[c];
                for (int k = 0; k < stages_; ++k) {
                    const float y = a * x + z_[c][static_cast<std::size_t>(k)];
                    z_[c][static_cast<std::size_t>(k)] = x - a * y;
                    x = y;
                }
                last_[c] = x;
                io[c][s] = mix(io[c][s], 0.5f * (io[c][s] + x), wet_);
            }
        }
    }

private:
    float rateHz_=.3f,sync_=0,feedbackSign_=1;
    double sr_ = 48000.0, centre_ = 800.0;
    int stages_ = 6;
    float depth_ = 0.5f, fb_ = 0.0f, wet_ = 0.5f, last_[2] = {0.0f, 0.0f}, coef_[2] = {0.0f, 0.0f};
    int countdown_[2] = {0, 0};
    std::array<std::array<float, 24>, 2> z_{};
    Tri lfo_[2];
    EffectParams params_;
};

// ---------------------------------------------------------------------------------------
// Flanger: a short delay swept by a triangle LFO, with feedback and an optional pan sweep.
class Flanger final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::flanger;
        baseMs_=std::clamp(v[time_ms],.05f,85.f);depth_=v[time_depth];fb_=std::min(v[feedback],.95f)*invWet_;panDepth_=v[pan_depth];rateHz_=v[rate_hz];
        lfo_[1].setOffset(.5*v[stereo]);setTempo(bpm_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Flanger*>(&source);if(!old)return;baseTime_=old->baseTime_;
        dl_.inheritHistory(old->dl_);dr_.inheritHistory(old->dr_);
        for(int c=0;c<2;++c)lfo_[c].inheritPhase(old->lfo_[c]);
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        params_ = p;
        for (auto* d : {&dl_, &dr_}) d->init(static_cast<int>(0.2 * sr) + 8);
        baseMs_ = clampf(param(p, "time_ms", 2.0f), 0.05f, 85.0f);
        depth_ = clampf(param(p, "time_depth", 0.5f), 0.0f, 1.0f);
        const float inv = param(p, "invert", 0.0f) > 0.5f ? -1.0f : 1.0f;
        fb_ = clampf(param(p, "feedback", 0.5f), 0.0f, 0.95f) * inv;
        invWet_ = inv;
        panDepth_ = clampf(param(p, "pan_depth", 0.0f), 0.0f, 1.0f);
        wet_ = clampf(param(p, "wet", 0.5f), 0.0f, 1.0f);
        lfo_[1].setPhase(0.5 * clampf(param(p, "stereo", 0.5f), 0.0f, 1.0f));
        setTempo(bpm_);
        sync_=param(p,"sync_beats",0);
        initialiseControls(p,"flanger");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);for(auto& o:lfo_)o.setRate(sync_>0?bpm_/60./sync_:rateHz_,sr_);
    }
    void reset() override {
        dl_.clear();
        dr_.clear();
    }
    void process(float* l, float* r, int n) override {
        DelayLine* d[2] = {&dl_, &dr_};
        const double cap = dl_.capacity();
        for (int s = 0; s < n; ++s) {
            const double base=baseTime_.next(baseMs_,sr_);
            float out[2];
            float lfoVal = 0.0f;
            for (int c = 0; c < 2; ++c) {
                const float m = lfo_[c].next();
                if (c == 0) lfoVal = m;
                const double t = std::max(0.05, base * (1.0 + depth_ * m)) * sr_ / 1000.0;
                const float y = d[c]->read(std::clamp(t, 1.0, cap));
                d[c]->push((c ? r[s] : l[s]) + fb_ * y);
                out[c] = y * invWet_;
            }
            const float pl = 1.0f - panDepth_ * std::max(0.0f, lfoVal), pr = 1.0f - panDepth_ * std::max(0.0f, -lfoVal);
            l[s] = mix(l[s], 0.5f * (l[s] + out[0] * pl), wet_);
            r[s] = mix(r[s], 0.5f * (r[s] + out[1] * pr), wet_);
        }
    }

private:
    SlewTime baseTime_;
    float rateHz_=.2f,sync_=0;
    double sr_ = 48000.0, baseMs_ = 2.0;
    DelayLine dl_, dr_;
    Tri lfo_[2];
    float depth_ = 0.5f, fb_ = 0.5f, invWet_ = 1.0f, panDepth_ = 0.0f, wet_ = 0.5f;
    EffectParams params_;
};

// ---------------------------------------------------------------------------------------
// Shared building blocks for the remaining effects.

// State-variable filter (trapezoidal form). mode: 0 low-pass, 1 high-pass, 2 band-pass, 3 notch.
struct Svf {
    float ic1 = 0.0f, ic2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.4f;
    int mode = 0;
    void set(double hz, float resonance, double sr) {
        setDamping(hz, 2.0f - 1.94f * std::clamp(resonance, 0.0f, 1.0f), sr);
    }
    void setDamping(double hz, float damping, double sr) {
        const double g = std::tan(kPi * std::clamp(hz, 10.0, 0.45 * sr) / sr);
        k = std::clamp(damping, 0.06f, 2.0f);
        a1 = static_cast<float>(1.0 / (1.0 + g * (g + k)));
        a2 = static_cast<float>(g) * a1;
        a3 = static_cast<float>(g) * a2;
    }
    float operator()(float x) {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        switch (mode) {
            case 1: return x - k * v1 - v2;
            case 2: return v1 * k;  // unity peak
            case 3: return x - k * v1;
            default: return v2;
        }
    }
    void clear() { ic1 = ic2 = 0.0f; }
};

float softClip(float x) { return x / (1.0f + std::fabs(x)); }
float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }

// Filter effect: drive, a resonant multimode filter and a triangle LFO on the cutoff.
class FilterFx final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::filter;
        cutoff_=v[cutoff_hz];res_=v[resonance];depth_=v[mod_depth];rateHz_=v[mod_rate_hz];level_=dbGain(v[level_db]);
        const double settings[]={0,.1,.2,.35,.5,.75,1};
        const double powers[]={1,.98061,.94006,.82821,.63834,.22255,.00175};
        const double gains[]={1,.842542,.714045,.565208,.464006,.373560,.347807};
        power_=static_cast<float>(interpolate(v[saturation],settings,powers));
        saturationGain_=static_cast<float>(interpolate(v[saturation],settings,gains));
        setTempo(bpm_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<FilterFx*>(&source);if(!old)return;
        for(int c=0;c<2;++c){svf_[c].ic1=old->svf_[c].ic1;svf_[c].ic2=old->svf_[c].ic2;}
        lfo_.inheritPhase(old->lfo_);count_=0;
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        params_ = p;
        mode_ = static_cast<int>(clampf(param(p, "type", 0.0f), 0.0f, 3.0f));
        // Original behavioural fit to the round-2 sine pairs, not a binary lookup table.
        // See spectral_architecture.md: saturation reduces gain and tends toward a square.
        const double sat = clampf(param(p, "saturation", 0.0f), 0.0f, 1.0f);
        const double settings[] = {0, .1, .2, .35, .5, .75, 1};
        const double powers[] = {1, .98061, .94006, .82821, .63834, .22255, .00175};
        const double gains[] = {1, .842542, .714045, .565208, .464006, .373560, .347807};
        power_ = static_cast<float>(interpolate(sat, settings, powers));
        saturationGain_ = static_cast<float>(interpolate(sat, settings, gains));
        cutoff_ = clampf(param(p, "cutoff_hz", 2000.0f), 20.0f, 20480.0f);
        res_ = clampf(param(p, "resonance", 0.0f), 0.0f, 1.0f);
        depth_ = clampf(param(p, "mod_depth", 0.0f), 0.0f, 1.0f);
        level_ = dbGain(clampf(param(p, "level_db", 0.0f), -60.0f, 24.0f));
        wet_ = clampf(param(p, "wet", 1.0f), 0.0f, 1.0f);
        for (auto& f : svf_) f.mode = mode_;
        setTempo(bpm_);
        reset();
        sync_=param(p,"sync_beats",0);
        initialiseControls(p,"filter");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);lfo_.setRate(sync_>0?bpm_/60./sync_:rateHz_,sr_);
    }
    void reset() override {
        for (auto& f : svf_) f.clear();
        lfo_.setPhase(.25);  // measured triangle starts at its midpoint, rising
        count_ = 0;
    }
    void process(float* l, float* r, int n) override {
        float* io[2] = {l, r};
        for (int s = 0; s < n; ++s) {
            const float m = lfo_.next();
            if (count_-- <= 0) {  // coefficients at control rate
                count_ = 15;
                // Measured effective cutoff is the displayed value / pi, with a triangle
                // sweep of +-3 octaves at full depth. Damping is a low-input sine fit;
                // the original's level-dependent resonance remains approximate.
                const double hz = cutoff_ / kPi * std::pow(2.0, 3.0 * depth_ * m);
                const double settings[] = {0, .25, .5, .75, .9, 1};
                const double damping[] = {1.012, .584, .315, .209, .189, .18};
                const float k = static_cast<float>(interpolate(res_, settings, damping));
                for (auto& f : svf_) f.setDamping(hz, k, sr_);
            }
            for (int c = 0; c < 2; ++c) {
                const float x = io[c][s];
                const float shaped = power_ == 1.0f || x == 0.0f ? x :
                    std::copysign(saturationGain_ * std::pow(std::fabs(x), power_), x);
                const float y = svf_[c](shaped) * level_ / (mode_ == 2 ? svf_[c].k : 1.0f);
                io[c][s] = mix(x, y, wet_);
            }
        }
    }

private:
    float rateHz_=.5f,sync_=0;
    double sr_ = 48000.0;
    Svf svf_[2];
    Tri lfo_;
    int mode_ = 0, count_ = 0;
    float power_ = 1.0f, saturationGain_ = 1.0f;
    float cutoff_ = 2000.0f, res_ = 0.0f, depth_ = 0.0f, level_ = 1.0f, wet_ = 1.0f;
    EffectParams params_;
};

// Compressor: stereo-linked level detection in dB, threshold and ratio, attack and release.
class Compressor final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::compressor;
        ratio_=v[ratio];thr_=v[threshold_db];
        aAtt_=static_cast<float>(std::exp(-1000./(std::max(.05f,v[attack_ms])*sr_)));
        aRel_=static_cast<float>(std::exp(-1000./(v[release_ms]*sr_)));
        out_=dbGain(v[out_db]-thr_*(1-1/ratio_));
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Compressor*>(&source);if(!old)return;
        env_=old->env_;
    }
    void prepare(double sr, const EffectParams& p) override {
        ratio_ = clampf(param(p, "ratio", 2.0f), 1.0f, 40.0f);
        thr_ = clampf(param(p, "threshold_db", -12.0f), -60.0f, 0.0f);
        const double att = clampf(param(p, "attack_ms", 10.0f), 0.05f, 1000.0f) / 1000.0;
        const double rel = clampf(param(p, "release_ms", 200.0f), 1.0f, 5000.0f) / 1000.0;
        aAtt_ = static_cast<float>(std::exp(-1.0 / (att * sr)));
        aRel_ = static_cast<float>(std::exp(-1.0 / (rel * sr)));
        // Full automatic make-up is measured. Detector offset is provisional: together with
        // the peak follower it reads a steady C3 sine about 2.44 dB below its peak.
        out_ = dbGain(clampf(param(p, "out_db", 0.0f), -60.0f, 24.0f) - thr_ * (1.0f - 1.0f / ratio_));
        wet_ = clampf(param(p, "wet", 1.0f), 0.0f, 1.0f);
        sr_=sr;
        initialiseControls(p,"compressor");
    }
    void reset() override { env_ = 0.0f; }
    void process(float* l, float* r, int n) override {
        for (int s = 0; s < n; ++s) {
            const float x = std::max(std::fabs(l[s]), std::fabs(r[s]));
            const float a = x > env_ ? aAtt_ : aRel_;
            env_ = x + (env_ - x) * a;
            const float detectorDb = 20.0f * std::log10(std::max(env_, 1e-12f)) - 2.2f;
            const float over = detectorDb - thr_;
            const float g = over > 0.0f ? dbGain(-over * (1.0f - 1.0f / ratio_)) * out_ : out_;
            l[s] = mix(l[s], l[s] * g, wet_);
            r[s] = mix(r[s], r[s] * g, wet_);
        }
    }

private:
    double sr_=48000;
    float ratio_ = 2.0f, thr_ = -12.0f, aAtt_ = 0.0f, aRel_ = 0.0f, out_ = 1.0f, wet_ = 1.0f, env_ = 0.0f;
};

// Crusher: continuous averaging or sample-rate division (step/slide), bit reduction with optional dither,
// and a filter before or after the divider.
class Crusher final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::crusher;
        bits_=v[bits];divider_=v[divider];
        phase_=std::fmod(phase_,divider_);
        for(auto& f:svf_)f.set(v[cutoff_hz],v[resonance],sr_);
        filterOn_=v[cutoff_hz]<20000.f||svf_[0].mode!=0;
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Crusher*>(&source);if(!old)return;
        phase_=std::fmod(old->phase_,divider_);seed_=old->seed_;
        for(int c=0;c<2;++c){held_[c]=old->held_[c];prev_[c]=old->prev_[c];svf_[c].ic1=old->svf_[c].ic1;svf_[c].ic2=old->svf_[c].ic2;}
    }
    void prepare(double sr, const EffectParams& p) override {
        bits_ = clampf(param(p, "bits", 16.0f), 1.0f, 16.01f);
        dither_ = param(p, "dither", 0.0f) > 0.5f;
        divMode_ = static_cast<int>(clampf(param(p, "divider_mode", 1.0f), 0.0f, 2.0f));
        divider_ = clampf(param(p, "divider", 1.0f), 1.0f, 32.0f);  // samples, fractional
        post_ = param(p, "filter_post", 0.0f) > 0.5f;
        autoVol_ = param(p, "auto_volume", 0.0f) > 0.5f;
        wet_ = clampf(param(p, "wet", 1.0f), 0.0f, 1.0f);
        const int mode = static_cast<int>(clampf(param(p, "filter_type", 0.0f), 0.0f, 3.0f));
        for (auto& f : svf_) {
            f.mode = mode;
            f.set(clampf(param(p, "cutoff_hz", 20480.0f), 20.0f, 20480.0f), clampf(param(p, "resonance", 0.0f), 0.0f, 1.0f), sr);
        }
        filterOn_ = param(p, "cutoff_hz", 20480.0f) < 20000.0f || mode != 0;
        sr_=sr;filterBlend_=filterOn_?1.f:0.f;
        initialiseControls(p,"crusher");
    }
    void reset() override {
        for (auto& f : svf_) f.clear();
        phase_ = 0.0f;
        held_[0] = held_[1] = prev_[0] = prev_[1] = 0.0f;
    }
    void process(float* l, float* r, int n) override {
        float* io[2] = {l, r};
        const float steps = std::pow(2.0f, bits_ - 1.0f);
        for (int s = 0; s < n; ++s) {
            float x[2] = {l[s], r[s]};
            filterBlend_+=(static_cast<float>(filterOn_)-filterBlend_)*static_cast<float>(-std::expm1(-1./(.004*sr_)));
            if(!post_)for(int c=0;c<2;++c)x[c]=mix(x[c],svf_[c](x[c]),filterBlend_);
            if (divMode_ == 0) {
                // Measured AVG transfer at dividers 2.5/4/8: a one-pole updated every
                // sample, alpha=1/divider. It does not decimate or hold sample batches.
                for (int c = 0; c < 2; ++c) held_[c] += (x[c] - held_[c]) / divider_;
            } else if ((phase_ += 1.0f) >= divider_) {
                phase_ -= divider_;
                for (int c = 0; c < 2; ++c) {
                    prev_[c] = held_[c];
                    held_[c] = x[c];
                }
            }
            for (int c = 0; c < 2; ++c) {
                // slide: a straight line from the previous held sample to the current one
                float y = divMode_ == 2 ? prev_[c] + (held_[c] - prev_[c]) * std::min(1.0f, phase_ / divider_) : held_[c];
                if (dither_) {
                    seed_ = seed_ * 1664525u + 1013904223u;
                    y += ((seed_ >> 8) / 16777216.0f - 0.5f) / steps;
                }
                y = std::round(y * steps) / steps;
                if(post_)y=mix(y,svf_[c](y),filterBlend_);
                if (autoVol_) y *= 1.0f / (1.0f + 0.02f * (16.0f - bits_));  // our compensation
                io[c][s] = mix(io[c][s], y, wet_);
            }
        }
    }

private:
    double sr_=48000; float filterBlend_=0;
    Svf svf_[2];
    float bits_ = 16.0f, wet_ = 1.0f, divider_ = 1.0f, phase_ = 0.0f, held_[2] = {0, 0}, prev_[2] = {0, 0};
    int divMode_ = 1;
    bool dither_ = false, post_ = false, autoVol_ = false, filterOn_ = false;
    unsigned seed_ = 7u;
};

// Gator: two 16-step on/off patterns at a synced step length, with smoothed edges.
class Gator final : public Effect {
public:

    void setControls(const float* v) override {
        smooth_=v[fxparam::gator::smooth];
        const double settings[]={0,.05,.1,.25,.5,.75,.9,1};
        const double seconds[]={.0007,.0009,.0009,.002,.004,.014,.036,.132};
        slew_=static_cast<float>(1.-std::exp(-1./(interpolate(smooth_,settings,seconds)*sr_)));
    }
    void setWet(float value) override { wet_=value;closed_=(1-value)*(1-value); }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Gator*>(&source);if(!old)return;
        phase_=old->phase_;g_[0]=old->g_[0];g_[1]=old->g_[1];
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        params_ = p;
        patL_ = static_cast<unsigned>(clampf(param(p, "pattern_l", 43690.0f), 0.0f, 65535.0f));
        patR_ = static_cast<unsigned>(clampf(param(p, "pattern_r", 21845.0f), 0.0f, 65535.0f));
        link_ = static_cast<int>(clampf(param(p, "link", 0.0f), 0.0f, 2.0f));
        smooth_ = clampf(param(p, "smooth", 0.1f), 0.0f, 1.0f);
        wet_ = clampf(param(p, "wet", 1.0f), 0.0f, 1.0f);
        closed_ = (1.0f - wet_) * (1.0f - wet_);  // measured at 25/50/75% wet
        stepBeats_ = clampf(param(p, "step_beats", 0.25f), 1.0f / 64.0f, 64.0f);
        // Provisional causal approximation: measured low/mid settings have similar times
        // at different step lengths. Full Smooth has unresolved phase/rate dependence.
        const double settings[] = {0, .05, .1, .25, .5, .75, .9, 1};
        const double seconds[] = {.0007, .0009, .0009, .002, .004, .014, .036, .132};
        const double tau = interpolate(smooth_, settings, seconds);
        slew_ = static_cast<float>(1.0 - std::exp(-1.0 / (tau * sr_)));
        setTempo(bpm_);
        
        initialiseControls(p,"gator");
    }
    void setTempo(double bpm) override {
        Effect::setTempo(bpm);
        increment_ = bpm_ / (60.0 * stepBeats_ * sr_);
    }
    void setSongPosition(double ppq) override {
        if (!std::isfinite(ppq)) return;
        phase_ = std::fmod(ppq, 16.0 * stepBeats_) / stepBeats_;
        if (phase_ < 0.0) phase_ += 16.0;
    }
    void reset() override {
        phase_ = 0.0;
        g_[0] = g_[1] = 0.0f;
    }
    void process(float* l, float* r, int n) override {
        for (int s = 0; s < n; ++s) {
            const int step = std::min(15, static_cast<int>(phase_ + 1e-10));
            const bool onL = (patL_ >> (15 - step)) & 1u;
            bool onR = (patR_ >> (15 - step)) & 1u;
            if (link_ == 1) onR = onL;        // link: right follows left
            else if (link_ == 2) onR = !onL;  // cross-link: right is the inverse of left
            const float t[2] = {onL ? 1.0f : closed_, onR ? 1.0f : closed_};
            for (int c = 0; c < 2; ++c) g_[c] += slew_ * (t[c] - g_[c]);
            // Exactly dry stays bit-identical, including the initial smoothing transient.
            if (wet_ > 0.0f) { l[s] *= g_[0]; r[s] *= g_[1]; }
            phase_ += increment_;
            if (phase_ >= 16.0 - 1e-10) phase_ = std::max(0.0, phase_ - 16.0);
        }
    }

private:
    double sr_ = 48000.0, phase_ = 0.0, increment_ = 1.0 / 6000.0, stepBeats_ = 0.25;
    unsigned patL_ = 0, patR_ = 0;
    int link_ = 0;
    float smooth_ = 0.1f, slew_ = 1.0f, wet_ = 1.0f, closed_ = 0.0f, g_[2] = {0, 0};
    EffectParams params_;
};

// Parametric EQ: two peaking bands plus low and high shelves (standard biquad designs).
class Eq final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::eq;
        set(band_[0],0,v[freq1_hz],v[gain1_db],v[q1],sr_);
        set(band_[1],0,v[freq2_hz],v[gain2_db],v[q2],sr_);
        set(band_[2],1,v[low_hz],v[low_db],.707f,sr_);
        set(band_[3],2,v[high_hz],v[high_db],.707f,sr_);
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Eq*>(&source);if(!old)return;
        for(int i=0;i<4;++i)for(int c=0;c<2;++c)for(int z=0;z<2;++z)band_[i].z[c][z]=old->band_[i].z[c][z];
    }
    void prepare(double sr, const EffectParams& p) override {
        set(band_[0], 0, clampf(param(p, "freq1_hz", 500.0f), 20.0f, 20480.0f), param(p, "gain1_db", 0.0f), param(p, "q1", 1.0f), sr);
        set(band_[1], 0, clampf(param(p, "freq2_hz", 2000.0f), 20.0f, 20480.0f), param(p, "gain2_db", 0.0f), param(p, "q2", 1.0f), sr);
        set(band_[2], 1, clampf(param(p, "low_hz", 100.0f), 20.0f, 1000.0f), param(p, "low_db", 0.0f), 0.707f, sr);
        set(band_[3], 2, clampf(param(p, "high_hz", 8000.0f), 440.0f, 11000.0f), param(p, "high_db", 0.0f), 0.707f, sr);
        wet_ = clampf(param(p, "wet", 1.0f), 0.0f, 1.0f);
        sr_=sr;
        initialiseControls(p,"eq");
    }
    void reset() override {
        for (auto& b : band_) b.z[0][0] = b.z[0][1] = b.z[1][0] = b.z[1][1] = 0.0f;
    }
    void process(float* l, float* r, int n) override {
        float* io[2] = {l, r};
        for (int s = 0; s < n; ++s) {
            for (int c = 0; c < 2; ++c) {
                float y = io[c][s];
                for (auto& b : band_) {
                    // Trapezoidal integrator state does not encode the old feed-forward
                    // coefficients. Preserve it as cutoff/Q/gain move.
                    const double v3=y-b.z[c][1];
                    const double v1=b.denom*(b.z[c][0]+b.g*v3);
                    const double v2=b.z[c][1]+b.g*v1;
                    b.z[c][0]=2*v1-b.z[c][0];b.z[c][1]=2*v2-b.z[c][1];
                    double o=b.m0*y+b.m1*v1+b.m2*v2;
                    // Independent protection against invalid/hostile input.
                    if(!std::isfinite(o)||!std::isfinite(b.z[c][0])||!std::isfinite(b.z[c][1])
                        ||std::abs(o)>1.e6f||std::abs(b.z[c][0])>1.e6f||std::abs(b.z[c][1])>1.e6f){
                        o=0;b.z[c][0]=b.z[c][1]=0;
                    }
                    y = static_cast<float>(o);
                }
                io[c][s] = mix(io[c][s], y, wet_);
            }
        }
    }

private:
    double sr_=48000;
    struct Biquad {
        double g=0,denom=1,m0=1,m1=0,m2=0,z[2][2]={{0,0},{0,0}};
    };
    static void set(Biquad& f, int kind, double hz, float db, float q, double sr) {
        db = std::isfinite(db) ? clampf(db, -24.0f, 24.0f) : 0.0f;
        q = std::isfinite(q) ? clampf(q, 0.08f, 12.5f) : 1.0f;
        hz = std::clamp(hz, 10.0, 0.45 * sr);  // a band above Nyquist would make the filter unstable
        const double A = std::pow(10.0, db / 40.0), w = 2.0 * kPi * hz / sr, cw = std::cos(w), sw = std::sin(w);
        const double alpha = sw / (2.0 * q);
        double b0, b1, b2, a0, a1, a2;
        if (kind == 0) {  // peaking
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
        } else {
            const double sq = 2.0 * std::sqrt(A) * alpha, sgn = kind == 1 ? 1.0 : -1.0;  // 1 low shelf, 2 high shelf
            b0 = A * ((A + 1) - sgn * (A - 1) * cw + sq);
            b1 = sgn * 2 * A * ((A - 1) - sgn * (A + 1) * cw);
            b2 = A * ((A + 1) - sgn * (A - 1) * cw - sq);
            a0 = (A + 1) + sgn * (A - 1) * cw + sq;
            a1 = -sgn * 2 * ((A - 1) + sgn * (A + 1) * cw);
            a2 = (A + 1) + sgn * (A - 1) * cw - sq;
        }
        // Same RBJ transfer function, expressed as input/band/low-pass mixes of a
        // TPT SVF. Derive in double precision, before near-cancelling coefficients
        // at low frequencies are rounded to float.
        const double minus=a0-a1+a2,plus=a0+a1+a2;
        f.g=std::sqrt(std::max(1.e-18,plus/minus));
        const double k=2*(a0-a2)/(minus*f.g);
        f.denom=1/(1+f.g*(f.g+k));
        f.m0=(b0-b1+b2)/minus;
        f.m2=(b0+b1+b2)/plus-f.m0;
        f.m1=((b0/a0-f.m0)*(4*a0/minus)-f.m2*f.g*f.g)/f.g;
    }
    Biquad band_[4];
    float wet_ = 1.0f;
};

// Wah-wah: an envelope follower moves a resonant band-pass up from its base cutoff.
class Wah final : public Effect {
public:

    void setControls(const float* v) override {
        using namespace fxparam::wah;
        base_=v[cutoff_hz];res_=v[resonance];sens_=v[sensitivity];level_=v[level];
        aAtt_=static_cast<float>(std::exp(-1./(v[attack_s]*sr_)));
        aRel_=static_cast<float>(std::exp(-1./(v[release_s]*sr_)));
    }
    void setWet(float value) override { wet_=value; }
    void inheritState(Effect& source) override {
        auto* old=dynamic_cast<Wah*>(&source);if(!old)return;
        env_=old->env_;count_=0;
        for(int c=0;c<2;++c){svf_[c].ic1=old->svf_[c].ic1;svf_[c].ic2=old->svf_[c].ic2;}
    }
    void prepare(double sr, const EffectParams& p) override {
        sr_ = sr;
        base_ = clampf(param(p, "cutoff_hz", 400.0f), 20.0f, 20480.0f);
        res_ = clampf(param(p, "resonance", 0.5f), 0.0f, 1.0f);
        sens_ = clampf(param(p, "sensitivity", 0.01f), -1.0f, 1.0f);  // signed: negative sweeps down
        level_ = clampf(param(p, "level", 1.0f), 0.0f, 10.0f);          // linear, on dry and wet
        aAtt_ = static_cast<float>(std::exp(-1.0 / (clampf(param(p, "attack_s", 0.01f), 0.001f, 8.0f) * sr)));
        aRel_ = static_cast<float>(std::exp(-1.0 / (clampf(param(p, "release_s", 0.1f), 0.001f, 8.0f) * sr)));
        wet_ = clampf(param(p, "wet", 1.0f), 0.0f, 1.0f);
        for (auto& f : svf_) f.mode = 2;
        
        initialiseControls(p,"wah");
    }
    void reset() override {
        for (auto& f : svf_) f.clear();
        env_ = 0.0f;
    }
    void process(float* l, float* r, int n) override {
        float* io[2] = {l, r};
        for (int s = 0; s < n; ++s) {
            const float x = std::max(std::fabs(l[s]), std::fabs(r[s]));
            env_ = x + (env_ - x) * (x > env_ ? aAtt_ : aRel_);
            if (count_-- <= 0) {
                count_ = 15;
                // our law (not measured): 100 x sensitivity octaves per unit envelope, within +-5
                const double hz = base_ * std::pow(2.0, clampf(100.0f * sens_ * env_, -5.0f, 5.0f));
                for (auto& f : svf_) f.set(hz, res_, sr_);
            }
            for (int c = 0; c < 2; ++c) {
                const float y = svf_[c](io[c][s]);
                io[c][s] = mix(io[c][s], softClip(y * 0.5f) * 2.0f, wet_) * level_;
            }
        }
    }

private:
    double sr_ = 48000.0, base_ = 400.0;
    Svf svf_[2];
    float res_ = 0.5f, sens_ = 0.01f, level_ = 1.0f, aAtt_ = 0.0f, aRel_ = 0.0f, wet_ = 1.0f, env_ = 0.0f;
    int count_ = 0;
};

}  // namespace

void OutputStage::prepare(double sampleRate) {
    const double sr = std::isfinite(sampleRate) && sampleRate >= 8000.0 ? sampleRate : 48000.0;
    release_ = std::exp(-1.0 / (.1 * sr));  // chosen, not a measured release time
    reset();
}

void OutputStage::process(double& left, double& right) {
    if (!std::isfinite(left) || !std::isfinite(right)) {
        left = right = 0.0;
        reset();
        return;
    }
    const double peak = std::max(std::fabs(left), std::fabs(right));
    envelope_ = peak > envelope_ ? peak : peak + (envelope_ - peak) * release_;
    double gain = 1.0;
    if (envelope_ > 1.0) {
        // Small measured transfer curve fitted in probe_fx_analyse.py model. These are
        // behavioural observations, not copied proprietary lookup data. Interpolate in
        // amplitude; the unmeasured range above the last probe approaches a safe ceiling.
        const double input[] = {1, 1.3646, 1.9275, 2.7227, 3.846, 5.433, 7.674};
        const double beforeShape[] = {1, 1.310577, 1.598973, 1.825177, 1.998341, 2.128206, 2.224061};
        const double target = envelope_ <= input[6] ? interpolate(envelope_, input, beforeShape) :
            2.35 - (2.35 - beforeShape[6]) * input[6] / envelope_;
        gain = target / envelope_;
    }
    for (double* channel : {&left, &right}) {
        double y = *channel * gain;
        const double over = std::fabs(y) - 1.0;
        if (over > 0.0) y = std::copysign(1.0 + over / (1.0 + .3 * over), y);
        *channel = std::clamp(y, -2.0, 2.0);  // independent last-resort host protection
    }
}

void DelayLine::init(int maxSamples) {
    size_ = std::max(4, maxSamples + 2);
    buf_.assign(static_cast<std::size_t>(size_), 0.0f);
    write_ = 0;
}

void DelayLine::clear() {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    write_ = 0;
}

float DelayLine::read(double delay) const {
    if (size_ < 4) return 0.0f;
    delay = std::clamp(delay, 1.0, static_cast<double>(size_ - 2));
    double pos = write_ - delay;
    if (pos < 0) pos += size_;
    int i0 = static_cast<int>(pos);
    const float f = static_cast<float>(pos - i0);
    if (i0 >= size_) i0 -= size_;
    int i1 = i0 + 1;
    if (i1 >= size_) i1 = 0;
    return buf_[static_cast<std::size_t>(i0)] * (1.0f - f) + buf_[static_cast<std::size_t>(i1)] * f;
}

std::unique_ptr<Effect> makeEffect(const std::string& type) {
    if (type == "reverb") return std::make_unique<Reverb>();
    if (type == "delay_stereo") return std::make_unique<StereoDelay>();
    if (type == "delay_mod") return std::make_unique<ModDelay>();
    if (type == "delay_pingpong") return std::make_unique<PingPong>();
    if (type == "chorus") return std::make_unique<Chorus>();
    if (type == "phaser") return std::make_unique<Phaser>();
    if (type == "flanger") return std::make_unique<Flanger>();
    if (type == "filter") return std::make_unique<FilterFx>();
    if (type == "compressor") return std::make_unique<Compressor>();
    if (type == "crusher") return std::make_unique<Crusher>();
    if (type == "gator") return std::make_unique<Gator>();
    if (type == "eq") return std::make_unique<Eq>();
    if (type == "wah") return std::make_unique<Wah>();
    return nullptr;
}

bool effectImplemented(const std::string& type) { return makeEffect(type) != nullptr; }

}  // namespace partials
