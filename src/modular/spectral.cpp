#include "runtime.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optisynth {
SpectralPtr spectralPreset(partials::Patch patch,const partials::TableCache& cache){
    partials::sanitise(patch);
    return std::make_shared<const SpectralPreset>(SpectralPreset{std::move(patch),cache});
}
partials::Patch spectralDemoPatch(){
    auto p=partials::defaultPatch();p.name="Optisynth / Spectral stereo";
    for(int h=1;h<=partials::kHarmonics;++h){
        p.osc[0].waveA.db[h]=-20.f*std::log10(float(h));
        p.osc[0].waveB.db[h]=(h&1)?p.osc[0].waveA.db[h]:partials::kSilentDb;
    }
    for(int i=0;i<partials::kCurvePoints;++i)p.osc[0].filter.curve.db[i]=-std::max(0.f,float(i-120))*2.f;
    p.osc[0].waveMix=.2f;p.osc[0].unisonVoices=3;p.osc[0].unisonDetune=9;p.osc[0].unisonStereo=.85f;
    p.osc[0].filter.on=true;p.osc[0].filter.cutoffHz=1800;p.osc[0].filter.envDepth=.2f;
    p.ampEnv[0].attack=.01f;p.ampEnv[0].sustain=.65f;p.ampEnv[0].release=.4f;
    p.mainEnv.release=.4f;p.lfo[0].rateHz=.8f;
    return p;
}
bool spectralParameterActive(const partials::Patch& p,int i){
    using namespace partials;
    if(i<0||i>=kLiveControls)return false;
    std::array<ModRoute,kMatrixSlots> routes{};const int count=resolveMatrix(p,routes);
    auto sourceUsed=[&](ModSource source,int index){for(int r=0;r<count;++r)if(routes[r].source==source&&routes[r].sourceIndex==index&&p.matrix[routes[r].slotIndex].amount!=0)return true;return false;};
    auto depthUsed=[&](int osc){if(p.osc[osc].filter.envDepth!=0)return true;for(int r=0;r<count;++r)if(routes[r].dest==ModDest::FilEnvDepth&&routes[r].destIndex==osc&&routes[r].amount!=0)return true;return false;};
    bool any=false;for(const auto& o:p.osc)any|=o.on;
    if(i<48){const auto& o=p.osc[i/12];const int field=i%12;if(!o.on)return false;
        if(field==0)return o.waveA.db!=o.waveB.db||o.waveA.phase!=o.waveB.phase;
        if(field==3||field==4||field==7)return o.filter.on;
        if(field>=8){const auto& x=field<10?o.xmod[field-8]:o.filter.xmod[field-10];return p.osc[x.source%4].on&&(field<10||(o.filter.on&&x.type==0));}
        return true;
    }
    if(i<53)return p.lfo[i-48].syncBeats==0&&sourceUsed(ModSource::Lfo,i-48);
    if(i==kLiveMain)return any;
    std::array<bool,kFxSlots> fxUsed{};
    for(const auto& o:p.osc)if(o.on){for(int route:{o.route1,o.route2}){for(int step=0;route>0&&route<=6&&step<6;++step){fxUsed[route-1]=true;route=p.fx[route-1].route;}}}
    if(i<60)return fxUsed[i-54]&&p.fx[i-54].on&&p.fx[i-54].type!="none";
    if(i<168){const int e=(i-60)/9;
        return e<4?p.osc[e].on:e<8?(p.osc[e-4].on&&p.osc[e-4].filter.on&&(depthUsed(e-4)||sourceUsed(ModSource::CutEnv,e-4))):
            e<11?sourceUsed(ModSource::ModEnv,e-8):any;
    }
    if(i<192){for(int r=0;r<count;++r)if(routes[r].slotIndex==i-168)return true;return false;}
    const int slot=(i-kLiveFxStart)/kEffectControls,field=(i-kLiveFxStart)%kEffectControls;
    const auto& fx=p.fx[slot];const auto& control=effectControlSpecs[field];
    if(!fxUsed[slot]||!fx.on||fx.type!=control.type)return false;
    auto value=[&](const char* key,float fallback=0.f){auto it=fx.params.find(key);return it==fx.params.end()?fallback:it->second;};
    if(value("wet",effectDefaultWet(fx.type))<=0)return false;
    const std::string key=control.key;
    if(key=="time_ms"&&(fx.type=="delay_mod")&&value("sync_beats")>0)return false;
    if(key=="time_l_ms"&&value("sync_l_beats")>0)return false;
    if(key=="time_r_ms"&&value("sync_r_beats")>0)return false;
    if(key=="time_c_ms"&&value("sync_c_beats")>0)return false;
    if(key=="rate_hz"&&value("sync_beats")>0)return false;
    if(key=="mod_rate_hz"&&(value(fx.type=="delay_mod"?"mod_sync_beats":"sync_beats")>0||value("mod_depth")<=0))return false;
    if(fx.type=="reverb"&&key=="mod_depth"&&value("modulation")<=.5f)return false;
    return true;
}
namespace {
class SpectralEngine final:public AudioModule {
public:
    SpectralEngine(const NodeSpec& n,SpectralPtr state):engine(48000){
        if(!state)state=spectralPreset(partials::defaultPatch());
        auto patch=state->patch;auto values=partials::liveValues(patch);const auto& ps=panel("spectral.engine").parameters;
        for(const auto& v:n.parameters){auto it=std::find_if(ps.begin(),ps.end(),[&](const Parameter& p){return p.id==v.first;});
            if(it==ps.end()||!std::isfinite(v.second)||v.second<it->minimum||v.second>it->maximum)throw std::invalid_argument("Invalid Spectral control");
            const auto index=size_t(it-ps.begin());if(index<values.size())values[index]=v.second;else tempo=v.second;
        }
        for(size_t i=0;i<values.size();++i){values[i]=std::clamp(values[i],ps[i].minimum,ps[i].maximum);controls->values[i]=values[i];}
        controls->values[values.size()]=tempo;partials::applyLiveValues(patch,values);
        engine.load(patch,state->tables,true);engine.setTempo(tempo);
        prepared=spectralPreset(state->patch,engine.tableCache());
    }
    SpectralPtr spectralState() const override{return prepared;}
    void process(const AudioBlock& b) noexcept override {
        partials::LiveValues values;for(size_t i=0;i<values.size();++i)values[i]=b.parameters[i];
        engine.setLiveControls(values);engine.setTempo(b.parameters[values.size()]);
        if(held>=0&&(!b.note.gate||b.note.trigger||held!=b.note.midi)){engine.noteOff(held);held=-1;}
        if(b.note.gate&&(held<0||b.note.trigger)){engine.noteOn({b.note.midi,b.note.velocity});held=b.note.midi;}
        engine.render(b.outputs[0],b.outputsRight[0],b.frames);
        const auto snapshot=engine.modulationSnapshot();
        for(size_t i=0;i<snapshot.size();++i)std::fill_n(b.outputs[i+1],b.frames,snapshot[i]);
    }
private:
    partials::Engine engine;SpectralPtr prepared;int held=-1;float tempo=120;
};
}
std::unique_ptr<PreparedModule> makeSpectral(const NodeSpec& n,SpectralPtr p){return std::make_unique<SpectralEngine>(n,std::move(p));}
}
