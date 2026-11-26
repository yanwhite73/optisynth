// Mala: the open Beads texture-synthesizer DSP in the rack (programme item 1,
// requests 136-140; docs/research/modules/MALA.md).
//
// The granular processor, delay controller, grain players, reverb, limiters,
// wow and flutter and the no-input wavetable synth are the upstream Beads code,
// compiled unchanged from third_party/mutable/beads (MIT, revision pinned in
// PROVENANCE.json). This file takes the place of beads.cc and the hardware:
// pot laws and CV scaling (cv_reader.h, settings.cc defaults), socket
// normalisation, the input gain control (gain_control.cc, ported from integer
// codec frames to floats), quality switching with its 250 ms silent changeover,
// the 2.5 ms seed delay, and the codec rate itself. Each quality runs the DSP at
// its hardware codec rate (48, 32 or 24 kHz); the rack's 48 kHz is converted with
// the upstream stmlib converters and Beads' own FIR tables. Every block is one
// hardware block (codec rate / 4000), so control timing matches the module.
#include "granular_tape.h"

#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/dsp/sample_rate_conversion_filters.h"
#include "stmlib/dsp/hysteresis_filter.h"
#include "stmlib/dsp/sample_rate_converter.h"
#include "stmlib/utils/gate_flags.h"

#include <array>
#include <cmath>
#include <vector>

