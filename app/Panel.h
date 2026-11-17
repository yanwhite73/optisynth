#pragma once
#include <JuceHeader.h>
#include "../src/modular/player.h"
#include "../plugin/Source/TypingKeyboard.h"
#include <ApplicationServices/ApplicationServices.h>
#include <map>

using namespace juce;
using namespace optisynth;
namespace {
const Colour ink(0xff202224), paper(0xffe8e5d8), teal(0xff86b9c7), orange(0xffd7a96b);
Font font(float size) { return Font(FontOptions(size)); }
using Values=std::map<std::string,std::map<std::string,float>>;
Colour panelColour(const std::string& id) {
    if(id=="modal.voice"||id=="modal.resonator")return Colour(0xffb4a4d1);
    if(id=="drum.analog")return Colour(0xffd7b282);
    if(id.rfind("osc.",0)==0)return Colour(0xff8ebecb);
    if(panel(id).family=="filter"&&id!="filter.bypass")return Colour(0xffa2c67f);
    return Colour(0xff36383a);
}
Colour panelInk(const std::string& id) {return panelColour(id).getBrightness()>.6f?ink:paper;}
int panelWidth(const std::string& id) {
    if(id=="spectral.ahdsfr")return 330;
    if(id.rfind("spectral.",0)==0&&panel(id).family=="filter")return 290;
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
    Label label;Slider slider;
    Control(const Parameter& p,float value,std::function<void(float)> changed,KnobLook& look,Colour text,bool fader=false,bool compactTrim=false) {
        label.setText(String(p.label).toUpperCase(),dontSendNotification);label.setFont(font(11));label.setColour(Label::textColourId,text);label.setJustificationType(Justification::centred);
        const bool trim=compactTrim||p.id.find("_curve")!=std::string::npos||p.id=="fade"||p.id=="mod_rate_hz"||p.id=="mod_depth";
        slider.setSliderStyle(fader?Slider::LinearVertical:trim?Slider::LinearHorizontal:Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(Slider::TextBoxBelow,false,88,20);
        slider.setColour(Slider::textBoxTextColourId,text);slider.setColour(Slider::textBoxBackgroundColourId,Colours::transparentBlack);
        slider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);slider.setColour(Slider::trackColourId,teal);
        slider.setColour(Slider::thumbColourId,paper);slider.setColour(Slider::backgroundColourId,Colour(0xff151718));
        slider.setRange(p.minimum,p.maximum,.0001);
        if(p.logarithmic)slider.setSkewFactorFromMidPoint(p.minimum>0?std::sqrt(p.minimum*p.maximum):.3);
        slider.setTextValueSuffix(p.unit.empty()?"":" "+String(p.unit));slider.setNumDecimalPlacesToDisplay(p.unit=="Hz"?1:p.unit=="st"?2:3);
        slider.setValue(value,dontSendNotification);slider.setDoubleClickReturnValue(true,p.initial);
        slider.setTooltip(p.label+" / double-click to reset");slider.setLookAndFeel(&look);
        slider.onValueChange=[this,changed]{changed(float(slider.getValue()));};addAndMakeVisible(label);addAndMakeVisible(slider);
    }
    ~Control() override {slider.setLookAndFeel(nullptr);}
    void resized() override {
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
struct ModulePanel : juce::Component {
    ComboBox choice;KnobLook look;std::vector<std::unique_ptr<Control>> knobs;
    std::vector<std::unique_ptr<Jack>> jacks;std::vector<std::string> types;
    std::string current,family,instance;bool routed=true,highlighted=false;
    std::function<void()> selectionChanged,activated;
    std::function<void(const Port&,bool,juce::Component*)> portClicked;
    ModulePanel(std::string group):family(std::move(group)) {
        choice.setColour(ComboBox::backgroundColourId,Colour(0xff232527));choice.setColour(ComboBox::textColourId,paper);
        choice.setColour(ComboBox::outlineColourId,Colours::transparentBlack);
        for(const auto& p:panels())if(family=="all"||p.family==family){types.push_back(p.type);choice.addItem(p.name,int(types.size()));}
        addAndMakeVisible(choice);choice.onChange=[this]{if(selectionChanged)selectionChanged();};addMouseListener(this,true);
    }
    ~ModulePanel() override {removeMouseListener(this);}
    void mouseDown(const MouseEvent&) override {if(activated)activated();}
    std::string selected() const {int i=choice.getSelectedId()-1;return i>=0&&i<int(types.size())?types[size_t(i)]:types.front();}
    void select(const std::string& type) {auto it=std::find(types.begin(),types.end(),type);if(it==types.end())throw std::invalid_argument("Wrong module family: "+type);choice.setSelectedId(int(it-types.begin())+1,dontSendNotification);}
    void show(const std::string& type,Values& state,std::shared_ptr<Controls> controls,std::function<void(const std::string&,float)> changed={}) {
        current=type;family=panel(type).family;routed=bool(controls);select(type);knobs.clear();jacks.clear();look.markings=panelInk(type);
        const auto& d=panel(type);
        for(size_t i=0;i<d.parameters.size();++i){auto p=d.parameters[i];auto& saved=state[type];if(!saved.count(p.id))saved[p.id]=p.initial;
            auto knob=std::make_unique<Control>(p,saved[p.id],[&,controls,i,type,id=p.id,changed](float v){state[type][id]=v;if(controls)controls->values[i].store(v,std::memory_order_relaxed);if(changed)changed(id,v);},look,panelInk(type),(family=="envelope"&&p.id=="sustain")||type=="utility.mix",family=="filter"&&type.rfind("spectral.",0)==0&&p.id=="env_octaves");
            addAndMakeVisible(*knob);knobs.push_back(std::move(knob));}
        for(const auto& t:moduleRegistry())if(t.id==type){
            auto add=[&](Port p,bool output){auto jack=std::make_unique<Jack>(p,output);jack->text=panelInk(type);auto* ptr=jack.get();
                jack->setTooltip(String(instance)+"."+String(p.id)+(output?" / output":" / choose input source"));
                jack->onClick=[this,p,output,ptr]{if(portClicked)portClicked(p,output,ptr);};addAndMakeVisible(*jack);jacks.push_back(std::move(jack));};
            for(const auto& p:t.inputs)add(p,false);for(const auto& p:t.outputs)add(p,true);}
        resized();repaint();
    }
    void setConnections(const GraphSpec& graph) {
        for(auto& j:jacks){j->connected=false;String links;
            for(const auto& c:graph.connections){const auto& e=j->output?c.source:c.destination;if(e.node==instance&&e.port==j->port.id){j->connected=true;const auto& other=j->output?c.destination:c.source;links+="\n"+String(other.node)+"."+String(other.port);}}
            if(j->output)for(const auto& e:graph.outputs)if(e.node==instance&&e.port==j->port.id){j->connected=true;links+="\nInstrument output";}
            j->setTooltip(String(instance)+"."+String(j->port.id)+(j->output?" / output":" / input")+(links.isEmpty()?"\nNot connected":links)+"\nClick to patch");j->repaint();}
    }
    void setValue(const std::string& id,float value){const auto& ps=panel(current).parameters;for(size_t i=0;i<ps.size();++i)if(ps[i].id==id)knobs[i]->slider.setValue(value,dontSendNotification);}
    void paint(Graphics& g) override {
        if(current.empty())return;auto r=getLocalBounds().toFloat();const auto text=panelInk(current);
        g.setColour(panelColour(current));g.fillRoundedRectangle(r,3);g.setColour(highlighted?Colour(0xfff0c76c):text.withAlpha(.6f));g.drawRoundedRectangle(r.reduced(1),3,highlighted?2:1);
        g.setFont(font(10));g.setColour(text);g.drawText(String(instance).toUpperCase()+"  /  "+String(family).toUpperCase(),12,10,getWidth()-24,18,Justification::centredLeft);
        g.setColour(routed?Colour(0xffcbe0a8):Colour(0xff7b7b70));g.fillEllipse(float(getWidth()-19),14,6,6);
        g.setColour(text.withAlpha(.35f));g.drawLine(12,135,float(getWidth()-12),135,.7f);
        if(knobs.empty()){g.setColour(text);g.setFont(font(16));g.drawFittedText(current=="utility.vca"?"AUDIO\nx\nCONTROL":"SIGNAL\nTHROUGH",15,195,getWidth()-30,100,Justification::centred,3);}
        g.setColour(text.withAlpha(.8f));g.setFont(font(10));g.drawFittedText(routed?panel(current).description:"UNROUTED / connect to activate",12,getHeight()-37,getWidth()-24,29,Justification::centred,2);
    }
    void resized() override {
        choice.setBounds(12,35,getWidth()-24,31);
        for(size_t i=0;i<jacks.size();++i){const int w=(getWidth()-24)/std::max(1,int(jacks.size()));jacks[i]->setBounds(12+int(i)*w,77,w,53);}
        if(current.empty())return;
        const auto& ps=panel(current).parameters;const float w=float(getWidth()-24),h=float(std::max(220,getHeight()-190));
        auto put=[&](const char* id,float x,float y,float width,float height){for(size_t i=0;i<ps.size();++i)if(ps[i].id==id)knobs[i]->setBounds(12+int(x*w),147+int(y*h),int(width*w),int(height*h));};
        if(current=="spectral.adsr"){
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
