// Voices, oscillator playback and the effect chain. See ../../docs/design.md.
#pragma once

#include <array>
#include <memory>
#include <random>
#include <vector>

#include "arp.h"
#include "effects.h"
#include "envelope.h"
#include "patch.h"
#include "live_controls.h"
#include "table_bank.h"

namespace partials {

constexpr int kControlBlock = 16;   // modulated values update every 16 samples, then ramp
constexpr int kMaxVoices = 32;
constexpr int kMaxUnison = 6;
constexpr int kBuses = kFxSlots + 1;  // bus 0 = output, 1..6 = FX slot inputs

// Filter position as a table level (fractional, 1..216) for an oscillator.
// cutoffNorm is the 0..1 cutoff after modulation; oscNote is the note the oscillator plays.
double filterLevel(double cutoffNorm, double oscNote);
// 20 Hz..20480 Hz on a log scale, ten octaves centred on 640 Hz, to 0..1; and back.
double cutoffToNorm(double hz);
double normToCutoff(double norm);
// Symmetry phase warp on a 0..kTableSize phase.
double warpPhase(double phase, double symmetry);
// Unison voice positions in -1..1 for a voice count and layout (our own layout).
int unisonPositions(int voices, int layout, float* positions);

// Tables for one oscillator, shared by all voices.
struct OscTables {
    TableBank a, b;
    bool filtered = false;
    Spectrum sourceA, sourceB;
    FilterCurve sourceCurve;
};
using TableCache = std::array<std::shared_ptr<OscTables>, kOscillators>;

struct Note {
    int pitch = 60;
    float velocity = 1.0f;  // 0..1
};

// Performance controllers and clock shared by all voices (written by the engine owner,
// read by voices at control rate).
struct Controllers {
    float modWheel = 0.0f, aftertouch = 0.0f, pitchWheel = 0.0f;  // 0..1, 0..1, -1..1
    float cc[4] = {0.0f, 0.0f, 0.0f, 0.0f};                        // MIDI CC 16..19, 0..1
    double bpm = 120.0;
    double time = 0.0;  // seconds since the engine started (free-running LFOs)
    float arpVelocity = 0.0f, arpTranspose = 0.0f, arpLength = 0.0f;  // current arpeggiator step
};

// Matrix destinations, resolved from ids once per voice start.
enum class ModDest {
    None, OscPitch, OscSymmetry, OscWaveMix, OscVolume, OscXMod1, OscXMod2, FilCutoff, FilResonance,
    FilVolume, FilPan, FilEnvDepth, FilXMod1, FilXMod2, MainVolume, MainPitch, MatrixDepth, LfoSpeed
};
enum class ModSource {
    None, Lfo, ModWheel, Aftertouch, Velocity, Note, PitchWheelUp, PitchWheelDown, Cc, ModEnv, AmpEnv,
    CutEnv, MainEnv, Alternate, Constant, RandomBi, RandomUni, ArpVelocity, ArpTranspose, ArpLength, Unsupported
};

// A matrix slot resolved from its ids (done at load time, so note-on never parses strings).
struct ModRoute {
    int slotIndex = 0;  // original patch slot, retained when inactive routes are omitted
    ModSource source = ModSource::None;
    int sourceIndex = 0;
    ModDest dest = ModDest::None;
    int destIndex = 0;
    float amount = 0.0f;
};
// Resolves the patch's matrix; returns the number of usable routes written to out.
int resolveMatrix(const Patch& patch, std::array<ModRoute, kMatrixSlots>& out, int* unsupported = nullptr);

// bus[2*b] and bus[2*b+1] are the left and right channels of bus b, each kControlBlock long.
using BusArray = std::array<float*, 2 * kBuses>;

class Voice {
public:
    void start(const Patch& patch, const TableCache& tables,
               Note note, double sampleRate, std::mt19937& rng, const Controllers* controllers,
               float alternate, const std::array<ModRoute, kMatrixSlots>& routes, int routeCount);
    void noteOff();
    bool active() const { return active_; }
    Note note() const { return note_; }
    void inheritPlayback(const Voice& previous);  // after start() binds the new patch/tables
    // Adds one control block (up to kControlBlock frames) into the buses.
    void render(const BusArray& bus, int frames);

private:
    struct Unison {
        double phase = 0.0, increment = 0.0, baseIncrement = 0.0;
        float gainL = 0.0f, gainR = 0.0f;
    };
    struct OscState {
        bool on = false;
        int tier = 0, voices = 1, sourceVoice = 0;
        std::array<Unison, kMaxUnison> uni{};
        double smoothCutoff = 0.0;
        double level = 1.0;  // current filter level (ramped)
        // resonance band-pass (state-variable) and LP+ stages, per channel
        float ic1[2] = {0, 0}, ic2[2] = {0, 0};
        float lp[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
        float resG = 0.0f, resDenom = 1.0f, resGain = 0.0f, lpA = 1.0f;
        int lpStages = 0;
        float gainL = 0.0f, gainR = 0.0f;  // filter volume and pan; per-voice Main modulation
        float baseGain = 0.0f;             // the same without pan, before matrix volume
        float w1 = 1.0f, w2 = 0.0f;        // route balance
        int route1 = 0, route2 = 0;
        Envelope amp, cut;
    };

