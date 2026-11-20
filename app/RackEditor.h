#pragma once
#include "Panel.h"
#include "PatchIO.h"
#include "SampleLoader.h"
#include "ModulationEditor.h"
#include "SynthPanel.h"
#include "PresetBrowser.h"
#include <chrono>
#include <fstream>

namespace {
// Header preset display: name, modified mark, category and tags; click to browse.
struct PresetDisplay final : Button {
    String title="Untitled",detail;bool modified=false;
    PresetDisplay():Button("Preset"){setTooltip("Browse presets: categories, tags, favourites, search");}
    void paintButton(Graphics& g,bool over,bool) override {
        auto r=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(Colour(0xff111416));g.fillRoundedRectangle(r,4);
        g.setColour(getToggleState()||over?teal:Colour(0xff3c4449));g.drawRoundedRectangle(r,4,1.2f);
        g.setColour(Colour(0xff8fd0c2));g.setFont(font(15));g.drawText(title+(modified?"  *":""),12,3,getWidth()-46,19,Justification::centredLeft,true);
        g.setColour(paper.withAlpha(.55f));g.setFont(font(10.5f));g.drawText(detail,12,21,getWidth()-46,14,Justification::centredLeft,true);
        Path arrow;const float x=float(getWidth()-24),y=getHeight()*.5f;
        if(getToggleState())arrow.addTriangle(x,y+3,x+10,y+3,x+5,y-3);else arrow.addTriangle(x,y-3,x+10,y-3,x+5,y+3);
        g.setColour(paper.withAlpha(.7f));g.fillPath(arrow);
    }
};
class RackSynth final : public AudioAppComponent, private Timer,
                        private MidiKeyboardState::Listener, private ListBoxModel, private MidiInputCallback {
public:
    RackSynth(const File& settingsFile,const File& libraryFolder) : prefs(settingsFile), browser(prefs,libraryFolder),
        keyboard(keys,MidiKeyboardComponent::horizontalKeyboard), rack("Modules",this) {
        setWantsKeyboardFocus(true);setSize(1440,940);
        for(auto* c:std::initializer_list<juce::Component*>{&rack,&editor,&addChoice,&add,&remove,&audio,&silence,&gain,&keyboard,&output,&routes,&status,&keyboardHelp,&rackView,&focusMode,&matrixButton,&play,&restart,&tempo,&position,
            &prevPreset,&nextPreset,&favouriteButton,&presetDisplay,&savePreset,&panelViewButton,&modularViewButton,&viewMenu,&zoomOut,&zoomLabel,&zoomIn,&zoomFit,&synthPanel})addAndMakeVisible(c);
        addChildComponent(browser);
        rackView.setViewedComponent(&hardware,false);rackView.setScrollBarsShown(true,true);rackView.setScrollBarThickness(12);
        hardware.knob=std::clamp(int(prefs.number("rackKnob",44)),densityKnobs.front(),densityKnobs.back());
        hardware.magnified=[this](float scale){if(currentView!="panel"&&std::abs(scale-1.f)>.08f)setDensity(stepDensity(scale>1?1:-1));};
        zoomOut.onClick=[this]{setDensity(stepDensity(-1));};zoomIn.onClick=[this]{setDensity(stepDensity(1));};zoomLabel.onClick=[this]{setDensity(44);};zoomFit.onClick=[this]{fitDensity();};
        zoomOut.setTooltip("Denser rack: smaller knobs and spacing; text keeps its size (Cmd -, or pinch)");zoomIn.setTooltip("Roomier rack (Cmd =)");
        zoomLabel.setTooltip("Back to Normal (Cmd 0)");zoomFit.setTooltip("Largest knob size that shows the whole rack");
        prevPreset.onClick=[this]{stepPreset(-1);};nextPreset.onClick=[this]{stepPreset(1);};
        prevPreset.setTooltip("Previous preset in the browser's current list");nextPreset.setTooltip("Next preset in the browser's current list");
        favouriteButton.onClick=[this]{browser.toggleFavourite(presetKey);updatePresetDisplay();};favouriteButton.setTooltip("Favourite this preset");
        presetDisplay.onClick=[this]{showBrowser(!browser.isVisible());};
        savePreset.onClick=[this]{showSaveForm();};savePreset.setTooltip("Save the current sound to the preset library (Cmd S)");
        for(auto* b:{&panelViewButton,&modularViewButton}){b->setRadioGroupId(71);b->setClickingTogglesState(true);
            b->setColour(TextButton::buttonOnColourId,teal);b->setColour(TextButton::textColourOnId,ink);}
        panelViewButton.onClick=[this]{setView("panel",true);};modularViewButton.onClick=[this]{setView("modular",true);};
        panelViewButton.setTooltip("Synth panel: the patch's playing controls on one front panel");modularViewButton.setTooltip("Modular rack: every module, socket and route");
        viewMenu.onClick=[this]{showViewMenu();};viewMenu.setTooltip("Which view opens when a patch loads");
        synthPanel.changed=[this](const std::string& node,const std::string& key,float value){setParameter(node,key,value);};
        synthPanel.openEditor=[this](const std::string& node){if(auto* card=hardware.find(node))card->openSequencerWindow();};
        browser.load=[this](const PresetEntry& e){loadPreset(e);};
        browser.close=[this]{showBrowser(false);};
        browser.importFile=[this]{chooseFile(false);};browser.exportFile=[this]{chooseFile(true);};
        browser.saveCurrent=[this](const PresetInfo& info,bool panelView,bool overwrite){return saveToLibrary(info,panelView,overwrite);};
        browser.requestSaveForm=[this]{showSaveForm();};
        browser.infoEdited=[this](const std::string& key,const PresetInfo& info){if(key==presetKey){document.info=info;synthPanel.build(document);updatePresetDisplay();}};
        browser.favouritesChanged=[this]{updatePresetDisplay();};
        focusMode.setClickingTogglesState(true);focusMode.onClick=[this]{showSelected();};
        rack.setRowHeight(62);rack.setColour(ListBox::backgroundColourId,Colour(0xff25343a));
        rack.setColour(ListBox::outlineColourId,Colours::transparentBlack);
        int i=0;for(const auto& p:panels()){addTypes.push_back(p.type);addChoice.addItem(p.name,++i);}
        addChoice.setSelectedId(1,dontSendNotification);
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
        matrixButton.onClick=[this]{showMatrix();};
        audio.onClick=[this]{toggleAudio();};silence.onClick=[this]{stopAudio();};
        play.onClick=[this]{player.playing.store(!player.playing.load());updateTransport();};
        restart.onClick=[this]{player.restart();};restart.setTooltip("Return all sequencers to beat zero; keep current play/stop state");
        tempo.setScrollWheelEnabled(false);tempo.setSliderStyle(Slider::LinearHorizontal);tempo.setRange(20,300,.1);tempo.setValue(120);
        tempo.setTextBoxStyle(Slider::TextBoxRight,false,82,24);tempo.setTextValueSuffix(" BPM");
        tempo.onValueChange=[this]{cancelLoading();document.tempo=float(tempo.getValue());player.tempo.store(document.tempo);};
        position.setFont(font(12));position.setColour(Label::textColourId,paper);
        gain.setScrollWheelEnabled(false);gain.setSliderStyle(Slider::LinearHorizontal);gain.setRange(0,1,.001);gain.setValue(document.gain);
        gain.setTextBoxStyle(Slider::TextBoxRight,false,55,24);gain.setColour(Slider::trackColourId,teal);
        gain.onValueChange=[this]{cancelLoading();document.gain=float(gain.getValue());player.gain.store(document.gain);};
        keyboard.setAvailableRange(24,96);keyboard.setLowestVisibleKey(24);keyboard.setOctaveForMiddleC(4);keyboard.clearKeyMappings();
        keys.addListener(this);
        routes.setMultiLine(true);routes.setReadOnly(true);routes.setScrollbarsShown(true);
        routes.setColour(TextEditor::backgroundColourId,Colour(0xff243238));routes.setColour(TextEditor::textColourId,paper);
        routes.setColour(TextEditor::outlineColourId,Colours::transparentBlack);routes.setFont(font(12));
        status.setFont(font(13));status.setColour(Label::textColourId,paper);
        keyboardHelp.setFont(font(12));keyboardHelp.setColour(Label::textColourId,paper.withAlpha(.8f));
        loadFactory(0);startTimerHz(60);
    }
    ~RackSynth() override {stopTimer();deviceManager.removeMidiInputDeviceCallback({},this);shutdownAudio();keys.removeListener(this);rack.setModel(nullptr);rackView.setViewedComponent(nullptr,false);}
    void prepareToPlay(int,double rate) override {converter.prepare(rate);}
    void releaseResources() override {}
    void showSampleDemo(){loadFactory(4);}
    void showSpectralDemo(){loadFactory(6);}
    void showReferenceDemo(){openReferenceWhenReady=true;loadFactory(17);}
    void showSequencerDemo(){openPerformanceWhenReady=true;loadFactory(13);}
    void getNextAudioBlock(const AudioSourceChannelInfo& block) override {
        if(block.buffer->getNumChannels()<1)return;
        if(block.buffer->getNumChannels()>1)converter.render(player,block.buffer->getWritePointer(0,block.startSample),block.buffer->getWritePointer(1,block.startSample),block.numSamples);
        else converter.render(player,block.buffer->getWritePointer(0,block.startSample),block.numSamples);
        for(int c=2;c<block.buffer->getNumChannels();++c)block.buffer->clear(c,block.startSample,block.numSamples);
    }
    void paint(Graphics& g) override {
        g.fillAll(Colour(0xff222426));g.setColour(paper);g.setFont(font(29));g.drawText("OPTISYNTH",24,14,200,35,Justification::centredLeft);
        g.setFont(font(12));g.setColour(teal);g.drawText("MODULAR INSTRUMENT  /  "+JUCEApplication::getInstance()->getApplicationVersion(),25,50,200,18,Justification::centredLeft);
        const int right=getWidth()-324,top=bodyTop();
        g.setColour(paper);g.setFont(font(12));
        if(currentView!="panel"){
            g.drawText("OUTPUT",414,110,58,30,Justification::centredLeft);
            if(focusMode.getToggleState()){
                g.drawText("INPUTS  /  "+String(selected),right,top+5,300,20,Justification::centredLeft);
                g.drawText("CONNECTIONS",right,top+173,300,20,Justification::centredLeft);
            }
        }
        g.drawText("OUTPUT",24,getHeight()-158,75,26,Justification::centredLeft);
        const float peak=player.peak.load(std::memory_order_relaxed);
        g.setColour(Colour(0xff34464c));g.fillRoundedRectangle(340.f,float(getHeight()-150),160,8,4);
        g.setColour(peak>.9f?orange:teal);g.fillRoundedRectangle(340.f,float(getHeight()-150),160*std::min(1.f,peak),8,4);
        g.setColour(paper.withAlpha(.6f));g.setFont(font(11));
        const String summary=currentView=="panel"
            ?String(int(synthPanel.controlCount()))+" panel controls in "+String(int(synthPanel.sectionCount()))+" sections  |  "+(synthPanel.isCurated()?"layout saved with the patch":"automatic layout: one section per module")+"   |   Patching and module changes are in the Modular view."
            :String(activeCount)+" active / "+String(int(document.graph.nodes.size()))+" modules  |  stereo / independent note routes   |   Click sockets to patch.   |   Right-click a module (or its dots button) for rows.   |   Size: Cmd - / = / 0 or pinch.";
        g.drawText(summary,24,bodyBottom()+2,getWidth()-48,20,Justification::centredLeft);
    }
    void resized() override {
        const int w=getWidth(),right=w-324,bottom=bodyBottom(),top=bodyTop();
        const bool modular=currentView!="panel",focused=modular&&focusMode.getToggleState();
        // Preset bar, view switch, audio.
        const int y1=16;
        audio.setBounds(w-258,y1,145,34);silence.setBounds(w-105,y1,81,34);
        viewMenu.setBounds(w-258-14-30,y1,30,34);modularViewButton.setBounds(viewMenu.getX()-86,y1,86,34);panelViewButton.setBounds(modularViewButton.getX()-72,y1,72,34);
        prevPreset.setBounds(230,y1,30,34);nextPreset.setBounds(262,y1,30,34);favouriteButton.setBounds(298,y1,34,34);
        savePreset.setBounds(panelViewButton.getX()-14-70,y1,70,34);
        presetDisplay.setBounds(340,y1-2,std::max(160,savePreset.getX()-8-340),38);
        // Transport and status.
        const int y2=64;
        play.setBounds(230,y2,86,32);restart.setBounds(322,y2,75,32);tempo.setBounds(405,y2+1,206,30);position.setBounds(619,y2,170,32);
        status.setBounds(797,y2-6,std::max(100,w-797-24),44);
        // Rack tools (modular view only).
        const int y3=110;
        for(auto* c:std::initializer_list<juce::Component*>{&addChoice,&add,&remove,&output,&matrixButton,&focusMode,&zoomOut,&zoomLabel,&zoomIn,&zoomFit})c->setVisible(modular);
        addChoice.setBounds(24,y3,190,30);add.setBounds(222,y3,90,30);remove.setBounds(318,y3,84,30);output.setBounds(474,y3,200,30);
        zoomFit.setBounds(w-24-48,y3,48,30);zoomIn.setBounds(zoomFit.getX()-6-30,y3,30,30);zoomLabel.setBounds(zoomIn.getX()-76,y3,76,30);zoomOut.setBounds(zoomLabel.getX()-30,y3,30,30);
        focusMode.setBounds(zoomOut.getX()-16-124,y3,124,30);matrixButton.setBounds(focusMode.getX()-6-80,y3,80,30);
        const bool zoomable=modular&&!focused;for(auto* c:{&zoomOut,&zoomLabel,&zoomIn,&zoomFit})c->setEnabled(zoomable);
        // Body.
        rack.setBounds(24,top+5,220,bottom-top-5);editor.setBounds(260,top+5,right-276,bottom-top-5);
        rack.setVisible(focused);editor.setVisible(focused);routes.setVisible(focused);rackView.setVisible(modular&&!focused);
        int y=top+34;for(size_t i=0;i<inputs.size();++i){auto& row=*inputs[i];const bool visible=focused&&!selected.empty()&&i<type(editor.current).inputs.size();row.label.setVisible(visible);row.choice.setVisible(visible);row.label.setBounds(right,y,300,20);row.choice.setBounds(right,y+23,300,32);y+=65;}
        routes.setBounds(right,top+200,300,std::max(40,bottom-top-200));
        rackView.setBounds(24,top,w-48,bottom-top);arrangeRack();
        synthPanel.setVisible(!modular);synthPanel.setBounds(24,top,w-48,bottom-top);
        browser.setBounds(24,106,w-48,bottom-106);
        gain.setBounds(98,getHeight()-158,220,27);keyboardHelp.setBounds(525,getHeight()-163,getWidth()-549,38);
        keyboard.setBounds(24,getHeight()-113,getWidth()-48,89);keyboard.setKeyWidth(float(getWidth()-48)/43.f);
    }
    bool keyPressed(const KeyPress& key) override {
        // Left / right step presets like the header arrows (browser closed, no modifiers).
        if(!browser.isVisible()&&key.getModifiers().isAnyModifierKeyDown()==false){
            if(key==KeyPress::leftKey){stepPreset(-1);return true;}
            if(key==KeyPress::rightKey){stepPreset(1);return true;}
        }
        if(!key.getModifiers().isCommandDown())return false;
        const int code=key.getKeyCode();
        if(code=='S'||code=='s'){showSaveForm();return true;}
        if(currentView=="panel"||focusMode.getToggleState())return false;
        if(code=='='||code=='+'){setDensity(stepDensity(1));return true;}
        if(code=='-'||code=='_'){setDensity(stepDensity(-1));return true;}
        if(code=='0'){setDensity(44);return true;}
        return false;
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
        // Request 72: every module card at three densities. Controls sit inside the card's grid area,
        // never overlap, and are at least as wide as their label and every value they can show.
        for(const auto& spec:panels())for(int density:{32,44,60}){
            ModulePanel test("all");Values values;test.instance="layout";test.knob=density;test.show(spec.type,values,{});test.setSize(test.naturalSize().getWidth(),test.naturalSize().getHeight());
            for(size_t i=0;i<test.knobs.size();++i){if(!test.knobs[i]->isVisible())continue;const auto bounds=test.knobs[i]->getBounds();
                require(bounds.getY()>=test.dividerY&&bounds.getBottom()<=test.getHeight()-ModulePanel::footerHeight&&bounds.getX()>=ModulePanel::pad-1&&bounds.getRight()<=test.getWidth()-ModulePanel::pad+1,"Module control bounds");
                if(bounds.getWidth()<test.knobs[i]->textWidth())throw std::runtime_error("Module control text fits: "+spec.type+" / "+test.knobs[i]->label.getText().toStdString()+" width "+std::to_string(bounds.getWidth())+" < "+std::to_string(test.knobs[i]->textWidth())+" at knob "+std::to_string(density));
                for(size_t j=i+1;j<test.knobs.size();++j)if(test.knobs[j]->isVisible())require(!bounds.intersects(test.knobs[j]->getBounds()),"Module controls overlap");
            }
            for(const auto& jack:test.jacks)require(test.getLocalBounds().contains(jack->getBounds())&&jack->getBottom()<=test.dividerY,"Module sockets inside the card header");
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
            if(model==0){
                auto* full=sequence->openPerformanceEditor(false);require(full!=nullptr,"Large Pattern editor");full->selectStep(2);full->pitchControl().setValue(79,sendNotificationSync);
                require(handles.at("sequence")->values[patternIndex(2,Pitch)]==79,"Large editor pitch reaches DSP");full->undo();require(handles.at("sequence")->values[patternIndex(2,Pitch)]==72,"Undo restores pitch");full->redo();require(handles.at("sequence")->values[patternIndex(2,Pitch)]==79,"Redo restores pitch");full->undo();
                full->copy();full->selectStep(18);full->paste();require(handles.at("sequence")->values[patternIndex(18,Pitch)]==72,"Clipboard reaches later pages");full->undo();
                setParameter("sequence","step1_lock",1);const float protectedPitch=handles.at("sequence")->values[patternIndex(0,Pitch)].load();full->generate(0);require(handles.at("sequence")->values[patternIndex(0,Pitch)]==protectedPitch,"Generator protects locked steps");full->undo();
                full->duplicate();require(handles.at("sequence")->values[PatLength]==32,"Pattern duplication extends loop");full->undo();require(handles.at("sequence")->values[PatLength]==16,"Undo restores loop length");
                require((handles.at("sequence")->editRevision.load()&1)==0,"Editor transactions complete");
            }
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
        for(int preset=11;preset<=12;++preset){
            require(commit(starterDocument(preset,.25f),"sequence"),"Motion starter queued");drain();require(document.graph.modulations.size()==2,"Both CV lanes assigned");
            auto* full=hardware.find("sequence")->sequence->openPerformanceEditor(false);require(full!=nullptr,"Motion editor ready");
            auto renderMotion=[](const RackDocument& d){Runtime r(d.graph,{},d.spectral);r.clock.tempo(d.tempo);r.clock.play(true);std::vector<float> x(96000),y(96000);r.render(x.data(),y.data(),96000,{});x.insert(x.end(),y.begin(),y.end());return x;};
            auto saved=rackJSON(document);auto restored=readRack(saved);prepareSamples(restored);const auto sound=renderMotion(document);require(sound==renderMotion(restored),"Motion state and stereo audio recall");require(std::any_of(sound.begin(),sound.end(),[](float x){return std::abs(x)>.001f;}),"Motion starter audible offline");
            require(directory.getChildFile("motion-"+String(preset)+".optisynth").replaceWithText(JSON::toString(saved)),"Save Motion preset");
            player.playing=true;player.restart();std::array<float,480> audioBlock;for(int i=0;i<90;++i){player.collect();player.render(audioBlock.data(),480);}updateTransport();
            auto captureFull=[&](const String& name){auto image=full->createComponentSnapshot(full->getLocalBounds());FileOutputStream stream(directory.getChildFile(name));require(stream.openedOk(),"Motion screenshot file");stream.setPosition(0);stream.truncate();require(PNGImageFormat().writeImageToStream(image,stream),"Motion screenshot");};
            captureFull("motion-"+String(preset)+".png");full->selectStep(31);captureFull("motion-page-2-"+String(preset)+".png");full->setSize(1120,800);captureFull("motion-minimum-"+String(preset)+".png");
            player.playing=false;updateTransport();player.render(audioBlock.data(),480);
        }
        require(directory.getChildFile("motion-check.txt").replaceWithText("PASS: large editor edits; undo/redo; page clipboard; generator locks; loop duplication; completed atomic transactions; both CV routes; two stereo FX starters with identical state/audio recall; six native captures. No audio device opened.\n"),"Write Motion receipt");
        require(directory.getChildFile("sequencer-check.txt").replaceWithText("PASS: three contextual native editors; selected-step rack/focus edits reach DSP; full sequence and tempo recall with identical audio; v3 compatibility; invalid tempo rejection; Player clock and stop; seven sequencer captures. No audio device opened.\n"),"Write sequencer receipt");
        // Pad starters (requests 57-58): voice group, analog modules and FX; offline chord renders.
        for(int pad=0;pad<4;++pad){
            require(commit(starterDocument(pad<2?15+pad:17+pad,.25f),"filter"),"Pad starter loads");drain();
            require(hardware.find("voices")&&hardware.find("filter")&&hardware.find("reverb"),"Pad modules shown in the rack");
            const String name=String(std::array<const char*,4>{"pad-ob-swell","pad-prophet","pad-xpander","pad-harmonic-drone"}[size_t(pad)]);
            capture(name+".png");
            auto renderPad=[](const RackDocument& d){
                Runtime r(d.graph);
                const int frames=48000*12,release=48000*8;
                std::vector<float> l(static_cast<size_t>(frames)),rr(static_cast<size_t>(frames));
                NoteEventBlock on,off;on.begin(quantum);off.begin(quantum);
                for(int k:{48,55,60,64,67}){on.push({0,1,uint64_t(k)+1,NoteEventKind::On,k,.7f});off.push({0,1,uint64_t(k)+1,NoteEventKind::Off,k,0});}
                for(int i=0;i<frames;i+=quantum)r.render(l.data()+i,rr.data()+i,quantum,Note{},nullptr,i==0?&on:i==release?&off:nullptr);
                l.insert(l.end(),rr.begin(),rr.end());return l;};
            const auto sound=renderPad(document);
            require(sound==renderPad(readRack(rackJSON(document))),"Pad state and stereo audio recall");
            float peak=0;for(float x:sound)peak=std::max(peak,std::abs(x));
            std::fprintf(stderr,"Pad %d raw peak %.3f, at app gain 0.25: %.3f\n",pad,peak,peak*.25f);
            require(peak>.01f&&peak*document.gain<.95f,"Pad chord audible without clipping at the app output gain");
            const int frames=int(sound.size()/2);
            AudioBuffer<float> buffer(2,frames);
            for(int i=0;i<frames;++i){buffer.setSample(0,i,sound[size_t(i)]);buffer.setSample(1,i,sound[size_t(frames+i)]);}
            const auto wav=directory.getChildFile(name+".wav");wav.deleteFile();
            auto* stream=wav.createOutputStream().release();WavAudioFormat format;
            std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream,48000,2,24,{},0));
            if(!writer)delete stream;
            require(writer&&writer->writeFromAudioSampleBuffer(buffer,0,frames),"Write pad WAV");
        }
        require(directory.getChildFile("pad-check.txt").replaceWithText("PASS: all four pad starters (OB, Prophet poly-mod, Xpander unison/glide, harmonic drone) load and display; five-note chord held 8 s then released; identical stereo audio after JSON recall; audible without clipping; WAVs written. No audio device opened.\n"),"Write pad receipt");
        // Request 71: preset catalogue and browser, synth-panel view, rack zoom.
        setSize(1440,940);resized();
        require(prefs.file().isAChildOf(directory)&&browser.libraryFolder().isAChildOf(directory),"Snapshot settings and library stay inside the snapshot folder");
        browser.setKeepSequence(false);prefs.set("viewPolicy",String("patch"));
        {std::set<int> starters;
            for(const auto& f:factoryPresets()){starters.insert(f.starter);
                require(std::find(presetCategories().begin(),presetCategories().end(),f.info.category)!=presetCategories().end()&&!f.info.tags.empty()&&!f.info.description.empty(),"Factory metadata complete");
                for(const auto& t:f.info.tags)require(normaliseTag(t)==t,"Factory tags normalised");}
            require(factoryPresets().size()==21&&starters.size()==21&&*starters.begin()==0&&*starters.rbegin()==20,"Factory catalogue lists every starter once");}
        for(int pad:{15,16,19,20}){const auto d=factoryDocument(pad,.25f);require(!d.panel.sections.empty()&&d.view=="panel","Pad has a curated panel and opens in it");
            for(const auto& section:d.panel.sections)for(const auto& c:section.controls)require(panelControlValid(d,c),"Curated pad control resolves to a real parameter");}
        require(render(factoryDocument(3,.25f).graph)==render(starterDocument(3,.25f).graph),"Catalogue load keeps starter audio");
        require(parseTags(" Warm, Berlin School ,poly_mod,warm,, ")==std::vector<std::string>{"warm","berlin-school","poly-mod"},"Tag normalisation and de-duplication");
        auto filterCount=[&](PresetFilter f){browser.filter=std::move(f);browser.applyFilter();return browser.visibleCount();};
        {PresetFilter f;f.category="Pad";require(filterCount(f)==3,"Pad category");}
        {PresetFilter f;f.tags={"berlin-school"};require(filterCount(f)==3,"Tag filter");}
        {PresetFilter f;f.tags={"analog","ambient"};require(filterCount(f)==2,"Tags combine with AND");}
        {PresetFilter f;f.search="prophet";require(filterCount(f)==1&&browser.visibleEntry(0).starter==16,"Search by name");}
        {PresetFilter f;f.search="EUCLIDEAN sequence";require(filterCount(f)==2,"Search words across tags and category");}
        browser.toggleFavourite("factory:16");browser.toggleFavourite("factory:20");
        {PresetFilter f;f.collection="favourites";require(filterCount(f)==2,"Favourites collection");}
        {Preferences reread(prefs.file());require(reread.list("favourites").size()==2,"Favourites persist in the settings file");}
        {const auto* e=browser.step("factory:16",1);require(e&&e->key=="factory:20","Next follows the filtered list");
         e=browser.step("factory:20",1);require(e&&e->key=="factory:16","Next wraps");e=browser.step("factory:16",-1);require(e&&e->key=="factory:20","Previous wraps");}
        presetKey.clear();stepPreset(1);
        require(presetKey=="factory:16"&&currentView=="panel"&&synthPanel.isVisible()&&!rackView.isVisible(),"Header next loads the first favourite in its saved panel view");
        require(synthPanel.isCurated()&&synthPanel.layoutSound(),"Prophet panel layout");
        auto* cutoff=synthPanel.control("filter","cutoff_hz");require(cutoff!=nullptr,"Panel shows the filter cutoff");
        const auto& filterParameters=panel("analog.filter").parameters;
        const size_t cutoffIndex=size_t(std::find_if(filterParameters.begin(),filterParameters.end(),[](const Parameter& x){return x.id=="cutoff_hz";})-filterParameters.begin());
        cutoff->slider.setValue(1500,sendNotificationSync);
        require(handles.at("filter")->values[cutoffIndex].load()==1500&&std::abs(hardware.find("filter")->knobs[cutoffIndex]->slider.getValue()-1500)<1e-3,"Panel edit reaches the DSP handle and the rack card");
        hardware.find("filter")->knobs[cutoffIndex]->slider.setValue(900,sendNotificationSync);
        require(std::abs(cutoff->slider.getValue()-900)<1e-3,"Rack card edit moves the panel knob");
        auto valueOf=[&](const std::string& node,const std::string& key){for(const auto& n:document.graph.nodes)if(n.id==node)for(const auto& v:n.parameters)if(v.first==key)return v.second;return -1.f;};
        require(valueOf("filter","cutoff_hz")==900&&presetDirty,"Panel edits are saved in the patch and mark it modified");
        for(int pad:{15,16,19,20}){
            require(loadFactory(pad),"Pad preset loads");drain();
            require(currentView=="panel"&&synthPanel.isCurated()&&synthPanel.layoutSound()&&!presetDirty,"Pad opens in a sound panel layout");
            capture(String(pad==15?"panel-ob-swell":pad==16?"panel-prophet":pad==19?"panel-xpander":"panel-drone")+".png");
        }
        setSize(1120,800);resized();require(synthPanel.layoutSound(),"Panel layout at minimum window size");capture("panel-minimum.png");setSize(1440,940);resized();
        require(commit(RackDocument{rackPreset(3),.25f,{}},"source"),"Two-oscillator rack");setView("panel",true);
        require(!synthPanel.isCurated()&&synthPanel.sectionCount()==5&&synthPanel.layoutSound()&&synthPanel.control("mixer","gain_b")!=nullptr,"Automatic panel: one section per module with parameters");
        capture("panel-automatic.png");
        require(document.view=="panel"&&readRack(rackJSON(document)).view=="panel","Manual view switch is saved with the patch");
        require(commit(starterDocument(8,.25f),"sequence"),"Pattern rack");require(synthPanel.launcherCount()==1,"Sequencer appears as an editor launcher");
        prefs.set("viewPolicy",String("modular"));require(loadFactory(15),"OB swell");require(currentView=="modular","Always-modular preference overrides the patch view");
        prefs.set("viewPolicy",String("panel"));require(loadFactory(0),"VA starter");require(currentView=="panel","Always-panel preference overrides the patch view");
        prefs.set("viewPolicy",String("patch"));
        // Library: save, overwrite guard, in-place metadata edit, older files, unreadable files.
        auto padAudio=[](const RackDocument& d){Runtime r(d.graph);std::vector<float> l(48000),rr(48000);NoteEventBlock on;on.begin(quantum);on.push({0,1,61,NoteEventKind::On,60,.7f});
            for(int i=0;i<48000;i+=quantum)r.render(l.data()+i,rr.data()+i,quantum,Note{},nullptr,i==0?&on:nullptr);l.insert(l.end(),rr.begin(),rr.end());return l;};
        require(loadFactory(16),"Prophet for saving");showSaveForm();
        browser.setFormFields("My Prophet sweep","Pad"," Warm, Berlin School, poly_mod","Saved by the native check");capture("browser-save.png");
        require(browser.saveFromForm(false).empty()&&!browser.isSaving(),"Save to library");
        const auto savedFile=browser.userFolder().getChildFile("My Prophet sweep.optisynth");
        require(savedFile.existsAsFile()&&presetKey=="user:"+savedFile.getFullPathName().toStdString()&&!presetDirty,"Library file written and becomes the current preset");
        const auto savedDocument=readRack(JSON::parse(savedFile));
        require(savedDocument.info.tags==std::vector<std::string>{"warm","berlin-school","poly-mod"}&&savedDocument.view=="panel"&&!savedDocument.panel.sections.empty()&&savedDocument.info.category=="Pad","Saved metadata, view and curated panel");
        require(padAudio(savedDocument)==padAudio(document),"Saved preset sounds the same");
        require(saveToLibrary(document.info,true,false)=="exists","An existing name needs confirmation before replacing");
        browser.filter=PresetFilter{};browser.applyFilter();browser.selectKey(presetKey);
        browser.setFormFields("My Prophet sweep","Texture","dark, slow","Edited by the native check");
        require(browser.saveFromForm(false).empty(),"Edit user preset metadata");
        const auto edited=readRack(JSON::parse(savedFile));
        require(edited.info.category=="Texture"&&edited.info.tags==std::vector<std::string>{"dark","slow"}&&padAudio(edited)==padAudio(savedDocument)&&edited.panel.sections.size()==savedDocument.panel.sections.size(),"Edited metadata only; same sound and panel");
        require(document.info.category=="Texture"&&presetDisplay.detail.startsWith("Texture"),"Editing the loaded preset updates the header");
        auto older=rackJSON({rackPreset(0),.25f,{}});for(const char* k:{"info","view","panel"})older.getDynamicObject()->removeProperty(k);older.getDynamicObject()->setProperty("version",5);
        require(browser.libraryFolder().getChildFile("Old rack.optisynth").replaceWithText(JSON::toString(older))&&browser.libraryFolder().getChildFile("Broken.optisynth").replaceWithText("{not json"),"Library fixtures");
        browser.rescan();{PresetFilter f;f.collection="user";require(filterCount(f)==2,"User collection lists readable files only");}
        {const auto* old=browser.entry("user:"+browser.libraryFolder().getChildFile("Old rack.optisynth").getFullPathName().toStdString());
         require(old&&old->info.name=="Old rack"&&old->category()=="Uncategorised","Version 5 file listed by file name");
         updatePresetInfo(old->file,{"Old rack","Template","",{"legacy"}});const auto upgraded=readRack(JSON::parse(browser.libraryFolder().getChildFile("Old rack.optisynth")));
         require(upgraded.info.tags==std::vector<std::string>{"legacy"}&&render(upgraded.graph)==render(rackPreset(0)),"Older file upgraded with its sound unchanged");}
        browser.rescan();browser.filter=PresetFilter{};browser.applyFilter();browser.selectKey(presetKey);capture("browser.png");
        setSize(1120,800);resized();capture("browser-minimum.png");setSize(1440,940);resized();showBrowser(false);
        // Format: version 6 round trip, version 5 compatibility, strict panel references, pruning.
        {const auto again=readRack(rackJSON(document));
         require(again.info.name==document.info.name&&again.info.tags==document.info.tags&&again.view==document.view&&again.panel.sections.size()==document.panel.sections.size()&&again.panel.style==document.panel.style,"Version 6 round trip");
         auto v5=rackJSON(document);v5.getDynamicObject()->setProperty("version",5);const auto five=readRack(v5);require(five.info.name.empty()&&five.panel.sections.empty(),"Version 5 ignores version 6 fields");
         auto bad=rackJSON(document);auto* sections=bad.getDynamicObject()->getProperty("panel").getDynamicObject()->getProperty("sections").getArray();
         (*(*sections)[0].getDynamicObject()->getProperty("controls").getArray())[0].getDynamicObject()->setProperty("parameter","no_such_parameter");
         bool rejected=false;try{readRack(bad);}catch(const std::exception&){rejected=true;}require(rejected,"Panel control with a missing parameter rejected on read");}
        require(replaceModule("vco2","osc.sine"),"Replace oscillator B");
        {bool stale=false,sound=true;for(const auto& section:document.panel.sections)for(const auto& c:section.controls){stale|=c.node=="vco2"&&c.parameter=="saw";sound&=panelControlValid(document,c);}
         require(!stale&&sound&&synthPanel.control("vco2","transpose")!=nullptr,"Replacing a module prunes stale panel controls and keeps shared ones");}
        // Request 72: density (not scaling), rows, arrows, keyboard with a kept sequencer, user presets.
        require(loadFactory(15),"OB swell for density");setView("modular",true);setDensity(44);
        auto fontsFixed=[&]{for(const auto& m:hardware.modules)for(const auto& k:m->knobs)if(k->isVisible()&&(std::abs(k->label.getFont().getHeight()-text::label)>.01f))return false;return true;};
        auto equalRowHeights=[&]{for(const auto& a:hardware.modules)for(const auto& b:hardware.modules)if(a->getY()==b->getY()&&a->getHeight()!=b->getHeight())return false;return true;};
        require(hardware.getTransform().isIdentity()&&fontsFixed()&&equalRowHeights(),"Normal density: no transform, fixed text, cards in a row share its height");
        const auto normalHeight=hardware.getHeight();const int normalVco=hardware.find("vco1")->getWidth();
        capture("rack-normal.png");
        const int normalVcoHeight=hardware.find("vco1")->getHeight();
        // Text sets the column widths, so compact cards get shorter (smaller knobs, tighter rows), not narrower.
        setDensity(32);require(hardware.find("vco1")->getWidth()<=normalVco&&hardware.find("vco1")->getHeight()<normalVcoHeight&&hardware.getHeight()<normalHeight&&fontsFixed(),"Compact: smaller knobs and spacing, same text size");
        capture("rack-compact.png");
        // Fit: the largest density that shows everything; when nothing does (12 modules, readable text), the most compact.
        fitDensity();require(hardware.knob==32,"Fit falls back to Compact when the whole rack cannot be shown");
        {const auto saved=document;require(commit(RackDocument{rackPreset(3),.25f,{}},"source"),"Six-module rack for Fit");fitDensity();
         require(hardware.getWidth()<=rackView.getWidth()&&hardware.getHeight()<=rackView.getHeight()-rackView.getScrollBarThickness()&&hardware.knob>32,"Fit shows a six-module rack whole at a roomier density");
         capture("rack-fit.png");require(commit(saved,"filter"),"Back to the OB rack");}
        setDensity(60);capture("rack-large.png");
        selectNode("reverb");require(rackView.getViewArea().intersects(hardware.find("reverb")->getBounds()),"Selected module scrolled into view");
        {Preferences reread(prefs.file());require(int(reread.number("rackKnob",0))==60,"Density saved as a preference");}
        hardware.magnified(1.3f);require(hardware.knob==60,"Pinch past the largest size stays there");hardware.magnified(.7f);require(hardware.knob==52,"Pinch in steps density down");
        setSize(1120,800);resized();fitDensity();capture("rack-fit-minimum.png");setSize(1440,940);resized();setDensity(44);
        // Rows chosen by hand, saved with the patch.
        editRows("filter",1);
        require(document.rows.size()>=2&&document.rows[1].front()=="filter"&&hardware.find("filter")->getX()<=HardwareRack::margin+HardwareRack::gap,"Start a new row with the filter");
        editRows("vca",1);editRows("vco1",4);
        require(readRack(rackJSON(document)).rows==document.rows,"Rows saved in the patch");
        require(hardware.find("vco1")->getX()>hardware.find("vco2")->getX(),"Move right reorders within a row");
        capture("rack-rows.png");
        {auto removed=document;removed.graph.modulations.erase(std::remove_if(removed.graph.modulations.begin(),removed.graph.modulations.end(),[](const Modulation& m){return m.source.node=="breath";}),removed.graph.modulations.end());
         removed.graph.nodes.erase(std::remove_if(removed.graph.nodes.begin(),removed.graph.nodes.end(),[](const NodeSpec& n){return n.id=="breath";}),removed.graph.nodes.end());
         require(commit(removed,"vco1"),"Remove a module from hand-made rows");
         bool gone=true;for(const auto& row:document.rows)for(const auto& id:row)gone&=id!="breath";require(gone,"Removed modules leave the rows");}
        editRows("vco1",5);require(document.rows.empty(),"Automatic rows again");
        // Header arrows, arrow keys, browser keys.
        loadFactory(17);drain();
        nextPreset.onClick();drain();require(presetKey=="factory:18","Header next arrow loads the next preset");
        prevPreset.onClick();drain();require(presetKey=="factory:17","Header previous arrow");
        keyPressed(KeyPress(KeyPress::rightKey));drain();require(presetKey=="factory:18","Right arrow key: next preset");
        keyPressed(KeyPress(KeyPress::leftKey));drain();require(presetKey=="factory:17","Left arrow key: previous preset");
        {PresetFilter f;f.collection="favourites";browser.filter=f;browser.applyFilter();}
        nextPreset.onClick();drain();require(presetKey!="factory:17","Arrows still step when the current preset is outside the browser's filter");
        browser.filter=PresetFilter{};browser.applyFilter();
        showBrowser(true);browser.selectKey("factory:0");browser.keyPressed(KeyPress(KeyPress::downKey));
        require(browser.visibleEntry(1).key=="factory:1"&&browser.keyPressed(KeyPress(KeyPress::returnKey))&&presetKey=="factory:1","Browser: down arrow then Return loads the next row");
        showBrowser(false);
        // User presets in Presets/User are found (request 73).
        require(browser.userFolder().getChildFile("Lush test.optisynth").replaceWithText(JSON::toString(rackJSON(factoryDocument(15,.25f)))),"User preset fixture");
        browser.rescan();require(browser.entry("user:"+browser.userFolder().getChildFile("Lush test.optisynth").getFullPathName().toStdString())!=nullptr,"Presets/User is listed");
        // Keyboard plays a pad that a kept sequencer drives (the reported silence).
        browser.setKeepSequence(true);loadFactory(17);drain();require(loadFactory(15),"OB swell keeps the sequencer");drain();
        {bool driven=false;for(const auto& c:document.graph.connections)driven|=c.destination.node=="voices"&&c.destination.port=="notes";require(driven,"Sequencer drives the pad's voices");
         player.playing=false;std::array<float,480> block{};for(int i=0;i<4;++i){player.collect();player.render(block.data(),480);}
         keys.noteOn(1,60,.8f);float peak=0;for(int i=0;i<300;++i){player.collect();player.render(block.data(),480);for(float x:block)peak=std::max(peak,std::abs(x));}
         keys.noteOff(1,60,0);require(peak>.001f,"Keyboard plays the pad while a kept sequencer is stopped");}
        require(synthPanel.launcherCount()==1,"The kept sequencer appears on the curated panel");
        browser.setKeepSequence(false);
        require(directory.getChildFile("panel-browser-check.txt").replaceWithText("PASS: 21-entry factory catalogue with categories/tags; curated pad panels resolve; category, tag (AND), word search and favourites filters; favourites persist; previous/next wrap through the filtered list; pad presets open in their saved panel view; panel <-> rack card <-> DSP handle synchronisation; automatic panel and sequencer launcher; view preference overrides; save to library, overwrite guard, in-place metadata edit with identical audio, version 5 listing/upgrade, unreadable files skipped; version 6 round trip, version 5 compatibility, strict panel references, pruning on module replacement; density (fixed text, equal row heights, compact/fit/large), hand-made rows saved and pruned, scroll-into-view, pinch steps, saved density; header arrows, arrow keys, browser down/Return, arrows outside the filter; Presets/User listed; keyboard plays a sequencer-driven pad; native captures. Settings and library inside the snapshot folder. No audio device opened.\n"),"Write panel/browser receipt");
        require(directory.getChildFile("ui-check.txt").replaceWithText("PASS: independent controls; rack/focus synchronization; JSON/audio recall; invalid-edit preservation; dormant add/remove; typed routing and cycle rejection; contextual replacement; 26 panel layouts; legacy rack import; background sample and Spectral loading/recall/cancellation; contextual sequencer editing and transport. No audio device opened.\n"),"Write UI receipt");
    }
private:
    struct InputRow { Label label;ComboBox choice;std::vector<Endpoint> ports; };
    Preferences prefs;PresetBrowser browser;
    Player player;OutputConverter converter;RackDocument document{rackPreset(0),.25f};
    SampleLoader sampleLoader;bool loading=false;
    std::shared_ptr<RouteControls> routeHandles;
    std::unique_ptr<MatrixWindow> matrixWindow;
    TextButton matrixButton{"Matrix"};
    Registry registry=moduleRegistry();
    MidiKeyboardState keys;MidiKeyboardComponent keyboard;TypingKeyboard typing;
    ListBox rack;ModulePanel editor{"all"};Values panelValues;
    HardwareRack hardware;Viewport rackView;SynthPanelView synthPanel;TextButton focusMode{"Focus module"};TooltipWindow tips{this,700};
    std::map<std::string,std::shared_ptr<Controls>> handles;
    std::vector<std::unique_ptr<InputRow>> inputs;
    std::vector<std::string> addTypes;std::vector<Endpoint> outputPorts;
    std::vector<int> held;std::set<int> sustained;bool sustain=false;std::string selected;int activeCount=0;
    ComboBox addChoice,output;
    TextButton add{"Add module"},remove{"Remove"},audio{"Start audio"},silence{"Silence"};
    PresetDisplay presetDisplay;
    TextButton prevPreset{String::fromUTF8("\xe2\x80\xb9")},nextPreset{String::fromUTF8("\xe2\x80\xba")},favouriteButton{starOff},savePreset{"Save"};
    TextButton panelViewButton{"Panel"},modularViewButton{"Modular"},viewMenu{String::fromUTF8("\xe2\x96\xbe")};
    TextButton zoomOut{String::fromUTF8("\xe2\x88\x92")},zoomLabel{"Normal"},zoomIn{"+"},zoomFit{"Fit"};
    std::string currentView="modular",presetKey,pendingPreset;bool presetDirty=false;
    Slider gain;TextEditor routes;Label status,keyboardHelp;
    TextButton play{"Play seq"},restart{"Restart"};Slider tempo;Label position;
    std::unique_ptr<FileChooser> chooser;
    bool refreshing=false,running=false,openPatternWhenReady=false,openPerformanceWhenReady=false,openReferenceWhenReady=false;
    int bodyBottom() const {return getHeight()-185;}
    int bodyTop() const {return currentView=="panel"?106:152;}
    // Request 71: presets, views and zoom.
    void showBrowser(bool show){
        if(show){browser.rescan();browser.setCurrent(presetKey);}
        browser.setVisible(show);presetDisplay.setToggleState(show,dontSendNotification);
        if(show){browser.toFront(false);browser.focusList();}else grabKeyboardFocus();
    }
    void showSaveForm(){showBrowser(true);browser.showSaveForm(document.info,currentView=="panel");}
    bool loadFactory(int index){
        try{auto next=chooseStarter(document,index,browser.keepCurrentSequence());
            return commit(std::move(next),index==15||index==16||index==19||index==20?"filter":index>=8?"sequence":"source","factory:"+std::to_string(index));
        }catch(const std::exception& e){message(e.what(),true);return false;}
    }
    bool loadUserFile(const File& file){
        try{if(file.getSize()>2*1024*1024)throw std::runtime_error("Patch file too large");
            auto next=readRack(JSON::parse(file));if(next.info.name.empty())next.info.name=file.getFileNameWithoutExtension().toStdString();
            if(!commit(std::move(next),"","user:"+file.getFullPathName().toStdString()))return false;
            if(!loading)message("Opened "+file.getFileName());return true;
        }catch(const std::exception& e){message(String(file.getFileName())+": "+e.what(),true);return false;}
    }
    void loadPreset(const PresetEntry& e){if(e.factory())loadFactory(e.starter);else loadUserFile(e.file);}
    void stepPreset(int delta){if(const auto* e=browser.step(presetKey,delta)){const auto copy=*e;loadPreset(copy);}}
    std::string saveToLibrary(const PresetInfo& info,bool panelView,bool overwrite){
        const auto file=presetFileFor(browser.userFolder(),info.name);
        if(file.exists()&&!overwrite)return "exists";
        try{auto saved=document;saved.info=info;saved.view=panelView?"panel":"modular";writePresetFile(file,rackJSON(saved));
            document.info=info;document.view=saved.view;presetKey="user:"+file.getFullPathName().toStdString();presetDirty=false;
            browser.rescan();synthPanel.build(document);updatePresetDisplay();
            message("Saved "+file.getFileName()+" to the preset library"+(document.samples.empty()?"":" - WAV files remain external"));return {};
        }catch(const std::exception& e){return e.what();}
    }
    // byHand: the user chose the view, so the patch remembers it for the next save.
    void setView(const std::string& view,bool byHand){
        currentView=view=="panel"?"panel":"modular";if(byHand)document.view=currentView;
        panelViewButton.setToggleState(currentView=="panel",dontSendNotification);modularViewButton.setToggleState(currentView!="panel",dontSendNotification);
        if(status.getText().startsWith("Ready"))message(currentView=="panel"?"Ready - play the keyboard and shape the sound on the panel":"Ready - select a module to edit");
        resized();repaint();
    }
    void applyViewPolicy(){
        const auto policy=prefs.text("viewPolicy","patch");
        setView(policy=="panel"||policy=="modular"?policy.toStdString():document.view,false);
    }
    void showViewMenu(){
        PopupMenu menu;const auto policy=prefs.text("viewPolicy","patch");menu.addSectionHeader("When a patch loads, open");
        menu.addItem(1,"The view saved with the patch",true,policy=="patch");menu.addItem(2,"Always the modular view",true,policy=="modular");menu.addItem(3,"Always the synth panel",true,policy=="panel");
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&viewMenu),[safe=SafePointer<RackSynth>(this)](int result){
            if(!safe||result==0)return;safe->prefs.set("viewPolicy",String(result==1?"patch":result==2?"modular":"panel"));safe->applyViewPolicy();});
    }
    void updatePresetDisplay(){
        presetDisplay.title=document.info.name.empty()?"Untitled patch":String(document.info.name);presetDisplay.modified=presetDirty;
        String detail=document.info.category.empty()?String("No category"):String(document.info.category);
        for(size_t i=0;i<document.info.tags.size()&&i<4;++i)detail+="  /  "+String(document.info.tags[i]);
        presetDisplay.detail=detail;presetDisplay.repaint();
        const bool favourite=browser.isFavourite(presetKey);favouriteButton.setButtonText(favourite?starOn:starOff);
        favouriteButton.setColour(TextButton::textColourOffId,favourite?orange:paper);favouriteButton.setEnabled(!presetKey.empty());
        browser.setCurrent(presetKey);
    }
    // Request 72: rack density. Knob size and spacing change; text sizes never do.
    static constexpr std::array<int,5> densityKnobs{32,38,44,52,60};
    static String densityName(int knob){return knob<=32?"Compact":knob<=38?"Snug":knob<=44?"Normal":knob<=52?"Roomy":"Large";}
    int stepDensity(int direction) const {
        if(direction>0){for(int k:densityKnobs)if(k>hardware.knob)return k;return densityKnobs.back();}
        for(auto it=densityKnobs.rbegin();it!=densityKnobs.rend();++it)if(*it<hardware.knob)return *it;return densityKnobs.front();
    }
    void setDensity(int knob){hardware.knob=std::clamp(knob,densityKnobs.front(),densityKnobs.back());prefs.set("rackKnob",hardware.knob);arrangeRack();}
    // Largest density at which the whole rack is visible without scrolling (else the most compact).
    void fitDensity(){
        const int w=rackView.getWidth()-rackView.getScrollBarThickness(),h=rackView.getHeight()-rackView.getScrollBarThickness();int chosen=densityKnobs.front();
        for(auto it=densityKnobs.rbegin();it!=densityKnobs.rend();++it){hardware.knob=*it;const auto size=hardware.arrange(w);if(size.getWidth()<=w&&size.getHeight()<=h){chosen=*it;break;}}
        setDensity(chosen);
    }
    void arrangeRack(){
        hardware.rows=document.rows;hardware.arrange(rackView.getWidth()-rackView.getScrollBarThickness());
        zoomLabel.setButtonText(densityName(hardware.knob));
    }
    // Rows: start a new row here, join the row above, move left / right, or wrap automatically.
    void showRowMenu(const std::string& id){
        auto rows=document.rows.empty()?hardware.shownRows():document.rows;
        size_t r=0,c=0;bool found=false;for(size_t i=0;i<rows.size()&&!found;++i)for(size_t j=0;j<rows[i].size();++j)if(rows[i][j]==id){r=i;c=j;found=true;break;}
        if(!found)return;
        PopupMenu menu;menu.addSectionHeader(String(id)+"  /  rack rows");
        menu.addItem(1,"Start a new row with this module",c>0);
        menu.addItem(2,"Join the row above",c==0&&r>0);
        menu.addItem(3,"Move left",c>0);menu.addItem(4,"Move right",c+1<rows[r].size());
        menu.addSeparator();menu.addItem(5,"Automatic rows (wrap to the window)",true,document.rows.empty());
        menu.showMenuAsync(PopupMenu::Options(),[safe=SafePointer<RackSynth>(this),id](int result){if(safe&&result>0)safe->editRows(id,result);});
    }
    void editRows(const std::string& id,int action){
        auto rows=document.rows.empty()?hardware.shownRows():document.rows;
        for(size_t r=0;r<rows.size();++r)for(size_t c=0;c<rows[r].size();++c)if(rows[r][c]==id){
            auto& row=rows[r];
            if(action==1&&c>0){std::vector<std::string> tail(row.begin()+long(c),row.end());row.erase(row.begin()+long(c),row.end());rows.insert(rows.begin()+long(r)+1,tail);}
            else if(action==2&&c==0&&r>0){rows[r-1].insert(rows[r-1].end(),row.begin(),row.end());rows.erase(rows.begin()+long(r));}
            else if(action==3&&c>0)std::swap(row[c],row[c-1]);
            else if(action==4&&c+1<row.size())std::swap(row[c],row[c+1]);
            else if(action==5)rows.clear();
            document.rows=rows;if(!presetDirty){presetDirty=true;updatePresetDisplay();}
            arrangeRack();if(auto* p=hardware.find(id))rackView.setViewPosition(rackView.getViewPositionX(),std::max(0,p->getY()-8));
            return;
        }
    }
    const ModuleType& type(const std::string& id) const {for(const auto& t:registry)if(t.id==id)return t;throw std::invalid_argument("Unknown type");}
    void message(const String& text,bool error=false){status.setColour(Label::textColourId,error?Colour(0xffffb180):paper);status.setText(text,dontSendNotification);}
    void setParameter(const std::string& id,const std::string& key,float value) {
        cancelLoading();
        for(auto& n:document.graph.nodes)if(n.id==id){auto it=std::find_if(n.parameters.begin(),n.parameters.end(),[&](const auto& p){return p.first==key;});if(it==n.parameters.end())n.parameters.push_back({key,value});else it->second=value;}
        if(auto* p=hardware.find(id)){hardware.values[id][p->current][key]=value;p->setValue(key,value);}
        if(editor.instance==id){panelValues[editor.current][key]=value;editor.setValue(key,value);}
        synthPanel.setValue(id,key,value);
        if(!presetDirty){presetDirty=true;updatePresetDisplay();}
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
    void bindSequencePanel(ModulePanel& panel){
        const auto playAction=[this]{if(!running)toggleAudio();if(running){player.playing.store(!player.playing.load());updateTransport();}};
        if(panel.referenceSequence){auto* view=panel.referenceSequence.get();view->getTempo=[this]{return double(document.tempo);};view->changeTempo=[this](double value){tempo.setValue(value,sendNotificationSync);};view->onPlay=playAction;}
        if(panel.sequence){panel.sequence->getTempo=[this]{return double(document.tempo);};panel.sequence->changeTempo=[this](double value){tempo.setValue(value,sendNotificationSync);};panel.sequence->onPlay=playAction;panel.sequence->onRestart=[this]{player.restart();};}
        if(panel.performance){const auto id=panel.instance;auto* view=panel.performance.get();
            for(const auto& n:document.graph.nodes)if(n.id==id)view->setData(n.performance?*n.performance:defaultPerformance());
            view->getTempo=[this]{return double(document.tempo);};view->changeTempo=[this](double value){tempo.setValue(value,sendNotificationSync);};view->onPlay=playAction;view->onRestart=[this]{player.restart();};
            view->dataChanged=[this,id](const PerformanceData& data){cancelLoading();validatePerformance(data);auto saved=std::make_shared<const PerformanceData>(data);
                for(auto& n:document.graph.nodes)if(n.id==id)n.performance=saved;
                if(auto handle=handles[id])if(handle->performance)handle->performance->publish(data);
                if(auto* p=hardware.find(id))if(p->performance)p->performance->setData(data);
                if(editor.instance==id&&editor.performance)editor.performance->setData(data);
            };
        }
    }
    void rebuildHardware() {
        hardware.clear();
        for(const auto& node:document.graph.nodes){
            auto card=std::make_unique<ModulePanel>("all");auto* p=card.get();p->instance=node.id;p->knob=hardware.knob;
            p->rowMenu=[this,id=node.id]{MessageManager::callAsync([safe=SafePointer<RackSynth>(this),id]{if(safe)safe->showRowMenu(id);});};
            auto& values=hardware.values[node.id];for(const auto& v:node.parameters)values[node.type][v.first]=v.second;
            p->show(node.type,values,handles[node.id],[this,id=node.id](const std::string& key,float value){setParameter(id,key,value);});p->setConnections(document.graph);bindSamplePanel(*p,node.id);bindSpectralPanel(*p,node.id);
            bindSequencePanel(*p);
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
        auto next=document;for(auto& n:next.graph.nodes)if(n.id==id){n.type=nextType;n.parameters.clear();if(nextType!="seq.performance")n.performance.reset();}return commit(std::move(next),id);
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
    // presetLoad: the preset key when this commit loads a preset (applies its view; clears the modified mark).
    bool commit(RackDocument next,const std::string& preferred,const std::string& presetLoad={}) {
        cancelLoading();const auto target=preferred;
        try {
            validateRack(next.graph);pruneSampleReferences(next);prunePanel(next);pruneRows(next);
            if(needsSamples(next)){sampleLoader.submit(std::move(next),target);loading=true;pendingPreset=presetLoad;message("Preparing patch... current sound stays ready");return true;}
            prepareSamples(next);auto runtime=std::make_unique<Runtime>(next.graph,sampleBank(next),next.spectral);install(std::move(next),std::move(runtime),target,presetLoad);return true;
        } catch(const std::exception& e){refresh(selected);message(String("Route unchanged: ")+e.what(),true);return false;}
    }
    void install(RackDocument next,std::unique_ptr<Runtime> runtime,const std::string& target,const std::string& presetLoad={}) {
        std::map<std::string,std::shared_ptr<Controls>> controls;for(const auto& n:next.graph.nodes)controls[n.id]=runtime->controls(n.id);
        routeHandles=runtime->routeControls();document=std::move(next);handles=std::move(controls);activeCount=int(runtime->activeModules());
        player.gain.store(document.gain);gain.setValue(document.gain,dontSendNotification);player.publish(std::move(runtime));
        player.tempo.store(document.tempo);tempo.setValue(document.tempo,dontSendNotification);
        if(!presetLoad.empty()){presetKey=presetLoad;presetDirty=false;}else presetDirty=true;
        refresh(target);if(!presetLoad.empty())applyViewPolicy();
        if(matrixWindow)matrixWindow->editor->refresh(document);
        message(currentView=="panel"?"Ready - play the keyboard and shape the sound on the panel":"Ready - select a module to edit");updatePresetDisplay();
    }
    void cancelLoading(){if(loading){sampleLoader.cancel();loading=false;pendingPreset.clear();message("Pending patch load cancelled by edit");}}
    void collectSamples(){if(auto r=sampleLoader.take()){
        loading=false;if(!r->error.empty())message(String("Patch unchanged: ")+r->error,true);
        else install(std::move(r->document),std::move(r->runtime),r->preferred,pendingPreset);
        pendingPreset.clear();
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
        std::map<std::string,std::string> openEditors;
        auto remember=[&](const ModulePanel& p){
            if(p.sequencerWindowOpen())openEditors[p.instance]=p.current;
        };
        remember(editor);
        for(const auto& card:hardware.modules)remember(*card);
        rebuildHardware();synthPanel.build(document);
        selectNode(preferred);
        for(const auto& [id,model]:openEditors)
            if(auto* card=hardware.find(id);card && card->current==model)card->openSequencerWindow();
        repaint();
    }
    void selectNode(const std::string& id) {
        int row=0;for(size_t i=0;i<document.graph.nodes.size();++i)if(document.graph.nodes[i].id==id)row=int(i);
        refreshing=true;rack.selectRow(row);refreshing=false;
        if(!document.graph.nodes.empty()){selected=document.graph.nodes[size_t(row)].id;showSelected();
            if(auto* p=hardware.find(selected)){const auto r=p->getBounds();const auto view=rackView.getViewArea();int x=view.getX(),y=view.getY();
                if(r.getX()<x||r.getWidth()>view.getWidth())x=r.getX();else if(r.getRight()>view.getRight())x=r.getRight()-view.getWidth()+8;
                if(r.getY()<y||r.getHeight()>view.getHeight())y=r.getY();else if(r.getBottom()>view.getBottom())y=r.getBottom()-view.getHeight()+8;
                rackView.setViewPosition(x,y);}}
    }
    void showSelected() {
        auto it=std::find_if(document.graph.nodes.begin(),document.graph.nodes.end(),[&](const NodeSpec& n){return n.id==selected;});
        if(it==document.graph.nodes.end())return;
        const auto node=*it;panelValues.clear();for(const auto& p:node.parameters)panelValues[node.type][p.first]=p.second;
        if(editor.sequencerWindowOpen())
            if(auto* card=hardware.find(editor.instance);card && card->current==editor.current && !card->sequencerWindowOpen())
                card->openSequencerWindow();
        editor.instance=node.id;editor.knob=60;
        editor.show(node.type,panelValues,handles[node.id],[this,id=node.id](const std::string& key,float value){setParameter(id,key,value);});bindSequencePanel(editor);
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
        chooser=std::make_unique<FileChooser>(writing?"Export Optisynth patch":"Import Optisynth patch",File::getSpecialLocation(File::userDocumentsDirectory),"*.optisynth");
        const int flags=writing?FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting:FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(flags,[safe=SafePointer<RackSynth>(this),writing](const FileChooser& c){
            if(!safe)return;auto f=c.getResult();if(f==File())return;
            try{if(writing){f=f.withFileExtension("optisynth");if(!f.replaceWithText(JSON::toString(rackJSON(safe->document))))throw std::runtime_error("Could not save file");safe->message("Saved "+f.getFileName()+(safe->document.samples.empty()?"":" - WAV files remain external"));}
                else safe->loadUserFile(f);}
            catch(const std::exception& e){safe->message(e.what(),true);}
        });
    }
    void toggleAudio() {
        if(running){stopAudio();return;}
        setAudioChannels(0,2);running=deviceManager.getCurrentAudioDevice()!=nullptr;
        // Hardware MIDI opens only with audio: every available input, notes merged to channel 1.
        for(const auto& device:MidiInput::getAvailableDevices())deviceManager.setMidiInputDeviceEnabled(device.identifier,true);
        deviceManager.removeMidiInputDeviceCallback({},this);deviceManager.addMidiInputDeviceCallback({},this);
        audio.setButtonText(running?"Stop audio":"Start audio");message(running?"Audio on - play the keyboard":"Could not open audio output",!running);grabKeyboardFocus();
    }
    void stopAudio() {player.playing.store(false);updateTransport();deviceManager.removeMidiInputDeviceCallback({},this);sustained.clear();sustain=false;typing.release([this](int n){keys.noteOff(1,n,0);});keys.allNotesOff(1);held.clear();player.note(48,false);player.releaseAll();shutdownAudio();running=false;audio.setButtonText("Start audio");message("Audio stopped");}
    void updateTransport(){
        const bool active=player.playing.load();play.setButtonText(active?"Stop seq":"Play seq");
        if(editor.sequence)editor.sequence->setTransportActive(active);
        if(editor.performance)editor.performance->setActive(active);
        if(editor.referenceSequence)editor.referenceSequence->setActive(active);
        for(auto& panel:hardware.modules)if(panel->sequence)panel->sequence->setTransportActive(active);
        for(auto& panel:hardware.modules)if(panel->performance)panel->performance->setActive(active);
        for(auto& panel:hardware.modules)if(panel->referenceSequence)panel->referenceSequence->setActive(active);
        const double beat=player.beat.load();position.setText(String(int(beat/4)+1)+" : "+String(std::fmod(beat,4)+1,1)+(running?" / DSP "+String(int(std::lround(player.load.load()*100)))+"%":" / audio off"),dontSendNotification);
    }
    void handleNoteOn(MidiKeyboardState*,int,int note,float velocity) override {held.erase(std::remove(held.begin(),held.end(),note),held.end());held.push_back(note);player.note(note,true);player.noteEvent(note,velocity,true);}
    void handleNoteOff(MidiKeyboardState*,int,int note,float) override {held.erase(std::remove(held.begin(),held.end(),note),held.end());player.note(held.empty()?note:held.back(),!held.empty());player.noteEvent(note,0,false);}
    // MIDI thread: continuous controls go straight to atomics; notes are forwarded to the
    // message thread so the on-screen keyboard, mono and voice paths stay consistent.
    void handleIncomingMidiMessage(MidiInput*,const MidiMessage& m) override {
        if(m.isPitchWheel()){player.bend.store(float(m.getPitchWheelValue()-8192)/8192.f*2.f);return;}
        if(m.isController()&&m.getControllerNumber()==1){player.wheel.store(float(m.getControllerValue())/127.f);return;}
        if(m.isNoteOnOrOff()||m.isSustainPedalOn()||m.isSustainPedalOff()||m.isAllNotesOff()||m.isAllSoundOff())
            MessageManager::callAsync([safe=SafePointer<RackSynth>(this),m]{if(safe)safe->midiNote(m);});
    }
    void midiNote(const MidiMessage& m) {
        const int n=m.getNoteNumber();
        if(m.isNoteOn()){sustained.erase(n);keys.noteOn(1,n,m.getFloatVelocity());}
        else if(m.isNoteOff()){if(sustain)sustained.insert(n);else keys.noteOff(1,n,0);}
        else if(m.isSustainPedalOn())sustain=true;
        else if(m.isSustainPedalOff()){sustain=false;for(int k:sustained)keys.noteOff(1,k,0);sustained.clear();}
        else {sustained.clear();keys.allNotesOff(1);}
    }
    void timerCallback() override {
        player.collect();collectSamples();
        if(openReferenceWhenReady&&!loading){if(auto* card=hardware.find("sequence"))if(card->referenceSequence){card->referenceSequence->openEditor();openReferenceWhenReady=false;}}
        if(openPatternWhenReady&&!loading){if(auto* card=hardware.find("sequence"))if(card->sequence){card->sequence->openPerformanceEditor();openPatternWhenReady=false;}}
        if(openPerformanceWhenReady&&!loading){if(auto* card=hardware.find("sequence"))if(card->performance){card->performance->openEditor();openPerformanceWhenReady=false;}}
        updateTransport();
        auto* focus=juce::Component::getCurrentlyFocusedComponent();
        bool textEditing=false;for(auto* c=focus;c;c=c->getParentComponent())if(dynamic_cast<TextEditor*>(c)||dynamic_cast<ComboBox*>(c))textEditing=true;
        const auto mods=ModifierKeys::getCurrentModifiersRealtime();
        const bool caps=(CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState)&kCGEventFlagMaskAlphaShift)!=0;
        bool noteFocus=hasKeyboardFocus(true)||(editor.sequence&&editor.sequence->performanceHasFocus());
        for(const auto& card:hardware.modules)noteFocus|=card->sequence&&card->sequence->performanceHasFocus();
        noteFocus|=editor.performance&&editor.performance->hasEditorFocus();
        noteFocus|=editor.referenceSequence&&editor.referenceSequence->hasEditorFocus();
        for(const auto& card:hardware.modules)noteFocus|=card->referenceSequence&&card->referenceSequence->hasEditorFocus();
        for(const auto& card:hardware.modules)noteFocus|=card->performance&&card->performance->hasEditorFocus();
        const bool enabled=caps&&Process::isForegroundProcess()&&noteFocus&&!textEditing&&!mods.isCommandDown()&&!mods.isCtrlDown()&&!mods.isAltDown();
        std::array<bool,18> down{};for(size_t i=0;i<down.size();++i)down[i]=KeyPress::isKeyCurrentlyDown(TypingKeyboard::keys[i]);
        typing.update(enabled,down,KeyPress::isKeyCurrentlyDown('Z'),KeyPress::isKeyCurrentlyDown('X'),[this](int n){keys.noteOn(1,n,.8f);},[this](int n){keys.noteOff(1,n,0);});
        MidiBuffer discard;keys.processNextMidiBuffer(discard,0,1,false);
        keyboardHelp.setText(String(enabled?"CAPS LOCK ON":"Caps Lock to play")+"  |  A W S E D F ...   Z/X octave  |  base "+MidiMessage::getMidiNoteName(typing.base,true,true,4),dontSendNotification);
        repaint(335,getHeight()-155,170,20);
    }
};
}
