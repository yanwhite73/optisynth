#include "runtime.h"
#include <algorithm>
#include <stdexcept>

namespace optisynth {
Runtime::Runtime(const GraphSpec& spec,const SampleBank& samples) : spec_(spec) {
    auto registry = moduleRegistry(samples);
    graph_ = prepareGraph(spec, registry, {48000, quantum, Scope::Voice});
    nodes_.resize(graph_.modules.size());
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& type = spec.nodes[graph_.plan.nodes[i]].type;
        auto r = std::find_if(registry.begin(), registry.end(), [&](const auto& t) { return t.id == type; });
        auto& n = nodes_[i];
        n.module = dynamic_cast<AudioModule*>(graph_.modules[i].get());
        if (!n.module) throw std::invalid_argument("Module has no audio implementation");
        n.outputs.resize(r->outputs.size());
        n.in.resize(r->inputs.size(), zero_.data());
        for (auto& output : n.outputs) n.out.push_back(output.data());
        for (const auto& p : r->inputs) if (p.type == SignalType::Event)
            throw std::invalid_argument("Event cables are not implemented yet");
    }
    for (const auto& c : graph_.plan.connections)
        nodes_[c.destination.node].in[c.destination.port] = nodes_[c.source.node].out[c.source.port];
    for (const auto& o : graph_.plan.outputs) {
        const auto& type = spec.nodes[graph_.plan.nodes[o.node]].type;
        auto r = std::find_if(registry.begin(), registry.end(), [&](const auto& t) { return t.id == type; });
        if (r->outputs[o.port].type != SignalType::Audio)
            throw std::invalid_argument("Instrument output must be audio");
        roots_.push_back(nodes_[o.node].out[o.port]);
    }
}
void Runtime::render(float* out, int frames, Note note) noexcept {
    while (frames > 0) {
        const int n = std::min(frames, quantum);
        for (auto& node : nodes_) {
            for (auto* p : node.out) std::fill_n(p, n, 0.f);
            node.module->process({n, note, node.in.data(), node.out.data()});
        }
        std::fill_n(out, n, 0.f);
        for (const auto* root : roots_) for (int i = 0; i < n; ++i) out[i] += root[i];
        note.trigger = false;
        out += n; frames -= n;
    }
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
