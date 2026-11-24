#pragma once
#include "../src/modular/performance.h"
namespace {
struct PerformanceTrace {
    std::array<std::vector<RecordedNote>,4> notes;
    std::vector<float> bend,gate;
};
PerformanceData plainPerformance(){
    auto d=defaultPerformance();d.chainFirst=0;d.chainLast=1;
    for(int t=0;t<4;++t)for(int bank=0;bank<8;++bank){auto& p=d.tracks[t].patterns[bank];p.first=0;p.last=3;p.division=1;p.traversal=0;p.rotation=0;p.scale=0;p.swing=0;
        for(auto& step:p.steps){step=PerformanceStep{};step.pitch=36+t*12+bank;step.gate=.5f;}}
    for(int r=0;r<16;++r){d.chain[r].bars=1;d.chain[r].patterns.fill(r%2);}return d;
}
PerformanceTrace performanceTrace(const PerformanceData& data,int frames,int chunk,const std::function<void(int,SampleClock&,AudioModule&)>& action={}){
    auto module=makePerformance({"performance","seq.performance",{},std::make_shared<const PerformanceData>(data)});
    SampleClock clock;clock.play(true);PerformanceTrace result;
    std::array<ClockFrame,24> time;std::array<NoteEventBlock,5> buses;std::array<NoteEventBlock*,performanceAllNotes+1> events{};
    std::array<std::array<float,24>,16> output{};std::array<float*,16> pointers{};
    for(int i=0;i<16;++i)pointers[i]=output[i].data();for(int t=0;t<4;++t)events[t*4]=&buses[t];
    events[performanceAllNotes]=&buses.back();
    result.bend.reserve(frames);result.gate.reserve(frames);
    for(int offset=0;offset<frames;){const int n=std::min({chunk,24,frames-offset});if(action)action(offset,clock,*module);
        module->controls->performance->collect();for(int i=0;i<n;++i)time[i]=clock.next();for(auto& bus:buses)bus.begin(n);
        module->process({n,{},nullptr,pointers.data(),nullptr,nullptr,nullptr,time.data(),&buses[0],events.data()});
        for(int t=0;t<4;++t){if(buses[t].failed())throw std::runtime_error("Performance bus overflow");for(const auto& e:buses[t])result.notes[t].push_back({uint64_t(offset)+e.offset,e.kind,e.pitch});}
        size_t expected=0;
        for(int t=0;t<performanceTracks;++t)expected+=buses[t].size();
        const auto& all=buses.back();
        if(all.failed() || all.size()!=expected)throw std::runtime_error("Combined track output lost notes");
        for(const auto& e:all){
            if(e.source<1 || e.source>performanceTracks)throw std::runtime_error("Unknown track source");
            const auto& lane=buses[e.source-1];
            if(std::none_of(lane.begin(),lane.end(),[&](const NoteEvent& n){
                return e.offset==n.offset && e.noteId==n.noteId && e.kind==n.kind && e.pitch==n.pitch && e.velocity==n.velocity;
            }))throw std::runtime_error("Combined output changed a track event");
        }
        result.bend.insert(result.bend.end(),output[2].begin(),output[2].begin()+n);result.gate.insert(result.gate.end(),output[1].begin(),output[1].begin()+n);offset+=n;
    }return result;
}
void performanceChecks(){
    auto data=plainPerformance();
    for(int t=0;t<4;++t)data.tracks[t].patterns[0].division=t;
    const auto independent=performanceTrace(data,48000,17);
    for(int t=0;t<4;++t){const auto a=attacks(independent.notes[t]);check(a.size()==size_t(16>>t)&&a[1]==uint64_t(3000<<t),"Four tracks use independent divisions and event outputs");check(independent.notes[t][0].pitch==36+t*12,"Four output buses retain distinct pitches");}
    data=plainPerformance();
    auto changed=performanceTrace(data,48001,1,[&](int n,SampleClock&,AudioModule& module){
        if(n==1000){auto d=data;d.tracks[0].patterns[1].steps[0].pitch=81;module.controls->performance->publish(d);module.controls->values[PerfLaunch]=1;module.controls->performance->launch(0,1);}
    });
    bool atBoundary=false,untouched=true;for(const auto& e:changed.notes[0])if(e.kind==NoteEventKind::On){if(e.sample<24000)untouched&=e.pitch==36;if(e.sample==24000)atBoundary=e.pitch==81;}
    check(untouched&&atBoundary,"Editing inactive bank leaves A unchanged; queued B starts on exact beat");
    auto resumed=performanceTrace(data,40000,1,[](int n,SampleClock& clock,AudioModule& m){if(n==1000)m.controls->performance->launch(0,1,true);if(n==10000)clock.play(false);if(n==12000)clock.play(true);});
    bool keptBank=false;for(const auto& e:resumed.notes[0])if(e.sample==12000&&e.kind==NoteEventKind::On)keptBank=e.pitch==37;
    check(keptBank,"Stop/start resets phase but retains the launched pattern");
    auto cancelled=performanceTrace(data,48001,1,[](int n,SampleClock&,AudioModule& m){if(n==1000){m.controls->values[PerfLaunch]=1;m.controls->performance->launch(0,1);}if(n==15000)m.controls->performance->cancel(0);});
    check(std::all_of(cancelled.notes[0].begin(),cancelled.notes[0].end(),[](const RecordedNote& e){return e.pitch==36;}),"Queued launch cancellation preserves playing bank");
    auto chain=performanceTrace(data,192001,17,[](int,SampleClock&,AudioModule& m){m.controls->values[PerfChain]=1;});
    for(int t=0;t<4;++t){bool second=false,loop=false;for(const auto& e:chain.notes[t])if(e.kind==NoteEventKind::On){if(e.sample==96000)second=e.pitch==37+12*t;if(e.sample==192000)loop=e.pitch==36+12*t;}check(second&&loop,"Chain changes all four patterns on bar boundary and loops");}
    auto reset=performanceTrace(data,15000,1,[](int n,SampleClock&,AudioModule& m){if(n==7000)m.controls->performance->reset(0);});
    check(attacks(reset.notes[0])==std::vector<uint64_t>({0,6000,7000,13000})&&attacks(reset.notes[1])==std::vector<uint64_t>({0,6000,12000}),"Track reset changes only that track's phase");
    auto stopped=performanceTrace(data,2000,1,[](int n,SampleClock& c,AudioModule&){if(n==1000)c.play(false);});
    for(int t=0;t<4;++t)check(stopped.notes[t].size()==2&&stopped.notes[t].back().sample==1000,"Stop releases each track's owned note immediately");
    auto solo=performanceTrace(data,15000,1,[](int n,SampleClock&,AudioModule& m){if(n==1000)m.controls->values[performanceIndex(1,PerfSolo)]=1;});
    check(attacks(solo.notes[0])==std::vector<uint64_t>({0})&&attacks(solo.notes[1])==std::vector<uint64_t>({0,6000,12000}),"Solo silences other buses without changing chosen track timing");
    data.tracks[0].patterns[0].steps[0].tie=true;data.tracks[0].patterns[0].steps[1].pitch=48;data.tracks[0].patterns[0].steps[1].slide=.1f;
    const auto tied=performanceTrace(data,12000,1);
    check(attacks(tied.notes[0])==std::vector<uint64_t>({0})&&tied.notes[0].back().sample==9000,"Tie sustains through next pitch without envelope retrigger");
    check(tied.gate[6000]==1&&tied.gate[8999]==1&&tied.gate[9000]==0,"Tied gate remains high until final duration ends");
    check(std::abs(tied.bend[8400]-.125f)<.001f&&std::abs(tied.bend[10800]-.25f)<.001f,"Slide uses its full declared duration and note-relative bend");
    data=plainPerformance();auto& p=data.tracks[0].patterns[0];p.swing=.25f;p.steps[0].ratchets=3;p.steps[1].ratchets=2;
    auto ratchets=performanceTrace(data,12000,17);check(attacks(ratchets.notes[0])==std::vector<uint64_t>({0,2000,4000,7500,9750}),"Performance swing and ratchets preserve exact timing");
    p.steps[0].condition=1;p.steps[1].condition=2;
    auto fill=performanceTrace(data,12000,1,[](int,SampleClock&,AudioModule& m){m.controls->values[PerfFill]=1;});
    check(attacks(fill.notes[0])==std::vector<uint64_t>({0,2000,4000}),"Live fill controls actual conditional events");
    data=defaultPerformance();data.tracks[0].patterns[0].traversal=5;
    const auto one=performanceTrace(data,100000,1),blocks=performanceTrace(data,100000,24);bool same=true;
    for(int t=0;t<4;++t){same&=one.notes[t].size()==blocks.notes[t].size();for(size_t i=0;same&&i<one.notes[t].size();++i)same&=one.notes[t][i].sample==blocks.notes[t][i].sample&&one.notes[t][i].kind==blocks.notes[t][i].kind&&one.notes[t][i].pitch==blocks.notes[t][i].pitch;}
    check(same&&one.bend==blocks.bend,"Random traversal, events and slides are block-size independent");
    auto g=instrumentPatch("osc.sine","spectral.adsr","filter.bypass",true,false);
    g.nodes.push_back({"performance","seq.performance",{},std::make_shared<const PerformanceData>(plainPerformance())});
    g.connections.push_back({{"performance","t4_notes"},{"source","notes"}});g.connections.push_back({{"performance","t4_notes"},{"envelope","notes"}});
    Runtime runtime(g);runtime.clock.play(true);std::array<float,12000> sound{};allocations=0;watchAllocations=true;runtime.render(sound.data(),sound.size(),{});watchAllocations=false;
    check(allocations==0&&std::any_of(sound.begin(),sound.end(),[](float v){return std::abs(v)>.001f;}),"Fourth event output reaches oscillator/envelope without render allocation");
    auto exchange=runtime.controls("performance")->performance;auto next=plainPerformance();next.tracks[3].patterns[0].steps[0].pitch=83;exchange->publish(next);
    allocations=0;watchAllocations=true;runtime.render(sound.data(),sound.size(),{});watchAllocations=false;check(allocations==0,"Immutable bank handoff allocates nothing in callback");exchange->collect();
    auto invalid=plainPerformance();invalid.tracks[1].patterns[7].steps[63].slide=std::numeric_limits<float>::quiet_NaN();bool rejected=false;try{validatePerformance(invalid);}catch(const std::exception&){rejected=true;}check(rejected,"Dormant banks reject invalid values");
}
}
