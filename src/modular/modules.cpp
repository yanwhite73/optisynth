#include "runtime.h"
#include "../core/envelope.h"
#include "../core/effects.h"
#include "plaits/dsp/drums/analog_bass_drum.h"
#include "plaits/dsp/physical_modelling/modal_voice.h"
#include "plaits/dsp/oscillator/variable_shape_oscillator.h"
#include <cmath>
#include <stdexcept>

namespace optisynth {
const std::vector<Panel>& panels() {
    static const std::vector<Panel> list = [] {
        std::vector<Panel> p{
            {"osc.variable", "VA oscillator", "Original Plaits DSP: triangle / saw / pulse with hard sync", "source", {
                {"transpose","Pitch","st",-36,36,0}, {"shape","Shape","",0,1,.5f},
                {"pulse_width","Pulse width","",.01f,.99f,.5f},
                {"sync","Sync ratio",":1",1,16,1,true}, {"level","Level","",0,1,.5f}}},
            {"osc.sine", "Sine oscillator", "Original Plaits DSP: continuous sine wave", "source", {
                {"transpose","Pitch","st",-36,36,0}, {"level","Level","",0,1,.5f}}},
            {"modal.resonator", "Modal resonator", "Original Plaits DSP: excite 24 modes with another module", "resonator", {
                {"transpose","Pitch","st",-36,36,0}, {"structure","Material","",0,1,.35f},
                {"brightness","Brightness","",0,1,.5f}, {"decay","Decay","",0,1,.45f},
                {"input_gain","Excitation","",0,1,.25f}}},
            {"modal.voice", "Modal percussion", "Strike a bank of 24 resonant modes", "source", {
                {"transpose","Pitch","st",-36,36,0},
                {"structure","Material","",0,1,.35f},
                {"brightness","Brightness","",0,1,.5f},
                {"decay","Decay","",0,1,.45f}}},
            {"drum.analog", "Bass drum", "Pulse, bridged-T resonance and feedback", "source", {
                {"transpose","Pitch","st",-36,36,-12},
                {"tone","Tone","",0,1,.35f},
                {"decay","Decay","",0,1,.35f},
                {"attack_fm","Attack FM","",0,1,.45f},
                {"self_fm","Self FM","",0,1,.15f}}},
            {"spectral.ahdsfr","Spectral AHDSFR","Shape attack, hold, decay, fade and release","envelope",{
                {"attack","Attack","s",0,8,.005f,true}, {"hold","Hold","s",0,8,0,true},
                {"decay","Decay","s",0,12,.6f,true}, {"sustain","Sustain","",0,1,.5f},
                {"fade","Fade","dB/s",-24,24,0}, {"release","Release","s",0,12,1.5f,true},
                {"attack_curve","A curve","",.2f,1.8f,1},
                {"decay_curve","D curve","",.2f,1.8f,1},
                {"release_curve","R curve","",.2f,1.8f,1}}},
            {"spectral.adsr","Spectral ADSR","Four-stage configuration of the Spectral envelope","envelope",{
                {"attack","Attack","s",0,8,.005f,true}, {"decay","Decay","s",0,12,.6f,true},
                {"sustain","Sustain","",0,1,.5f}, {"release","Release","s",0,12,1.5f,true}}},
            {"filter.bypass","No filter","Pass the source through unchanged","filter",{}},
            {"utility.vca","VCA","Audio multiplied by a shared envelope","utility",{}},
            {"utility.mix","Mixer","Sum two independently adjustable audio sources","utility",{
                {"gain_a","Input A","",0,2,1}, {"gain_b","Input B","",0,2,1}}}
        };
        const char* ids[] = {"spectral.lowpass","spectral.highpass","spectral.bandpass","spectral.notch"};
        const char* names[] = {"Spectral low-pass","Spectral high-pass","Spectral band-pass","Spectral notch"};
        for (int i=0;i<4;++i) p.push_back({ids[i],names[i],"Spectral filter effect with optional envelope modulation","filter",{
            {"cutoff_hz","Cutoff","Hz",20,20480,10000,true},
            {"resonance","Resonance","",0,1,.2f},
            {"saturation","Saturation","",0,1,0},
            {"env_octaves","Env amount","oct",-6,6,2},
            {"mod_rate_hz","LFO rate","Hz",.01f,20,.5f,true},
            {"mod_depth","LFO depth","",0,1,0}}});
        const char* miIds[]={"mutable.lowpass","mutable.bandpass","mutable.highpass"};
        const char* miNames[]={"Digital SVF low-pass","Digital SVF band-pass","Digital SVF high-pass"};
        for(int i=0;i<3;++i)p.push_back({miIds[i],miNames[i],"Original Mutable stmlib DSP: resonant digital filter","filter",{
            {"cutoff_hz","Cutoff","Hz",20,20000,2500,true},
            {"resonance","Resonance","Q",.5f,20,.707f,true},
            {"env_octaves","Mod amount","oct",-6,6,2}}});
        return p;
    }();
    return list;
}
const Panel& panel(const std::string& type) {
    for (const auto& p : panels()) if (p.type == type) return p;
    throw std::invalid_argument("Unknown module: " + type);
}
namespace {
class Module : public AudioModule {
public:
    explicit Module(const NodeSpec& node) : definition(panel(node.type)) {
        for (std::size_t i=0;i<definition.parameters.size();++i) {
            values[i] = definition.parameters[i].initial;
            controls->values[i].store(values[i]);
        }
        for (const auto& pair : node.parameters) {
            auto it = std::find_if(definition.parameters.begin(),definition.parameters.end(),
                                   [&](const Parameter& p){return p.id==pair.first;});
            if (it==definition.parameters.end() || !std::isfinite(pair.second) ||
                pair.second<it->minimum || pair.second>it->maximum)
                throw std::invalid_argument("Invalid parameter: " + node.id + "." + pair.first);
            const auto i=std::distance(definition.parameters.begin(),it);
            controls->values[i].store(pair.second);values[i]=pair.second;
        }
    }
    void update(int frames) noexcept {
        const float amount = -std::expm1(-frames/(48000.f*.005f));
        for (std::size_t i=0;i<definition.parameters.size();++i) {
            const auto& p=definition.parameters[i];
            float target=controls->values[i].load(std::memory_order_relaxed);
            if (!std::isfinite(target)) target=p.initial;
            target=std::clamp(target,p.minimum,p.maximum);
            values[i] += amount*(target-values[i]);
        }
    }
    const Panel& definition;
    std::array<float,maxParameters> values{};
};
float noteFrequency(const AudioBlock& b,float transpose) {
    return std::clamp(440.f*std::exp2((b.note.midi+transpose-69.f)/12.f)/48000.f,8.f/48000,.24f);
}
class VariableOsc final : public Module {
public:
    explicit VariableOsc(const NodeSpec& n):Module(n){osc.Init();}
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);const float f=noteFrequency(b,values[0]);
        if(values[3]<=1.00001f)osc.Render(f,values[2],values[1],b.outputs[0],b.frames);
        else osc.Render(f,std::min(.249f,f*values[3]),values[2],values[1],b.outputs[0],b.frames);
        for(int i=0;i<b.frames;++i)b.outputs[0][i]*=values[4];
    }
private:plaits::VariableShapeOscillator osc;
};
class Sine final : public Module {
public:
    explicit Sine(const NodeSpec& n):Module(n){osc.Init();}
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);osc.Render(noteFrequency(b,values[0]),b.outputs[0],b.frames);
        for(int i=0;i<b.frames;++i)b.outputs[0][i]*=values[1];
    }
