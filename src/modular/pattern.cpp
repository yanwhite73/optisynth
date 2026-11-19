#include "pattern.h"
#include "sequencer.h"

namespace optisynth {
Panel patternPanel(){
    Panel p{"seq.pattern","Pattern / Motion","64 steps / conditions / ratchets / independent lanes","sequencer",{
        {"enabled","Enabled","",0,1,1,false,1},{"division","Division","",0,5,1,false,1},
        {"length","Last step","",1,64,16,false,1},{"gate","Gate scale","",.05f,.95f,.6f,false,.01},
        {"velocity","Velocity scale","",.01f,1,.8f,false,.01},
        {"first_step","First step","",1,64,1,false,1},{"run_mode","Traversal","",0,5,0,false,1},
        {"rotate","Rotate","steps",-63,63,0,false,1},{"transpose","Transpose","st",-36,36,0,false,1},
        {"scale","Scale","",0,6,0,false,1},{"root","Root","",0,11,0,false,1},
        {"swing","Swing","",0,.45f,0,false,.01},{"seed","Seed","",1,65535,1,false,1},
        {"pitch_length","Pitch loop (0 = follow)","",0,64,0,false,1},
        {"velocity_length","Velocity loop (0 = follow)","",0,64,0,false,1},
        {"cv1_length","CV 1 loop (0 = follow)","",0,64,0,false,1},
        {"cv2_length","CV 2 loop (0 = follow)","",0,64,0,false,1},
        {"fill","Fill","",0,1,0,false,1},{"keyboard_transpose","Keyboard transpose","",0,1,0,false,1},
        {"cv_slew","CV glide","s",0,2,0,false,.001},
        {"density","Generate density","",0,1,.75f,false,.01},
        {"generate_range","Generate range","st",0,36,12,false,1},
        {"generate_root","Generate base","MIDI",0,100,48,false,1}}};
    constexpr int notes[]={48,55,58,60,51,55,63,58};
    for(int s=0;s<patternCapacity;++s){
        const Parameter fields[]={
            {"","Pitch","MIDI",0,127,float(notes[s%8]),false,1},
            {"","Velocity","",.01f,1,1,false,.01},{"","On","",0,1,1,false,1},
            {"","Gate","",.05f,1,1,false,.01},{"","Chance","",0,1,1,false,.01},
            {"","Ratchets","",1,8,1,false,1},{"","Condition","",0,14,0,false,1},
            {"","CV 1","",-1,1,0,false,.01},{"","CV 2","",-1,1,0,false,.01},
            {"","Lock","",0,1,0,false,1}};
        for(int f=0;f<patternStride;++f){auto d=fields[f];d.id=patternKey(s,f);p.parameters.push_back(std::move(d));}
    }
    return p;
}
namespace {
// Instance-local generator: no shared random state and no allocation in render.
uint32_t randomNext(uint32_t& state) noexcept {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int modulo(int64_t n,int d) noexcept {return int((n%d+d)%d);}
class Pattern final:public AudioModule {
public:
    explicit Pattern(const NodeSpec& node):spec(panel("seq.pattern")){
        for(size_t i=0;i<spec.parameters.size();++i)controls->values[i]=spec.parameters[i].initial;
        for(const auto& saved:node.parameters){auto it=std::find_if(spec.parameters.begin(),spec.parameters.end(),[&](const auto& p){return p.id==saved.first;});
            if(it==spec.parameters.end()||!std::isfinite(saved.second)||saved.second<it->minimum||saved.second>it->maximum)throw std::invalid_argument("Invalid Pattern parameter");
            controls->values[size_t(it-spec.parameters.begin())]=saved.second;}
        for(auto& head:controls->laneHeads)head=-1;
    }
    void process(const AudioBlock& b) noexcept override {
        if(!b.clock||!b.events)return;
        for(size_t k=0;k<spec.parameters.size();++k){const auto& d=spec.parameters[k];float v=b.parameters?b.parameters[k]:controls->values[k].load(std::memory_order_relaxed);
            v=std::isfinite(v)?std::clamp(v,d.minimum,d.maximum):d.initial;p[k]=d.step>=1?std::round(v):v;}
        const int last=int(p[PatLength])-1,first=std::min(last,int(p[PatFirst])-1),length=last-first+1,mode=int(p[PatRun]);
        const double division=sequenceDivision(int(p[PatDivision]));grid.division(division);
        if(p[PatKeyboard]&&b.note.gate)keyboard=b.note.midi-48;
        for(int i=0;i<b.frames;++i){const auto& t=b.clock[i];auto pulse=grid.consume(t);
            auto release=[&]{if(held){b.events->push({uint32_t(i),1,held,NoteEventKind::Off,heldPitch,0});held=0;}};
            if(pulse.reset||first!=oldFirst||last!=oldLast||mode!=oldMode){release();pending=0;walk=-1;count=0;prevCondition=false;random=uint32_t(p[PatSeed]);oldFirst=first;oldLast=last;oldMode=mode;}
            if(!t.playing||p[PatEnabled]<.5f){release();pending=0;controls->playhead=-1;for(auto& h:controls->laneHeads)h=-1;}
            else {
                if(held&&t.beat+1e-10>=offBeat)release();
                if(pulse.tick){
                    // Deterministic aligned traversal; random walk advances only once per grid pulse.
                    const int64_t tick=pulse.index;int position=modulo(tick,length);
                    int cycleLength=length;
                    if(mode==1)position=length-1-position;
                    if(mode==2){cycleLength=2*length;int n=modulo(tick,cycleLength);position=n<length?n:2*length-1-n;}
                    if(mode==3){cycleLength=std::max(1,2*length-2);int n=modulo(tick,cycleLength);position=n<length?n:cycleLength-n;}
                    if(mode==4)position=int(randomNext(random)%uint32_t(length));
                    if(mode==5){walk=walk<0?0:modulo(walk+(randomNext(random)&1?1:-1),length);position=walk;}
                    const int step=first+modulo(position+int(p[PatRotate]),length);
                    const uint64_t iteration=(mode>=4?count:uint64_t(std::max<int64_t>(0,tick)))/uint64_t(cycleLength);++count;
                    controls->cycle.store(int(iteration),std::memory_order_relaxed);
                    auto lane=[&](int global,int head){int span=int(p[global]);int index=span?modulo(tick,span):step;controls->laneHeads[head]=index;return index;};
                    const int pitchStep=lane(PatPitchLength,0),velocityStep=lane(PatVelocityLength,1);
                    const int c1=lane(PatCV1Length,2),c2=lane(PatCV2Length,3);
                    target[0]=p[patternIndex(c1,CV1)];target[1]=p[patternIndex(c2,CV2)];
                    controls->playhead=step;
                    const float chance=p[patternIndex(step,Chance)];const float roll=float(randomNext(random)>>8)*(1.f/16777216.f);
                    bool fire=p[patternIndex(step,On)]>.5f&&roll<chance;
                    if(fire)fire=condition(int(p[patternIndex(step,Condition)]),iteration,p[PatFill]>.5f);
                    controls->stepFired=fire;
                    // Swing offsets odd grid positions. Fit ratchets into the remaining step.
                    pending=fire?int(p[patternIndex(step,Ratchet)]):0;
                    pendingPitch=patternQuantize(int(p[patternIndex(pitchStep,Pitch)]+p[PatTranspose])+(p[PatKeyboard]?keyboard:0),int(p[PatScale]),int(p[PatRoot]));
                    pendingVelocity=p[PatVelocity]*p[patternIndex(velocityStep,Velocity)];
                    const double delay=modulo(tick,2)?division*p[PatSwing]:0;
                    nextBeat=t.beat+delay;interval=(division-delay)/std::max(1,pending);
                    duration=interval*p[PatGate]*p[patternIndex(step,Gate)];
                }
                if(pending&&t.beat+1e-10>=nextBeat){release();held=++identity;heldPitch=pendingPitch;
                    b.events->push({uint32_t(i),1,held,NoteEventKind::On,heldPitch,pendingVelocity});offBeat=nextBeat+duration;nextBeat+=interval;--pending;}
            }
            for(int c=0;c<2;++c){const float slew=p[PatSlew];cv[c]=slew>0?cv[c]+std::clamp(target[c]-cv[c],-1.f/(48000*slew),1.f/(48000*slew)):target[c];
                if(b.outputs)b.outputs[c+1][i]=cv[c];}
            if(b.events->failed()){held=0;pending=0;controls->playhead=-1;}
        }
    }
private:
    // Implemented from the documented condition truth table. No firmware code/dependencies.
    bool condition(int id,uint64_t pass,bool fill) noexcept {
        if(id<7){
            const std::array<bool,7> outcomes={true,fill,!fill,prevCondition,!prevCondition,pass==0,pass>0};
            const bool answer=outcomes[size_t(id)];
            constexpr unsigned remember=(1u<<1)|(1u<<2)|(1u<<5)|(1u<<6);
            if(remember&(1u<<id))prevCondition=answer;
            return answer;
        }
        const int ordinal=id-7;
        const int period=ordinal<2?2:ordinal<5?3:4;
        const int position=ordinal<2?ordinal:ordinal<5?ordinal-2:ordinal==7?3:ordinal-5;
        prevCondition=int(pass%uint64_t(period))==position;
        return prevCondition;
    }
    const Panel& spec;std::array<float,patternGlobals+patternCapacity*patternStride> p{};GridClock grid;
    int oldFirst=-1,oldLast=-1,oldMode=-1,walk=-1,keyboard=0,pending=0,pendingPitch=48,heldPitch=48;
    uint32_t random=1;uint64_t held=0,identity=0,count=0;bool prevCondition=false;
    double nextBeat=0,offBeat=0,interval=0,duration=0;float pendingVelocity=.8f,cv[2]{},target[2]{};
};
}
std::unique_ptr<AudioModule> makePattern(const NodeSpec& node){return std::make_unique<Pattern>(node);}
}
