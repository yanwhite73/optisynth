#pragma once
#include "runtime.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace optisynth {
// Single UI producer / audio consumer. No deletion in the audio callback.
// Caller must stop audio before destruction. A 5 ms crossfade retires old plans.
class Player {
public:
    ~Player() { delete active_;delete fading_;delete pending_.load();delete retired_.load(); }
    void publish(std::unique_ptr<Runtime> next,bool movementEdit=false) {
        if(!movementEdit)++generation_;
        next->patchGeneration_=generation_;
        delete pending_.exchange(next.release());collect();
    }
    void collect() { delete retired_.exchange(nullptr); }
    // UI thread only. A packed event prevents torn pitch/gate/sequence reads.
    void note(int midi,bool gate) {
        packed_.store((++serial_<<8) | (std::clamp(midi,0,127)<<1) | int(gate),std::memory_order_release);
    }
    // Polyphonic key events for voice groups. One producer (UI/message thread),
    // drained on the audio thread. Full ring drops the event rather than blocking.
    void noteEvent(int midi,float velocity,bool on) noexcept {pushKey(uint8_t(std::clamp(midi,0,127)),on?2:1,velocity);}
    void releaseAll() noexcept {pushKey(0,3,0);}
    std::atomic<float> bend{0},wheel{0}; // semitones, 0..1 (MIDI thread may write)
    std::atomic<float> load{0}; // smoothed DSP time / real time of the rendered audio
    std::atomic<float> gain{.25f};
    std::atomic<float> peak{0};
    std::atomic<float> tempo{120};
    std::atomic<bool> playing{false};
    std::atomic<double> beat{0};
    void restart() noexcept {restart_.fetch_add(1,std::memory_order_release);}
    void render(float* output,int n) noexcept {
        std::array<float,quantum> right;
        while(n>0){const int count=std::min(n,quantum);render(output,right.data(),count);output+=count;n-=count;}
    }
    void render(float* output,float* right,int n) noexcept {
        while(n>0) {
            const int count=std::min(n,quantum);
            const auto reset=restart_.load(std::memory_order_acquire);
            if(reset!=seenRestart_){clock_.seek(0);seenRestart_=reset;}
            float bpm=tempo.load(std::memory_order_relaxed);
            if(!std::isfinite(bpm))bpm=120;bpm=std::clamp(bpm,20.f,300.f);
            if(bpm!=lastTempo_){clock_.tempo(bpm);lastTempo_=bpm;}
            clock_.play(playing.load(std::memory_order_relaxed));
            std::array<ClockFrame,quantum> time;
            for(int i=0;i<count;++i)time[i]=clock_.next();
            beat.store(time[count-1].beat,std::memory_order_relaxed);
            const auto packed=packed_.load(std::memory_order_acquire);
            Note note{int((packed>>1)&127),.8f,bool(packed&1),false};
            note.trigger=(packed>>8)!=seen_ && note.gate;seen_=packed>>8;
            float b=bend.load(std::memory_order_relaxed),w=wheel.load(std::memory_order_relaxed);
            note.bend=std::isfinite(b)?std::clamp(b,-24.f,24.f):0.f;note.wheel=std::isfinite(w)?std::clamp(w,0.f,1.f):0.f;
            keys_.begin(uint32_t(count));
            drainKeys();
            if(!fading_ && !retired_.load(std::memory_order_acquire)) {
                if(auto* next=pending_.exchange(nullptr,std::memory_order_acq_rel)) {
                    if(active_ && active_->patchGeneration_==next->patchGeneration_){
                        next->continueMovementFrom(*active_);
                        retired_.store(active_,std::memory_order_release);
                        active_=next;
                    }else{
                        fading_=active_;active_=next;fade_=0;note.trigger=note.gate;
                        // A new patch receives every key still held.
                        keys_.begin(uint32_t(count));
                        for(int k=0;k<128;++k)if(held_[size_t(k)]>0)keys_.push({0,1,uint64_t(k)+1,NoteEventKind::On,k,held_[size_t(k)]});
                    }
                }
            }
            std::fill_n(output,count,0.f);
            std::fill_n(right,count,0.f);
            const auto started=std::chrono::steady_clock::now();
            if(active_)active_->render(output,right,count,note,time.data(),&keys_);
            if(fading_) {
                Note oldNote=note;oldNote.trigger=false;oldNote.gate=false;
                for(int i=0;i<count;++i)time[i].playing=false;
                fading_->render(old_.data(),oldRight_.data(),count,oldNote,time.data());
            }
            const float spent=std::chrono::duration<float>(std::chrono::steady_clock::now()-started).count();
            smoothedLoad_+=(spent*48000.f/float(count)-smoothedLoad_)*.002f;
            load.store(smoothedLoad_,std::memory_order_relaxed);
            float max=0;
            float target=gain.load(std::memory_order_relaxed);
            if(!std::isfinite(target))target=0;
            target=std::clamp(target,0.f,1.f);
            for(int i=0;i<count;++i) {
                fade_=std::min(240,fade_+1);
                const float mix=fade_/240.f;
                float x=output[i]*mix+(fading_?old_[i]*(1-mix):0);
                smoothedGain_ += (target-smoothedGain_)*.004f;
                x*=smoothedGain_;
                // Guard unforeseen extreme resonance, not a model of Mutable's DAC.
                output[i]=std::isfinite(x)?std::clamp(x,-.95f,.95f):0;
                x=(right[i]*mix+(fading_?oldRight_[i]*(1-mix):0))*smoothedGain_;
                right[i]=std::isfinite(x)?std::clamp(x,-.95f,.95f):0;
                max=std::max(max,std::max(std::abs(output[i]),std::abs(right[i])));
            }
            if(fading_ && fade_>=240) { retired_.store(fading_,std::memory_order_release);fading_=nullptr; }
            peak.store(max,std::memory_order_relaxed);
            output+=count;right+=count;n-=count;
        }
    }
private:
    struct Key { uint8_t pitch=0,kind=0; float velocity=0; };
    void pushKey(uint8_t pitch,uint8_t kind,float velocity) noexcept {
        const auto head=keyHead_.load(std::memory_order_relaxed);
        if(head-keyTail_.load(std::memory_order_acquire)>=ring_.size())return;
        ring_[head%ring_.size()]={pitch,kind,std::isfinite(velocity)?std::clamp(velocity,0.f,1.f):.8f};
        keyHead_.store(head+1,std::memory_order_release);
    }
    void drainKeys() noexcept {
        auto tail=keyTail_.load(std::memory_order_relaxed);
        const auto head=keyHead_.load(std::memory_order_acquire);
        for(;tail!=head;++tail){
            const auto key=ring_[tail%ring_.size()];
            if(key.kind==3){held_.fill(0);keys_.push({0,1,0,NoteEventKind::Reset,0,0});continue;}
            const bool on=key.kind==2;
            held_[key.pitch]=on?std::max(key.velocity,.001f):0.f;
            keys_.push({0,1,uint64_t(key.pitch)+1,on?NoteEventKind::On:NoteEventKind::Off,int(key.pitch),key.velocity});
        }
        keyTail_.store(tail,std::memory_order_release);
    }
    std::array<Key,256> ring_{};
    std::atomic<uint64_t> keyHead_{0},keyTail_{0};
    std::array<float,128> held_{};
    NoteEventBlock keys_;
    std::atomic<Runtime*> pending_{nullptr},retired_{nullptr};
    Runtime *active_=nullptr,*fading_=nullptr;
    uint64_t generation_=0; // UI producer only
    SampleClock clock_;
    std::atomic<uint64_t> restart_{0};
    uint64_t seenRestart_=0;float lastTempo_=120;
    std::atomic<uint64_t> packed_{48<<1};
    uint64_t serial_=0,seen_=0;
    int fade_=0;
    float smoothedGain_=0,smoothedLoad_=0;
    std::array<float,quantum> old_{};
    std::array<float,quantum> oldRight_{};
};

