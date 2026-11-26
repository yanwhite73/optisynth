// Mala: a desktop host for the open Beads texture-synthesizer DSP (programme item 1,
// requests 136-140). See granular_tape.cpp and docs/research/modules/MALA.md.
#pragma once
#include "runtime.h"

namespace optisynth {
Panel granularTapePanel();
void granularTapePorts(ModuleType& type);
std::unique_ptr<AudioModule> makeGranularTape(const NodeSpec& node);
// The pitch knob's law (virtual notches at 0, 1, 3, 4, 7, 12, 24 semitones), for the panel readout.
float granularTapePitch(float knob);
// The attenurandomizer law: a centre dead band, then +-1.
float granularTapeAmount(float knob);
// Meters published in Controls::meters for the panel lights.
enum GranularTapeMeter { kTapeInputLevel, kTapeSeedActivity, kTapeReserve, kTapeSynthMode };
}
