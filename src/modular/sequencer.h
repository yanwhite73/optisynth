#pragma once
#include "runtime.h"
#include <algorithm>

namespace optisynth {
constexpr int sequenceCommon=5,patternSteps=16,stageSteps=8;
inline bool isSequencer(const std::string& id){return id=="seq.pattern"||id=="seq.euclidean"||id=="seq.stage";}
inline double sequenceDivision(int index) noexcept {
    constexpr double beats[]={.125,.25,.5,1,2,4};
    return beats[std::clamp(index,0,5)];
}
inline bool euclideanHit(int step,int length,int hits,int rotation) noexcept {
    length=std::clamp(length,1,32);hits=std::clamp(hits,0,length);
    const int position=((step-rotation)%length+length)%length;
    return (position*hits)%length<hits;
}
std::vector<Panel> sequencerPanels();
std::unique_ptr<AudioModule> makeSequencer(const NodeSpec&);
GraphSpec sequencerPatch(int model);
}
