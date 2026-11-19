#include "runtime.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optisynth {
Runtime::Runtime(const GraphSpec& spec,const SampleBank& samples,const SpectralBank& spectral) : spec_(spec) {
    auto registry = moduleRegistry(samples,spectral);
    const auto plan=compileGraph(spec,registry);int engines=0;
    for(int i:plan.nodes)if(spec.nodes[i].type=="spectral.engine")++engines;
    if(engines>maximumSpectralEngines)throw std::invalid_argument("Maximum two active Spectral engines per rack");
    graph_ = prepareGraph(spec, registry, {48000, quantum, Scope::Voice});
    nodes_.resize(graph_.modules.size());
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& type = spec.nodes[graph_.plan.nodes[i]].type;
        auto r = std::find_if(registry.begin(), registry.end(), [&](const auto& t) { return t.id == type; });
        auto& n = nodes_[i];
        n.module = dynamic_cast<AudioModule*>(graph_.modules[i].get());
        if (!n.module) throw std::invalid_argument("Module has no audio implementation");
        n.outputs.resize(r->outputs.size());
        n.right.resize(r->outputs.size());
        n.in.resize(r->inputs.size(), zero_.data());
        n.inRight.resize(r->inputs.size(),zero_.data());
        for (auto& output : n.outputs) n.out.push_back(output.data());
        for(auto& output:n.right)n.outRight.push_back(output.data());
        for(const auto& p:r->outputs)n.channels.push_back(p.channels);
        n.parameters=panel(type).parameters;
        if(n.parameters.size()>maxParameters)throw std::invalid_argument("Too many module parameters");
        for(size_t k=0;k<n.parameters.size();++k)n.base[k]=n.module->controls->values[k].load();
        n.sliceIn.resize(n.in.size());n.sliceRight.resize(n.in.size());
        n.sliceOut.resize(n.out.size());n.sliceOutRight.resize(n.out.size());
        const auto count=std::count_if(r->outputs.begin(),r->outputs.end(),[](const Port& p){return p.type==SignalType::Event;});
        n.additionalEvents.resize(count>0?size_t(count-1):0);
        n.eventOutputs.resize(r->outputs.size(),nullptr);
        int eventIndex=0;
        for(size_t p=0;p<r->outputs.size();++p)if(r->outputs[p].type==SignalType::Event)
            n.eventOutputs[p]=eventIndex++==0?&n.events:&n.additionalEvents[size_t(eventIndex-2)];
    }
    for (const auto& c : graph_.plan.connections) {
        if(c.type==SignalType::Event){
            nodes_[c.destination.node].noteInput=nodes_[c.source.node].eventOutputs[size_t(c.source.port)];continue;
        }
        nodes_[c.destination.node].in[c.destination.port] = nodes_[c.source.node].out[c.source.port];
        nodes_[c.destination.node].inRight[c.destination.port] = nodes_[c.source.node].outRight[c.source.port];
    }
    for(size_t i=0;i<spec.modulations.size();++i)routes_->amounts[i]=spec.modulations[i].amount;
    for(const auto& m:graph_.plan.modulations)nodes_[m.destination.node].routes.push_back(
        {nodes_[m.source.node].out[m.source.port],m.destination.port,m.index});
    for (const auto& o : graph_.plan.outputs) {
        const auto& type = spec.nodes[graph_.plan.nodes[o.node]].type;
        auto r = std::find_if(registry.begin(), registry.end(), [&](const auto& t) { return t.id == type; });
        if (r->outputs[o.port].type != SignalType::Audio)
            throw std::invalid_argument("Instrument output must be audio");
        roots_.push_back(nodes_[o.node].out[o.port]);
        rightRoots_.push_back(nodes_[o.node].outRight[o.port]);
    }
}
void Runtime::render(float* out, int frames, Note note) noexcept {
    std::array<float,quantum> right;
    while(frames>0){const int n=std::min(frames,quantum);render(out,right.data(),n,note);note.trigger=false;out+=n;frames-=n;}
}
void Runtime::render(float* out,float* right,int frames,Note note,const ClockFrame* clockFrames) noexcept {
    while (frames > 0) {
        const int n = std::min(frames, quantum);
        std::array<ClockFrame,quantum> localClock;
        if(!clockFrames)for(int i=0;i<n;++i)localClock[i]=clock.next();
        const auto* time=clockFrames?clockFrames:localClock.data();
        for (auto& node : nodes_) {
            node.events.begin(n);
            for(auto& bus:node.additionalEvents)bus.begin(n);
            for (auto* p : node.out) std::fill_n(p, n, 0.f);
            for (auto* p : node.outRight) std::fill_n(p,n,0.f);
            std::fill_n(node.offset.data(),node.parameters.size(),0.f);
            for(const auto& m:node.routes){
                const float source=std::isfinite(m.source[0])?std::clamp(m.source[0],-1.f,1.f):0.f;
                const float depth=routes_->amounts[m.index].load(std::memory_order_relaxed);
                if(std::isfinite(depth))node.offset[m.parameter]+=source*std::clamp(depth,-1.f,1.f);
            }
            const auto revision=node.module->controls->editRevision.load();
            if(!(revision&1)){
                for(size_t i=0;i<node.parameters.size();++i)node.candidate[i]=node.module->controls->values[i].load();
                if(revision==node.module->controls->editRevision.load())std::copy_n(node.candidate.data(),node.parameters.size(),node.base.data());
            }
            for(size_t i=0;i<node.parameters.size();++i){
                const auto& p=node.parameters[i];float base=node.base[i];
                base=std::isfinite(base)?std::clamp(base,p.minimum,p.maximum):p.initial;
                const float offset=std::clamp(node.offset[i],-1.f,1.f);
                float value=base;
                if(offset!=0)value=p.logarithmic&&p.minimum>0?
                    base*std::exp(offset*std::log(p.maximum/p.minimum)):base+offset*(p.maximum-p.minimum);
                node.effective[i]=std::clamp(value,p.minimum,p.maximum);
            }
            if(!node.noteInput)node.module->process({n,note,node.in.data(),node.out.data(),node.inRight.data(),node.outRight.data(),node.effective.data(),time,&node.events,node.eventOutputs.data()});
            else {
                const auto& bus=*node.noteInput;node.held.beginBlock(bus);
                if(bus.failed()){node.routedNote.gate=false;node.routedNote.trigger=false;}
                auto event=bus.begin();
                for(int offset=0;offset<n;){
                    bool attack=false;
                    while(event!=bus.end()&&int(event->offset)==offset){
                        const auto change=node.held.apply(*event++);
                        attack|=change==NoteChange::Added||change==NoteChange::Updated;
                    }
                    if(const auto* held=node.held.newest())node.routedNote={held->pitch,held->velocity,true,attack};
                    else {node.routedNote.gate=false;node.routedNote.trigger=false;}
                    const int end=event==bus.end()?n:int(event->offset);
                    for(size_t i=0;i<node.in.size();++i){node.sliceIn[i]=node.in[i]+offset;node.sliceRight[i]=node.inRight[i]+offset;}
                    for(size_t i=0;i<node.out.size();++i){node.sliceOut[i]=node.out[i]+offset;node.sliceOutRight[i]=node.outRight[i]+offset;}
                    node.module->process({end-offset,node.routedNote,node.sliceIn.data(),node.sliceOut.data(),node.sliceRight.data(),node.sliceOutRight.data(),node.effective.data(),time+offset,nullptr});
                    offset=end;
                }
            }
            for(size_t p=0;p<node.out.size();++p)if(node.channels[p]==1)std::copy_n(node.out[p],n,node.outRight[p]);
        }
        std::fill_n(out, n, 0.f);
        std::fill_n(right,n,0.f);
        for (const auto* root : roots_) for (int i = 0; i < n; ++i) out[i] += root[i];
        for (const auto* root : rightRoots_) for (int i=0;i<n;++i)right[i]+=root[i];
        note.trigger = false;
        out += n;right+=n; frames -= n;
        if(clockFrames)clockFrames+=n;
    }
}
SpectralBank Runtime::spectralBank() const {
    SpectralBank bank;
    for(size_t i=0;i<nodes_.size();++i)if(auto state=nodes_[i].module->spectralState())bank.emplace(spec_.nodes[graph_.plan.nodes[i]].id,std::move(state));
    return bank;
}
std::shared_ptr<Controls> Runtime::controls(const std::string& node) const {
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        if (spec_.nodes[graph_.plan.nodes[i]].id == node) return nodes_[i].module->controls;
    return {};
}
GraphSpec instrumentPatch(const std::string& source, const std::string& envelope,
                          const std::string& filter, bool amp, bool cutoff) {
    GraphSpec g;
    g.nodes = {{"source", source, {}}, {"envelope", envelope, {}},
               {"filter", filter, {}}, {"vca", "utility.vca", {}}};
    g.connections = {{{"source","out"},{"filter","in"}}};
    if (cutoff) g.connections.push_back({{"envelope","out"},{"filter","mod"}});
    if (amp) {
        g.connections.push_back({{"filter","out"},{"vca","in"}});
        g.connections.push_back({{"envelope","out"},{"vca","gain"}});
    }
    g.outputs = {{amp ? "vca" : "filter", "out"}};
    return g;
}
} // namespace optisynth
