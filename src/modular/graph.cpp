#include "graph.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace optisynth {
namespace {
void require(bool condition, const std::string& error) {
    if (!condition) throw std::invalid_argument(error);
}
int portIndex(const std::vector<Port>& ports, const std::string& id) {
    for (size_t i = 0; i < ports.size(); ++i) if (ports[i].id == id) return static_cast<int>(i);
    throw std::invalid_argument("Unknown port: " + id);
}
void checkPorts(const std::vector<Port>& ports) {
    require(ports.size() <= kMaximumPorts, "Port limit exceeded");
    std::set<std::string> names;
    for (const auto& p : ports) require(!p.id.empty() && names.insert(p.id).second, "Empty/duplicate port ID");
}
}

GraphPlan compileGraph(const GraphSpec& spec, const Registry& registry) {
    require(spec.version == 1, "Unsupported graph version");
    require(!spec.nodes.empty() && spec.nodes.size() <= kMaximumNodes, "Node count outside bounds");
    require(!spec.outputs.empty() && spec.outputs.size() <= kMaximumPorts, "Output count outside bounds");
    require(spec.connections.size() <= kMaximumConnections, "Connection limit exceeded");
    std::map<std::string, const ModuleType*> types;
    for (const auto& type : registry) {
        require(!type.id.empty() && types.emplace(type.id, &type).second, "Empty/duplicate module type");
        checkPorts(type.inputs); checkPorts(type.outputs);
    }
    const int count = static_cast<int>(spec.nodes.size());
    std::map<std::string, int> names;
    std::vector<const ModuleType*> definitions(count);
    for (int i = 0; i < count; ++i) {
        const auto& node = spec.nodes[i];
        require(!node.id.empty() && names.emplace(node.id, i).second, "Empty/duplicate node ID");
        const auto type = types.find(node.type);
        require(type != types.end(), "Unknown module type: " + node.type);
        definitions[i] = type->second;
        std::set<std::string> parameters;
        for (const auto& p : node.parameters)
            require(!p.first.empty() && std::isfinite(p.second) && parameters.insert(p.first).second,
                    "Invalid/duplicate parameter on " + node.id);
    }
    auto endpoint = [&](const Endpoint& e, bool input) {
        const auto n = names.find(e.node);
        require(n != names.end(), "Unknown node: " + e.node);
        const auto& ports = input ? definitions[n->second]->inputs : definitions[n->second]->outputs;
        return ResolvedEndpoint{n->second, portIndex(ports, e.port)};
    };
    std::vector<std::vector<int>> dependencies(count);
    std::set<std::pair<int, int>> connectedInputs;
    std::vector<ResolvedConnection> links;
    for (const auto& c : spec.connections) {
        auto from = endpoint(c.source, false), to = endpoint(c.destination, true);
        const auto type = definitions[from.node]->outputs[from.port].type;
        require(type == definitions[to.node]->inputs[to.port].type, "Port type mismatch");
        require(connectedInputs.emplace(to.node, to.port).second,
                "Input has multiple sources; use an explicit mixer");
        dependencies[to.node].push_back(from.node);
        links.push_back({from, to, type});
    }
    std::vector<ResolvedEndpoint> outputs;
    for (const auto& e : spec.outputs) outputs.push_back(endpoint(e, false));
    // Validate cycles throughout the saved graph, including dormant subgraphs.
    std::vector<int> state(count), order;
    std::function<void(int)> visit = [&](int n) {
        require(state[n] != 1, "Routing cycle requires an explicit feedback design");
        if (state[n] == 2) return;
        state[n] = 1;
        for (int d : dependencies[n]) visit(d);
        state[n] = 2; order.push_back(n);
    };
    for (int i = 0; i < count; ++i) visit(i);
    std::vector<bool> active(count);
    std::function<void(int)> mark = [&](int n) {
        if (active[n]) return;
        active[n] = true;
        for (int d : dependencies[n]) mark(d);
    };
    for (const auto& out : outputs) mark(out.node);
    GraphPlan plan; plan.scope = spec.scope;
    std::vector<int> runtimeIndex(count, -1);
    for (int n : order) {
        if (!active[n]) { plan.unusedNodes.push_back(spec.nodes[n].id); continue; }
        for (size_t p = 0; p < definitions[n]->inputs.size(); ++p)
            require(!definitions[n]->inputs[p].required || connectedInputs.count({n, static_cast<int>(p)}),
                    "Required input missing on " + spec.nodes[n].id);
        runtimeIndex[n] = static_cast<int>(plan.nodes.size());
        plan.nodes.push_back(n);
    }
    for (auto c : links) if (active[c.destination.node]) {
        c.source.node = runtimeIndex[c.source.node]; c.destination.node = runtimeIndex[c.destination.node];
        plan.connections.push_back(c);
    }
    for (auto out : outputs) { out.node = runtimeIndex[out.node]; plan.outputs.push_back(out); }
    return plan;
}

PreparedGraph prepareGraph(const GraphSpec& spec, const Registry& registry, const PrepareContext& context) {
    require(std::isfinite(context.sampleRate) && context.sampleRate >= 8000 && context.sampleRate <= 384000,
            "Unsupported sample rate");
    require(context.maximumBlock > 0 && context.maximumBlock <= 8192, "Unsupported maximum block size");
    require(context.scope == spec.scope, "Voice/global scope mismatch");
    PreparedGraph prepared;
    prepared.plan = compileGraph(spec, registry);
    prepared.modules.reserve(prepared.plan.nodes.size());
    for (int index : prepared.plan.nodes) {
        const auto& node = spec.nodes[index];
        const auto type = std::find_if(registry.begin(), registry.end(), [&](const ModuleType& t) { return t.id == node.type; });
        require(static_cast<bool>(type->create), "No factory for active module " + node.type);
        auto module = type->create(node, context);
        require(module != nullptr, "Module preparation failed: " + node.id);
        prepared.modules.push_back(std::move(module));
    }
    return prepared;
}
} // namespace optisynth
