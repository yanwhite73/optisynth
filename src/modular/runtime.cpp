#include "runtime.h"
#include "analog.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optisynth {
namespace {
const ModuleType& findType(const Registry& registry,const std::string& id) {
    auto r=std::find_if(registry.begin(),registry.end(),[&](const auto& t){return t.id==id;});
    if(r==registry.end())throw std::invalid_argument("Unknown module: "+id);
    return *r;
}
}

void Runtime::prepareBuffers(NodeBuffers& n,AudioModule* module,const ModuleType& r,const std::string& type) {
    n.module=module;
    if(!n.module)throw std::invalid_argument("Module has no audio implementation");
    n.lfoPhase=module->freeLfoPhase();
    n.outputs.resize(r.outputs.size());
    n.right.resize(r.outputs.size());
    n.in.assign(r.inputs.size(),zero_.data());
    n.inRight.assign(r.inputs.size(),zero_.data());
    for(auto& output:n.outputs)n.out.push_back(output.data());
    for(auto& output:n.right)n.outRight.push_back(output.data());
    for(const auto& p:r.outputs)n.channels.push_back(p.channels);
    n.parameters=panel(type).parameters;
    if(n.parameters.size()>maxParameters)throw std::invalid_argument("Too many module parameters");
    for(size_t k=0;k<n.parameters.size();++k)n.base[k]=n.module->controls->values[k].load();
    n.sliceIn.resize(n.in.size());n.sliceRight.resize(n.in.size());
    n.sliceOut.resize(n.out.size());n.sliceOutRight.resize(n.out.size());
    const auto count=std::count_if(r.outputs.begin(),r.outputs.end(),[](const Port& p){return p.type==SignalType::Event;});
    n.additionalEvents.resize(count>0?size_t(count-1):0);
    n.eventOutputs.resize(r.outputs.size(),nullptr);
    int eventIndex=0;
    for(size_t p=0;p<r.outputs.size();++p)if(r.outputs[p].type==SignalType::Event)
        n.eventOutputs[p]=eventIndex++==0?&n.events:&n.additionalEvents[size_t(eventIndex-2)];
}