private:plaits::SineOscillator osc;
};
class Resonator final : public Module {
public:
    explicit Resonator(const NodeSpec& n):Module(n){resonator.Init(.015f,plaits::kMaxNumModes);}
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);
        for(int i=0;i<b.frames;++i)input[i]=b.inputs[0][i]*values[4];
        resonator.Process(noteFrequency(b,values[0]),std::min(values[1],.999999f),values[2],values[3],input.data(),b.outputs[0],b.frames);
    }
private:plaits::Resonator resonator;std::array<float,quantum> input{};
};
class MutableFilter final : public Module {
public:
    MutableFilter(const NodeSpec& n,int mode):Module(n),mode_(mode){filter.Init();}
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);
        const float f=std::clamp(values[0]*std::exp2(values[2]*b.inputs[1][0])/48000.f,20.f/48000,.45f);
        filter.set_f_q<stmlib::FREQUENCY_ACCURATE>(f,values[1]);
        if(mode_==0)filter.Process<stmlib::FILTER_MODE_LOW_PASS>(b.inputs[0],b.outputs[0],b.frames);
        else if(mode_==1)filter.Process<stmlib::FILTER_MODE_BAND_PASS_NORMALIZED>(b.inputs[0],b.outputs[0],b.frames);
        else filter.Process<stmlib::FILTER_MODE_HIGH_PASS>(b.inputs[0],b.outputs[0],b.frames);
    }
private:stmlib::Svf filter;int mode_;
};
class Modal final : public Module {
public:
    explicit Modal(const NodeSpec& n) : Module(n) { voice.Init(); }
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);
        float f=440.f*std::exp2((b.note.midi+values[0]-69.f)/12.f)/48000.f;
        // Interpolate reads index+1 even at a zero fractional offset. The source
        // stiffness array has 65 entries, so never pass an exact index of 64.
        voice.Render(false,b.note.trigger,std::clamp(b.note.velocity,0.f,1.f),
            std::clamp(f,8.f/48000,.2f),std::min(values[1],.999999f),values[2],values[3],
            temp.data(),b.outputs[0],b.outputs[1],b.frames);
    }
