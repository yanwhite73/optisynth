#pragma once
#include "../src/modular/gate_delay.h"

namespace {
void gateDelayChecks() {
    // The same sustained strike is a single edge, and higher damping shortens the tail.
    OpticalGateChannel longGate, shortGate;
    float longTail = 0, shortTail = 0, peak = 0;
    const float slow = OpticalGateChannel::recovery(0), fast = OpticalGateChannel::recovery(1);
    for(int i=0;i<12000;++i){
        longTail=longGate.process(1,0,1,0,slow);
        shortTail=shortGate.process(1,0,1,0,fast);
        peak=std::max(peak,shortTail);
    }
    check(peak>.1f && longTail>.05f && shortTail<longTail*.01f,"Strike edge and variable optical decay");
    // A brighter open gate passes proportionally more high-frequency energy.
    auto brightness=[&](float level){
        OpticalGateChannel dc, alternating;
        float low=0,high=0;
        for(int i=0;i<2400;++i){
            low=dc.process(1,level,0,0,slow);
            high=alternating.process(i%2?1.f:-1.f,level,0,0,slow);
        }
        return high/low;
    };
    check(brightness(1) > 8*brightness(.2f),"Brightness and amplitude close together");
    {
        auto gate=makeAnalog({"gate","analog.dual_lpg",{{"a_keys",0},{"b_keys",0}}},{});
        std::array<float,quantum> zero{},ones{},aux{},sum{},a{},b{};
        ones.fill(1.f); aux.fill(.1f);
        const float* inputs[]{ones.data(),zero.data(),zero.data(),zero.data(),
                              ones.data(),ones.data(),zero.data(),zero.data(),aux.data(),zero.data()};
        float* outputs[]{sum.data(),a.data(),b.data()};
        for(int i=0;i<100;++i)gate->process({quantum,{},inputs,outputs});
        check(a.back()==0 && b.back()>.99f && std::abs(sum.back()-b.back()-.1f)<1e-6f,
              "Independent channel CV, individual outputs and auxiliary sum");
    }
    // Capture a non-symmetric phrase, overdub nothing in Hold, change tempo and reverse.
    ClockDelayChannel line;
    constexpr int length=480;
    auto sample=[](int pos){return pos==120?.7f:pos==310?.3f:0.f;};
    line.configure(length,false,false);
    for(int i=0;i<length;++i)line.process(sample(i),1,0);
    line.configure(length,true,false);
    bool held=true,reversed=true;
    for(int i=0;i<2*length;++i){
        line.configure(2*length,true,false);
        const float wet=line.process(-1,1,0);
        held &= std::abs(wet-sample(i%length))<1e-6f;
    }
    line.configure(length,true,true);
    for(int i=0;i<length;++i)reversed &= std::abs(line.process(1,1,0)-sample(length-1-i))<1e-6f;
    check(held&&reversed,"Captured phrase repeats at original tempo and plays backwards");
    line.configure(length,false,true);
    for(int i=0;i<length;++i)line.process(.125f,1,0);
    check(std::abs(line.process(0,1,0)-.125f)<1e-6f,"Reverse release resumes recording");

    // Exercise real module ports and tempo propagation. Feedback repeats at the same interval.
    for(int bpm : {90,120,150}){
        auto delay=makeAnalog({"delay","fx.dual_loop",{{"a_mix",1},{"b_mix",1},{"a_feedback",.5f},{"b_feedback",0}}},{});
        std::array<float,quantum> mono{},zero{},ping{},left{},right{},wetA{},wetB{};
        const float* ins[]{zero.data(),mono.data(),mono.data(),ping.data()};
        float* outs[]{left.data(),wetA.data(),wetB.data()};
        float* rights[]{right.data(),wetA.data(),wetB.data()};
        std::array<ClockFrame,quantum> clock{};for(auto& t:clock)t.tempo=bpm==150?120:bpm;
        const int a=int(48000.*30/bpm), b=int(48000.*45/bpm);
        bool exact=true;
        if(bpm==150){
            // Two ping edges 19,200 samples apart override a 120 BPM rack clock.
            for(int offset=0;offset<19200;offset+=quantum){
                ping[0]=offset==0;
                delay->process({quantum,{},ins,outs,ins,rights,nullptr,clock.data()});
            }
        }
        allocations=0;watchAllocations=true;
        for(int start=0;start<2*a+quantum;start+=quantum){
            mono[0]=start==0?1.f:0.f;ping[0]=bpm==150&&start==0;
            delay->process({quantum,{},ins,outs,ins,rights,nullptr,clock.data()});
            for(int i=0;i<quantum;++i){
                int time=start+i;
                exact &= left[i]==(time==a?1.f:time==2*a?.5f:0.f) && right[i]==(time==b?1.f:0.f);
                exact &= left[i]==wetA[i] && right[i]==wetB[i];
            }
        }
        watchAllocations=false;
        check(exact&&allocations==0,"Independent beat divisions, stereo and wet taps without render allocation");
    }
}
}
