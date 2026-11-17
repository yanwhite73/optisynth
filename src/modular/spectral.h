#pragma once
#include "../core/engine.h"
#include <map>

namespace optisynth {
// Immutable after publication. Tables are reused only by Engine::load's exact
// compatibility check. No access to live voice state from the loader/UI.
struct SpectralPreset {
    partials::Patch patch;
    partials::TableCache tables;
};
using SpectralPtr=std::shared_ptr<const SpectralPreset>;
using SpectralBank=std::map<std::string,SpectralPtr>;
constexpr int maximumSpectralEngines=2;
SpectralPtr spectralPreset(partials::Patch patch,const partials::TableCache& cache={});
partials::Patch spectralDemoPatch();
// Conservative saved-patch routing eligibility, not an audibility proof.
bool spectralParameterActive(const partials::Patch&,int index);
class PreparedModule;
struct NodeSpec;
std::unique_ptr<PreparedModule> makeSpectral(const NodeSpec&,SpectralPtr);
}
