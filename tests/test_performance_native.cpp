#include "../app/PatchIO.h"
#include "../app/PerformanceEditor.h"
#include "../app/ModulationEditor.h"
#include <cstdio>
#include <fstream>

namespace {
using namespace optisynth;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int checkScrollNavigation(juce::Component& component){
    struct WheelSink:juce::Component {int received=0;void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&)override{++received;}} sink;
    sink.addAndMakeVisible(component);int sliders=0;
    std::function<void(juce::Component&)> visit=[&](juce::Component& c){
        if(auto* slider=dynamic_cast<juce::Slider*>(&c)){
            ++sliders;const auto before=slider->getValue();const auto now=juce::Time::getCurrentTime();
            const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),{10,10},{},0,0,0,0,0,slider,slider,now,{10,10},now,0,false);
            for(int direction=0;direction<4;++direction){juce::MouseWheelDetails wheel{};wheel.deltaX=direction<2?(direction?-.3f:.3f):0;wheel.deltaY=direction>=2?(direction==2?-.3f:.3f):0;wheel.isSmooth=true;
                const int forwarded=sink.received;slider->mouseWheelMove(event,wheel);
                require(slider->getValue()==before,"Scrolling changed a parameter");require(sink.received==forwarded+1,"Slider swallowed navigation scroll");}
        }
        for(auto* child:c.getChildren())visit(*child);
    };visit(component);sink.removeChildComponent(&component);return sliders;
}
std::vector<float> render(const RackDocument& document,int seconds){
    Runtime runtime(document.graph,sampleBank(document),document.spectral);runtime.clock.tempo(document.tempo);runtime.clock.play(true);
    std::vector<float> left(48000*seconds),right(left.size());runtime.render(left.data(),right.data(),int(left.size()),{});left.insert(left.end(),right.begin(),right.end());return left;
}
void presets(){
    for(int i=13;i<=14;++i){auto d=starterDocument(i,.25f);prepareSamples(d);const auto saved=juce::JSON::toString(rackJSON(d));auto restored=readRack(juce::JSON::parse(saved));prepareSamples(restored);
        require(*d.graph.nodes[0].performance==*restored.graph.nodes[0].performance,"Every pattern/chain field survives JSON recall");
        const auto a=render(d,12),b=render(restored,12);require(a==b,"Four-track stereo audio differs after recall");float peak=0;double energy=0;for(float v:a){require(std::isfinite(v),"Nonfinite performance audio");peak=std::max(peak,std::abs(v));energy+=v*v;}require(energy>1e-4,"Silent performance preset");
        for(int t=0;t<4;++t){auto solo=d;solo.graph.nodes[0].parameters.push_back({"track"+std::to_string(t+1)+"_solo",1});auto sound=render(solo,2);double e=0;for(float v:sound)e+=v*v;require(e>1e-5,"A track has no audible destination");}
        auto invalid=juce::JSON::parse(saved);auto* banks=invalid["nodes"][0]["performance"]["tracks"][3]["patterns"].getArray();(*banks)[7].getDynamicObject()->setProperty("last",90);bool rejected=false;try{readRack(invalid);}catch(const std::exception&){rejected=true;}require(rejected,"Malformed inactive bank accepted");
        const auto name=i==13?"performance-four-currents":"performance-clockwork-garden";
        std::ofstream preset(std::string("patches/")+name+".optisynth");preset<<saved.replace("\r\n","\n");require(bool(preset),"Write performance preset");
        d.graph.nodes[0].parameters.push_back({"chain",1});auto song=render(d,24);const size_t frames=song.size()/2;
        std::ofstream wav(std::string("build/")+name+".wav",std::ios::binary);auto u16=[&](uint16_t v){wav.put(char(v));wav.put(char(v>>8));};auto u32=[&](uint32_t v){u16(uint16_t(v));u16(uint16_t(v>>16));};
        wav.write("RIFF",4);u32(uint32_t(36+frames*4));wav.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);wav.write("data",4);u32(uint32_t(frames*4));
        for(size_t frame=0;frame<frames;++frame)for(size_t ch=0;ch<2;++ch)u16(uint16_t(int16_t(std::clamp(song[ch*frames+frame]*d.gain,-.95f,.95f)*32767)));
        require(bool(wav),"Write performance audio example");std::printf("PASS %s: all banks/chains recalled; identical 12-second stereo render; four audible solo tracks; peak %.5f; 24-second chain WAV\n",name,peak);
    }
}
void editorChecks(){
    std::puts("Initialise JUCE message handling without a desktop application/window");std::fflush(stdout);
    juce::ScopedJuceInitialiser_GUI initialise;
    std::puts("Construct native performance editor without a window peer");std::fflush(stdout);
    auto data=defaultPerformance();auto module=makePerformance({"sequence","seq.performance",{},std::make_shared<const PerformanceData>(data)});
    optiui::PerformanceEditor editor(data,module->controls,{});int publications=0;
    int wheelSliders=checkScrollNavigation(editor);
    {ModulePanel card("all");Values values;for(const auto& p:panels()){card.show(p.type,values,{});wheelSliders+=checkScrollNavigation(card);}}
    {ModulationEditor matrix;matrix.refresh(starterDocument(13,.25f));wheelSliders+=checkScrollNavigation(matrix);}
    std::printf("PASS scroll navigation: %d native sliders preserve values and forward four wheel directions\n",wheelSliders);
    editor.dataChanged=[&](const PerformanceData& changed){data=changed;module->controls->performance->publish(changed);++publications;};
    editor.selectPattern(0,1);editor.selectStep(3);editor.pitchControl().setValue(83,juce::sendNotificationSync);
    require(data.tracks[0].patterns[1].steps[3].pitch==83&&publications==1,"Editor gesture does not reach saved bank");require(module->controls->performance->playing[0]==0,"Selecting/editing B launched it");
    auto external=data;external.tracks[3].patterns[7].steps[63].pitch=98;editor.setData(external);editor.undo();require(data.tracks[0].patterns[1].steps[3].pitch!=83&&data.tracks[3].patterns[7].steps[63].pitch==98,"Undo overwrote an unrelated bank edit");editor.redo();require(data.tracks[0].patterns[1].steps[3].pitch==83,"Redo did not restore step");
    auto click=[&](const char* title){for(auto* child:editor.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()==title){require(button->isEnabled(),"Expected enabled action");button->onClick();return;}throw std::runtime_error("Missing editor action");};
    const auto copied=data.tracks[0].patterns[1];click("Copy pattern");editor.selectPattern(1,7);click("Paste pattern");require(data.tracks[1].patterns[7]==copied,"Pattern clipboard targets wrong bank");editor.undo();
    editor.selectPattern(0,1);auto protectedData=editor.data();protectedData.tracks[0].patterns[1].steps[0].protect=true;editor.setData(protectedData);const auto original=editor.data();
    click("Preview variation");require(editor.data()==original,"Variation preview changed saved data");click("Apply variation");require(data.tracks[0].patterns[1].steps[0]==original.tracks[0].patterns[1].steps[0],"Variation changed protected step");editor.undo();
    editor.showChain(true);editor.chainBarsControl(7).setValue(5,juce::sendNotificationSync);require(data.chain[7].bars==5,"Chain inspector failed to edit saved duration");editor.undo();require(data.chain[7].bars==2,"Chain undo failed");
    editor.setActive(true);editor.launchPattern(2,6);require((module->controls->performance->requests[2].load()&127)==7&&editor.editingTrack()==0&&editor.editingBank()==1,"Launch changed edit selection or targeted wrong track");
    // Paint real current/queued state after DSP consumes the UI's commands.
    SampleClock clock;clock.play(true);std::array<ClockFrame,24> time;std::array<NoteEventBlock,4> buses;std::array<NoteEventBlock*,16> events{};std::array<std::array<float,24>,16> output{};std::array<float*,16> outputs{};
    for(int i=0;i<16;++i)outputs[i]=output[i].data();for(int t=0;t<4;++t)events[t*4]=&buses[t];
    for(int block=0;block<500;++block){module->controls->performance->collect();for(auto& t:time)t=clock.next();for(auto& bus:buses)bus.begin(24);module->process({24,{},nullptr,outputs.data(),nullptr,nullptr,nullptr,time.data(),&buses[0],events.data()});}
    require(module->controls->performance->steps[0]>=0&&module->controls->performance->queued[2]==6,"UI launch not reflected in actual engine state");editor.setTempoDisplay(120);
    const juce::File folder(juce::File::getCurrentWorkingDirectory().getChildFile("build/performance-ui-review"));require(folder.createDirectory().wasOk(),"Create UI output folder");
    auto capture=[&](const char* name){auto image=editor.createComponentSnapshot(editor.getLocalBounds());juce::FileOutputStream stream(folder.getChildFile(name));require(stream.openedOk(),"Open editor image");stream.setPosition(0);stream.truncate();require(juce::PNGImageFormat().writeImageToStream(image,stream),"Write editor image");};
    editor.setSize(1260,930);editor.showChain(false);capture("performance-patterns.png");
    module->controls->values[PerfChain]=1;editor.setValue("chain",1);for(auto& t:time)t=clock.next();for(auto& bus:buses)bus.begin(24);module->process({24,{},nullptr,outputs.data(),nullptr,nullptr,nullptr,time.data(),&buses[0],events.data()});
    editor.showChain(true);capture("performance-chain.png");
    editor.setSize(1120,800);editor.showChain(false);capture("performance-minimum.png");editor.showChain(true);capture("performance-chain-minimum.png");
    require(editor.chainBarsControl(15).getParentComponent()->getBottom()<editor.getHeight()-30,"Last chain row is clipped at minimum size");
    require(folder.getChildFile("editor-check.txt").replaceWithText("PASS: native editor without desktop peer; inactive bank editing; scoped undo/redo; chain edits; separate launch/selection; four actual component snapshots. No audio device opened. Desktop window launch remains a separate check.\n"),"Write editor receipt");
    std::puts("PASS native component interaction and screenshots (no desktop peer)");
}
#include "reference_native_checks.h"
}
int main(int argc,char** argv){try{if(argc>1&&std::string(argv[1])=="--sound-change")soundChangeChecks();else if(argc>1&&std::string(argv[1])=="--ui")editorChecks();else if(argc>1&&std::string(argv[1])=="--references-ui")referenceEditors();else if(argc>1&&std::string(argv[1])=="--references"){referencePresets();referenceEditors();}else presets();return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
