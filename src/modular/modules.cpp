#include "runtime.h"
#include "sequencer.h"
#include "analog.h"
#include "performance.h"
#include "reference_sequencers.h"
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
            {"sample.player", "Sample player", "WAV one-shot or held loop / stereo files mixed to mono", "source", {
                {"transpose","Pitch","st",-36,36,0}, {"root_note","Root note","note",0,127,48,false,1},
                {"start","Start","",0,1,0}, {"end","End","",0,1,1},
                {"level","Level","",0,1,.8f}, {"loop","Loop while held","",0,1,0,false,1}}},
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
        p.push_back(performancePanel());
        for(auto& model:referenceSequencerPanels())p.push_back(std::move(model));
        Panel spectral{"spectral.engine","Spectral engine","Complete instrument / stereo / latest-voice modulation outputs","engine",{}};
        for(const auto& s:partials::liveControlSpecs())spectral.parameters.push_back({s.id,s.name,s.unit,s.minimum,s.maximum,s.initial,s.logarithmic});
        spectral.parameters.push_back({"tempo","Tempo","BPM",20,300,120,false,.1});p.push_back(std::move(spectral));
        p.push_back({"utility.downmix","Stereo to mono","Explicit average: (left + right) / 2","utility",{}});
        p.push_back({"utility.stereo","Stereo pair","Left and right mono inputs to stereo","utility",{}});
        p.push_back({"utility.stereo_mix","Stereo mixer","Mix two stereo sources without collapsing their image","utility",{
            {"gain_a","Input A","",0,2,1},{"gain_b","Input B","",0,2,1}}});
        p.push_back({"mod.lfo","LFO","Independent sine / triangle / ramp / pulse modulation","modulator",{
            {"rate","Rate","Hz",.01f,30,1,true},{"depth","Depth","",0,1,1},
            {"wave","Shape","",0,3,0,false,1},{"retrigger","Note reset","",0,1,0,false,1}}});
        for(auto& sequencer:sequencerPanels())p.push_back(std::move(sequencer));
        for(auto& analog:analogPanels())p.push_back(std::move(analog));
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
    void wake() noexcept override { snap = true; }
    void update(const AudioBlock& b) noexcept {
        const float amount = -std::expm1(-b.frames/(48000.f*.005f));
        const float rate_ = snap ? 1.f : amount; snap = false;
        for (std::size_t i=0;i<definition.parameters.size();++i) {
            const auto& p=definition.parameters[i];
            float target=b.parameters?b.parameters[i]:controls->values[i].load(std::memory_order_relaxed);
            if (!std::isfinite(target)) target=p.initial;
            target=std::clamp(target,p.minimum,p.maximum);
            if(p.step>=1)values[i]=std::round(target);else values[i] += rate_*(target-values[i]);
        }
    }
    const Panel& definition;
    std::array<float,maxParameters> values{};
    bool snap=false;
};
float noteFrequency(const AudioBlock& b,float transpose,bool useBend=false) {
    const float bend=useBend&&b.inputs&&b.inputs[0]?b.inputs[0][0]*48.f:0.f;
    return std::clamp(440.f*std::exp2((b.note.midi+transpose+bend-69.f)/12.f)/48000.f,8.f/48000,.24f);
}
class SamplePlayer final : public Module {
public:
    SamplePlayer(const NodeSpec& n,SamplePtr data):Module(n),sample(std::move(data)){
        if(sample&&(!std::isfinite(sample->rate)||sample->rate<8000||sample->rate>384000||sample->mono.empty()||sample->mono.size()>maximumSampleFrames))throw std::invalid_argument("Invalid prepared sample");
    }
    void process(const AudioBlock& b) noexcept override {
        update(b);std::fill_n(b.outputs[0],b.frames,0.f);if(!sample||sample->mono.empty())return;
        const auto count=sample->mono.size();
        if(b.note.trigger){
            auto trim=[&](int i){float v=b.parameters?b.parameters[size_t(i)]:controls->values[size_t(i)].load();return std::isfinite(v)?std::clamp(v,0.f,1.f):definition.parameters[size_t(i)].initial;};
            first=size_t(trim(2)*double(count-1));last=std::max(first+1,size_t(std::ceil(trim(3)*double(count))));last=std::min(last,count);
            position=double(first);age=0;release=1;playing=true;
        }
        const bool loop=values[5]>=.5f;const double rate=sample->rate/48000.*std::exp2((b.note.midi-values[1]+values[0])/12.);
        for(int i=0;i<b.frames&&playing;++i){
            if(position>=double(last)){if(loop&&b.note.gate)position=first+std::fmod(position-first,double(last-first));else{playing=false;break;}}
            const auto index=size_t(position),next=std::min(index+1,last-1);const float fraction=float(position-index);
            float value=sample->mono[index]+fraction*(sample->mono[next]-sample->mono[index]);
            if(loop&&!b.note.gate){release=std::max(0.f,release-1.f/240);if(release==0){playing=false;break;}}
            const float edge=std::min(1.f,float((double(last)-position)/std::max(1.,rate*16)));
            const float attack=std::min(1.f,float(++age)/8.f);
            b.outputs[0][i]=value*values[4]*std::clamp(b.note.velocity,0.f,1.f)*edge*attack*release;
            position+=rate;
        }
    }
private:
    SamplePtr sample;double position=0;size_t first=0,last=0;uint64_t age=0;float release=1;bool playing=false;
};
class VariableOsc final : public Module {
public:
    explicit VariableOsc(const NodeSpec& n):Module(n){osc.Init();}
    void process(const AudioBlock& b) noexcept override {
        update(b);const float f=noteFrequency(b,values[0],true);
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
        update(b);osc.Render(noteFrequency(b,values[0],true),b.outputs[0],b.frames);
        for(int i=0;i<b.frames;++i)b.outputs[0][i]*=values[1];
    }
private:plaits::SineOscillator osc;
};
class Resonator final : public Module {
public:
    explicit Resonator(const NodeSpec& n):Module(n){resonator.Init(.015f,plaits::kMaxNumModes);}
    void process(const AudioBlock& b) noexcept override {
        update(b);
        for(int i=0;i<b.frames;++i)input[i]=b.inputs[0][i]*values[4];
        resonator.Process(noteFrequency(b,values[0]),std::clamp(values[1]+b.inputs[1][0],0.f,.999999f),
            std::clamp(values[2]+b.inputs[2][0],0.f,1.f),std::clamp(values[3]+b.inputs[3][0],0.f,1.f),input.data(),b.outputs[0],b.frames);
    }
private:plaits::Resonator resonator;std::array<float,quantum> input{};
};
class MutableFilter final : public Module {
public:
    MutableFilter(const NodeSpec& n,int mode):Module(n),mode_(mode){filter.Init();}
    void process(const AudioBlock& b) noexcept override {
        update(b);
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
        update(b);
        float f=440.f*std::exp2((b.note.midi+values[0]-69.f)/12.f)/48000.f;
        // Interpolate reads index+1 even at a zero fractional offset. The source
        // stiffness array has 65 entries, so never pass an exact index of 64.
        voice.Render(false,b.note.trigger,std::clamp(b.note.velocity,0.f,1.f),
            std::clamp(f,8.f/48000,.2f),std::clamp(values[1]+b.inputs[0][0],0.f,.999999f),
            std::clamp(values[2]+b.inputs[1][0],0.f,1.f),std::clamp(values[3]+b.inputs[2][0],0.f,1.f),
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
        update(b);
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
        update(b);
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
        update(b);
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
        update(b);
        for (int i=0;i<b.frames;++i) b.outputs[0][i]=kind_==0?b.inputs[0][i]:
            kind_==1?b.inputs[0][i]*b.inputs[1][i]:b.inputs[0][i]*values[0]+b.inputs[1][i]*values[1];
    }
private: int kind_;
};
class StereoUtility final:public Module {
public:
    StereoUtility(const NodeSpec& n,int kind):Module(n),kind_(kind){}
    void process(const AudioBlock& b) noexcept override {
        update(b);
        for(int i=0;i<b.frames;++i){
            if(kind_==0)b.outputs[0][i]=.5f*(b.inputs[0][i]+b.inputsRight[0][i]);
            else if(kind_==1){b.outputs[0][i]=b.inputs[0][i];b.outputsRight[0][i]=b.inputs[1][i];}
            else{b.outputs[0][i]=values[0]*b.inputs[0][i]+values[1]*b.inputs[1][i];b.outputsRight[0][i]=values[0]*b.inputsRight[0][i]+values[1]*b.inputsRight[1][i];}
        }
    }
private:int kind_;
};
class Lfo final:public Module {
public:
    explicit Lfo(const NodeSpec& n):Module(n){}
    void process(const AudioBlock& b) noexcept override {
        update(b);if(b.note.trigger&&values[3]>=.5f)phase=0;
        for(int i=0;i<b.frames;++i){
            const int shape=int(values[2]);
            const float v=shape==0?float(std::sin(6.283185307179586*phase)):shape==1?float(1-4*std::abs(phase-.5)):shape==2?float(2*phase-1):phase<.5?1.f:-1.f;
            b.outputs[0][i]=v*values[1];phase+=values[0]/48000.;phase-=std::floor(phase);
        }
    }
private:double phase=0;
};
}
Registry moduleRegistry(const SampleBank& samples,const SpectralBank& spectral) {
    Registry r;
    for (const auto& p : panels()) {
        ModuleType type;type.id=p.type;
        const auto id=p.type;
        type.outputs={{"out", p.family=="envelope"||p.family=="modulator"?SignalType::Control:SignalType::Audio}};
        for(const auto& parameter:p.parameters)type.parameters.push_back(parameter.id);
        if(id=="spectral.engine"){
            type.outputs={{"out",SignalType::Audio,false,2}};
            for(const auto* name:{"lfo1","lfo2","lfo3","lfo4","lfo5","mainenv","amp1","amp2","amp3","amp4","cut1","cut2","cut3","cut4","mod1","mod2","mod3"})type.outputs.push_back({name,SignalType::Control});
        }
        if(id=="utility.downmix")type.inputs={{"in",SignalType::Audio,false,2}};
        if(id=="utility.stereo"){type.inputs={{"left",SignalType::Audio,false},{"right",SignalType::Audio,false}};type.outputs={{"out",SignalType::Audio,false,2}};}
        if(id=="utility.stereo_mix"){type.inputs={{"a",SignalType::Audio,false,2},{"b",SignalType::Audio,false,2}};type.outputs={{"out",SignalType::Audio,false,2}};}
        if (id=="modal.voice") type.outputs.push_back({"exciter",SignalType::Audio});
        if (id=="modal.resonator") type.inputs={{"in",SignalType::Audio,false}};
        if(id=="modal.voice" || id=="modal.resonator")
            for(const auto* cv:{"mat","bright","decay"})type.inputs.push_back({cv,SignalType::Control});
        if (p.family=="filter") type.inputs={{"in",SignalType::Audio,false},{"mod",SignalType::Control,false}};
        if (id=="utility.vca") type.inputs={{"in",SignalType::Audio,false},{"gain",SignalType::Control,false}};
        if (id=="utility.mix") type.inputs={{"a",SignalType::Audio,false},{"b",SignalType::Audio,false}};
        if(isAnalog(id))analogPorts(id,type);
        if(id=="osc.variable"||id=="osc.sine")type.inputs.push_back({"bend",SignalType::Control,false});
        if(p.family=="source"||p.family=="envelope"||p.family=="engine"||p.family=="resonator"||id=="mod.lfo")type.inputs.push_back({"notes",SignalType::Event,false});
        if(isSequencer(id))type.outputs={{"notes",SignalType::Event}};
        if(id=="seq.pattern"){type.outputs.push_back({"cv1",SignalType::Control});type.outputs.push_back({"cv2",SignalType::Control});}
        if(id=="seq.performance"){type.outputs.clear();for(int t=1;t<=4;++t){const auto prefix="t"+std::to_string(t)+"_";type.outputs.push_back({prefix+"notes",SignalType::Event});for(const char* name:{"gate","bend","cv"})type.outputs.push_back({prefix+name,SignalType::Control});}}
        if(isReferenceSequencer(id))referenceSequencerPorts(id,type);
        type.create=[id,assets=id=="sample.player"?samples:SampleBank{},engines=id=="spectral.engine"?spectral:SpectralBank{}](const NodeSpec& n,const PrepareContext& c)->std::unique_ptr<PreparedModule> {
            if(c.sampleRate!=48000 || c.maximumBlock>quantum) throw std::invalid_argument("Module requires 48 kHz / <=24 frame blocks");
            if(isAnalog(id))return makeAnalog(n,c);
            if(isSequencer(id))return makeSequencer(n);
            if(isReferenceSequencer(id))return makeReferenceSequencer(n);
            if(id=="seq.performance")return makePerformance(n);
            if(id=="sample.player"){auto it=assets.find(n.id);return std::make_unique<SamplePlayer>(n,it==assets.end()?SamplePtr{}:it->second);}
            if(id=="spectral.engine"){auto it=engines.find(n.id);return makeSpectral(n,it==engines.end()?SpectralPtr{}:it->second);}
            if(id=="mod.lfo")return std::make_unique<Lfo>(n);
            if(id=="utility.downmix")return std::make_unique<StereoUtility>(n,0);
            if(id=="utility.stereo")return std::make_unique<StereoUtility>(n,1);
            if(id=="utility.stereo_mix")return std::make_unique<StereoUtility>(n,2);
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
