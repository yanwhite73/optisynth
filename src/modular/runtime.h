#pragma once
#include "graph.h"
#include "sample.h"
#include "spectral.h"
#include "timing.h"
#include <array>
#include <atomic>

namespace optisynth {
class PerformanceExchange;
struct ReferenceSequenceState;
class VoiceMixer;
constexpr int quantum = 24, maxParameters = 768, maximumVoices = 8;
struct Parameter {
    std::string id, label, unit;
    float minimum, maximum, initial;
    bool logarithmic = false;
    double step = .0001;
};
struct Panel {
    std::string type, name, description, family;
    std::vector<Parameter> parameters;
};
const std::vector<Panel>& panels();
const Panel& panel(const std::string& type);
struct Controls {
    std::array<std::atomic<float>, maxParameters> values{};
    std::atomic<int> playhead{-1};
    std::array<std::atomic<int>,4> laneHeads{};
    std::atomic<int> cycle{0};
    std::atomic<bool> stepFired{false};
    // UI batches publish an even revision; audio retains the last complete edit.
    std::atomic<uint64_t> editRevision{0};
    std::shared_ptr<PerformanceExchange> performance;
    std::shared_ptr<ReferenceSequenceState> referenceSequence;
};
struct RouteControls { std::array<std::atomic<float>,kMaximumModulations> amounts{}; };
// bend is in semitones and wheel in 0..1; both are shared performance controls.
struct Note { int midi = 48; float velocity = .8f; bool gate = false, trigger = false; float bend = 0, wheel = 0; };
struct AudioBlock {
    int frames;
    Note note;
    const float* const* inputs;
    float* const* outputs;
    const float* const* inputsRight = nullptr;
    float* const* outputsRight = nullptr;
    const float* parameters = nullptr;
    const ClockFrame* clock = nullptr;
    NoteEventBlock* events = nullptr;
    NoteEventBlock* const* eventOutputs = nullptr; // indexed by resolved output port
};
class AudioModule : public PreparedModule {
public:
    std::shared_ptr<Controls> controls = std::make_shared<Controls>();
    virtual void process(const AudioBlock&) noexcept = 0;
    virtual SpectralPtr spectralState() const { return {}; }
};
Registry moduleRegistry(const SampleBank& samples={},const SpectralBank& spectral={});

// Construct and resolve controls on loader/UI only; render has no allocation,
// ownership changes, locks or string lookup. Mono/stereo ports at 48 kHz, with
// shared monophonic note context; Spectral retains its internal voice/arp engine.
// A voice.mix node makes everything patched upstream of it run once per voice
// (request 57). Keyboard events then allocate voices; without one, behaviour is mono.
class Runtime {
public:
    explicit Runtime(const GraphSpec&,const SampleBank& samples={},const SpectralBank& spectral={});
    void render(float* out, int frames, Note note) noexcept;
    // keys: optional keyboard note events for a voice group, applied in the first quantum.
    void render(float* left,float* right,int frames,Note note,const ClockFrame* clockFrames=nullptr,const NoteEventBlock* keys=nullptr) noexcept;
    SampleClock clock; // offline default; Player supplies the shared transport
    std::shared_ptr<Controls> controls(const std::string& node) const;
    SpectralBank spectralBank() const;
    std::shared_ptr<RouteControls> routeControls() const { return routes_; }
    std::size_t activeModules() const { return nodes_.size(); }
    // Offline inspection of the voice group (not for concurrent use with render).
    int voiceGroupSize() const noexcept { return mixer_<0?0:maximumVoices; }
    Note voiceNote(int voice) const noexcept { return voice>=0&&voice<maximumVoices?slots_[size_t(voice)].note:Note{}; }
private:
    struct NodeBuffers {
        AudioModule* module = nullptr;
        std::vector<std::array<float, quantum>> outputs;
        std::vector<std::array<float, quantum>> right;
        std::vector<const float*> in;
        std::vector<float*> out;
        std::vector<const float*> inRight;
        std::vector<float*> outRight;
        std::vector<Parameter> parameters;
        std::vector<int> channels;
        struct Route { const float* source; int parameter,index; };
        std::vector<Route> routes;
        std::array<float,maxParameters> effective{},offset{},base{},candidate{};
        NoteEventBlock events;
        std::vector<NoteEventBlock> additionalEvents;
        std::vector<NoteEventBlock*> eventOutputs;
        const NoteEventBlock* noteInput=nullptr;
        HeldNotes held;
        Note routedNote;
        std::vector<const float*> sliceIn,sliceRight;
        std::vector<float*> sliceOut,sliceOutRight;
    };
    struct VoiceChange { int offset; Note note; };
    struct VoiceSlot { Note note; bool active=false; uint32_t source=0; uint64_t noteId=0, started=0, released=0; };
    void prepareBuffers(NodeBuffers&, AudioModule*, const ModuleType&, const std::string& type);
    NodeBuffers& instance(int node,int voice) noexcept { return voice==0?nodes_[size_t(node)]:voiceCopies_[size_t(node)][size_t(voice-1)]; }
    void computeParameters(NodeBuffers&) noexcept;
    void processNode(NodeBuffers&,int frames,const ClockFrame*,const Note&) noexcept;
    void processVoice(NodeBuffers&,int frames,const ClockFrame*,int voice) noexcept;
    void allocate(const NoteEventBlock*,int frames,int voices,const Note& performance) noexcept;
    void pushChange(int voice,int offset,const Note&) noexcept;
    GraphSpec spec_;
    PreparedGraph graph_;
    std::vector<NodeBuffers> nodes_;
    std::vector<int> order_;
    int mixer_=-1;
    VoiceMixer* voiceMixer_=nullptr;
    std::vector<char> voiceNode_;
    std::vector<std::vector<NodeBuffers>> voiceCopies_;
    std::vector<std::unique_ptr<PreparedModule>> voiceModules_;
    std::array<VoiceSlot,maximumVoices> slots_{};
    std::array<Note,maximumVoices> voiceStart_{};
    std::array<std::array<VoiceChange,32>,maximumVoices> changes_{};
    std::array<int,maximumVoices> changeCount_{};
    std::array<const float*,maximumVoices> mixLeft_{},mixRight_{};
    uint64_t voiceClock_=0;
    std::array<float, quantum> zero_{};
    std::vector<const float*> roots_;
    std::vector<const float*> rightRoots_;
    std::shared_ptr<RouteControls> routes_=std::make_shared<RouteControls>();
};
// First playable patch builder; arbitrary graphs also work through Runtime.
GraphSpec instrumentPatch(const std::string& source, const std::string& envelope,
                          const std::string& filter, bool amplitudeEnvelope,
                          bool filterEnvelope);
} // namespace optisynth
