// Off-audio-thread graph compilation and lazy processor construction.
// No renderer is implied by a valid plan: a subsequent runtime owns buffers,
// per-voice instances, event scheduling and atomic plan exchange.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace optisynth {

struct PerformanceData;

enum class SignalType { Audio, Control, Event };
enum class Scope { Voice, Global };
struct Port {
    std::string id;
    SignalType type = SignalType::Audio;
    bool required = false;
    int channels = 1; // Audio: mono or stereo. Control/event ports are single-channel.
};
struct NodeSpec {
    std::string id, type;
    std::vector<std::pair<std::string, float>> parameters;
    std::shared_ptr<const PerformanceData> performance;
    NodeSpec() = default;
    NodeSpec(std::string name,std::string model,std::vector<std::pair<std::string,float>> values,
             std::shared_ptr<const PerformanceData> banks={})
        :id(std::move(name)),type(std::move(model)),parameters(std::move(values)),performance(std::move(banks)){}
};
struct Endpoint { std::string node, port; };
struct Connection { Endpoint source, destination; };
// destination.port identifies a parameter, not an input socket. Amount is a
// signed fraction of its normalised range (logarithmic for positive log controls).
struct Modulation { Endpoint source, destination; float amount = 0; };
struct GraphSpec {
    int version = 1;
    Scope scope = Scope::Voice;
    std::vector<NodeSpec> nodes;
    std::vector<Connection> connections;
    std::vector<Endpoint> outputs;
    std::vector<Modulation> modulations;
};

// Module construction/preparation belongs to the loader. The DSP execution
// interface will be added with the graph runtime, rather than hidden here.
class PreparedModule {
public:
    virtual ~PreparedModule() = default;
};
struct PrepareContext { double sampleRate = 48000; int maximumBlock = 256; Scope scope = Scope::Voice; };
struct ModuleType {
    std::string id;
    std::vector<Port> inputs, outputs;
    std::function<std::unique_ptr<PreparedModule>(const NodeSpec&, const PrepareContext&)> create;
    std::vector<std::string> parameters;
};
using Registry = std::vector<ModuleType>;

struct ResolvedEndpoint { int node = -1, port = -1; };
struct ResolvedConnection { ResolvedEndpoint source, destination; SignalType type; };
struct ResolvedModulation { ResolvedEndpoint source, destination; int index; };
struct GraphPlan {
    Scope scope = Scope::Voice;
    // Source-spec indices, in dependency order. Runtime node indices refer here.
    std::vector<int> nodes;
    std::vector<ResolvedConnection> connections;
    std::vector<ResolvedEndpoint> outputs;
    std::vector<std::string> unusedNodes;
    std::vector<ResolvedModulation> modulations;
};
struct PreparedGraph {
    GraphPlan plan;
    std::vector<std::unique_ptr<PreparedModule>> modules;
};

constexpr int kMaximumNodes = 128, kMaximumConnections = 1024, kMaximumPorts = 64;
constexpr int kMaximumModulations = 128;
// Throw invalid_argument on malformed graphs. Validate first, then construct
// only reachable processors. A failure destroys the partial new graph and does
// not mutate a caller's currently playing graph. Both calls are loader-only.
GraphPlan compileGraph(const GraphSpec&, const Registry&);
PreparedGraph prepareGraph(const GraphSpec&, const Registry&, const PrepareContext&);

} // namespace optisynth
