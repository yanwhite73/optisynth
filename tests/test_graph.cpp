#include "../src/modular/graph.h"
#include <cstdio>
#include <stdexcept>
#include <limits>

using namespace optisynth;
namespace {
int checks = 0, made = 0, alive = 0;
void check(bool ok, const char* name) {
    ++checks;
    if (!ok) throw std::runtime_error(name);
}
struct Counted : PreparedModule {
    Counted() { ++made; ++alive; }
    ~Counted() override { --alive; }
};
std::unique_ptr<PreparedModule> create(const NodeSpec&, const PrepareContext&) {
    return std::make_unique<Counted>();
}
Registry registry() {
    return {
        {"oscillator", {}, {{"out", SignalType::Audio}}, create},
        {"envelope", {}, {{"out", SignalType::Control}}, create},
        {"vca", {{"in", SignalType::Audio, true}, {"gain", SignalType::Control, true}}, {{"out", SignalType::Audio}}, create},
        {"mix", {{"a", SignalType::Audio, true}, {"b", SignalType::Audio, true}}, {{"out", SignalType::Audio}}, create},
        {"unused-heavy", {}, {{"out", SignalType::Audio}}, [](const NodeSpec&, const PrepareContext&) -> std::unique_ptr<PreparedModule> {
            throw std::runtime_error("Unused processor was constructed");
        }}
    };
}
GraphSpec patch() {
    GraphSpec p;
    // Deliberately out of dependency order. One envelope drives both sources.
    p.nodes = {{"sum", "mix", {}}, {"a", "oscillator", {}}, {"gainB", "vca", {}},
               {"sharedEnv", "envelope", {}}, {"b", "oscillator", {}},
               {"gainA", "vca", {}}, {"unused", "unused-heavy", {}}};
    p.connections = {{{"a","out"},{"gainA","in"}}, {{"b","out"},{"gainB","in"}},
        {{"sharedEnv","out"},{"gainA","gain"}}, {{"sharedEnv","out"},{"gainB","gain"}},
        {{"gainA","out"},{"sum","a"}}, {{"gainB","out"},{"sum","b"}}};
    p.outputs = {{"sum", "out"}};
    return p;
}
template<class F> void rejects(F fn, const char* name) {
    bool failed = false;
    try { fn(); } catch (const std::invalid_argument&) { failed = true; }
    check(failed, name);
}
}
int main() {
    try {
        auto p = patch(); auto r = registry();
        auto plan = compileGraph(p, r);
        check(made == 0, "Planning must not construct modules");
        check(plan.nodes.size() == 6 && plan.unusedNodes == std::vector<std::string>{"unused"}, "Output pruning");
        for (const auto& edge : plan.connections)
            check(edge.source.node < edge.destination.node, "Dependency ordering");
        check(plan.connections.size() == 6, "Control fan-out is retained");
        check(p.nodes[plan.nodes[plan.outputs[0].node]].id == "sum", "Output remapping");
        {
            auto g = prepareGraph(p, r, {});
            check(made == 6 && alive == 6 && g.modules.size() == 6, "Only reachable modules constructed; shared envelope once");
        }
        check(alive == 0, "Prepared graph cleans up");
        auto bad = p; bad.connections[0].source = {"sharedEnv", "out"};
        rejects([&] { prepareGraph(bad, r, {}); }, "Reject audio/control mismatch");
        check(made == 6, "Validate entire graph before any construction");
        bad = p; bad.connections[0].source.node = "missing";
        rejects([&] { compileGraph(bad, r); }, "Reject missing node");
        bad = p; bad.connections[0].source.port = "missing";
        rejects([&] { compileGraph(bad, r); }, "Reject missing port");
        bad = p; bad.nodes[1].id = "sum";
        rejects([&] { compileGraph(bad, r); }, "Reject duplicate identity");
        bad = p; bad.connections.push_back(bad.connections[0]);
        rejects([&] { compileGraph(bad, r); }, "Reject multiply driven input");
        bad = p; bad.connections.erase(bad.connections.begin());
        rejects([&] { compileGraph(bad, r); }, "Reject missing required active input");
        bad = p; bad.connections[0].source = {"sum", "out"};
        rejects([&] { compileGraph(bad, r); }, "Reject feedback cycle");
        bad = p; bad.version = 2;
        rejects([&] { compileGraph(bad, r); }, "Reject unknown schema version");
        bad = p; bad.nodes[1].parameters = {{"pitch", std::numeric_limits<float>::infinity()}};
        rejects([&] { compileGraph(bad, r); }, "Reject nonfinite parameter");
        bad = p; bad.nodes.resize(kMaximumNodes + 1);
        rejects([&] { compileGraph(bad, r); }, "Bound node count");
        bad = p; bad.connections.resize(kMaximumConnections + 1);
        rejects([&] { compileGraph(bad, r); }, "Bound connections");
        rejects([&] { prepareGraph(p, r, {48000, 256, Scope::Global}); }, "Explicit voice/global scope");
        rejects([&] { prepareGraph(p, r, {std::numeric_limits<double>::quiet_NaN(), 256}); }, "Reject nonfinite sample rate");
        // A loader failure must destroy the partial replacement graph.
        r[2].create = [](const NodeSpec&, const PrepareContext&) -> std::unique_ptr<PreparedModule> { return nullptr; };
        rejects([&] { prepareGraph(p, r, {}); }, "Factory failure is reported");
        check(alive == 0, "Failed preparation cleans up constructed modules");
        std::printf("%d graph checks passed\n", checks);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Graph check failed: %s\n", e.what()); return 1;
    }
}
