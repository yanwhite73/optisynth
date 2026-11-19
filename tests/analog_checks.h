#pragma once
#include "../src/modular/analog.h"
namespace {
NoteEvent keyEvent(int pitch,bool on,float velocity=.8f){return {0,1,uint64_t(pitch)+1,on?NoteEventKind::On:NoteEventKind::Off,pitch,velocity};}
std::vector<float> renderKeys(Runtime& r,const std::vector<NoteEvent>& events,double seconds,std::vector<float>* rightOut=nullptr){
    NoteEventBlock keys;keys.begin(quantum);for(const auto& e:events)keys.push(e);
    const int frames=int(48000*seconds);std::vector<float> left(static_cast<size_t>(frames),0.f),right(static_cast<size_t>(frames),0.f);
    for(int i=0;i<frames;i+=quantum){const int n=std::min(quantum,frames-i);r.render(left.data()+i,right.data()+i,n,Note{},nullptr,i==0?&keys:nullptr);}
    if(rightOut)*rightOut=right;
    return left;
}
double rms(const std::vector<float>& x,size_t from=0,size_t to=SIZE_MAX){double e=0;to=std::min(to,x.size());for(size_t i=from;i<to;++i){if(!std::isfinite(x[i]))throw std::runtime_error("nonfinite");e+=double(x[i])*x[i];}return std::sqrt(e/double(std::max<size_t>(1,to-from)));}
std::vector<float> runModule(const NodeSpec& spec,int frames,Note note,int voice=0,const std::vector<float>& input={}){
    auto module=makeAnalog(spec,{48000,quantum,Scope::Voice,voice});
    std::array<float,quantum> zero{},a{},b{},in{};std::vector<float> out;
    const float* inputs[3]={in.data(),zero.data(),zero.data()};float* outputs[2]={a.data(),b.data()};
    for(int i=0;i<frames;i+=quantum){
        const int n=std::min(quantum,frames-i);
        for(int k=0;k<n;++k)in[size_t(k)]=input.empty()?0.f:input[size_t(i+k)];
        module->process({n,note,inputs,outputs,inputs,outputs});note.trigger=false;
        out.insert(out.end(),a.begin(),a.begin()+n);
    }
    return out;
}
double frequency(const std::vector<float>& x,size_t skip){
    int crossings=0;size_t first=0,last=0;
    for(size_t i=skip+1;i<x.size();++i)if(x[i-1]<=0&&x[i]>0){if(!crossings)first=i;last=i;++crossings;}
    return crossings>1?48000.0*(crossings-1)/double(last-first):0;
}
GraphSpec withVoices(int model,std::vector<std::pair<std::string,float>> params){auto g=padPatch(model);for(auto& n:g.nodes)if(n.id=="voices")n.parameters=std::move(params);return g;}
void voiceFeatureChecks(){
    // Unison stacks detuned voices per key; release frees all of them.
    {
        Runtime r(withVoices(0,{{"voices",16},{"unison",4},{"detune",20}}));
        renderKeys(r,{keyEvent(60,true)},.05);
        std::vector<float> bends;for(int v=0;v<maximumVoices;++v)if(r.voiceNote(v).gate&&r.voiceNote(v).midi==60)bends.push_back(r.voiceNote(v).bend);
        std::sort(bends.begin(),bends.end());
        check(bends.size()==4&&std::abs(bends.front()+.1f)<1e-4f&&std::abs(bends.back()-.1f)<1e-4f,"Unison of four spreads 20 cents across one key");
        renderKeys(r,{keyEvent(60,false)},.05);
        int gated=0;for(int v=0;v<maximumVoices;++v)gated+=r.voiceNote(v).gate;
        check(gated==0,"Releasing a unison key frees all its voices");
        renderKeys(r,{keyEvent(48,true),keyEvent(52,true),keyEvent(55,true),keyEvent(59,true),keyEvent(62,true)},.05);
        gated=0;for(int v=0;v<maximumVoices;++v)gated+=r.voiceNote(v).gate;
        check(gated==16,"Sixteen voices hold four unison notes, the fifth steals");
    }
    // When every voice is held, the quietest one is stolen rather than the oldest.
    {
        Runtime r(withVoices(0,{{"voices",2}}));
        renderKeys(r,{keyEvent(60,true,1.f)},3);
        renderKeys(r,{keyEvent(64,true,.05f)},3);   // newer but much quieter (velocity scales level)
        renderKeys(r,{keyEvent(67,true)},.05);
        bool loudKept=false,quietGone=true;
        for(int v=0;v<maximumVoices;++v){const auto n=r.voiceNote(v);if(!n.gate)continue;loudKept|=n.midi==60;quietGone&=n.midi!=64;}
        check(loudKept&&quietGone,"Stealing takes the quietest held voice, not the oldest");
    }
    // Stealing can be switched off: surplus notes are not played.
    {
        Runtime r(withVoices(0,{{"voices",2},{"steal",0}}));
        renderKeys(r,{keyEvent(60,true),keyEvent(64,true),keyEvent(67,true)},.05);
        bool third=false;int gated=0;for(int v=0;v<maximumVoices;++v){gated+=r.voiceNote(v).gate;third|=r.voiceNote(v).gate&&r.voiceNote(v).midi==67;}
        check(gated==2&&!third,"With stealing off a third key on two voices is ignored");
    }
    // Glide moves a voice from its previous pitch to the new one over the set time.
    {
        Runtime r(withVoices(0,{{"voices",1},{"glide",.5f}}));
        renderKeys(r,{keyEvent(60,true)},.3);
        renderKeys(r,{keyEvent(72,true)},.01);
        const float early=r.voiceNote(0).bend;
        renderKeys(r,{},.6);
        const float late=r.voiceNote(0).bend;
        check(r.voiceNote(0).midi==72&&early<-10.f&&late>-1.f&&late<=0.f,"Glide sweeps 12 semitones in about the glide time");
    }
    // Tuning tables: exact just-intonation and harmonic-series offsets.
    {
        Runtime ji(withVoices(0,{{"tuning",1},{"root",0}})),harmonic(withVoices(0,{{"tuning",4},{"root",0}}));
        renderKeys(ji,{keyEvent(64,true),keyEvent(67,true)},.05);
        renderKeys(harmonic,{keyEvent(61,true),keyEvent(72,true)},.05);
        auto bendFor=[](const Runtime& r,int key){for(int v=0;v<maximumVoices;++v)if(r.voiceNote(v).gate&&r.voiceNote(v).midi==key)return r.voiceNote(v).bend;return 99.f;};
        check(std::abs(bendFor(ji,64)-float(12*std::log2(1.25)-4))<1e-4f,"5-limit major third is 5/4");
        check(std::abs(bendFor(ji,67)-float(12*std::log2(1.5)-7))<1e-4f,"5-limit fifth is 3/2");
        check(std::abs(bendFor(harmonic,61)-float(12*std::log2(9./8)-1))<1e-4f,"Harmonic series: next key is partial 9");
        check(std::abs(bendFor(harmonic,72)-float(12*std::log2(20./8)-12))<1e-4f,"Harmonic series is not octave-repeating");
    }
    // Released voices go to sleep once silent; held voices stay awake.
    {
        Runtime r(padPatch(1));
        renderKeys(r,{keyEvent(60,true),keyEvent(64,true),keyEvent(67,true)},1);
        check(r.awakeVoices()==3,"Only the three played voices are processed");
        renderKeys(r,{keyEvent(60,false),keyEvent(64,false),keyEvent(67,false)},20);
        check(r.awakeVoices()==0,"Released voices sleep after their tails fall silent");
    }
    // Xpander pole mixing: LP4 equals the classic 4-pole; every mode stays bounded.
    {
        std::vector<float> noise(48000);uint32_t s=11;for(auto& x:noise){s=s*1664525u+1013904223u;x=float(s>>8)/8388608.f-1.f;}
        Note n{60,.8f,true,true};
        check(runModule({"f","analog.filter",{{"poles",1},{"resonance",.5f}}},48000,n,0,noise)==runModule({"f","analog.filter",{{"pole_mix",1},{"resonance",.5f}}},48000,n,0,noise),"Xpander LP 4 equals the classic 4-pole");
        auto lowBand=[](const std::vector<float>& x){double a=0,b=0,e=0;for(float v:x){a+=(v-a)*.013;b+=(a-b)*.013;e+=b*b;}return e;};
        const auto lp1=runModule({"f","analog.filter",{{"pole_mix",4},{"cutoff_hz",2000}}},48000,n,0,noise),hp1=runModule({"f","analog.filter",{{"pole_mix",7},{"cutoff_hz",2000}}},48000,n,0,noise);
        check(lowBand(lp1)>4*lowBand(hp1),"Xpander HP 1 removes the low end that LP 1 keeps");
        std::vector<float> impulse(48000);impulse[10]=1;
        bool bounded=true;
        for(int mode=1;mode<=15;++mode)for(float res:{0.f,1.f}){
            const auto y=runModule({"f","analog.filter",{{"pole_mix",float(mode)},{"resonance",res}}},48000,n,0,mode%2?noise:impulse);
            for(float v:y)bounded&=std::isfinite(v)&&std::abs(v)<6.f;
        }
        check(bounded,"All 15 Xpander modes stay finite and bounded at zero and full resonance");
    }
    // Quality levels agree in the passband.
    {
        std::vector<float> tone(48000);for(size_t i=0;i<tone.size();++i)tone[i]=.5f*std::sin(float(i)*2*3.14159265f*200/48000);
        Note n{60,.8f,true,true};
        double level[3];
        for(int q=0;q<3;++q)level[q]=rms(runModule({"f","analog.filter",{{"cutoff_hz",2000},{"quality",float(q)}}},48000,n,0,tone),4800);
        check(std::abs(20*std::log10(level[0]/level[1]))<1&&std::abs(20*std::log10(level[2]/level[1]))<1,"Draft, normal and high quality agree within 1 dB in the passband");
    }
    // Sixteen voices with unison: cost of the busiest pad starter.
    {
        Runtime r(padPatch(2));
        NoteEventBlock keys;keys.begin(quantum);for(int k:{48,55,60,64,67,71,74,79})keys.push(keyEvent(k,true));
        std::array<float,quantum> l{},rr{};
        r.render(l.data(),rr.data(),quantum,Note{},nullptr,&keys);
        allocations=0;watchAllocations=true;
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<48000*10/quantum;++i)r.render(l.data(),rr.data(),quantum,Note{});
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        watchAllocations=false;
        check(allocations==0&&r.awakeVoices()==16,"Sixteen unison voices render without allocation");
        std::printf("Xpander pad: 16 voices (8 keys x unison 2) + chorus + reverb, 10 s audio in %.3f s (%.2f%% of one core)\n",seconds,seconds*10);
    }
}
void analogChecks(){
    // Voice allocation from keyboard events.
    {
        Runtime r(padPatch(0));
        check(r.voiceGroupSize()==maximumVoices,"Pad patch forms an eight-voice group");
        auto chord=renderKeys(r,{keyEvent(60,true),keyEvent(64,true),keyEvent(67,true)},3);
        std::vector<int> held;for(int v=0;v<maximumVoices;++v)if(r.voiceNote(v).gate)held.push_back(r.voiceNote(v).midi);
        std::sort(held.begin(),held.end());
        check(held==std::vector<int>({60,64,67}),"Three keys hold three separate voices");
        check(rms(chord,48000*2)>.01,"Held chord is audible after the slow attack");
        check(rms(chord,0,2400)<rms(chord,48000*2),"Swell rises from the attack");
        auto tail=renderKeys(r,{keyEvent(60,false),keyEvent(64,false),keyEvent(67,false)},1.5);
        bool anyGate=false;for(int v=0;v<maximumVoices;++v)anyGate|=r.voiceNote(v).gate;
        check(!anyGate,"Key releases close every voice gate");
        check(rms(tail,0,24000)>.005,"Release and reverb tail continue after the keys are released");
    }
    // Stealing, retriggering and the voice-count control.
    {
        Runtime r(padPatch(1));
        std::vector<NoteEvent> nine;for(int k=0;k<9;++k)nine.push_back(keyEvent(50+k,true));
        renderKeys(r,nine,.2);
        int active=0;bool oldest=false,newest=false;
        for(int v=0;v<maximumVoices;++v){const auto n=r.voiceNote(v);if(n.gate){++active;oldest|=n.midi==50;newest|=n.midi==58;}}
        check(active==8&&!oldest&&newest,"Ninth key steals the oldest held voice");
        renderKeys(r,{keyEvent(58,true)},.05);
        int count58=0;for(int v=0;v<maximumVoices;++v)count58+=r.voiceNote(v).gate&&r.voiceNote(v).midi==58;
        check(count58==1,"Repeated key retriggers its own voice");
        r.controls("voices")->values[0].store(2);
        renderKeys(r,{},.05);
        int still=0;for(int v=0;v<maximumVoices;++v)still+=r.voiceNote(v).gate;
        check(still<=2,"Reducing the voice count releases the surplus voices");
    }
    // Knobs are shared by every voice: a closed filter silences all of them.
    {
        auto g=padPatch(1);
        for(auto& n:g.nodes){if(n.id=="voices")n.parameters={{"voices",2},{"level",.5f}};if(n.id=="filter_env")n.parameters={{"attack",.001f},{"sustain",1}};
            if(n.id=="amp_env")n.parameters={{"attack",.001f},{"sustain",1}};}
        g.modulations.clear();
        // Poly-mod PWM creates near-DC beating that passes even a closed filter; isolate knob sharing.
        g.nodes.erase(std::remove_if(g.nodes.begin(),g.nodes.end(),[](const NodeSpec& n){return n.id=="polymod";}),g.nodes.end());
        g.connections.erase(std::remove_if(g.connections.begin(),g.connections.end(),[](const Connection& c){return c.source.node=="polymod"||c.destination.node=="polymod";}),g.connections.end());
        Runtime open(g),closed(g);
        closed.controls("filter")->values[0].store(20);closed.controls("filter")->values[5].store(-6);
        renderKeys(open,{},.1);renderKeys(closed,{},.1);   // let the 5 ms knob smoothing settle first
        const auto a=renderKeys(open,{keyEvent(60,true),keyEvent(72,true)},.5),b=renderKeys(closed,{keyEvent(60,true),keyEvent(72,true)},.5);
        check(rms(b,12000)<.05*rms(a,12000),"One cutoff knob controls both voices");
    }
    // Voice-group rules are explicit errors.
    {
        auto expectThrow=[](GraphSpec g,const char* label){bool threw=false;try{Runtime r(g);}catch(const std::invalid_argument&){threw=true;}check(threw,label);};
        auto g=padPatch(0);g.outputs.push_back({"vca","out"});
        expectThrow(g,"Single voices cannot bypass the Voice mixer to the output");
        g=padPatch(0);g.nodes.push_back({"second","voice.mix",{}});g.connections.push_back({{"vca","out"},{"second","in"}});g.outputs.push_back({"second","out"});
        expectThrow(g,"Only one Voice mixer per rack");
        g=padPatch(0);g.nodes.push_back({"seq","seq.euclidean",{}});g.connections.push_back({{"seq","notes"},{"amp_env","notes"}});
        expectThrow(g,"Sequencers must address the Voice mixer, not single voices");
        g=padPatch(0);g.modulations.push_back({{"amp_env","out"},{"reverb","time_s"},.2f});
        expectThrow(g,"Per-voice modulation of a global effect is rejected");
    }
    // A sequencer can play the voice group through the mixer's notes input.
    {
        auto g=padPatch(1);g.nodes.push_back({"seq","seq.euclidean",{{"hits",4},{"length",8}}});
        g.connections.push_back({{"seq","notes"},{"voices","notes"}});
        Runtime r(g);r.clock.play(true);
        std::vector<float> x(48000*2);bool gated=false;
        for(size_t i=0;i<x.size();i+=quantum){r.render(x.data()+i,std::vector<float>(quantum).data(),quantum,Note{});for(int v=0;v<maximumVoices;++v)gated|=r.voiceNote(v).gate;}
        check(gated,"Sequencer notes allocate voices");
    }
    // Rendering a full chord allocates nothing and costs little.
    {
        Runtime r(padPatch(0));
        NoteEventBlock keys;keys.begin(quantum);for(int k:{48,55,60,64,67,71,74,79})keys.push(keyEvent(k,true));
        std::array<float,quantum> l{},rr{};
        r.render(l.data(),rr.data(),quantum,Note{},nullptr,&keys);
        allocations=0;watchAllocations=true;
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<48000*10/quantum;++i)r.render(l.data(),rr.data(),quantum,Note{});
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        watchAllocations=false;
        check(allocations==0,"Eight-voice pad renders without allocation");
        check(seconds<10*.25,"Eight-voice pad renders well under real time");
        std::printf("Analog pad: 8 voices + chorus + reverb, 10 s audio in %.3f s (%.2f%% of one core)\n",seconds,seconds*10);
    }
    voiceFeatureChecks();
    // VCO tuning and per-voice drift.
    {
        Note a4{69,.8f,true,true};
        const auto x=runModule({"o","analog.vco",{{"drift",0},{"saw",1}}},48000,a4);
        check(std::abs(frequency(x,4800)/440.0-1)<.003,"VCO tracks A4 = 440 Hz without drift");
        const auto v0=runModule({"o","analog.vco",{{"drift",1}}},96000,a4,0),v1=runModule({"o","analog.vco",{{"drift",1}}},96000,a4,1);
        const double f0=frequency(v0,4800),f1=frequency(v1,4800);
        check(std::abs(1200*std::log2(f0/f1))>.5&&std::abs(1200*std::log2(f0/440))<30,"Voices drift individually, within tens of cents");
        check(runModule({"o","analog.vco",{{"drift",1}}},9600,a4,3)==runModule({"o","analog.vco",{{"drift",1}}},9600,a4,3),"Per-voice drift is deterministic");
    }
    // CEM3310-style envelope: attack reaches peak at the set time; release decays.
    {
        Note on{60,1,true,true};
        const auto e=runModule({"e","analog.env",{{"attack",1},{"decay",1},{"sustain",.5f},{"velocity",0}}},72000,on);
        size_t peak=0;while(peak<e.size()&&e[peak]<.999f)++peak;
        check(std::abs(double(peak)/48000-1)<.05,"Envelope attack reaches its peak at the set time");
        check(e[e.size()/4]>e[e.size()/8],"Envelope attack is rising");
        check(e[size_t(48000*.5)]>.6f&&e[size_t(48000*.5)]<.75f,"Attack follows the RC charge curve above a straight line");
    }
    // Filter: modes behave, and full resonance stays bounded in both topologies.
    {
        std::vector<float> noise(48000);uint32_t s=7;for(auto& x:noise){s=s*1664525u+1013904223u;x=float(s>>8)/8388608.f-1.f;}
        Note n{60,.8f,true,true};
        const auto lp=runModule({"f","analog.filter",{{"cutoff_hz",2000},{"mode",0}}},48000,n,0,noise);
        const auto hp=runModule({"f","analog.filter",{{"cutoff_hz",2000},{"mode",1}}},48000,n,0,noise);
        const auto notch=runModule({"f","analog.filter",{{"cutoff_hz",2000},{"mode",.5f}}},48000,n,0,noise);
        auto lowBand=[](const std::vector<float>& x){double a=0,b=0,e=0;for(float v:x){a+=(v-a)*.013;b+=(a-b)*.013;e+=b*b;}return e;};  // 2-pole ~100 Hz
        check(lowBand(lp)>4*lowBand(hp),"2-pole low-pass keeps low end; high-pass removes it");
        check(rms(notch)<rms(lp)+rms(hp),"Mode centre forms the notch crossfade");
        std::vector<float> impulse(96000);impulse[10]=1;
        for(int poles=0;poles<2;++poles){
            const auto ring=runModule({"f","analog.filter",{{"resonance",1},{"poles",float(poles)},{"cutoff_hz",800}}},96000,n,0,impulse);
            float peakValue=0;bool finite=true;for(float v:ring){finite&=std::isfinite(v);peakValue=std::max(peakValue,std::abs(v));}
            check(finite,"Self-oscillating filter output finite");
            check(peakValue<4,"Full resonance is bounded (diode / OTA limiting)");
        }
        std::vector<float> loud(48000);for(size_t i=0;i<loud.size();++i)loud[i]=4*std::sin(float(i)*.05f);
        const auto driven=runModule({"f","analog.filter",{{"resonance",1},{"poles",1},{"drive",1}}},48000,n,0,loud);
        check(rms(driven)<4,"Overdriven 4-pole stays bounded");
    }
}
}