Runtime::Runtime(const GraphSpec& spec,const SampleBank& samples,const SpectralBank& spectral) : spec_(spec) {
    auto registry = moduleRegistry(samples,spectral);
    const auto plan=compileGraph(spec,registry);int engines=0;
    for(int i:plan.nodes)if(spec.nodes[i].type=="spectral.engine")++engines;
    if(engines>maximumSpectralEngines)throw std::invalid_argument("Maximum two active Spectral engines per rack");
    graph_ = prepareGraph(spec, registry, {48000, quantum, Scope::Voice});
    const int count=int(graph_.modules.size());
    nodes_.resize(size_t(count));
    auto typeOf=[&](int i)->const std::string&{return spec.nodes[size_t(graph_.plan.nodes[size_t(i)])].type;};
    for(int i=0;i<count;++i)
        prepareBuffers(nodes_[size_t(i)],dynamic_cast<AudioModule*>(graph_.modules[size_t(i)].get()),findType(registry,typeOf(i)),typeOf(i));

    // Voice group: everything feeding the voice mixer through audio/control cables
    // runs once per voice, except modules that are inherently global.
    voiceNode_.assign(size_t(count),0);
    for(int i=0;i<count;++i)if(typeOf(i)=="voice.mix"){
        if(mixer_>=0)throw std::invalid_argument("Only one Voice mixer per rack is supported");
        mixer_=i;
    }
    const auto& connections=graph_.plan.connections;
    if(mixer_>=0){
        voiceMixer_=dynamic_cast<VoiceMixer*>(nodes_[size_t(mixer_)].module);
        std::vector<int> stack;
        for(const auto& c:connections)if(c.destination.node==mixer_&&c.type!=SignalType::Event)stack.push_back(c.source.node);
        while(!stack.empty()){
            const int i=stack.back();stack.pop_back();
            if(voiceNode_[size_t(i)])continue;
            if(isGlobalOnly(typeOf(i))){
                if(typeOf(i)=="spectral.engine")throw std::invalid_argument("Spectral is already polyphonic; place it after the Voice mixer");
                continue;
            }
            voiceNode_[size_t(i)]=1;
            for(const auto& c:connections)if(c.destination.node==i&&c.type!=SignalType::Event)stack.push_back(c.source.node);
        }
        for(const auto& c:connections){
            if(c.type==SignalType::Event&&voiceNode_[size_t(c.destination.node)])
                throw std::invalid_argument("Connect sequencers to the Voice mixer's notes input, not to modules inside the voice");
            if(c.type!=SignalType::Event&&voiceNode_[size_t(c.source.node)]&&!voiceNode_[size_t(c.destination.node)]&&
               !(c.destination.node==mixer_&&c.destination.port==0))
                throw std::invalid_argument("Only the Voice mixer can read individual voices");
        }
        for(const auto& m:graph_.plan.modulations)
            if(voiceNode_[size_t(m.source.node)]&&!voiceNode_[size_t(m.destination.node)])
                throw std::invalid_argument("A per-voice source cannot modulate a module after the Voice mixer");
        for(const auto& o:graph_.plan.outputs)if(voiceNode_[size_t(o.node)])
            throw std::invalid_argument("Route voices to the output through the Voice mixer");
        // Additional voices: separate DSP state, shared knob controls.
        voiceCopies_.resize(size_t(count));
        for(int i=0;i<count;++i)if(voiceNode_[size_t(i)]){
            const auto& node=spec.nodes[size_t(graph_.plan.nodes[size_t(i)])];
            const auto& type=findType(registry,node.type);
            voiceCopies_[size_t(i)].resize(maximumVoices-1);
            for(int v=1;v<maximumVoices;++v){
                PrepareContext context{48000,quantum,Scope::Voice,v};
                voiceModules_.push_back(type.create(node,context));
                auto* module=dynamic_cast<AudioModule*>(voiceModules_.back().get());
                if(!module)throw std::invalid_argument("Module has no audio implementation");
                module->controls=nodes_[size_t(i)].module->controls;
                prepareBuffers(voiceCopies_[size_t(i)][size_t(v-1)],module,type,node.type);
            }
        }
        // Order: globals feeding the voices, the voices, the mixer, then downstream globals.
        std::vector<char> downstream(size_t(count),0);
        std::vector<int> work{mixer_};
        while(!work.empty()){
            const int i=work.back();work.pop_back();
            if(downstream[size_t(i)])continue;
            downstream[size_t(i)]=1;
            for(const auto& c:connections)if(c.source.node==i)work.push_back(c.destination.node);
            for(const auto& m:graph_.plan.modulations)if(m.source.node==i)work.push_back(m.destination.node);
        }
        for(int i=0;i<count;++i)if(!downstream[size_t(i)]&&!voiceNode_[size_t(i)])order_.push_back(i);
        for(int i=0;i<count;++i)if(voiceNode_[size_t(i)])order_.push_back(i);
        for(int i=0;i<count;++i)if(downstream[size_t(i)])order_.push_back(i);
    } else for(int i=0;i<count;++i)order_.push_back(i);

    const auto voicesOf=[&](int node){return voiceNode_[size_t(node)]?maximumVoices:1;};
    for (const auto& c : connections) {
        if(c.type==SignalType::Event){
            nodes_[size_t(c.destination.node)].noteInput=nodes_[size_t(c.source.node)].eventOutputs[size_t(c.source.port)];continue;
        }
        for(int v=0;v<voicesOf(c.destination.node);++v){
            auto& source=instance(c.source.node,voiceNode_[size_t(c.source.node)]?v:0);
            auto& destination=instance(c.destination.node,v);
            destination.in[size_t(c.destination.port)]=source.out[size_t(c.source.port)];
            destination.inRight[size_t(c.destination.port)]=source.outRight[size_t(c.source.port)];
        }
        if(c.destination.node==mixer_&&c.destination.port==0)for(int v=0;v<maximumVoices;++v){
            auto& source=instance(c.source.node,voiceNode_[size_t(c.source.node)]?v:0);
            mixLeft_[size_t(v)]=source.out[size_t(c.source.port)];
            mixRight_[size_t(v)]=source.outRight[size_t(c.source.port)];
        }
    }
    if(mixer_>=0)for(int v=0;v<maximumVoices;++v)if(!mixLeft_[size_t(v)]){mixLeft_[size_t(v)]=zero_.data();mixRight_[size_t(v)]=zero_.data();}
    for(size_t i=0;i<spec.modulations.size();++i)routes_->amounts[i]=spec.modulations[i].amount;
    for(const auto& m:graph_.plan.modulations)for(int v=0;v<voicesOf(m.destination.node);++v)
        instance(m.destination.node,v).routes.push_back(
            {instance(m.source.node,voiceNode_[size_t(m.source.node)]?v:0).out[size_t(m.source.port)],m.destination.port,m.index});
    for (const auto& o : graph_.plan.outputs) {
        const auto& r=findType(registry,typeOf(o.node));
        if (r.outputs[size_t(o.port)].type != SignalType::Audio)
            throw std::invalid_argument("Instrument output must be audio");
        roots_.push_back(nodes_[size_t(o.node)].out[size_t(o.port)]);
        rightRoots_.push_back(nodes_[size_t(o.node)].outRight[size_t(o.port)]);
    }
}

