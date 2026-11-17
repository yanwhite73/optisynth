// Stress test for the partials plugin processor, run before it goes anywhere near a host.
//
//   PartialsStress PRESET_DIR [PRESETS_TO_LOAD] [PHASES]   (PHASES: any of A, B, C; default ABC)
//
// Phase A  an "audio" thread calls processBlock continuously (random block sizes, dense random
//          MIDI) while the main thread loads presets one after another, as a user browsing would.
// Phase B  every sample rate and block size, including 1-sample and 8192-sample blocks.
// Phase C  lifecycle and bad input: destroy while loading, garbage state, missing and damaged
//          preset files.
// Every output sample must be finite and within the safety ceiling. Block times are compared
// with the real-time budget (block length / sample rate). Exit code 0 only if all checks pass.
#include <JuceHeader.h>

#include <atomic>
#include <chrono>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <map>

#include "../Source/PluginProcessor.h"

namespace {

int failures = 0;

void fail(const juce::String& what) {
    ++failures;
    std::printf("FAIL: %s\n", what.toRawUTF8());
}

struct BlockStats {
    double worstRatio = 0.0;  // processing time / real-time budget
    long blocks = 0, badSamples = 0;
    std::vector<double> ratios;
};

void addRandomMidi(juce::MidiBuffer& midi, int n, std::mt19937& rng) {
    std::uniform_int_distribution<int> pos(0, std::max(0, n - 1)), note(0, 127), vel(0, 127), kind(0, 99);
    const int events = std::uniform_int_distribution<int>(0, 6)(rng);
    for (int i = 0; i < events; ++i) {
        const int k = kind(rng), t = pos(rng);
        if (k < 45) midi.addEvent(juce::MidiMessage::noteOn(1, note(rng), (juce::uint8) vel(rng)), t);
        else if (k < 90) midi.addEvent(juce::MidiMessage::noteOff(1, note(rng)), t);
        else if (k < 92) midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, k % 2 ? 127 : 0), t);
        else if (k < 93) midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, vel(rng)), t);
        else if (k < 94) midi.addEvent(juce::MidiMessage::channelPressureChange(1, vel(rng)), t);
        else if (k < 95) midi.addEvent(juce::MidiMessage::pitchWheel(1, vel(rng) * 128), t);
        else if (k < 98) midi.addEvent(juce::MidiMessage::allNotesOff(1), t);
        else midi.addEvent(juce::MidiMessage::allSoundOff(1), t);
    }
}

void runBlock(PartialsProcessor& p, juce::AudioBuffer<float>& buf, int n, double sr, std::mt19937& rng, BlockStats& st) {
    juce::MidiBuffer midi;
    addRandomMidi(midi, n, rng);
    buf.setSize(2, n, false, false, true);
    const auto t0 = std::chrono::steady_clock::now();
    p.processBlock(buf, midi);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double ratio = secs / (n / sr);
    st.worstRatio = std::max(st.worstRatio, ratio);
    st.ratios.push_back(ratio);
    ++st.blocks;
    for (int c = 0; c < 2; ++c) {
        const float* d = buf.getReadPointer(c);
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(d[i]) || std::abs(d[i]) > 2.0f) ++st.badSamples;
        }
    }
}

double percentile(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, static_cast<size_t>(q * static_cast<double>(v.size())))];
}

bool waitForLoad(PartialsProcessor& p, int timeoutMs) {
    for (int waited = 0; waited < timeoutMs; waited += 10) {
        if (!p.isLoading()) return true;
        juce::Thread::sleep(10);
    }
    return false;
}

void phaseA(const juce::Array<juce::File>& presets, int count) {
    std::printf("Phase A: concurrent playback while loading %d presets\n", count);
    PartialsProcessor p;
    const double sr = 48000.0;
    p.setRateAndBufferSizeDetails(sr, 512);
    p.prepareToPlay(sr, 512);
    std::atomic<bool> stop{false};
    BlockStats st;
    std::thread audio([&] {
        std::mt19937 rng(1);
        juce::AudioBuffer<float> buf(2, 4096);
        std::uniform_int_distribution<int> size(1, 1024);
        while (!stop.load()) runBlock(p, buf, size(rng), sr, rng, st);
    });
    std::mt19937 pick(7);
    for (int i = 0; i < count; ++i) {
        // UI keyboard producer runs concurrently with the audio consumer and preset swaps.
        p.keyboardState.noteOn(1, 48 + i % 12, .5f);
        p.keyboardState.noteOff(1, 48 + i % 12, 0);
        p.refreshKeyboardDisplay();
        const juce::File& f = presets[(int) (pick() % (unsigned) presets.size())];
        p.loadPresetFile(f);
        if (i % 5 == 4) p.loadPresetFile(presets[(int) (pick() % (unsigned) presets.size())]);  // supersede mid-load
        if (!waitForLoad(p, 20000)) fail("load did not finish: " + f.getFileName());
        if (p.getStatus().startsWith("Could not")) fail("load failed: " + p.getStatus());
        if (i % 10 == 0) {  // state round trip while playing
            juce::MemoryBlock mb;
            p.getStateInformation(mb);
            p.setStateInformation(mb.getData(), (int) mb.getSize());
            waitForLoad(p, 20000);
        }
    }
    stop = true;
    audio.join();
    std::printf("  %ld blocks, bad samples %ld, real-time load: 99.9th pct %.3f, worst %.3f\n", st.blocks,
                st.badSamples, percentile(st.ratios, 0.999), st.worstRatio);
    if (st.badSamples) fail("non-finite or out-of-range samples in phase A");
}

