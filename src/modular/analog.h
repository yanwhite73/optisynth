// Analog-style polyphonic voice modules (request 57) and the voice-group boundary.
// Circuit references: docs/research/instruments/CLASSIC_PAD_ARCHITECTURE.md.
#pragma once
#include "runtime.h"

namespace optisynth {
inline bool isAnalog(const std::string& id) {
    return id.rfind("analog.",0)==0||id=="voice.mix"||id=="mod.wheel"||id.rfind("fx.",0)==0;
}
// Modules that always run once per rack, even when patched upstream of a voice mixer.
inline bool isGlobalOnly(const std::string& id) {
    return id=="voice.mix"||id=="mod.wheel"||id.rfind("fx.",0)==0||id.rfind("seq.",0)==0||id=="spectral.engine";
}
std::vector<Panel> analogPanels();
void analogPorts(const std::string& id, ModuleType& type);
std::unique_ptr<AudioModule> makeAnalog(const NodeSpec&, const PrepareContext&);
// Starter pads: 0 = Oberheim-style 2-pole swell, 1 = Prophet-style 4-pole pad.
GraphSpec padPatch(int model);

// The voice-group boundary. Runtime supplies one input per active voice.
class VoiceMixer : public AudioModule {
public:
    virtual int voices() const noexcept = 0;
    virtual void mix(const float* const* left, const float* const* right, int voices, int frames,
                     float* outLeft, float* outRight, const float* parameters) noexcept = 0;
};
} // namespace optisynth