void Runtime::render(float* out, int frames, Note note) noexcept {
    std::array<float,quantum> right;
    while(frames>0){const int n=std::min(frames,quantum);render(out,right.data(),n,note);note.trigger=false;out+=n;frames-=n;}
}

void Runtime::computeParameters(NodeBuffers& node,int sampleOffset) noexcept {
    std::fill_n(node.offset.data(),node.parameters.size(),0.f);
    for(const auto& m:node.routes){
        const float sample=m.source[sampleOffset];
        const float source=std::isfinite(sample)?std::clamp(sample,-1.f,1.f):0.f;
        const float depth=routes_->amounts[size_t(m.index)].load(std::memory_order_relaxed);
        if(std::isfinite(depth))node.offset[size_t(m.parameter)]+=source*std::clamp(depth,-1.f,1.f);
    }
    const auto revision=node.module->controls->editRevision.load();
    if(!(revision&1)){
        for(size_t i=0;i<node.parameters.size();++i)node.candidate[i]=node.module->controls->values[i].load();
        if(revision==node.module->controls->editRevision.load())std::copy_n(node.candidate.data(),node.parameters.size(),node.base.data());
    }
    for(size_t i=0;i<node.parameters.size();++i){
        const auto& p=node.parameters[i];float base=node.base[i];
        base=std::isfinite(base)?std::clamp(base,p.minimum,p.maximum):p.initial;
        node.effective[i]=movementValue(p,base,node.offset[i]);
    }
}

