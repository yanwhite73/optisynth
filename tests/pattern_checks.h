#pragma once
#include "../src/modular/pattern.h"
namespace {
void patternChecks(){
    auto notes=[](const std::vector<RecordedNote>& events){std::vector<int> result;for(const auto& e:events)if(e.kind==NoteEventKind::On)result.push_back(e.pitch);return result;};
    NodeSpec spec{"seq","seq.pattern",{{"length",3},{"step1_pitch",60},{"step2_pitch",62},{"step3_pitch",64}}};
    const std::vector<std::vector<int>> expected={{60,62,64,60,62,64},{64,62,60,64,62,60},{60,62,64,64,62,60},{60,62,64,62,60,62}};
    for(int mode=0;mode<4;++mode){auto n=spec;n.parameters.push_back({"run_mode",float(mode)});check(notes(sequenceEvents(n,36000,17))==expected[mode],"Forward/backward/pendulum/ping-pong traversal");}
    auto loop=spec;loop.parameters.insert(loop.parameters.end(),{{"first_step",2},{"rotate",1}});
    check(notes(sequenceEvents(loop,24000,24))==std::vector<int>({64,62,64,62}),"Loop range with non-destructive rotation");
    loop.parameters={{"length",64},{"first_step",63},{"step63_pitch",81},{"step64_pitch",84}};
    check(notes(sequenceEvents(loop,18000,17))==std::vector<int>({81,84,81}),"All 64 steps playable");
    auto poly=spec;poly.parameters.push_back({"pitch_length",2});
    check(notes(sequenceEvents(poly,36000,17))==std::vector<int>({60,62,60,62,60,62}),"Pitch lane cycles independently of gates");
    NodeSpec ratchet{"seq","seq.pattern",{{"length",2},{"step1_ratchet",3},{"step2_ratchet",2},{"swing",.25f},{"gate",.5f}}};
    const auto r=sequenceEvents(ratchet,12000,17);
    check(attacks(r)==std::vector<uint64_t>({0,2000,4000,7500,9750}),"Swing and ratchets at exact sub-step samples");
    check(r[1].sample==1000&&r[7].sample==8625,"Ratchet release fits its swung subdivision");
    auto cancel=sequenceEvents(ratchet,12000,1,[](int frame,SampleClock& c,AudioModule&){if(frame==2200)c.play(false);});
    check(attacks(cancel)==std::vector<uint64_t>({0,2000})&&cancel.back().sample==2200,"Stop cancels every pending ratchet");
    NodeSpec condition{"seq","seq.pattern",{{"length",2},{"step1_condition",7},{"step2_condition",3}}};
    check(attacks(sequenceEvents(condition,48000,24))==std::vector<uint64_t>({0,6000,24000,30000}),"Previous follows the previous condition and 1:2 loop test");
    condition.parameters={{"length",2},{"step1_condition",1},{"step2_condition",2}};
    check(attacks(sequenceEvents(condition,24000,24))==std::vector<uint64_t>({6000,18000}),"Fill and not-fill conditions");
    condition.parameters.push_back({"fill",1});check(attacks(sequenceEvents(condition,24000,24))==std::vector<uint64_t>({0,12000}),"Performance fill switches conditional gates");
    condition.parameters={{"length",2},{"step1_condition",5},{"step2_condition",6}};
    check(attacks(sequenceEvents(condition,24000,24))==std::vector<uint64_t>({0,18000}),"First and not-first pass conditions");
    condition.parameters={{"length",2},{"step1_chance",0},{"step2_chance",1}};
    check(attacks(sequenceEvents(condition,24000,24))==std::vector<uint64_t>({6000,18000}),"Chance endpoints are exact");
    for(int mode:{4,5}){auto random=spec;random.parameters.insert(random.parameters.end(),{{"run_mode",float(mode)},{"seed",419},{"step2_chance",.4f}});
        auto a=sequenceEvents(random,300000,1),b=sequenceEvents(random,300000,24);check(notes(a)==notes(b)&&attacks(a)==attacks(b),"Seeded random traversal/chance independent of block size");}
    check(patternQuantize(61,1,0)==60&&patternQuantize(63,2,0)==63&&patternQuantize(127,6,11)<=127,"Scale quantization with downward tie and MIDI limits");
    auto module=makePattern({"seq","seq.pattern",{{"length",4},{"cv1_length",3},{"cv2_length",2},{"step1_cv1",-.5f},{"step2_cv1",.5f},{"step3_cv1",1},{"step1_cv2",1},{"step2_cv2",-1}}});
    SampleClock clock;clock.play(true);std::array<ClockFrame,24> frames;float out[3][24]{};float* ports[]={out[0],out[1],out[2]};NoteEventBlock events;
    std::vector<float> cv1,cv2;for(int n=0;n<24000;n+=24){for(auto& f:frames)f=clock.next();events.begin(24);module->process({24,{},nullptr,ports,nullptr,nullptr,nullptr,frames.data(),&events});if(n%6000==0){cv1.push_back(out[1][0]);cv2.push_back(out[2][0]);}}
    check(cv1==std::vector<float>({-.5f,.5f,1,-.5f})&&cv2==std::vector<float>({1,-1,1,-1}),"Two independent control outputs have independent loops");
    check(module->controls->laneHeads[2]==0&&module->controls->laneHeads[3]==1,"Distinct lane cursors match emitted controls");
    auto graph=sequencerPatch(0);Runtime a(graph),b(graph);a.clock.play(true);b.clock.play(true);std::vector<float> x(6000),y(6000);a.render(x.data(),6000,{});b.render(y.data(),6000,{});
    auto handle=a.controls("sequence");handle->editRevision.fetch_add(1);handle->values[patternIndex(1,Pitch)]=84;
    a.render(x.data(),6000,{});b.render(y.data(),6000,{});check(x==y,"Audio retains last complete parameter batch without blocking");
    handle->editRevision.fetch_add(1);a.clock.seek(0);b.clock.seek(0);a.render(x.data(),6000,{});b.render(y.data(),6000,{});a.render(x.data(),6000,{});b.render(y.data(),6000,{});check(x!=y,"Completed parameter batch becomes audible");
    auto cvGraph=sequencerPatch(0);cvGraph.modulations.push_back({{"sequence","cv1"},{"filter","cutoff_hz"},.25f});cvGraph.nodes.front().parameters={{"step1_cv1",.8f}};validateRack(cvGraph);Runtime cvRuntime(cvGraph);cvRuntime.clock.play(true);allocations=0;watchAllocations=true;cvRuntime.render(x.data(),6000,{});watchAllocations=false;check(allocations==0&&energy(x)>0,"Pattern CV routes through matrix without render allocation");
}
}