namespace optisynth {
namespace {
using beads::FloatFrame;
using stmlib::GateFlags;

enum Param { Density, Time, Size, Shape, Pitch, TimeAmt, SizeAmt, ShapeAmt, PitchAmt, Feedback, DryWet, Reverb,
             Quality, Assign, AssignFb, AssignDw, AssignRv, Freeze, SeedLatch, Seed, InputGain, RGate, ParamCount };
enum Input { InStereo, InL, InR, FreezeIn, SeedIn, TimeCv, SizeCv, ShapeCv, PitchCv, DensityCv, AssignCv, InputCount };

constexpr int kBlockRate = 4000;      // beads.cc
constexpr int kHostRate = 48000;
constexpr int kHostBlock = kHostRate / kBlockRate;
constexpr float kGateThreshold = .25f;  // about 1.25 V with 1.0 = 5 V

// Panel order is the manual's quality table; the firmware's media enum runs the other way.
constexpr beads::Processor::Media kMedia[4] = {
    beads::Processor::MEDIA_DIGITAL, beads::Processor::MEDIA_VINTAGE_DIGITAL,
    beads::Processor::MEDIA_CLEAN_TAPE, beads::Processor::MEDIA_DIRTY_TAPE};

float detune(float raw) {   // cv_reader.h, PotChannel::LAW_DETUNE
    float value = 2.0f * raw - 1.0f;
    const float sign = value < 0.0f ? -1.0f : 1.0f;
    value = value * sign * 6.0f;
    MAKE_INTEGRAL_FRACTIONAL(value);
    const float intervals[8] = {0.0f, 1.0f, 3.0f, 4.0f, 7.0f, 12.0f, 24.0f, 24.0f};
    value_fractional = std::max((value_fractional - 0.33333333f) * 1.5f, 0.0f);
    const float a = intervals[value_integral];
    const float b = intervals[value_integral + 1];
    return stmlib::Crossfade(a, b, stmlib::SmoothStep(value_fractional)) * sign;
}
float attenuverter(float raw) {   // cv_reader.h, PotChannel::LAW_ATTENUVERTER
    const float v = raw - 0.5f;
    return (2.0f / 0.47f) * v * std::max(std::fabs(v) - 0.03f, 0.0f);
}

// cv_reader.h PotChannel: law, then a one-pole and hysteresis at the block rate.
struct Pot {
    enum Law { Normal, Attenuverter, Detune } law = Normal;
    float lp = 0;
    stmlib::HysteresisFilter hysteresis;
    void init(Law l, float threshold) { law = l; lp = 0; hysteresis.Init(threshold); }
    void process(float raw) {
        const float v = law == Attenuverter ? attenuverter(raw) : law == Detune ? detune(raw) : raw;
        ONE_POLE(lp, v, 0.05f);
        hysteresis.Process(lp);
    }
    float value() const { return hysteresis.value(); }
};

// cv_reader.h CvChannel with the settings.cc calibration defaults. Rack control
// signals use 1.0 = 5 V, so Time/Size/Shape span +-1.6, Pitch +-60 st (12 st/V),
// Density and Assign +-1.
struct Cv {
    float lp = 0, coefficient = 1, scale = 1;
    void init(float c, float s) { coefficient = c; scale = s; lp = 0; }
    float process(float volts5) { ONE_POLE(lp, volts5, coefficient); return std::clamp(lp * scale, -scale, scale); }
};

// gain_control.cc, ported from 32-bit codec frames to floats; logic and constants unchanged.
class GainStage {
public:
    void init(float sampleRate, beads::Processor::Media media, bool reset) {
        mode = media == beads::Processor::MEDIA_DIRTY_TAPE ? On
             : media == beads::Processor::MEDIA_CLEAN_TAPE ? Dynamic : Off;
        if (reset) { stableAgcPeak = agcPeak = peak = 0.5f; gain = 1.0f; softClipping = 0.0f; }
        ignoreTime = size_t(sampleRate * 1.0f);
        const bool noReadjust = !reset && time && time >= activeTime;
        activeTime = size_t(sampleRate * (1.0f + 5.0f));
        time = noReadjust ? activeTime : 0;
        oneHertz = 1.0f / sampleRate;
    }
    void process(bool active, uint8_t setting, bool leftNormalised, bool rightNormalised,
                 const FloatFrame* in, FloatFrame* out, size_t size) {
        if (leftNormalised && rightNormalised) {
            for (size_t i = 0; i < size; ++i) out[i].l = out[i].r = 0.0f;
            peak = 0.0f; agcPeak = stableAgcPeak = 0.5f; gain = 1.0f; time = 0;
            return;
        }
        const float hz = float(size) * oneHertz;
        if (time < ignoreTime) agcPeak = 0.5f;
        else if (time < activeTime) { SLOPE(agcPeak, peak, 10.0f * hz, 2.0f * hz); stableAgcPeak = agcPeak; }
        else SLOPE(agcPeak, peak, 5.0f * hz, (peak > stableAgcPeak ? 0.5f : 0.01f) * hz);
        if (active) {
            if (time > ignoreTime || peak > 0.001f) time += size;
            if (time >= activeTime) time = activeTime;
        }
        autoGain = setting == 0;
        float manual = float(setting) / 255.0f;
        manual *= manual; manual *= manual;
        manual = 1.0f + (40.0f - 1.0f) * manual;
        ONE_POLE(manualGain, manual, 0.01f);
        const float optimal = std::min(0.9f / std::min(agcPeak, 0.9f), 40.0f);
        const float target = autoGain ? optimal : manualGain;
        float clipping = mode == Off ? 0.0f : mode == On ? 1.0f : 2.0f * (manualGain / optimal - 0.75f);
        CONSTRAIN(clipping, 0.0f, 1.0f);
        stmlib::ParameterInterpolator gainModulation(&gain, target, size);
        stmlib::ParameterInterpolator clippingModulation(&softClipping, clipping, size);
        for (size_t i = 0; i < size; ++i) {
            float l = leftNormalised ? in[i].r : in[i].l;
            float r = rightNormalised ? in[i].l : in[i].r;
            if (active) { const float p = std::max(std::fabs(l), std::fabs(r)); SLOPE(peak, p, 0.1f, 2.0f * oneHertz); }
            const float g = gainModulation.Next();
            l *= g; r *= g;
            if (!autoGain) {
                if (mode == Off) { CONSTRAIN(l, -1.0f, 1.0f); CONSTRAIN(r, -1.0f, 1.0f); }
                else {
                    const float amount = clippingModulation.Next(), bias = 0.05f;
                    const float softL = (1.0f - bias) * stmlib::SoftClip(l + bias) - bias;
                    const float softR = (1.0f - bias) * stmlib::SoftClip(r + bias) - bias;
                    CONSTRAIN(l, -1.0f, 1.0f); CONSTRAIN(r, -1.0f, 1.0f);
                    l = stmlib::Crossfade(l, softL, amount); r = stmlib::Crossfade(r, softR, amount);
                }
            }
            out[i].l = l; out[i].r = r;
        }
    }
    float level() const { const float v = peak * gain; return v > 1 ? 1.f : v; }   // gain_control.h vu_meter
private:
    enum Mode { Off, On, Dynamic } mode = Off;
    float peak = .5f, agcPeak = .5f, stableAgcPeak = .5f, gain = 1, softClipping = 0, manualGain = 1, oneHertz = 1.f / kHostRate;
    bool autoGain = true;
    size_t time = 0, ignoreTime = 0, activeTime = 0;
};

// The rack's 48 kHz to a codec rate and back, with upstream converters and FIR tables.
struct RateBridge {
    stmlib::SampleRateConverter<stmlib::SRC_UP, 2, 48> up2[2], up2Out[2];
    stmlib::SampleRateConverter<stmlib::SRC_DOWN, 3, 48> down3[2];
    stmlib::SampleRateConverter<stmlib::SRC_UP, 3, 48> up3[2];
    stmlib::SampleRateConverter<stmlib::SRC_DOWN, 2, 48> down2[2], down2Out[2];
    void reset() {
        for (int c = 0; c < 2; ++c) { up2[c].Init(); up2Out[c].Init(); down3[c].Init(); up3[c].Init(); down2[c].Init(); down2Out[c].Init(); }
    }
    // codecRate: 48000, 32000 or 24000. in/out hold kHostBlock frames; codec frames = rate / 4000.
    void down(int codecRate, const FloatFrame* in, FloatFrame* out) {
        if (codecRate == kHostRate) { std::copy(in, in + kHostBlock, out); return; }
        for (int c = 0; c < 2; ++c) {
            float host[kHostBlock], wide[2 * kHostBlock], codec[kHostBlock];
            for (int i = 0; i < kHostBlock; ++i) host[i] = c ? in[i].r : in[i].l;
            if (codecRate == 32000) { up2[c].Process(host, wide, kHostBlock); down3[c].Process(wide, codec, 2 * kHostBlock); }
            else down2[c].Process(host, codec, kHostBlock);
            for (int i = 0; i < codecRate / kBlockRate; ++i) (c ? out[i].r : out[i].l) = codec[i];
        }
    }
    void up(int codecRate, const FloatFrame* in, FloatFrame* out) {
        if (codecRate == kHostRate) { std::copy(in, in + kHostBlock, out); return; }
        const int frames = codecRate / kBlockRate;
        for (int c = 0; c < 2; ++c) {
            float codec[kHostBlock], wide[2 * kHostBlock], host[kHostBlock];
            for (int i = 0; i < frames; ++i) codec[i] = c ? in[i].r : in[i].l;
            if (codecRate == 32000) { up3[c].Process(codec, wide, size_t(frames)); down2Out[c].Process(wide, host, 2 * kHostBlock); }
            else up2Out[c].Process(codec, host, size_t(frames));
            for (int i = 0; i < kHostBlock; ++i) (c ? out[i].r : out[i].l) = host[i];
        }
    }
};

class GranularTape final : public AudioModule {
public:
    explicit GranularTape(const NodeSpec& node) : definition(panel(node.type)) {
        for (size_t i = 0; i < definition.parameters.size(); ++i) controls->values[i].store(definition.parameters[i].initial);
        for (const auto& saved : node.parameters) {
            const auto& id = saved.first;
            const float value = saved.second;
            const auto it = std::find_if(definition.parameters.begin(), definition.parameters.end(), [&](const Parameter& p) { return p.id == id; });
            if (it == definition.parameters.end() || !std::isfinite(value) || value < it->minimum || value > it->maximum)
                throw std::invalid_argument("Invalid parameter: " + node.id + "." + id);
            controls->values[size_t(it - definition.parameters.begin())].store(value);
        }
        delay.assign(768 * 1024, 0);
        reverbMemory.assign(32768, 0);
        uint8_t* buffers[3] = {delay.data(), delay.data() + 256 * 1024, delay.data() + 512 * 1024};
        processor = std::make_unique<beads::Processor>();
        synth = std::make_unique<beads::Synth>();
        processor->Init(buffers, reverbMemory.data());
        synth->Init(reverbMemory.data());
        for (int i = 0; i < ParamCount; ++i) {
            const bool amount = i >= TimeAmt && i <= PitchAmt;
            pots[size_t(i)].init(amount ? Pot::Attenuverter : i == Pitch ? Pot::Detune : Pot::Normal, i == Time ? 0.001f : 0.0f);
        }
        cv[TimeCv].init(.5f, 1.6f); cv[SizeCv].init(.5f, 1.6f); cv[ShapeCv].init(.5f, 1.6f);
        cv[PitchCv].init(1, 60); cv[DensityCv].init(1, 1); cv[AssignCv].init(.1f, 1);
        mediaIndex = qualityIndex(controls->values[Quality].load());
        configure(true);
    }