void phaseB(const juce::File& preset) {
    std::printf("Phase B: sample rates and block sizes with %s\n", preset.getFileName().toRawUTF8());
    for (double sr : {22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
        PartialsProcessor p;
        p.setRateAndBufferSizeDetails(sr, 8192);
        p.prepareToPlay(sr, 8192);
        p.loadPresetFile(preset);
        if (!waitForLoad(p, 20000)) fail("load timed out");
        BlockStats st;
        std::mt19937 rng(3);
        juce::AudioBuffer<float> buf(2, 8192);
        for (int n : {1, 2, 15, 16, 17, 64, 128, 333, 512, 1024, 4096, 8192}) {
            for (int k = 0; k < 40; ++k) runBlock(p, buf, n, sr, rng, st);
        }
        // a 64-note chord, then sustain games
        juce::MidiBuffer chord;
        for (int nn = 30; nn < 94; ++nn) chord.addEvent(juce::MidiMessage::noteOn(1, nn, (juce::uint8) 127), 0);
        buf.setSize(2, 512, false, false, true);
        p.processBlock(buf, chord);
        for (int k = 0; k < 200; ++k) runBlock(p, buf, 256, sr, rng, st);
        std::printf("  %6.0f Hz: %ld blocks, bad samples %ld, worst real-time load %.3f\n", sr, st.blocks, st.badSamples,
                    st.worstRatio);
        if (st.badSamples) fail("bad samples at " + juce::String(sr));
    }
}

void midiAndTransportChecks() {
    PartialsProcessor p;
    p.prepareToPlay(48000, 512);
    struct PlayHead final : juce::AudioPlayHead {
        juce::Optional<PositionInfo> info;
        mutable int calls = 0;
        juce::Optional<PositionInfo> getPosition() const override { ++calls; return info; }
    } playHead;
    p.setPlayHead(&playHead);
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    auto render = [&] {
        p.processBlock(buf, midi);
        midi.clear();
        return buf.getMagnitude(0, 512);
    };
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, .8f), 0);
    if (render() < .001f) fail("MIDI note-on produces no sound");
    p.refreshKeyboardDisplay();
    if (!p.keyboardState.isNoteOn(2, 48)) fail("host note is not mirrored on the keyboard");
    midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
    if (render() != 0) fail("All Sound Off was swallowed by generic CC handling");
    p.refreshKeyboardDisplay();
    if (p.keyboardState.isNoteOn(2, 48)) fail("All Sound Off left a keyboard highlight");
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, .8f), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 1);
    render();
    midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    for (int i = 0; i < 128; ++i) render();
    if (render() > 1e-8f) fail("All Notes Off left a sustained voice");
    p.keyboardState.noteOn(1, 55, .8f);
    if (render() < .001f) fail("UI keyboard queue lost note-on");
    p.keyboardState.noteOff(1, 55, 0);
    for (int i = 0; i < 128; ++i) render();
    if (render() > 1e-8f) fail("UI keyboard queue lost note-off");
    for (int i = 0; i < 600; ++i) {
        p.keyboardState.noteOn(1, 60, .8f);
        p.keyboardState.noteOff(1, 60, 0);
    }
    if (render() != 0) fail("keyboard overflow did not clear potential stuck notes");
    std::vector<char> sysex(4096, 1);  // allocated by the test host, outside processBlock
    midi.addEvent(juce::MidiMessage::createSysExMessage(sysex.data(), (int) sysex.size()), 0);
    if (render() != 0) fail("unsupported SysEx affected the engine");
    juce::AudioPlayHead::PositionInfo info;
    for (double ppq : {-4.25, 0.0, 19.75, 2.0, std::numeric_limits<double>::quiet_NaN()}) {
        info.setBpm(135.0);
        info.setPpqPosition(ppq);
        info.setIsPlaying(true);
        playHead.info = info;
        if (!std::isfinite(render())) fail("transport discontinuity made output non-finite");
    }
    info.setIsPlaying(false); playHead.info = info; render();
    playHead.info = juce::nullopt; render();
    if (playHead.calls == 0) fail("processor did not read the playhead");
    p.setPlayHead(nullptr);
    std::printf("  MIDI panic, UI queue, overflow, display, SysEx and optional transport checks complete\n");
}

