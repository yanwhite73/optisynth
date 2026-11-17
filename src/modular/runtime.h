#pragma once
#include "graph.h"
#include <array>
#include <atomic>

namespace optisynth {
constexpr int quantum = 24, maxParameters = 16;
struct Parameter {
    std::string id, label, unit;
    float minimum, maximum, initial;
    bool logarithmic = false;
};
struct Panel {
    std::string type, name, description, family;
    std::vector<Parameter> parameters;
};
const std::vector<Panel>& panels();
const Panel& panel(const std::string& type);
struct Controls {
    std::array<std::atomic<float>, maxParameters> values{};
};
struct Note { int midi = 48; float velocity = .8f; bool gate = false, trigger = false; };
struct AudioBlock {
    int frames;
    Note note;
    const float* const* inputs;
    float* const* outputs;
};
class AudioModule : public PreparedModule {
public:
    std::shared_ptr<Controls> controls = std::make_shared<Controls>();
    virtual void process(const AudioBlock&) noexcept = 0;
};
Registry moduleRegistry();

// Construct and resolve controls on loader/UI only; render has no allocation,
// ownership changes, locks or string lookup. Mono voice graph at 48 kHz for now.
class Runtime {
public:
    explicit Runtime(const GraphSpec&);
    void render(float* out, int frames, Note note) noexcept;
    std::shared_ptr<Controls> controls(const std::string& node) const;
    std::size_t activeModules() const { return nodes_.size(); }
private:
    struct NodeBuffers {
        AudioModule* module = nullptr;
        std::vector<std::array<float, quantum>> outputs;
        std::vector<const float*> in;
        std::vector<float*> out;
    };
    GraphSpec spec_;
    PreparedGraph graph_;
    std::vector<NodeBuffers> nodes_;
    std::array<float, quantum> zero_{};
    std::vector<const float*> roots_;
};
// First playable patch builder; arbitrary graphs also work through Runtime.
GraphSpec instrumentPatch(const std::string& source, const std::string& envelope,
                          const std::string& filter, bool amplitudeEnvelope,
                          bool filterEnvelope);
} // namespace optisynth