// Bounded, causal 32-tap / 256-phase windowed-sinc output conversion. Tables
// prepared off-thread; no callback allocation or locks. ~16 internal samples
// latency. Fixed-rate DSP keeps its pitch and drum timing at every device rate.
class OutputConverter {
public:
    void prepare(double deviceRate) {
        if(!std::isfinite(deviceRate)||deviceRate<8000||deviceRate>384000)
            throw std::invalid_argument("Unsupported device sample rate");
        ratio_=48000/deviceRate;position_=0;write_=0;index_=quantum;history_.fill(0);historyRight_.fill(0);
        const double cutoff=.46*std::min(1.,deviceRate/48000);
        for(int phase=0;phase<256;++phase) {
            double sum=0;
            for(int k=0;k<32;++k) {
                const double distance=k+phase/256.-15;
                const double x=2*cutoff*distance;
                const double sinc=std::abs(x)<1e-10?1:std::sin(3.141592653589793*x)/(3.141592653589793*x);
                const double window=.5+.5*std::cos(3.141592653589793*distance/16);
                table_[phase][k]=float(2*cutoff*sinc*window);sum+=table_[phase][k];
            }
            for(auto& c:table_[phase])c/=float(sum);
        }
    }
    void render(Player& player,float* out,int frames) noexcept {
        std::array<float,quantum> right;
        while(frames>0){const int n=std::min(frames,quantum);render(player,out,right.data(),n);out+=n;frames-=n;}
    }
    void render(Player& player,float* out,float* right,int frames) noexcept {
        for(int i=0;i<frames;++i) {
            while(position_>=0) {
                if(index_==quantum) { player.render(block_.data(),blockRight_.data(),quantum);index_=0; }
                write_=(write_+1)&31;history_[write_]=block_[index_];historyRight_[write_]=blockRight_[index_++];position_-=1;
            }
            const int phase=std::clamp(int((position_+1)*256),0,255);
            float sample=0,sampleRight=0;
            for(int k=0;k<32;++k){sample+=table_[phase][k]*history_[(write_-k)&31];sampleRight+=table_[phase][k]*historyRight_[(write_-k)&31];}
            out[i]=std::isfinite(sample)?std::clamp(sample,-.95f,.95f):0;
            right[i]=std::isfinite(sampleRight)?std::clamp(sampleRight,-.95f,.95f):0;
            position_+=ratio_;
        }
    }
private:
    std::array<std::array<float,32>,256> table_{};
    std::array<float,32> history_{};
    std::array<float,32> historyRight_{};
    std::array<float,quantum> block_{};
    std::array<float,quantum> blockRight_{};
    double ratio_=1,position_=0;
    int write_=0,index_=quantum;
};
} // namespace optisynth