    void patched(const std::vector<int>& sources) override {
        for (int i = 0; i < InputCount && size_t(i) < sources.size(); ++i) wired[size_t(i)] = sources[size_t(i)];
        configure(true);
    }

    void process(const AudioBlock& b) noexcept override {
        for (int i = 0; i < b.frames; ++i) {
            outL(b)[i] = held[size_t(position)].l;
            outR(b)[i] = held[size_t(position)].r;
            FloatFrame frame;
            // In L / In R are the panel's mono sockets; In takes a stereo cable from the rack.
            frame.l = wired[InL] ? b.inputs[InL][i] : wired[InStereo] ? b.inputs[InStereo][i] : 0.f;
            frame.r = wired[InR] ? b.inputs[InR][i] : wired[InStereo] ? b.inputsRight[InStereo][i] : 0.f;
            pending[size_t(position)] = frame;
            freezeGate[size_t(position)] = b.inputs[FreezeIn][i] > kGateThreshold;
            seedGate[size_t(position)] = b.inputs[SeedIn][i] > kGateThreshold;
            if (++position == kHostBlock) {
                for (int k = TimeCv; k < InputCount; ++k) lastCv[size_t(k)] = b.inputs[k][i];
                block(b.parameters);
                position = 0;
            }
        }
    }

private:
    static float* outL(const AudioBlock& b) { return b.outputs[0]; }
    static float* outR(const AudioBlock& b) { return b.outputsRight[0]; }
    static int qualityIndex(float v) { return std::clamp(int(std::lround(v)), 0, 3); }
    float parameter(const float* effective, int i) const {
        const auto& p = definition.parameters[size_t(i)];
        const float v = effective ? effective[i] : controls->values[size_t(i)].load(std::memory_order_relaxed);
        return std::isfinite(v) ? std::clamp(v, p.minimum, p.maximum) : p.initial;
    }