void phaseC(const juce::Array<juce::File>& presets) {
    std::printf("Phase C: lifecycle and bad input\n");
    midiAndTransportChecks();
    for (int i = 0; i < 20; ++i) {  // destroy while a load is running
        auto p = std::make_unique<PartialsProcessor>();
        p->prepareToPlay(44100.0, 256);
        p->loadPresetFile(presets[i % presets.size()]);
        if (i % 2) juce::Thread::sleep(i * 3);
        p.reset();
    }
    PartialsProcessor p;
    p.prepareToPlay(48000.0, 256);
    std::mt19937 rng(9);
    std::vector<char> junk(5000);
    for (int i = 0; i < 50; ++i) {
        for (auto& c : junk) c = (char) rng();
        p.setStateInformation(junk.data(), (int) (rng() % junk.size()));
    }
    p.setStateInformation(nullptr, 0);
    const juce::File tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("partials_stress");
    tmp.createDirectory();
    const juce::File missing = tmp.getChildFile("missing.ptch");
    missing.deleteFile();
    p.loadPresetFile(missing);
    waitForLoad(p, 5000);
    if (!p.getStatus().startsWith("Could not")) fail("missing file not reported");
    const juce::File damaged = tmp.getChildFile("damaged.ptch");
    damaged.replaceWithText("partials-patch 1\nosc.1.on 1\nosc.1.wave_a.db 1 2 3\nosc.1.filter.cutoff_hz nan\n");
    p.loadPresetFile(damaged);
    waitForLoad(p, 5000);
    if (!p.getStatus().startsWith("Could not")) fail("damaged file not reported");
    const juce::File wrong = tmp.getChildFile("wrong.ptch");
    wrong.replaceWithText("this is not a patch");
    p.loadPresetFile(wrong);
    waitForLoad(p, 5000);
    if (!p.getStatus().startsWith("Could not")) fail("non-patch file not reported");
    BlockStats st;
    juce::AudioBuffer<float> buf(2, 512);
    for (int k = 0; k < 200; ++k) runBlock(p, buf, 512, 48000.0, rng, st);
    if (st.badSamples) fail("bad samples after bad input");
    std::printf("  done; status after bad input: %s\n", p.getStatus().toRawUTF8());
}

