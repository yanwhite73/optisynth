#include "../src/modular/timing.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

static bool countAllocations=false;
static size_t allocations=0;
void* operator new(std::size_t n) {
    if(countAllocations)++allocations;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }

using namespace optisynth;
namespace {
int checks=0;
void check(bool ok,const char* why) { ++checks;if(!ok)throw std::runtime_error(why); }
bool close(double a,double b) { return std::abs(a-b)<1e-9; }
NoteEvent on(uint32_t source,uint64_t id,int pitch=60,uint32_t offset=0) {
    return {offset,source,id,NoteEventKind::On,pitch,.8f};
}
NoteEvent off(uint32_t source,uint64_t id,uint32_t offset=0) {
    return {offset,source,id,NoteEventKind::Off,0,0};
}
NoteEvent reset(uint32_t source=allNoteSources,uint32_t offset=0) {
    return {offset,source,0,NoteEventKind::Reset,0,0};
}

// This harness groups samples into host-sized chunks, while transport commands
// have absolute sample timestamps. A later adapter must split at those commands.
std::vector<uint64_t> schedule(int blockSize) {
    SampleClock clock;GridClock grid;clock.play(true);
    std::vector<uint64_t> ticks;
    for(int first=0;first<480000;first+=blockSize){
        const int end=std::min(480000,first+blockSize);
        for(int sample=first;sample<end;++sample){
            if(sample==33333)clock.tempo(137.25);
            if(sample==77777)clock.meter(7,8);
            if(sample==100001)clock.seek(-.125);
            if(sample==130003)clock.loop(true,0,1.75);
            if(sample==230007)clock.play(false);
            if(sample==240011)clock.play(true);
            const auto f=clock.next();
            if(grid.consume(f).tick)ticks.push_back(f.sample);
        }
    }
    return ticks;
}
void clocks() {
    bool rejected=false;
    try{SampleClock bad(std::numeric_limits<double>::quiet_NaN());}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Nonfinite sample rate rejected");
    SampleClock c;GridClock g;
    check(!c.tempo(0)&&!c.tempo(std::numeric_limits<double>::infinity()),"Invalid tempo rejected");
    check(!c.meter(0,4)&&!c.meter(4,3)&&c.meter(7,8),"Meter validation");
    check(!c.seek(std::numeric_limits<double>::quiet_NaN())&&!c.loop(true,2,1),"Invalid seek and loop rejected");
    check(!g.division(0)&&!g.division(100)&&!g.division(std::numeric_limits<double>::quiet_NaN()),"Invalid divisions rejected");
    auto f=c.next();check(!f.playing&&f.sample==0&&f.beat==0&&close(f.beatsPerBar,3.5),"Stopped transport and 7/8 units");
    c.seek(-.25);c.play(true);f=c.next();auto p=g.consume(f);
    check(f.changes==(ClockStart|ClockSeek)&&p.reset&&p.tick&&p.index==-1&&close(f.barStart,-3.5),"Negative preroll and simultaneous seek/start");
    f=c.next();check(f.changes==0&&!g.consume(f).tick,"Discontinuity consumed once");
    c.seek(7);f=c.next();check(close(f.barStart,7)&&g.consume(f).tick,"Seek to exact grid emits once");
    c.seek(7.1);f=c.next();p=g.consume(f);check(p.reset&&!p.tick,"Seek between grid lines waits");
    c.play(false);f=c.next();const double stopped=f.beat;const auto stoppedSample=f.sample;
    check(f.changes==ClockStop&&!g.consume(f).tick,"Stop emits reset without attack");
    for(int i=0;i<1000;++i)f=c.next();
    check(f.sample==stoppedSample+1000&&f.beat==stopped,"Render counter advances while musical time freezes");
    c.play(true);f=c.next();check(f.beat==stopped&&f.changes==ClockStart&&!g.consume(f).tick,"Resume keeps phase");
    c.meter(3,4);c.seek(6.2);f=c.next();check(close(f.beatsPerBar,3)&&close(f.barStart,6),"Three-quarter bar boundaries");

    SampleClock phase;phase.play(true);
    for(int i=0;i<12000;++i)phase.next();
    phase.tempo(60);f=phase.next();check(close(f.beat,.5)&&f.changes==0,"Tempo change preserves phase without reset");
    for(int i=0;i<47999;++i)phase.next();
    f=phase.next();check(close(f.beat,1.5),"New tempo controls future samples");

    SampleClock loop;GridClock loops;loop.loop(true,0,1);loop.play(true);
    size_t loopCount=0,ticks=0;bool correct=true;
    for(int i=0;i<72001;++i){f=loop.next();p=loops.consume(f);ticks+=p.tick;
        if(f.changes&ClockLoop){++loopCount;correct&=i%24000==0&&f.beat==0&&p.reset&&p.tick;}}
    check(loopCount==3&&correct&&ticks==13,"Loop wraps and ticks exactly once per boundary");
    SampleClock fractional;fractional.tempo(137);fractional.loop(true,0,.75);fractional.play(true);
    double last=-1;bool bounded=true;int wraps=0;
    for(int i=0;i<480000;++i){f=fractional.next();bounded&=f.beat>=0&&f.beat<.75;
        if(f.changes&ClockLoop){++wraps;bounded&=f.beat<137./(60*48000);}
        else bounded&=f.beat>=last;last=f.beat;}
    check(bounded&&wraps==30,"Fractional loop retains sub-sample overshoot");

    SampleClock shared;GridClock sixteenth,sevenths;sevenths.division(1./7);shared.play(true);
    int a=0,b=0;
    for(int i=0;i<24000;++i){f=shared.next();a+=sixteenth.consume(f).tick;b+=sevenths.consume(f).tick;}
    check(a==4&&b==7,"Independent divisions share one transport");
    const auto reference=schedule(1);
    for(int block:{17,24,64,511,1024})check(schedule(block)==reference,"Absolute events independent of block grouping");

    SampleClock longRun;GridClock grid;longRun.tempo(123.45);longRun.play(true);
    uint64_t count=0;bool accurate=true;
    for(int i=0;i<48000*60;++i){f=longRun.next();p=grid.consume(f);
        if(p.tick){const double expected=count*.25*60*48000/123.45;accurate&=double(f.sample)>=expected-1e-6&&double(f.sample)<expected+1.000001;++count;}}
    check(count==494&&accurate,"Minute-long noninteger-tempo grid has less than one sample quantisation error");
}
void events() {
    NoteEventBlock b;b.begin(64);HeldNotes notes;
    check(b.push(on(2,1,60,12))&&b.push(on(1,1,60,12))&&b.push(off(1,1,12))&&b.push(reset(1,12)),"Accept same-sample event batch");
    auto e=b.begin();
    check(e[0].kind==NoteEventKind::Reset&&e[1].kind==NoteEventKind::Off&&e[2].source==1&&e[3].source==2,"Reset then release then attack; stable source order");
    notes.beginBlock(b);for(const auto& n:b)notes.apply(n);
    check(notes.size()==2&&notes.newest()->source==2,"Same pitch from two sources stays independent");
    check(notes.apply(off(1,1))==NoteChange::Released&&notes.size()==1&&notes.newest()->source==2,"Releasing one source preserves the other");
    check(notes.apply(off(1,1))==NoteChange::Ignored&&notes.size()==1,"Repeated/unknown release harmless");
    check(notes.apply(on(2,1,72))==NoteChange::Updated&&notes.size()==1&&notes.newest()->pitch==72,"Repeated identity updates without leaking held slot");
    notes.apply(on(2,2));notes.apply(on(3,1));notes.clear(2);
    check(notes.size()==1&&notes.newest()->source==3,"Removal/reset clears only its source");
    notes.apply(reset());check(notes.size()==0&&!notes.newest(),"Global reset releases all ownership");

    b.begin(64);b.push(on(1,1));
    check(!b.push(off(1,1,64))&&b.failed()&&b.size()==0&&!b.push(on(1,2)),"Out-of-block release fails entire batch closed");
    notes.apply(on(1,1));notes.beginBlock(b);
    check(notes.size()==0&&notes.apply(on(1,2))==NoteChange::Ignored,"Failed batch clears existing notes and suppresses attacks");
    b.begin(64);notes.beginBlock(b);notes.apply(on(1,2));
    check(notes.size()==1,"Next valid block recovers");
    bool filled=true;for(size_t i=0;i<maximumNoteEvents;++i)filled&=b.push(on(1,i+1,60,uint32_t(i%64)));
    check(filled&&b.size()==maximumNoteEvents&&!b.push(off(1,2))&&b.failed()&&b.size()==0,"Event overflow cannot drop release and retain attacks");
    notes.beginBlock(b);check(notes.size()==0,"Overflow releases previously held notes");
    b.begin(64);notes.beginBlock(b);
    for(size_t i=0;i<maximumHeldNotes;++i)notes.apply(on(1,i+1));
    check(notes.size()==maximumHeldNotes&&notes.apply(on(2,1))==NoteChange::Overflow&&notes.size()==0,"Held-note capacity fails closed");
    check(notes.apply(on(2,2))==NoteChange::Ignored,"Held-note overflow stays muted until next block");
    b.begin(1);check(!b.push(on(allNoteSources,1))&&b.failed(),"Wildcard only valid for reset");
    b.begin(1);check(!b.push(on(1,0))&&b.failed(),"Zero note identity rejected");
    b.begin(1);auto bad=on(1,1);bad.velocity=std::numeric_limits<float>::quiet_NaN();
    check(!b.push(bad)&&b.failed(),"Nonfinite velocity rejected");
    b.begin(1);check(!b.push(on(1,1,128))&&b.failed(),"Out-of-range pitch rejected");
    b.begin(maximumEventBlockFrames+1);check(b.failed(),"Bound event block span");
    b.begin(0);check(!b.failed()&&b.size()==0&&!b.push(reset()),"Zero-frame blocks contain no events");

    SampleClock clock;GridClock grid;clock.play(true);b.begin(24);notes.beginBlock(b);
    allocations=0;countAllocations=true;
    for(int i=0;i<48000;++i){
        auto f=clock.next();auto pulse=grid.consume(f);
        if(i%24==0){b.begin(24);notes.beginBlock(b);}
        if(pulse.tick){b.push(off(7,1,uint32_t(i%24)));b.push(on(7,1,60,uint32_t(i%24)));}
        for(const auto& event:b)notes.apply(event);
        if(i==36000){clock.tempo(90);clock.seek(0);notes.clear(7);}
    }
    countAllocations=false;
    check(allocations==0,"Clock, event ordering and note ownership allocate nothing in playback");
}
}
int main() {
    try{clocks();events();std::printf("%d timing/event checks passed\n",checks);return 0;}
    catch(const std::exception& e){countAllocations=false;std::fprintf(stderr,"FAIL after %d checks: %s\n",checks,e.what());return 1;}
}
