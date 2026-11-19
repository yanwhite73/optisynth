#pragma once
#include "sequencer.h"
#include "pattern.h"

namespace optisynth {
inline bool isReferenceSequencer(const std::string& id){return id=="seq.probability"||id=="seq.orbit";}
constexpr int probabilityRows=16,probabilitySteps=32,probabilityGlobals=16,probabilityStride=22;
enum ProbabilityGlobal { PrEnabled,PrDivision,PrLength,PrFirst,PrTranspose,PrScale,PrRoot,PrOctave,PrSeed,PrSwing,PrRepeat,PrFill,PrDensity,PrVelocity,PrGate,PrTraversal };
enum ProbabilityField { PrStepVelocity=16,PrStepGate,PrStepChance,PrStepRatchet,PrStepCondition,PrStepLock };
constexpr int probabilityMute(int row){return probabilityGlobals+row;}
constexpr int probabilityIndex(int step,int field){return probabilityGlobals+probabilityRows+step*probabilityStride+field;}
constexpr int probabilityCount=probabilityIndex(probabilitySteps,0);
static_assert(probabilityCount<=maxParameters,"Probability fits existing parameter storage");
std::string probabilityKey(int step,int field);
int probabilityPitch(int row,int scale,int root,int octave,int transpose=0) noexcept;
constexpr int orbitGlobals=3,orbitStride=15;
enum OrbitGlobal { OrEnabled,OrSeed,OrFill };
enum OrbitField { OrLength,OrHits,OrRotation,OrDivision,OrPitch,OrVelocity,OrGate,OrChance,OrRatchet,OrSwing,OrMute,OrSolo,OrAccentEvery,OrAccentAmount,OrDrift };
constexpr int orbitIndex(int ring,int field){return orbitGlobals+ring*orbitStride+field;}
std::string orbitKey(int ring,int field);
struct ReferenceSequenceState {
    std::array<std::atomic<int>,32> outcomes;
    std::array<std::atomic<int>,4> heads,rotations,passes;
    std::array<std::atomic<bool>,4> fired;
    std::atomic<uint64_t> reset{0};
    ReferenceSequenceState(){for(auto& v:outcomes)v=-2;for(auto& v:heads)v=-1;for(auto& v:rotations)v=0;for(auto& v:passes)v=0;for(auto& v:fired)v=false;}
};
std::vector<Panel> referenceSequencerPanels();
void referenceSequencerPorts(const std::string&,ModuleType&);
std::unique_ptr<AudioModule> makeReferenceSequencer(const NodeSpec&);
}