juce::Component* findControl(juce::Component& root,const juce::String& id) {
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* found=findControl(*child,id))return found;
    return nullptr;
}
void pumpUi(int milliseconds=180) {
    for(int elapsed=0;elapsed<milliseconds;elapsed+=20){
        juce::Thread::sleep(20);juce::Timer::callPendingTimersSynchronously();
    }
}
// Host normalization and decimal patch serialization can each round a float once.
// Compare numeric tokens within float precision while retaining exact field/text structure.
bool samePatch(const partials::Patch& a,const partials::Patch& b) {
    std::istringstream left(partials::patchText(a)),right(partials::patchText(b));
    std::string x,y;
    while(left>>x){
        if(!(right>>y))return false;
        if(x==y)continue;
        char *xe=nullptr,*ye=nullptr;
        const double xv=std::strtod(x.c_str(),&xe),yv=std::strtod(y.c_str(),&ye);
        if(xe==x.c_str()||ye==y.c_str()||*xe||*ye||!std::isfinite(xv)||!std::isfinite(yv)||
           std::abs(xv-yv)>1e-6*std::max({1.0,std::abs(xv),std::abs(yv)})){
            std::printf("  patch mismatch: %s / %s\n",x.c_str(),y.c_str());return false;
        }
    }
    return !(right>>y);
}
void phaseD(const juce::Array<juce::File>& presets) {
    std::printf("Phase D: live edits, complete state, editor controls and screenshots\n");
    PartialsProcessor p;p.prepareToPlay(48000,256);p.loadPresetFile(presets[0]);
    if(!waitForLoad(p,15000)){fail("editor preset load timeout");return;}
    auto patch=p.editablePatch();patch.name="Editor validation";
    patch.osc[0].on=true;patch.osc[0].filter.on=true;
    patch.fx[0].type="reverb";patch.fx[0].on=true;patch.fx[0].params={{"wet",.2f}};
    p.applyEditedPatch(patch);waitForLoad(p,15000);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto slider=[&](const juce::String& id,double value){
        auto* c=dynamic_cast<juce::Slider*>(findControl(*editor,id));
        if(!c){fail("missing slider "+id);return;}c->setValue(value,juce::sendNotificationSync);
    };
    auto click=[&](const juce::String& id){
        auto* c=dynamic_cast<juce::Button*>(findControl(*editor,id));
        if(!c){fail("missing button "+id);return;}if(c->onClick)c->onClick();pumpUi();
    };
    const auto images=juce::File::getCurrentWorkingDirectory().getChildFile("build/ui_qualification");images.createDirectory();
    auto snapshot=[&](const char* name){
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        juce::FileOutputStream out(images.getChildFile(name));
        out.setPosition(0);out.truncate();
        if(!out.openedOk()||!juce::PNGImageFormat().writeImageToStream(image,out))fail("UI screenshot failed");
    };
    slider("filter.cutoff",3210);slider("osc.wavemix",23);slider("main.volume",-13);
    pumpUi();waitForLoad(p,15000);
    auto edited=p.editablePatch();
    if(std::abs(edited.osc[0].filter.cutoffHz-3210)>.01f||std::abs(edited.osc[0].waveMix-.23f)>.00001f||std::abs(edited.mainVolumeDb+13)>.0001f)fail("editor sliders did not reach processor patch");
    for(float normalized:{0.f,1.f}){
        p.params.getParameter("osc1_symmetry")->setValueNotifyingHost(normalized);
        auto* symmetry=dynamic_cast<juce::Slider*>(findControl(*editor,"osc.symmetry"));
        if(!symmetry||std::abs(symmetry->getValue()-(normalized==0?1.:99.))>.001)
            fail("editor clipped the host symmetry range");
    }
    snapshot("main.png");click("env.right4");slider("lfo.rate",3.25);pumpUi();snapshot("lfo.png");
    click("env.right0");
    slider("env.left.attack",.2);slider("env.right.attack",.3);slider("env.left.A curve",1.7);
    pumpUi();
    if(p.isLoading())fail("envelope slider queued a rebuild");
    if(std::abs(p.editablePatch().mainEnv.attack-.2f)>1.e-5f
        ||std::abs(p.editablePatch().cutEnv[0].attack-.3f)>1.e-5f
        ||std::abs(p.editablePatch().mainEnv.attackCurve-1.7f)>1.e-5f)fail("envelope UI host binding/scaling");
    click("wave.edit");click("spectral.saw");pumpUi();waitForLoad(p,15000);snapshot("spectrum.png");
    if(std::abs(p.editablePatch().osc[0].waveA.db[2]+6.0206f)>.01f)fail("spectral editor did not change harmonics");
    click("spectral.close");click("matrix.page");slider("matrix.amount23",.35);pumpUi();
    if(std::abs(p.editablePatch().matrix[23].amount-.35f)>.001f)fail("second matrix page did not edit row 24");
    click("arp.page");slider("arp.step31.transpose",7);pumpUi();
    if(p.editablePatch().arp.steps[31].transpose!=7)fail("second arp page did not edit step 32");
    auto* fx=dynamic_cast<juce::ComboBox*>(findControl(*editor,"fx.type"));
    if(fx){fx->setSelectedItemIndex(11,juce::sendNotificationSync);pumpUi();slider("fx.bits",7.25);pumpUi();snapshot("crusher.png");}
    else fail("missing effect type selector");
    if(std::abs(p.editablePatch().fx[0].params["bits"]-7.25f)>.001f)fail("effect control did not edit fractional bit depth");
    editor->setSize(960,700);snapshot("small.png");editor.reset();waitForLoad(p,15000);
    juce::MemoryBlock state;p.getStateInformation(state);
    PartialsProcessor restored;restored.prepareToPlay(48000,256);restored.setStateInformation(state.getData(),(int)state.getSize());waitForLoad(restored,15000);
    if(!samePatch(p.editablePatch(),restored.editablePatch())||!restored.isEdited())fail("complete edited patch did not survive host state restore");
    auto revised=restored.editablePatch();revised.mainVolumeDb=-19;restored.applyEditedPatch(revised);waitForLoad(restored,15000);
    restored.getStateInformation(state);p.setStateInformation(state.getData(),(int)state.getSize());waitForLoad(p,15000);
    if(std::abs(p.editablePatch().mainVolumeDb+19)>.0001f)fail("second host save/restore used stale embedded state");
    const auto saved=images.getChildFile("saved-edit.ptch");
    if(!p.saveEditedPatch(saved))fail("edited patch save failed");
    partials::Patch disk;std::string error;
    if(!partials::loadPatch(saved.getFullPathName().toStdString(),disk,error)||partials::patchText(disk)!=partials::patchText(p.editablePatch()))fail("saved patch lost edits");
    revised=p.editablePatch();revised.mainVolumeDb=-25;p.applyEditedPatch(revised);p.restoreLoadedPatch();waitForLoad(p,15000);
    if(std::abs(p.editablePatch().mainVolumeDb+19)>.0001f||p.isEdited())fail("restore did not return to saved baseline");
    // Audio runs concurrently with many scalar changes and a waveform change. No device is opened.
    std::atomic<bool> stop{false};BlockStats st;
    std::thread audio([&]{juce::AudioBuffer<float> buf(2,512);std::mt19937 rng(17);
        while(!stop){runBlock(p,buf,256,48000,rng,st);juce::Thread::sleep(1);}});
    for(int i=0;i<60;++i){auto q=p.editablePatch();q.osc[0].filter.cutoffHz=150.f+i*130;q.osc[0].filter.resonance=(i%10)/10.f;
        if(i==30)q.osc[0].waveA.db[9]=-10;p.applyEditedPatch(q);juce::Thread::sleep(8);}
    if(!waitForLoad(p,15000))fail("live edit coalescing did not finish");stop=true;audio.join();
    if(st.badSamples)fail("invalid samples during live edits");
    if(std::abs(p.editablePatch().osc[0].filter.cutoffHz-7820)>.01f)fail("rapid edits lost the newest value");
    std::printf("  Editor pages, waveform/FX edits, save/restore and %ld concurrent audio blocks passed; screenshots in %s\n",st.blocks,images.getFullPathName().toRawUTF8());
}

