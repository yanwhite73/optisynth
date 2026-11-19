#include "sequencer.h"
#include "rack.h"
#include <cmath>

namespace optisynth {
std::vector<Panel> sequencerPanels(){
    std::vector<Panel> result;
    for(int model=0;model<3;++model){
        const char* ids[]={"seq.pattern","seq.euclidean","seq.stage"};
        const char* names[]={"Pattern sequencer","Euclidean sequencer","Stage sequencer"};
        const char* descriptions[]={"16 notes / per-step velocity and rests","Evenly spaced hits / independent length and rotation","8 stages / pulse duration / Repeat, Hold or Rest"};
        Panel p{ids[model],names[model],descriptions[model],"sequencer",{
            {"enabled","Enabled","",0,1,1,false,1},
            {"division","Division","",0,5,1,false,1},
            {"length","Length","",1,float(model==0?16:model==1?32:8),float(model==2?4:16),false,1},
            {"gate","Gate","",.05f,.95f,.6f,false,.01},
            {"velocity","Velocity","",.01f,1,.8f,false,.01}}};
        if(model==1){
            p.parameters.push_back({"hits","Hits","",0,32,5,false,1});
            p.parameters.push_back({"rotation","Rotation","",0,31,0,false,1});
            p.parameters.push_back({"pitch","Pitch","MIDI",0,127,48,false,1});
        }else for(int i=0;i<(model==0?patternSteps:stageSteps);++i){
            const auto prefix="step"+std::to_string(i+1)+"_";
            constexpr int notes[]={48,55,58,60,51,55,63,58};
            p.parameters.push_back({prefix+"pitch","Pitch","MIDI",0,127,float(notes[i%8]),false,1});
            if(model==0){
                p.parameters.push_back({prefix+"velocity","Step velocity","",.01f,1,1,false,.01});
                p.parameters.push_back({prefix+"on","Note on","",0,1,1,false,1});
            }else{
                p.parameters.push_back({prefix+"pulses","Pulses","",1,8,float(i%3+1),false,1});
                p.parameters.push_back({prefix+"mode","Articulation","",0,2,float(i==1?1:0),false,1});
            }
        }
        result.push_back(std::move(p));
    }
    return result;
}
namespace {
class Sequencer final:public AudioModule {
public:
    explicit Sequencer(const NodeSpec& node):spec(panel(node.type)),model(node.type=="seq.pattern"?0:node.type=="seq.euclidean"?1:2){
        for(size_t i=0;i<spec.parameters.size();++i)controls->values[i]=spec.parameters[i].initial;
        for(const auto& saved:node.parameters){
            const auto p=std::find_if(spec.parameters.begin(),spec.parameters.end(),[&](const Parameter& p){return p.id==saved.first;});
            if(p==spec.parameters.end()||!std::isfinite(saved.second)||saved.second<p->minimum||saved.second>p->maximum)throw std::invalid_argument("Invalid sequencer parameter");
            controls->values[size_t(p-spec.parameters.begin())]=saved.second;
        }
    }
    void process(const AudioBlock& b) noexcept override {
        if(!b.events||!b.clock)return;
        // Discrete edits are snapshotted once per quantum, never smoothed into notes.
        std::array<float,sequenceCommon+3*patternSteps> p{};
        for(size_t i=0;i<spec.parameters.size();++i){const auto& d=spec.parameters[i];float x=b.parameters?b.parameters[i]:controls->values[i].load(std::memory_order_relaxed);
            x=std::isfinite(x)?std::clamp(x,d.minimum,d.maximum):d.initial;p[i]=d.step>=1?std::round(x):x;}
        const int length=int(p[2]);const double division=sequenceDivision(int(p[1]));grid.division(division);
        for(int i=0;i<b.frames;++i){
            const auto& clock=b.clock[i];const auto pulse=grid.consume(clock);
            auto release=[&]{if(held){b.events->push({uint32_t(i),1,held,NoteEventKind::Off,pitch,0});held=0;}};
            if(pulse.reset||!clock.playing||p[0]<.5f||length!=previousLength){release();previousStage=-1;}
            previousLength=length;
            if(!clock.playing||p[0]<.5f){controls->playhead.store(-1,std::memory_order_relaxed);continue;}
            if(held&&clock.beat+1e-10>=offBeat)release();
            if(!pulse.tick)continue;
            auto modulo=[](int64_t n,int d){return int((n%d+d)%d);};
            int step=modulo(pulse.index,length),stagePulse=0,dwell=1;
            if(model==2){
                int total=0;for(int s=0;s<length;++s)total+=int(p[sequenceCommon+s*3+1]);
                stagePulse=modulo(pulse.index,total);step=0;
                while(step+1<length&&stagePulse>=int(p[sequenceCommon+step*3+1])){stagePulse-=int(p[sequenceCommon+step*3+1]);++step;}
                dwell=int(p[sequenceCommon+step*3+1]);
            }
            controls->playhead.store(step,std::memory_order_relaxed);
            float velocity=p[4];bool trigger=true;double duration=division*p[3];
            int nextPitch=48;
            if(model==0){const int k=sequenceCommon+3*step;nextPitch=int(p[k]);velocity*=p[k+1];trigger=p[k+2]>=.5f;}
            if(model==1){nextPitch=int(p[7]);trigger=euclideanHit(step,length,int(p[5]),int(p[6]));}
            if(model==2){const int k=sequenceCommon+3*step;nextPitch=int(p[k]);const int mode=int(p[k+2]);
                trigger=mode==0||(mode==1&&(stagePulse==0||previousStage!=step));
                if(mode==1)duration=division*((dwell-stagePulse-1)+p[3]);
                if(mode==2)release();
            }
            previousStage=step;
            if(trigger){release();pitch=nextPitch;held=++identity;
                offBeat=clock.beat+duration;b.events->push({uint32_t(i),1,held,NoteEventKind::On,pitch,velocity});}
            if(b.events->failed()){held=0;controls->playhead.store(-1,std::memory_order_relaxed);}
        }
    }
private:
    const Panel& spec;int model,previousLength=0,previousStage=-1,pitch=48;
    GridClock grid;uint64_t held=0,identity=0;double offBeat=0;
};
}
std::unique_ptr<AudioModule> makeSequencer(const NodeSpec& n){return std::make_unique<Sequencer>(n);}
GraphSpec sequencerPatch(int model){
    auto g=model==1?rackPreset(1):instrumentPatch("osc.variable","spectral.adsr","mutable.lowpass",true,true);
    g.nodes.insert(g.nodes.begin(),{"sequence",model==0?"seq.pattern":model==1?"seq.euclidean":"seq.stage",{}});
    g.connections.push_back({{"sequence","notes"},{"source","notes"}});
    g.connections.push_back({{"sequence","notes"},{"envelope","notes"}});
    for(auto& n:g.nodes){
        if(n.id=="envelope")n.parameters={{"attack",.003f},{"decay",.18f},{"sustain",.35f},{"release",.12f}};
        if(n.id=="filter")n.parameters={{"cutoff_hz",1200},{"env_octaves",2},{"resonance",1.2f}};
    }
    // Modal starter uses bypass, which has no filter parameters.
    if(model==1)for(auto& n:g.nodes)if(n.id=="filter")n.parameters.clear();
    return g;
}
}
