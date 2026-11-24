#include "../src/modular/player.h"
#include "../src/modular/rack.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <new>
#include <stdexcept>
#include <limits>

static bool watchAllocations=false;
static int allocations=0;
void* operator new(std::size_t n) { if(watchAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace optisynth;
namespace {
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
double energy(const std::vector<float>& x) {double e=0;for(float v:x){if(!std::isfinite(v))throw std::runtime_error("nonfinite");e+=v*v;}return e;}
std::vector<float> render(GraphSpec g) {
    Runtime r(g);std::vector<float> x(48000);r.render(x.data(),24000,{48,.8f,true,true});
    r.render(x.data()+24000,24000,{48,.8f,false,false});return x;
}
void wav(const std::string& path,const std::vector<float>& x) {
    std::ofstream o(path,std::ios::binary);
    auto u16=[&](uint16_t v){o.put(v&255);o.put(v>>8);};
    auto u32=[&](uint32_t v){u16(v&65535);u16(v>>16);};
    o.write("RIFF",4);u32(36+x.size()*2);o.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(48000);u32(96000);u16(2);u16(16);
    o.write("data",4);u32(x.size()*2);for(float v:x)u16(uint16_t(int16_t(std::clamp(v*.25f,-.95f,.95f)*32767)));
}
}
#include "sample_checks.h"
#include "spectral_checks.h"
#include "sequencer_checks.h"
#include "analog_checks.h"
#include "gate_delay_checks.h"
#include "prism_checks.h"
#include "pattern_checks.h"
#include "performance_checks.h"
#include "reference_sequencer_checks.h"
int main(int argc,char** argv){try {
    // At fixed pitch, doubling bubble size halves its oscillation frequency.
    auto drop=[](float size,float decay){
        GraphSpec g;
        g.nodes={{"drop","drum.droplet",{{"size",size},{"decay",decay},{"splash",0},{"rise",0}}}};
        g.outputs={{"drop","out"}};
        return render(g);
    };
    const auto small=drop(1,.5f),large=drop(2,.5f),shortDrop=drop(1,.025f);
    auto crossings=[](const std::vector<float>& x){int n=0;for(int i=1;i<4800;++i)n+=x[i-1]<=0 && x[i]>0;return n;};
    check(std::abs(crossings(small)-2*crossings(large))<=2,"Bubble radius controls the ringing pitch");
    check(energy(small)>8*energy(shortDrop) && energy(shortDrop)>0,"Droplet damping shortens an audible strike");
    auto handStroke=[](float body,float opening,float damp,bool lift,float position=0.f){
        GraphSpec g;g.nodes={{"hand","drum.skin_vessel",{{"body",body},{"opening",opening},{"damp",damp},{"lift",10},{"position",position}}}};
        g.outputs={{"hand","out"}};Runtime r(g);
        std::vector<float> x(24000);
        r.render(x.data(),1440,{48,.85f,true,true});
        r.render(x.data()+1440,int(x.size())-1440,{48,.85f,!lift,false});
        return x;
    };
    auto skin=handStroke(0,1,.2f,true),vessel=handStroke(1,1,.2f,true),sealed=handStroke(1,0,.2f,true);
    check(skin==handStroke(0,0,.2f,true),"Aperture leaves the frame endpoint alone");
    check(vessel!=sealed && vessel!=handStroke(1,1,.2f,false),"Open aperture and hand release affect the vessel tail");
    auto ringCrossings=[](const std::vector<float>& x){int n=0;for(int i=3001;i<8000;++i)n+=x[i-1]<0 && x[i]>=0;return n;};
    check(ringCrossings(vessel)>ringCrossings(sealed)+3,"Uncovering the vessel raises the air mode");
    auto tailEnergy=[](const std::vector<float>& stroke){
        double sum=0;
        for(size_t i=4800;i<9600;++i)sum+=double(stroke[i])*stroke[i];
        return sum;
    };
    check(tailEnergy(vessel)<.55*tailEnergy(sealed),
          "Lifting the hand damps the pitched vessel tail after the bend");
    const auto shellTap=handStroke(1,1,.2f,true,1);
    check(ringCrossings(shellTap)>5*ringCrossings(vessel)
          && tailEnergy(shellTap)/energy(shellTap)>2*tailEnergy(vessel)/energy(vessel),
          "The higher shell tap rings past the low palm thud");
    auto sequencedTap=[](float precedingPosition){
        auto pattern=std::make_shared<PerformanceData>();
        auto& steps=pattern->tracks[0].patterns[0].steps;
        steps[0].on=false;steps[0].cv=precedingPosition;
        steps[1].on=true;steps[1].cv=1;
        GraphSpec g;
        g.nodes={{"pot","drum.skin_vessel",{{"body",1},{"position",0}}},
                 {"mix","voice.mix",{}},{"seq","seq.performance",{},pattern}};
        g.connections={{{"pot","out"},{"mix","in"}},{{"seq","t1_notes"},{"mix","notes"}}};
        g.modulations={{{"seq","t1_cv"},{"pot","position"},1}};
        g.outputs={{"mix","out"}};
        Runtime r(g);r.clock.tempo(92);r.clock.play(true);
        std::vector<float> audio(14000);r.render(audio.data(),int(audio.size()),{});
        return audio;
    };
    const auto steppedTap=sequencedTap(0);
    check(energy(steppedTap)>0 && steppedTap==sequencedTap(1),
          "A finger stroke takes its position from the new step at the note offset");
    check(energy(skin)>2*energy(handStroke(0,1,1,true)) && energy(vessel)>0,"Hand damping absorbs membrane energy");
    auto energySpanMs=[](const std::vector<float>& audio){
        const double total=energy(audio);double sum=0;int start=-1;
        for(int i=0;i<int(audio.size());++i){
            sum+=double(audio[i])*audio[i];
            if(start<0 && sum>=total*.05)start=i;
            if(sum>=total*.95)return (i-start)/48.;
        }
        return 0.;
    };
    const auto openSpan=energySpanMs(handStroke(0,1,.04f,true));
    const auto mutedSpan=energySpanMs(handStroke(0,1,.64f,true));
    check(openSpan>=193 && openSpan<=287 && mutedSpan>=100 && mutedSpan<=141,
          "Open and damped frame strokes reach measured VCSL energy-duration ranges");
    prismChecks();
    gateDelayChecks();
    if(argc==2 && std::string(argv[1])=="--gate-delay"){std::printf("%d device checks passed\n",checks);return 0;}
    analogChecks();
    sequencerChecks();patternChecks();performanceChecks();referenceSequencerChecks();
    spectralChecks();
    sampleChecks();
    for(int i=0;i<4;++i){auto g=rackPreset(i);validateRack(g);auto x=render(g);check(energy(x)>1e-6,"Rack starter audible");check(x==render(g),"Rack starter deterministic");wav("build/rack-"+std::to_string(i)+".wav",x);}
    for(const auto* source:{"osc.variable","osc.sine"})for(const auto* filter:{"mutable.lowpass","mutable.bandpass","mutable.highpass"}){
        auto g=instrumentPatch(source,"spectral.adsr",filter,true,true);Runtime r(g);
        std::array<float,257> block{};
        for(int extreme=0;extreme<2;++extreme){
            for(const auto& n:g.nodes){auto c=r.controls(n.id);if(!c)continue;const auto& ps=panel(n.type).parameters;for(size_t j=0;j<ps.size();++j)c->values[j]=extreme?ps[j].maximum:ps[j].minimum;}
            allocations=0;watchAllocations=true;bool finite=true;
            for(int k=0;k<200;++k){r.render(block.data(),block.size(),{extreme?127:0,1,true,k==0});for(float v:block)finite&=std::isfinite(v);}
            watchAllocations=false;check(finite&&allocations==0,"New oscillator/filter extremes finite without allocation");
        }
    }
    {
        auto g=rackPreset(2);Runtime r(g);auto c=r.controls("resonator");std::array<float,10000> block{};
        for(int extreme=0;extreme<2;++extreme){const auto& ps=panel("modal.resonator").parameters;for(size_t j=0;j<ps.size();++j)c->values[j]=extreme?ps[j].maximum:ps[j].minimum;
            allocations=0;watchAllocations=true;r.render(block.data(),block.size(),{extreme?127:0,1,true,true});watchAllocations=false;
            check(allocations==0&&std::all_of(block.begin(),block.end(),[](float v){return std::isfinite(v);}),"External resonator bounds and allocation");
        }
        g=rackPreset(3);Runtime pair(g);auto a=pair.controls("source"),b=pair.controls("second");a->values[0]=12;
        check(a!=b&&b->values[0].load()==7.05f,"Two oscillator instances have independent controls");
        auto before=render(g);g.nodes.push_back({"dormant","modal.resonator",{}});validateRack(g);
        Runtime dormant(g);check(!dormant.controls("dormant")&&render(g)==before,"Unused resonator unconstructed and silent");
        g.nodes.back().parameters={{"brightness",2}};bool rejected=false;try{validateRack(g);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Dormant parameters validated");
        g=rackPreset(0);g.outputs={{"envelope","out"}};rejected=false;try{validateRack(g);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Control signal cannot become audio root");
        // Low and high outputs should discriminate a low sine at a high cutoff.
        auto lp=instrumentPatch("osc.sine","spectral.adsr","mutable.lowpass",false,false);
        auto hp=instrumentPatch("osc.sine","spectral.adsr","mutable.highpass",false,false);
        check(energy(render(lp))>energy(render(hp))*100,"Mutable low/high filter responses differ correctly");
        auto plain=instrumentPatch("osc.variable","spectral.adsr","filter.bypass",false,false);
        auto sync=plain;sync.nodes[0].parameters={{"sync",3}};
        check(render(plain)!=render(sync),"Hard sync changes oscillator sound");
    }
    for(int preset:{1,2}){
        const auto base=rackPreset(preset);
        const auto dry=render(base);
        for(const char* port:{"mat","bright","decay"}){
            auto patch=base;
            patch.nodes.push_back({"lfo","mod.lfo",{{"depth",0.f}}});
            setRackConnection(patch,{preset==1?"source":"resonator",port},{"lfo","out"});
            check(render(patch)==dry,"Zero CV preserves original modal render");
            patch.nodes.back().parameters={{"depth",.35f}};
            const auto wet=render(patch);
            double difference=0;
            for(size_t i=0;i<wet.size();++i)difference+=std::abs(wet[i]-dry[i]);
            check(difference>0.01 && energy(wet)>0,"Modal CV input changes the rendered sound");
        }
    }
    for(const auto* source:{"modal.voice","drum.analog"}) {
        auto g=instrumentPatch(source,"spectral.ahdsfr","filter.bypass",false,false);
        Runtime r(g);check(r.activeModules()==2,"Dormant envelope and VCA excluded");
        check(!r.controls("envelope"),"No dormant controls allocated");
        auto x=render(g);check(energy(x)>1e-6,"Source audible");
        check(x==render(g),"Deterministic retrigger and recall");
        wav(std::string("build/")+source+".wav",x);
        g.nodes[0].parameters={{"decay",.9f}};
        auto y=render(g);double delta=0;for(size_t i=0;i<x.size();++i)delta+=std::abs(x[i]-y[i]);
        check(delta>1,"Contextual decay changes audio");
        for(const auto* filter:{"spectral.lowpass","spectral.highpass","spectral.bandpass","spectral.notch"}) {
            g=instrumentPatch(source,"spectral.adsr",filter,true,true);
            Runtime live(g);auto c=live.controls("source");auto f=live.controls("filter");
            std::array<float,257> block{};
            for(int extreme=0;extreme<2;++extreme) {
                for(size_t i=0;i<panel(source).parameters.size();++i)c->values[i]=extreme?panel(source).parameters[i].maximum:panel(source).parameters[i].minimum;
                for(size_t i=0;i<panel(filter).parameters.size();++i)f->values[i]=extreme?panel(filter).parameters[i].maximum:panel(filter).parameters[i].minimum;
                allocations=0;watchAllocations=true;
                for(int k=0;k<100;++k){live.render(block.data(),block.size(),{extreme?127:0,1,true,k==0});for(float v:block)if(!std::isfinite(v))std::abort();}
                watchAllocations=false;check(allocations==0,"No allocation in audio render at extremes");
            }
        }
    }
    auto g=instrumentPatch("modal.voice","spectral.ahdsfr","spectral.lowpass",true,true);
    Runtime r(g);check(r.activeModules()==4,"One shared envelope for amplitude and filter");
    auto e=r.controls("envelope");e->values[5]=0;
    std::array<float,10000> b{};r.render(b.data(),b.size(),{48,.8f,true,true});
    r.render(b.data(),b.size(),{48,.8f,false,false});
    check(std::abs(b.back())<1e-10,"Envelope release closes VCA");
    // Inserting a second source/mixer uses the same runtime, not fixed slots.
    g.nodes.push_back({"second","drum.analog",{}});g.nodes.push_back({"mix","utility.mix",{}});
    g.connections[0].source={"mix","out"};
    g.connections.push_back({{"source","out"},{"mix","a"}});
    g.connections.push_back({{"second","out"},{"mix","b"}});
    check(energy(render(g))>1e-6,"Two engines share envelope and filter");
    for(double rate:{44100.,48000.,96000.,192000.}) {
        Player p;OutputConverter convert;convert.prepare(rate);
        p.publish(std::make_unique<Runtime>(g));p.note(48,true);
        allocations=0;watchAllocations=true;convert.render(p,b.data(),b.size());watchAllocations=false;
        check(allocations==0,"Conversion, handoff and playback allocate nothing");
        double power=0;bool bounded=true;for(float v:b){bounded&=std::isfinite(v)&&std::abs(v)<=.95f;power+=v*v;}
        check(bounded,"10,000 converted samples finite and bounded");
        check(power>1e-6,"Converted output audible");
        p.publish(std::make_unique<Runtime>(instrumentPatch("drum.analog","spectral.adsr","filter.bypass",false,false)));
        allocations=0;watchAllocations=true;convert.render(p,b.data(),b.size());watchAllocations=false;
        check(allocations==0,"Plan crossfade allocates nothing");p.collect();
    }
    auto bad=g;bad.nodes[0].parameters={{"not-a-control",1}};
    bool rejected=false;try{Runtime invalid(bad);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Unknown saved control rejected");
    Runtime bench(g);auto start=std::chrono::steady_clock::now();
    for(int i=0;i<500;++i)bench.render(b.data(),b.size(),{48,.8f,true,i%5==0});
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::printf("%d modular checks passed; 104.17 seconds audio in %.3f seconds (%.2f%% of one core)\n",checks,seconds,seconds/104.1667*100);
    return 0;
}catch(const std::exception& e){watchAllocations=false;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