void phaseE(const juce::Array<juce::File>& presets) {
    std::printf("Phase E: closed-editor host automation, registry and state\n");
    PartialsProcessor p;p.prepareToPlay(48000,256);
    const auto& parameters=p.getParameters();
    if(parameters.size()!=partials::kLiveControls+2)fail("host parameter count changed");
    std::set<std::string> ids,names;
    juce::Array<juce::var> rows;
    for(auto* raw:parameters){
        auto* param=dynamic_cast<juce::RangedAudioParameter*>(raw);
        if(!param){fail("non-ranged host parameter");continue;}
        const auto id=param->paramID,name=param->getName(128);
        if(!ids.insert(id.toStdString()).second||!names.insert(name.toStdString()).second||name.length()>16)
            fail("duplicate ID/name or truncated host name");
        const auto range=param->getNormalisableRange();
        for(float n:{0.f,.1f,.5f,.9f,1.f})if(!param->isDiscrete()&&std::abs(param->convertTo0to1(param->convertFrom0to1(n))-n)>.002f)
            fail("normalization roundtrip: "+id);
        const partials::LiveControlSpec* spec=nullptr;
        for(const auto& item:partials::liveControlSpecs())if(id==juce::String(item.id)){spec=&item;break;}
        const bool excluded=!spec||!spec->lfoCandidate;
        const juce::String condition=spec?juce::String(spec->condition):juce::String();
        auto* row=new juce::DynamicObject;
        row->setProperty("id",id);row->setProperty("name",name);row->setProperty("unit",param->getLabel());
        row->setProperty("minimum",range.start);row->setProperty("maximum",range.end);
        row->setProperty("stepInterval",range.interval);
        row->setProperty("default",param->convertFrom0to1(param->getDefaultValue()));
        row->setProperty("normalization",spec&&spec->logarithmic?"logarithmic":spec&&(spec->scale==partials::ControlScale::ZeroSeconds||spec->scale==partials::ControlScale::ZeroMilliseconds)?"zero-logarithmic":"linear");
        row->setProperty("discrete",param->isDiscrete());row->setProperty("automatable",param->isAutomatable());
        row->setProperty("lfoCandidate",!excluded);row->setProperty("condition",condition);
        if(spec){
            row->setProperty("patchKey",juce::String(spec->patchKey));
            const int index=static_cast<int>(spec-partials::liveControlSpecs().data());
            row->setProperty("family",index<60?"legacy":index<partials::kLiveMatrixStart?"envelope":index<partials::kLiveFxStart?"matrix":"effect");
            if(index>=partials::kLiveFxStart){
                const int field=(index-partials::kLiveFxStart)%partials::kEffectControls;
                row->setProperty("effectType",partials::effectControlSpecs[field].type);
                row->setProperty("slot",(index-partials::kLiveFxStart)/partials::kEffectControls+1);
            }
        }
        row->setProperty("versionHint",param->getVersionHint());rows.add(juce::var(row));
    }
    if(dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[0])->paramID!="output"||
       dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[1])->paramID!="autolevel")fail("legacy IDs/order changed");
    auto patch=partials::defaultPatch();
    for(int h=1;h<=partials::kHarmonics;++h)patch.osc[0].waveA.db[h]=-20.f*std::log10((float)h);
    patch.osc[0].waveB=patch.osc[0].waveA;
    patch.osc[0].filter.on=true;patch.osc[0].filter.cutoffHz=100;patch.osc[0].filter.lpOffset=0;patch.osc[0].filter.lpType=4;patch.mainVolumeDb=-12;
    p.applyEditedPatch(patch);if(!waitForLoad(p,15000)){fail("host test patch timeout");return;}
    p.params.getParameter("autolevel")->setValueNotifyingHost(0);
    juce::AudioBuffer<float> buf(2,256);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,48,.7f),0);
    p.processBlock(buf,midi);midi.clear();
    auto energy=[&]{double sum=0;for(int k=0;k<100;++k){p.processBlock(buf,midi);if(k>50)
        for(int c=0;c<2;++c)for(int i=0;i<256;++i)sum+=std::pow(buf.getSample(c,i),2);}return sum;};
    const double low=energy();
    auto* cutoff=p.params.getParameter("osc1_filter_cutoff");
    cutoff->setValueNotifyingHost(cutoff->convertTo0to1(10000));
    const double high=energy();
    if(!(high>low*1.2&&low>0))fail("closed-editor cutoff did not change audible output");
    if(p.isLoading())fail("continuous host control queued a rebuild");
    for(int block=0;block<500;++block){
        for(int i=2;i<parameters.size();++i)parameters[i]->setValueNotifyingHost(.5f+.49f*std::sin(.02f*block+.31f*i));
        p.processBlock(buf,midi);
        if(p.isLoading()){fail("host modulation queued a rebuild");break;}
        for(int c=0;c<2;++c)for(int i=0;i<256;++i)if(!std::isfinite(buf.getSample(c,i))||std::abs(buf.getSample(c,i))>2)
            {fail("invalid modulated sample");return;}
    }
    juce::MemoryBlock state;p.getStateInformation(state);
    PartialsProcessor restored;restored.prepareToPlay(48000,256);restored.setStateInformation(state.getData(),(int)state.getSize());
    if(!waitForLoad(restored,15000)||!samePatch(p.editablePatch(),restored.editablePatch()))fail("closed-editor host state lost controls");
    const auto beforeRecall=restored.hostLiveValues();
    for(int recall=0;recall<10;++recall){restored.getStateInformation(state);restored.setStateInformation(state.getData(),(int)state.getSize());}
    waitForLoad(restored,15000);
    if(beforeRecall!=restored.hostLiveValues())fail("host parameter values drifted across repeated recall");
    midi.addEvent(juce::MidiMessage::allSoundOff(1),0);p.processBlock(buf,midi);midi.clear();
    if(energy()>1e-10)fail("host modulation left stuck sound after reset");
    p.loadPresetFile(presets[0]);waitForLoad(p,15000);partials::Patch disk;std::string error;
    partials::loadPatch(presets[0].getFullPathName().toStdString(),disk,error);
    const auto expected=partials::liveValues(disk),actual=p.hostLiveValues();
    for(int i=0;i<partials::kLiveControls;++i){const auto& s=partials::liveControlSpecs()[i];
        const float value=std::clamp(expected[i],s.minimum,s.maximum);
        if(std::abs(value-actual[i])>1e-5f*std::max(1.f,std::abs(value)))fail("preset did not update host parameter: "+s.id);}
    auto* root=new juce::DynamicObject;root->setProperty("schema",1);root->setProperty("plugin",p.getName());
    root->setProperty("processorParameterCount",parameters.size());root->setProperty("parameters",rows);
    root->setProperty("identityNote","IDs are JUCE stable string IDs, not Live parameter indices or DeviceParameter IDs. VST3 numeric IDs are wrapper hashes. Match unique host names in Live.");
    const auto registry=juce::File::getCurrentWorkingDirectory().getChildFile("docs/legacy/optispectral/host_parameters.json");
    if(!registry.replaceWithText(juce::JSON::toString(juce::var(root),true)))fail("could not write host registry");
    std::printf("  %d unique parameters; cutoff energy %.6f -> %.6f; 500 modulation blocks; registry %s\n",
        parameters.size(),low,high,registry.getFullPathName().toRawUTF8());
}

