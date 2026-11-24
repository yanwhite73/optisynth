#include "performance.h"
#include "pattern.h"
#include "sequencer.h"
#include <stdexcept>

namespace optisynth {
PerformanceData defaultPerformance(){
    PerformanceData d;
    const char* names[]={"Bass","Pulse","Glass","Percussion"};
    constexpr int melody[]={0,7,12,10,3,7,15,10};
    for(int t=0;t<4;++t){d.tracks[t].name=names[t];
        for(int p=0;p<8;++p){auto& pattern=d.tracks[t].patterns[p];
            pattern.last=(t==0?16:t==1?13:t==2?7:16)-1;
            pattern.division=t==2?2:1;pattern.swing=t==1?.08f:0;
            for(int s=0;s<64;++s){auto& step=pattern.steps[s];
                step.pitch=(t==0?36:t==1?48:t==2?60:36)+melody[(s+p)%8]+(p>=4?5:0);
                step.on=t==3?(s%4==0||(p%2&&s%8==7)):s%8!=6;
                step.velocity=s%4==0?.95f:.6f;step.gate=t==0?.65f:.35f;
                step.cv=float(std::sin((s+p)*.71)*.65);
                if(p>0&&s%8==7){step.ratchets=2+p%3;step.chance=.75f;}
                if(t==0&&s%8==2){step.tie=true;pattern.steps[(s+1)%64].slide=.12f;}
                if(t==2&&s%7==5)step.condition=8;
            }
        }
    }
    for(int row=0;row<16;++row){d.chain[row].bars=2;for(int t=0;t<4;++t)d.chain[row].patterns[t]=(row+t/2)%4;}
    return d;
}
void validatePerformance(const PerformanceData& d){
    auto range=[](int v,int lo,int hi){if(v<lo||v>hi)throw std::invalid_argument("Performance integer outside range");};
    auto real=[](float v,float lo,float hi){if(!std::isfinite(v)||v<lo||v>hi)throw std::invalid_argument("Performance value outside range");};
    range(d.chainFirst,0,15);range(d.chainLast,d.chainFirst,15);
    if(!d.seed)throw std::invalid_argument("Performance seed must be nonzero");
    for(const auto& track:d.tracks){if(track.name.size()>48)throw std::invalid_argument("Track name too long");range(track.initial,0,7);
        for(const auto& p:track.patterns){range(p.first,0,63);range(p.last,p.first,63);range(p.division,0,5);range(p.traversal,0,5);range(p.rotation,-63,63);range(p.scale,0,6);range(p.root,0,11);real(p.swing,0,.45f);
            for(const auto& s:p.steps){range(s.pitch,0,127);range(s.ratchets,1,8);range(s.condition,0,14);real(s.velocity,.01f,1);real(s.gate,.01f,1);real(s.chance,0,1);real(s.cv,-1,1);real(s.slide,0,2);}}
    }
    for(const auto& row:d.chain){range(row.bars,1,16);for(int p:row.patterns)range(p,0,7);}
}
PerformanceExchange::PerformanceExchange(const PerformanceData& data):current_(nullptr){
    validatePerformance(data);current_=new PerformanceData(data);
    for(int t=0;t<4;++t){playing[t]=data.tracks[t].initial;queued[t]=-1;steps[t]=-1;passes[t]=0;}
}
PerformanceExchange::~PerformanceExchange(){delete current_;delete pending_.load();delete retired_.load();}
void PerformanceExchange::publish(const PerformanceData& data){
    validatePerformance(data);auto next=std::make_unique<PerformanceData>(data);
    delete retired_.exchange(nullptr);delete pending_.exchange(next.release());
}
void PerformanceExchange::collect() noexcept {delete retired_.exchange(nullptr);}
const PerformanceData& PerformanceExchange::consume() noexcept {
    if(!retired_.load())if(auto* next=pending_.exchange(nullptr)){auto* old=current_;current_=next;retired_.store(old);}
    return *current_;
}
void PerformanceExchange::launch(int t,int p,bool immediate) noexcept {if(t>=0&&t<4&&p>=0&&p<8){auto n=requests[t].load();requests[t]=((n>>8)+1)*256+uint64_t(p+1)+(immediate?128:0);}}
void PerformanceExchange::cancel(int t) noexcept {if(t>=0&&t<4){auto n=requests[t].load();requests[t]=((n>>8)+1)*256;}}
void PerformanceExchange::reset(int t) noexcept {if(t>=0&&t<4)resets[t].fetch_add(1);}
Panel performancePanel(){
    Panel p{"seq.performance","Performance sequencer","Four tracks / eight patterns / phrase chains","performance",{
        {"enabled","Run","",0,1,1,false,1},{"transpose","Master transpose","st",-36,36,0,false,1},
        {"fill","Fill","",0,1,0,false,1},{"chain","Play chain","",0,1,0,false,1},
        {"launch","Launch boundary","",0,3,2,false,1}}};
    for(int t=0;t<4;++t){const auto prefix="track"+std::to_string(t+1)+"_";
        p.parameters.push_back({prefix+"mute","Mute","",0,1,0,false,1});
        p.parameters.push_back({prefix+"solo","Solo","",0,1,0,false,1});
        p.parameters.push_back({prefix+"transpose","Transpose","st",-36,36,0,false,1});
        p.parameters.push_back({prefix+"velocity","Velocity","",.01f,1,1,false,.01});}
    return p;
}
namespace {
uint32_t randomValue(uint32_t& s) noexcept {s^=s<<13;s^=s>>17;s^=s<<5;return s;}
int wrap(int64_t n,int span) noexcept {return int((n%span+span)%span);}
bool condition(int id,uint64_t pass,bool fill,bool& previous) noexcept {
    if(id<7){const bool values[]={true,fill,!fill,previous,!previous,pass==0,pass>0};const bool result=values[id];if(id==1||id==2||id==5||id==6)previous=result;return result;}
    const int n=id-7,period=n<2?2:n<5?3:4,position=n<2?n:n<5?n-2:n==7?3:n-5;
    previous=int(pass%uint64_t(period))==position;return previous;
}
class Performance final:public AudioModule {
    struct Track {
        GridClock grid;
        int pattern=0,queued=-1,pitch=48,targetPitch=48,pending=0,walk=-1;
        int oldFirst=-1,oldLast=-1,oldDivision=-1,oldTraversal=-1;
        uint64_t identity=0,held=0,count=0,request=0,reset=0;
        uint32_t random=1,seed=1;
        double origin=0,launchAt=0,offAt=0,nextAt=0,interval=0,duration=0,glideAt=0;
        float velocity=.8f,cv=0,pitchValue=48,glideStep=0;
        bool previous=false,tie=false,pitchReady=false;
    };
public:
    explicit Performance(const NodeSpec& n):definition(panel("seq.performance")){
        const auto initial=n.performance?*n.performance:defaultPerformance();
        controls->performance=std::make_shared<PerformanceExchange>(initial);
        for(size_t i=0;i<definition.parameters.size();++i)controls->values[i]=definition.parameters[i].initial;
        for(const auto& saved:n.parameters){auto it=std::find_if(definition.parameters.begin(),definition.parameters.end(),[&](const Parameter& p){return p.id==saved.first;});
            if(it==definition.parameters.end()||!std::isfinite(saved.second)||saved.second<it->minimum||saved.second>it->maximum)throw std::invalid_argument("Invalid performance parameter");
            controls->values[size_t(it-definition.parameters.begin())]=saved.second;}
        for(int t=0;t<4;++t)tracks[t].pattern=initial.tracks[t].initial;
    }
    void process(const AudioBlock& b) noexcept override {
        if(!b.clock||!b.eventOutputs)return;
        auto& display=*controls->performance;const auto& data=display.consume();
        std::array<float,PerfGlobalCount+4*PerfTrackControlCount> values{};
        for(size_t k=0;k<values.size();++k){const auto& p=definition.parameters[k];const float v=b.parameters?b.parameters[k]:controls->values[k].load();values[k]=std::isfinite(v)?std::clamp(v,p.minimum,p.maximum):p.initial;}
        const bool solo=values[performanceIndex(0,PerfSolo)]>.5f||values[performanceIndex(1,PerfSolo)]>.5f||values[performanceIndex(2,PerfSolo)]>.5f||values[performanceIndex(3,PerfSolo)]>.5f;
        for(int i=0;i<b.frames;++i){const auto& time=b.clock[i];
            const bool changed=!seen||time.revision!=revision;
            const bool discontinuity=changed&&(!seen||(time.changes&(ClockSeek|ClockLoop|ClockStart)));
            seen=true;revision=time.revision;
            if(discontinuity){chainStarted=false;chainHasRow=false;for(int t=0;t<4;++t){tracks[t].seed=(data.seed+uint32_t(t)*997)|1u;resetTrack(t,time.beat,b,i);tracks[t].queued=-1;display.queued[t]=-1;}}
            const bool running=time.playing&&values[PerfEnabled]>.5f;
            if(running&&values[PerfChain]>.5f){
                if(!chainStarted||chainPosition<data.chainFirst||chainPosition>data.chainLast){chainPosition=data.chainFirst;chainNext=time.beat;chainStarted=true;}
                if(time.beat+1e-10>=chainNext){if(chainHasRow){chainPosition=chainPosition>=data.chainLast?data.chainFirst:chainPosition+1;}
                    const auto& row=data.chain[size_t(chainPosition)];
                    for(int t=0;t<4;++t){resetTrack(t,time.beat,b,i);tracks[t].pattern=row.patterns[t];}
                    chainNext=time.beat+row.bars*time.beatsPerBar;chainHasRow=true;}
                display.chainRow=chainPosition;display.chainRemaining=std::max(0.,chainNext-time.beat);
            }else {chainStarted=false;chainHasRow=false;display.chainRow=-1;display.chainRemaining=0;}
            for(int t=0;t<4;++t){auto& track=tracks[t];auto* bus=b.eventOutputs[t*4];if(!bus)continue;
                const auto request=display.requests[t].load();
                if(request!=track.request){track.request=request;track.queued=int(request&127)-1;const int boundary=int(std::round(values[PerfLaunch]));
                    const double span=boundary==1?1.:boundary==2?time.beatsPerBar:time.beatsPerBar*2;
                    track.launchAt=boundary==0||!running||(request&128)?time.beat:(std::floor((time.beat+1e-10)/span)+1)*span;display.queued[t]=track.queued;}
                if(track.queued>=0&&(!running||time.beat+1e-10>=track.launchAt)){const int target=track.queued;resetTrack(t,time.beat,b,i);track.pattern=target;track.queued=-1;display.queued[t]=-1;}
                const auto reset=display.resets[t].load();if(reset!=track.reset){track.reset=reset;resetTrack(t,time.beat,b,i);}
                const auto& pattern=data.tracks[t].patterns[size_t(track.pattern)];
                if(pattern.first!=track.oldFirst||pattern.last!=track.oldLast||pattern.division!=track.oldDivision||pattern.traversal!=track.oldTraversal){
                    resetTrack(t,time.beat,b,i);track.oldFirst=pattern.first;track.oldLast=pattern.last;track.oldDivision=pattern.division;track.oldTraversal=pattern.traversal;}
                display.playing[t]=track.pattern;
                const double division=sequenceDivision(pattern.division);track.grid.division(division);
                auto local=time;local.beat=time.beat-track.origin;
                const auto pulse=track.grid.consume(local);
                const bool audible=running&&values[performanceIndex(t,PerfMute)]<.5f&&(!solo||values[performanceIndex(t,PerfSolo)]>.5f);
                if(!audible){release(track,*bus,i,t);track.pending=0;track.tie=false;display.fired[t]=false;}
                if(!running){display.steps[t]=-1;}
                else if(pulse.tick){
                    const int length=pattern.last-pattern.first+1,mode=pattern.traversal;const int64_t tick=pulse.index;
                    int position=wrap(tick,length),cycle=length;
                    if(mode==1)position=length-1-position;
                    if(mode==2){cycle=length*2;const int n=wrap(tick,cycle);position=n<length?n:cycle-1-n;}
                    if(mode==3){cycle=std::max(1,length*2-2);const int n=wrap(tick,cycle);position=n<length?n:cycle-n;}
                    if(mode==4)position=int(randomValue(track.random)%uint32_t(length));
                    if(mode==5){track.walk=track.walk<0?0:wrap(track.walk+(randomValue(track.random)&1?1:-1),length);position=track.walk;}
                    const int step=pattern.first+wrap(position+pattern.rotation,length);const uint64_t pass=track.count++/uint64_t(cycle);
                    display.steps[t]=step;display.passes[t]=int(pass);const auto& event=pattern.steps[size_t(step)];track.cv=event.cv;
                    const bool chance=float(randomValue(track.random)>>8)/16777216.f<event.chance;
                    const bool fire=event.on&&chance&&condition(event.condition,pass,values[PerfFill]>.5f,track.previous);
                    display.fired[t]=audible&&fire;
                    if(audible){
                        if(!fire){release(track,*bus,i,t);track.pending=0;track.tie=false;}
                        else {
                            const bool legato=track.held&&track.tie;
                            track.targetPitch=patternQuantize(event.pitch+int(std::round(values[PerfTranspose]+values[performanceIndex(t,PerfTrackTranspose)])),pattern.scale,pattern.root);
                            track.velocity=event.velocity*values[performanceIndex(t,PerfVelocity)];
                            track.glideStep=event.slide>0?std::abs(float(track.targetPitch)-track.pitchValue)/(48000*event.slide):127;
                            track.tie=event.tie;
                            const double delay=wrap(tick,2)?division*pattern.swing:0;
                            track.interval=(division-delay)/(event.tie||legato?1:event.ratchets);track.duration=track.interval*event.gate;
                            track.nextAt=time.beat+delay;track.glideAt=track.nextAt;
                            if(legato){track.pending=0;track.offAt=track.tie?std::numeric_limits<double>::infinity():track.nextAt+track.duration;}
                            else {track.pending=event.tie?1:event.ratchets;}
                        }
                    }
                }
                if(audible){
                    if(track.held&&time.beat+1e-10>=track.offAt)release(track,*bus,i,t);
                    if(track.pending&&time.beat+1e-10>=track.nextAt){
                        release(track,*bus,i,t);track.held=++track.identity;track.pitch=track.targetPitch;
                        bus->push({uint32_t(i),uint32_t(t+1),track.held,NoteEventKind::On,track.pitch,track.velocity});
                        track.offAt=track.tie?std::numeric_limits<double>::infinity():track.nextAt+track.duration;track.nextAt+=track.interval;--track.pending;
                    }
                }
                if(!track.pitchReady){track.pitchValue=float(track.targetPitch);track.pitchReady=true;}
                else if(time.beat+1e-10>=track.glideAt)track.pitchValue+=std::clamp(float(track.targetPitch)-track.pitchValue,-track.glideStep,track.glideStep);
                if(b.outputs){b.outputs[t*4+1][i]=track.held?1.f:0.f;b.outputs[t*4+2][i]=(track.pitchValue-track.pitch)/48.f;b.outputs[t*4+3][i]=track.cv;}
                if(bus->failed()){track.held=0;track.pending=0;track.tie=false;display.fired[t]=false;}
            }
        }
        if(auto* all=b.eventOutputs[performanceAllNotes]) {
            for(int t=0;t<performanceTracks;++t) {
                const auto* bus=b.eventOutputs[t*4];
                if(!bus)continue;
                if(bus->failed()){all->fail();break;}
                for(const auto& event:*bus)all->push(event);
            }
        }
    }
private:
    const Panel& definition;std::array<Track,4> tracks;
    uint64_t revision=0;bool seen=false,chainStarted=false,chainHasRow=false;int chainPosition=0;double chainNext=0;
    static void release(Track& track,NoteEventBlock& bus,int offset,int t) noexcept {if(track.held){bus.push({uint32_t(offset),uint32_t(t+1),track.held,NoteEventKind::Off,track.pitch,0});track.held=0;}}
    void resetTrack(int t,double beat,const AudioBlock& b,int i) noexcept {
        auto& track=tracks[t];if(b.eventOutputs[t*4])release(track,*b.eventOutputs[t*4],i,t);
        track.pending=0;track.origin=beat;track.grid=GridClock{};track.count=0;track.walk=-1;track.previous=false;track.tie=false;track.pitchReady=false;track.random=track.seed;
    }
};
}
std::unique_ptr<AudioModule> makePerformance(const NodeSpec& n){return std::make_unique<Performance>(n);}
}
