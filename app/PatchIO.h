#pragma once
#include <JuceHeader.h>
#include "../src/modular/rack.h"

namespace optisynth {
struct RackDocument { GraphSpec graph; float gain=.25f; };
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
    auto* root=new juce::DynamicObject;root->setProperty("format","optisynth.rack");root->setProperty("version",1);root->setProperty("gain",document.gain);
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
    if(!value.isObject()||!value["version"].isInt()||int(value["version"])!=1)throw std::invalid_argument("Unsupported patch version");
    RackDocument d;d.gain=number(value["gain"]);
    if(d.gain<0||d.gain>1)throw std::invalid_argument("Output gain outside range");
    if(value["format"].toString()=="optisynth.instrument") {
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
    } else throw std::invalid_argument("Not an Optisynth patch");
    validateRack(d.graph);return d;
}
}
