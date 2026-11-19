#pragma once
#include "../src/modular/reference_sequencers.h"
namespace {
using ReferenceTrace=std::array<std::vector<RecordedNote>,5>;
ReferenceTrace referenceTrace(const NodeSpec& node,int frames,int chunk,const std::function<void(int,SampleClock&,AudioModule&)>& action={}){
    auto module=makeReferenceSequencer(node);SampleClock clock;clock.play(true);ReferenceTrace result;
    std::array<ClockFrame,24> time;std::array<NoteEventBlock,5> buses;std::array<NoteEventBlock*,13> events{};std::array<std::array<float,24>,13> output{};std::array<float*,13> pointers{};
    for(int i=0;i<13;++i)pointers[i]=output[i].data();for(int t=0;t<5;++t)events[t*3]=&buses[t];
    for(int offset=0;offset<frames;){const int n=std::min({chunk,24,frames-offset});if(action)action(offset,clock,*module);for(int i=0;i<n;++i)time[i]=clock.next();for(auto& bus:buses)bus.begin(n);
        module->process({n,{},nullptr,pointers.data(),nullptr,nullptr,nullptr,time.data(),&buses[0],events.data()});
        for(int t=0;t<5;++t){if(buses[t].failed())throw std::runtime_error("Reference sequencer event overflow");for(const auto& e:buses[t])result[t].push_back({uint64_t(offset)+e.offset,e.kind,e.pitch});}offset+=n;
    }return result;
}
bool sameReferenceTrace(const ReferenceTrace& a,const ReferenceTrace& b){for(int t=0;t<5;++t){if(a[t].size()!=b[t].size())return false;for(size_t i=0;i<a[t].size();++i)if(a[t][i].sample!=b[t][i].sample||a[t][i].kind!=b[t][i].kind||a[t][i].pitch!=b[t][i].pitch)return false;}return true;}
NodeSpec plainProbability(){NodeSpec node{"sequence","seq.probability",{{"swing",0},{"scale",0},{"octave",4},{"length",4}}};for(int s=0;s<32;++s){for(int r=0;r<16;++r)node.parameters.push_back({probabilityKey(s,r),r==s%4?100.f:0.f});node.parameters.push_back({probabilityKey(s,PrStepChance),1});}return node;}
void referenceSequencerChecks(){
    check(panel("seq.probability").parameters.size()==size_t(probabilityCount)&&probabilityCount<=maxParameters,"Probability uses bounded existing parameter storage");
    auto node=plainProbability();auto notes=referenceTrace(node,24000,17);check(attacks(notes[0])==std::vector<uint64_t>({0,6000,12000,18000}),"Probability grid emits exact clock positions");
    std::vector<int> pitches;for(const auto& e:notes[0])if(e.kind==NoteEventKind::On)pitches.push_back(e.pitch);check(pitches==std::vector<int>({48,49,50,51}),"Painted pitch rows determine actual notes");
    node.parameters.push_back({probabilityKey(1,1),0});notes=referenceTrace(node,24000,24);check(attacks(notes[0])==std::vector<uint64_t>({0,12000,18000}),"Empty probability column is a rest");
    node=plainProbability();node.parameters.push_back({"row1_mute",1});notes=referenceTrace(node,24000,24);check(attacks(notes[0])==std::vector<uint64_t>({6000,12000,18000}),"Muted pitch rows cannot be selected");
    node=plainProbability();node.parameters.push_back({probabilityKey(0,PrStepRatchet),3});node.parameters.push_back({"swing",.25f});notes=referenceTrace(node,12000,17);check(attacks(notes[0])==std::vector<uint64_t>({0,2000,4000,7500}),"Probability ratchets and swing are sample timed");
    node=plainProbability();node.parameters.push_back({"length",1});node.parameters.push_back({probabilityKey(0,4),25});node.parameters.push_back({"division",0});notes=referenceTrace(node,2400000,24);int low=0,high=0;for(const auto& e:notes[0])if(e.kind==NoteEventKind::On){low+=e.pitch==48;high+=e.pitch==52;}check(low+high==800&&low>570&&low<700,"Relative weights choose only candidates at the expected proportion");
    node.parameters.push_back({"repeat",1});notes=referenceTrace(node,48000,24);bool constant=true;for(const auto& e:notes[0])if(e.kind==NoteEventKind::On)constant&=e.pitch==notes[0].front().pitch;check(constant,"Repeat choices reuses the same weighted decision");
    node=plainProbability();node.parameters.push_back({probabilityKey(0,PrStepCondition),1});notes=referenceTrace(node,12000,24);check(attacks(notes[0])==std::vector<uint64_t>({6000}),"Fill-only cell is silent outside fill");notes=referenceTrace(node,12000,24,[](int,SampleClock&,AudioModule& m){m.controls->values[PrFill]=1;});check(attacks(notes[0])==std::vector<uint64_t>({0,6000}),"Fill changes actual conditional attacks");
    node={"sequence","seq.probability",{}};check(sameReferenceTrace(referenceTrace(node,45000,1),referenceTrace(node,45000,24)),"Probability random decisions and notes do not depend on callback size");
    notes=referenceTrace(node,2000,1,[](int n,SampleClock& c,AudioModule&){if(n==1000)c.play(false);});check(notes[0].size()==2&&notes[0].back().kind==NoteEventKind::Off&&notes[0].back().sample==1000,"Probability transport stop releases held note");
    node={"sequence","seq.orbit",{}};for(int r=0;r<4;++r){node.parameters.push_back({orbitKey(r,OrLength),4});node.parameters.push_back({orbitKey(r,OrHits),4});node.parameters.push_back({orbitKey(r,OrDivision),float(r)});}
    notes=referenceTrace(node,48000,17);for(int r=0;r<4;++r)check(attacks(notes[r]).size()==size_t(16>>r),"Orbit rings have independent clock rates and outputs");check(attacks(notes[4]).size()==30,"Orbit combined bus contains all four ring events");
    notes=referenceTrace(node,2000,1,[](int n,SampleClock& c,AudioModule&){if(n==1000)c.play(false);});for(int r=0;r<4;++r)check(notes[r].size()==2&&notes[r].back().sample==1000,"Orbit stop releases each ring note");
    notes=referenceTrace(node,15000,1,[](int n,SampleClock&,AudioModule& m){if(n==1000)m.controls->values[orbitIndex(1,OrSolo)]=1;});check(attacks(notes[0])==std::vector<uint64_t>({0})&&attacks(notes[1])==std::vector<uint64_t>({0,6000,12000}),"Orbit solo preserves other ring phase while silencing attacks");
    node={"sequence","seq.orbit",{{"ring1_length",4},{"ring1_hits",1},{"ring1_rotation",0},{"ring1_drift",1}}};notes=referenceTrace(node,48000,24);check(attacks(notes[0])==std::vector<uint64_t>({0,30000}),"Orbit per-cycle rotation changes the realised Euclidean rhythm");
    node.parameters.push_back({"ring1_chance",.63f});node.parameters.push_back({"ring3_ratchet",3});check(sameReferenceTrace(referenceTrace(node,48000,1),referenceTrace(node,48000,24)),"Orbit chance, ratchets and drifting rings are callback-size independent");
    for(const auto* type:{"seq.probability","seq.orbit"}){auto g=instrumentPatch("osc.sine","spectral.adsr","filter.bypass",true,false);g.nodes.push_back({"sequence",type,{}});const std::string port=std::string(type)=="seq.orbit"?"r4_notes":"notes";g.connections.push_back({{"sequence",port},{"source","notes"}});g.connections.push_back({{"sequence",port},{"envelope","notes"}});Runtime runtime(g);runtime.clock.play(true);std::array<float,12000> sound{};allocations=0;watchAllocations=true;runtime.render(sound.data(),sound.size(),{});watchAllocations=false;check(allocations==0&&std::any_of(sound.begin(),sound.end(),[](float v){return std::abs(v)>.001f;}),"Reference sequencer reaches its destination without callback allocation");}
}
}
