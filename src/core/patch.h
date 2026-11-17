// Our patch model and its text format.
//
// Format: first line "partials-patch 1"; then one "key value [value ...]" per line; '#'
// starts a comment. Keys are stable machine names, never display text. Unknown keys are
// kept but ignored, and missing keys take the defaults below, so the format can grow.
// Silent dB entries in spectra and curves are written as "x".
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "envelope.h"
#include "spectrum.h"

namespace partials {

constexpr int kOscillators = 4;
constexpr int kFxSlots = 6;
constexpr int kRouteOut = 0;  // routes: 0 = output, 1..6 = FX slot
constexpr int kPatchVersion = 1;

// Cross-modulation slot. Sources 0..3 = oscillator 1..4 output, 4..7 = filter 1..4 output,
// Both follow the oscillator amp envelope and precede Main. OSC precedes the spectral
// filter/oscillator volume; FIL follows both but precedes filter volume (measured 9 November).
struct XMod {
    int type = 0;          // oscillator slots: 0 AM, 1 FM, 2 PM; filter slots: 0 FM, 1 RM
    int source = 0;        // 0..7
    float amount = 0.0f;   // 0..1
};

struct FilterParams {
    bool on = false;
    float cutoffHz = 20480.0f;
    float keyTrack = 0.0f;     // -1..1: 1 keeps the filter fixed relative to the note
    float envDepth = 0.0f;     // -1..1: Cut envelope pulls the cutoff towards max (+) or min (-)
    float velTrack = 0.0f;     // -1..1
    float resonance = 0.0f;    // 0..1   (M2)
    float lpOffset = 61.0f;    // semitones above cutoff; 61 = off   (M2)
    int lpType = 3;            // 1..4 one-pole stages   (M2)
    float volumeDb = -6.0f;
    float pan = 0.0f;          // -1..1
    std::array<XMod, 2> xmod{};  // FM moves the filter position; RM is not built yet
    FilterCurve curve = FilterCurve::flat();
};

struct OscParams {
    bool on = false;
    int semitone = 0;
    float cent = 0.0f;
    bool keyTrack = true;      // off: plays as if note 60
    float symmetry = 0.5f;     // 0.04..0.96
    float startPhase = 0.0f;   // 0..1 of a cycle
    bool freeRun = false;      // random start phase
    float volumeDb = 0.0f;
    float waveMix = 0.5f;      // 0 = wave A, 1 = wave B
    int unisonVoices = 1;      // 1..6; spread voices sound only when unisonDetune > 0 (inferred)
    int unisonLayout = 0;      // 0 normal, 1 = the original's alternative 4-voice layout ("4b")
    float unisonDetune = 0.0f; // cents, outer voices at +-half of this
    float unisonVolume = 1.0f; // level of the non-centre voices, 0.01..1
    float unisonStereo = 0.5f; // 0..1 stereo spread of the voices
    std::array<XMod, 2> xmod{};
    int route1 = kRouteOut, route2 = kRouteOut;
    float routeBalance = -1.0f; // -1 = all to route 1, +1 = all to route 2
    Spectrum waveA = Spectrum::sine();
    Spectrum waveB = Spectrum::sine();
    FilterParams filter;
};

constexpr int kLfos = 5;
constexpr int kMatrixSlots = 24;

struct LfoParams {
    int wave = 0;            // 0 sine, 1 triangle, 2 saw, 3 pulse, 4 impulse, 5 peak, 6 noise,
                             // 7 sample & hold, 8 random pulse
    float rateHz = 1.0f;
    float syncBeats = 0.0f;  // > 0: tempo-synced period in beats (overrides rateHz)
    float delay = 0.0f, attack = 0.0f;  // seconds
    float symmetry = 0.5f;   // 0.01..0.99
    float phase = 0.0f;      // 0..1 start phase
    int polarity = 0;        // 0 bipolar, 1 unipolar +, 2 unipolar -, 3 positive half, 4 negative half
    float smooth = 0.0f;     // 0..0.5
    int freeRun = 0;         // 0 restart per note, 1 free-running, 2 restart per note (mono variant)
};

// Matrix slot: stable source and destination ids (see engine.cpp, "matrix"), amount -1..1.
struct MatrixSlot {
    std::string source = "none";
    std::string dest = "none";
    float amount = 0.0f;
};

constexpr int kArpSteps = 32;

struct ArpStep {
    int transpose = 0;        // semitones
    int velocity = 100;       // 0 = rest, 1..127, 128 = hold the previous note
    float length = 0.5f;      // 0..1 (also the Arp Length matrix source)
    int advance = 0;          // 0 = follow the mode; n = move up n held notes
    bool legato = false;
};

struct ArpParams {
    int mode = 0;  // 0 off, 1 mod-only, 2 up, 3 down, 4 up/down, 5 up/down+, 6 down/up, 7 down/up+,
                   // 8 random, 9 chord, 10 as played
    float resolutionBeats = 0.25f;
    int length = 16;
    int octaves = 1;
    float swing = 0.0f;       // 0..1
    float veloBlend = 0.0f;   // 0 = step velocity, 1 = as played
    float noteLength = 0.5f;  // 0..1
    std::array<ArpStep, kArpSteps> steps{};
};

struct FxSlot {
    std::string type = "none";  // reverb, delay_stereo, delay_mod, delay_pingpong, chorus, phaser,
                                // flanger, filter, compressor, gator, crusher, eq, wah, none
    bool on = false;
    int route = kRouteOut;      // a later slot (1..6) or 0 for the output
    std::map<std::string, float> params;  // keys per effect type, see effects.cpp
};

struct Patch {
    std::string name = "Init";
    float mainVolumeDb = -6.0f;
    std::array<OscParams, kOscillators> osc{};
    std::array<EnvelopeParams, kOscillators> ampEnv{};
    std::array<EnvelopeParams, kOscillators> cutEnv{};
    std::array<EnvelopeParams, 3> modEnv{};
    EnvelopeParams mainEnv{};
    std::array<FxSlot, kFxSlots> fx{};
    std::array<LfoParams, kLfos> lfo{};
    std::array<MatrixSlot, kMatrixSlots> matrix{};
    ArpParams arp{};
    std::map<std::string, std::vector<std::string>> unknown;  // keys this build does not use
};

Patch defaultPatch();  // oscillator 1 on, a sine
// Replaces non-finite values with defaults and clamps every value to its valid range, so a
// damaged or hand-edited file can never drive the engine out of range. loadPatch calls it.
void sanitise(Patch& patch);
bool loadPatch(const std::string& path, Patch& out, std::string& error);
bool savePatch(const std::string& path, const Patch& patch, std::string& error);
bool parsePatch(const std::string& text, Patch& out, std::string& error);
std::string patchText(const Patch& patch);

}  // namespace partials
