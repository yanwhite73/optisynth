// Analog-style voice modules. Our own implementations from the circuit study in
// docs/research/instruments/CLASSIC_PAD_ARCHITECTURE.md; no third-party DSP code.
#include "analog.h"
#include "gate_delay.h"
#include "../core/effects.h"
#include <cmath>
#include <stdexcept>

namespace optisynth {
namespace {
constexpr double pi = 3.141592653589793;
constexpr float rate = 48000.f;

// Rational tanh approximation, exact at 0 and saturating to +-1 beyond |x| = 3.
inline float softClip(float x) noexcept {
    x = std::clamp(x, -3.f, 3.f);
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}
inline uint32_t hashMix(uint32_t x) noexcept {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
}
inline uint32_t hashString(const std::string& s) noexcept {
    uint32_t h = 2166136261U;
    for (unsigned char c : s) { h ^= c; h *= 16777619U; }
    return h;
}

// Shared parameter handling: saved values, then 5 ms smoothing of continuous controls.
class Base : public AudioModule {
public:
    explicit Base(const NodeSpec& node) : definition(panel(node.type)) {
        for (std::size_t i = 0; i < definition.parameters.size(); ++i) {
            values[i] = definition.parameters[i].initial;
            controls->values[i].store(values[i]);
        }
        for (const auto& saved : node.parameters) {
            const auto& id = saved.first;
            const float value = saved.second;
            auto it = std::find_if(definition.parameters.begin(), definition.parameters.end(),
                                   [&](const Parameter& p) { return p.id == id; });
            if (it == definition.parameters.end() || !std::isfinite(value) || value < it->minimum || value > it->maximum)
                throw std::invalid_argument("Invalid parameter: " + node.id + "." + id);
            const auto i = std::size_t(it - definition.parameters.begin());
            controls->values[i].store(value);
            values[i] = value;
        }
    }
protected:
    void wake() noexcept override { snap = true; }
    void update(const AudioBlock& b) noexcept {
        const float amount = -std::expm1(-b.frames / (rate * .005f));
        const float rate_ = snap ? 1.f : amount; snap = false;
        for (std::size_t i = 0; i < definition.parameters.size(); ++i) {
            const auto& p = definition.parameters[i];
            float target = b.parameters ? b.parameters[i] : controls->values[i].load(std::memory_order_relaxed);
            if (!std::isfinite(target)) target = p.initial;
            target = std::clamp(target, p.minimum, p.maximum);
            if (p.step >= 1) values[i] = std::round(target);
            else values[i] += rate_ * (target - values[i]);
        }
    }
    const Panel& definition;
    std::array<float, 32> values{};
    bool snap = false;
};

// --- CEM3340-style VCO -----------------------------------------------------
// Saw, pulse and triangle with PolyBLEP correction, exponential FM, pulse-width
// modulation, hard sync, and a per-voice tolerance offset plus slow random drift.
class Vco final : public Base {
public:
    Vco(const NodeSpec& n, const PrepareContext& c) : Base(n) {
        seed = hashMix(hashString(n.id) ^ hashMix(uint32_t(c.voice) + 1U));
        offsetCents = (float(hashMix(seed) & 0xffff) / 32767.5f - 1.f);
        driftTarget = random();
        phase = double(hashMix(seed ^ 0x9e3779b9U) & 0xffff) / 65536.0;
    }
    void process(const AudioBlock& b) noexcept override {
        update(b);
        const float transpose = values[0], fine = values[1], saw = values[2], pulse = values[3], tri = values[4];
        const float width = values[5], fmDepth = values[6], pwmDepth = values[7], drift = values[8], level = values[9];
        const float* fm = b.inputs[0];
        const float* pwm = b.inputs[1];
        const float* sync = b.inputs[2];
        const float normal = 1.f / std::max(1.f, saw + pulse + tri);
        for (int i = 0; i < b.frames; ++i) {
            if (--driftCountdown <= 0) {
                driftTarget = random();
                driftCountdown = int(rate * (.4f + .8f * std::abs(random())));
            }
            driftState += (driftTarget - driftState) * 4.0e-5f;  // ~0.5 s glide between targets
            const float cents = fine + drift * (6.f * offsetCents + 12.f * driftState);
            const float pitch = float(b.note.midi) + b.note.bend + transpose + cents * .01f;
            float hz = 440.f * std::exp2((pitch - 69.f) / 12.f);
            if (fmDepth > 0) hz *= std::exp2(fmDepth * std::clamp(fm[i], -4.f, 4.f));
            const double inc = std::clamp(double(hz) / rate, 1e-6, .45);
            // Hard sync on a rising zero crossing of the sync input.
            const float s = sync[i];
            if (lastSync <= 0.f && s > 0.f && lastSync != s) {
                const double frac = double(-lastSync / (s - lastSync));
                phase = (1.0 - frac) * inc;
            }
            lastSync = s;
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            const float t = float(phase), dt = float(inc);
            float value = 0;
            if (saw > 0) value += saw * (2.f * t - 1.f - blep(t, dt));
            if (pulse > 0) {
                const float pw = std::clamp(width + pwmDepth * pwm[i], .03f, .97f);
                float p = t < pw ? 1.f : -1.f;
                p += blep(t, dt);
                float t2 = t - pw; if (t2 < 0) t2 += 1.f;
                p -= blep(t2, dt);
                // Zero-mean pulse: a voice waking from sleep must not start with a DC step.
                value += pulse * (p - (2.f * pw - 1.f));
            }
            if (tri > 0) value += tri * (1.f - 4.f * std::abs(t - .5f));
            b.outputs[0][i] = value * normal * level;
        }
    }
private:
    static float blep(float t, float dt) noexcept {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    float random() noexcept { seed = seed * 1664525U + 1013904223U; return float(seed >> 8) / 8388607.5f - 1.f; }
    uint32_t seed = 1;
    double phase = 0;
    float offsetCents = 0, driftState = 0, driftTarget = 0, lastSync = 0;
    int driftCountdown = 0;
};

// --- 2x halfband resampler (31-tap windowed sinc) ---------------------------
struct Halfband {
    std::array<float, 32> z{};
    int w = 0;
    static const std::array<float, 31>& taps() {
        static const std::array<float, 31> h = [] {
            std::array<float, 31> t{};
            for (int i = 0; i < 31; ++i) {
                const double n = i - 15;
                const double sinc = n == 0 ? .5 : std::sin(pi * n / 2) / (pi * n);
                t[size_t(i)] = float(sinc * (.5 + .5 * std::cos(pi * n / 16)));
            }
            return t;
        }();
        return h;
    }
    float push(float x) noexcept {
        z[size_t(w)] = x;
        const auto& h = taps();
        float y = 0;
        for (int i = 0; i < 31; ++i) if (i == 15 || (i & 1) == 0) y += h[size_t(i)] * z[size_t((w - i) & 31)];
        w = (w + 1) & 31;
        return y;
    }
};

// --- Oberheim-style filter ---------------------------------------------------
// 2-pole: OTA state-variable (SEM/OB-X/OB-Xa 2-pole) with diode-limited resonance
// and the SEM LP->notch->HP mode crossfade or band-pass. 4-pole: OTA cascade
// (OB-Xa/OB-8 4-pole, CEM3320). Pole mixing (Xpander/Matrix-12): weighted sums of
// the cascade input and stage outputs give 15 responses from the 4-pole core.
// Quality: 1x, 2x or 4x oversampling of the nonlinear core.
class ObFilter final : public Base {
public:
    explicit ObFilter(const NodeSpec& n) : Base(n) {}
    // Coefficients on {input, stage 1, stage 2, stage 3, stage 4}: binomial forms of
    // A = one-pole low-pass, (1 - A) = one-pole high-pass, (1 - 2A) = one-pole all-pass.
    static constexpr float mixes[15][5] = {
        {0, 0, 0, 0, 1}, {0, 0, 0, 1, 0}, {0, 0, 1, 0, 0}, {0, 1, 0, 0, 0},            // LP 4, 3, 2, 1
        {1, -3, 3, -1, 0}, {1, -2, 1, 0, 0}, {1, -1, 0, 0, 0},                         // HP 3, 2, 1
        {0, 0, 4, -8, 4}, {0, 2, -2, 0, 0}, {1, -2, 2, 0, 0}, {1, -6, 12, -8, 0},      // BP 4, BP 2, notch, phase
        {0, 1, -3, 3, -1}, {0, 1, -2, 1, 0}, {0, 1, -2, 2, 0}, {0, 1, -6, 12, -8}};    // + 1-pole LP variants
    void process(const AudioBlock& b) noexcept override {
        update(b);
        const float cutoff = values[0], res = values[1], mode = values[2], band = values[3], poles = values[4];
        const float envOct = values[5], key = values[6], drive = values[7], fmDepth = values[8];
        const int poleMix = int(values[9]), factor = values[10] < .5f ? 1 : values[10] < 1.5f ? 2 : 4;
        const float driveGain = 1.f + 5.f * drive, driveNorm = 1.f / std::sqrt(driveGain);
        const float damping = std::pow(.025f, res);   // 2-pole: 1 (Q 0.5) to 0.025
        const float k = 4.15f * res;                   // 4-pole: self-oscillation near 4
        const float keyOct = key * (float(b.note.midi) + b.note.bend - 60.f) / 12.f;
        const bool cascade = poleMix > 0 || poles >= .5f;
        const float* coefficients = mixes[std::clamp(poleMix > 0 ? poleMix - 1 : 0, 0, 14)];
        auto tick = [&](float u, float g) {
            return cascade ? ladder(u, g, k, coefficients) : svf(u, g, damping, mode, band >= .5f);
        };
        for (int i = 0; i < b.frames; ++i) {
            float oct = keyOct + envOct * b.inputs[1][i];
            if (fmDepth > 0) oct += fmDepth * std::clamp(b.inputs[2][i], -4.f, 4.f);
            const float hz = std::clamp(cutoff * std::exp2(std::clamp(oct, -12.f, 12.f)), 10.f, 22000.f);
            const float g = float(std::tan(pi * hz / (double(factor) * rate)));
            const float x = softClip(b.inputs[0][i] * driveGain) * driveNorm;
            float y = 0;
            if (factor == 1) y = tick(x, g);
            else if (factor == 2) {
                for (int os = 0; os < 2; ++os) y = down.push(tick(up.push(os == 0 ? 2.f * x : 0.f), g));
            } else {
                for (int a = 0; a < 2; ++a) {
                    const float u1 = up.push(a == 0 ? 2.f * x : 0.f);
                    float d1 = 0;
                    for (int c = 0; c < 2; ++c) d1 = down2.push(tick(up2.push(c == 0 ? 2.f * u1 : 0.f), g));
                    y = down.push(d1);
                }
            }
            // Output DC block (~2 Hz), as after the Prophet-5 VCF (68 kOhm / 2.2 uF).
            const float blocked = y - dcIn + .99974f * dcOut;
            dcIn = y; dcOut = std::isfinite(blocked) ? blocked : 0.f;
            b.outputs[0][i] = dcOut;
        }
        if (!std::isfinite(s1) || !std::isfinite(s2)) s1 = s2 = 0;
        for (auto& s : stage) if (!std::isfinite(s)) s = 0;
    }
private:
    float svf(float x, float g, float r, float mode, bool band) noexcept {
        const float hp = (x - (2.f * r + g) * s1 - s2) / (1.f + 2.f * r * g + g * g);
        const float v1 = g * hp, bp = v1 + s1;
        s1 = 2.5f * softClip((bp + v1) * .4f);       // diode pair limits the band-pass state
        const float v2 = g * bp, lp = v2 + s2;
        s2 = lp + v2;
        if (band) return bp * .7f;
        return (1.f - mode) * lp + mode * hp;          // centre of the sweep is the notch
    }
    float ladder(float x, float g, float k, const float* mix) noexcept {
        const float G = g / (1.f + g);
        const float one = 1.f - G;
        const float sum = G * G * G * one * stage[0] + G * G * one * stage[1] + G * one * stage[2] + one * stage[3];
        const float g4 = G * G * G * G;
        const float y4 = (g4 * x + sum) / (1.f + k * g4);
        float u = softClip(x - k * y4);                 // input OTA saturation
        float out = mix[0] * u;
        for (int i = 0; i < 4; ++i) {
            auto& s = stage[size_t(i)];
            const float v = (u - s) * G;
            const float y = v + s;
            s = y + v;
            u = y;
            out += mix[i + 1] * y;
        }
        return out;
    }
    Halfband up, down, up2, down2;
    float s1 = 0, s2 = 0, dcIn = 0, dcOut = 0;
    std::array<float, 4> stage{};
};

// --- Prophet-style poly-mod ----------------------------------------------------
// Per-voice mix of oscillator B (audio) and the filter envelope (control), routed to
// oscillator A pitch/PW and the filter; destination depth knobs act as the switches.
class PolyMod final : public Base {
public:
    explicit PolyMod(const NodeSpec& n) : Base(n) {}
    void process(const AudioBlock& b) noexcept override {
        update(b);
        for (int i = 0; i < b.frames; ++i) {
            const float v = values[0] * b.inputs[0][i] + values[1] * b.inputs[1][i];
            b.outputs[0][i] = v;
            b.outputs[1][i] = v;
        }
    }
};

// --- CEM3310-style envelope --------------------------------------------------
// RC stages with exponential time; attack charges toward 1.3 and turns at 1.0,
// so it rises fast and turns sharply at the peak. Attack restarts from the current level.
class AnalogEnv final : public Base {
public:
    explicit AnalogEnv(const NodeSpec& n) : Base(n) {}
    void process(const AudioBlock& b) noexcept override {
        update(b);
        const auto coefficient = [](float seconds, float constants) {
            return 1.f - std::exp(-constants / (std::max(seconds, 1e-4f) * rate));
        };
        const float attack = coefficient(values[0], 1.466f), decay = coefficient(values[1], 4.6f);
        const float sustain = values[2], release = coefficient(values[3], 4.6f), velocityAmount = values[4];
        if (b.note.trigger) stage = Stage::Attack;
        else if (!b.note.gate && stage != Stage::Release && stage != Stage::Idle) stage = Stage::Release;
        else if (b.note.gate && stage == Stage::Idle) stage = Stage::Attack;
        if (b.note.trigger || (b.note.gate && stage == Stage::Attack))
            scale = 1.f - velocityAmount + velocityAmount * std::clamp(b.note.velocity, 0.f, 1.f);
        for (int i = 0; i < b.frames; ++i) {
            switch (stage) {
                case Stage::Attack: level += (1.3f - level) * attack; if (level >= 1.f) { level = 1.f; stage = Stage::Decay; } break;
                case Stage::Decay: level += (sustain - level) * decay; break;
                case Stage::Release: level += (0.f - level) * release; if (level < 1e-5f) { level = 0; stage = Stage::Idle; } break;
                case Stage::Idle: break;
            }
            b.outputs[0][i] = level * scale;
        }
    }
private:
    enum class Stage { Idle, Attack, Decay, Release } stage = Stage::Idle;
    float level = 0, scale = 1;
};

// --- OTA VCA -------------------------------------------------------------------
class OtaVca final : public Base {
public:
    explicit OtaVca(const NodeSpec& n) : Base(n) {}
    void process(const AudioBlock& b) noexcept override {
        update(b);
        const float level = values[0], driveGain = 1.f + 4.f * values[1];
        for (int i = 0; i < b.frames; ++i)
            b.outputs[0][i] = softClip(b.inputs[0][i] * driveGain) / driveGain * std::max(0.f, b.inputs[1][i]) * level;
    }
};

// --- Performance controls -------------------------------------------------------
class Wheel final : public Base {
public:
    explicit Wheel(const NodeSpec& n) : Base(n) {}
    void process(const AudioBlock& b) noexcept override {
        update(b);
        for (int i = 0; i < b.frames; ++i) {
            smoothedWheel += (b.note.wheel - smoothedWheel) * .002f;
            b.outputs[0][i] = smoothedWheel * values[0];
            b.outputs[1][i] = std::clamp(b.note.bend / 12.f, -1.f, 1.f);
        }
    }
private:
    float smoothedWheel = 0;
};

// --- Voice mixer: per-voice constant-power pan spread (OB-X/Xpander style) ------
class VoiceMix final : public VoiceMixer {
public:
    explicit VoiceMix(const NodeSpec& n) : base(n) { controls = base.controls(); }
    int voices() const noexcept override {
        const float v = controls->values[0].load(std::memory_order_relaxed);
        return std::isfinite(v) ? std::clamp(int(std::lround(v)), 1, maximumVoices) : maximumVoices;
    }
    void process(const AudioBlock& b) noexcept override {
        const float* l[1] = {b.inputs[0]};
        const float* r[1] = {b.inputsRight ? b.inputsRight[0] : b.inputs[0]};
        mix(l, r, 1, b.frames, b.outputs[0], b.outputsRight[0], b.parameters);
    }
    void mix(const float* const* left, const float* const* right, int count, int frames,
             float* outLeft, float* outRight, const float* parameters) noexcept override {
        const float spread = parameters ? std::clamp(parameters[1], 0.f, 1.f) : .7f;
        const float level = parameters ? std::clamp(parameters[2], 0.f, 2.f) : .5f;
        static constexpr float positions[maximumVoices] = {-.85f, .85f, -.3f, .3f, -.6f, .6f, -.1f, .1f, -.95f, .95f, -.45f, .45f, -.75f, .75f, -.2f, .2f};
        std::fill_n(outLeft, frames, 0.f);
        std::fill_n(outRight, frames, 0.f);
        for (int v = 0; v < count; ++v) {
            const float angle = float((spread * positions[v] + 1.f) * pi / 4.0);
            const float gl = std::cos(angle) * level * 1.41421356f, gr = std::sin(angle) * level * 1.41421356f;
            for (int i = 0; i < frames; ++i) {
                const float x = .5f * (left[v][i] + right[v][i]);
                outLeft[i] += x * gl;
                outRight[i] += x * gr;
            }
        }
    }
private:
    struct Holder final : Base {
        explicit Holder(const NodeSpec& n) : Base(n) {}
        void process(const AudioBlock&) noexcept override {}
        std::shared_ptr<Controls> controls() const { return AudioModule::controls; }
    } base;
};

// --- Stereo effects wrapping the inherited first-party library -----------------
class DualGate final : public Base {
public:
    using Base::Base;
    void process(const AudioBlock& b) noexcept override {
        update(b);
        for (int ch = 0; ch < 2; ++ch) {
            const int port = ch * 4, param = ch * 3;
            const float key = b.note.trigger ? values[param + 2] * b.note.velocity : 0.f;
            for (int i = 0; i < b.frames; ++i) {
                const float damp = std::clamp(values[param + 1] + b.inputs[port + 3][i], 0.f, 1.f);
                if (damp != previousDamp[ch]) {
                    recovery[ch] = OpticalGateChannel::recovery(damp);
                    previousDamp[ch] = damp;
                }
                b.outputs[ch + 1][i] = channel[ch].process(b.inputs[port][i],
                    values[param] + b.inputs[port + 1][i], b.inputs[port + 2][i],
                    i == 0 ? key : 0.f, recovery[ch]);
            }
        }
        for (int i = 0; i < b.frames; ++i)
            b.outputs[0][i] = b.outputs[1][i] + b.outputs[2][i] + b.inputs[8][i];
    }
private:
    OpticalGateChannel channel[2];
    float previousDamp[2]{-1,-1}, recovery[2]{};
};

constexpr std::array<double, 10> echoBeats{.125, .25, 1./3., .5, .75, 1., 1.5, 2., 4., 8.};
class DualDelay final : public Base {
public:
    using Base::Base;
    void process(const AudioBlock& b) noexcept override {
        update(b);
        double configuredPeriod = -1;
        for (int i = 0; i < b.frames; ++i) {
            const bool high = b.inputs[3][i] > .1f;
            if (high && !pingHigh) {
                const auto interval = sample - lastPing;
                if (seenPing && interval >= 9600 && interval <= 144000)
                    externalPeriod = double(interval);
                lastPing = sample;
                seenPing = true;
            }
            pingHigh = high;
            ++sample;
            const double tempo = b.clock ? b.clock[i].tempo : 120.;
            const double period = externalPeriod > 0 ? externalPeriod : 48000. * 60. / std::clamp(tempo, 1., 1000.);
            for (int ch = 0; ch < 2; ++ch) {
                const int p = ch * 6;
                if (period != configuredPeriod) {
                    const int samples = int(std::lround(period * echoBeats[size_t(values[p])]));
                    channel[ch].configure(samples, values[p + 4] > .5f, values[p + 5] > .5f);
                }
                const float dry = (ch ? b.inputsRight[0] : b.inputs[0])[i] + b.inputs[ch + 1][i];
                const float wet = channel[ch].process(dry, values[p + 1], values[p + 2]);
                (ch ? b.outputsRight[0] : b.outputs[0])[i] = dry + values[p + 3] * (wet - dry);
                b.outputs[ch + 1][i] = wet;
            }
            configuredPeriod = period;
        }
    }
private:
    uint64_t sample = 0, lastPing = 0;
    double externalPeriod = 0;
    bool seenPing = false, pingHigh = false;
    ClockDelayChannel channel[2];
};

class Fx final : public Base {
public:
    Fx(const NodeSpec& n, std::string type) : Base(n), kind(std::move(type)), effect(partials::makeEffect(kind)) {
        if (!effect) throw std::invalid_argument("Unknown effect: " + kind);
        partials::EffectParams params{{"wet", values[0]}, {"modulation", 1.f}, {"diffusors", 4.f}};
        const int offset = partials::effectControlOffset(kind);
        for (int i = offset, k = 1; i < partials::kEffectControls && kind == partials::effectControlSpecs[size_t(i)].type; ++i, ++k)
            params[partials::effectControlSpecs[size_t(i)].key] = values[size_t(k)];
        effect->prepare(rate, params);   // allocation happens here, off the audio thread
    }
    void process(const AudioBlock& b) noexcept override {
        update(b);
        effect->setWet(values[0]);
        effect->updateControls(values.data() + 1);
        std::copy_n(b.inputs[0], b.frames, b.outputs[0]);
        std::copy_n(b.inputsRight ? b.inputsRight[0] : b.inputs[0], b.frames, b.outputsRight[0]);
        effect->process(b.outputs[0], b.outputsRight[0], b.frames);
    }
private:
    std::string kind;
    std::unique_ptr<partials::Effect> effect;
};

Panel effectPanel(const char* id, const char* name, const char* type, const char* description, float mix) {
    Panel p{id, name, description, "effect", {{"mix", "Mix", "", 0, 1, mix}}};
    const int offset = partials::effectControlOffset(type);
    for (int i = offset; i < partials::kEffectControls && std::string_view(type) == partials::effectControlSpecs[size_t(i)].type; ++i) {
        const auto& s = partials::effectControlSpecs[size_t(i)];
        if (s.manualOnly) continue;
        const bool log = s.scale == partials::ControlScale::Logarithmic && s.minimum > 0;
        p.parameters.push_back({s.key, s.label, s.unit, s.minimum, s.maximum, s.initial, log});
    }
    return p;
}
} // namespace

std::vector<Panel> analogPanels() {
    std::vector<Panel> p;
    p.push_back({"analog.vco", "Analog VCO", "CEM3340-style saw / pulse / triangle, FM, PWM, sync, per-voice drift", "source", {
        {"transpose", "Pitch", "st", -36, 36, 0}, {"fine", "Fine", "ct", -50, 50, 0},
        {"saw", "Saw", "", 0, 1, 1}, {"pulse", "Pulse", "", 0, 1, 0}, {"triangle", "Triangle", "", 0, 1, 0},
        {"pulse_width", "Pulse width", "", .05f, .95f, .5f}, {"fm_depth", "FM depth", "oct", 0, 4, 0},
        {"pwm_depth", "PWM depth", "", 0, .45f, 0}, {"drift", "Drift", "", 0, 1, .3f}, {"level", "Level", "", 0, 1, .5f}}});
    p.push_back({"analog.filter", "OB filter", "Oberheim-style 2-pole SVF (LP-notch-HP / band) or 4-pole OTA cascade", "filter", {
        {"cutoff_hz", "Cutoff", "Hz", 20, 20000, 1200, true}, {"resonance", "Resonance", "", 0, 1, .2f},
        {"mode", "Mode", "", 0, 1, 0}, {"band", "Band-pass", "", 0, 1, 0, false, 1, {"Off", "On"}}, {"poles", "4-pole", "", 0, 1, 0, false, 1, {"Off", "On"}},
        {"env_octaves", "Env amount", "oct", -6, 6, 2}, {"key_track", "Key track", "", 0, 1, .5f},
        {"drive", "Drive", "", 0, 1, .2f}, {"fm_depth", "FM depth", "oct", 0, 4, 0},
        {"pole_mix", "Xpander mode", "", 0, 15, 0, false, 1, {"Classic", "LP 4", "LP 3", "LP 2", "LP 1", "HP 3", "HP 2", "HP 1", "BP 4", "BP 2",
            "Notch", "Phase", "HP3 + LP1", "HP2 + LP1", "Notch + LP1", "Phase + LP1"}},
        {"quality", "Quality", "", 0, 2, 1, false, 1, {"Draft 1x", "Normal 2x", "High 4x"}}}});
    p.push_back({"analog.env", "Analog envelope", "CEM3310-style ADSR: RC curves, exponential times, long swells", "envelope", {
        {"attack", "Attack", "s", .001f, 30, .01f, true}, {"decay", "Decay", "s", .001f, 30, .5f, true},
        {"sustain", "Sustain", "", 0, 1, .7f}, {"release", "Release", "s", .001f, 60, 1.5f, true},
        {"velocity", "Velocity", "", 0, 1, .3f}}});
    p.push_back({"analog.vca", "OTA VCA", "Audio times control voltage with OTA input saturation", "utility", {
        {"level", "Level", "", 0, 2, 1}, {"drive", "Drive", "", 0, 1, .15f}}});
    p.push_back({"voice.mix", "Voice mixer", "Polyphony boundary: patch above runs per voice; pan-spread stereo sum", "voice", {
        {"voices", "Voices", "", 1, float(maximumVoices), 8, false, 1},
        {"spread", "Spread", "", 0, 1, .7f}, {"level", "Level", "", 0, 2, .5f},
        {"glide", "Glide", "s", 0, 5, 0}, {"unison", "Unison", "", 1, 8, 1, false, 1}, {"detune", "Detune", "ct", 0, 50, 10},
        {"steal", "Steal", "", 0, 1, 1, false, 1, {"Off", "On"}},
        {"tuning", "Tuning", "", 0, 4, 0, false, 1, {"Equal", "Just 5-limit", "Just 7-limit", "Pythagorean", "Harmonic series"}},
        {"root", "Root", "", 0, 11, 0, false, 1, {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}},
        // Request 72: when a sequencer drives the voices, the keyboard can still play them.
        {"keyboard", "Keyboard", "", 0, 1, 1, false, 1, {"Seq only", "Plays too"}}}});
    p.push_back({"analog.polymod", "Poly-mod", "Prophet-style per-voice mix of oscillator B and filter envelope for FM / PWM / filter", "utility", {
        {"osc_amount", "Osc B", "", -1, 1, 1}, {"env_amount", "Filter env", "", -1, 1, 0}}});
    p.push_back({"mod.wheel", "Wheel / bend", "Hardware MIDI mod wheel and pitch bend as control sources", "modulator", {
        {"amount", "Amount", "", 0, 1, 1}}});
    Panel gate{"analog.dual_lpg","Dual low-pass gate","Two optical gates with separate strikes, damping and an auxiliary sum","utility",{}};
    Panel delay{"fx.dual_loop","Dual looping delay","Two clocked delay lines with independent feed, hold, reverse and wet outputs","effect",{}};
    for (int ch = 0; ch < 2; ++ch) {
        const std::string prefix = ch ? "b_" : "a_", label = ch ? "B " : "A ";
        gate.parameters.push_back({prefix+"control",label+"Control","",0,1,0});
        gate.parameters.push_back({prefix+"damp",label+"Damp","",0,1,.4f});
        gate.parameters.push_back({prefix+"keys",label+"Key strike","",0,1,ch?0.f:1.f});
        delay.parameters.push_back({prefix+"time",label+"Time","",0,9,ch?4.f:3.f,false,1,
            {"1/32","1/16","1/8 triplet","1/8","Dotted 1/8","1/4","Dotted 1/4","1/2","1 bar (4/4)","2 bars (4/4)"}});
        delay.parameters.push_back({prefix+"feed",label+"Feed","",0,1,1});
        delay.parameters.push_back({prefix+"feedback",label+"Feedback","",0,1,.42f});
        delay.parameters.push_back({prefix+"mix",label+"Mix","",0,1,.38f});
        for (const std::string action : {"hold","reverse"})
            delay.parameters.push_back({prefix+action,label+(action=="hold"?"Hold":"Reverse"),"",0,1,0,false,1,{"Off","On"}});
    }
    p.push_back(std::move(gate));
    p.push_back(std::move(delay));
    p.push_back(effectPanel("fx.reverb", "Reverb", "reverb", "Long modulated stereo reverb (inherited library)", .3f));
    p.push_back(effectPanel("fx.chorus", "Chorus", "chorus", "Stereo chorus (inherited library)", .4f));
    p.push_back(effectPanel("fx.delay", "Stereo delay", "delay_stereo", "Stereo delay with filtered feedback (inherited library)", .25f));
    p.push_back(effectPanel("fx.phaser", "Phaser", "phaser", "Stereo phaser (inherited library)", .4f));
    return p;
}

void analogPorts(const std::string& id, ModuleType& type) {
    if (id == "analog.vco")
        type.inputs = {{"fm", SignalType::Audio, false}, {"pwm", SignalType::Control, false}, {"sync", SignalType::Audio, false}};
    else if (id == "analog.filter")
        type.inputs = {{"in", SignalType::Audio, true}, {"mod", SignalType::Control, false}, {"fm", SignalType::Audio, false}};
    else if (id == "analog.polymod") {
        type.inputs = {{"osc", SignalType::Audio, false}, {"env", SignalType::Control, false}};
        type.outputs = {{"fm", SignalType::Audio}, {"pw", SignalType::Control}};
    } else if (id == "analog.vca")
        type.inputs = {{"in", SignalType::Audio, true}, {"cv", SignalType::Control, true}};
    else if (id == "voice.mix") {
        type.inputs = {{"in", SignalType::Audio, true}, {"notes", SignalType::Event, false}};
        type.outputs = {{"out", SignalType::Audio, false, 2}};
    } else if (id == "mod.wheel")
        type.outputs = {{"out", SignalType::Control}, {"bend", SignalType::Control}};
    else if (id == "analog.dual_lpg") {
        for (const std::string ch : {"a_", "b_"}) {
            type.inputs.push_back({ch+"in", SignalType::Audio});
            for (const std::string cv : {"cv","strike","damp"}) type.inputs.push_back({ch+cv, SignalType::Control});
        }
        type.inputs.push_back({"aux", SignalType::Audio});
        type.inputs.push_back({"notes", SignalType::Event});
        type.outputs = {{"sum",SignalType::Audio},{"a",SignalType::Audio},{"b",SignalType::Audio}};
    } else if (id == "fx.dual_loop") {
        type.inputs = {{"in",SignalType::Audio,false,2},{"a",SignalType::Audio},{"b",SignalType::Audio},{"ping",SignalType::Control}};
        type.outputs = {{"out",SignalType::Audio,false,2},{"wet_a",SignalType::Audio},{"wet_b",SignalType::Audio}};
    }
    else if (id.rfind("fx.", 0) == 0) {
        type.inputs = {{"in", SignalType::Audio, true, 2}};
        type.outputs = {{"out", SignalType::Audio, false, 2}};
    }
}

GraphSpec padPatch(int model) {
    // 0 OB swell (2-pole), 1 Prophet 4-pole with poly-mod, 2 Xpander pole-mix unison,
    // 3 just-intonation drone (harmonic tuning, glide).
    using Values = std::vector<std::pair<std::string, float>>;
    const bool ob = model == 0 || model == 3, prophet = model == 1, xpander = model == 2;
    Values vco1 = prophet ? Values{{"saw", .8f}, {"pulse", .5f}, {"pulse_width", .4f}, {"drift", .3f}, {"level", .55f}, {"fm_depth", .06f}, {"pwm_depth", .1f}}
                          : Values{{"saw", 1}, {"drift", .45f}, {"level", .55f}};
    if (xpander) vco1 = {{"saw", .6f}, {"pulse", .6f}, {"pulse_width", .3f}, {"drift", .35f}, {"level", .55f}};
    Values vco2 = {{"saw", 1}, {"fine", prophet ? -5.f : 7.f}, {"drift", prophet ? .3f : .45f}, {"level", .55f}};
    if (model == 3) vco2 = {{"triangle", 1}, {"saw", .3f}, {"transpose", 12}, {"drift", .5f}, {"level", .4f}};
    Values filter = {{"cutoff_hz", 520}, {"resonance", .35f}, {"mode", .12f}, {"env_octaves", 2.6f}, {"key_track", .4f}, {"drive", .25f}};
    if (prophet) filter = {{"cutoff_hz", 700}, {"resonance", .42f}, {"poles", 1}, {"env_octaves", 3.f}, {"key_track", .5f}, {"drive", .3f}, {"fm_depth", .25f}};
    if (xpander) filter = {{"cutoff_hz", 900}, {"resonance", .55f}, {"pole_mix", 13}, {"env_octaves", 2.f}, {"key_track", .5f}, {"drive", .2f}};
    if (model == 3) filter = {{"cutoff_hz", 380}, {"resonance", .45f}, {"mode", .3f}, {"env_octaves", 3.f}, {"key_track", .6f}, {"drive", .3f}};
    Values filterEnv = prophet ? Values{{"attack", 1.1f}, {"decay", 3}, {"sustain", .4f}, {"release", 4}, {"velocity", .35f}}
                               : Values{{"attack", 2.8f}, {"decay", 5}, {"sustain", .55f}, {"release", 7}, {"velocity", .2f}};
    if (model == 3) filterEnv = {{"attack", 9}, {"decay", 12}, {"sustain", .5f}, {"release", 20}, {"velocity", .1f}};
    Values ampEnv = prophet ? Values{{"attack", .7f}, {"decay", 2}, {"sustain", .85f}, {"release", 5}, {"velocity", .4f}}
                            : Values{{"attack", 2.2f}, {"decay", 3}, {"sustain", .9f}, {"release", 8}, {"velocity", .3f}};
    if (model == 3) ampEnv = {{"attack", 6}, {"decay", 8}, {"sustain", .95f}, {"release", 24}, {"velocity", .1f}};
    Values voices = {{"spread", .85f}, {"level", .42f}};
    if (prophet) voices = {{"spread", .55f}, {"level", 1.25f}};                     // 4-pole path is ~10 dB quieter
    if (xpander) voices = {{"voices", 16}, {"unison", 2}, {"detune", 14}, {"glide", .12f}, {"spread", .9f}, {"level", 2.f}};   // HP2+LP1 removes ~13 dB
    if (model == 3) voices = {{"tuning", 4}, {"root", 2}, {"glide", .35f}, {"spread", .9f}, {"level", .2f}};
    GraphSpec g;
    g.nodes = {{"vco1", "analog.vco", vco1}, {"vco2", "analog.vco", vco2}, {"osc_mix", "utility.mix", {{"gain_a", .7f}, {"gain_b", .7f}}},
               {"filter", "analog.filter", filter}, {"filter_env", "analog.env", filterEnv}, {"amp_env", "analog.env", ampEnv},
               {"vca", "analog.vca", {}}, {"voices", "voice.mix", voices}, {"wheel", "mod.wheel", {}},
               {"breath", "mod.lfo", {{"rate", prophet ? .11f : model == 3 ? .03f : .06f}, {"depth", 1}}}};
    const std::string space = prophet ? "delay" : "chorus";
    if (prophet) g.nodes.push_back({"delay", "fx.delay", {{"mix", .18f}, {"time_l_ms", 410}, {"time_r_ms", 615}, {"feedback", .45f}, {"lowpass_hz", 3500}}});
    else g.nodes.push_back({"chorus", "fx.chorus", {{"mix", .3f}, {"rate_hz", .25f}}});
    g.nodes.push_back({"reverb", "fx.reverb", {{"mix", ob ? .38f : .3f}, {"time_s", model == 3 ? 18.f : ob ? 11.f : 7.f}, {"damping", .35f}, {"mod_depth", 14}, {"predelay_ms", 45}}});
    g.connections = {{{"vco1", "out"}, {"osc_mix", "a"}}, {{"vco2", "out"}, {"osc_mix", "b"}}, {{"osc_mix", "out"}, {"filter", "in"}},
                     {{"filter_env", "out"}, {"filter", "mod"}}, {{"filter", "out"}, {"vca", "in"}}, {{"amp_env", "out"}, {"vca", "cv"}},
                     {{"vca", "out"}, {"voices", "in"}}, {{"voices", "out"}, {space, "in"}}, {{space, "out"}, {"reverb", "in"}}};
    if (prophet) {
        // Poly-mod: osc B and the filter envelope into osc A pitch and PW and the filter.
        g.nodes.push_back({"polymod", "analog.polymod", {{"osc_amount", 1}, {"env_amount", .4f}}});
        for (Connection c : std::vector<Connection>{{{"vco2", "out"}, {"polymod", "osc"}}, {{"filter_env", "out"}, {"polymod", "env"}},
                                                     {{"polymod", "fm"}, {"vco1", "fm"}}, {{"polymod", "pw"}, {"vco1", "pwm"}}, {{"polymod", "fm"}, {"filter", "fm"}}})
            g.connections.push_back(c);
    }
    g.modulations = {{{"wheel", "out"}, {"filter", "cutoff_hz"}, .3f}, {{"breath", "out"}, {"filter", "cutoff_hz"}, model == 3 ? .1f : .05f}};
    g.outputs = {{"reverb", "out"}};
    return g;
}

std::unique_ptr<AudioModule> makeAnalog(const NodeSpec& n, const PrepareContext& c) {
    const auto& id = n.type;
    if (id == "analog.vco") return std::make_unique<Vco>(n, c);
    if (id == "analog.filter") return std::make_unique<ObFilter>(n);
    if (id == "analog.env") return std::make_unique<AnalogEnv>(n);
    if (id == "analog.vca") return std::make_unique<OtaVca>(n);
    if (id == "analog.polymod") return std::make_unique<PolyMod>(n);
    if (id == "voice.mix") return std::make_unique<VoiceMix>(n);
    if (id == "mod.wheel") return std::make_unique<Wheel>(n);
    if (id == "analog.dual_lpg") return std::make_unique<DualGate>(n);
    if (id == "fx.dual_loop") return std::make_unique<DualDelay>(n);
    if (id == "fx.reverb") return std::make_unique<Fx>(n, "reverb");
    if (id == "fx.chorus") return std::make_unique<Fx>(n, "chorus");
    if (id == "fx.delay") return std::make_unique<Fx>(n, "delay_stereo");
    if (id == "fx.phaser") return std::make_unique<Fx>(n, "phaser");
    throw std::invalid_argument("No analog factory for " + id);
}
} // namespace optisynth
