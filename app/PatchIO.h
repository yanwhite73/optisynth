#pragma once
#include <JuceHeader.h>
#include "../src/modular/rack.h"
#include "../src/modular/sequencer.h"
#include "../src/modular/pattern.h"
#include "PerformanceStateIO.h"
#include <set>

namespace optisynth {
struct SampleReference { std::string path,fingerprint;SamplePtr data; };
struct RackDocument { GraphSpec graph; float gain=.25f; std::map<std::string,SampleReference> samples; SpectralBank spectral; float tempo=120; };
inline SpectralPtr effectiveSpectral(const RackDocument& d,const std::string& id){
    auto state=d.spectral.find(id);if(state==d.spectral.end()||!state->second)return {};
    auto patch=state->second->patch;auto values=partials::liveValues(patch);const auto& specs=panel("spectral.engine").parameters;
    for(const auto& node:d.graph.nodes)if(node.id==id)for(const auto& v:node.parameters){
        const auto p=std::find_if(specs.begin(),specs.end(),[&](const Parameter& x){return x.id==v.first;});
        if(p!=specs.end()&&size_t(p-specs.begin())<values.size())values[size_t(p-specs.begin())]=v.second;
    }
    partials::applyLiveValues(patch,values);return spectralPreset(std::move(patch),state->second->tables);
}
inline RackDocument starterDocument(int which,float gain) {
    if(which==13||which==14){
        auto d=starterDocument(11,gain);d.tempo=which==13?104:118;
        auto data=std::make_shared<PerformanceData>(defaultPerformance());
        if(which==14){data->seed=8107;for(auto& track:data->tracks)for(auto& p:track.patterns){p.traversal=3;p.root=2;p.swing=.12f;}data->tracks[2].name="Chimes";}
        for(auto& p:data->tracks[3].patterns)for(auto& s:p.steps)s.pitch=48;
        d.graph.nodes={{"sequence","seq.performance",{},data},{"bass","osc.variable",{{"shape",.3f},{"level",.28f}}},
            {"bass_env","spectral.adsr",{{"attack",.003f},{"decay",.19f},{"sustain",.35f},{"release",.2f}}},
            {"bass_filter","mutable.lowpass",{{"cutoff_hz",900},{"resonance",1.8f},{"env_octaves",2}}},{"bass_vca","utility.vca",{}},
            {"spectral","spectral.engine",{}},{"glass","modal.voice",{{"structure",.45f},{"brightness",.45f},{"decay",.35f}}},
            {"drum","drum.analog",{{"decay",.2f},{"attack_fm",.22f}}},
            {"rhythm_mix","utility.mix",{{"gain_a",.5f},{"gain_b",.35f}}},{"voice_mix","utility.mix",{{"gain_a",.7f},{"gain_b",.5f}}},
            {"pair","utility.stereo",{}},{"output","utility.stereo_mix",{{"gain_a",.7f},{"gain_b",.6f}}}};
        d.graph.connections={{{"sequence","t1_notes"},{"bass","notes"}},{{"sequence","t1_notes"},{"bass_env","notes"}},{{"sequence","t1_bend"},{"bass","bend"}},
            {{"bass","out"},{"bass_filter","in"}},{{"bass_env","out"},{"bass_filter","mod"}},{{"bass_filter","out"},{"bass_vca","in"}},{{"bass_env","out"},{"bass_vca","gain"}},
            {{"sequence","t2_notes"},{"spectral","notes"}},{{"sequence","t3_notes"},{"glass","notes"}},{{"sequence","t4_notes"},{"drum","notes"}},
            {{"glass","out"},{"rhythm_mix","a"}},{{"drum","out"},{"rhythm_mix","b"}},{{"bass_vca","out"},{"voice_mix","a"}},{{"rhythm_mix","out"},{"voice_mix","b"}},
            {{"voice_mix","out"},{"pair","left"}},{{"voice_mix","out"},{"pair","right"}},{{"pair","out"},{"output","a"}},{{"spectral","out"},{"output","b"}}};
        d.graph.modulations={{{"sequence","t1_cv"},{"bass_filter","cutoff_hz"},.08f},{{"sequence","t2_cv"},{"spectral","osc1_filter_cutoff"},.09f},{{"sequence","t3_cv"},{"glass","brightness"},.15f},{{"sequence","t4_cv"},{"drum","tone"},.12f}};
        d.graph.outputs={{"output","out"}};return d;
    }
    if(which==11||which==12){
        RackDocument d;d.gain=gain;d.tempo=which==11?96:112;
        auto sound=spectralDemoPatch();sound.name=which==11?"Motion / Night current":"Motion / Glass tide";
        sound.mainVolumeDb=-12;sound.osc[0].unisonVoices=2;sound.osc[0].route1=1;sound.osc[0].filter.cutoffHz=which==11?950:2200;
        sound.ampEnv[0].attack=.003f;sound.ampEnv[0].decay=.24f;sound.ampEnv[0].sustain=.18f;sound.ampEnv[0].release=.28f;
        sound.mainEnv.release=.3f;
        sound.fx[0]={"delay_stereo",true,2,{{"wet",.32f},{"time_l_ms",float(45000/d.tempo)},{"time_r_ms",float(60000/d.tempo)},{"feedback",.42f},{"highpass_hz",180},{"lowpass_hz",5200},{"cross",1}}};
        sound.fx[1]={"reverb",true,0,{{"wet",.24f},{"time_s",4.8f},{"damping",.4f}}};
        if(which==12){sound.osc[0].waveMix=.85f;sound.osc[0].unisonDetune=4;}
        d.spectral["spectral"]=spectralPreset(sound);
        NodeSpec sequence{"sequence","seq.pattern",{{"length",32},{"pitch_length",which==11?7.f:11.f},{"velocity_length",5},{"cv1_length",11},{"cv2_length",7},{"scale",2},{"gate",.68f},{"swing",.08f},{"cv_slew",.035f},{"seed",713},{"keyboard_transpose",1}}};
        constexpr int notes[]={48,55,60,58,51,63,55,67,60,58,51};
        for(int i=0;i<64;++i){
            sequence.parameters.push_back({patternKey(i,Pitch),float(notes[i%11]+(which==12?12:0))});
            sequence.parameters.push_back({patternKey(i,Velocity),i%4==0?1.f:.65f});
            sequence.parameters.push_back({patternKey(i,On),i%8==6?0.f:1.f});
            sequence.parameters.push_back({patternKey(i,Chance),i%4==3?.72f:1.f});
            sequence.parameters.push_back({patternKey(i,Ratchet),i%16==15?3.f:1.f});
            sequence.parameters.push_back({patternKey(i,Condition),i%16==14?8.f:0.f});
            sequence.parameters.push_back({patternKey(i,CV1),float(std::sin(i*1.7)*.65)});
            sequence.parameters.push_back({patternKey(i,CV2),float(std::cos(i*.77)*.7)});
        }
        d.graph.nodes={std::move(sequence),{"spectral","spectral.engine",{}}};
        d.graph.connections={{{"sequence","notes"},{"spectral","notes"}}};d.graph.outputs={{"spectral","out"}};
        d.graph.modulations={{{"sequence","cv1"},{"spectral","osc1_filter_cutoff"},.10f},{{"sequence","cv2"},{"spectral","osc1_wave_mix"},.18f}};
        return d;
    }
    if(which>=8&&which<=10)return {sequencerPatch(which-8),gain};
    RackDocument d{rackPreset(which),gain,{}};
    if(which==4||which==5)d.samples["source"]={"builtin:click","builtin-click-v1",{}};
    if(which>=6){
        d.graph.nodes={{"spectral","spectral.engine",{}}};d.graph.connections.clear();d.graph.outputs={{"spectral","out"}};
        d.spectral["spectral"]=spectralPreset(spectralDemoPatch());
        if(which==6){d.graph.nodes.push_back({"lfo","mod.lfo",{{"rate",.3f}}});d.graph.modulations={{{"lfo","out"},{"spectral","osc1_filter_cutoff"},.16f}};}
        else {d.graph.nodes.push_back({"mono","utility.downmix",{}});d.graph.nodes.push_back({"resonator","modal.resonator",{{"input_gain",.3f}}});
            d.graph.connections={{{"spectral","out"},{"mono","in"}},{{"mono","out"},{"resonator","in"}}};d.graph.outputs={{"resonator","out"}};
            d.graph.modulations={{{"spectral","lfo1"},{"resonator","brightness"},.25f}};
        }
    }
    return d;
}
inline void pruneSampleReferences(RackDocument& d) {
    for(auto it=d.samples.begin();it!=d.samples.end();){
        if(std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==it->first&&n.type=="sample.player";}))it=d.samples.erase(it);
        else ++it;
    }
    for(auto it=d.spectral.begin();it!=d.spectral.end();){
        if(std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==it->first&&n.type=="spectral.engine";}))it=d.spectral.erase(it);else ++it;
    }
    for(auto& n:d.graph.nodes)if(n.type=="spectral.engine"){
        auto& preset=d.spectral[n.id];if(!preset)preset=spectralPreset(partials::defaultPatch());
        const auto values=partials::liveValues(preset->patch);const auto& specs=panel(n.type).parameters;
        for(size_t i=0;i<values.size();++i)if(std::none_of(n.parameters.begin(),n.parameters.end(),[&](const auto& p){return p.first==specs[i].id;}))
            n.parameters.push_back({specs[i].id,std::clamp(values[i],specs[i].minimum,specs[i].maximum)});
    }
}
inline SampleBank sampleBank(const RackDocument& d) {
    SampleBank b;for(const auto& s:d.samples)if(s.second.data)b[s.first]=s.second.data;return b;
}
inline bool needsSamples(const RackDocument& d) {
    auto plan=compileGraph(d.graph,moduleRegistry());
    for(auto i:plan.nodes){if(d.graph.nodes[i].type=="spectral.engine")return true;auto it=d.samples.find(d.graph.nodes[i].id);if(it!=d.samples.end()&&!it->second.data)return true;}return false;
}
inline void prepareSamples(RackDocument& d,const CancelSample& cancelled={}) {
    validateRack(d.graph);const auto plan=compileGraph(d.graph,moduleRegistry());
    std::set<std::string> active;for(auto i:plan.nodes)active.insert(d.graph.nodes[i].id);
    for(auto& s:d.spectral)if(!active.count(s.first)&&s.second)s.second=spectralPreset(s.second->patch);
    std::map<std::string,SamplePtr> loaded;std::set<const SampleData*> counted;size_t bytes=0;
    for(auto& s:d.samples){auto& ref=s.second;
        if(cancelled&&cancelled())throw std::runtime_error("Sample load cancelled");
        if(!active.count(s.first)){ref.data.reset();continue;}
        if(!ref.data){auto it=loaded.find(ref.path);ref.data=it!=loaded.end()?it->second:ref.path=="builtin:click"?demoSample():loadWave(ref.path,cancelled);loaded[ref.path]=ref.data;}
        if(!ref.fingerprint.empty()&&ref.fingerprint!=ref.data->fingerprint)throw std::runtime_error("Sample file changed: "+ref.path+". Load WAV again to use the new audio.");
        ref.fingerprint=ref.data->fingerprint;
        if(counted.insert(ref.data.get()).second)bytes+=ref.data->mono.size()*sizeof(float);
        if(bytes>maximumBankBytes)throw std::runtime_error("Active samples exceed 128 MiB");
    }
}
inline float number(const juce::var& v) {
    if((!v.isDouble()&&!v.isInt())||!std::isfinite(double(v)))throw std::invalid_argument("Expected a finite number");
    return float(double(v));
}
inline std::string identifier(const juce::var& v) {
    if(!v.isString()||v.toString().isEmpty()||v.toString().length()>128)throw std::invalid_argument("Invalid module/port identity");
    return v.toString().toStdString();
}
inline juce::var endpointJSON(const Endpoint& e) {
    auto* v=new juce::DynamicObject;v->setProperty("node",juce::String(e.node));v->setProperty("port",juce::String(e.port));return v;
}
inline Endpoint readEndpoint(const juce::var& v) {
    if(!v.isObject())throw std::invalid_argument("Invalid endpoint");
    return {identifier(v["node"]),identifier(v["port"])};
}
inline juce::var rackJSON(const RackDocument& document) {
    auto* root=new juce::DynamicObject;root->setProperty("format","optisynth.rack");root->setProperty("version",5);root->setProperty("gain",document.gain);root->setProperty("tempo",document.tempo);
    auto* assets=new juce::DynamicObject;
    for(const auto& s:document.samples){auto* ref=new juce::DynamicObject;ref->setProperty("path",juce::String(s.second.path));ref->setProperty("fingerprint",juce::String(s.second.fingerprint));assets->setProperty(juce::Identifier(s.first),juce::var(ref));}
    root->setProperty("samples",juce::var(assets));
    auto* engines=new juce::DynamicObject;
    for(const auto& s:document.spectral)if(s.second)engines->setProperty(juce::Identifier(s.first),juce::String(partials::patchText(s.second->patch)));
    root->setProperty("spectral",juce::var(engines));
    juce::Array<juce::var> modulation;
    for(const auto& m:document.graph.modulations){auto* row=new juce::DynamicObject;row->setProperty("from",endpointJSON(m.source));row->setProperty("to",endpointJSON(m.destination));row->setProperty("amount",m.amount);modulation.add(row);}
    root->setProperty("modulations",modulation);
    juce::Array<juce::var> nodes,links,outputs;
    for(const auto& n:document.graph.nodes) {
        auto* node=new juce::DynamicObject;node->setProperty("id",juce::String(n.id));node->setProperty("type",juce::String(n.type));
        auto* params=new juce::DynamicObject;for(const auto& p:n.parameters)params->setProperty(juce::Identifier(juce::String(p.first)),p.second);
        node->setProperty("parameters",juce::var(params));
        if(n.type=="seq.performance")node->setProperty("performance",performanceJSON(n.performance?*n.performance:defaultPerformance()));
        nodes.add(node);
    }
    for(const auto& c:document.graph.connections){auto* link=new juce::DynamicObject;link->setProperty("from",endpointJSON(c.source));link->setProperty("to",endpointJSON(c.destination));links.add(link);}
    for(const auto& e:document.graph.outputs)outputs.add(endpointJSON(e));
    root->setProperty("nodes",nodes);root->setProperty("connections",links);root->setProperty("outputs",outputs);return root;
}
inline RackDocument readRack(const juce::var& value) {
    if(!value.isObject()||!value["version"].isInt()||int(value["version"])<1||int(value["version"])>5)throw std::invalid_argument("Unsupported patch version");
    RackDocument d;d.gain=number(value["gain"]);
    if(int(value["version"])>=4){d.tempo=number(value["tempo"]);if(d.tempo<20||d.tempo>300)throw std::invalid_argument("Tempo outside range");}
    if(d.gain<0||d.gain>1)throw std::invalid_argument("Output gain outside range");
    if(value["format"].toString()=="optisynth.instrument") {
        if(int(value["version"])!=1)throw std::invalid_argument("Unsupported legacy patch version");
        auto source=identifier(value["source"]),env=identifier(value["envelope"]),filter=identifier(value["filter"]);
        if(panel(source).family!="source"||panel(env).family!="envelope"||panel(filter).family!="filter")throw std::invalid_argument("Invalid legacy module family");
        if(!value["amplitudeEnvelope"].isBool()||!value["filterEnvelope"].isBool())throw std::invalid_argument("Invalid legacy routing");
        d.graph=instrumentPatch(source,env,filter,bool(value["amplitudeEnvelope"]),bool(value["filterEnvelope"])&&filter!="filter.bypass");
        auto* parameters=value["parameters"].getDynamicObject();if(!parameters)throw std::invalid_argument("Missing legacy parameters");
        for(const auto& entry:parameters->getProperties()) {
            const auto type=entry.name.toString().toStdString();const auto& specs=panel(type).parameters;
            auto* vals=entry.value.getDynamicObject();if(!vals)throw std::invalid_argument("Invalid legacy parameters");
            for(const auto& p:vals->getProperties()) {
                const auto id=p.name.toString().toStdString();float f=number(p.value);
                auto spec=std::find_if(specs.begin(),specs.end(),[&](const Parameter& s){return s.id==id;});
                if(spec==specs.end()||f<spec->minimum||f>spec->maximum)throw std::invalid_argument("Invalid legacy parameter value");
                for(auto& node:d.graph.nodes)if(node.type==type)node.parameters.push_back({id,f});
            }
        }
    } else if(value["format"].toString()=="optisynth.rack") {
        auto* nodes=value["nodes"].getArray();auto* links=value["connections"].getArray();auto* outputs=value["outputs"].getArray();
        if(!nodes||!links||!outputs||nodes->size()>kMaximumNodes||links->size()>kMaximumConnections||outputs->size()>kMaximumPorts)throw std::invalid_argument("Invalid/oversized rack");
        for(const auto& v:*nodes){
            NodeSpec n{identifier(v["id"]),identifier(v["type"]),{}};
            auto* params=v["parameters"].getDynamicObject();if(!params)throw std::invalid_argument("Missing node parameters");
            for(const auto& p:params->getProperties())n.parameters.push_back({p.name.toString().toStdString(),number(p.value)});
            if(n.type=="seq.performance")n.performance=readPerformance(v["performance"]);
            else if(v.hasProperty("performance"))throw std::invalid_argument("Performance data on incompatible module");
            d.graph.nodes.push_back(std::move(n));
        }
        for(const auto& v:*links)d.graph.connections.push_back({readEndpoint(v["from"]),readEndpoint(v["to"])});
        for(const auto& v:*outputs)d.graph.outputs.push_back(readEndpoint(v));
        if(int(value["version"])>=2){
            auto* assets=value["samples"].getDynamicObject();if(!assets||assets->getProperties().size()>kMaximumNodes)throw std::invalid_argument("Invalid sample references");
            for(const auto& s:assets->getProperties()){
                const auto id=s.name.toString().toStdString();const auto& v=s.value;
                if(!v.isObject()||!v["path"].isString()||v["path"].toString().isEmpty()||v["path"].toString().length()>4096||!v["fingerprint"].isString())throw std::invalid_argument("Invalid sample reference");
                const auto path=v["path"].toString(),hash=v["fingerprint"].toString();
                if(path!="builtin:click"&&!juce::File::isAbsolutePath(path))throw std::invalid_argument("Sample paths must be absolute");
                if(!hash.isEmpty()&&hash!="builtin-click-v1"&&(hash.length()!=16||!hash.containsOnly("0123456789abcdef")))throw std::invalid_argument("Invalid sample fingerprint");
                if(std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id&&n.type=="sample.player";}))throw std::invalid_argument("Sample reference has no sample module");
                d.samples[id]={path.toStdString(),hash.toStdString(),{}};
            }
        }
        if(int(value["version"])>=3){
            auto* engines=value["spectral"].getDynamicObject();auto* modulation=value["modulations"].getArray();
            if(!engines||engines->getProperties().size()>kMaximumNodes||!modulation||modulation->size()>kMaximumModulations)throw std::invalid_argument("Invalid engine or modulation state");
            size_t bytes=0;
            for(const auto& entry:engines->getProperties()){
                auto id=entry.name.toString().toStdString();
                if(!entry.value.isString()||std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id&&n.type=="spectral.engine";}))throw std::invalid_argument("Orphan or malformed Spectral state");
                auto text=entry.value.toString().toStdString();bytes+=text.size();if(bytes>1024*1024)throw std::invalid_argument("Spectral state exceeds 1 MiB");
                partials::Patch patch;std::string error;if(!partials::parsePatch(text,patch,error))throw std::invalid_argument(error);
                d.spectral[id]=spectralPreset(std::move(patch));
            }
            for(const auto& m:*modulation)d.graph.modulations.push_back({readEndpoint(m["from"]),readEndpoint(m["to"]),number(m["amount"])});
            for(const auto& n:d.graph.nodes)if(n.type=="spectral.engine"&&!d.spectral.count(n.id))throw std::invalid_argument("Missing Spectral engine state");
        }
    } else throw std::invalid_argument("Not an Optisynth patch");
    validateRack(d.graph);return d;
}
}
