#pragma once
#include <JuceHeader.h>
#include "../src/modular/rack.h"
#include "../src/modular/sequencer.h"
#include "../src/modular/pattern.h"
#include "../src/modular/analog.h"
#include "../src/modular/reference_sequencers.h"
#include "PerformanceStateIO.h"
#include <set>

namespace optisynth {
struct SampleReference { std::string path,fingerprint;SamplePtr data; };
// Request 71: browser metadata, preferred view and synth-panel layout travel with the patch.
struct PresetInfo { std::string name,category,description; std::vector<std::string> tags; };
struct PanelControl { std::string node,parameter,label; };   // empty parameter: open the module's editor
struct PanelSection { std::string title; std::vector<PanelControl> controls; };
struct PanelLayout { std::string style; std::vector<PanelSection> sections; };
struct RackDocument { GraphSpec graph; float gain=.25f; std::map<std::string,SampleReference> samples; SpectralBank spectral; float tempo=120;
    PresetInfo info; std::string view; PanelLayout panel;
    std::vector<std::vector<std::string>> rows; };   // request 72: rack rows chosen by hand; empty = wrap to the window
inline SpectralPtr effectiveSpectral(const RackDocument& d,const std::string& id){
    auto state=d.spectral.find(id);if(state==d.spectral.end()||!state->second)return {};
    auto patch=state->second->patch;auto values=partials::liveValues(patch);const auto& specs=panel("spectral.engine").parameters;
    for(const auto& node:d.graph.nodes)if(node.id==id)for(const auto& v:node.parameters){
        const auto p=std::find_if(specs.begin(),specs.end(),[&](const Parameter& x){return x.id==v.first;});
        if(p!=specs.end()&&size_t(p-specs.begin())<values.size())values[size_t(p-specs.begin())]=v.second;
    }
    partials::applyLiveValues(patch,values);return spectralPreset(std::move(patch),state->second->tables);
}
inline const std::vector<std::string>& presetCategories() {
    static const std::vector<std::string> c{"Pad","Keys","Lead","Ostinato","Bass","Percussion","Sequence","Texture","Drone","Template"};return c;
}
inline const std::vector<std::string>& panelStyles() {static const std::vector<std::string> s{"graphite","slate","walnut","ivory"};return s;}
// Tags are lower-case words joined by hyphens, at most 32 characters, 16 per preset.
inline std::string normaliseTag(const std::string& text) {
    std::string t;bool gap=false;
    for(unsigned char c:text){
        if(std::isalnum(c)){if(gap&&!t.empty())t+='-';gap=false;t+=char(std::tolower(c));}
        else if(c=='-'||c=='_'||c==' '||c=='+'||c=='.')gap=true;
    }
    return t.substr(0,32);
}
inline std::vector<std::string> parseTags(const std::string& text) {
    std::vector<std::string> tags;size_t start=0;
    while(start<=text.size()){
        const auto end=std::min(text.find(',',start),text.size());auto tag=normaliseTag(text.substr(start,end-start));
        if(!tag.empty()&&std::find(tags.begin(),tags.end(),tag)==tags.end()&&tags.size()<16)tags.push_back(tag);
        start=end+1;
    }
    return tags;
}
inline bool isSequencerModule(const std::string& type){return isSequencer(type)||type=="seq.performance"||isReferenceSequencer(type);}
// Curated front panels for the four pad starters (padPatch node IDs).
inline PanelLayout padPanel(int model) {
    const bool prophet=model==1,xpander=model==2,drone=model==3;
    PanelLayout l;l.style=prophet?"walnut":model==0?"slate":xpander?"graphite":"ivory";
    auto vco=[](const char* title,const char* node){PanelSection s{title,{}};
        for(const char* p:{"transpose","fine","saw","pulse","triangle","pulse_width","drift"})s.controls.push_back({node,p,""});return s;};
    l.sections.push_back(vco("OSCILLATOR A","vco1"));l.sections.push_back(vco("OSCILLATOR B","vco2"));
    l.sections.push_back({"MIXER",{{"osc_mix","gain_a","Osc A"},{"osc_mix","gain_b","Osc B"}}});
    if(prophet)l.sections.push_back({"POLY-MOD",{{"polymod","osc_amount","Osc B"},{"polymod","env_amount","Filter env"},{"vco1","fm_depth","Freq A"},{"vco1","pwm_depth","PW A"},{"filter","fm_depth","Filter"}}});
    PanelSection f{"FILTER",{{"filter","cutoff_hz",""},{"filter","resonance",""},{"filter","env_octaves",""},{"filter","key_track",""},{"filter","drive",""}}};
    if(!prophet)f.controls.push_back({"filter","fm_depth","FM depth"});
    f.controls.push_back({"filter",prophet?"poles":xpander?"pole_mix":"mode",""});f.controls.push_back({"filter","quality",""});
    l.sections.push_back(f);
    auto env=[](const char* title,const char* node){return PanelSection{title,{{node,"attack",""},{node,"decay",""},{node,"sustain",""},{node,"release",""},{node,"velocity",""}}};};
    l.sections.push_back(env("FILTER ENVELOPE","filter_env"));l.sections.push_back(env("AMP ENVELOPE","amp_env"));
    PanelSection v{"VOICES",{{"voices","voices",""},{"voices","unison",""},{"voices","detune",""},{"voices","glide",""},{"voices","spread",""},{"voices","level",""}}};
    if(drone){v.controls.push_back({"voices","tuning",""});v.controls.push_back({"voices","root",""});}
    l.sections.push_back(v);
    l.sections.push_back({"MODULATION",{{"breath","rate","LFO rate"},{"breath","depth","LFO depth"},{"wheel","amount","Wheel"}}});
    PanelSection fx{"EFFECTS",{}};
    if(prophet)fx.controls={{"delay","mix","Delay"},{"delay","time_l_ms",""},{"delay","feedback",""}};
    else fx.controls={{"chorus","mix","Chorus"},{"chorus","rate_hz",""}};
    for(const char* p:{"mix","time_s","damping"})fx.controls.push_back({"reverb",p,std::string(p)=="mix"?"Reverb":""});
    l.sections.push_back(fx);return l;
}
// UTC revision time belongs to the sound; preserve it across unrelated app builds.
struct FactoryPreset { int starter; int64_t updatedSeconds; PresetInfo info; std::string view; };
// Browser order follows the former starter menu: instrument sounds, pads, then sequencer racks.
inline const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> list{
        {0,1794935289,{"VA oscillator + filter","Template","Variable-shape oscillator, envelope-swept low-pass filter and VCA. A plain subtractive starting point.",{"subtractive","starter"}},"modular"},
        {1,1794935289,{"Modal percussion","Percussion","A struck bank of 24 resonant modes with an AHDSFR envelope.",{"modal","physical-model","struck"}},"modular"},
        {2,1794935289,{"Drum into resonator","Percussion","Analog-style drum exciting a modal resonator, then a band-pass filter.",{"drum","resonator","physical-model"}},"modular"},
        {3,1794935289,{"Two oscillators + shared filter","Template","Two variable oscillators a fifth apart, mixed into one filter.",{"subtractive","starter","detuned"}},"modular"},
        {4,1794940108,{"Sample into resonator","Percussion","The built-in click sample exciting a modal resonator. Drop a WAV on the sample player.",{"sample","resonator","physical-model"}},"modular"},
        {5,1794940108,{"Sample attack + resonator","Keys","The sample attack blended with its resonated tail.",{"sample","resonator","layered"}},"modular"},
        {6,1794943185,{"Spectral stereo + external LFO","Texture","The full spectral engine in stereo, its filter swept by a rack LFO.",{"spectral","stereo","evolving"}},"modular"},
        {7,1794943185,{"Spectral audio + LFO into resonator","Texture","The spectral engine through a modal resonator; the engine's LFO moves resonator brightness.",{"spectral","resonator","evolving"}},"modular"},
        {21,1795378702,{"Reed squelch","Bass","Two struck optical gates blend a resonant saw and a short sub. Play short notes with varying velocity.",{"low-pass-gate","squelch","pluck"}},"panel"},
        {22,1795378702,{"Glass loops","Percussion","Struck modal glass into independent clocked echoes. Hold one side and play over it.",{"modal","looping-delay","glass"}},"panel"},
        {25,1795523516,{"Optical vapour","Pad","An eight-voice reed chord. Play seq for overlapping reed lines, or hold keys for the octave swell.",{"low-pass-gate","polyphonic","swell"}},"panel"},
        {40,1795734175,{"Seed rain","Texture","The reed chord falling through Mala's grains. Density sets how often seeds fall and Time how far back they reach. Press Freeze to hold the cloud.",{"granular","texture","reverb"}},"panel"},
        {24,1795523516,{"Prism afterimage","Ostinato","A pitched triangle mallet with a changing ceramic strike. Play seq for the four-bar mallet phrase and dotted echo.",{"mallet","modal","echo"}},"panel"},
        {23,1795378702,{"String afterglow","Pad","Slow drifting strings with ensemble movement and two independently held stereo echoes.",{"strings","swells","looping-delay"}},"panel"},
        {15,1795102287,{"OB swell","Pad","Oberheim-style 2-pole pad: two drifting saws, slow filter swell, chorus and long reverb. 8 voices.",{"analog","oberheim","swell","warm","ambient"}},"panel"},
        {16,1795102287,{"Prophet poly-mod","Pad","Prophet-style 4-pole pad; oscillator B and the filter envelope poly-modulate oscillator A and the filter. Stereo delay. 8 voices.",{"analog","prophet","poly-mod","evolving"}},"panel"},
        {19,1795118367,{"Xpander unison glide","Pad","Xpander-style HP2 + LP1 pole mix, 16 voices with unison 2 and glide.",{"analog","xpander","unison","glide","bright"}},"panel"},
        {20,1795118367,{"Just-intonation harmonic drone","Drone","Harmonic-series tuning on D, very slow envelopes and an 18 s reverb.",{"analog","just-intonation","drone","ambient","evolving"}},"panel"},
        {8,1795091315,{"Pattern / melodic voice","Sequence","Step pattern sequencer playing a filtered oscillator voice.",{"pattern","melodic","step"}},"modular"},
        {9,1795091315,{"Euclidean / modal percussion","Sequence","Euclidean rhythm generator striking modal percussion.",{"euclidean","percussion","modal"}},"modular"},
        {10,1795091315,{"Stage / pulsing voice","Sequence","Stage sequencer with per-stage pulses driving a voice.",{"stage","pulsing","step"}},"modular"},
        {11,1795096264,{"Motion / Night current","Sequence","Berlin-school motion sequence on the spectral engine; CV lanes move filter cutoff and wave mix.",{"berlin-school","motion","spectral","evolving","dark"}},"modular"},
        {12,1795096264,{"Motion / Glass tide","Sequence","Brighter, higher motion sequence with an 11-step pitch lane against a 32-step loop.",{"berlin-school","motion","spectral","bright"}},"modular"},
        {13,1795100331,{"Performance / Four currents","Sequence","Four-track performance sequencer: bass voice, spectral, glass and drum.",{"performance","multi-track","berlin-school"}},"modular"},
        {14,1795100331,{"Performance / Clockwork garden","Sequence","Four-track performance rack with swung, rooted patterns and chimes.",{"performance","multi-track","generative"}},"modular"},
        {17,1795105055,{"Probability / Glass branches","Sequence","Weighted-probability pitch grid driving the spectral motion voice.",{"probability","generative","spectral"}},"modular"},
        {18,1795105055,{"Orbit / Four satellites","Sequence","Four Euclidean rings with drift, each with its own accent lane.",{"orbit","euclidean","polyrhythm","generative"}},"modular"},
        {27,1795531650,{"Skin and vessel","Percussion","Turn Frame to udu while the pattern plays. Strike position moves from palm to rim or shell. Short gates let the hand lift off the pot.",{"frame-drum","udu","physical-model","hand-percussion"}},"panel"},
        {26,1795523931,{"Clay and water","Percussion","Four interlocking roles at 92 BPM. Fixed low strokes anchor softer rim, droplet and fingertip variations. Drop a local WAV on contact in the Modular view.",{"organic","drum-machine","physical-model","probability"}},"panel"}};
    return list;
}
inline const FactoryPreset* factoryPreset(int starter) {
    for(const auto& f:factoryPresets())if(f.starter==starter)return &f;return nullptr;
}
// Four bars in bank A. Tied steps sustain one note identity across the grid.
inline PerformancePtr prismPerformance(bool mallet) {
    auto data=std::make_shared<PerformanceData>();
    const std::array<std::string,4> names = mallet
        ? std::array<std::string,4>{"Mallet","Track 2","Track 3","Track 4"}
        : std::array<std::string,4>{"Bass reed","Inner reed","Middle reed","Upper reed"};
    for(int lane=0;lane<performanceTracks;++lane) {
        auto& track=data->tracks[lane];
        track.name=names[lane];
        for(auto& pattern:track.patterns) {
            pattern.last=63;
            pattern.scale=0;
            for(auto& step:pattern.steps)step.on=false;
        }
    }
    struct NoteSpan { int lane,start,length,pitch; float velocity; };
    auto write=[&](std::initializer_list<NoteSpan> notes) {
        for(const auto& n:notes) {
            auto& steps=data->tracks[n.lane].patterns[0].steps;
            for(int i=0;i<n.length;++i) {
                auto& step=steps[n.start+i];
                step.on=true;
                step.pitch=n.pitch;
                step.velocity=n.velocity;
                step.gate=1;
                step.tie=i+1<n.length;
            }
        }
    };
    if(mallet)write({
        {0,0,2,62,.90f},{0,4,1,69,.56f},{0,6,2,74,.77f},{0,11,1,64,.48f},{0,14,2,69,.66f},
        {0,16,2,62,.78f},{0,20,1,69,.53f},{0,22,3,77,.82f},{0,29,2,72,.60f},
        {0,32,2,65,.89f},{0,36,1,69,.55f},{0,38,2,76,.76f},{0,43,1,67,.49f},{0,46,2,72,.62f},
        {0,48,3,62,.83f},{0,54,2,69,.60f},{0,58,3,74,.74f}});
    else write({
        {0,0,16,50,.74f},{0,16,16,53,.68f},{0,32,14,48,.70f},{0,48,7,50,.68f},
        {1,1,28,57,.62f},{1,33,15,55,.56f},{1,49,7,57,.56f},
        {2,4,11,64,.63f},{2,18,12,65,.59f},{2,36,18,62,.64f},
        {3,8,16,69,.48f},{3,25,6,67,.53f},{3,40,8,64,.50f},{3,51,6,65,.48f}});
    return data;
}
inline RackDocument clayAndWater(float gain) {
    RackDocument d;
    d.gain=gain;
    d.tempo=92;
    auto data=std::make_shared<PerformanceData>();
    const char* names[]={"Palm","Wood rim","Water","Top line"};
    for(int t=0;t<4;++t){
        auto& track=data->tracks[t];
        track.name=names[t];
        for(auto& bank:track.patterns){
            bank.scale=0;
            bank.last=31;
            bank.swing=t==0?0:.065f;
            for(auto& step:bank.steps)step.on=false;
        }
    }
    auto hit=[&](int t,int at,int pitch,float velocity,float chance=1.f){
        auto& step=data->tracks[t].patterns[0].steps[at];
        step.on=true;step.pitch=pitch;step.velocity=velocity;step.chance=chance;
        step.cv=velocity-.5f;
    };
    for(int bar:{0,16}){
        hit(0,bar,45,.92f);hit(0,bar+10,48,.67f);
        hit(1,bar+4,58,.72f);hit(1,bar+12,60,.78f);hit(1,bar+15,62,.29f,.48f);
        hit(2,bar+6,65,.52f);hit(2,bar+13,69,.38f,.6f);
        hit(3,bar+2,58,.52f);hit(3,bar+7,62,.30f,.65f);hit(3,bar+11,60,.42f);
    }
    hit(0,22,50,.38f,.58f);hit(2,31,72,.35f,.42f);hit(3,30,63,.34f,.55f);
    d.graph.nodes={{"sequence","seq.performance",{},data},
        {"palm","drum.analog",{{"transpose",-5},{"attack_fm",.26f},{"self_fm",.12f},{"decay",.24f},{"tone",.30f}}},
        {"rim","modal.voice",{{"structure",.25f},{"brightness",.43f},{"decay",.19f}}},
        {"water","drum.droplet",{{"size",1.15f},{"decay",.21f},{"rise",.48f},{"splash",.23f}}},
        {"contact","sample.player",{{"root_note",60},{"level",.8f}}},
        {"shell","modal.resonator",{{"structure",.36f},{"decay",.24f},{"brightness",.43f},{"input_gain",.9f}}},
        {"fingers","utility.mix",{{"gain_a",.36f},{"gain_b",.8f}}},
        {"hands","utility.mix",{{"gain_a",.73f},{"gain_b",.47f}}},
        {"accents","utility.mix",{{"gain_a",.40f},{"gain_b",.67f}}},
        {"kit","utility.mix",{{"gain_a",.85f},{"gain_b",.8f}}}};
    d.samples["contact"]={"builtin:click","builtin-click-v1",{}};
    d.graph.connections={{{"sequence","t1_notes"},{"palm","notes"}},{{"sequence","t2_notes"},{"rim","notes"}},
        {{"sequence","t3_notes"},{"water","notes"}},{{"sequence","t4_notes"},{"contact","notes"}},
        {{"sequence","t4_notes"},{"shell","notes"}},{{"contact","out"},{"shell","in"}},
        {{"contact","out"},{"fingers","a"}},{{"shell","out"},{"fingers","b"}},
        {{"palm","out"},{"hands","a"}},{{"rim","out"},{"hands","b"}},
        {{"water","out"},{"accents","a"}},{{"fingers","out"},{"accents","b"}},
        {{"hands","out"},{"kit","a"}},{{"accents","out"},{"kit","b"}}};
    d.graph.modulations={{{"sequence","t2_cv"},{"rim","brightness"},.25f}};
    d.graph.outputs={{"kit","out"}};
    d.panel={"walnut",{
        {"PALM",{{"hands","gain_a","Level"},{"palm","attack_fm","Attack FM"},{"palm","decay","Decay"}}},
        {"WOOD RIM",{{"hands","gain_b","Level"},{"rim","structure","Material"},{"rim","brightness","Brightness"}}},
        {"WATER",{{"accents","gain_a","Level"},{"water","size","Size"},{"water","rise","Pitch rise"},{"water","decay","Ring time"},{"water","splash","Surface"}}},
        {"TOP LINE",{{"accents","gain_b","Level"},{"fingers","gain_a","Direct"},{"fingers","gain_b","Shell"},{"shell","decay","Decay"}}}}};
    return d;
}
inline RackDocument starterDocument(int which,float gain) {
    // Programme item 1 (request 136): the open Beads design as Mala, fed by the Optical vapour reeds.
    if(which==40){
        auto d=starterDocument(25,gain);
        for(auto& n:d.graph.nodes)if(n.id=="prism")
            n=NodeSpec{"mala","fx.granular_tape",{{"density",.72f},{"time",.32f},{"time_amt",.3f},{"size",.64f},{"size_amt",.2f},
                {"shape",.55f},{"feedback",.28f},{"dry_wet",.58f},{"reverb",.42f},{"quality",2}}};
        for(auto& c:d.graph.connections)for(auto* e:{&c.source,&c.destination})if(e->node=="prism")e->node="mala";
        d.graph.outputs={{"mala","out"}};
        d.panel.sections.erase(std::remove_if(d.panel.sections.begin(),d.panel.sections.end(),[](const PanelSection& s){return s.title=="SPACE";}),d.panel.sections.end());
        d.panel.sections.push_back({"MALA",{{"mala","density","Density"},{"mala","time","Time"},{"mala","size","Size"},{"mala","shape","Shape"},
            {"mala","pitch","Pitch"},{"mala","feedback","Feedback"},{"mala","dry_wet","Dry/wet"},{"mala","reverb","Reverb"},{"mala","freeze","Freeze"}}});
        return d;
    }
    if(which==27){
        RackDocument d;d.gain=gain;d.tempo=92;
        auto data=std::make_shared<PerformanceData>();
        for(auto& track:data->tracks){
            for(auto& bank:track.patterns){
                bank.scale=0;bank.last=31;
                for(auto& step:bank.steps)step.on=false;
            }
        }
        auto& hand=data->tracks[0];hand.name="Hand strokes";
        for(int t=1;t<4;++t)data->tracks[t].name="Track "+std::to_string(t+1);
        // Position CV alternates palms with fingers; the gate is hand contact time.
        struct Hit {int step,pitch;float velocity,gate,position;};
        for(const Hit h:std::initializer_list<Hit>{
            {0,46,.93f,.23f,-.52f},{3,46,.42f,.68f,.58f},{6,48,.73f,.38f,-.28f},
            {10,46,.81f,.86f,-.41f},{12,51,.54f,.28f,.68f},{15,48,.31f,.44f,.45f},
            {16,46,.94f,.20f,-.54f},{20,51,.51f,.72f,.66f},{22,48,.70f,.34f,-.20f},
            {26,46,.77f,.79f,-.37f},{29,51,.49f,.23f,.60f}}){
            auto& step=hand.patterns[0].steps[h.step];step.on=true;step.pitch=h.pitch;
            step.velocity=h.velocity;step.gate=h.gate;step.cv=h.position;
        }
        d.graph.nodes={{"hand","drum.skin_vessel",{{"body",.48f},{"position",.43f}}},
            {"voices","voice.mix",{{"voices",5},{"spread",.1f},{"level",.9f}}},
            {"sequence","seq.performance",{},data}};
        d.graph.connections={{{"hand","out"},{"voices","in"}},{{"sequence","t1_notes"},{"voices","notes"}}};
        d.graph.modulations={{{"sequence","t1_cv"},{"hand","position"},.54f}};
        d.graph.outputs={{"voices","out"}};
        d.panel={"walnut",{{"BODY",{{"hand","body","Frame to udu"},{"hand","position","Strike position"},{"hand","damp","Hand damping"}}},
            {"TOUCH",{{"hand","touch","Hardness"},{"hand","opening","Hole opening"},{"hand","lift","Hand lift"}}}}};
        return d;
    }
    if(which==26)return clayAndWater(gain);
    if(which==24 || which==25) {
        RackDocument d;
        d.gain=gain;
        if(which==24) {
            d.tempo=100;
            d.graph.nodes={
                {"body","analog.vco",{{"saw",0},{"triangle",1},{"level",.86f},{"drift",.05f},{"fm_depth",.42f}}},
                {"harmonic","osc.sine",{{"transpose",19.02f},{"level",.87f}}},
                {"impact","analog.env",{{"attack",.001f},{"decay",.105f},{"sustain",0},{"release",.09f},{"velocity",1}}},
                {"fm_index","utility.vca",{}},
                {"strike","modal.voice",{{"structure",.18f},{"brightness",.46f},{"decay",.19f}}},
                {"blend","utility.mix",{{"gain_a",.88f},{"gain_b",.30f}}},
                {"timbre","mod.lfo",{{"rate",.38f}}},
                {"amp_env","analog.env",{{"attack",.006f},{"decay",.48f},{"sustain",.085f},{"release",.31f},{"velocity",.72f}}},
                {"vca","utility.vca",{}},{"voices","voice.mix",{{"voices",6},{"level",.93f},{"spread",.19f}}},
                {"prism","fx.prism_tank",{{"sync",3},{"mix",.26f},{"echo",1},{"feedback",0},{"tank",0},
                    {"size",.49f},{"decay",.29f},{"diffusion",.43f},{"tone",.12f},{"shift",0},{"motion",.035f}}}};
            d.graph.connections={{{"harmonic","out"},{"fm_index","in"}},{{"impact","out"},{"fm_index","gain"}},
                {{"fm_index","out"},{"body","fm"}},{{"body","out"},{"blend","a"}},{{"strike","out"},{"blend","b"}},
                {{"blend","out"},{"vca","in"}}};
            d.graph.modulations={{{"timbre","out"},{"strike","brightness"},.22f},
                                 {{"timbre","out"},{"blend","gain_b"},.12f}};
            d.panel={"ivory",{{"MALLET",{{"body","fm_depth","Hardness"},{"blend","gain_b","Strike"},{"strike","structure","Material"},
                {"strike","brightness","Brightness"},{"timbre","rate","Colour rate"}}},
                {"DOTTED RETURN",{{"prism","sync","Division"},{"prism","mix","Echo mix"},{"prism","feedback","Repeats"}}}}};
        } else {
            d.tempo=80;
            d.graph.nodes={
                {"reed","analog.vco",{{"saw",.22f},{"pulse",.36f},{"triangle",.9f},{"pulse_width",.39f},
                    {"pwm_depth",.14f},{"drift",.18f},{"level",.64f}}},
                {"overtone","analog.vco",{{"transpose",12},{"fine",3.5f},{"saw",.21f},{"triangle",1},{"drift",.26f},{"level",.43f}}},
                {"rise","analog.env",{{"attack",1.45f},{"decay",2.8f},{"sustain",.77f},{"release",1.45f},{"velocity",.33f}}},
                {"gate","analog.dual_lpg",{{"a_control",.59f},{"a_damp",.36f},{"a_keys",.17f},
                    {"b_control",0},{"b_damp",.27f},{"b_keys",0}}},
                {"breath","mod.lfo",{{"rate",.16f}}},{"swell","mod.lfo",{{"rate",.093f}}},
                {"amp_env","analog.env",{{"attack",.16f},{"decay",1.1f},{"sustain",.79f},{"release",1.65f},{"velocity",.55f}}},
                {"vca","utility.vca",{}},{"voices","voice.mix",{{"voices",8},{"level",.73f},{"spread",.44f}}},
                {"prism","fx.prism_tank",{{"mix",.14f},{"predelay",37},{"size",.71f},{"decay",.59f},
                    {"echo",0},{"feedback",0},{"shift",0},{"tone",.13f},{"diffusion",.70f},{"motion",.08f}}}};
            d.graph.connections={{{"reed","out"},{"gate","a_in"}},{{"overtone","out"},{"gate","b_in"}},
                {{"rise","out"},{"gate","b_cv"}},{{"breath","out"},{"reed","pwm"}},{{"gate","sum"},{"vca","in"}}};
            d.graph.modulations={{{"swell","out"},{"gate","a_control"},.19f}};
            d.panel={"slate",{{"REED",{{"gate","a_control","Opening"},{"gate","a_damp","Damp"},{"reed","pwm_depth","Pulse motion"},
                {"breath","rate","Pulse rate"},{"swell","rate","Gate rate"}}},
                {"OCTAVE BLOOM",{{"overtone","level","Level"},{"rise","attack","Rise"},{"rise","release","Fade"},{"overtone","fine","Detune"}}}}};
        }
        d.graph.nodes.push_back({"sequence","seq.performance",{},prismPerformance(which==24)});
        d.graph.connections.push_back({{"sequence","all_notes"},{"voices","notes"}});
        d.graph.connections.push_back({{"amp_env","out"},{"vca","gain"}});
        d.graph.connections.push_back({{"vca","out"},{"voices","in"}});
        d.graph.connections.push_back({{"voices","out"},{"prism","in"}});
        d.graph.outputs={{"prism","out"}};
        d.panel.sections.push_back({"AMP ADSR",{{"amp_env","attack","Attack"},{"amp_env","decay","Decay"},
                                                {"amp_env","sustain","Sustain"},{"amp_env","release","Release"}}});
        if(which==25)d.panel.sections.push_back({"SPACE",{{"prism","mix","Mix"},{"prism","decay","Regeneration"},{"prism","freeze","Freeze"}}});
        return d;
    }
    if(which>=21 && which<=23) {
        RackDocument d;
        d.gain=gain;
        const PanelSection ampEnvelope{"AMP ADSR",{{"amp_env","attack","Attack"},{"amp_env","decay","Decay"},
                                                   {"amp_env","sustain","Sustain"},{"amp_env","release","Release"}}};
        if(which==21) {
            d.graph.nodes={{"reed","analog.vco",{{"level",.9f}}},{"sub","osc.sine",{{"transpose",-12},{"level",.5f}}},
                {"sweep","analog.env",{{"attack",.002f},{"decay",.17f},{"sustain",0},{"release",.12f},{"velocity",.8f}}},
                {"filter","analog.filter",{{"cutoff_hz",380},{"resonance",.8f},{"env_octaves",3.2f},{"drive",.36f}}},
                {"amp_env","analog.env",{{"attack",.003f},{"decay",.24f},{"sustain",.7f},{"release",.14f},{"velocity",0}}},
                {"vca","utility.vca",{}},
                {"gate","analog.dual_lpg",{{"a_control",.16f},{"a_damp",.23f},{"b_damp",.65f},{"b_keys",.75f}}},
                {"voices","voice.mix",{{"voices",1},{"level",1},{"spread",0}}}};
            d.graph.connections={{{"reed","out"},{"filter","in"}},{{"sweep","out"},{"filter","mod"}},
                {{"sub","out"},{"filter","fm"}},{{"filter","out"},{"gate","a_in"}},{{"sub","out"},{"gate","b_in"}},{{"gate","sum"},{"vca","in"}},
                {{"amp_env","out"},{"vca","gain"}},{{"vca","out"},{"voices","in"}}};
            d.graph.outputs={{"voices","out"}};
            d.panel={"graphite",{{"REED",{{"filter","cutoff_hz","Cutoff"},{"filter","resonance","Resonance"},{"sweep","decay","Sweep decay"},{"filter","env_octaves","Sweep depth"},{"filter","fm_depth","FM depth"}}}}};
            d.panel.sections.push_back(ampEnvelope);
            for(const std::string ch : {"a","b"})
                d.panel.sections.push_back({ch=="a"?"GATE A / REED":"GATE B / SUB",
                    {{"gate",ch+"_control","Open"},{"gate",ch+"_damp","Damp"},{"gate",ch+"_keys","Key strike"}}});
            return d;
        }
        if(which==22) {
            d.tempo=108;
            d.graph.nodes={{"glass","modal.voice",{{"transpose",12},{"structure",.64f},{"brightness",.74f},{"decay",.29f}}},
                           {"amp_env","analog.env",{{"attack",.001f},{"decay",.35f},{"sustain",.85f},{"release",.65f},{"velocity",0}}},
                           {"vca","utility.vca",{}},
                           {"voices","voice.mix",{{"voices",6},{"level",1},{"spread",.3f}}},
                           {"delay","fx.dual_loop",{{"a_mix",.49f},{"b_mix",.53f},{"b_feedback",.58f}}}};
            d.graph.connections={{{"glass","out"},{"vca","in"}},{{"amp_env","out"},{"vca","gain"}},
                                 {{"vca","out"},{"voices","in"}},{{"voices","out"},{"delay","in"}}};
            d.panel={"ivory",{{"STRUCK GLASS",{{"glass","structure","Material"},{"glass","brightness","Brightness"},{"glass","decay","Ring"}}}}};
        } else {
            d.graph=padPatch(0);
            d.graph.nodes.push_back({"delay","fx.dual_loop",{{"a_time",5},{"b_time",6},{"a_mix",.21f},{"b_mix",.26f}}});
            d.graph.connections.push_back({{"reverb","out"},{"delay","in"}});
            d.panel={"slate",{{"STRINGS",{{"vco2","fine","Detune"},{"filter_env","attack","Filter rise"},{"filter","fm_depth","FM depth"},
                {"chorus","mix","Chorus"},{"reverb","mix","Reverb"}}}}};
        }
        d.panel.sections.push_back(ampEnvelope);
        d.graph.outputs={{"delay","out"}};
        for(const std::string ch : {"a","b"})
            d.panel.sections.push_back({ch=="a"?"LOOP A / LEFT":"LOOP B / RIGHT",{{"delay",ch+"_time","Division"},
                {"delay",ch+"_feed","Record feed"},{"delay",ch+"_feedback","Feedback"},{"delay",ch+"_mix","Mix"},
                {"delay",ch+"_hold","Hold"},{"delay",ch+"_reverse","Reverse"}}});
        return d;
    }
    if(which==17){auto d=starterDocument(11,gain);d.tempo=108;for(auto& n:d.graph.nodes)if(n.id=="sequence"){n.type="seq.probability";n.parameters={{"octave",4},{"density",.9f},{"swing",.12f}};n.performance.reset();}
        for(auto& m:d.graph.modulations)if(m.source.node=="sequence"){m.source.port="pitch";m.amount*=.4f;}return d;}
    if(which==18){auto d=starterDocument(13,gain);d.tempo=116;for(auto& n:d.graph.nodes)if(n.id=="sequence"){n.type="seq.orbit";n.parameters={{"ring2_drift",1},{"ring4_drift",-1}};n.performance.reset();}
        d.graph.connections.erase(std::remove_if(d.graph.connections.begin(),d.graph.connections.end(),[](const Connection& c){return c.source.node=="sequence"&&c.source.port=="t1_bend";}),d.graph.connections.end());
        for(auto& c:d.graph.connections)if(c.source.node=="sequence")c.source.port="r"+c.source.port.substr(1);
        for(auto& m:d.graph.modulations)if(m.source.node=="sequence")m.source.port="r"+m.source.port.substr(1,1)+"_accent";return d;}

    if(which==15||which==16||which==19||which==20){RackDocument d;d.gain=gain;d.graph=padPatch(which<19?which-15:which-17);return d;}
    if(which==13||which==14){
        auto d=starterDocument(11,gain);d.tempo=which==13?104:118;
        auto data=std::make_shared<PerformanceData>(defaultPerformance());
        if(which==14){data->seed=8107;for(auto& track:data->tracks)for(auto& p:track.patterns){p.traversal=3;p.root=2;p.swing=.12f;}data->tracks[2].name="Chimes";}
        for(auto& p:data->tracks[3].patterns)for(auto& s:p.steps)s.pitch=48;
        d.graph.nodes={{"sequence","seq.performance",{},data},{"bass","osc.variable",{{"shape",.3f},{"level",.28f}}},
            {"bass_env","spectral.adsr",{{"attack",.003f},{"decay",.19f},{"sustain",.35f},{"release",.2f}}},
            {"bass_filter","mutable.lowpass",{{"cutoff_hz",900},{"resonance",1.8f},{"env_octaves",2}}},{"bass_vca","utility.vca",{}},
            {"spectral","spectral.engine",{}},{"glass","modal.voice",{{"structure",.45f},{"brightness",.45f},{"decay",.35f}}},
            {"drum","drum.analog",{{"decay",.2f},{"attack_fm",.22f}}},
            {"rhythm_mix","utility.mix",{{"gain_a",.5f},{"gain_b",.35f}}},{"voice_mix","utility.mix",{{"gain_a",.7f},{"gain_b",.5f}}},
            {"pair","utility.stereo",{}},{"output","utility.stereo_mix",{{"gain_a",.7f},{"gain_b",.6f}}}};
        d.graph.connections={{{"sequence","t1_notes"},{"bass","notes"}},{{"sequence","t1_notes"},{"bass_env","notes"}},{{"sequence","t1_bend"},{"bass","bend"}},
            {{"bass","out"},{"bass_filter","in"}},{{"bass_env","out"},{"bass_filter","mod"}},{{"bass_filter","out"},{"bass_vca","in"}},{{"bass_env","out"},{"bass_vca","gain"}},
            {{"sequence","t2_notes"},{"spectral","notes"}},{{"sequence","t3_notes"},{"glass","notes"}},{{"sequence","t4_notes"},{"drum","notes"}},
            {{"glass","out"},{"rhythm_mix","a"}},{{"drum","out"},{"rhythm_mix","b"}},{{"bass_vca","out"},{"voice_mix","a"}},{{"rhythm_mix","out"},{"voice_mix","b"}},
            {{"voice_mix","out"},{"pair","left"}},{{"voice_mix","out"},{"pair","right"}},{{"pair","out"},{"output","a"}},{{"spectral","out"},{"output","b"}}};
        d.graph.modulations={{{"sequence","t1_cv"},{"bass_filter","cutoff_hz"},.08f},{{"sequence","t2_cv"},{"spectral","osc1_filter_cutoff"},.09f},{{"sequence","t3_cv"},{"glass","brightness"},.15f},{{"sequence","t4_cv"},{"drum","tone"},.12f}};
        d.graph.outputs={{"output","out"}};return d;
    }
    if(which==11||which==12){
        RackDocument d;d.gain=gain;d.tempo=which==11?96:112;
        auto sound=spectralDemoPatch();sound.name=which==11?"Motion / Night current":"Motion / Glass tide";
        sound.mainVolumeDb=-12;sound.osc[0].unisonVoices=2;sound.osc[0].route1=1;sound.osc[0].filter.cutoffHz=which==11?950:2200;
        sound.ampEnv[0].attack=.003f;sound.ampEnv[0].decay=.24f;sound.ampEnv[0].sustain=.18f;sound.ampEnv[0].release=.28f;
        sound.mainEnv.release=.3f;
        sound.fx[0]={"delay_stereo",true,2,{{"wet",.32f},{"time_l_ms",float(45000/d.tempo)},{"time_r_ms",float(60000/d.tempo)},{"feedback",.42f},{"highpass_hz",180},{"lowpass_hz",5200},{"cross",1}}};
        sound.fx[1]={"reverb",true,0,{{"wet",.24f},{"time_s",4.8f},{"damping",.4f}}};
        if(which==12){sound.osc[0].waveMix=.85f;sound.osc[0].unisonDetune=4;}
        d.spectral["spectral"]=spectralPreset(sound);
        NodeSpec sequence{"sequence","seq.pattern",{{"length",32},{"pitch_length",which==11?7.f:11.f},{"velocity_length",5},{"cv1_length",11},{"cv2_length",7},{"scale",2},{"gate",.68f},{"swing",.08f},{"cv_slew",.035f},{"seed",713},{"keyboard_transpose",1}}};
        constexpr int notes[]={48,55,60,58,51,63,55,67,60,58,51};
        for(int i=0;i<64;++i){
            sequence.parameters.push_back({patternKey(i,Pitch),float(notes[i%11]+(which==12?12:0))});
            sequence.parameters.push_back({patternKey(i,Velocity),i%4==0?1.f:.65f});
            sequence.parameters.push_back({patternKey(i,On),i%8==6?0.f:1.f});
            sequence.parameters.push_back({patternKey(i,Chance),i%4==3?.72f:1.f});
            sequence.parameters.push_back({patternKey(i,Ratchet),i%16==15?3.f:1.f});
            sequence.parameters.push_back({patternKey(i,Condition),i%16==14?8.f:0.f});
            sequence.parameters.push_back({patternKey(i,CV1),float(std::sin(i*1.7)*.65)});
            sequence.parameters.push_back({patternKey(i,CV2),float(std::cos(i*.77)*.7)});
        }
        d.graph.nodes={std::move(sequence),{"spectral","spectral.engine",{}}};
        d.graph.connections={{{"sequence","notes"},{"spectral","notes"}}};d.graph.outputs={{"spectral","out"}};
        d.graph.modulations={{{"sequence","cv1"},{"spectral","osc1_filter_cutoff"},.10f},{{"sequence","cv2"},{"spectral","osc1_wave_mix"},.18f}};
        return d;
    }
    if(which>=8&&which<=10)return {sequencerPatch(which-8),gain};
    RackDocument d{rackPreset(which),gain,{}};
    if(which==4||which==5)d.samples["source"]={"builtin:click","builtin-click-v1",{}};
    if(which>=6){
        d.graph.nodes={{"spectral","spectral.engine",{}}};d.graph.connections.clear();d.graph.outputs={{"spectral","out"}};
        d.spectral["spectral"]=spectralPreset(spectralDemoPatch());
        if(which==6){d.graph.nodes.push_back({"lfo","mod.lfo",{{"rate",.3f}}});d.graph.modulations={{{"lfo","out"},{"spectral","osc1_filter_cutoff"},.16f}};}
        else {d.graph.nodes.push_back({"mono","utility.downmix",{}});d.graph.nodes.push_back({"resonator","modal.resonator",{{"input_gain",.3f}}});
            d.graph.connections={{{"spectral","out"},{"mono","in"}},{{"mono","out"},{"resonator","in"}}};d.graph.outputs={{"resonator","out"}};
            d.graph.modulations={{{"spectral","lfo1"},{"resonator","brightness"},.25f}};
        }
    }
    return d;
}
// A factory starter with its browser metadata, preferred view and curated panel.
inline RackDocument factoryDocument(int which,float gain) {
    auto d=starterDocument(which,gain);
    if(const auto* f=factoryPreset(which)){d.info=f->info;d.view=f->view;}
    if(which==15||which==16||which==19||which==20)d.panel=padPanel(which<19?which-15:which-17);
    return d;
}
// Instrument starters provide a new sound graph. Carry the edited note generator
// into that graph when requested. Sequencer starters supply their own patterns.
inline RackDocument chooseStarter(const RackDocument& current, int index, bool keepSequence) {
    auto next = factoryDocument(index, current.gain);
    auto isSequence = [](const NodeSpec& n) { return n.type.rfind("seq.", 0) == 0; };
    if (!keepSequence || std::any_of(next.graph.nodes.begin(), next.graph.nodes.end(), isSequence))
        return next;
    const auto& nodes = current.graph.nodes;
    auto seq = std::find_if(nodes.begin(), nodes.end(), isSequence);
    if (seq == nodes.end()) return next;
    if (std::any_of(std::next(seq), nodes.end(), isSequence))
        throw std::invalid_argument("Multiple sequencers in this rack. Save it and turn off Keep sequence to load a starter.");
    if (std::any_of(next.graph.nodes.begin(), next.graph.nodes.end(), [&](const NodeSpec& n){ return n.id == seq->id; }))
        throw std::invalid_argument("Sequencer ID conflicts with this starter. Current rack kept.");

    const bool polyphonic = std::any_of(next.graph.nodes.begin(), next.graph.nodes.end(),
        [](const NodeSpec& n){ return n.type == "voice.mix"; });
    const auto registry = moduleRegistry();
    const bool usedAllNotes=std::any_of(current.graph.connections.begin(),current.graph.connections.end(),
        [&](const Connection& c){return c.source.node==seq->id && c.source.port=="all_notes";});
    const auto port = usedAllNotes || seq->type == "seq.orbit" ? "all_notes" :
                      seq->type == "seq.performance" ? "t1_notes" : "notes";
    for (const auto& node : next.graph.nodes) {
        if (polyphonic && node.type != "voice.mix") continue;
        const auto type = std::find_if(registry.begin(), registry.end(),
            [&](const ModuleType& t){ return t.id == node.type; });
        if (type != registry.end() && std::any_of(type->inputs.begin(), type->inputs.end(),
            [](const Port& p){ return p.type == SignalType::Event && p.id == "notes"; }))
            setRackConnection(next.graph, {node.id, "notes"}, {seq->id, port});
    }
    next.graph.nodes.insert(next.graph.nodes.begin(), *seq);
    next.tempo = current.tempo;
    return next;
}
inline void pruneSampleReferences(RackDocument& d) {
    for(auto it=d.samples.begin();it!=d.samples.end();){
        if(std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==it->first&&n.type=="sample.player";}))it=d.samples.erase(it);
        else ++it;
    }
    for(auto it=d.spectral.begin();it!=d.spectral.end();){
        if(std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==it->first&&n.type=="spectral.engine";}))it=d.spectral.erase(it);else ++it;
    }
    for(auto& n:d.graph.nodes)if(n.type=="spectral.engine"){
        auto& preset=d.spectral[n.id];if(!preset)preset=spectralPreset(partials::defaultPatch());
        const auto values=partials::liveValues(preset->patch);const auto& specs=panel(n.type).parameters;
        for(size_t i=0;i<values.size();++i)if(std::none_of(n.parameters.begin(),n.parameters.end(),[&](const auto& p){return p.first==specs[i].id;}))
            n.parameters.push_back({specs[i].id,std::clamp(values[i],specs[i].minimum,specs[i].maximum)});
    }
}
// "vco1" -> "VCO 1", "filter_env" -> "FILTER ENV".
inline std::string panelTitle(const std::string& id) {
    std::string t;
    for(size_t i=0;i<id.size();++i){const unsigned char c=static_cast<unsigned char>(id[i]);
        if(c=='_'||c=='-'||c=='.'){t+=' ';continue;}
        if(std::isdigit(c)&&i>0&&std::isalpha(static_cast<unsigned char>(id[i-1])))t+=' ';
        t+=char(std::toupper(c));}
    return t;
}
inline const Parameter* findParameter(const std::string& type,const std::string& id) {
    for(const auto& p:panel(type).parameters)if(p.id==id)return &p;return nullptr;
}
inline bool panelControlValid(const RackDocument& d,const PanelControl& c) {
    const auto n=std::find_if(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& x){return x.id==c.node;});
    if(n==d.graph.nodes.end())return false;
    return c.parameter.empty()?isSequencerModule(n->type):findParameter(n->type,c.parameter)!=nullptr;
}
// Patches without a curated layout: one section per module in rack order.
inline PanelLayout autoPanel(const RackDocument& d) {
    PanelLayout l{"graphite",{}};
    for(const auto& n:d.graph.nodes){
        PanelSection s{panelTitle(n.id),{}};
        if(isSequencerModule(n.type))s.controls.push_back({n.id,"",""});
        else if(n.type=="spectral.engine"){
            for(const char* p:{"main_volume","osc1_wave_mix","osc1_filter_cutoff","osc1_filter_resonance","osc1_filter_envelope","lfo1_rate"})s.controls.push_back({n.id,p,""});
        } else for(const auto& p:panel(n.type).parameters){
            PanelControl c{n.id,p.id,""};
            if(n.type=="utility.mix"||n.type=="utility.stereo_mix"){const std::string port=p.id=="gain_a"?"a":"b";
                for(const auto& link:d.graph.connections)if(link.destination.node==n.id&&link.destination.port==port)c.label=panelTitle(link.source.node);}
            s.controls.push_back(std::move(c));
        }
        if(!s.controls.empty())l.sections.push_back(std::move(s));
    }
    return l;
}
inline const PanelLayout& effectivePanel(const RackDocument& d,PanelLayout& scratch) {
    if(!d.panel.sections.empty())return d.panel;scratch=autoPanel(d);return scratch;
}
// Rows keep only existing modules; new modules join the last row.
inline void pruneRows(RackDocument& d) {
    if(d.rows.empty())return;std::set<std::string> placed;
    for(auto& row:d.rows)row.erase(std::remove_if(row.begin(),row.end(),[&](const std::string& id){
        return placed.count(id)||std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id;})||!placed.insert(id).second;}),row.end());
    d.rows.erase(std::remove_if(d.rows.begin(),d.rows.end(),[](const auto& row){return row.empty();}),d.rows.end());
    for(const auto& n:d.graph.nodes)if(!placed.count(n.id)){if(d.rows.empty())d.rows.emplace_back();d.rows.back().push_back(n.id);}
}
// Module replacement or removal can orphan curated controls; drop them.
inline void prunePanel(RackDocument& d) {
    for(auto& s:d.panel.sections)s.controls.erase(std::remove_if(s.controls.begin(),s.controls.end(),[&](const PanelControl& c){return !panelControlValid(d,c);}),s.controls.end());
    d.panel.sections.erase(std::remove_if(d.panel.sections.begin(),d.panel.sections.end(),[](const PanelSection& s){return s.controls.empty();}),d.panel.sections.end());
}
inline SampleBank sampleBank(const RackDocument& d) {
    SampleBank b;for(const auto& s:d.samples)if(s.second.data)b[s.first]=s.second.data;return b;
}
inline bool needsSamples(const RackDocument& d) {
    auto plan=compileGraph(d.graph,moduleRegistry());
    for(auto i:plan.nodes){if(d.graph.nodes[i].type=="spectral.engine")return true;auto it=d.samples.find(d.graph.nodes[i].id);if(it!=d.samples.end()&&!it->second.data)return true;}return false;
}
inline void prepareSamples(RackDocument& d,const CancelSample& cancelled={}) {
    validateRack(d.graph);const auto plan=compileGraph(d.graph,moduleRegistry());
    std::set<std::string> active;for(auto i:plan.nodes)active.insert(d.graph.nodes[i].id);
    for(auto& s:d.spectral)if(!active.count(s.first)&&s.second)s.second=spectralPreset(s.second->patch);
    std::map<std::string,SamplePtr> loaded;std::set<const SampleData*> counted;size_t bytes=0;
    for(auto& s:d.samples){auto& ref=s.second;
        if(cancelled&&cancelled())throw std::runtime_error("Sample load cancelled");
        if(!active.count(s.first)){ref.data.reset();continue;}
        if(!ref.data){auto it=loaded.find(ref.path);ref.data=it!=loaded.end()?it->second:ref.path=="builtin:click"?demoSample():loadWave(ref.path,cancelled);loaded[ref.path]=ref.data;}
        if(!ref.fingerprint.empty()&&ref.fingerprint!=ref.data->fingerprint)throw std::runtime_error("Sample file changed: "+ref.path+". Load WAV again to use the new audio.");
        ref.fingerprint=ref.data->fingerprint;
        if(counted.insert(ref.data.get()).second)bytes+=ref.data->mono.size()*sizeof(float);
        if(bytes>maximumBankBytes)throw std::runtime_error("Active samples exceed 128 MiB");
    }
}
inline float number(const juce::var& v) {
    if((!v.isDouble()&&!v.isInt())||!std::isfinite(double(v)))throw std::invalid_argument("Expected a finite number");
    return float(double(v));
}
inline std::string identifier(const juce::var& v) {
    if(!v.isString()||v.toString().isEmpty()||v.toString().length()>128)throw std::invalid_argument("Invalid module/port identity");
    return v.toString().toStdString();
}
inline juce::var endpointJSON(const Endpoint& e) {
    auto* v=new juce::DynamicObject;v->setProperty("node",juce::String(e.node));v->setProperty("port",juce::String(e.port));return v;
}
inline Endpoint readEndpoint(const juce::var& v) {
    if(!v.isObject())throw std::invalid_argument("Invalid endpoint");
    return {identifier(v["node"]),identifier(v["port"])};
}
inline juce::var rackJSON(const RackDocument& document) {
    auto* root=new juce::DynamicObject;root->setProperty("format","optisynth.rack");root->setProperty("version",6);root->setProperty("gain",document.gain);root->setProperty("tempo",document.tempo);
    auto* assets=new juce::DynamicObject;
    for(const auto& s:document.samples){auto* ref=new juce::DynamicObject;ref->setProperty("path",juce::String(s.second.path));ref->setProperty("fingerprint",juce::String(s.second.fingerprint));assets->setProperty(juce::Identifier(s.first),juce::var(ref));}
    root->setProperty("samples",juce::var(assets));
    auto* engines=new juce::DynamicObject;
    for(const auto& s:document.spectral)if(s.second)engines->setProperty(juce::Identifier(s.first),juce::String(partials::patchText(s.second->patch)));
    root->setProperty("spectral",juce::var(engines));
    juce::Array<juce::var> modulation;
    for(const auto& m:document.graph.modulations){auto* row=new juce::DynamicObject;row->setProperty("from",endpointJSON(m.source));row->setProperty("to",endpointJSON(m.destination));row->setProperty("amount",m.amount);modulation.add(row);}
    root->setProperty("modulations",modulation);
    juce::Array<juce::var> nodes,links,outputs;
    for(const auto& n:document.graph.nodes) {
        auto* node=new juce::DynamicObject;node->setProperty("id",juce::String(n.id));node->setProperty("type",juce::String(n.type));
        auto* params=new juce::DynamicObject;for(const auto& p:n.parameters)params->setProperty(juce::Identifier(juce::String(p.first)),p.second);
        node->setProperty("parameters",juce::var(params));
        if(n.type=="seq.performance")node->setProperty("performance",performanceJSON(n.performance?*n.performance:defaultPerformance()));
        nodes.add(node);
    }
    for(const auto& c:document.graph.connections){auto* link=new juce::DynamicObject;link->setProperty("from",endpointJSON(c.source));link->setProperty("to",endpointJSON(c.destination));links.add(link);}
    for(const auto& e:document.graph.outputs)outputs.add(endpointJSON(e));
    root->setProperty("nodes",nodes);root->setProperty("connections",links);root->setProperty("outputs",outputs);
    auto* info=new juce::DynamicObject;info->setProperty("name",juce::String(document.info.name));info->setProperty("category",juce::String(document.info.category));
    info->setProperty("description",juce::String(document.info.description));
    juce::Array<juce::var> tags;for(const auto& t:document.info.tags)tags.add(juce::String(t));info->setProperty("tags",tags);
    root->setProperty("info",juce::var(info));root->setProperty("view",juce::String(document.view));
    auto* layout=new juce::DynamicObject;layout->setProperty("style",juce::String(document.panel.style));juce::Array<juce::var> sections;
    for(const auto& section:document.panel.sections){auto* row=new juce::DynamicObject;row->setProperty("title",juce::String(section.title));juce::Array<juce::var> controls;
        for(const auto& c:section.controls){auto* control=new juce::DynamicObject;control->setProperty("node",juce::String(c.node));control->setProperty("parameter",juce::String(c.parameter));control->setProperty("label",juce::String(c.label));controls.add(control);}
        row->setProperty("controls",controls);sections.add(row);}
    layout->setProperty("sections",sections);root->setProperty("panel",juce::var(layout));
    auto* rack=new juce::DynamicObject;juce::Array<juce::var> rows;
    for(const auto& row:document.rows){juce::Array<juce::var> ids;for(const auto& id:row)ids.add(juce::String(id));rows.add(ids);}
    rack->setProperty("rows",rows);root->setProperty("rack",juce::var(rack));return root;
}
inline std::string boundedText(const juce::var& v,int maximum,const char* what) {
    if(v.isVoid())return {};
    if(!v.isString()||v.toString().length()>maximum)throw std::invalid_argument(std::string("Invalid ")+what);
    return v.toString().toStdString();
}
// Version 6: browser metadata, preferred view, synth-panel layout. All optional.
inline void readPresetFields(const juce::var& value,RackDocument& d) {
    const auto& info=value["info"];
    if(!info.isVoid()){
        if(!info.isObject())throw std::invalid_argument("Invalid preset info");
        d.info.name=boundedText(info["name"],80,"preset name");d.info.category=boundedText(info["category"],32,"preset category");
        d.info.description=boundedText(info["description"],2000,"preset description");
        if(!info["tags"].isVoid()){auto* tags=info["tags"].getArray();if(!tags||tags->size()>16)throw std::invalid_argument("Invalid preset tags");
            for(const auto& t:*tags){const auto tag=normaliseTag(boundedText(t,32,"preset tag"));if(tag.empty())throw std::invalid_argument("Empty preset tag");
                if(std::find(d.info.tags.begin(),d.info.tags.end(),tag)==d.info.tags.end())d.info.tags.push_back(tag);}}
    }
    d.view=boundedText(value["view"],16,"view");
    if(!d.view.empty()&&d.view!="panel"&&d.view!="modular")throw std::invalid_argument("Unknown view");
    const auto& layout=value["panel"];
    if(!layout.isVoid()){
        if(!layout.isObject())throw std::invalid_argument("Invalid panel layout");
        d.panel.style=boundedText(layout["style"],16,"panel style");
        if(!d.panel.style.empty()&&std::find(panelStyles().begin(),panelStyles().end(),d.panel.style)==panelStyles().end())throw std::invalid_argument("Unknown panel style");
        auto* sections=layout["sections"].getArray();if(!sections||sections->size()>24)throw std::invalid_argument("Invalid panel sections");
        for(const auto& v:*sections){
            auto* controls=v["controls"].getArray();if(!v.isObject()||!controls||controls->size()>48)throw std::invalid_argument("Invalid panel section");
            PanelSection section{boundedText(v["title"],40,"panel title"),{}};
            for(const auto& c:*controls){if(!c.isObject())throw std::invalid_argument("Invalid panel control");
                section.controls.push_back({identifier(c["node"]),boundedText(c["parameter"],128,"panel parameter"),boundedText(c["label"],40,"panel label")});}
            d.panel.sections.push_back(std::move(section));
        }
    }
    const auto& rack=value["rack"];
    if(!rack.isVoid()){
        auto* rows=rack["rows"].getArray();if(!rack.isObject()||!rows||rows->size()>kMaximumNodes)throw std::invalid_argument("Invalid rack rows");
        for(const auto& row:*rows){auto* ids=row.getArray();if(!ids||ids->size()>kMaximumNodes)throw std::invalid_argument("Invalid rack row");
            std::vector<std::string> list;for(const auto& id:*ids)list.push_back(identifier(id));d.rows.push_back(std::move(list));}
    }
}
inline RackDocument readRack(const juce::var& value) {
    if(!value.isObject()||!value["version"].isInt()||int(value["version"])<1||int(value["version"])>6)throw std::invalid_argument("Unsupported patch version");
    RackDocument d;d.gain=number(value["gain"]);
    if(int(value["version"])>=4){d.tempo=number(value["tempo"]);if(d.tempo<20||d.tempo>300)throw std::invalid_argument("Tempo outside range");}
    if(d.gain<0||d.gain>1)throw std::invalid_argument("Output gain outside range");
    if(value["format"].toString()=="optisynth.instrument") {
        if(int(value["version"])!=1)throw std::invalid_argument("Unsupported legacy patch version");
        auto source=identifier(value["source"]),env=identifier(value["envelope"]),filter=identifier(value["filter"]);
        if(panel(source).family!="source"||panel(env).family!="envelope"||panel(filter).family!="filter")throw std::invalid_argument("Invalid legacy module family");
        if(!value["amplitudeEnvelope"].isBool()||!value["filterEnvelope"].isBool())throw std::invalid_argument("Invalid legacy routing");
        d.graph=instrumentPatch(source,env,filter,bool(value["amplitudeEnvelope"]),bool(value["filterEnvelope"])&&filter!="filter.bypass");
        auto* parameters=value["parameters"].getDynamicObject();if(!parameters)throw std::invalid_argument("Missing legacy parameters");
        for(const auto& entry:parameters->getProperties()) {
            const auto type=entry.name.toString().toStdString();const auto& specs=panel(type).parameters;
            auto* vals=entry.value.getDynamicObject();if(!vals)throw std::invalid_argument("Invalid legacy parameters");
            for(const auto& p:vals->getProperties()) {
                const auto id=p.name.toString().toStdString();float f=number(p.value);
                auto spec=std::find_if(specs.begin(),specs.end(),[&](const Parameter& s){return s.id==id;});
                if(spec==specs.end()||f<spec->minimum||f>spec->maximum)throw std::invalid_argument("Invalid legacy parameter value");
                for(auto& node:d.graph.nodes)if(node.type==type)node.parameters.push_back({id,f});
            }
        }
    } else if(value["format"].toString()=="optisynth.rack") {
        auto* nodes=value["nodes"].getArray();auto* links=value["connections"].getArray();auto* outputs=value["outputs"].getArray();
        if(!nodes||!links||!outputs||nodes->size()>kMaximumNodes||links->size()>kMaximumConnections||outputs->size()>kMaximumPorts)throw std::invalid_argument("Invalid/oversized rack");
        for(const auto& v:*nodes){
            NodeSpec n{identifier(v["id"]),identifier(v["type"]),{}};
            auto* params=v["parameters"].getDynamicObject();if(!params)throw std::invalid_argument("Missing node parameters");
            for(const auto& p:params->getProperties())n.parameters.push_back({p.name.toString().toStdString(),number(p.value)});
            if(n.type=="seq.performance")n.performance=readPerformance(v["performance"]);
            else if(v.hasProperty("performance"))throw std::invalid_argument("Performance data on incompatible module");
            d.graph.nodes.push_back(std::move(n));
        }
        for(const auto& v:*links)d.graph.connections.push_back({readEndpoint(v["from"]),readEndpoint(v["to"])});
        for(const auto& v:*outputs)d.graph.outputs.push_back(readEndpoint(v));
        if(int(value["version"])>=2){
            auto* assets=value["samples"].getDynamicObject();if(!assets||assets->getProperties().size()>kMaximumNodes)throw std::invalid_argument("Invalid sample references");
            for(const auto& s:assets->getProperties()){
                const auto id=s.name.toString().toStdString();const auto& v=s.value;
                if(!v.isObject()||!v["path"].isString()||v["path"].toString().isEmpty()||v["path"].toString().length()>4096||!v["fingerprint"].isString())throw std::invalid_argument("Invalid sample reference");
                const auto path=v["path"].toString(),hash=v["fingerprint"].toString();
                if(path!="builtin:click"&&!juce::File::isAbsolutePath(path))throw std::invalid_argument("Sample paths must be absolute");
                if(!hash.isEmpty()&&hash!="builtin-click-v1"&&(hash.length()!=16||!hash.containsOnly("0123456789abcdef")))throw std::invalid_argument("Invalid sample fingerprint");
                if(std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id&&n.type=="sample.player";}))throw std::invalid_argument("Sample reference has no sample module");
                d.samples[id]={path.toStdString(),hash.toStdString(),{}};
            }
        }
        if(int(value["version"])>=3){
            auto* engines=value["spectral"].getDynamicObject();auto* modulation=value["modulations"].getArray();
            if(!engines||engines->getProperties().size()>kMaximumNodes||!modulation||modulation->size()>kMaximumModulations)throw std::invalid_argument("Invalid engine or modulation state");
            size_t bytes=0;
            for(const auto& entry:engines->getProperties()){
                auto id=entry.name.toString().toStdString();
                if(!entry.value.isString()||std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id&&n.type=="spectral.engine";}))throw std::invalid_argument("Orphan or malformed Spectral state");
                auto text=entry.value.toString().toStdString();bytes+=text.size();if(bytes>1024*1024)throw std::invalid_argument("Spectral state exceeds 1 MiB");
                partials::Patch patch;std::string error;if(!partials::parsePatch(text,patch,error))throw std::invalid_argument(error);
                d.spectral[id]=spectralPreset(std::move(patch));
            }
            for(const auto& m:*modulation)d.graph.modulations.push_back({readEndpoint(m["from"]),readEndpoint(m["to"]),number(m["amount"])});
            for(const auto& n:d.graph.nodes)if(n.type=="spectral.engine"&&!d.spectral.count(n.id))throw std::invalid_argument("Missing Spectral engine state");
        }
        if(int(value["version"])>=6)readPresetFields(value,d);
    } else throw std::invalid_argument("Not an Optisynth patch");
    validateRack(d.graph);
    {std::set<std::string> seen;for(const auto& row:d.rows)for(const auto& id:row)
        if(!seen.insert(id).second||std::none_of(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id;}))throw std::invalid_argument("Rack row lists a missing or repeated module: "+id);}
    for(const auto& section:d.panel.sections)for(const auto& c:section.controls)
        if(!panelControlValid(d,c))throw std::invalid_argument("Panel control refers to a missing module or parameter: "+c.node+"."+c.parameter);
    return d;
}
}
