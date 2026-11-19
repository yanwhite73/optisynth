// Included inside the native-check namespace; exercises the actual app components.
void referencePresets(){
    for(int i: {17,18}){
        auto d=starterDocument(i,.25f);prepareSamples(d);
        const auto saved=juce::JSON::toString(rackJSON(d));auto restored=readRack(juce::JSON::parse(saved));prepareSamples(restored);
        require(juce::JSON::toString(rackJSON(restored))==saved,"Reference sequencer state differs after JSON recall");
        const auto a=render(d,4),b=render(restored,4);require(a==b,"Reference sequencer stereo audio differs after recall");
        double energy=0;float peak=0;for(float v:a){require(std::isfinite(v),"Nonfinite reference preset audio");energy+=v*v;peak=std::max(peak,std::abs(v));}require(energy>1e-4,"Silent reference preset");
        if(i==18)for(int r=0;r<4;++r){auto solo=d;solo.graph.nodes[0].parameters.push_back({orbitKey(r,OrSolo),1});double energy=0;for(float v:render(solo,2))energy+=v*v;require(energy>1e-5,"Orbit ring has no audible destination");}
        const auto name=i==17?"probability-glass-branches":"orbit-four-satellites";
        std::ofstream preset(std::string("patches/")+name+".optisynth");preset<<saved.replace("\r\n","\n");require(bool(preset),"Write reference preset");
        std::printf("PASS %s: identical state and 4-second stereo recall, finite audible output, peak %.5f\n",name,peak);std::fflush(stdout);
    }
}
void referenceEditors(){
    juce::ScopedJuceInitialiser_GUI initialise;
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("build/reference-ui-review");require(folder.createDirectory().wasOk(),"Create reference UI folder");
    auto click=[](juce::Component& editor,const char* title){for(auto* child:editor.getChildren())if(auto* button=dynamic_cast<juce::Button*>(child))if(button->getButtonText()==title){require(button->isEnabled(),"Expected enabled reference action");button->onClick();return;}throw std::runtime_error(std::string("Missing reference action: ")+title);};
    auto capture=[&](juce::Component& editor,const char* name){juce::FileOutputStream stream(folder.getChildFile(name));require(stream.openedOk(),"Open reference image");stream.setPosition(0);stream.truncate();require(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),stream),"Write reference image");};
    auto gesture=[](optiui::ReferenceSequencerEditor& editor,juce::Point<float> from,juce::Point<float> to){const auto now=juce::Time::getCurrentTime();
        auto event=[&](juce::Point<float> p){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys::leftButtonModifier,0,0,0,0,0,&editor,&editor,now,from,now,1,p!=from);};
        editor.mouseDown(event(from));editor.mouseDrag(event(to));editor.mouseUp(event(to));};
    int sliders=0;
    for(const auto type:{"seq.probability","seq.orbit"}){
        const bool orbit=std::string(type)=="seq.orbit";auto module=makeReferenceSequencer({"sequence",type,{}});
        optiui::ReferenceSequencerEditor editor(type,{},module->controls);std::map<std::string,float> saved;int publications=0;
        editor.parameterChanged=[&](const std::string& k,float v){saved[k]=v;++publications;};sliders+=checkScrollNavigation(editor);
        const auto original=editor.values();
        click(editor,"New choices");require(editor.values()!=original,"New choices did not change saved seed");editor.undo();require(editor.values()==original,"New choices undo failed");
        if(!orbit){
            gesture(editor,editor.probabilityCell(0,15).getCentre(),editor.probabilityCell(1,15).getCentre());
            require(saved[probabilityKey(0,15)]==100&&saved[probabilityKey(1,15)]==100,"Paint gesture did not reach both saved columns");
            editor.undo();require(editor.values()==original,"Grid drag did not undo as one gesture");editor.redo();require(editor.values()[probabilityIndex(1,15)]==100,"Grid redo failed");editor.undo();
            click(editor,"Select");gesture(editor,editor.probabilityCell(2,10).getCentre(),editor.probabilityCell(2,10).getCentre());require(editor.values()==original,"Select tool painted a note");
            editor.setValue(probabilityKey(2,PrStepLock),1);const auto protectedValues=editor.values();
            click(editor,"Snapshot");const int before=publications;click(editor,"Preview variation");require(editor.values()==protectedValues&&before==publications,"Preview was published to playback");
            capture(editor,"probability-preview.png");click(editor,"Apply");for(int r=0;r<16;++r)require(editor.values()[probabilityIndex(2,r)]==protectedValues[probabilityIndex(2,r)],"Variation changed a protected column");
            require(editor.values()!=protectedValues,"Variation generated no change");click(editor,"Restore");require(editor.values()==protectedValues,"Snapshot restore failed");
        }else{
            gesture(editor,editor.orbitPoint(1,0),editor.orbitPoint(1,5));require(editor.values()[orbitIndex(1,OrRotation)]==5,"Ring rotation gesture did not edit phase");editor.undo();require(editor.values()==original,"Ring rotation undo failed");
            const int before=publications;click(editor,"Preview variation");require(editor.values()==original&&before==publications,"Orbit preview changed playback");click(editor,"Apply");require(editor.values()!=original,"Orbit variation did not change rings");editor.undo();require(editor.values()==original,"Orbit variation undo failed");
        }
        // Drive the same module receiving editor gestures, so playheads/outcomes are engine state.
        SampleClock clock;clock.play(true);std::array<ClockFrame,24> time;std::array<NoteEventBlock,5> buses;std::array<NoteEventBlock*,13> events{};std::array<std::array<float,24>,13> output{};std::array<float*,13> outputs{};
        for(int i=0;i<13;++i)outputs[i]=output[i].data();for(int r=0;r<5;++r)events[r*3]=&buses[r];
        for(int block=0;block<3250;++block){for(auto& t:time)t=clock.next();for(auto& bus:buses)bus.begin(24);module->process({24,{},nullptr,outputs.data(),nullptr,nullptr,nullptr,time.data(),&buses[0],events.data()});}
        require(module->controls->referenceSequence->heads[0]>=0,"No actual engine playhead in editor");editor.setActive(true);
        if(!orbit){
            const auto s=module->controls->referenceSequence;const auto before=editor.values();click(editor,"Capture played notes");int visited=0;
            for(int step=0;step<32;++step){const int chosen=s->outcomes[step];if(chosen<-1)continue;++visited;for(int row=0;row<16;++row)require(editor.values()[probabilityIndex(step,row)]==(row==chosen?100:0),"Capture disagrees with actual played choice");}
            require(visited>8,"Too few actual played steps captured");editor.undo();require(editor.values()==before,"Capture undo failed");
        }
        editor.setSize(1220,860);capture(editor,orbit?"orbit.png":"probability.png");editor.setSize(1120,820);capture(editor,orbit?"orbit-minimum.png":"probability-minimum.png");
        for(auto* child:editor.getChildren())if(child->isVisible())require(editor.getLocalBounds().contains(child->getBounds()),"Visible editor control clipped at minimum size");
        for(const auto* label:{"Snapshot","Restore","Preview variation","Apply","Discard"}){bool visible=false;for(auto* c:editor.getChildren())if(auto* b=dynamic_cast<juce::Button*>(c))if(b->getButtonText()==label)visible=b->isVisible()&&!b->getBounds().isEmpty();require(visible,"Footer action missing at minimum size");}
        std::printf("PASS %s: actual editor gestures, grouped undo, protected preview, engine display, native normal/minimum captures\n",type);
    }
    std::printf("PASS %d reference editor sliders preserve values and forward all wheel directions\n",sliders);
    require(folder.getChildFile("editor-check.txt").replaceWithText("PASS: actual native Probability and Orbit components; grid paint/select, ring rotation, grouped undo/redo, preview/apply/protection, snapshot restore, capture of actual DSP outcomes, scroll forwarding and five component images. No desktop peer or audio device opened.\n"),"Write reference UI receipt");
}