void Runtime::processNode(NodeBuffers& node,int n,const ClockFrame* time,const Note& note) noexcept {
    node.events.begin(uint32_t(n));
    for(auto& bus:node.additionalEvents)bus.begin(uint32_t(n));
    for (auto* p : node.out) std::fill_n(p, n, 0.f);
    for (auto* p : node.outRight) std::fill_n(p,n,0.f);
    computeParameters(node);
    if(!node.noteInput)node.module->process({n,note,node.in.data(),node.out.data(),node.inRight.data(),node.outRight.data(),node.effective.data(),time,&node.events,node.eventOutputs.data()});
    else {
        const auto& bus=*node.noteInput;node.held.beginBlock(bus);
        if(bus.failed()){node.routedNote.gate=false;node.routedNote.trigger=false;}
        auto event=bus.begin();
        for(int offset=0;offset<n;){
            bool attack=false;
            while(event!=bus.end()&&int(event->offset)==offset){
                const auto change=node.held.apply(*event++);
                attack|=change==NoteChange::Added||change==NoteChange::Updated;
            }
            if(const auto* held=node.held.newest())node.routedNote={held->pitch,held->velocity,true,attack,note.bend,note.wheel};
            else {node.routedNote.gate=false;node.routedNote.trigger=false;}
            const int end=event==bus.end()?n:int(event->offset);
            for(size_t i=0;i<node.in.size();++i){node.sliceIn[i]=node.in[i]+offset;node.sliceRight[i]=node.inRight[i]+offset;}
            for(size_t i=0;i<node.out.size();++i){node.sliceOut[i]=node.out[i]+offset;node.sliceOutRight[i]=node.outRight[i]+offset;}
            node.module->process({end-offset,node.routedNote,node.sliceIn.data(),node.sliceOut.data(),node.sliceRight.data(),node.sliceOutRight.data(),node.effective.data(),time+offset,nullptr});
            offset=end;
        }
    }
    for(size_t p=0;p<node.out.size();++p)if(node.channels[p]==1)std::copy_n(node.out[p],n,node.outRight[p]);
}

void Runtime::processVoice(NodeBuffers& node,int n,const ClockFrame* time,int voice) noexcept {
    node.events.begin(uint32_t(n));
    for(auto& bus:node.additionalEvents)bus.begin(uint32_t(n));
    for (auto* p : node.out) std::fill_n(p, n, 0.f);
    for (auto* p : node.outRight) std::fill_n(p,n,0.f);
    computeParameters(node);
    if(woke_[size_t(voice)])node.module->wake();
    const auto& changes=changes_[size_t(voice)];
    const int total=changeCount_[size_t(voice)];
    Note current=voiceStart_[size_t(voice)];
    int k=0;
    for(int offset=0;offset<n;){
        Note use=current;
        while(k<total&&changes[size_t(k)].offset==offset){use=changes[size_t(k)].note;++k;}
        if(offset && use.trigger && !node.routes.empty())computeParameters(node,offset);
        const int end=k<total?changes[size_t(k)].offset:n;
        for(size_t i=0;i<node.in.size();++i){node.sliceIn[i]=node.in[i]+offset;node.sliceRight[i]=node.inRight[i]+offset;}
        for(size_t i=0;i<node.out.size();++i){node.sliceOut[i]=node.out[i]+offset;node.sliceOutRight[i]=node.outRight[i]+offset;}
        node.module->process({end-offset,use,node.sliceIn.data(),node.sliceOut.data(),node.sliceRight.data(),node.sliceOutRight.data(),node.effective.data(),time+offset,nullptr});
        current=use;current.trigger=false;
        offset=end;
    }
    for(size_t p=0;p<node.out.size();++p)if(node.channels[p]==1)std::copy_n(node.out[p],n,node.outRight[p]);
}

void Runtime::pushChange(int voice,int offset,const Note& note) noexcept {
    auto& count=changeCount_[size_t(voice)];
    auto& list=changes_[size_t(voice)];
    if(count>0&&list[size_t(count-1)].offset==offset){list[size_t(count-1)].note=note;return;}
    if(count<int(list.size()))list[size_t(count++)]={offset,note};
    else list[list.size()-1]={offset,note};
}

