#include "reference_sequencers.h"
#include <stdexcept>

namespace optisynth {
std::string probabilityKey(int step,int field){
    if(field<16)return "step"+std::to_string(step+1)+"_weight"+std::to_string(field+1);
    const char* names[]={"velocity","gate","chance","ratchet","condition","lock"};
    return "step"+std::to_string(step+1)+"_"+names[field-16];
}
std::string orbitKey(int ring,int field){const char* names[]={"length","hits","rotation","division","pitch","velocity","gate","chance","ratchet","swing","mute","solo","accent_every","accent","drift"};return "ring"+std::to_string(ring+1)+"_"+names[field];}
int probabilityPitch(int row,int scale,int root,int octave,int transpose) noexcept {
    int note=std::clamp(octave*12+root,0,127);
    while(row>0&&note<127){++note;if(patternQuantize(note,scale,root)==note)--row;}
    return std::clamp(note+transpose,0,127);
}
std::vector<Panel> referenceSequencerPanels(){
    Panel grid{"seq.probability","Probability grid","Weighted pitch choices / repeat or evolve / 32 steps","sequence_model",{
        {"enabled","Run","",0,1,1,false,1},{"division","Clock","",0,5,1,false,1},{"length","Last step","",1,32,16,false,1},{"first","First step","",1,32,1,false,1},
        {"transpose","Transpose","st",-24,24,0,false,1},{"scale","Scale","",0,6,2,false,1},{"root","Root","",0,11,0,false,1},{"octave","Base octave","",1,6,3,false,1},
        {"seed","Seed","",1,65535,719,false,1},{"swing","Swing","",0,.45f,.08f,false,.01},{"repeat","Repeat choices","",0,1,0,false,1},{"fill","Fill","",0,1,0,false,1},
        {"density","Density","",0,1,1,false,.01},{"velocity","Velocity","",.01f,1,.8f,false,.01},{"gate","Gate scale","",.01f,1,.8f,false,.01},{"traversal","Direction","",0,3,0,false,1}}};
    for(int row=0;row<16;++row)grid.parameters.push_back({"row"+std::to_string(row+1)+"_mute","Mute pitch","",0,1,0,false,1});
    for(int s=0;s<32;++s){for(int r=0;r<16;++r){const int base=(s%8==0?0:(s*3)%9);const float weight=r==base?75.f:r==base+4?35.f:r==base+7?15.f:0;grid.parameters.push_back({probabilityKey(s,r),"Pitch weight","",0,100,weight,false,1});}
        const Parameter fields[]={{"","Velocity","",.01f,1,s%4==0?1.f:.7f,false,.01},{"","Gate","",.01f,1,.6f,false,.01},{"","Chance","",0,1,s%8==6?.65f:1.f,false,.01},{"","Ratchets","",1,8,1,false,1},{"","Condition","",0,7,0,false,1},{"","Protect","",0,1,0,false,1}};
        for(int f=0;f<6;++f){auto p=fields[f];p.id=probabilityKey(s,16+f);grid.parameters.push_back(std::move(p));}}
    Panel orbit{"seq.orbit","Orbit rhythm sequencer","Four independent Euclidean rings / separate note and gate outputs","sequence_model",{
        {"enabled","Run","",0,1,1,false,1},{"seed","Seed","",1,65535,317,false,1},{"fill","Fill","",0,1,0,false,1}}};
    for(int ring=0;ring<4;++ring){const int lengths[]={16,13,9,7},hits[]={5,4,3,2},pitches[]={36,55,67,74};
        const Parameter fields[]={{"","Steps","",1,32,float(lengths[ring]),false,1},{"","Hits","",0,32,float(hits[ring]),false,1},{"","Rotation","",0,31,float(ring),false,1},{"","Clock","",0,5,1,false,1},{"","Pitch","MIDI",0,127,float(pitches[ring]),false,1},
            {"","Velocity","",.01f,1,.8f,false,.01},{"","Gate","",.01f,1,.45f,false,.01},{"","Chance","",0,1,1,false,.01},{"","Ratchets","",1,8,1,false,1},{"","Swing","",0,.45f,0,false,.01},{"","Mute","",0,1,0,false,1},{"","Solo","",0,1,0,false,1},
            {"","Accent every","steps",0,32,4,false,1},{"","Accent amount","",0,1,.35f,false,.01},{"","Rotate per cycle","steps",-4,4,0,false,1}};
        for(int f=0;f<orbitStride;++f){auto p=fields[f];p.id=orbitKey(ring,f);orbit.parameters.push_back(std::move(p));}}
    return {std::move(grid),std::move(orbit)};
}
void referenceSequencerPorts(const std::string& id,ModuleType& type){
    if(id=="seq.probability")type.outputs={{"notes",SignalType::Event},{"gate",SignalType::Control},{"pitch",SignalType::Control}};
    if(id=="seq.orbit"){type.outputs.clear();for(int r=1;r<=4;++r){const auto pre="r"+std::to_string(r)+"_";type.outputs.push_back({pre+"notes",SignalType::Event});type.outputs.push_back({pre+"gate",SignalType::Control});type.outputs.push_back({pre+"accent",SignalType::Control});}type.outputs.push_back({"all_notes",SignalType::Event});}
}
namespace {
uint32_t nextRandom(uint32_t& s) noexcept{s^=s<<13;s^=s>>17;s^=s<<5;return s;}
float randomUnit(uint32_t& s) noexcept{return float(nextRandom(s)>>8)/16777216.f;}
int mod(int64_t n,int span) noexcept{return int((n%span+span)%span);}
struct ReferenceModule:AudioModule {
    const Panel& spec;std::array<float,maxParameters> p{};
    explicit ReferenceModule(const NodeSpec& n):spec(panel(n.type)){
        controls->referenceSequence=std::make_shared<ReferenceSequenceState>();
        for(size_t i=0;i<spec.parameters.size();++i)controls->values[i]=spec.parameters[i].initial;
        for(const auto& saved:n.parameters){const auto it=std::find_if(spec.parameters.begin(),spec.parameters.end(),[&](const Parameter& v){return v.id==saved.first;});if(it==spec.parameters.end()||!std::isfinite(saved.second)||saved.second<it->minimum||saved.second>it->maximum)throw std::invalid_argument("Invalid reference sequencer parameter");controls->values[size_t(it-spec.parameters.begin())]=saved.second;}
    }
    void read(const AudioBlock& b) noexcept{for(size_t k=0;k<spec.parameters.size();++k){const auto& d=spec.parameters[k];float v=b.parameters?b.parameters[k]:controls->values[k].load();v=std::isfinite(v)?std::clamp(v,d.minimum,d.maximum):d.initial;p[k]=d.step>=1?std::round(v):v;}}
};
class Probability final:public ReferenceModule {
public:
    explicit Probability(const NodeSpec& n):ReferenceModule(n){}
    void process(const AudioBlock& b) noexcept override{
        if(!b.clock||!b.events)return;read(b);auto& display=*controls->referenceSequence;
        const int last=int(p[PrLength])-1,first=std::min(last,int(p[PrFirst])-1),length=last-first+1;
        const double division=sequenceDivision(int(p[PrDivision]));
        for(int i=0;i<b.frames;++i){const auto& time=b.clock[i];auto release=[&]{if(held){b.events->push({uint32_t(i),1,held,NoteEventKind::Off,pitch,0});held=0;}};
            const uint64_t request=display.reset.load();
            if(request!=reset||first!=oldFirst||last!=oldLast||int(p[PrSeed])!=oldSeed){release();pending=0;origin=time.beat;grid=GridClock{};reset=request;oldFirst=first;oldLast=last;oldSeed=int(p[PrSeed]);}
            grid.division(division);auto local=time;local.beat=time.beat-origin;const auto pulse=grid.consume(local);
            if(pulse.reset){release();pending=0;rng=uint32_t(p[PrSeed]);count=0;for(auto& row:display.outcomes)row=-2;}
            if(!time.playing||p[PrEnabled]<.5f){release();pending=0;display.heads[0]=-1;controls->playhead=-1;display.fired[0]=false;}
            else {
                if(held&&time.beat+1e-10>=offBeat)release();
                if(pulse.tick){int position=mod(pulse.index,length);const int mode=int(p[PrTraversal]);int cycle=length;
                    if(mode==1)position=length-1-position;
                    if(mode==2){cycle=std::max(1,length*2-2);const int n=mod(pulse.index,cycle);position=n<length?n:cycle-n;}
                    if(mode==3)position=int(nextRandom(rng)%uint32_t(length));
                    const int step=first+position;const uint64_t pass=count++/uint64_t(cycle);display.heads[0]=step;display.passes[0]=int(pass);controls->playhead=step;
                    uint32_t fixed=uint32_t(p[PrSeed])^(uint32_t(step+1)*0x9e3779b9u);if(!fixed)fixed=1;auto& random=p[PrRepeat]>.5f?fixed:rng;
                    const float chance=randomUnit(random);const float pick=randomUnit(random);float total=0;for(int row=0;row<16;++row)if(p[probabilityMute(row)]<.5f)total+=p[probabilityIndex(step,row)];
                    const int condition=int(p[probabilityIndex(step,PrStepCondition)]);bool allowed=condition==0||(condition==1&&p[PrFill]>.5f)||(condition==2&&p[PrFill]<.5f)||(condition==3&&pass%2==0)||(condition==4&&pass%2==1)||(condition>=5&&pass%3==uint64_t(condition-5));
                    int row=-1;if(total>0&&allowed&&chance<p[probabilityIndex(step,PrStepChance)]*p[PrDensity]){float target=pick*total;for(int r=0;r<16;++r){const float w=p[probabilityMute(r)]>.5f?0:p[probabilityIndex(step,r)];if(w>0&&target<w){row=r;break;}target-=w;}}
                    display.outcomes[step]=row;display.fired[0]=row>=0;controls->stepFired=row>=0;
                    if(row<0){release();pending=0;}else{nextPitch=probabilityPitch(row,int(p[PrScale]),int(p[PrRoot]),int(p[PrOctave]),int(p[PrTranspose]));pending=int(p[probabilityIndex(step,PrStepRatchet)]);velocity=p[PrVelocity]*p[probabilityIndex(step,PrStepVelocity)];const double delay=mod(pulse.index,2)?division*p[PrSwing]:0;nextBeat=time.beat+delay;interval=(division-delay)/pending;duration=interval*p[PrGate]*p[probabilityIndex(step,PrStepGate)];}
                }
                if(pending&&time.beat+1e-10>=nextBeat){release();pitch=nextPitch;held=++identity;b.events->push({uint32_t(i),1,held,NoteEventKind::On,pitch,velocity});offBeat=nextBeat+duration;nextBeat+=interval;--pending;}
            }
            if(b.outputs){b.outputs[1][i]=held?1:0;b.outputs[2][i]=float(pitch)/127;}
            if(b.events->failed()){held=0;pending=0;display.fired[0]=false;}
        }
    }
private:
    GridClock grid;uint32_t rng=1;uint64_t identity=0,held=0,count=0,reset=0;int pitch=48,nextPitch=48,pending=0,oldFirst=-1,oldLast=-1,oldSeed=-1;
    double origin=0,nextBeat=0,offBeat=0,interval=0,duration=0;float velocity=.8f;
};
class Orbit final:public ReferenceModule {
    struct Ring {GridClock grid;uint64_t held=0,identity=0;uint32_t random=1;int pitch=48,nextPitch=48,pending=0;double next=0,off=0,interval=0,duration=0;float velocity=.8f,accent=0;};
public:
    explicit Orbit(const NodeSpec& n):ReferenceModule(n){}
    void process(const AudioBlock& b) noexcept override{
        if(!b.clock||!b.eventOutputs)return;read(b);auto& display=*controls->referenceSequence;
        const bool solo=p[orbitIndex(0,OrSolo)]>.5f||p[orbitIndex(1,OrSolo)]>.5f||p[orbitIndex(2,OrSolo)]>.5f||p[orbitIndex(3,OrSolo)]>.5f;
        for(int i=0;i<b.frames;++i){const auto& time=b.clock[i];const auto request=display.reset.load();const bool resetAll=request!=reset||seed!=int(p[OrSeed]);if(resetAll){reset=request;seed=int(p[OrSeed]);origin=time.beat;}
            for(int r=0;r<4;++r){auto& ring=rings[r];auto* bus=b.eventOutputs[r*3];auto* all=b.eventOutputs[12];if(!bus)continue;
                auto emit=[&](NoteEventKind kind){const NoteEvent e{uint32_t(i),uint32_t(r+1),ring.held,kind,ring.pitch,kind==NoteEventKind::On?ring.velocity:0};bus->push(e);if(all)all->push(e);};
                auto release=[&]{if(ring.held){emit(NoteEventKind::Off);ring.held=0;}};
                if(resetAll){release();ring.pending=0;ring.grid=GridClock{};}
                const int k=orbitIndex(r,0),length=int(p[k+OrLength]);const double division=sequenceDivision(int(p[k+OrDivision]));ring.grid.division(division);auto local=time;local.beat=time.beat-origin;const auto pulse=ring.grid.consume(local);
                if(pulse.reset){release();ring.pending=0;ring.random=(uint32_t(p[OrSeed])+uint32_t(r)*997)|1u;}
                const bool running=time.playing&&p[OrEnabled]>.5f;
                const bool audible=running&&p[k+OrMute]<.5f&&(!solo||p[k+OrSolo]>.5f);
                if(!audible){release();ring.pending=0;display.fired[r]=false;}
                if(!running)display.heads[r]=-1;
                else if(pulse.tick){const int step=mod(pulse.index,length);const int pass=int(std::max<int64_t>(0,pulse.index)/length);const int rotation=mod(int(p[k+OrRotation])+int64_t(pass)*int(p[k+OrDrift]),length);
                    display.heads[r]=step;display.rotations[r]=rotation;display.passes[r]=pass;
                    const float chance=randomUnit(ring.random);const bool hit=euclideanHit(step,length,p[OrFill]>.5f?length:int(p[k+OrHits]),rotation)&&chance<p[k+OrChance];display.fired[r]=audible&&hit;
                    if(audible&&hit){ring.nextPitch=int(p[k+OrPitch]);const int every=int(p[k+OrAccentEvery]);ring.accent=every>0&&step%every==0?p[k+OrAccentAmount]:0;ring.velocity=std::clamp(p[k+OrVelocity]*(1+ring.accent),.01f,1.f);ring.pending=int(p[k+OrRatchet]);const double delay=mod(pulse.index,2)?division*p[k+OrSwing]:0;ring.next=time.beat+delay;ring.interval=(division-delay)/ring.pending;ring.duration=ring.interval*p[k+OrGate];}
                }
                if(audible){if(ring.held&&time.beat+1e-10>=ring.off)release();if(ring.pending&&time.beat+1e-10>=ring.next){release();ring.pitch=ring.nextPitch;ring.held=++ring.identity;emit(NoteEventKind::On);ring.off=ring.next+ring.duration;ring.next+=ring.interval;--ring.pending;}}
                if(b.outputs){b.outputs[r*3+1][i]=ring.held?1:0;b.outputs[r*3+2][i]=ring.held?ring.accent:0;}
                if(bus->failed()||(all&&all->failed())){ring.held=0;ring.pending=0;display.fired[r]=false;}
            }
        }
    }
private:std::array<Ring,4> rings;uint64_t reset=0;int seed=-1;double origin=0;
};
}
std::unique_ptr<AudioModule> makeReferenceSequencer(const NodeSpec& n){if(n.type=="seq.probability")return std::make_unique<Probability>(n);return std::make_unique<Orbit>(n);}
}
