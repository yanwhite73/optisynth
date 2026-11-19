#pragma once
#include "runtime.h"
#include <cmath>

namespace optisynth {
// Stable named parameters keep the original sixteen-step rack documents valid.
constexpr int patternCapacity=64, patternStride=10, patternGlobals=23;
enum PatternGlobal { PatEnabled,PatDivision,PatLength,PatGate,PatVelocity,
    PatFirst,PatRun,PatRotate,PatTranspose,PatScale,PatRoot,PatSwing,PatSeed,
    PatPitchLength,PatVelocityLength,PatCV1Length,PatCV2Length,PatFill,
    PatKeyboard,PatSlew,PatDensity,PatGenerateRange,PatGenerateRoot };
enum PatternField { Pitch,Velocity,On,Gate,Chance,Ratchet,Condition,CV1,CV2,Locked };
inline constexpr int patternIndex(int step,int field){return patternGlobals+step*patternStride+field;}
inline std::string patternKey(int step,int field){
    constexpr const char* fields[]={"pitch","velocity","on","gate","chance","ratchet","condition","cv1","cv2","lock"};
    return "step"+std::to_string(step+1)+"_"+fields[field];
}
inline int patternQuantize(int note,int scale,int root) noexcept {
    constexpr unsigned masks[]={0xfff,0xab5,0x5ad,0x6ad,0x295,0x4a9,0x6b5};
    const auto mask=masks[std::clamp(scale,0,6)];note=std::clamp(note,0,127);
    for(int d=0;d<12;++d)for(int direction:{-1,1}){
        const int n=note+d*direction,pc=((n-root)%12+12)%12;
        if(n>=0&&n<=127&&(mask&(1u<<pc)))return n;
    }
    return note;
}
Panel patternPanel();
std::unique_ptr<AudioModule> makePattern(const NodeSpec&);
} // namespace optisynth