void Runtime::updateTuning(int scale,int root) noexcept {
    scale=std::clamp(scale,0,4);root=std::clamp(root,0,11);
    if(scale==tuningScale_&&root==tuningRoot_)return;
    tuningScale_=scale;tuningRoot_=root;
    // Ratios per pitch class above the root: 5-limit (Ptolemaic), 7-limit, Pythagorean.
    static constexpr double ratios[3][12]={
        {1,16./15,9./8,6./5,5./4,4./3,45./32,3./2,8./5,5./3,9./5,15./8},
        {1,15./14,9./8,7./6,5./4,4./3,7./5,3./2,8./5,5./3,7./4,15./8},
        {1,256./243,9./8,32./27,81./64,4./3,729./512,3./2,128./81,27./16,16./9,243./128}};
    for(int m=0;m<128;++m){
        double pitch=m;
        if(scale>=1&&scale<=3){
            const int rel=m-root,octave=int(std::floor(rel/12.0)),pc=rel-12*octave;
            pitch=root+12.0*octave+12.0*std::log2(ratios[scale-1][pc]);
        }else if(scale==4){
            // Non-octave harmonic series: the root key at C4's octave is partial 8;
            // each key above is the next partial. Lower keys fold down by octaves.
            const int reference=60+root;int key=m,folds=0;
            while(8+(key-reference)<1){key+=12;++folds;}
            pitch=reference+12.0*std::log2((8.0+(key-reference))/8.0)-12.0*folds;
        }
        tuning_[size_t(m)]=float(pitch-m);
    }
}

