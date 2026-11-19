#pragma once
#include <JuceHeader.h>
#include "../src/modular/player.h"
#include "../plugin/Source/TypingKeyboard.h"
#include <ApplicationServices/ApplicationServices.h>
#include <map>
#include "SequencerEditor.h"
#include "PerformanceEditor.h"
#include "ReferenceSequencerEditor.h"

using namespace juce;
using namespace optisynth;
namespace {
const Colour ink(0xff202224), paper(0xffe8e5d8), teal(0xff86b9c7), orange(0xffd7a96b);
Font font(float size) { return Font(FontOptions(size)); }
using Values=std::map<std::string,std::map<std::string,float>>;
Colour panelColour(const std::string& id) {
    if(isSequencer(id))return Colour(0xffc9bf91);
    if(id=="sample.player")return Colour(0xffd7ca96);
    if(id=="spectral.engine")return Colour(0xff8ebecb);
    if(id=="mod.lfo")return Colour(0xffd7b282);
    if(id=="modal.voice"||id=="modal.resonator")return Colour(0xffb4a4d1);
    if(id=="drum.analog")return Colour(0xffd7b282);
    if(id.rfind("osc.",0)==0||id=="analog.vco")return Colour(0xff8ebecb);
    if(id=="voice.mix")return Colour(0xffc9a96a);
    if(id.rfind("fx.",0)==0)return Colour(0xff6f8f8b);
    if(id=="mod.wheel")return Colour(0xffd7b282);
    if(panel(id).family=="filter"&&id!="filter.bypass")return Colour(0xffa2c67f);
    return Colour(0xff36383a);
}
Colour panelInk(const std::string& id) {return panelColour(id).getBrightness()>.6f?ink:paper;}
int panelWidth(const std::string& id) {
    if(id=="seq.performance"||id=="seq.orbit")return 800;
    if(id=="seq.probability")return 650;
    if(isSequencer(id))return 560;
    if(id=="spectral.engine")return 580;
    if(id=="sample.player")return 340;
    if(id=="spectral.ahdsfr")return 330;
    if(id.rfind("spectral.",0)==0&&panel(id).family=="filter")return 290;
    if(id=="analog.vco"||id=="analog.filter")return 290;
    if(id=="analog.env")return 230;
    if(id=="analog.vca"||id=="mod.wheel")return 170;
    if(id=="voice.mix")return 200;
    if(id=="utility.vca"||id=="filter.bypass")return 155;
    if(id=="utility.mix"||id=="osc.sine")return 200;
    return id=="modal.resonator"?270:250;
}
struct KnobLook : LookAndFeel_V4 {
    Colour markings=ink;
    void drawLinearSlider(Graphics& g,int x,int y,int width,int height,float pos,float,float,Slider::SliderStyle style,Slider&) override {
        const bool vertical=style==Slider::LinearVertical;const float centre=vertical?x+width*.5f:y+height*.5f;
        g.setColour(Colour(0xff151718));
        if(vertical)g.fillRoundedRectangle(centre-3,float(y),6,float(height),2);
        else g.fillRoundedRectangle(float(x),centre-2,float(width),4,2);
        if(vertical){g.setColour(markings.withAlpha(.55f));for(int i=0;i<=10;++i){const float yy=y+height*i/10.f;g.drawLine(centre-19,yy,centre-8,yy,.7f);g.drawLine(centre+8,yy,centre+19,yy,.7f);}}
        auto handle=vertical?Rectangle<float>(centre-22,pos-7,44,14):Rectangle<float>(pos-7,centre-6,14,12);
        g.setColour(Colours::black.withAlpha(.3f));g.fillRoundedRectangle(handle.translated(1,2),2);
        g.setColour(Colour(0xff181a1c));g.fillRoundedRectangle(handle,2);g.setColour(Colour(0xff737572));g.drawRoundedRectangle(handle,2,1);
        g.setColour(paper);if(vertical)g.drawLine(centre-18,pos,centre+18,pos,2);else g.drawLine(pos,centre-4,pos,centre+4,2);
    }
    void drawRotarySlider(Graphics& g,int x,int y,int width,int height,float position,float start,float end,Slider&) override {
        auto r=Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(3);
        const float diameter=std::min(r.getWidth(),r.getHeight());r=r.withSizeKeepingCentre(diameter,diameter);
        const auto c=r.getCentre();const float radius=diameter*.5f;
        for(int i=0;i<=10;++i){const float a=start+(end-start)*i/10.f;g.setColour(markings.withAlpha(.7f));
            g.drawLine(c.x+std::sin(a)*(radius-3),c.y-std::cos(a)*(radius-3),c.x+std::sin(a)*radius,c.y-std::cos(a)*radius,i==5?1.6f:.8f);}
        auto face=r.reduced(std::max(4.f,diameter*.1f));
        g.setColour(Colours::black.withAlpha(.25f));g.fillEllipse(face.translated(1.5f,2.5f));
        g.setColour(Colour(0xff141618));g.fillEllipse(face);g.setColour(Colour(0xff4b4c4d));g.drawEllipse(face.reduced(.8f),1.3f);
        auto cap=face.reduced(face.getWidth()*.22f);g.setColour(Colour(0xffc5c5bd));g.fillEllipse(cap);
        g.setColour(Colour(0xffecebe3));g.drawEllipse(cap.reduced(.8f),1);
        const float a=start+position*(end-start),outer=face.getWidth()*.46f;
        g.setColour(paper);g.drawLine(c.x+std::sin(a)*outer*.68f,c.y-std::cos(a)*outer*.68f,c.x+std::sin(a)*outer,c.y-std::cos(a)*outer,2.3f);
    }
};
struct Control : juce::Component {
    Label label;Slider slider;ToggleButton toggle;bool isSwitch=false;
    Control(const Parameter& p,float value,std::function<void(float)> changed,KnobLook& look,Colour text,bool fader=false,bool compactTrim=false) {
        label.setText(String(p.label).toUpperCase(),dontSendNotification);label.setFont(font(11));label.setColour(Label::textColourId,text);label.setJustificationType(Justification::centred);
        const bool trim=compactTrim||p.id.find("_curve")!=std::string::npos||p.id=="fade"||p.id=="mod_rate_hz"||p.id=="mod_depth"||p.id=="start"||p.id=="end";
        slider.setScrollWheelEnabled(false);
        slider.setSliderStyle(fader?Slider::LinearVertical:trim?Slider::LinearHorizontal:Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(Slider::TextBoxBelow,false,88,20);
        slider.setColour(Slider::textBoxTextColourId,text);slider.setColour(Slider::textBoxBackgroundColourId,Colours::transparentBlack);
        slider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);slider.setColour(Slider::trackColourId,teal);
        slider.setColour(Slider::thumbColourId,paper);slider.setColour(Slider::backgroundColourId,Colour(0xff151718));
        slider.setRange(p.minimum,p.maximum,p.step);
        if(p.logarithmic)slider.setSkewFactorFromMidPoint(p.minimum>0?std::sqrt(p.minimum*p.maximum):.3);
        slider.setTextValueSuffix(p.unit.empty()?"":" "+String(p.unit));slider.setNumDecimalPlacesToDisplay(p.step>=1?0:p.unit=="Hz"?1:p.unit=="st"?2:3);
        if(p.id=="root_note")slider.setTextValueSuffix(" MIDI");
        if(p.id=="wave"){
            slider.textFromValueFunction=[](double v){const char* names[]={"Sine","Triangle","Ramp","Pulse"};return String(names[std::clamp(int(std::round(v)),0,3)]);};
            slider.valueFromTextFunction=[](const String& t){return t.equalsIgnoreCase("Triangle")?1.:t.equalsIgnoreCase("Ramp")?2.:t.equalsIgnoreCase("Pulse")?3.:0.;};
        }
        slider.setValue(value,dontSendNotification);slider.updateText();slider.setDoubleClickReturnValue(true,p.initial);
        slider.setTooltip(p.label+" / double-click to reset");slider.setLookAndFeel(&look);
        slider.onValueChange=[this,changed]{toggle.setToggleState(slider.getValue()>=.5,dontSendNotification);changed(float(slider.getValue()));};addAndMakeVisible(label);addAndMakeVisible(slider);
        if(p.id=="loop"||p.id=="retrigger"){
            isSwitch=true;label.setVisible(false);slider.setVisible(false);addAndMakeVisible(toggle);toggle.setButtonText(p.id=="loop"?"Loop while held":"Reset on note");
            toggle.setColour(ToggleButton::textColourId,text);toggle.setColour(ToggleButton::tickColourId,text);toggle.setColour(ToggleButton::tickDisabledColourId,text.withAlpha(.5f));
            toggle.setToggleState(value>=.5f,dontSendNotification);toggle.onClick=[this]{slider.setValue(toggle.getToggleState()?1:0,sendNotificationSync);};
        }
    }
    void setValue(float v){slider.setValue(v,dontSendNotification);toggle.setToggleState(v>=.5f,dontSendNotification);}
    ~Control() override {slider.setLookAndFeel(nullptr);}
    void resized() override {
        if(isSwitch){toggle.setBounds(getLocalBounds());return;}
        const bool compact=slider.isRotary()&&getHeight()<90&&getWidth()>=105;
        const auto position=compact?Slider::TextBoxRight:Slider::TextBoxBelow;
        if(slider.getTextBoxPosition()!=position)slider.setTextBoxStyle(position,false,compact?64:88,20);
        label.setBounds(0,0,getWidth(),19);slider.setBounds(0,18,getWidth(),getHeight()-18);
    }
};
struct Jack : Button {
    Port port;bool output=false,connected=false;Colour text=paper;
    Jack(Port p,bool out):Button(String(p.id)+(out?" output":" input")),port(std::move(p)),output(out){}
    void paintButton(Graphics& g,bool over,bool down) override {
        const float cx=getWidth()*.5f,cy=17;
        g.setColour(Colour(0xff16181a));g.fillEllipse(cx-12,cy-12,24,24);
        g.setColour(over||down?Colour(0xfff0c76c):Colour(0xffc2c3bd));g.drawEllipse(cx-10,cy-10,20,20,2.8f);
        g.setColour(connected?(port.type==SignalType::Control?Colour(0xfff0c76c):teal):Colour(0xff080a0c));g.fillEllipse(cx-5,cy-5,10,10);
        g.setColour(text);g.setFont(font(9));g.drawText(String(port.id).toUpperCase(),0,32,getWidth(),15,Justification::centred);
        if(output){g.setColour(text.withAlpha(.6f));g.drawLine(cx-10,48,cx+10,48,1);}
    }
};
struct ModulePanel : juce::Component, FileDragAndDropTarget {
    std::unique_ptr<optiui::SequencerEditor> sequence;
    std::unique_ptr<optiui::PerformanceLauncher> performance;
    std::unique_ptr<optiui::ReferenceSequenceLauncher> referenceSequence;
    ComboBox choice;KnobLook look;std::vector<std::unique_ptr<Control>> knobs;
    ComboBox section;TextButton presetButton{"Load Spectral patch..."},more{"More"};Label presetName;int subpage=0;
    std::function<void()> choosePreset;std::vector<std::vector<int>> pages;
    std::vector<std::unique_ptr<Jack>> jacks;std::vector<std::string> types;
    std::string current,family,instance;bool routed=true,highlighted=false;
    std::function<void()> selectionChanged,activated;
    std::function<void(const Port&,bool,juce::Component*)> portClicked;
    TextButton sampleButton{"Load WAV..."};Label sampleName;SamplePtr sample;std::string samplePath;
    std::function<void()> chooseSample;std::function<void(const File&)> sampleDropped;
    ModulePanel(std::string group):family(std::move(group)) {
        choice.setColour(ComboBox::backgroundColourId,Colour(0xff232527));choice.setColour(ComboBox::textColourId,paper);
        choice.setColour(ComboBox::outlineColourId,Colours::transparentBlack);
        for(const auto& p:panels())if(family=="all"||p.family==family){types.push_back(p.type);choice.addItem(p.name,int(types.size()));}
        addAndMakeVisible(choice);choice.onChange=[this]{if(selectionChanged)selectionChanged();};addMouseListener(this,true);
        addChildComponent(sampleButton);addChildComponent(sampleName);sampleButton.onClick=[this]{if(chooseSample)chooseSample();};
        sampleName.setFont(font(11));sampleName.setColour(Label::textColourId,ink);
        addChildComponent(section);addChildComponent(presetButton);addChildComponent(presetName);addChildComponent(more);
        more.onClick=[this]{++subpage;resized();repaint();};
        presetButton.onClick=[this]{if(choosePreset)choosePreset();};section.onChange=[this]{subpage=0;resized();repaint();};
        presetName.setFont(font(12));presetName.setColour(Label::textColourId,ink);
    }
    ~ModulePanel() override {removeMouseListener(this);}
    void mouseDown(const MouseEvent&) override {if(activated)activated();}
    bool isInterestedInFileDrag(const StringArray& files) override {return current=="sample.player"&&files.size()==1&&files[0].endsWithIgnoreCase(".wav");}
    void filesDropped(const StringArray& files,int,int) override {
        if(!isInterestedInFileDrag(files))return;
        // Loading can replace this panel. Dispatch after the drag callback returns.
        auto callback=sampleDropped;auto file=File(files[0]);MessageManager::callAsync([safe=SafePointer<ModulePanel>(this),callback,file]{if(safe&&callback)callback(file);});
    }
    void setSample(const std::string& path,SamplePtr data){samplePath=path;sample=std::move(data);
        const String name=path.empty()?"Drop a WAV here":path=="builtin:click"?"Built-in percussive click":File(String(path)).getFileName();
        sampleName.setText(name,dontSendNotification);sampleName.setTooltip(String(path)+(sample?"\n"+String(sample->mono.size()/sample->rate,2)+" s / "+String(sample->sourceChannels)+" channel(s) to mono\nWAV files stay external to the saved patch.":"\nSample loads when this module is connected."));repaint();
    }
    std::string selected() const {int i=choice.getSelectedId()-1;return i>=0&&i<int(types.size())?types[size_t(i)]:types.front();}
    void select(const std::string& type) {auto it=std::find(types.begin(),types.end(),type);if(it==types.end())throw std::invalid_argument("Wrong module family: "+type);choice.setSelectedId(int(it-types.begin())+1,dontSendNotification);}
    void show(const std::string& type,Values& state,std::shared_ptr<Controls> controls,std::function<void(const std::string&,float)> changed={}) {
        current=type;family=panel(type).family;routed=bool(controls);select(type);sequence.reset();performance.reset();referenceSequence.reset();knobs.clear();jacks.clear();look.markings=panelInk(type);
        sampleButton.setVisible(type=="sample.player");sampleName.setVisible(type=="sample.player");
        const bool spectral=type=="spectral.engine";section.setVisible(spectral);presetButton.setVisible(spectral);presetName.setVisible(spectral);more.setVisible(spectral);subpage=0;
        const auto& d=panel(type);
        for(size_t i=0;i<d.parameters.size();++i){auto p=d.parameters[i];auto& saved=state[type];if(!saved.count(p.id))saved[p.id]=p.initial;
            if(isSequencer(type)||type=="seq.performance"||isReferenceSequencer(type))continue;
            auto knob=std::make_unique<Control>(p,saved[p.id],[&,controls,i,type,id=p.id,changed](float v){state[type][id]=v;if(controls)controls->values[i].store(v,std::memory_order_relaxed);if(changed)changed(id,v);},look,panelInk(type),(family=="envelope"&&p.id=="sustain")||type=="utility.mix",family=="filter"&&type.rfind("spectral.",0)==0&&p.id=="env_octaves");
            addAndMakeVisible(*knob);knobs.push_back(std::move(knob));}
        if(isReferenceSequencer(type)){referenceSequence=std::make_unique<optiui::ReferenceSequenceLauncher>(type,state[type],controls);referenceSequence->parameterChanged=changed;addAndMakeVisible(*referenceSequence);}
        if(type=="seq.performance"){performance=std::make_unique<optiui::PerformanceLauncher>(controls,state[type]);performance->parameterChanged=changed;addAndMakeVisible(*performance);}
        if(isSequencer(type)){
            sequence=std::make_unique<optiui::SequencerEditor>(type,state[type],controls,[&,type,controls,changed](const std::string& id,float v){
                state[type][id]=v;const auto& ps=panel(type).parameters;
                for(size_t i=0;i<ps.size();++i)if(ps[i].id==id&&controls)controls->values[i].store(v);
                if(changed)changed(id,v);
            });addAndMakeVisible(*sequence);
        }
        if(spectral){
            pages.clear();section.clear(dontSendNotification);
            auto page=[&](const String& name,int first,int count){std::vector<int> indices;for(int i=0;i<count;++i)indices.push_back(first+i);pages.push_back(std::move(indices));section.addItem(name,int(pages.size()));};
            pages.push_back({partials::kLiveMain,partials::kLiveControls,48,49,50,51,52});section.addItem("Main / clock / LFO rates",1);
            for(int o=0;o<4;++o)page("Oscillator "+String(o+1)+" / filter",o*12,12);
            for(int e=0;e<12;++e)page(e<4?"Amp envelope "+String(e+1):e<8?"Cutoff envelope "+String(e-3):e<11?"Mod envelope "+String(e-7):"Main envelope",60+e*9,9);
            for(int i=0;i<3;++i)page("Internal matrix depths "+String(i*8+1)+"-"+String(i*8+8),168+i*8,8);
            for(int slot=0;slot<6;++slot)page("FX "+String(slot+1),54+slot,1);
            section.setSelectedId(1,dontSendNotification);
        }
        for(const auto& t:moduleRegistry())if(t.id==type){
            auto add=[&](Port p,bool output){auto jack=std::make_unique<Jack>(p,output);jack->text=panelInk(type);auto* ptr=jack.get();
                jack->setTooltip(String(instance)+"."+String(p.id)+(output?" / output":" / choose input source"));
                jack->onClick=[this,p,output,ptr]{if(portClicked)portClicked(p,output,ptr);};addAndMakeVisible(*jack);jacks.push_back(std::move(jack));};
            for(const auto& p:t.inputs)add(p,false);for(const auto& p:t.outputs)add(p,true);}
        resized();repaint();
    }
    void setSpectral(SpectralPtr state,const GraphSpec& graph){
        if(current!="spectral.engine"||!state)return;
        presetName.setText(state->patch.name,dontSendNotification);
        presetName.setTooltip("Whole preset embedded in rack. Modulation sockets follow the newest active Spectral voice, including release.");
        for(int slot=0;slot<6;++slot){auto& page=pages[size_t(20+slot)];page={54+slot};
            for(int f=0;f<partials::kEffectControls;++f)if(partials::effectControlSpecs[f].type==state->patch.fx[slot].type)page.push_back(192+slot*partials::kEffectControls+f);
        }
        for(size_t i=0;i<knobs.size();++i)if(i<partials::kLiveControls){
            bool active=spectralParameterActive(state->patch,int(i));
            if(i>=48&&i<53&&state->patch.lfo[i-48].syncBeats==0){
                const auto port="lfo"+std::to_string(i-47);
                for(const auto& route:graph.modulations)if(route.source.node==instance&&route.source.port==port)active=true;
                for(const auto& route:graph.connections)if(route.source.node==instance&&route.source.port==port)active=true;
            }
            knobs[i]->setAlpha(active?1.f:.45f);
        }
        resized();
    }
    void setConnections(const GraphSpec& graph) {
        for(auto& j:jacks){j->connected=false;String links;
            for(const auto& c:graph.connections){const auto& e=j->output?c.source:c.destination;if(e.node==instance&&e.port==j->port.id){j->connected=true;const auto& other=j->output?c.destination:c.source;links+="\n"+String(other.node)+"."+String(other.port);}}
            if(j->output)for(const auto& c:graph.modulations)if(c.source.node==instance&&c.source.port==j->port.id){j->connected=true;links+="\nModulates "+String(c.destination.node)+"."+String(c.destination.port);}
            if(j->output)for(const auto& e:graph.outputs)if(e.node==instance&&e.port==j->port.id){j->connected=true;links+="\nInstrument output";}
            j->setTooltip(String(instance)+"."+String(j->port.id)+(j->output?" / output":" / input")+(links.isEmpty()?"\nNot connected":links)+"\nClick to patch");j->repaint();}
    }
    void setValue(const std::string& id,float value){if(referenceSequence){referenceSequence->setValue(id,value);return;}if(performance){performance->setValue(id,value);return;}if(sequence){sequence->setValue(id,value);return;}const auto& ps=panel(current).parameters;for(size_t i=0;i<ps.size();++i)if(ps[i].id==id)knobs[i]->setValue(value);if(current=="sample.player")repaint();}
    void paint(Graphics& g) override {
        if(current.empty())return;auto r=getLocalBounds().toFloat();const auto text=panelInk(current);
        g.setColour(panelColour(current));g.fillRoundedRectangle(r,3);g.setColour(highlighted?Colour(0xfff0c76c):text.withAlpha(.6f));g.drawRoundedRectangle(r.reduced(1),3,highlighted?2:1);
        g.setFont(font(10));g.setColour(text);g.drawText(String(instance).toUpperCase()+"  /  "+String(family).toUpperCase(),12,10,getWidth()-24,18,Justification::centredLeft);
        g.setColour(routed?Colour(0xffcbe0a8):Colour(0xff7b7b70));g.fillEllipse(float(getWidth()-19),14,6,6);
        g.setColour(text.withAlpha(.35f));g.drawLine(12,current=="spectral.engine"?184.f:135.f,float(getWidth()-12),current=="spectral.engine"?184.f:135.f,.7f);
        if(current=="sample.player"){
            auto wave=Rectangle<float>(16,204,float(getWidth()-32),40);g.setColour(ink);g.fillRoundedRectangle(wave,2);
            if(sample){const float start=float(knobs[2]->slider.getValue()),end=float(knobs[3]->slider.getValue());
                g.setColour(Colour(0xffd7ca96).withAlpha(.18f));g.fillRect(wave.withX(wave.getX()+start*wave.getWidth()).withWidth(std::max(0.f,end-start)*wave.getWidth()));
                g.setColour(Colour(0xffd7ca96));for(size_t i=0;i<sample->peaks.size();++i){float x=wave.getX()+(i+.5f)*wave.getWidth()/128,amplitude=std::min(1.f,sample->peaks[i])*18;g.drawLine(x,224-amplitude,x,224+amplitude,1);}
                g.setColour(teal);g.drawVerticalLine(int(wave.getX()+start*wave.getWidth()),204,244);g.drawVerticalLine(int(wave.getX()+end*wave.getWidth()),204,244);
            }else{g.setColour(paper.withAlpha(.6f));g.setFont(font(11));g.drawText(samplePath.empty()?"LOAD OR DROP A WAV":"CONNECT TO LOAD SAMPLE",wave,Justification::centred);}
        }
        if(knobs.empty()&&!sequence&&!performance&&!referenceSequence){g.setColour(text);g.setFont(font(16));g.drawFittedText(current=="utility.vca"?"AUDIO\nx\nCONTROL":current=="utility.downmix"?"(L + R) / 2":current=="utility.stereo"?"LEFT / RIGHT":"SIGNAL\nTHROUGH",15,195,getWidth()-30,100,Justification::centred,3);}
        g.setColour(text.withAlpha(.8f));g.setFont(font(10));g.drawFittedText(routed?panel(current).description:"UNROUTED / connect to activate",12,getHeight()-37,getWidth()-24,29,Justification::centred,2);
    }
    void resized() override {
        choice.setBounds(12,35,getWidth()-24,31);
        const int columns=current=="spectral.engine"?10:std::max(1,int(jacks.size()));
        for(size_t i=0;i<jacks.size();++i){const int w=(getWidth()-24)/columns;jacks[i]->setBounds(12+int(i)%columns*w,77+int(i)/columns*53,w,53);}
        if(current.empty())return;
        if(referenceSequence){referenceSequence->setBounds(12,143,getWidth()-24,getHeight()-184);return;}
        if(performance){performance->setBounds(12,143,getWidth()-24,getHeight()-184);return;}
        if(sequence){sequence->setBounds(12,143,getWidth()-24,getHeight()-184);return;}
        const auto& ps=panel(current).parameters;const float w=float(getWidth()-24),h=float(std::max(220,getHeight()-190));
        auto put=[&](const char* id,float x,float y,float width,float height){for(size_t i=0;i<ps.size();++i)if(ps[i].id==id)knobs[i]->setBounds(12+int(x*w),147+int(y*h),int(width*w),int(height*h));};
        if(current=="spectral.engine"){
            presetButton.setBounds(14,193,190,28);presetName.setBounds(213,193,getWidth()-227,28);
            section.setBounds(14,228,getWidth()-124,28);more.setBounds(getWidth()-102,228,88,28);
            for(auto& knob:knobs)knob->setVisible(false);
            const int page=section.getSelectedId()-1;
            if(page>=0&&page<int(pages.size())){
                const auto& visible=pages[size_t(page)];const int parts=std::max(1,(int(visible.size())+5)/6);subpage%=parts;
                more.setEnabled(parts>1);more.setButtonText(String(subpage+1)+" / "+String(parts)+" >");
                const int first=subpage*6,count=std::min(6,int(visible.size())-first),cols=getHeight()<530?6:3,rows=std::max(1,(count+cols-1)/cols);
                const int cell=std::max(32,(getHeight()-307)/rows);
                for(int i=0;i<count;++i){auto& knob=*knobs[size_t(visible[size_t(first+i)])];knob.setVisible(true);knob.setBounds(12+i%cols*int(w/cols),266+i/cols*cell,int(w/cols),cell);}
            }
        }else if(current=="sample.player"){
            sampleButton.setBounds(16,147,int(w*.48f),28);sampleName.setBounds(12,178,getWidth()-24,23);
            const int row=std::max(80,int((getHeight()-291)*.62f));
            for(size_t i=0;i<ps.size();++i){const auto& id=ps[i].id;
                if(id=="loop")knobs[i]->setBounds(12+int(w*.51f),147,int(w*.49f),28);
                else if(id=="start"||id=="end")knobs[i]->setBounds(12+(id=="end"?int(w*.5f):0),252+row,int(w*.5f),getHeight()-289-row);
                else {int col=id=="transpose"?0:id=="root_note"?1:2;knobs[i]->setBounds(12+int(col*w/3),252,int(w/3),row);}
            }
        }else if(current=="spectral.adsr"){
            put("attack",0,0,.5f,.33f);put("decay",0,.33f,.5f,.33f);put("release",0,.66f,.5f,.33f);put("sustain",.55f,0,.4f,.99f);
        }else if(current=="spectral.ahdsfr"){
            put("attack",0,0,.34f,.39f);put("hold",.34f,0,.34f,.39f);put("decay",0,.40f,.34f,.39f);put("release",.34f,.40f,.34f,.39f);
            put("sustain",.70f,0,.28f,.58f);put("fade",.70f,.59f,.28f,.21f);
            put("attack_curve",0,.80f,.33f,.20f);put("decay_curve",.33f,.80f,.33f,.20f);put("release_curve",.66f,.80f,.33f,.20f);
        }else if(family=="filter"){
            put("cutoff_hz",.17f,0,.66f,.49f);put("resonance",0,.50f,.5f,.34f);
            if(current.rfind("mutable.",0)==0)put("env_octaves",.5f,.50f,.5f,.34f);
            else {put("cutoff_hz",.17f,0,.66f,.40f);put("saturation",.5f,.41f,.5f,.36f);put("resonance",0,.41f,.5f,.36f);put("env_octaves",0,.78f,.33f,.22f);put("mod_rate_hz",.33f,.78f,.33f,.22f);put("mod_depth",.66f,.78f,.33f,.22f);}
        }else if(current=="osc.variable"){
            put("transpose",0,0,.5f,.33f);put("sync",.5f,0,.5f,.33f);put("shape",0,.34f,.5f,.33f);put("pulse_width",.5f,.34f,.5f,.33f);put("level",.25f,.68f,.5f,.32f);
        }else if(current=="modal.resonator"){
            put("transpose",0,0,.5f,.28f);put("input_gain",.5f,0,.5f,.28f);put("structure",.23f,.28f,.54f,.37f);put("brightness",0,.65f,.5f,.34f);put("decay",.5f,.65f,.5f,.34f);
        }else if(current=="drum.analog"){
            put("transpose",0,0,.5f,.32f);put("tone",.5f,0,.5f,.32f);put("decay",.22f,.32f,.56f,.36f);put("attack_fm",0,.69f,.5f,.30f);put("self_fm",.5f,.69f,.5f,.30f);
        }else if(current=="utility.mix"){
            put("gain_a",0,0,.5f,.95f);put("gain_b",.5f,0,.5f,.95f);
        }else if(current=="osc.sine"){
            put("transpose",.15f,0,.7f,.47f);put("level",.15f,.5f,.7f,.47f);
        }else{
            const int cols=2,rows=std::max(1,(int(knobs.size())+1)/2);
            for(size_t i=0;i<knobs.size();++i)knobs[i]->setBounds(12+int(i)%cols*int(w/cols),147+int(i)/cols*int(h/rows),int(w/cols),int(h/rows));
        }
    }
};
struct HardwareRack : juce::Component {
    std::map<std::string,Values> values;
    std::vector<std::unique_ptr<ModulePanel>> modules;
    void clear(){modules.clear();values.clear();}
    void arrange(int availableWidth,int height){int natural=16+int(modules.size())*6;for(const auto& m:modules)natural+=panelWidth(m->current);
        const float stretch=std::max(1.f,float(availableWidth-16-int(modules.size())*6)/float(std::max(1,natural-16-int(modules.size())*6)));
        int x=8;for(auto& m:modules){const int w=int(panelWidth(m->current)*stretch);m->setBounds(x,8,w,height-16);x+=w+6;}setSize(std::max(availableWidth,x+8),height);}
    ModulePanel* find(const std::string& id){for(auto& m:modules)if(m->instance==id)return m.get();return nullptr;}
    void highlight(const std::string& id){for(auto& m:modules){m->highlighted=m->instance==id;m->repaint();}}
    void paint(Graphics& g) override {g.fillAll(Colour(0xff17191b));}
};
}