    bool leftPatched() const { return wired[InL] > 0 || wired[InStereo] > 0; }
    bool rightPatched() const { return wired[InR] > 0 || wired[InStereo] > 0; }

    // beads.cc MonitorStateChange and ConfigureAndStartCodec. Off the audio thread from the
    // constructor and patched(); on it when the quality changes (no allocation).
    void configure(bool resetGain) {
        synthMode = !leftPatched() && !rightPatched();
        mono = !leftPatched() || !rightPatched();
        processor->Configure(kMedia[mediaIndex], mono);
        codecRate = synthMode ? kHostRate : int(processor->sample_rate());
        gainControl.init(float(codecRate), kMedia[mediaIndex], resetGain);
        bridge.reset();
        std::fill(seedHistory.begin(), seedHistory.end(), false);
        transitionBlocks = kBlockRate / 4;
    }

    void block(const float* effective) noexcept {
        const int quality = qualityIndex(parameter(effective, Quality));
        const bool freezeButton = parameter(effective, Freeze) >= .5f;
        // The firmware will not change quality while frozen (ui.h should_ignore_state_change).
        if (quality != mediaIndex && !freezeButton && !synthMode) { mediaIndex = quality; configure(false); }
        else if (quality != mediaIndex && synthMode) mediaIndex = quality;

        const int frames = codecRate / kBlockRate;
        FloatFrame in[kHostBlock], gained[kHostBlock], out[kHostBlock];
        bridge.down(codecRate, pending.data(), in);
        bool freeze[kHostBlock];
        GateFlags seed[kHostBlock];
        bool freezeBlock = false, seedBlock = false;
        for (int j = 0; j < frames; ++j) {
            // Gate samples spanning this codec sample (1, 1.5 or 2 rack samples).
            const int from = j * kHostBlock / frames, to = std::max(from + 1, (j + 1) * kHostBlock / frames);
            bool f = false, s = false;
            for (int k = from; k < to; ++k) { f |= freezeGate[size_t(k)]; s |= seedGate[size_t(k)]; }
            freeze[j] = freezeButton || f;
            freezeBlock |= freeze[j];
            if (wired[SeedIn]) {
                // beads.cc delays the seed input by 2.5 ms (ten blocks) so CV can settle.
                seedHistory[size_t(seedWrite)] = s;
                const bool delayed = seedHistory[size_t((seedWrite - 10 * frames + 256) % 256)];
                seedWrite = (seedWrite + 1) % 256;
                previousSeed = seed[j] = stmlib::ExtractGateFlags(previousSeed, delayed);
                seedBlock |= delayed;
            }
        }
        if (!wired[SeedIn]) previousSeed = stmlib::GATE_FLAG_LOW;

        for (int i = 0; i < ParamCount; ++i) {
            float raw = parameter(effective, i);
            if (i == Pitch || (i >= TimeAmt && i <= PitchAmt)) raw = (raw + 1.f) * .5f;   // stored centred
            pots[size_t(i)].process(raw);
        }
        float cvValue[InputCount]{};
        for (int k = TimeCv; k < InputCount; ++k) cvValue[k] = cv[size_t(k)].process(lastCv[size_t(k)]);

        const float input = parameter(effective, InputGain);
        const uint8_t gainSetting = input <= 0 ? 0 : uint8_t(std::min(255.f, 1.f + input * 255.f));
        gainControl.process(!transitionBlocks, gainSetting, !leftPatched(), !rightPatched(), in, gained, size_t(frames));
        controls->meters[kTapeInputLevel].store(synthMode ? 0.f : gainControl.level(), std::memory_order_relaxed);
        controls->meters[kTapeSeedActivity].store(seedBlock ? 1.f : 0.f, std::memory_order_relaxed);
        controls->meters[kTapeReserve].store(synthMode ? 1.f : processor->overwrite_reserve(), std::memory_order_relaxed);
        controls->meters[kTapeSynthMode].store(synthMode ? 1.f : 0.f, std::memory_order_relaxed);

        if (transitionBlocks) {
            --transitionBlocks;
            for (int i = 0; i < frames; ++i) out[i].l = out[i].r = 0.f;
        } else {
            beads::Parameters p;
            auto set = [&](beads::RandomizedParameter& target, int base, int amount, int input) {
                target.base = pots[size_t(base)].value();
                target.attenuverter = pots[size_t(amount)].value();
                target.has_cv = wired[size_t(input)] > 0;
                target.cv = cvValue[input];
            };
            set(p.time, Time, TimeAmt, TimeCv);
            set(p.size, Size, SizeAmt, SizeCv);
            set(p.shape, Shape, ShapeAmt, ShapeCv);
            set(p.pitch, Pitch, PitchAmt, PitchCv);
            p.monophonic_mode = false;   // ENABLE_ADVANCED_FEATURES is off in the release firmware
            beads::ModulatedParameter d;
            d.base = pots[Density].value();
            d.cv = wired[DensityCv] ? cvValue[DensityCv] : 0.0f;
            const int destination = qualityIndex(parameter(effective, Assign));
            const float custom[3] = {parameter(effective, AssignFb), parameter(effective, AssignDw), parameter(effective, AssignRv)};
            float mix[3];
            for (int i = 0; i < 3; ++i) {
                const float base = pots[size_t(Feedback + i)].value() * 1.005f;
                const float amount = destination == 3 ? custom[i] : (destination == i ? 1.0f : 0.0f);
                mix[i] = std::clamp(base + (cvValue[AssignCv] - 0.002f) * amount, 0.0f, 1.0f);
            }
            beads::Processor::MixParameters m;
            m.feedback = mix[0];
            m.dry_wet = mix[1];
            m.reverb_amount = mix[2];
            m.output_gate = parameter(effective, RGate) >= .5f;
            const bool latched = parameter(effective, SeedLatch) >= .5f;
            const auto seedState = latched ? beads::SEED_BUTTON_STATE_LOCKED
                : parameter(effective, Seed) >= .5f ? beads::SEED_BUTTON_STATE_ON : beads::SEED_BUTTON_STATE_OFF;
            const GateFlags* seedInput = wired[SeedIn] ? seed : nullptr;
            if (synthMode)
                synth->Render(seedState, seedInput, freeze, beads::Synth::Resolution(kMedia[mediaIndex]), d, p, m, out, size_t(frames));
            else
                processor->Process(seedState, seedInput, freeze, d, p, m, gained, out, size_t(frames));
            for (int i = 0; i < frames; ++i) {   // beads.cc: conversion to the codec's integer range
                CONSTRAIN(out[i].l, -1.0f, 0.999999f);
                CONSTRAIN(out[i].r, -1.0f, 0.999999f);
            }
        }
        bridge.up(codecRate, out, held.data());
        (void)freezeBlock;
    }

