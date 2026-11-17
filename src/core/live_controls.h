#pragma once
#include "patch.h"
#include "effect_controls.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace partials {
// Append-only host parameter order. These controls need no tables or memory allocation.
constexpr int kLiveOscControls=12, kLiveLfoStart=48, kLiveMain=53, kLiveWetStart=54;
constexpr int kLegacyLiveControls=60, kLiveEnvStart=60, kEnvFields=9, kLiveMatrixStart=168,
              kLiveFxStart=192, kLiveControls=kLiveFxStart+6*kEffectControls;
using LiveValues=std::array<float,kLiveControls>;
struct LiveControlSpec {
    std::string id,name,unit;
    float minimum,maximum,initial;
    bool logarithmic=false;
    ControlScale scale=ControlScale::Linear;
    std::string patchKey,condition;
    bool lfoCandidate=true;
    int versionHint=2;
    LiveControlSpec()=default;
    LiveControlSpec(std::string i,std::string n,std::string u,float lo,float hi,float init,bool log)
      :id(std::move(i)),name(std::move(n)),unit(std::move(u)),minimum(lo),maximum(hi),initial(init),logarithmic(log) {}
};
inline EnvelopeParams& liveEnvelope(Patch& p,int i) {
    return i<4?p.ampEnv[i]:i<8?p.cutEnv[i-4]:i<11?p.modEnv[i-8]:p.mainEnv;
}
inline const EnvelopeParams& liveEnvelope(const Patch& p,int i) {
    return i<4?p.ampEnv[i]:i<8?p.cutEnv[i-4]:i<11?p.modEnv[i-8]:p.mainEnv;
}
inline constexpr std::array<float EnvelopeParams::*,kEnvFields> envelopeFields{{
    &EnvelopeParams::attack,&EnvelopeParams::hold,&EnvelopeParams::decay,&EnvelopeParams::sustain,
    &EnvelopeParams::fade,&EnvelopeParams::release,&EnvelopeParams::attackCurve,
    &EnvelopeParams::decayCurve,&EnvelopeParams::releaseCurve}};
