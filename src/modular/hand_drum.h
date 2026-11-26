#pragma once
#include "hand_drum_profiles.h"
#include "runtime.h"

namespace optisynth::handdrum {
inline bool isHandDrum(const std::string& type){return type=="drum.frame"||type=="drum.pakhawaj";}
inline std::string sampleKey(const std::string& node,int index){return node+"__skin_"+std::to_string(index);}

// A finite contact force drives six measured membrane modes. One passive mode
// exchanges velocity with the strongest driven mode. All states run at 48 kHz.
class Head {
    struct Mode {
        float x=0,y=0,c=1,s=0,drive=0;
        void tune(float hz,float tau){
            const float angle=6.2831853f*std::min(hz,21500.f)/48000.f;
            const float loss=std::exp(-1.f/(48000.f*std::max(tau,.002f)));
            c=loss*std::cos(angle);s=loss*std::sin(angle);
        }
        void advance(float force){
            y+=force*drive;
            const float next=c*x+s*y;y=c*y-s*x;x=next;
        }
    };
    struct Layer {
        const StrokeFit* fit=nullptr;const SampleData* sample=nullptr;
        Mode poles[6],passive;
        float weight=0,noiseEnv=1,noiseLoss=1,low=0,mid=0;
        double cursor=0;
    };
    std::array<SamplePtr,30> files{};
    Layer layers[4]{};
    bool barrel,hasSamples=false;int count=0,age=0,contactLength=0,stroke=0;
    float velocity=0,radialPosition=0,variation=1,overall=0;
    uint32_t rng;
    float random(){rng=rng*1664525u+1013904223u;return float(rng>>8)/8388608.f-1;}
public:
    Head(bool pakhawaj,const std::string& node,const SampleBank& bank,int voice):barrel(pakhawaj),rng(151u+7919u*uint32_t(voice)){
        for(size_t i=0;i<files.size();++i){
            auto found=bank.find(sampleKey(node,int(i)));if(found!=bank.end()&&found->second){files[i]=found->second;hasSamples=true;}
        }
    }
    void strike(const Note& note,const float* p){
        static constexpr int keys[]={0,0,1,1,2,3,3,4,4,1,2,3};
        stroke=keys[std::clamp(note.midi,0,127)%12];
        velocity=std::clamp(note.velocity,0.f,1.f);overall=.43f*std::pow(velocity,.82f);
        radialPosition=std::clamp(p[1]+random()*.10f*p[1]*(1-p[1]),0.f,1.f);
        variation=1+random()*.0035f;
        const float dynamic=std::clamp((velocity-.22f)/.69f,0.f,1.f)*2;
        const int lower=std::min(1,int(dynamic));const float fraction=dynamic-lower;
        count=0;age=0;
        contactLength=std::max(12,int(48*(.45f+3.6f*(1-p[2])*(1-.55f*velocity))*(stroke==0?1.3f:.85f)));
        const int zones=barrel&&(!hasSamples||p[9]==0)?1:2;
        for(int zone=0;zone<zones;++zone)for(int side=0;side<2;++side){
            const float w=(side?fraction:1-fraction)*(zones==1?1.f:zone?radialPosition:1-radialPosition);
            if(w<.0001f)continue;
            const int index=barrel?30+stroke*3+lower+side:stroke*6+zone*3+lower+side;
            auto& layer=layers[count++];layer=Layer{};
            layer.weight=barrel&&zones==2?std::sqrt(side?fraction:1-fraction)*(zone?radialPosition:1-radialPosition):std::sqrt(w);layer.fit=&fits[index];
            layer.sample=files[size_t(stroke*6+zone*3+lower+side)].get();
            if(layer.sample)layer.cursor=layer.sample->strikeStart;
            layer.noiseLoss=std::exp(-1.f/(48000*layer.fit->noiseTime));
            auto displacement=[](int mode,float radius){
                const float pos=std::clamp(radius,0.f,1.f)*8;const int r=std::min(7,int(pos));
                return radial[mode][r]+(pos-r)*(radial[mode][r+1]-radial[mode][r]);
            };
            for(int k=0;k<6;++k){
                const auto& pole=layer.fit->poles[k];
                // Order the spatial basis by measured frequency, independently of peak strength.
                int rank=0;for(const auto& other:layer.fit->poles)if(other.hz<pole.hz)++rank;
                const float width=.015f+.27f*p[8];
                const float spatial=(displacement(rank,radialPosition-width)+2*displacement(rank,radialPosition)
                                    +displacement(rank,radialPosition+width))*.25f;
                layer.poles[k].drive=pole.gain*(.55f+.65f*std::abs(spatial))*(k%2?-1.f:1.f);
            }
        }
    }
    void render(const AudioBlock& b,const float* p){
        if(b.note.trigger)strike(b.note,p);
        if(!count){std::fill_n(b.outputs[0],b.frames,0.f);return;}
        const float pressure=std::clamp(p[4]+b.note.wheel*.72f,0.f,1.f);
        const float held=pressure*(b.note.gate?1.f:.35f);
        const float pitch=std::exp2((p[0]+b.note.bend+held*(stroke==0?2.4f:.5f))/12)*variation;
        const float stretch=p[3]/(1+held*2.8f);
        const float kappa=p[5]*(barrel?55.f:23.f)/48000.f;
        const float rotation=1.f-.5f*kappa*kappa;
        for(int l=0;l<count;++l){
            auto& layer=layers[l];
            for(int k=0;k<6;++k){
                const auto& pole=layer.fit->poles[k];
                layer.poles[k].tune(pole.hz*pitch,pole.tau*stretch);
            }
            const float passiveHz=barrel?(stroke==0?158.f:82.f):185.f;
            layer.passive.tune(passiveHz*pitch*std::exp2(p[6]/12.f),(barrel?.37f:.19f)*stretch*p[7]);
        }
        for(int i=0;i<b.frames;++i,++age){
            const float t=(age+.5f)/contactLength;
            // Unit-area half-sine. Shorter contact naturally passes higher modes.
            const float broad=t<1?1.5707963f*std::sin(3.14159265f*t)/contactLength:0;
            // Palm lands as one broad contact. Slaps and fingers also excite the
            // skin with a narrower fingertip contact just after the hand arrives.
            const float tipFraction=stroke==0?.10f:stroke==2?.64f:stroke==3?.74f:stroke==4?.49f:.32f;
            const int tipLength=std::max(8,int(contactLength*.23f));
            const float tipTime=(age+.5f-contactLength*.55f)/tipLength;
            const float tip=tipTime>0&&tipTime<1?1.5707963f*std::sin(3.14159265f*tipTime)/tipLength:0;
            const float force=broad*(1-tipFraction)+tip*tipFraction;
            const float white=random();float out=0;
            for(int l=0;l<count;++l){
                auto& layer=layers[l];auto& passive=layer.passive;
                float recorded=0;
                if(layer.sample){
                    const auto& sample=*layer.sample;
                    const auto pos=size_t(layer.cursor);
                    if(pos+1<sample.mono.size()){
                        const float frac=float(layer.cursor-pos);
                        recorded=(sample.mono[pos]+frac*(sample.mono[pos+1]-sample.mono[pos]))/sample.strikePeak;
                    }
                    layer.cursor+=sample.rate/48000.*pitch;
                }
                const float recordedImpulse=layer.sample&&age<960?recorded*p[9]*.008f*(1-age/960.f):0;
                float body=0;
                for(auto& mode:layer.poles){mode.advance(force+recordedImpulse);body+=mode.x;}
                passive.advance(0);
                auto& main=layer.poles[0];
                const float y=rotation*main.y-kappa*passive.y;
                passive.y=rotation*passive.y+kappa*main.y;main.y=y;
                body+=passive.x*.78f;
                layer.low+=.08f*(white-layer.low);layer.mid+=.36f*(white-layer.mid);
                const auto& colour=layer.fit->contact;
                const float impact=layer.noiseEnv*(colour[0]*layer.low*3+colour[1]*(layer.mid-layer.low)*5.3f
                                  +colour[2]*(white-layer.mid)*3.2f)*(.30f+1.6f*p[2]);
                layer.noiseEnv*=layer.noiseLoss;
                float dry=body+impact;
                if(layer.sample){
                    // Let recorded skin dominate the contact; hand damping also shortens its body.
                    const float mix=p[9]*std::exp(-float(age)/48000.f*(2.8f+held*16.f));
                    dry+=mix*(recorded-dry);
                }
                out+=layer.weight*dry;
            }
            const float endFade=std::clamp((288000-age)/1920.f,0.f,1.f);
            b.outputs[0][i]=out*overall*endFade;
        }
        if(age>48000*6)count=0;
    }
};
} // namespace optisynth::handdrum