    const Panel& definition;
    std::vector<uint8_t> delay;
    std::vector<uint16_t> reverbMemory;
    std::unique_ptr<beads::Processor> processor;
    std::unique_ptr<beads::Synth> synth;
    std::array<Pot, ParamCount> pots{};
    std::array<Cv, InputCount> cv{};
    std::array<float, InputCount> lastCv{};
    std::array<int, InputCount> wired{};
    std::array<FloatFrame, kHostBlock> pending{}, held{};
    std::array<bool, kHostBlock> freezeGate{}, seedGate{};
    std::array<bool, 256> seedHistory{};
    int seedWrite = 0, position = 0, codecRate = kHostRate, mediaIndex = 0, transitionBlocks = 0;
    GateFlags previousSeed = stmlib::GATE_FLAG_LOW;
    bool synthMode = true, mono = false;
    GainStage gainControl;
    RateBridge bridge;
};
}  // namespace

float granularTapePitch(float knob) { return detune((std::clamp(knob, -1.f, 1.f) + 1.f) * .5f); }
float granularTapeAmount(float knob) { return attenuverter((std::clamp(knob, -1.f, 1.f) + 1.f) * .5f); }

Panel granularTapePanel() {
    return {"fx.granular_tape", "Mala", "Granular tape: grains, delay taps and reverb from the open Beads design", "effect", {
        {"density", "Density", "", 0, 1, .68f},
        {"time", "Time", "", 0, 1, .25f},
        {"size", "Size", "", 0, 1, .55f},
        {"shape", "Shape", "", 0, 1, .4f},
        {"pitch", "Pitch", "", -1, 1, 0},
        {"time_amt", "Time amount", "", -1, 1, 0},
        {"size_amt", "Size amount", "", -1, 1, 0},
        {"shape_amt", "Shape amount", "", -1, 1, 0},
        {"pitch_amt", "Pitch amount", "", -1, 1, 0},
        {"feedback", "Feedback", "", 0, 1, 0},
        {"dry_wet", "Dry/wet", "", 0, 1, .5f},
        {"reverb", "Reverb", "", 0, 1, .25f},
        {"quality", "Quality", "", 0, 3, 0, false, 1, {"Bright digital", "Cold digital", "Sunny tape", "Scorched cassette"}},
        {"assign", "Assign", "", 0, 3, 0, false, 1, {"Feedback", "Dry/wet", "Reverb", "Custom"}},
        {"assign_fb", "Assign feedback", "", 0, 1, 1},
        {"assign_dw", "Assign dry/wet", "", 0, 1, 0},
        {"assign_rv", "Assign reverb", "", 0, 1, .5f},
        {"freeze", "Freeze", "", 0, 1, 0, false, 1},
        {"seed_latch", "Latched seeds", "", 0, 1, 1, false, 1},
        {"seed", "Seed", "", 0, 1, 0, false, 1},
        {"input_gain", "Input gain", "", 0, 1, 0},
        {"r_gate", "Grain gate on R", "", 0, 1, 0, false, 1}}};
}

void granularTapePorts(ModuleType& type) {
    type.inputs = {{"in", SignalType::Audio, false, 2}, {"in_l", SignalType::Audio}, {"in_r", SignalType::Audio}, {"freeze", SignalType::Control},
                   {"seed", SignalType::Control}, {"time", SignalType::Control}, {"size", SignalType::Control},
                   {"shape", SignalType::Control}, {"pitch", SignalType::Control}, {"density", SignalType::Control},
                   {"assign", SignalType::Control}};
    type.outputs = {{"out", SignalType::Audio, false, 2}};
}

std::unique_ptr<AudioModule> makeGranularTape(const NodeSpec& node) { return std::make_unique<GranularTape>(node); }
}  // namespace optisynth