void phaseF() {
    std::printf("Phase F: old-state migration, inactive banks and legacy parameter contract\n");
    PartialsProcessor source;source.prepareToPlay(48000,256);
    auto patch=partials::defaultPatch();
    for(int e=0;e<12;++e){auto& env=partials::liveEnvelope(patch,e);env.attack=.013f*(e+1);env.sustain=.3f;env.release=.17f;env.attackCurve=1.7f;}
    patch.fx[0].on=true;patch.fx[0].type="filter";patch.fx[0].params={{"cutoff_hz",4321},{"resonance",.37f},{"wet",.6f}};
    patch.matrix[23]={"constant","osc1.pitch",.123f};
    source.applyEditedPatch(patch);if(!waitForLoad(source,15000)){fail("migration source timeout");return;}
    const auto expected=source.hostLiveValues();
    juce::MemoryBlock state;source.getStateInformation(state);
    auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),(int)state.getSize());
    xml->setAttribute("liveParameterSchema",1);
    for(int i=partials::kLegacyLiveControls;i<partials::kLiveControls;++i)
        for(auto* child=xml->getFirstChildElement();child;){
            auto* next=child->getNextElement();
            if(child->hasTagName("PARAM")&&child->getStringAttribute("id")==juce::String(partials::liveControlSpecs()[i].id))xml->removeChildElement(child,true);
            child=next;
        }
    juce::AudioProcessor::copyXmlToBinary(*xml,state);
    PartialsProcessor restored;restored.prepareToPlay(48000,256);
    for(int round=0;round<2;++round){
        for(const auto& spec:partials::liveControlSpecs())restored.params.getParameter(spec.id)->setValueNotifyingHost(.91f);
        restored.setStateInformation(state.getData(),(int)state.getSize());
        if(!waitForLoad(restored,15000))fail("migration recall timeout");
        const auto actual=restored.hostLiveValues();
        for(int i=0;i<partials::kLiveControls;++i){
            auto* parameter=restored.params.getParameter(partials::liveControlSpecs()[i].id);
            if(std::abs(parameter->convertTo0to1(actual[i])-parameter->convertTo0to1(expected[i]))>1e-6f)
                fail("old-state parameter migration: "+partials::liveControlSpecs()[i].id);
        }
        if(std::abs(restored.editablePatch().mainEnv.attack-patch.mainEnv.attack)>1e-6f)fail("embedded envelope replaced by Init");
    }
    auto* inactive=restored.params.getParameter("fx1_wah_sensitivity");
    inactive->setValueNotifyingHost(.82f);
    restored.getStateInformation(state);const float saved=inactive->getValue();
    inactive->setValueNotifyingHost(.1f);restored.setStateInformation(state.getData(),(int)state.getSize());waitForLoad(restored,15000);
    if(inactive->getValue()!=saved||restored.editablePatch().fx[0].type!="filter")fail("inactive type bank recall changed active FX");
    const auto fixture=juce::JSON::parse(juce::File::getCurrentWorkingDirectory().getChildFile("tests/fixtures/host_parameters_v1.json"));
    auto* legacy=fixture["parameters"].getArray();
    if(!legacy||legacy->size()!=62){fail("missing legacy compatibility fixture");return;}
    for(int i=0;i<legacy->size();++i){
        const auto& old=legacy->getReference(i);
        auto* param=dynamic_cast<juce::RangedAudioParameter*>(restored.getParameters()[i]);
        if(!param||param->paramID!=old["id"].toString()||param->getName(128)!=old["name"].toString()
            ||param->getVersionHint()!=(int)old["versionHint"]){fail("legacy ID/name/order/version changed");continue;}
        const auto range=param->getNormalisableRange();
        if(range.start!=(float)(double)old["minimum"]||range.end!=(float)(double)old["maximum"]||range.interval!=(float)(double)old["stepInterval"])
            fail("legacy range changed: "+param->paramID);
        for(float n:{0.f,.25f,.5f,.75f,1.f}){
            const bool log=old["normalization"].toString()=="logarithmic";
            const float physical=log?range.start*std::pow(range.end/range.start,n):range.start+n*(range.end-range.start);
            if(!param->isDiscrete()&&std::abs(param->convertTo0to1(physical)-n)>1e-6f)fail("legacy normalization changed: "+param->paramID);
        }
    }
    std::printf("  Legacy 62-control contract, schema-1 repeated load and typed FX bank recall passed\n");
}