    struct LfoState {
        double phase = 0.0, t = 0.0, uniOffset = 1.0;
        float value = 0.0f, held = 0.0f, smoothed = 0.0f;
        bool wrapped = false;
    };
    // Matrix sums for this control block, per oscillator where it applies.
    struct ModSums {
        float pitch[kOscillators], symmetry[kOscillators], waveMix[kOscillators], oscVol[kOscillators];
        float xmod[kOscillators][2], filXmod[kOscillators][2], cutoff[kOscillators], resonance[kOscillators];
        float filVol[kOscillators], pan[kOscillators], envDepth[kOscillators];
        float mainVol, mainPitch, lfoSpeed[kLfos];
    };

    void updateControl(int osc, double& levelTarget);
    void runModulation(double dt);
    float sourceValue(const ModRoute& r) const;
    float uniform();  // per-voice random number, 0..1

    const Patch* patch_ = nullptr;
    const TableCache* tables_ = nullptr;
    Note note_{};
    double sampleRate_ = 48000.0;
    std::array<OscState, kOscillators> osc_{};
    Envelope main_;
    std::array<Envelope, 3> modEnv_{};
    std::array<LfoState, kLfos> lfo_{};
    std::array<ModRoute, kMatrixSlots> routes_{};
    int routeCount_ = 0;
    ModSums mod_{};
    const Controllers* ctl_ = nullptr;
    float alternate_ = 1.0f, randomBi_ = 0.0f, randomUni_ = 0.0f;
    unsigned int seed_ = 1;
    bool active_ = false;
    // Each oscillator's output (after its amp envelope, before its volume) as a modulation
    // source: this sample's value and the previous sample's.
    float curOsc_[kOscillators] = {0, 0, 0, 0}, curFil_[kOscillators] = {0, 0, 0, 0};
    float prevOsc_[kOscillators] = {0, 0, 0, 0}, prevFil_[kOscillators] = {0, 0, 0, 0};
};

// Real-time rules: load() allocates and is slow, so call it off the audio thread. noteOn,
// noteOff, allNotesOff, reset, setTempo and render never allocate, lock or touch files.
class Engine {
public:
    explicit Engine(double sampleRate);
    // Builds the tables and effects (the expensive part; the original does this at load time).
    void load(const Patch& patch, const TableCache& reusable = {}, bool liveControlMode = false);
    void setLiveControls(const LiveValues& values);  // realtime: bounded, no allocation
    const LiveValues& currentLiveControls() const { return liveCurrent_; }
    const TableCache& tableCache() const { return tables_; }  // copy under processor's engine lock
    void inheritPerformance(Engine& previous);  // loader thread, while audio is excluded
    void noteOn(Note note);
    void noteOff(int pitch);
    void allNotesOff();
    void reset();  // silences every voice and effect tail at once
    void setTempo(double bpm);
    // Host position is valid at the start of the next render. While stopped or missing,
    // synced effects free-run; MIDI note-ons do not restart their clocks.
    void setTransport(double ppq, bool playing);
    void setModWheel(float v) { ctl_.modWheel = v; }
    void setAftertouch(float v) { ctl_.aftertouch = v; }
    void setPitchWheel(float v) { ctl_.pitchWheel = v; }
    void setController(int cc, float v) {  // MIDI CC 16..19 are matrix sources
        if (cc >= 16 && cc <= 19) ctl_.cc[cc - 16] = v;
    }
    // Renders frames of stereo audio (overwrites the buffers). Any frame count >= 0.
    void render(float* left, float* right, int frames);
    int activeVoices() const;

private:
    struct Slot {
        Voice voice;
        int pitch = -1;  // -1 once released, so a retriggered pitch gets its own voice
        unsigned long long age = 0;
    };
    void startVoice(Note note);
    void releasePitch(int pitch);
    void advanceLiveControls(int frames);

    double sampleRate_;
    double bpm_ = 120.0;
    Patch patch_;
    LiveValues liveTarget_{},liveCurrent_{};
    bool liveControlMode_=false, liveMoving_=false;
    TableCache tables_{};
    std::array<std::unique_ptr<Effect>, kFxSlots> fx_{};
    std::array<int,kFxSlots> fxControlOffsets_{};
    std::vector<Slot> slots_;  // fixed pool of kMaxVoices; the oldest is stolen when full
    unsigned long long counter_ = 0;
    std::mt19937 rng_;
    Controllers ctl_{};
    float alternate_ = 1.0f;
    std::array<float, 128> heldNotes_{};
    std::array<ModRoute, kMatrixSlots> routes_{};
    int routeCount_ = 0;
    Arpeggiator arp_;
    std::array<Arpeggiator::Event, Arpeggiator::kMaxEvents> arpEvents_{};
    std::array<float, 2 * kBuses * kControlBlock> busData_{};
    BusArray bus_{};
    // Audio-rate cross-modulation runs at 2x, then each send is low-passed before
    // entering the effects at the host rate. Fixed storage; no render-time allocation.
    bool oversample_ = false;
    std::array<bool, kBuses> activeSends_{};
    std::array<float, 2 * kBuses * 2 * kControlBlock> oversampledBus_{};
    std::array<float, 63> decimationKernel_{};
    std::array<std::array<float, 64>, 2 * kBuses> decimationHistory_{};
    unsigned int decimationPosition_ = 0;
    // Output DC blocker (our addition, not in the original): a one-pole high-pass at 5 Hz.
    // Symmetry warps can leave a DC offset that would thump at note on and off.
    double dcX_[2] = {0, 0}, dcY_[2] = {0, 0};
    OutputStage output_;
    double transportPpq_ = 0.0;
    bool transportPlaying_ = false;
};

}  // namespace partials
