#pragma once
#include "../src/modular/sequencer.h"

namespace {
struct RecordedNote {uint64_t sample;NoteEventKind kind;int pitch;};
std::vector<RecordedNote> sequenceEvents(NodeSpec spec,int frames,int chunk,const std::function<void(int,SampleClock&,AudioModule&)>& action={}){
    auto module=makeSequencer(spec);SampleClock clock;clock.play(true);
    std::vector<RecordedNote> result;std::array<ClockFrame,quantum> time;NoteEventBlock events;
    for(int offset=0;offset<frames;){const int n=std::min({chunk,quantum,frames-offset});
        if(action)action(offset,clock,*module);
        for(int i=0;i<n;++i)time[i]=clock.next();events.begin(n);
        module->process({n,{},nullptr,nullptr,nullptr,nullptr,nullptr,time.data(),&events});
        if(events.failed())throw std::runtime_error("Sequencer event overflow");
        for(const auto& e:events)result.push_back({uint64_t(offset)+e.offset,e.kind,e.pitch});offset+=n;
    }return result;
}
std::vector<uint64_t> attacks(const std::vector<RecordedNote>& notes){std::vector<uint64_t> out;for(auto& n:notes)if(n.kind==NoteEventKind::On)out.push_back(n.sample);return out;}
void sequencerChecks(){
    NodeSpec pattern{"seq","seq.pattern",{{"length",4},{"gate",.5f},{"step1_pitch",60},{"step2_pitch",64},{"step3_on",0},{"step4_pitch",67}}};
    auto reference=sequenceEvents(pattern,30000,1);
    check(attacks(reference)==std::vector<uint64_t>({0,6000,18000,24000}),"Pattern rests and exact 16th-note offsets");
    check(reference[1].sample==3000&&reference[1].kind==NoteEventKind::Off&&reference[2].pitch==64,"Gate release and stored pitch");
    for(int chunk:{7,17,24,64,257,1024}){auto other=sequenceEvents(pattern,30000,chunk);bool equal=reference.size()==other.size();
        for(size_t i=0;equal&&i<reference.size();++i)equal=reference[i].sample==other[i].sample&&reference[i].kind==other[i].kind&&reference[i].pitch==other[i].pitch;
        check(equal,"Sequencer events independent of callback partition");}
    auto muted=sequenceEvents(pattern,10000,1,[](int n,SampleClock&,AudioModule& m){if(n==1000)m.controls->values[0]=0;});
    check(muted.size()==2&&muted[1].sample==1000&&muted[1].kind==NoteEventKind::Off,"Mute releases current note immediately");
    auto stopped=sequenceEvents(pattern,10000,1,[](int n,SampleClock& c,AudioModule&){if(n==500)c.play(false);});
    check(stopped.size()==2&&stopped[1].sample==500,"Stop releases current note without another tick");
    auto restarted=sequenceEvents(pattern,15000,1,[](int n,SampleClock& c,AudioModule&){if(n==7000)c.seek(0);});
    check(attacks(restarted)==std::vector<uint64_t>({0,6000,7000,13000}),"Restart resets pattern and retriggers at exact sample");
    check(restarted[3].kind==NoteEventKind::Off&&restarted[3].sample==7000,"Restart releases before replacement attack");
    auto tempo=sequenceEvents(pattern,18000,1,[](int n,SampleClock& c,AudioModule&){if(n==3000)c.tempo(60);});
    check(attacks(tempo)==std::vector<uint64_t>({0,9000}),"Tempo change preserves beat phase");
    NodeSpec euclid{"seq","seq.euclidean",{{"length",8},{"hits",3},{"rotation",0}}};
    check(attacks(sequenceEvents(euclid,48000,17))==std::vector<uint64_t>({0,18000,36000}),"Euclidean 3/8 spacing");
    euclid.parameters.back().second=1;
    check(attacks(sequenceEvents(euclid,48000,17))==std::vector<uint64_t>({6000,24000,42000}),"Euclidean rotation shifts generated hits");
    euclid.parameters[1].second=0;check(sequenceEvents(euclid,48000,24).empty(),"Zero hits is silent");
    euclid.parameters[1].second=32;check(attacks(sequenceEvents(euclid,48000,24)).size()==8,"Hits capped by length");
    NodeSpec stages{"seq","seq.stage",{{"length",3},{"gate",.75f},{"step1_pulses",2},{"step1_mode",1},
        {"step2_pulses",1},{"step2_mode",2},{"step3_pulses",3},{"step3_mode",0}}};
    const auto stageEvents=sequenceEvents(stages,42000,17);
    check(attacks(stageEvents)==std::vector<uint64_t>({0,18000,24000,30000,36000}),"Stage Hold/Rest/Repeat duration and cycle");
    check(stageEvents[1].sample==10500,"Hold spans stage pulses without repeated attacks");
    for(int model=0;model<3;++model){auto graph=sequencerPatch(model);validateRack(graph);Runtime runtime(graph);
        std::vector<float> audio(48000*4);runtime.clock.play(true);
        allocations=0;watchAllocations=true;runtime.render(audio.data(),int(audio.size()),{});watchAllocations=false;
        check(allocations==0&&energy(audio)>1,"Sequencer starter audible with no render allocation");
        wav("build/sequencer-"+std::to_string(model)+".wav",audio);
        check(runtime.controls("sequence")->playhead.load()>=0,"Audio publishes a live playhead");
        runtime.clock.play(false);runtime.render(audio.data(),int(audio.size()),{});
        check(runtime.controls("sequence")->playhead.load()==-1,"Stopped playhead is explicit");
        if(model!=1)check(std::abs(audio.back())<1e-8,"Stop releases routed envelope and closes VCA");
        for(int extreme=0;extreme<2;++extreme){const auto& ps=panel(graph.nodes.front().type).parameters;auto c=runtime.controls("sequence");
            for(size_t i=0;i<ps.size();++i)c->values[i]=extreme?ps[i].maximum:ps[i].minimum;
            runtime.clock.seek(0);runtime.clock.tempo(300);runtime.clock.play(true);
            allocations=0;watchAllocations=true;runtime.render(audio.data(),48000,{});watchAllocations=false;
            check(allocations==0&&std::all_of(audio.begin(),audio.end(),[](float x){return std::isfinite(x);}),"Sequencer extremes stay finite and allocation free");}
    }
    auto g=sequencerPatch(0);g.nodes.front().parameters={{"step1_on",0}};
    Runtime rest(g);rest.clock.play(true);std::vector<float> x(6000);rest.render(x.data(),6000,{80,1,true,true});
    check(energy(x)==0,"Connected note input excludes shared keyboard, rest closes both branches");
    rest.render(x.data(),6000,{});check(energy(x)>1,"Note event fan-out drives oscillator and envelope");
    // 123 BPM deliberately puts both edges inside a 24-sample quantum.
    auto precise=instrumentPatch("osc.sine","spectral.adsr","filter.bypass",true,false);
    precise.nodes.push_back({"sequence","seq.pattern",{{"step1_on",0},{"gate",.5f}}});
    precise.nodes[1].parameters={{"attack",0},{"decay",0},{"sustain",1},{"release",0}};
    precise.connections.push_back({{"sequence","notes"},{"source","notes"}});
    precise.connections.push_back({{"sequence","notes"},{"envelope","notes"}});
    for(int chunk:{17,257}){Runtime r(precise);r.clock.tempo(123);r.clock.play(true);std::vector<float> audio(10000);
        for(int offset=0;offset<int(audio.size());offset+=chunk)r.render(audio.data()+offset,std::min(chunk,int(audio.size())-offset),{});
        check(std::all_of(audio.begin(),audio.begin()+5854,[](float x){return x==0;})&&audio[5854]!=0,"Runtime note starts at exact sub-quantum offset");
        check(audio[8780]!=0&&std::all_of(audio.begin()+8781,audio.end(),[](float x){return x==0;}),"Runtime gate ends at exact sub-quantum offset");
    }
    // Two independent note sources route to separate voices; muting one must not release the other.
    auto two=sequencerPatch(0);two.nodes.push_back({"rhythm","seq.euclidean",{{"hits",16}}});
    two.nodes.push_back({"drum","drum.analog",{}});two.nodes.push_back({"mix","utility.stereo",{}});
    two.connections.push_back({{"rhythm","notes"},{"drum","notes"}});
    two.connections.push_back({{"vca","out"},{"mix","left"}});two.connections.push_back({{"drum","out"},{"mix","right"}});two.outputs={{"mix","out"}};
    Runtime independent(two);independent.clock.play(true);std::vector<float> left(48000),right(48000);
    independent.controls("sequence")->values[0]=0;independent.render(left.data(),right.data(),48000,{});
    check(energy(left)==0&&energy(right)>1,"Independent routing and mute ownership");
    Player player;player.publish(std::make_unique<Runtime>(sequencerPatch(0)));player.playing=true;
    player.render(left.data(),48000);check(energy(left)>1,"Player drives shared sequencer clock");
    player.playing=false;player.render(left.data(),48000);check(std::abs(left.back())<1e-8,"Player stop flushes notes");
    player.playing=true;player.restart();player.render(left.data(),1000);
    auto removed=instrumentPatch("osc.variable","spectral.adsr","filter.bypass",true,false);
    player.publish(std::make_unique<Runtime>(removed));allocations=0;watchAllocations=true;player.render(left.data(),48000);watchAllocations=false;
    check(allocations==0&&std::abs(left.back())<1e-8,"Removing sequencer retires note owners without allocation or stuck note");player.collect();
    auto dormant=removed;dormant.nodes.push_back({"unpatched","seq.stage",{}});Runtime unused(dormant);
    check(!unused.controls("unpatched"),"Unrouted sequencer is not constructed");
}
}