#if JUCE_PLUGINHOST_VST3
void phaseV() {
    std::printf("Phase V: actual local VST3 bundle, parameter IDs, sound and recall\n");
    const auto bundle=juce::File::getCurrentWorkingDirectory().getChildFile("plugin/Builds/PartialsMacOSX/build/Release/Optispectral.vst3");
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile(types,bundle.getFullPathName());
    if(types.size()!=1){fail("local VST3 did not enumerate one instrument");return;}
    juce::String error;auto plugin=format.createInstanceFromDescription(*types[0],48000,256,error);
    if(!plugin){fail("local VST3 could not load: "+error);return;}
    plugin->prepareToPlay(48000,256);
    std::set<std::string> ids,names;juce::Array<juce::var> rows;
    juce::AudioProcessorParameter *main=nullptr,*autoLevel=nullptr;
    int automatable=0,midiProxies=0;
    for(auto* param:plugin->getParameters()){
        auto* hosted=dynamic_cast<juce::HostedAudioProcessorParameter*>(param);
        if(!hosted){fail("missing hosted parameter identity");continue;}
        if(!ids.insert(hosted->getParameterID().toStdString()).second)fail("duplicate VST3 numeric ID");
        const auto name=param->getName(128);
        if(name.startsWith("MIDI CC ")){++midiProxies;if(param->isAutomatable())fail("MIDI proxy exposed as automation target");}
        if(!param->isAutomatable())continue;
        ++automatable;
        if(!names.insert(name.toStdString()).second)fail("duplicate VST3 host name");
        if(name=="Main Volume")main=param;
        if(name=="Auto level")autoLevel=param;
        auto* row=new juce::DynamicObject;row->setProperty("name",name);row->setProperty("vst3Id",hosted->getParameterID());
        row->setProperty("discrete",param->isDiscrete());rows.add(juce::var(row));
    }
    const auto legacyRegistry=juce::JSON::parse(juce::File::getCurrentWorkingDirectory().getChildFile("tests/fixtures/vst3_parameters_v1.json"));
    if(auto* previous=legacyRegistry["parameters"].getArray()){
        for(const auto& old:*previous){
            bool found=false;
            for(const auto& row:rows)if(row["name"]==old["name"]&&row["vst3Id"]==old["vst3Id"]){found=true;break;}
            if(!found)fail("legacy numeric VST3 ID changed: "+old["name"].toString());
        }
    }else fail("missing old VST3 registry");
    PartialsProcessor reference;
    for(auto* param:reference.getParameters())if(names.count(param->getName(128).toStdString())!=1)fail("VST3 omitted processor parameter");
    if(automatable!=partials::kLiveControls+3||!main||!autoLevel){fail("unexpected VST3 automation list");plugin->releaseResources();return;}
    autoLevel->setValueNotifyingHost(0);
    juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,60,.7f),0);plugin->processBlock(buffer,midi);midi.clear();
    auto energy=[&]{double total=0;for(int k=0;k<100;++k){plugin->processBlock(buffer,midi);
        for(int c=0;c<2;++c)for(int i=0;i<256;++i){const float value=buffer.getSample(c,i);
            if(!std::isfinite(value)||std::abs(value)>2){fail("unsafe VST3 output");return -1.;}
            if(k>50)total+=value*value;}}return total;};
    const double normal=energy();main->setValueNotifyingHost((-60.f+200.f)/224.f);const double quiet=energy();
    if(!(normal>1e-4&&quiet<normal*.001))fail("actual VST3 host automation did not change sound");
    juce::MemoryBlock state;plugin->getStateInformation(state);const float saved=main->getValue();
    main->setValueNotifyingHost(.8f);plugin->processBlock(buffer,midi);
    plugin->setStateInformation(state.getData(),(int)state.getSize());
    if(std::abs(main->getValue()-saved)>1e-6f)fail("VST3 container state lost parameter value");
    plugin->releaseResources();plugin.reset();
    auto* report=new juce::DynamicObject;report->setProperty("bundle",bundle.getFileName());
    report->setProperty("name",types[0]->name);report->setProperty("manufacturer",types[0]->manufacturerName);
    report->setProperty("juceDescriptionUID",juce::String::toHexString(types[0]->uniqueId));
    report->setProperty("automatableCount",automatable);report->setProperty("midiProxyCount",midiProxies);
    report->setProperty("parameters",rows);
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("docs/legacy/optispectral/vst3_parameters.json");
    if(!file.replaceWithText(juce::JSON::toString(juce::var(report),true)))fail("VST3 registry write failed");
    std::printf("  %d automatable (processor controls + bypass), %d non-automatable MIDI proxies; energy %.6f -> %.6f; recall passed\n",automatable,midiProxies,normal,quiet);
}
#endif