private:
    plaits::ModalVoice voice;
    std::array<float,quantum> temp{};
};
class Drum final : public Module {
public:
    explicit Drum(const NodeSpec& n) : Module(n) { drum.Init(); }
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);
        float f=440.f*std::exp2((b.note.midi+values[0]-69.f)/12.f)/48000.f;
        drum.Render(false,b.note.trigger,std::clamp(b.note.velocity,0.f,1.f),
            std::clamp(f,8.f/48000,.2f),values[1],values[2],values[3],values[4],b.outputs[0],b.frames);
    }
private:
    plaits::AnalogBassDrum drum;
};
class Env final : public Module {
public:
    explicit Env(const NodeSpec& n) : Module(n), full(n.type=="spectral.ahdsfr") {}
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);
        partials::EnvelopeParams p;
        if (full) {
            p.attack=values[0];p.hold=values[1];p.decay=values[2];p.sustain=values[3];
            p.fade=values[4];p.release=values[5];p.attackCurve=values[6];
            p.decayCurve=values[7];p.releaseCurve=values[8];
        } else { p.attack=values[0];p.decay=values[1];p.sustain=values[2];p.release=values[3]; }
        if (b.note.trigger) env.start(p); else env.updateParams(p);
        if (!b.note.gate && (gate || b.note.trigger)) env.noteOff();
        gate=b.note.gate;
        for (int i=0;i<b.frames;++i) b.outputs[0][i]=env.advance(1.f/48000);
    }
private:
    partials::Envelope env;
    bool full,gate=false;
};
class Filter final : public Module {
public:
    Filter(const NodeSpec& n, int mode) : Module(n), fx(partials::makeEffect("filter")) {
        fx->prepare(48000,{{"type",float(mode)}});
    }
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);
        for (int i=0;i<b.frames;++i) b.outputs[0][i]=right[i]=b.inputs[0][i];
        const float cutoff=std::clamp(values[0]*std::exp2(values[3]*b.inputs[1][0]),20.f,20480.f);
        const float c[]={cutoff,values[1],values[2],values[4],values[5],0};
        fx->updateControls(c);
        fx->process(b.outputs[0],right.data(),b.frames);
    }
private:
    std::unique_ptr<partials::Effect> fx;
    std::array<float,quantum> right{};
};
class Utility final : public Module {
public:
    Utility(const NodeSpec& n,int kind):Module(n),kind_(kind){}
    void process(const AudioBlock& b) noexcept override {
        update(b.frames);
        for (int i=0;i<b.frames;++i) b.outputs[0][i]=kind_==0?b.inputs[0][i]:
            kind_==1?b.inputs[0][i]*b.inputs[1][i]:b.inputs[0][i]*values[0]+b.inputs[1][i]*values[1];
    }
private: int kind_;
};
}
Registry moduleRegistry() {
    Registry r;
    for (const auto& p : panels()) {
        ModuleType type;type.id=p.type;
        const auto id=p.type;
        type.outputs={{"out", p.family=="envelope"?SignalType::Control:SignalType::Audio}};
        if (id=="modal.voice") type.outputs.push_back({"exciter",SignalType::Audio});
        if (id=="modal.resonator") type.inputs={{"in",SignalType::Audio,true}};
        if (p.family=="filter") type.inputs={{"in",SignalType::Audio,true},{"mod",SignalType::Control,false}};
        if (id=="utility.vca") type.inputs={{"in",SignalType::Audio,true},{"gain",SignalType::Control,true}};
        if (id=="utility.mix") type.inputs={{"a",SignalType::Audio,true},{"b",SignalType::Audio,true}};
        type.create=[id](const NodeSpec& n,const PrepareContext& c)->std::unique_ptr<PreparedModule> {
            if(c.sampleRate!=48000 || c.maximumBlock>quantum) throw std::invalid_argument("Module requires 48 kHz / <=24 frame blocks");
            if(id=="modal.voice")return std::make_unique<Modal>(n);
            if(id=="modal.resonator")return std::make_unique<Resonator>(n);
            if(id=="osc.variable")return std::make_unique<VariableOsc>(n);
            if(id=="osc.sine")return std::make_unique<Sine>(n);
            const char* miIds[]={"mutable.lowpass","mutable.bandpass","mutable.highpass"};
            for(int i=0;i<3;++i)if(id==miIds[i])return std::make_unique<MutableFilter>(n,i);
            if(id=="drum.analog")return std::make_unique<Drum>(n);
            if(id=="spectral.ahdsfr" || id=="spectral.adsr")return std::make_unique<Env>(n);
            if(id=="filter.bypass")return std::make_unique<Utility>(n,0);
            if(id=="utility.vca")return std::make_unique<Utility>(n,1);
            if(id=="utility.mix")return std::make_unique<Utility>(n,2);
            const char* ids[]={"spectral.lowpass","spectral.highpass","spectral.bandpass","spectral.notch"};
            for(int i=0;i<4;++i)if(id==ids[i])return std::make_unique<Filter>(n,i);
            throw std::invalid_argument("No factory for module");
        };
        r.push_back(std::move(type));
    }
    return r;
}
} // namespace optisynth
