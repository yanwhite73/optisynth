#pragma once
#include <JuceHeader.h>
#include "../src/modular/rack.h"
#include <set>

namespace optisynth {
struct SampleReference { std::string path,fingerprint;SamplePtr data; };
struct RackDocument { GraphSpec graph; float gain=.25f; std::map<std::string,SampleReference> samples; SpectralBank spectral; };
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
    auto* root=new juce::DynamicObject;root->setProperty("format","optisynth.rack");root->setProperty("version",3);root->setProperty("gain",document.gain);
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
        node->setProperty("parameters",juce::var(params));nodes.add(node);
    }
    for(const auto& c:document.graph.connections){auto* link=new juce::DynamicObject;link->setProperty("from",endpointJSON(c.source));link->setProperty("to",endpointJSON(c.destination));links.add(link);}
    for(const auto& e:document.graph.outputs)outputs.add(endpointJSON(e));
    root->setProperty("nodes",nodes);root->setProperty("connections",links);root->setProperty("outputs",outputs);return root;
}
inline RackDocument readRack(const juce::var& value) {
    if(!value.isObject()||!value["version"].isInt()||int(value["version"])<1||int(value["version"])>3)throw std::invalid_argument("Unsupported patch version");
    RackDocument d;d.gain=number(value["gain"]);
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
        if(int(value["version"])==3){
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
