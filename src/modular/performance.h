#pragma once
#include "runtime.h"
#include <tuple>

namespace optisynth {
constexpr int performanceTracks=4, performancePatterns=8, performanceSteps=64, performanceRows=16;
constexpr int performanceAllNotes=performanceTracks*4;
struct PerformanceStep {
    int pitch=48, ratchets=1, condition=0;
    float velocity=.8f, gate=.7f, chance=1, cv=0, slide=0;
    bool on=true, tie=false, protect=false;
    bool operator==(const PerformanceStep& s)const{return std::tie(pitch,ratchets,condition,velocity,gate,chance,cv,slide,on,tie,protect)==std::tie(s.pitch,s.ratchets,s.condition,s.velocity,s.gate,s.chance,s.cv,s.slide,s.on,s.tie,s.protect);}
};
struct PerformancePattern {
    int first=0, last=15, division=1, traversal=0, rotation=0, scale=2, root=0;
    float swing=0;
    std::array<PerformanceStep,performanceSteps> steps{};
    bool operator==(const PerformancePattern& p)const{return std::tie(first,last,division,traversal,rotation,scale,root,swing,steps)==std::tie(p.first,p.last,p.division,p.traversal,p.rotation,p.scale,p.root,p.swing,p.steps);}
};
struct PerformanceTrack {
    std::string name;
    int initial=0;
    std::array<PerformancePattern,performancePatterns> patterns{};
    bool operator==(const PerformanceTrack& t)const{return name==t.name&&initial==t.initial&&patterns==t.patterns;}
};
struct PerformanceRow { std::array<int,performanceTracks> patterns{}; int bars=2; bool operator==(const PerformanceRow& r)const{return patterns==r.patterns&&bars==r.bars;} };
struct PerformanceData {
    std::array<PerformanceTrack,performanceTracks> tracks;
    std::array<PerformanceRow,performanceRows> chain;
    int chainFirst=0, chainLast=3;
    uint32_t seed=713;
    bool operator==(const PerformanceData& d)const{return tracks==d.tracks&&chain==d.chain&&chainFirst==d.chainFirst&&chainLast==d.chainLast&&seed==d.seed;}
};
using PerformancePtr=std::shared_ptr<const PerformanceData>;
PerformanceData defaultPerformance();
void validatePerformance(const PerformanceData&);

// Single message-thread producer / audio consumer. Immutable payload deletion
// happens on the producer or at final off-thread graph destruction, never render.
class PerformanceExchange {
public:
    explicit PerformanceExchange(const PerformanceData&);
    ~PerformanceExchange();
    void publish(const PerformanceData&);
    void collect() noexcept; // message/loader thread; progress retirement while idle
    const PerformanceData& consume() noexcept;
    void launch(int track,int pattern,bool immediate=false) noexcept;
    void cancel(int track) noexcept;
    void reset(int track) noexcept;
    std::array<std::atomic<uint64_t>,performanceTracks> requests{},resets{};
    std::array<std::atomic<int>,performanceTracks> playing{},queued{},steps{},passes{};
    std::array<std::atomic<bool>,performanceTracks> fired{};
    std::atomic<int> chainRow{-1};
    std::atomic<double> chainRemaining{0};
private:
    PerformanceData* current_;
    std::atomic<PerformanceData*> pending_{nullptr},retired_{nullptr};
};
enum PerformanceGlobal { PerfEnabled,PerfTranspose,PerfFill,PerfChain,PerfLaunch,PerfGlobalCount };
enum PerformanceTrackControl { PerfMute,PerfSolo,PerfTrackTranspose,PerfVelocity,PerfTrackControlCount };
constexpr int performanceIndex(int track,int field){return PerfGlobalCount+track*PerfTrackControlCount+field;}
Panel performancePanel();
std::unique_ptr<AudioModule> makePerformance(const NodeSpec&);
}