// Keyboard notes merged into a sequencer-driven voice group carry this source bit, so
// sequencer note-offs and transport resets never release the player's held keys.
constexpr uint32_t keyboardSource=0x40000000u;
void Runtime::allocate(const NoteEventBlock* events,const NoteEventBlock* keyboard,int n,int voices,const Note& performance) noexcept {
    auto& mixer=nodes_[size_t(mixer_)];
    computeParameters(mixer);
    const float* mp=mixer.effective.data();
    const float glide=mp[3],detune=mp[5];
    const int unison=std::clamp(int(std::lround(mp[4])),1,8);
    const bool steal=mp[6]>=.5f;
    updateTuning(int(std::lround(mp[7])),int(std::lround(mp[8])));
    // Glide time is the time to cover ~95% of the interval (three RC constants).
    const float step=glide<=0?1.f:1.f-std::exp(-3.f*float(n)/(glide*48000.f));
    auto bendOf=[&](const VoiceSlot& s){return performance.bend+(s.pitch-float(s.note.midi))+s.detune*.01f;};
    for(int v=0;v<maximumVoices;++v){
        auto& slot=slots_[size_t(v)];
        if(slot.tuned)slot.pitch+=(slot.target-slot.pitch)*step;
        slot.note.trigger=false;slot.note.bend=bendOf(slot);slot.note.wheel=performance.wheel;
        woke_[size_t(v)]=false;
        voiceStart_[size_t(v)]=slot.note;
        changeCount_[size_t(v)]=0;
        if(v>=voices){if(slot.active){slot.active=false;slot.note.gate=false;slot.released=++voiceClock_;pushChange(v,0,slot.note);}slot.awake=false;}
    }
    if(keyboard&&(mp[9]<.5f||keyboard->failed()||keyboard->size()==0))keyboard=nullptr;
    if(!events&&!keyboard)return;
    auto release=[&](int v,int offset){
        auto& slot=slots_[size_t(v)];
        slot.active=false;slot.note.gate=false;slot.note.trigger=false;slot.released=++voiceClock_;
        pushChange(v,offset,slot.note);
    };
    auto fromKeyboard=[](const VoiceSlot& s){return (s.source&keyboardSource)!=0;};
    if(events&&events->failed()){for(int v=0;v<voices;++v)if(slots_[size_t(v)].active&&!fromKeyboard(slots_[size_t(v)]))release(v,0);events=nullptr;}
    // Both lists are offset-ordered; merge them so each voice's changes stay in time order.
    const NoteEvent* a=events?events->begin():nullptr;const NoteEvent* aEnd=events?events->end():nullptr;
    const NoteEvent* b=keyboard?keyboard->begin():nullptr;const NoteEvent* bEnd=keyboard?keyboard->end():nullptr;
    while(a!=aEnd||b!=bEnd){
        const bool takeKeyboard=a==aEnd||(b!=bEnd&&b->offset<a->offset);
        NoteEvent e=takeKeyboard?*b++:*a++;
        if(takeKeyboard)e.source|=keyboardSource;
        const int offset=std::clamp(int(e.offset),0,n-1);
        if(e.kind==NoteEventKind::Reset){for(int v=0;v<voices;++v){const auto& s=slots_[size_t(v)];if(s.active&&fromKeyboard(s)==takeKeyboard)release(v,offset);}continue;}
        if(e.kind==NoteEventKind::Off){
            for(int v=0;v<voices;++v){const auto& s=slots_[size_t(v)];if(s.active&&s.source==e.source&&s.noteId==e.noteId)release(v,offset);}
            continue;
        }
        const int pitch=std::clamp(e.pitch,0,127);
        auto start=[&](int v,int index){
            auto& slot=slots_[size_t(v)];
            if(!slot.awake)woke_[size_t(v)]=true;
            slot.active=slot.awake=true;slot.quiet=0;slot.level=1.f;   // protected until its level is measured
            slot.source=e.source;slot.noteId=e.noteId;slot.started=++voiceClock_;
            slot.target=tuned(pitch);
            if(!slot.tuned||glide<=0)slot.pitch=slot.target;
            slot.tuned=true;
            slot.detune=unison>1?detune*(float(index)/float(unison-1)-.5f):0.f;
            slot.note={pitch,std::clamp(e.velocity,0.f,1.f),true,true,0.f,performance.wheel};
            slot.note.bend=bendOf(slot);
            pushChange(v,offset,slot.note);
            slot.note.trigger=false;
        };
        // A key already sounding retriggers its own voices.
        int existing=0;
        for(int v=0;v<voices;++v){const auto& s=slots_[size_t(v)];if(s.active&&s.source==e.source&&s.noteId==e.noteId)start(v,existing++);}
        if(existing)continue;
        uint32_t chosen=0;
        for(int k=0;k<unison;++k){
            int pick=-1;uint64_t oldest=~uint64_t(0);
            for(int v=0;v<voices;++v){const auto& s=slots_[size_t(v)];if(!(chosen>>v&1)&&!s.active&&s.released<oldest){oldest=s.released;pick=v;}}
            if(pick<0&&steal){
                // Steal the quietest held voice (level estimate, as Korg's VoicePool sorts
                // clusters); ties fall back to the oldest. Quiet voices are least missed.
                float quietest=1e30f;oldest=~uint64_t(0);
                for(int v=0;v<voices;++v){const auto& s=slots_[size_t(v)];
                    if(chosen>>v&1)continue;
                    const bool quieter=s.level<quietest*.999f,tie=!quieter&&s.level<=quietest*1.001f;
                    if(quieter||(tie&&s.started<oldest)){quietest=s.level;oldest=s.started;pick=v;}}}
            if(pick<0)break;   // stealing off and no free voice: the note is not played
            chosen|=1u<<pick;start(pick,k);
        }
    }
}