inline float effectDefaultWet(const std::string& type) {
    if(type=="reverb")return .25f;
    if(type.find("delay")==0)return .3f;
    return type=="chorus"||type=="phaser"||type=="flanger"?.5f:1.f;
}
inline LiveValues liveValues(const Patch& p) {
    LiveValues v{};
    for(int i=0;i<4;++i){const auto& o=p.osc[i];const auto& f=o.filter;const int b=i*kLiveOscControls;
        v[b]=o.waveMix;v[b+1]=o.symmetry;v[b+2]=o.volumeDb;v[b+3]=f.cutoffHz;
        v[b+4]=f.resonance;v[b+5]=f.pan;v[b+6]=f.volumeDb;v[b+7]=f.envDepth;
        v[b+8]=o.xmod[0].amount;v[b+9]=o.xmod[1].amount;v[b+10]=f.xmod[0].amount;v[b+11]=f.xmod[1].amount;
    }
    for(int i=0;i<5;++i)v[kLiveLfoStart+i]=p.lfo[i].rateHz;
    v[kLiveMain]=p.mainVolumeDb;
    for(int i=0;i<6;++i){auto it=p.fx[i].params.find("wet");v[kLiveWetStart+i]=it==p.fx[i].params.end()?effectDefaultWet(p.fx[i].type):it->second;}
    for(int e=0;e<12;++e)for(int f=0;f<kEnvFields;++f)v[kLiveEnvStart+e*kEnvFields+f]=liveEnvelope(p,e).*envelopeFields[f];
    for(int i=0;i<kMatrixSlots;++i)v[kLiveMatrixStart+i]=p.matrix[i].amount;
    for(int s=0;s<6;++s)for(int f=0;f<kEffectControls;++f){
        const auto& spec=effectControlSpecs[f];float value=spec.initial;
        if(p.fx[s].type==spec.type){auto it=p.fx[s].params.find(spec.key);if(it!=p.fx[s].params.end())value=it->second;}
        v[kLiveFxStart+s*kEffectControls+f]=value;
    }
    return v;
}
inline const std::array<LiveControlSpec,kLiveControls>& liveControlSpecs() {
    static const auto specs=[] {
        std::array<LiveControlSpec,kLiveControls> result;
        const char* ids[]={"wave_mix","symmetry","volume","filter_cutoff","filter_resonance","filter_pan","filter_volume","filter_envelope","xmod1","xmod2","filter_xmod1","filter_xmod2"};
        const char* names[]={"Wave Mix","Symmetry","Osc Volume","Cutoff","Resonance","Pan","Filter Volume","Filter Env","XMod 1","XMod 2","Filter XMod 1","Filter XMod 2"};
        const float lo[]={0,.01f,-200,20,0,-1,-200,-1,0,0,0,0};
        const float hi[]={1,.99f,24,20480,1,1,24,1,1,1,1,1};
        auto initial=liveValues(defaultPatch());
        for(int o=0;o<4;++o)for(int i=0;i<kLiveOscControls;++i){const int k=o*kLiveOscControls+i;
            result[k]={"osc"+std::to_string(o+1)+"_"+ids[i],"O"+std::to_string(o+1)+" "+names[i],i==2||i==6?"dB":i==3?"Hz":"",lo[i],hi[i],initial[k],i==3};}
        for(int i=0;i<5;++i)result[kLiveLfoStart+i]={"lfo"+std::to_string(i+1)+"_rate","LFO "+std::to_string(i+1)+" Rate","Hz",0,200,1,false};
        result[kLiveMain]={"main_volume","Main Volume","dB",-200,24,-6,false};
        for(int i=0;i<6;++i)result[kLiveWetStart+i]={"fx"+std::to_string(i+1)+"_wet","FX "+std::to_string(i+1)+" Wet","",0,1,1,false};
        const char* patchFields[]={"wave_mix","symmetry","volume_db","filter.cutoff_hz","filter.resonance","filter.pan","filter.volume_db","filter.env_depth","xmod1.amount","xmod2.amount","filter.xmod1.amount","filter.xmod2.amount"};
        for(int o=0;o<4;++o)for(int f=0;f<12;++f){auto& s=result[o*12+f];s.patchKey="osc."+std::to_string(o+1)+"."+patchFields[f];
            s.scale=s.logarithmic?ControlScale::Logarithmic:ControlScale::Linear;
            s.lfoCandidate=f!=2&&f!=5&&f!=6&&f!=10&&f!=11;
            s.condition="Enabled oscillator/filter and supported source/mode required.";}
        for(int i=0;i<5;++i){result[48+i].patchKey="lfo."+std::to_string(i+1)+".rate_hz";result[48+i].condition="Free-running LFO only; sync overrides rate.";}
        result[kLiveMain].patchKey="main.volume_db";result[kLiveMain].lfoCandidate=false;
        for(int i=0;i<6;++i){result[54+i].patchKey="fx."+std::to_string(i+1)+".p.wet";result[54+i].condition="Enabled, routed effect required.";}
        const char* ef[]={"attack","hold","decay","sustain","fade","release","attack_curve","decay_curve","release_curve"};
        const char* en[]={"Attack","Hold","Decay","Sustain","Fade","Release","A Curve","D Curve","R Curve"};
        for(int e=0;e<12;++e)for(int f=0;f<kEnvFields;++f){
            const int n=e<4?e+1:e<8?e-3:e<11?e-7:0;
            const std::string id=e<4?"amp":e<8?"cut":e<11?"modenv":"mainenv";
            const std::string name=e<4?"A":e<8?"C":e<11?"M":"MainEnv";
            const std::string number=n?std::to_string(n):"";
            auto& s=result[kLiveEnvStart+e*kEnvFields+f];
            s={id+number+"_"+ef[f],name+number+" "+en[f],f==4?"dB/s":f<3||f==5?"s":"",f>=6?.05f:f==4?-60.f:0.f,f>=6?20.f:f==3?1.f:60.f,initial[kLiveEnvStart+e*kEnvFields+f],f>=6};
            s.scale=f>=6?ControlScale::Logarithmic:f<3||f==5?ControlScale::ZeroSeconds:ControlScale::Linear;
            s.patchKey=(e<4?"env.amp.":e<8?"env.cut.":e<11?"env.mod.":"env.main")+number+"."+ef[f];
            s.versionHint=3;s.condition="Envelope must drive an enabled/routed source; stage timing affects that stage, without retriggering.";
        }
        for(int i=0;i<24;++i){auto n=std::to_string(i+1);auto& s=result[kLiveMatrixStart+i];
            s={"matrix"+n+"_amount","Matrix "+n+" Amount","",-1,1,initial[kLiveMatrixStart+i],false};
            s.patchKey="matrix."+n+".amount";s.versionHint=3;
            s.condition="Explicit opt-in until active route metadata is available; source/destination must be supported and destination eligible.";
            s.lfoCandidate=false;
        }
        for(int slot=0;slot<6;++slot)for(int f=0;f<kEffectControls;++f){const auto& a=effectControlSpecs[f];auto& s=result[kLiveFxStart+slot*kEffectControls+f];auto n=std::to_string(slot+1);
            s={"fx"+n+"_"+a.type+"_"+a.key,"F"+n+" "+a.tag+" "+a.label,a.unit,a.minimum,a.maximum,a.initial,a.scale==ControlScale::Logarithmic};
            s.scale=a.scale;s.patchKey="fx."+n+".p."+a.key;s.versionHint=3;s.lfoCandidate=!a.manualOnly;
            s.condition=std::string("Matching enabled/routed FX type: ")+a.type+"; free time/rate only when not synced; internal modulation must be active.";
        }
        return result;
    }();return specs;
}
// Scalar writes only. FX maps are deliberately excluded from this realtime function.
inline void applyLiveScalars(Patch& p,const LiveValues& v) {
    for(int i=0;i<4;++i){auto& o=p.osc[i];auto& f=o.filter;const int b=i*kLiveOscControls;
        o.waveMix=v[b];o.symmetry=v[b+1];o.volumeDb=v[b+2];f.cutoffHz=v[b+3];
        f.resonance=v[b+4];f.pan=v[b+5];f.volumeDb=v[b+6];f.envDepth=v[b+7];
        o.xmod[0].amount=v[b+8];o.xmod[1].amount=v[b+9];f.xmod[0].amount=v[b+10];f.xmod[1].amount=v[b+11];
    }
    for(int i=0;i<5;++i)p.lfo[i].rateHz=v[kLiveLfoStart+i];p.mainVolumeDb=v[kLiveMain];
    for(int e=0;e<12;++e)for(int f=0;f<kEnvFields;++f)liveEnvelope(p,e).*envelopeFields[f]=v[kLiveEnvStart+e*kEnvFields+f];
    for(int i=0;i<kMatrixSlots;++i)p.matrix[i].amount=v[kLiveMatrixStart+i];
}
// Message/loader thread only: may create map entries when saving a host-controlled patch.
inline void applyLiveValues(Patch& p,const LiveValues& v) {
    applyLiveScalars(p,v);
    for(int i=0;i<6;++i)if(p.fx[i].type!="none")p.fx[i].params["wet"]=v[kLiveWetStart+i];
    for(int s=0;s<6;++s)for(int f=0;f<kEffectControls;++f){const auto& spec=effectControlSpecs[f];
        if(p.fx[s].type==spec.type)p.fx[s].params[spec.key]=v[kLiveFxStart+s*kEffectControls+f];}
}
inline bool liveControlActive(const Patch& p,int index) {
    return index<kLiveFxStart || p.fx[(index-kLiveFxStart)/kEffectControls].type==effectControlSpecs[(index-kLiveFxStart)%kEffectControls].type;
}
}
