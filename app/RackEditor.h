#pragma once
#include "Panel.h"
#include "PatchIO.h"
#include "SampleLoader.h"
#include "ModulationEditor.h"
#include <chrono>
#include <fstream>

namespace {
class RackSynth final : public AudioAppComponent, private Timer,
                        private MidiKeyboardState::Listener, private ListBoxModel {
public:
    RackSynth() : keyboard(keys,MidiKeyboardComponent::horizontalKeyboard), rack("Modules",this) {
        setWantsKeyboardFocus(true);setSize(1440,940);
        for(auto* c:std::initializer_list<juce::Component*>{&rack,&editor,&addChoice,&add,&remove,
            &starter,&save,&load,&audio,&silence,&gain,&keyboard,&output,&routes,&status,&keyboardHelp,&rackView,&focusMode,&matrixButton,&play,&restart,&tempo,&position})addAndMakeVisible(c);
        rackView.setViewedComponent(&hardware,false);rackView.setScrollBarsShown(false,true);rackView.setScrollBarThickness(12);
        focusMode.setClickingTogglesState(true);focusMode.onClick=[this]{showSelected();};
        rack.setRowHeight(62);rack.setColour(ListBox::backgroundColourId,Colour(0xff25343a));
        rack.setColour(ListBox::outlineColourId,Colours::transparentBlack);
        int i=0;for(const auto& p:panels()){addTypes.push_back(p.type);addChoice.addItem(p.name,++i);}
        addChoice.setSelectedId(1,dontSendNotification);
        const char* names[]={"VA oscillator + filter","Modal percussion","Drum into resonator","Two oscillators + shared filter","Sample into resonator","Sample attack + resonator","Spectral stereo + external LFO","Spectral audio + LFO into resonator","Pattern / melodic voice","Euclidean / modal percussion","Stage / pulsing voice"};
        for(i=0;i<11;++i)starter.addItem(names[i],i+1);
        starter.setTextWhenNothingSelected("Starter patches");
        starter.onChange=[this]{if(!refreshing&&starter.getSelectedId()>0)commit(starterDocument(starter.getSelectedId()-1,document.gain),starter.getSelectedId()>=9?"sequence":"source");};
        add.onClick=[this]{addModule(addTypes[size_t(addChoice.getSelectedId()-1)]);};
        remove.onClick=[this]{removeSelected();};
        editor.selectionChanged=[this]{
            if(refreshing||selected.empty())return;
            auto next=document;for(auto& n:next.graph.nodes)if(n.id==selected){n.type=editor.selected();n.parameters.clear();}
            commit(std::move(next),selected);
        };
        output.onChange=[this]{
            const int index=output.getSelectedId()-1;if(refreshing||index<0)return;
            auto next=document;next.graph.outputs={outputPorts[size_t(index)]};commit(std::move(next),selected);
        };
        save.onClick=[this]{chooseFile(true);};load.onClick=[this]{chooseFile(false);};
        matrixButton.onClick=[this]{showMatrix();};
        audio.onClick=[this]{toggleAudio();};silence.onClick=[this]{stopAudio();};
        play.onClick=[this]{player.playing.store(!player.playing.load());updateTransport();};
        restart.onClick=[this]{player.restart();};restart.setTooltip("Return all sequencers to beat zero; keep current play/stop state");
        tempo.setSliderStyle(Slider::LinearHorizontal);tempo.setRange(20,300,.1);tempo.setValue(120);
        tempo.setTextBoxStyle(Slider::TextBoxRight,false,82,24);tempo.setTextValueSuffix(" BPM");
        tempo.onValueChange=[this]{cancelLoading();document.tempo=float(tempo.getValue());player.tempo.store(document.tempo);};
        position.setFont(font(12));position.setColour(Label::textColourId,paper);
        gain.setSliderStyle(Slider::LinearHorizontal);gain.setRange(0,1,.001);gain.setValue(document.gain);
        gain.setTextBoxStyle(Slider::TextBoxRight,false,55,24);gain.setColour(Slider::trackColourId,teal);
        gain.onValueChange=[this]{cancelLoading();document.gain=float(gain.getValue());player.gain.store(document.gain);};
        keyboard.setAvailableRange(24,96);keyboard.setLowestVisibleKey(24);keyboard.setOctaveForMiddleC(4);keyboard.clearKeyMappings();
        keys.addListener(this);
        routes.setMultiLine(true);routes.setReadOnly(true);routes.setScrollbarsShown(true);
        routes.setColour(TextEditor::backgroundColourId,Colour(0xff243238));routes.setColour(TextEditor::textColourId,paper);
        routes.setColour(TextEditor::outlineColourId,Colours::transparentBlack);routes.setFont(font(12));
        status.setFont(font(13));status.setColour(Label::textColourId,paper);
        keyboardHelp.setFont(font(12));keyboardHelp.setColour(Label::textColourId,paper.withAlpha(.8f));
        commit({rackPreset(0),.25f},"source");startTimerHz(60);
    }
    ~RackSynth() override {stopTimer();shutdownAudio();keys.removeListener(this);rack.setModel(nullptr);rackView.setViewedComponent(nullptr,false);}
    void prepareToPlay(int,double rate) override {converter.prepare(rate);}
    void releaseResources() override {}
    void showSampleDemo(){starter.setSelectedId(5,sendNotificationSync);}
    void showSpectralDemo(){starter.setSelectedId(7,sendNotificationSync);}
    void showSequencerDemo(){starter.setSelectedId(9,sendNotificationSync);}
    void getNextAudioBlock(const AudioSourceChannelInfo& block) override {
        if(block.buffer->getNumChannels()<1)return;
        if(block.buffer->getNumChannels()>1)converter.render(player,block.buffer->getWritePointer(0,block.startSample),block.buffer->getWritePointer(1,block.startSample),block.numSamples);
        else converter.render(player,block.buffer->getWritePointer(0,block.startSample),block.numSamples);
        for(int c=2;c<block.buffer->getNumChannels();++c)block.buffer->clear(c,block.startSample,block.numSamples);
    }
    void paint(Graphics& g) override {
        g.fillAll(Colour(0xff222426));g.setColour(paper);g.setFont(font(29));g.drawText("OPTISYNTH",24,16,230,35,Justification::centredLeft);
        g.setFont(font(12));g.setColour(teal);g.drawText("MODULAR INSTRUMENT  /  0.0.7",25,53,300,20,Justification::centredLeft);
        const int right=getWidth()-324;
        g.setColour(paper);g.setFont(font(12));
        g.drawText("OUTPUT",530,125,75,24,Justification::centredLeft);
        if(focusMode.getToggleState()){
            g.drawText("INPUTS  /  "+String(selected),right,169,300,20,Justification::centredLeft);
            g.drawText("CONNECTIONS",right,337,300,20,Justification::centredLeft);
        }
        g.drawText("OUTPUT",24,getHeight()-158,75,26,Justification::centredLeft);
        const float peak=player.peak.load(std::memory_order_relaxed);
        g.setColour(Colour(0xff34464c));g.fillRoundedRectangle(340.f,float(getHeight()-150),160,8,4);
        g.setColour(peak>.9f?orange:teal);g.fillRoundedRectangle(340.f,float(getHeight()-150),160*std::min(1.f,peak),8,4);
        g.setColour(paper.withAlpha(.6f));g.setFont(font(11));
        g.drawText(String(activeCount)+" active / "+String(int(document.graph.nodes.size()))+" modules  |  stereo / independent note routes   |   Click sockets to patch; unpatched notes use keyboard.",24,bodyBottom()+2,getWidth()-48,20,Justification::centredLeft);
    }
    void resized() override {
        const int right=getWidth()-324,bottom=bodyBottom();const bool focused=focusMode.getToggleState();
        starter.setBounds(24,82,360,32);save.setBounds(400,82,85,32);load.setBounds(493,82,85,32);
        audio.setBounds(getWidth()-258,24,145,36);silence.setBounds(getWidth()-105,24,81,36);
        play.setBounds(315,22,86,34);restart.setBounds(409,22,75,34);tempo.setBounds(494,24,206,30);position.setBounds(710,24,std::max(80,getWidth()-978),30);
        status.setBounds(595,76,getWidth()-619,45);
        addChoice.setBounds(24,122,250,30);add.setBounds(286,122,105,30);remove.setBounds(403,122,105,30);
        output.setBounds(602,122,230,30);focusMode.setBounds(getWidth()-180,122,156,30);
        matrixButton.setBounds(getWidth()-278,122,90,30);
        rack.setBounds(24,169,220,bottom-169);editor.setBounds(260,169,right-276,bottom-169);
        rack.setVisible(focused);editor.setVisible(focused);routes.setVisible(focused);rackView.setVisible(!focused);
        int y=198;for(size_t i=0;i<inputs.size();++i){auto& row=*inputs[i];const bool visible=focused&&!selected.empty()&&i<type(editor.current).inputs.size();row.label.setVisible(visible);row.choice.setVisible(visible);row.label.setBounds(right,y,300,20);row.choice.setBounds(right,y+23,300,32);y+=65;}
        routes.setBounds(right,364,300,std::max(40,bottom-364));
        rackView.setBounds(24,164,getWidth()-48,bottom-164);hardware.arrange(rackView.getWidth(),rackView.getHeight()-14);
        gain.setBounds(98,getHeight()-158,220,27);keyboardHelp.setBounds(525,getHeight()-163,getWidth()-549,38);
        keyboard.setBounds(24,getHeight()-113,getWidth()-48,89);keyboard.setKeyWidth(float(getWidth()-48)/43.f);
    }
    // Native smoke checks and screenshots; no audio device is opened.
    void snapshots(const File& directory) {
        stopTimer();directory.createDirectory();
        auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        require(commit({rackPreset(3),.25f},"source"),"Load two-oscillator starter");
        const auto second=handles.at("second")->values[0].load();
        editor.knobs[0]->slider.setValue(2,sendNotificationSync);
        require(handles.at("source")->values[0].load()==2&&handles.at("second")->values[0].load()==second,"Independent instance controls");
        auto* visibleSource=hardware.find("source");
        if(!visibleSource||std::abs(visibleSource->knobs[0]->slider.getValue()-2)>1e-5)
            throw std::runtime_error("Rack/focus synchronization: rack="+std::to_string(visibleSource?visibleSource->knobs[0]->slider.getValue():-999)+", focus="+std::to_string(editor.knobs[0]->slider.getValue()));
        visibleSource->knobs[0]->slider.setValue(3,sendNotificationSync);
        require(handles.at("source")->values[0].load()==3&&handles.at("second")->values[0].load()==second,"Visible rack controls independent");
        auto serial=JSON::toString(rackJSON(document));auto recalled=readRack(JSON::parse(serial));
        auto render=[](const GraphSpec& graph){Runtime r(graph);std::vector<float> out(48000);r.render(out.data(),24000,{48,.8f,true,true});r.render(out.data()+24000,24000,{48,.8f,false,false});return out;};
        require(render(document.graph)==render(recalled.graph),"Rack recall audio differs");
        auto bad=document;setRackConnection(bad.graph,{"filter","in"},{"vca","out"});
        require(!commit(bad,selected)&&JSON::toString(rackJSON(document))==serial,"Invalid edit changed document");
        const auto originalCount=document.graph.nodes.size();addModule("osc.sine");
        require(document.graph.nodes.size()==originalCount+1&&!handles.at(selected),"Added module should be dormant");
        removeSelected();require(document.graph.nodes.size()==originalCount,"Remove dormant module");
        selectNode("filter");
        require(inputs.size()==2,"Filter routing inputs");
        inputs[1]->choice.setSelectedId(1,sendNotificationSync); // optional control disconnect
        require(std::none_of(document.graph.connections.begin(),document.graph.connections.end(),[](const Connection& c){return c.destination.node=="filter"&&c.destination.port=="mod";}),"Disconnect control");
        selectNode("filter");int envelopeId=0;
        for(size_t i=0;i<inputs[1]->ports.size();++i)if(inputs[1]->ports[i].node=="envelope")envelopeId=int(i)+1;
        require(envelopeId>0,"Compatible control output available");inputs[1]->choice.setSelectedId(envelopeId,sendNotificationSync);
        require(std::any_of(document.graph.connections.begin(),document.graph.connections.end(),[](const Connection& c){return c.destination.node=="filter"&&c.destination.port=="mod";}),"Reconnect control");
        applySocketChoice("filter",{"mod",SignalType::Control,false},false,1,{});
        require(!hardware.find("filter")->jacks[1]->connected,"Socket disconnect indicator");
        applySocketChoice("filter",{"mod",SignalType::Control,false},false,10,{{"envelope","out"}});
        require(hardware.find("filter")->jacks[1]->connected,"Socket reconnect indicator");
        require(hardware.find("filter")->jacks[1]->getTooltip().contains("envelope.out"),"Socket source tooltip");
        applySocketChoice("filter",{"out",SignalType::Audio,false},true,10,{{"filter","in"}});
        require(hardware.find("filter")->jacks[0]->getTooltip().contains("mixer.out"),"Socket cycle retains prior source");
        require(replaceModule("source","osc.sine"),"Replace module");
        require(hardware.find("source")->current=="osc.sine"&&hardware.find("source")->knobs.size()==2,"Contextual replacement panel");
        for(const auto& spec:panels()){
            ModulePanel test("all");Values values;test.instance="layout";test.show(spec.type,values,{});test.setSize(panelWidth(spec.type),421);
            for(size_t i=0;i<test.knobs.size();++i){if(!test.knobs[i]->isVisible())continue;const auto bounds=test.knobs[i]->getBounds();
                require(bounds.getY()>=147&&bounds.getBottom()<=384&&bounds.getX()>=12&&bounds.getRight()<=test.getWidth()-12,"Module control bounds");
                for(size_t j=i+1;j<test.knobs.size();++j)if(test.knobs[j]->isVisible())require(!bounds.intersects(test.knobs[j]->getBounds()),"Module controls overlap");
            }
        }
        // Exercise explicit old-format import using the actual 0.0.2 keys.
        const auto legacy=JSON::parse(R"({"format":"optisynth.instrument","version":1,"source":"modal.voice","envelope":"spectral.ahdsfr","filter":"spectral.lowpass","amplitudeEnvelope":true,"filterEnvelope":true,"gain":0.25,"parameters":{"modal.voice":{"decay":0.7},"spectral.lowpass":{"cutoff_hz":1800}}})");
        auto imported=readRack(legacy);auto expected=instrumentPatch("modal.voice","spectral.ahdsfr","spectral.lowpass",true,true);
        for(auto& n:expected.nodes){if(n.id=="source")n.parameters={{"decay",.7f}};if(n.id=="filter")n.parameters={{"cutoff_hz",1800}};}
        require(render(imported.graph)==render(expected),"Legacy patch sound changed");
        auto capture=[&](const String& name){auto image=createComponentSnapshot(getLocalBounds());FileOutputStream stream(directory.getChildFile(name));require(stream.openedOk(),"Snapshot file");stream.setPosition(0);stream.truncate();require(PNGImageFormat().writeImageToStream(image,stream),"PNG export");};
        commit({rackPreset(0),.25f},"source");capture("hardware-rack.png");
        selectNode("filter");focusMode.setToggleState(true,dontSendNotification);resized();capture("hardware-filter-focus.png");
        focusMode.setToggleState(false,dontSendNotification);commit({rackPreset(2),.25f},"resonator");capture("hardware-resonator.png");
        setSize(1120,800);commit({rackPreset(1),.25f},"envelope");capture("hardware-minimum.png");
        commit({rackPreset(0),.25f},"filter");replaceModule("filter","spectral.lowpass");capture("hardware-spectral-minimum.png");
        setSize(1440,940);commit({rackPreset(3),.25f},"second");capture("hardware-two-oscillators.png");
        auto drain=[&]{const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(loading&&std::chrono::steady_clock::now()<end){collectSamples();std::this_thread::sleep_for(std::chrono::milliseconds(1));}require(!loading,"Sample worker timed out");};
        auto sampleAudio=[](const RackDocument& d){Runtime r(d.graph,sampleBank(d));std::vector<float> x(48000);r.render(x.data(),24000,{48,.8f,true,true});r.render(x.data()+24000,24000,{48,.8f,false,false});return x;};
        require(commit(starterDocument(4,.25f),"source"),"Sample starter queued");drain();
        require(document.samples.at("source").data&&hardware.find("source")->sample,"Loaded waveform asset");capture("sample-resonator.png");
        require(commit(starterDocument(5,.25f),"source"),"Parallel starter queued");drain();capture("sample-parallel.png");
        const File fixture=directory.getChildFile("sample-native-fixture.wav");
        auto writeFixture=[&](float scale){std::ofstream stream(fixture.getFullPathName().toStdString(),std::ios::binary);require(bool(stream),"Native fixture write");
            auto u16=[&](uint16_t v){stream.put(char(v&255));stream.put(char(v>>8));};auto u32=[&](uint32_t v){u16(v&65535);u16(v>>16);};
            auto sample=demoSample();stream.write("RIFF",4);u32(36+uint32_t(sample->mono.size())*2);stream.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(48000);u32(96000);u16(2);u16(16);stream.write("data",4);u32(uint32_t(sample->mono.size())*2);for(float f:sample->mono)u16(uint16_t(int16_t(f*scale*32767)));
        };
        writeFixture(.7f);auto* samplePanel=hardware.find("source");
        require(samplePanel->isInterestedInFileDrag({fixture.getFullPathName()})&&!samplePanel->isInterestedInFileDrag({"not-a-wave.txt"}),"WAV drag interest");
        auto drop=samplePanel->sampleDropped;drop(fixture);drain();
        require(document.samples.at("source").path==fixture.getFullPathName().toStdString(),"WAV drop callback loading");
        editor.knobs[5]->toggle.setToggleState(true,dontSendNotification);editor.knobs[5]->toggle.onClick();
        require(handles.at("source")->values[5].load()==1&&hardware.find("source")->knobs[5]->toggle.getToggleState(),"Loop toggle synchronization");
        setParameter("source","start",.05f); // saved parameter change; UI callbacks tested above
        auto sampleSerial=JSON::toString(rackJSON(document));auto before=sampleAudio(document);
        require(commit(readRack(JSON::parse(sampleSerial)),"source"),"Sample recall queued");drain();
        require(before==sampleAudio(document)&&JSON::toString(rackJSON(document))==sampleSerial,"External sample recall audio/metadata");
        setSize(1120,800);capture("sample-minimum.png");
        writeFixture(.5f);commit(readRack(JSON::parse(sampleSerial)),"source");drain();
        require(status.getText().contains("changed")&&JSON::toString(rackJSON(document))==sampleSerial,"Changed sample preserves playing patch");
        auto mixed=document;mixed.graph.nodes.push_back({"another","sample.player",{}});mixed.graph.outputs.push_back({"another","out"});mixed.samples["another"]=document.samples.at("source");
        mixed.samples["source"]={fixture.getFullPathName().toStdString(),{}, {}};prepareSamples(mixed);
        require(mixed.samples["source"].fingerprint!=mixed.samples["another"].fingerprint,"Explicit reload cannot reuse another instance's cached audio");
        auto missing=document;missing.samples["source"]={directory.getChildFile("does-not-exist.wav").getFullPathName().toStdString(),{}, {}};
        commit(missing,"source");drain();require(status.getText().contains("Cannot open")&&JSON::toString(rackJSON(document))==sampleSerial,"Missing sample preserves playing patch");
        auto dormant=document;dormant.graph.nodes.push_back({"dormant","sample.player",{}});dormant.samples["dormant"]=missing.samples["source"];
        require(commit(dormant,"source")&&!loading&&!document.samples["dormant"].data,"Dormant missing file does not load");
        auto activated=document;activated.graph.outputs={{"dormant","out"}};commit(activated,"dormant");drain();require(document.graph.outputs[0].node!="dormant","Failed activation keeps previous output");
        commit(missing,"source");setParameter("source","level",.6f);require(!loading,"Editing cancels pending load");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));collectSamples();require(document.samples.at("source").data!=nullptr,"Cancelled load cannot replace patch");
        SampleLoader newest;newest.submit(missing,"obsolete");newest.submit(starterDocument(4,.25f),"newest");
        std::optional<SampleLoader::Result> latest;for(int i=0;i<10000&&!latest;++i){latest=newest.take();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        require(latest&&latest->preferred=="newest"&&latest->runtime&&latest->error.empty(),"Only newest worker result published");
        auto oldRack=rackJSON({rackPreset(0),.25f,{}});oldRack.getDynamicObject()->setProperty("version",1);oldRack.getDynamicObject()->removeProperty("samples");require(render(readRack(oldRack).graph)==render(rackPreset(0)),"Version one rack import unchanged");
        auto v2=oldRack;v2.getDynamicObject()->setProperty("version",2);v2.getDynamicObject()->setProperty("samples",juce::var(new juce::DynamicObject));require(render(readRack(v2).graph)==render(rackPreset(0)),"Version two rack import unchanged");
        setSize(1440,940);require(commit(starterDocument(6,.25f),"spectral")&&loading,"Spectral prepares in worker");drain();
        require(document.spectral.at("spectral")->tables[0]&&activeCount==2,"Whole Spectral and external LFO active");
        auto* spectralPanel=hardware.find("spectral");require(spectralPanel&&spectralPanel->jacks.size()==19&&spectralPanel->presetName.getText().contains("Spectral"),"Contextual full-engine panel");
        spectralPanel->section.setSelectedId(2,sendNotificationSync);capture("spectral-stereo.png");
        showMatrix();matrixWindow->editor->editDepth(0,.23f);require(document.graph.modulations[0].amount==.23f&&routeHandles->amounts[0].load()==.23f,"Live depth updates stored route");
        {auto image=matrixWindow->editor->createComponentSnapshot(matrixWindow->editor->getLocalBounds());FileOutputStream stream(directory.getChildFile("spectral-matrix.png"));require(stream.openedOk(),"Matrix snapshot file");stream.setPosition(0);stream.truncate();require(PNGImageFormat().writeImageToStream(image,stream),"Matrix screenshot");}
        matrixWindow->setVisible(false);
        spectralPanel->knobs[3]->slider.setValue(2500,sendNotificationSync);
        require(handles.at("spectral")->values[3].load()==2500&&editor.knobs[3]->slider.getValue()==2500,"Spectral live controls synchronize");
        auto spectralSerial=JSON::toString(rackJSON(document));auto spectralRecall=readRack(JSON::parse(spectralSerial));
        require(partials::patchText(spectralRecall.spectral.at("spectral")->patch)==partials::patchText(document.spectral.at("spectral")->patch)&&spectralRecall.graph.modulations.size()==1,"Full embedded patch and routes recalled");
        auto stereoAudio=[](const RackDocument& d){Runtime r(d.graph,sampleBank(d),d.spectral);std::vector<float> x(4800),y(4800);r.render(x.data(),y.data(),4800,{48,.8f,true,true});x.insert(x.end(),y.begin(),y.end());return x;};
        require(stereoAudio(document)==stereoAudio(spectralRecall),"Full-engine rack stereo audio recall");
        auto cycle=document;cycle.graph.modulations.push_back({{"spectral","lfo1"},{"lfo","depth"},.2f});require(!commit(cycle,"spectral")&&JSON::toString(rackJSON(document))==spectralSerial,"Matrix cycle keeps current state");
        auto broken=JSON::parse(spectralSerial);broken.getDynamicObject()->getProperty("spectral").getDynamicObject()->setProperty("spectral","not a patch");bool failed=false;
        try{readRack(broken);}catch(const std::exception&){failed=true;}require(failed,"Malformed embedded engine rejected");
        const auto presetFile=directory.getChildFile("spectral-native-fixture.ptch");auto patch=spectralDemoPatch();patch.name="Native imported preset";std::string error;
        require(partials::savePatch(presetFile.getFullPathName().toStdString(),patch,error),"Write first-party preset fixture");loadSpectral("spectral",presetFile);drain();
        require(document.spectral.at("spectral")->patch.name==patch.name,"Worker preset import installed whole state");
        auto preserved=JSON::toString(rackJSON(document));loadSpectral("spectral",directory.getChildFile("missing.ptch"));drain();require(JSON::toString(rackJSON(document))==preserved&&status.getText().contains("missing"),"Failed preset keeps playing graph");
        auto pending=starterDocument(6,.25f);commit(pending,"spectral");setParameter("spectral","main_volume",-9);require(!loading,"Spectral load cancelled by scalar edit");
        commit(starterDocument(7,.25f),"spectral");drain();
        require(hardware.find("spectral")->knobs[48]->getAlpha()==1.f&&hardware.find("spectral")->jacks[1]->connected,"Exported LFO rate and socket show active routing");
        capture("spectral-resonator.png");
        setSize(1120,800);selectNode("spectral");hardware.find("spectral")->section.setSelectedId(2,sendNotificationSync);capture("spectral-minimum.png");
        require(directory.getChildFile("spectral-check.txt").replaceWithText("PASS: background full-engine load; stereo; active LFO routing; matrix live depth; preset control synchronisation; complete embedded state and stereo audio recall; cycle/malformed-state rejection; imported preset worker; missing-file preservation; cancellation; explicit mono adapter; four native captures. No audio device opened.\n"),"Write Spectral UI receipt");
        for(int model=0;model<3;++model){
            require(commit(starterDocument(8+model,.25f),"sequence"),"Sequencer starter");
            require(hardware.find("sequence")->sequence&&editor.sequence,"Contextual sequencer editors");
            auto* sequence=hardware.find("sequence")->sequence.get();
            sequence->selectStep(model==1?0:2);sequence->pitchControl().setValue(72,sendNotificationSync);
            const auto key=model==1?"pitch":"step3_pitch";const auto& ps=panel(document.graph.nodes.front().type).parameters;
            const auto index=std::find_if(ps.begin(),ps.end(),[&](const Parameter& p){return p.id==key;})-ps.begin();
            require(handles.at("sequence")->values[size_t(index)].load()==72,"Visible sequencer edit reaches DSP");
            editor.sequence->selectStep(model==1?0:2);require(editor.sequence->pitchControl().getValue()==72,"Rack/focus selected-step synchronization");
            tempo.setValue(137.5,sendNotificationSync);const auto saved=rackJSON(document);const auto recalled=readRack(saved);
            require(recalled.tempo==137.5f,"Transport tempo recall");
            auto renderSequence=[](const RackDocument& d){Runtime r(d.graph);r.clock.tempo(d.tempo);r.clock.play(true);std::vector<float> out(48000);r.render(out.data(),int(out.size()),{});return out;};
            require(renderSequence(document)==renderSequence(recalled),"Sequencer native state/audio recall");
            auto old=JSON::parse(JSON::toString(saved));old.getDynamicObject()->setProperty("version",3);old.getDynamicObject()->removeProperty("tempo");
            require(readRack(old).tempo==120,"Version 3 rack defaults transport tempo");
            auto badTempo=JSON::parse(JSON::toString(saved));badTempo.getDynamicObject()->setProperty("tempo",0);bool rejected=false;try{readRack(badTempo);}catch(const std::exception&){rejected=true;}require(rejected,"Invalid tempo rejected");
            require(directory.getChildFile("sequencer-"+String(model)+".optisynth").replaceWithText(JSON::toString(saved)),"Saved sequencer starter fixture");
            player.playing=true;player.restart();std::array<float,480> clockAudio;
            for(int i=0;i<8;++i){player.collect();player.render(clockAudio.data(),int(clockAudio.size()));}
            require(handles.at("sequence")->playhead.load()>=0,"Player publishes routed playhead");
            sequence->pollPlayhead();editor.sequence->pollPlayhead();updateTransport();
            focusMode.setToggleState(true,dontSendNotification);resized();capture("sequencer-"+String(model)+"-focus.png");
            focusMode.setToggleState(false,dontSendNotification);resized();capture("sequencer-"+String(model)+"-rack.png");
            player.playing=false;updateTransport();require(sequence->displayedStep()==-1&&editor.sequence->displayedStep()==-1,"Stop clears stale UI playhead before another audio callback");
            player.render(clockAudio.data(),int(clockAudio.size()));require(handles.at("sequence")->playhead.load()==-1,"Native transport stop");
        }
        setSize(1120,800);resized();capture("sequencer-minimum.png");setSize(1440,940);resized();
        require(directory.getChildFile("sequencer-check.txt").replaceWithText("PASS: three contextual native editors; selected-step rack/focus edits reach DSP; full sequence and tempo recall with identical audio; v3 compatibility; invalid tempo rejection; Player clock and stop; seven sequencer captures. No audio device opened.\n"),"Write sequencer receipt");
        require(directory.getChildFile("ui-check.txt").replaceWithText("PASS: independent controls; rack/focus synchronization; JSON/audio recall; invalid-edit preservation; dormant add/remove; typed routing and cycle rejection; contextual replacement; 26 panel layouts; legacy rack import; background sample and Spectral loading/recall/cancellation; contextual sequencer editing and transport. No audio device opened.\n"),"Write UI receipt");
    }
private:
    struct InputRow { Label label;ComboBox choice;std::vector<Endpoint> ports; };
    Player player;OutputConverter converter;RackDocument document{rackPreset(0),.25f};
    SampleLoader sampleLoader;bool loading=false;
    std::shared_ptr<RouteControls> routeHandles;
    std::unique_ptr<MatrixWindow> matrixWindow;
    TextButton matrixButton{"Matrix"};
    Registry registry=moduleRegistry();
    MidiKeyboardState keys;MidiKeyboardComponent keyboard;TypingKeyboard typing;
    ListBox rack;ModulePanel editor{"all"};Values panelValues;
    HardwareRack hardware;Viewport rackView;TextButton focusMode{"Focus module"};TooltipWindow tips{this,700};
    std::map<std::string,std::shared_ptr<Controls>> handles;
    std::vector<std::unique_ptr<InputRow>> inputs;
    std::vector<std::string> addTypes;std::vector<Endpoint> outputPorts;
    std::vector<int> held;std::string selected;int activeCount=0;
    ComboBox addChoice,starter,output;
    TextButton add{"Add module"},remove{"Remove"},save{"Save"},load{"Open"},audio{"Start audio"},silence{"Silence"};
    Slider gain;TextEditor routes;Label status,keyboardHelp;
    TextButton play{"Play seq"},restart{"Restart"};Slider tempo;Label position;
    std::unique_ptr<FileChooser> chooser;
    bool refreshing=false,running=false;
    int bodyBottom() const {return getHeight()-185;}
    const ModuleType& type(const std::string& id) const {for(const auto& t:registry)if(t.id==id)return t;throw std::invalid_argument("Unknown type");}
    void message(const String& text,bool error=false){status.setColour(Label::textColourId,error?Colour(0xffffb180):paper);status.setText(text,dontSendNotification);}
    void setParameter(const std::string& id,const std::string& key,float value) {
        cancelLoading();
        for(auto& n:document.graph.nodes)if(n.id==id){auto it=std::find_if(n.parameters.begin(),n.parameters.end(),[&](const auto& p){return p.first==key;});if(it==n.parameters.end())n.parameters.push_back({key,value});else it->second=value;}
        if(auto* p=hardware.find(id)){hardware.values[id][p->current][key]=value;p->setValue(key,value);}
        if(editor.instance==id){panelValues[editor.current][key]=value;editor.setValue(key,value);}
        auto handle=handles.find(id);if(handle!=handles.end()&&handle->second){
            auto node=std::find_if(document.graph.nodes.begin(),document.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id;});
            if(node!=document.graph.nodes.end()){const auto& ps=panel(node->type).parameters;for(size_t i=0;i<ps.size();++i)if(ps[i].id==key)handle->second->values[i]=value;}
        }
    }
    void showMatrix(){
        if(!matrixWindow){matrixWindow=std::make_unique<MatrixWindow>();auto* m=matrixWindow->editor;
            m->addRoute=[this](const Endpoint& source,const Endpoint& destination,float amount){auto next=document;next.graph.modulations.push_back({source,destination,amount});return commit(std::move(next),selected);};
            m->setDepth=[this](int i,float amount){cancelLoading();if(i>=0&&i<int(document.graph.modulations.size())){document.graph.modulations[size_t(i)].amount=amount;if(routeHandles)routeHandles->amounts[size_t(i)]=amount;}};
            m->removeRoute=[this](int i){MessageManager::callAsync([safe=SafePointer<RackSynth>(this),i]{if(!safe)return;auto next=safe->document;if(i>=0&&i<int(next.graph.modulations.size())){next.graph.modulations.erase(next.graph.modulations.begin()+i);safe->commit(std::move(next),safe->selected);}});};
        }
        matrixWindow->editor->refresh(document);matrixWindow->setVisible(true);matrixWindow->toFront(true);
    }
    void loadSpectral(const std::string& id,const File& file){
        if(std::none_of(document.graph.nodes.begin(),document.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id&&n.type=="spectral.engine";}))return;
        cancelLoading();sampleLoader.submit(document,id,file.getFullPathName().toStdString());loading=true;message("Loading Spectral preset... current sound stays ready");
    }
    void bindSpectralPanel(ModulePanel& p,const std::string& id){
        if(auto state=effectiveSpectral(document,id))p.setSpectral(state,document.graph);
        p.choosePreset=[safe=SafePointer<RackSynth>(this),id]{if(!safe)return;
            safe->chooser=std::make_unique<FileChooser>("Load imported Spectral preset (.ptch)",File::getSpecialLocation(File::userDocumentsDirectory),"*.ptch");
            safe->chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[safe,id](const FileChooser& c){if(safe&&c.getResult()!=File())safe->loadSpectral(id,c.getResult());});
        };
    }
    void rebuildHardware() {
        hardware.clear();
        for(const auto& node:document.graph.nodes){
            auto card=std::make_unique<ModulePanel>("all");auto* p=card.get();p->instance=node.id;
            auto& values=hardware.values[node.id];for(const auto& v:node.parameters)values[node.type][v.first]=v.second;
            p->show(node.type,values,handles[node.id],[this,id=node.id](const std::string& key,float value){setParameter(id,key,value);});p->setConnections(document.graph);bindSamplePanel(*p,node.id);bindSpectralPanel(*p,node.id);
            p->activated=[this,id=node.id]{if(selected!=id)selectNode(id);};
            p->selectionChanged=[this,id=node.id,p]{
                const auto nextType=p->selected();
                MessageManager::callAsync([safe=SafePointer<RackSynth>(this),id,nextType]{if(safe)safe->replaceModule(id,nextType);});
            };
            p->portClicked=[this,id=node.id](const Port& port,bool output,juce::Component* target){socketMenu(id,port,output,target);};
            hardware.addAndMakeVisible(*p);hardware.modules.push_back(std::move(card));
        }
    }
    bool replaceModule(const std::string& id,const std::string& nextType) {
        auto next=document;for(auto& n:next.graph.nodes)if(n.id==id){n.type=nextType;n.parameters.clear();}return commit(std::move(next),id);
    }
    void applySocketChoice(const std::string& node,const Port& port,bool isOutput,int result,const std::vector<Endpoint>& endpoints) {
        if(result==0)return;auto next=document;
        if(result==1){if(isOutput)next.graph.outputs={{node,port.id}};else setRackConnection(next.graph,{node,port.id},{});}
        else {const int i=result-10;if(i<0||i>=int(endpoints.size()))return;
            if(isOutput)setRackConnection(next.graph,endpoints[size_t(i)],{node,port.id});else setRackConnection(next.graph,{node,port.id},endpoints[size_t(i)]);}
        commit(std::move(next),node);
    }
    void socketMenu(const std::string& node,const Port& port,bool isOutput,juce::Component* target) {
        PopupMenu menu;std::vector<Endpoint> endpoints;
        menu.addSectionHeader(String(node)+" / "+String(port.id)+(isOutput?" output":" input"));
        if(!isOutput)menu.addItem(1,"Disconnect input");
        else if(port.type==SignalType::Audio)menu.addItem(1,"Use as instrument output");
        menu.addSeparator();
        for(const auto& n:document.graph.nodes)if(n.id!=node){const auto& ports=isOutput?type(n.type).inputs:type(n.type).outputs;
            for(const auto& p:ports)if(p.type==port.type&&p.channels==port.channels){endpoints.push_back({n.id,p.id});menu.addItem(int(endpoints.size())+9,String(n.id)+" / "+String(p.id)+"  ("+String(panel(n.type).name)+")");}}
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(target),[safe=SafePointer<RackSynth>(this),node,port,isOutput,endpoints](int result){
            if(safe)safe->applySocketChoice(node,port,isOutput,result,endpoints);
        });
    }
    bool commit(RackDocument next,const std::string& preferred) {
        cancelLoading();const auto target=preferred;
        try {
            validateRack(next.graph);pruneSampleReferences(next);
            if(needsSamples(next)){sampleLoader.submit(std::move(next),target);loading=true;message("Preparing patch... current sound stays ready");return true;}
            prepareSamples(next);auto runtime=std::make_unique<Runtime>(next.graph,sampleBank(next),next.spectral);install(std::move(next),std::move(runtime),target);return true;
        } catch(const std::exception& e){refresh(selected);message(String("Route unchanged: ")+e.what(),true);return false;}
    }
    void install(RackDocument next,std::unique_ptr<Runtime> runtime,const std::string& target) {
        std::map<std::string,std::shared_ptr<Controls>> controls;for(const auto& n:next.graph.nodes)controls[n.id]=runtime->controls(n.id);
        routeHandles=runtime->routeControls();document=std::move(next);handles=std::move(controls);activeCount=int(runtime->activeModules());
        player.gain.store(document.gain);gain.setValue(document.gain,dontSendNotification);player.publish(std::move(runtime));
        player.tempo.store(document.tempo);tempo.setValue(document.tempo,dontSendNotification);
        refresh(target);if(matrixWindow)matrixWindow->editor->refresh(document);message("Ready - select a module to edit");
    }
    void cancelLoading(){if(loading){sampleLoader.cancel();loading=false;message("Pending patch load cancelled by edit");}}
    void collectSamples(){if(auto r=sampleLoader.take()){
        loading=false;if(!r->error.empty())message(String("Patch unchanged: ")+r->error,true);
        else install(std::move(r->document),std::move(r->runtime),r->preferred);
    }}
    void loadSample(const std::string& id,const File& file){
        auto next=document;
        if(std::none_of(next.graph.nodes.begin(),next.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id&&n.type=="sample.player";}))return;
        next.samples[id]={file.getFullPathName().toStdString(),{},{}};commit(std::move(next),id);
    }
    void bindSamplePanel(ModulePanel& p,const std::string& id){
        auto it=document.samples.find(id);p.setSample(it==document.samples.end()?"":it->second.path,it==document.samples.end()?SamplePtr{}:it->second.data);
        p.sampleDropped=[safe=SafePointer<RackSynth>(this),id](const File& f){if(safe)safe->loadSample(id,f);};
        p.chooseSample=[safe=SafePointer<RackSynth>(this),id]{if(!safe)return;
            safe->chooser=std::make_unique<FileChooser>("Load WAV (32 MiB maximum)",File::getSpecialLocation(File::userMusicDirectory),"*.wav");
            safe->chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[safe,id](const FileChooser& c){if(safe&&c.getResult()!=File())safe->loadSample(id,c.getResult());});
        };
    }
    void addModule(const std::string& moduleType) {
        auto next=document;int count=1;std::string id;
        do{id="module"+std::to_string(count++);}while(std::any_of(next.graph.nodes.begin(),next.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id;}));
        next.graph.nodes.push_back({id,moduleType,{}});commit(std::move(next),id);
    }
    void removeSelected() {
        if(selected.empty())return;auto next=document;const auto id=selected;
        next.graph.nodes.erase(std::remove_if(next.graph.nodes.begin(),next.graph.nodes.end(),[&](const NodeSpec& n){return n.id==id;}),next.graph.nodes.end());
        next.graph.connections.erase(std::remove_if(next.graph.connections.begin(),next.graph.connections.end(),[&](const Connection& c){return c.source.node==id||c.destination.node==id;}),next.graph.connections.end());
        next.graph.modulations.erase(std::remove_if(next.graph.modulations.begin(),next.graph.modulations.end(),[&](const Modulation& m){return m.source.node==id||m.destination.node==id;}),next.graph.modulations.end());
        if(!commit(std::move(next),""))message("Reroute its destinations/output before removing this module.",true);
    }
    void refresh(const std::string& preferred) {
        refreshing=true;output.clear(dontSendNotification);outputPorts.clear();
        for(const auto& n:document.graph.nodes)for(const auto& p:type(n.type).outputs)if(p.type==SignalType::Audio){
            outputPorts.push_back({n.id,p.id});output.addItem(String(n.id)+" / "+String(p.id),int(outputPorts.size()));
        }
        if(document.graph.outputs.size()==1)for(size_t i=0;i<outputPorts.size();++i)if(outputPorts[i].node==document.graph.outputs[0].node&&outputPorts[i].port==document.graph.outputs[0].port)output.setSelectedId(int(i)+1,dontSendNotification);
        output.setTextWhenNothingSelected(document.graph.outputs.empty()?"No output":"Multiple outputs (saved rack)");
        String text;for(const auto& c:document.graph.connections)text+=String(c.source.node)+"."+String(c.source.port)+"  >  "+String(c.destination.node)+"."+String(c.destination.port)+"\n";
        routes.setText(text,false);rack.updateContent();refreshing=false;
        rebuildHardware();selectNode(preferred);repaint();
    }
    void selectNode(const std::string& id) {
        int row=0;for(size_t i=0;i<document.graph.nodes.size();++i)if(document.graph.nodes[i].id==id)row=int(i);
        refreshing=true;rack.selectRow(row);refreshing=false;
        if(!document.graph.nodes.empty()){selected=document.graph.nodes[size_t(row)].id;showSelected();
            if(auto* p=hardware.find(selected)){const int left=rackView.getViewPositionX();if(p->getX()<left)rackView.setViewPosition(p->getX(),0);else if(p->getRight()>left+rackView.getWidth())rackView.setViewPosition(p->getRight()-rackView.getWidth()+8,0);}}
    }
    void showSelected() {
        auto it=std::find_if(document.graph.nodes.begin(),document.graph.nodes.end(),[&](const NodeSpec& n){return n.id==selected;});
        if(it==document.graph.nodes.end())return;
        const auto node=*it;panelValues.clear();for(const auto& p:node.parameters)panelValues[node.type][p.first]=p.second;
        editor.instance=node.id;
        editor.show(node.type,panelValues,handles[node.id],[this,id=node.id](const std::string& key,float value){setParameter(id,key,value);});
        bindSamplePanel(editor,node.id);
        bindSpectralPanel(editor,node.id);
        editor.setConnections(document.graph);editor.portClicked=[this,id=node.id](const Port& port,bool output,juce::Component* target){socketMenu(id,port,output,target);};
        hardware.highlight(selected);
        // Persistent rows avoid destroying the ComboBox inside its own onChange.
        const auto& ports=type(node.type).inputs;
        while(inputs.size()<ports.size()){auto row=std::make_unique<InputRow>();addAndMakeVisible(row->label);addAndMakeVisible(row->choice);inputs.push_back(std::move(row));}
        for(size_t i=0;i<inputs.size();++i){
            auto& row=*inputs[i];row.label.setVisible(i<ports.size());row.choice.setVisible(i<ports.size());if(i>=ports.size())continue;
            const auto p=ports[i];row.label.setText(String(p.id).toUpperCase()+"  /  "+(p.type==SignalType::Audio?"audio":p.type==SignalType::Event?"notes":"control")+(p.required?" *":""),dontSendNotification);
            row.label.setFont(font(12));row.label.setColour(Label::textColourId,p.type==SignalType::Audio?paper:Colour(0xffe7a775));
            row.choice.clear(dontSendNotification);row.ports={{"",""}};row.choice.addItem(p.type==SignalType::Event?"Keyboard (unpatched)":"Not connected",1);
            for(const auto& n:document.graph.nodes)if(n.id!=node.id)for(const auto& out:type(n.type).outputs)if(out.type==p.type&&out.channels==p.channels){row.ports.push_back({n.id,out.id});row.choice.addItem(String(n.id)+" / "+String(out.id),int(row.ports.size()));}
            int selection=1;for(const auto& c:document.graph.connections)if(c.destination.node==node.id&&c.destination.port==p.id)for(size_t j=1;j<row.ports.size();++j)if(row.ports[j].node==c.source.node&&row.ports[j].port==c.source.port)selection=int(j)+1;
            row.choice.setSelectedId(selection,dontSendNotification);
            row.choice.onChange=[this,i,id=node.id,port=p.id]{
                if(refreshing)return;const int index=inputs[i]->choice.getSelectedId()-1;if(index<0)return;
                auto next=document;setRackConnection(next.graph,{id,port},inputs[i]->ports[size_t(index)]);commit(std::move(next),id);
            };
        }
        resized();repaint();
    }
    int getNumRows() override {return int(document.graph.nodes.size());}
    void paintListBoxItem(int row,Graphics& g,int width,int height,bool isSelected) override {
        if(row<0||row>=getNumRows())return;const auto& n=document.graph.nodes[size_t(row)];
        g.setColour(isSelected?teal:Colour(0xff25343a));g.fillRoundedRectangle(0,2,float(width),float(height-4),5);
        g.setColour(paper);g.setFont(font(14));g.drawText(panel(n.type).name,12,8,width-24,23,Justification::centredLeft);
        g.setFont(font(11));g.setColour(paper.withAlpha(.65f));g.drawText(String(n.id)+(handles[n.id]?"  /  active":"  /  unrouted"),12,33,width-24,19,Justification::centredLeft);
    }
    void selectedRowsChanged(int row) override {if(!refreshing&&row>=0&&row<getNumRows()){selected=document.graph.nodes[size_t(row)].id;showSelected();}}
    void chooseFile(bool writing) {
        chooser=std::make_unique<FileChooser>(writing?"Save Optisynth rack":"Open Optisynth patch",File::getSpecialLocation(File::userDocumentsDirectory),"*.optisynth");
        const int flags=writing?FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting:FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(flags,[safe=SafePointer<RackSynth>(this),writing](const FileChooser& c){
            if(!safe)return;auto f=c.getResult();if(f==File())return;
            try{if(writing){f=f.withFileExtension("optisynth");if(!f.replaceWithText(JSON::toString(rackJSON(safe->document))))throw std::runtime_error("Could not save file");safe->message("Saved "+f.getFileName()+(safe->document.samples.empty()?"":" - WAV files remain external"));}
                else{if(f.getSize()>2*1024*1024)throw std::runtime_error("Patch file too large");if(safe->commit(readRack(JSON::parse(f)),"")&&!safe->loading)safe->message("Opened "+f.getFileName());}}
            catch(const std::exception& e){safe->message(e.what(),true);}
        });
    }
    void toggleAudio() {
        if(running){stopAudio();return;}
        setAudioChannels(0,2);running=deviceManager.getCurrentAudioDevice()!=nullptr;
        audio.setButtonText(running?"Stop audio":"Start audio");message(running?"Audio on - play the keyboard":"Could not open audio output",!running);grabKeyboardFocus();
    }
    void stopAudio() {player.playing.store(false);updateTransport();typing.release([this](int n){keys.noteOff(1,n,0);});keys.allNotesOff(1);held.clear();player.note(48,false);shutdownAudio();running=false;audio.setButtonText("Start audio");message("Audio stopped");}
    void updateTransport(){
        const bool active=player.playing.load();play.setButtonText(active?"Stop seq":"Play seq");
        if(editor.sequence)editor.sequence->setTransportActive(active);
        for(auto& panel:hardware.modules)if(panel->sequence)panel->sequence->setTransportActive(active);
        const double beat=player.beat.load();position.setText(String(int(beat/4)+1)+" : "+String(std::fmod(beat,4)+1,1)+(running?"":" / audio off"),dontSendNotification);
    }
    void handleNoteOn(MidiKeyboardState*,int,int note,float) override {held.erase(std::remove(held.begin(),held.end(),note),held.end());held.push_back(note);player.note(note,true);}
    void handleNoteOff(MidiKeyboardState*,int,int note,float) override {held.erase(std::remove(held.begin(),held.end(),note),held.end());player.note(held.empty()?note:held.back(),!held.empty());}
    void timerCallback() override {
        player.collect();collectSamples();
        updateTransport();
        auto* focus=juce::Component::getCurrentlyFocusedComponent();
        bool textEditing=false;for(auto* c=focus;c;c=c->getParentComponent())if(dynamic_cast<TextEditor*>(c)||dynamic_cast<ComboBox*>(c))textEditing=true;
        const auto mods=ModifierKeys::getCurrentModifiersRealtime();
        const bool caps=(CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState)&kCGEventFlagMaskAlphaShift)!=0;
        const bool enabled=caps&&Process::isForegroundProcess()&&hasKeyboardFocus(true)&&!textEditing&&!mods.isCommandDown()&&!mods.isCtrlDown()&&!mods.isAltDown();
        std::array<bool,18> down{};for(size_t i=0;i<down.size();++i)down[i]=KeyPress::isKeyCurrentlyDown(TypingKeyboard::keys[i]);
        typing.update(enabled,down,KeyPress::isKeyCurrentlyDown('Z'),KeyPress::isKeyCurrentlyDown('X'),[this](int n){keys.noteOn(1,n,.8f);},[this](int n){keys.noteOff(1,n,0);});
        MidiBuffer discard;keys.processNextMidiBuffer(discard,0,1,false);
        keyboardHelp.setText(String(enabled?"CAPS LOCK ON":"Caps Lock to play")+"  |  A W S E D F ...   Z/X octave  |  base "+MidiMessage::getMidiNoteName(typing.base,true,true,4),dontSendNotification);
        repaint(335,getHeight()-155,170,20);
    }
};
}