void Runtime::render(float* out,float* right,int frames,Note note,const ClockFrame* clockFrames,const NoteEventBlock* keys) noexcept {
    bool first=true;
    while (frames > 0) {
        const int n = std::min(frames, quantum);
        std::array<ClockFrame,quantum> localClock;
        if(!clockFrames)for(int i=0;i<n;++i)localClock[i]=clock.next();
        const auto* time=clockFrames?clockFrames:localClock.data();
        const int voices=voiceMixer_?voiceMixer_->voices():0;
        bool allocated=false;
        for (const int i : order_) {
            auto& node=nodes_[size_t(i)];
            if(voiceMixer_&&!allocated&&(voiceNode_[size_t(i)]||i==mixer_)){
                const auto* bus=node.noteInput?node.noteInput:nodes_[size_t(mixer_)].noteInput;
                allocate(bus?bus:(first?keys:nullptr),bus&&first?keys:nullptr,n,voices,note);
                allocated=true;
            }
            if(voiceMixer_&&voiceNode_[size_t(i)]){
                // Voice zero also keeps this LFO's continuous clock while the rack
                // is quiet. Seed waking copies before advancing the reference block.
                if(node.lfoPhase){
                    for(int v=1;v<voices;++v)if(woke_[size_t(v)])
                        *instance(i,v).lfoPhase=*node.lfoPhase;
                }
                for(int v=0;v<voices;++v)if(slots_[size_t(v)].awake || (v==0 && node.lfoPhase))
                    processVoice(instance(i,v),n,time,v);
                continue;
            }
            if(i==mixer_){
                node.events.begin(uint32_t(n));
                computeParameters(node);
                for(int v=0;v<maximumVoices;++v){
                    const bool awake=slots_[size_t(v)].awake;
                    liveLeft_[size_t(v)]=awake?mixLeft_[size_t(v)]:zero_.data();
                    liveRight_[size_t(v)]=awake?mixRight_[size_t(v)]:zero_.data();
                }
                voiceMixer_->mix(liveLeft_.data(),liveRight_.data(),voices,n,node.out[0],node.outRight[0],node.effective.data());
                for(int v=0;v<voices;++v){
                    auto& slot=slots_[size_t(v)];
                    if(!slot.awake){slot.level=0;continue;}
                    float peak=0;
                    for(int k=0;k<n;++k)peak=std::max(peak,std::max(std::abs(mixLeft_[size_t(v)][k]),std::abs(mixRight_[size_t(v)][k])));
                    slot.level+=(peak-slot.level)*.05f;   // ~10 ms level estimate for stealing
                    if(slot.active)continue;
                    slot.quiet=peak<1e-5f?slot.quiet+n:0;
                    if(slot.quiet>=24000)slot.awake=false;   // 0.5 s below -100 dBFS after release
                }
                continue;
            }
            processNode(node,n,time,note);
        }
        std::fill_n(out, n, 0.f);
        std::fill_n(right,n,0.f);
        for (const auto* root : roots_) for (int i = 0; i < n; ++i) out[i] += root[i];
        for (const auto* root : rightRoots_) for (int i=0;i<n;++i)right[i]+=root[i];
        note.trigger = false;first=false;
        out += n;right+=n; frames -= n;
        if(clockFrames)clockFrames+=n;
    }
}
SpectralBank Runtime::spectralBank() const {
    SpectralBank bank;
    for(size_t i=0;i<nodes_.size();++i)if(auto state=nodes_[i].module->spectralState())bank.emplace(spec_.nodes[size_t(graph_.plan.nodes[i])].id,std::move(state));
    return bank;
}
std::shared_ptr<Controls> Runtime::controls(const std::string& node) const {
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        if (spec_.nodes[size_t(graph_.plan.nodes[i])].id == node) return nodes_[i].module->controls;
    return {};
}
GraphSpec instrumentPatch(const std::string& source, const std::string& envelope,
                          const std::string& filter, bool amp, bool cutoff) {
    GraphSpec g;
    g.nodes = {{"source", source, {}}, {"envelope", envelope, {}},
               {"filter", filter, {}}, {"vca", "utility.vca", {}}};
    g.connections = {{{"source","out"},{"filter","in"}}};
    if (cutoff) g.connections.push_back({{"envelope","out"},{"filter","mod"}});
    if (amp) {
        g.connections.push_back({{"filter","out"},{"vca","in"}});
        g.connections.push_back({{"envelope","out"},{"vca","gain"}});
    }
    g.outputs = {{amp ? "vca" : "filter", "out"}};
    return g;
}
} // namespace optisynth