#if JUCE_PLUGINHOST_VST3
#include "PresetExport.h"
#endif

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    juce::ScopedJuceInitialiser_GUI juceInit;
#if JUCE_PLUGINHOST_VST3
    if (argc == 2 && juce::String(argv[1]) == "--test-export-state-comparison")
        return presetExport::testComparison();
    if (argc >= 2 && juce::String(argv[1]) == "--export-vstpreset") {
        bool stateOnly = false, badOption = false;
        juce::StringArray paths;
        for (int i = 2; i < argc; ++i) {
            const juce::String argument(argv[i]);
            if (argument == "--state-only" && !stateOnly) stateOnly = true;
            else if (argument.startsWith("--")) badOption = true;
            else paths.add(argument);
        }
        if (badOption || (paths.size() != 2 && paths.size() != 3)) {
            std::fprintf(stderr, "usage: PartialsStress --export-vstpreset [--state-only] INPUT.ptch OUTPUT.vstpreset [BUNDLE.vst3]\n");
            return 2;
        }
        const auto cwd = juce::File::getCurrentWorkingDirectory();
        return presetExport::run(cwd.getChildFile(paths[0]), cwd.getChildFile(paths[1]),
            cwd.getChildFile(paths.size() == 3 ? paths[2] : "plugin/Builds/PartialsMacOSX/build/Release/Optispectral.vst3"),
            stateOnly);
    }
#endif
    if (argc < 2) {
        std::printf("usage: PartialsStress PRESET_DIR [PRESETS_TO_LOAD] [PHASES]\n");
        return 2;
    }
    const juce::File dir(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    const int count = argc > 2 ? std::atoi(argv[2]) : 60;
    auto presets = dir.findChildFiles(juce::File::findFiles, false, "*.ptch");
    if (presets.isEmpty()) {
        std::printf("no .ptch files in %s\n", dir.getFullPathName().toRawUTF8());
        return 2;
    }
    std::sort(presets.begin(), presets.end());
    juce::File filtered = presets[0];
    for (const auto& f : presets) {  // phase B wants a preset with filtered oscillators
        if (f.loadFileAsString().contains("osc.1.on 1") && f.loadFileAsString().contains("osc.1.filter.on 1")) {
            filtered = f;
            break;
        }
    }
    const std::string phases = argc > 3 ? argv[3] : "ABC";
    if (phases.find('A') != std::string::npos) phaseA(presets, count);
    if (phases.find('B') != std::string::npos) phaseB(filtered);
    if (phases.find('C') != std::string::npos) phaseC(presets);
    if (phases.find('D') != std::string::npos) phaseD(presets);
    if (phases.find('E') != std::string::npos) phaseE(presets);
    if (phases.find('F') != std::string::npos) phaseF();
#if JUCE_PLUGINHOST_VST3
    if (phases.find('V') != std::string::npos) phaseV();
#endif
    std::printf("%s (%d failures)\n", failures ? "STRESS TEST FAILED" : "STRESS TEST PASSED", failures);
    return failures ? 1 : 0;
}
