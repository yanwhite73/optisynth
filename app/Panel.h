#pragma once
#include <JuceHeader.h>
#include "../src/modular/player.h"
#include "../plugin/Source/TypingKeyboard.h"
#include <ApplicationServices/ApplicationServices.h>
#include <map>

using namespace juce;
using namespace optisynth;
namespace {
const Colour ink(0xff263338), paper(0xffe5e4dd), teal(0xff247d7d), orange(0xffb46c43);
Font font(float size) { return Font(FontOptions(size)); }
using Values=std::map<std::string,std::map<std::string,float>>;

struct KnobLook : LookAndFeel_V4 {
    void drawRotarySlider(Graphics& g,int x,int y,int width,int height,float position,
                          float start,float end,Slider&) override {
        auto r=Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(2);
        const float diameter=std::min(r.getWidth(),r.getHeight());
        r=r.withSizeKeepingCentre(diameter,diameter);
        Path arc;arc.addCentredArc(r.getCentreX(),r.getCentreY(),diameter*.5f,diameter*.5f,0,start,end,true);
        g.setColour(ink.withAlpha(.18f));g.strokePath(arc,PathStrokeType(3));
        Path filled;filled.addCentredArc(r.getCentreX(),r.getCentreY(),diameter*.5f,diameter*.5f,0,start,start+position*(end-start),true);
        g.setColour(teal);g.strokePath(filled,PathStrokeType(3));
        auto face=r.reduced(std::min(7.f,diameter*.1f));g.setColour(Colour(0xff344146));g.fillEllipse(face);
        g.setColour(Colour(0xff526066));g.drawEllipse(face.reduced(1),1);
        const float angle=start+position*(end-start),radius=face.getWidth()*.38f;
        auto c=face.getCentre();g.setColour(paper);
        g.drawLine(c.x+std::sin(angle)*radius*.5f,c.y-std::cos(angle)*radius*.5f,
                   c.x+std::sin(angle)*radius,c.y-std::cos(angle)*radius,3);
    }
};
struct Control : juce::Component {
    Label label;Slider slider;
    Control(const Parameter& p,float value,std::function<void(float)> changed,KnobLook& look) {
        label.setText(p.label,dontSendNotification);label.setFont(font(13));
        label.setColour(Label::textColourId,ink);label.setJustificationType(Justification::centred);
        slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(Slider::TextBoxBelow,false,92,22);
        slider.setColour(Slider::textBoxTextColourId,ink);
        slider.setColour(Slider::textBoxBackgroundColourId,Colours::transparentBlack);
        slider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
        slider.setRange(p.minimum,p.maximum,.0001);
        if(p.logarithmic)slider.setSkewFactorFromMidPoint(p.minimum>0?std::sqrt(p.minimum*p.maximum):.3);
        slider.setTextValueSuffix(p.unit.empty()?"":" "+String(p.unit));
        slider.setNumDecimalPlacesToDisplay(p.unit=="Hz"?1:p.unit=="st"?1:3);
        slider.setValue(value,dontSendNotification);slider.setDoubleClickReturnValue(true,p.initial);
        slider.setTooltip(p.label+" / double-click to reset");
        slider.setLookAndFeel(&look);slider.onValueChange=[this,changed]{changed(float(slider.getValue()));};
        addAndMakeVisible(label);addAndMakeVisible(slider);
    }
    ~Control() override {slider.setLookAndFeel(nullptr);}
    void resized() override {label.setBounds(0,0,getWidth(),22);slider.setBounds(0,21,getWidth(),getHeight()-21);}
};

struct ModulePanel : juce::Component {
    ComboBox choice;
    KnobLook look;
    std::vector<std::unique_ptr<Control>> knobs;
    std::vector<std::string> types;
    std::string current,family;
    std::string instance;
    bool routed=true;
    std::function<void()> selectionChanged;
    ModulePanel(std::string group):family(std::move(group)) {
        choice.setColour(ComboBox::backgroundColourId,ink);
        choice.setColour(ComboBox::textColourId,paper);
        choice.setColour(ComboBox::outlineColourId,Colours::transparentBlack);
        for(const auto& p:panels())if(family=="all"||p.family==family) {types.push_back(p.type);choice.addItem(p.name,int(types.size()));}
        addAndMakeVisible(choice);
        choice.onChange=[this]{if(selectionChanged)selectionChanged();};
    }
    std::string selected() const {int i=choice.getSelectedId()-1;return i>=0&&i<int(types.size())?types[i]:types.front();}
    void select(const std::string& type) {
        auto it=std::find(types.begin(),types.end(),type);
        if(it==types.end())throw std::invalid_argument("Wrong module family: "+type);
        choice.setSelectedId(int(it-types.begin())+1,dontSendNotification);
    }
    void show(const std::string& type,Values& state,std::shared_ptr<Controls> controls,
              std::function<void(const std::string&,float)> changed={}) {
        current=type;family=panel(type).family;routed=bool(controls);select(type);knobs.clear();
        const auto& d=panel(type);
        for(size_t i=0;i<d.parameters.size();++i) {
            auto p=d.parameters[i];auto& saved=state[type];
            if(!saved.count(p.id))saved[p.id]=p.initial;
            auto knob=std::make_unique<Control>(p,saved[p.id],[&,controls,i,type,id=p.id,changed](float v){
                state[type][id]=v;if(controls)controls->values[i].store(v,std::memory_order_relaxed);
                if(changed)changed(id,v);
            },look);
            addAndMakeVisible(*knob);knobs.push_back(std::move(knob));
        }
        resized();repaint();
    }
    void paint(Graphics& g) override {
        auto r=getLocalBounds().toFloat();g.setColour(paper);g.fillRoundedRectangle(r,7);
        g.setColour(ink.withAlpha(.2f));g.drawRoundedRectangle(r.reduced(.5f),7,1);
        for(float x:{12.f,float(getWidth()-12)})for(float y:{13.f,float(getHeight()-13)}) {
            g.setColour(ink.withAlpha(.45f));g.fillEllipse(x-3,y-3,6,6);
            g.setColour(paper);g.drawLine(x-1.5f,y+1.5f,x+1.5f,y-1.5f,.7f);
        }
        g.setColour(teal);g.setFont(font(12));
        g.drawText(String(family).toUpperCase()+"   /   "+String(instance),26,13,getWidth()-52,20,Justification::centredLeft);
        if(current.empty())return;
        g.setColour(ink.withAlpha(.8f));g.setFont(font(12));
        g.drawFittedText(panel(current).description,26,83,getWidth()-52,40,Justification::centredLeft,2);
        if(family=="source") {
            g.setColour(ink.withAlpha(.1f));g.drawLine(26,132,float(getWidth()-26),132,1);
            g.setColour(teal);g.setFont(font(12));
            g.drawText(current=="modal.voice"?"RESONATOR / MALLET":current=="drum.analog"?"DRUM / PULSE EXCITATION":"OSCILLATOR / CONTINUOUS",26,144,getWidth()-52,22,Justification::centredLeft);
        }
        if(knobs.empty()) {g.setColour(ink.withAlpha(.5f));g.setFont(font(22));g.drawText(current=="utility.vca"?"AUDIO x CONTROL":"THROUGH",getLocalBounds().reduced(25),Justification::centred);}
        g.setColour(ink.withAlpha(.12f));g.drawLine(26,float(getHeight()-72),float(getWidth()-26),float(getHeight()-72));
        StringArray ports;
        for(const auto& t:moduleRegistry())if(t.id==current){for(const auto& p:t.inputs)ports.add(String(p.id).toUpperCase());for(const auto& p:t.outputs)ports.add(String(p.id).toUpperCase());}
        for(int i=0;i<ports.size();++i) {
            const float x=35+(getWidth()-70)*float(i)/std::max(1,ports.size()-1);
            g.setColour(ink);g.fillEllipse(x-7,float(getHeight()-48),14,14);
            g.setColour((ports[i]=="CONTROL"||ports[i]=="MOD")?orange:teal);g.drawEllipse(x-9,float(getHeight()-50),18,18,2);
            g.setFont(font(10));g.setColour(ink);g.drawText(ports[i],int(x)-30,getHeight()-29,60,17,Justification::centred);
        }
        if(!routed) {
            g.setColour(orange);g.setFont(font(11));g.drawText("Not connected to the output",26,122,getWidth()-52,18,Justification::centredLeft);
        }
    }
    void resized() override {
        choice.setBounds(25,41,getWidth()-50,35);
        const int cols=(family=="envelope"&&knobs.size()>4)||(family=="source"&&knobs.size()>4)?3:2;
        const int rows=std::max(1,(int(knobs.size())+cols-1)/cols);
        int top=family=="source"?188:155;
        int h=std::min(150,(getHeight()-top-85)/rows),w=(getWidth()-40)/cols;
        for(size_t i=0;i<knobs.size();++i)knobs[i]->setBounds(20+int(i)%cols*w,top+int(i)/cols*h,w,h);
    }
};

}
