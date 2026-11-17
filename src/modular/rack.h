#pragma once
#include "runtime.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optisynth {
// UI/loader helpers. Validate dormant parameters too, without constructing DSP.
inline void validateRack(const GraphSpec& g) {
    for(const auto& node:g.nodes) {
        const auto& p=panel(node.type);
        for(const auto& value:node.parameters) {
            auto it=std::find_if(p.parameters.begin(),p.parameters.end(),[&](const Parameter& x){return x.id==value.first;});
            if(it==p.parameters.end()||!std::isfinite(value.second)||value.second<it->minimum||value.second>it->maximum)
                throw std::invalid_argument("Invalid parameter on "+node.id+": "+value.first);
        }
    }
    auto registry=moduleRegistry();compileGraph(g,registry);
    if(g.scope!=Scope::Voice)throw std::invalid_argument("Only mono voice racks are currently supported");
    for(const auto& root:g.outputs) {
        auto n=std::find_if(g.nodes.begin(),g.nodes.end(),[&](const NodeSpec& n){return n.id==root.node;});
        auto t=std::find_if(registry.begin(),registry.end(),[&](const ModuleType& t){return t.id==n->type;});
        auto p=std::find_if(t->outputs.begin(),t->outputs.end(),[&](const Port& p){return p.id==root.port;});
        if(p->type!=SignalType::Audio)throw std::invalid_argument("Rack output must be audio");
    }
}
inline void setRackConnection(GraphSpec& g,Endpoint destination,Endpoint source) {
    g.connections.erase(std::remove_if(g.connections.begin(),g.connections.end(),[&](const Connection& c){return c.destination.node==destination.node&&c.destination.port==destination.port;}),g.connections.end());
    if(!source.node.empty())g.connections.push_back({std::move(source),std::move(destination)});
}
inline GraphSpec rackPreset(int which) {
    if(which==4||which==5){
        GraphSpec g;g.nodes={{"source","sample.player",{}},{"resonator","modal.resonator",{{"input_gain",.65f}}}};
        g.connections={{{"source","out"},{"resonator","in"}}};g.outputs={{"resonator","out"}};
        if(which==5){g.nodes.push_back({"mixer","utility.mix",{{"gain_a",.4f},{"gain_b",.8f}}});
            g.connections.push_back({{"source","out"},{"mixer","a"}});g.connections.push_back({{"resonator","out"},{"mixer","b"}});g.outputs={{"mixer","out"}};}
        return g;
    }
    if(which==1)return instrumentPatch("modal.voice","spectral.ahdsfr","filter.bypass",false,false);
    if(which==2){
        auto g=instrumentPatch("drum.analog","spectral.adsr","mutable.bandpass",false,false);
        g.nodes.push_back({"resonator","modal.resonator",{}});
        setRackConnection(g,{"resonator","in"},{"source","out"});
        setRackConnection(g,{"filter","in"},{"resonator","out"});return g;
    }
    auto g=instrumentPatch("osc.variable","spectral.adsr","mutable.lowpass",true,true);
    if(which==3){
        g.nodes.push_back({"second","osc.variable",{{"transpose",7.05f},{"shape",.25f}}});
        g.nodes.push_back({"mixer","utility.mix",{{"gain_a",.65f},{"gain_b",.65f}}});
        setRackConnection(g,{"mixer","a"},{"source","out"});setRackConnection(g,{"mixer","b"},{"second","out"});
        setRackConnection(g,{"filter","in"},{"mixer","out"});
    }
    return g;
}
}
