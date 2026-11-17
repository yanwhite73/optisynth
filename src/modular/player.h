#pragma once
#include "runtime.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optisynth {
// Single UI producer / audio consumer. No deletion in the audio callback.
// Caller must stop audio before destruction. A 5 ms crossfade retires old plans.
class Player {
public:
    ~Player() { delete active_;delete fading_;delete pending_.load();delete retired_.load(); }
    void publish(std::unique_ptr<Runtime> next) { delete pending_.exchange(next.release()); collect(); }
    void collect() { delete retired_.exchange(nullptr); }
    // UI thread only. A packed event prevents torn pitch/gate/sequence reads.
    void note(int midi,bool gate) {
        packed_.store((++serial_<<8) | (std::clamp(midi,0,127)<<1) | int(gate),std::memory_order_release);
    }
    std::atomic<float> gain{.25f};
    std::atomic<float> peak{0};
    void render(float* output,int n) noexcept {
        while(n>0) {
            const int count=std::min(n,quantum);
            const auto packed=packed_.load(std::memory_order_acquire);
            Note note{int((packed>>1)&127),.8f,bool(packed&1),false};
            note.trigger=(packed>>8)!=seen_ && note.gate;seen_=packed>>8;
            if(!fading_ && !retired_.load(std::memory_order_acquire)) {
                if(auto* next=pending_.exchange(nullptr,std::memory_order_acq_rel)) {
                    fading_=active_;active_=next;fade_=0;note.trigger=note.gate;
                }
            }
            std::fill_n(output,count,0.f);
            if(active_)active_->render(output,count,note);
            if(fading_) {
                Note oldNote=note;oldNote.trigger=false;oldNote.gate=false;
                fading_->render(old_.data(),count,oldNote);
            }
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
                max=std::max(max,std::abs(output[i]));
            }
            if(fading_ && fade_>=240) { retired_.store(fading_,std::memory_order_release);fading_=nullptr; }
            peak.store(max,std::memory_order_relaxed);
            output+=count;n-=count;
        }
    }
private:
    std::atomic<Runtime*> pending_{nullptr},retired_{nullptr};
    Runtime *active_=nullptr,*fading_=nullptr;
    std::atomic<uint64_t> packed_{48<<1};
    uint64_t serial_=0,seen_=0;
    int fade_=0;
    float smoothedGain_=0;
    std::array<float,quantum> old_{};
};

// Bounded, causal 32-tap / 256-phase windowed-sinc output conversion. Tables
// prepared off-thread; no callback allocation or locks. ~16 internal samples
// latency. Fixed-rate DSP keeps its pitch and drum timing at every device rate.
class OutputConverter {
public:
    void prepare(double deviceRate) {
        if(!std::isfinite(deviceRate)||deviceRate<8000||deviceRate>384000)
            throw std::invalid_argument("Unsupported device sample rate");
        ratio_=48000/deviceRate;position_=0;write_=0;index_=quantum;history_.fill(0);
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
        for(int i=0;i<frames;++i) {
            while(position_>=0) {
                if(index_==quantum) { player.render(block_.data(),quantum);index_=0; }
                write_=(write_+1)&31;history_[write_]=block_[index_++];position_-=1;
            }
            const int phase=std::clamp(int((position_+1)*256),0,255);
            float sample=0;
            for(int k=0;k<32;++k)sample+=table_[phase][k]*history_[(write_-k)&31];
            out[i]=std::isfinite(sample)?std::clamp(sample,-.95f,.95f):0;
            position_+=ratio_;
        }
    }
private:
    std::array<std::array<float,32>,256> table_{};
    std::array<float,32> history_{};
    std::array<float,quantum> block_{};
    double ratio_=1,position_=0;
    int write_=0,index_=quantum;
};
} // namespace optisynth
