#pragma once
#include <JuceHeader.h>
#include "../src/modular/player.h"
#include "../plugin/Source/TypingKeyboard.h"
#include <ApplicationServices/ApplicationServices.h>
#include <map>
#include <set>
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
    if(id=="analog.dual_lpg")return Colour(0xff30383b);
    if(id=="fx.prism_tank")return Colour(0xffabb8d7);
    if(id=="fx.dual_loop")return Colour(0xffb5c1be);
    if(id=="mod.lfo")return Colour(0xffd7b282);
    if(id=="modal.voice")return Colour(0xffdac9bd);
    if(id=="modal.resonator")return Colour(0xffbecdc2);
    if(id=="drum.analog")return Colour(0xffd7b282);
    if(id.rfind("osc.",0)==0||id=="analog.vco")return Colour(0xff8ebecb);
    if(id=="voice.mix")return Colour(0xffc9a96a);
    if(id.rfind("fx.",0)==0)return Colour(0xff6f8f8b);
    if(id=="mod.wheel"||id=="analog.polymod")return Colour(0xffd7b282);
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
    if(id=="analog.vca"||id=="mod.wheel"||id=="analog.polymod")return 170;
    if(id=="voice.mix")return 280;
    if(id=="utility.vca"||id=="filter.bypass")return 155;
    if(id=="utility.mix"||id=="osc.sine")return 200;
    return id=="modal.resonator"?270:250;
}
// Request 72: layout uses JUCE Grid and FlexBox. Text sizes are fixed so they stay
// readable at every density; density changes knob size and spacing only.
namespace text { constexpr float label=11.f,value=12.5f,jack=10.f,header=10.5f,footer=10.f; }
struct KnobLook : LookAndFeel_V4 {
    Colour markings=ink;
    // Value text: fixed size, no inner padding (the layout reserves its full width), never squashed.
    Label* createSliderTextBox(Slider& slider) override {auto* l=LookAndFeel_V4::createSliderTextBox(slider);l->setFont(font(text::value));l->setBorderSize({0,0,0,0});l->setMinimumHorizontalScale(1.f);return l;}
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
        auto r=Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(2);
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
// Request 120: movement on a parameter (docs/ui/PARAMETER_MOVEMENT.md, Placement A).
// A source keeps one colour wherever it moves a control, so shared motion is visible.
inline Colour movementColour(int number){
    static const uint32 colours[]={0xff5fd0b5,0xfff2a65a,0xffb28cf2,0xfff27d9b,0xffa3d65c,0xff6cb4f5,0xffe9d25e,0xffef8a5b};
    if(number<=8)return Colour(colours[std::max(0,number-1)]);
    return Colour::fromHSV(float(std::fmod((number-9)*0.61803398875+.58,1.0)),.74f,.94f,1.f);
}
// Movement created from a parameter owns its LFO; the id prefix marks that ownership in saved patches.
inline bool privateMovementSource(const std::string& id){return id.rfind("move_",0)==0;}
struct MovementSample {float offset, phase, rate;};
struct MovementLink {
    std::string source;          // node id of the moving source
    int shape=0;
    int number=0;                // LFO n, counted in patch order
    int route=-1;                // matrix route index; -1 when a CV cable carries it
    float amount=0;              // the route's signed amount (a fraction of the normalised range)
    float sourceDepth=1;         // the LFO's own Depth
    bool lfo=false,bypassed=false;
    std::vector<Endpoint> destinations;   // every parameter or input this source moves
    Colour colour() const {return lfo?movementColour(number):Colour(0xffc2c3bd);}
    String name() const {return lfo?"LFO "+String(number):String(source);}
};
// The matrix acts on the base value; modal cable CV is added after that clamp.
inline std::pair<float,float> movementRange(const Parameter& p,float base,const std::vector<MovementLink>& links){
    float matrix=0,cable=0;
    for(const auto& link:links)if(!link.bypassed){
        const float extent=std::abs(link.amount*link.sourceDepth);
        if(link.route<0)cable+=extent;else matrix+=extent;
    }
    return {std::clamp(movementValue(p,base,-matrix)-cable,p.minimum,p.maximum),
            std::clamp(movementValue(p,base,matrix)+cable,p.minimum,p.maximum)};
}
// Modal modules also take movement through their CV inputs (the 0.0.22 cables).
inline std::string modalCvPort(const std::string& model,const std::string& param){
    if(model!="modal.voice"&&model!="modal.resonator")return {};
    return param=="structure"?"mat":param=="brightness"?"bright":param=="decay"?"decay":"";
}
inline float nodeParameter(const GraphSpec& graph,const std::string& node,const std::string& key,float fallback){
    for(const auto& n:graph.nodes)if(n.id==node)for(const auto& [k,v]:n.parameters)if(k==key)return v;
    return fallback;
}
inline std::vector<MovementLink> movementLinks(const GraphSpec& graph,const std::string& model,const std::string& node,const std::string& param){
    std::vector<MovementLink> links;
    auto describe=[&](const std::string& source,int route,float amount){
        MovementLink link;link.source=source;link.route=route;link.amount=amount;int ordinal=0;
        for(const auto& n:graph.nodes){
            if(n.type=="mod.lfo")++ordinal;
            if(n.id==source){link.lfo=n.type=="mod.lfo";link.number=ordinal;break;}
        }
        if(link.lfo){link.sourceDepth=nodeParameter(graph,source,"depth",1);link.shape=int(nodeParameter(graph,source,"wave",0));}
        for(const auto& m:graph.modulations)if(m.source.node==source)link.destinations.push_back(m.destination);
        for(const auto& c:graph.connections)if(c.source.node==source)link.destinations.push_back(c.destination);
        links.push_back(std::move(link));
    };
    for(size_t i=0;i<graph.modulations.size();++i){
        const auto& m=graph.modulations[i];
        if(m.destination.node==node&&m.destination.port==param)describe(m.source.node,int(i),m.amount);
    }
    if(const auto port=modalCvPort(model,param);!port.empty())
        for(const auto& c:graph.connections)if(c.destination.node==node&&c.destination.port==port)describe(c.source.node,-1,1);
    return links;
}
// Continuous parameters can move. Stepped choices, switches and structural settings cannot.
inline bool movableParameter(const std::string& model,const Parameter& p){
    return p.choices.empty() && p.step<1 && model!="spectral.engine" && !isSequencer(model)
        && model!="seq.performance" && p.maximum>p.minimum;
}
// Quiet outlined wave when free; filled in the source colour once something moves the control.
struct MovementButton final : Button {
    Colour legend=paper,fill=teal;bool assigned=false;int sources=0,number=0,shape=0;float phase=-1;
    MovementButton():Button("Movement"){setWantsKeyboardFocus(true);}
    void paintButton(Graphics& g,bool over,bool down) override {
        auto r=getLocalBounds().toFloat().reduced(1.5f,1.f);
        const float radius=r.getHeight()*.5f;
        if(assigned){g.setColour(fill.withMultipliedBrightness(down?.85f:1.f));g.fillRoundedRectangle(r,radius);}
        else{g.setColour(legend.withAlpha(over||down?.95f:.5f));g.drawRoundedRectangle(r,radius,1.f);}
        Path wave;const auto x0=r.getX()+4,w=r.getWidth()-8-(assigned&&number>0?15.f:sources>1?5.f:0.f),y=r.getCentreY(),h=r.getHeight()*.26f;
        auto waveY=[&](float t){
            const float v=shape==1?1.f-4.f*std::abs(t-.5f):shape==2?2*t-1:shape==3?(t<.5f?1.f:-1.f):std::sin(t*MathConstants<float>::twoPi);
            return y-v*h;
        };
        wave.startNewSubPath(x0,waveY(0));for(int i=1;i<=20;++i){const float t=float(i)/20;wave.lineTo(x0+t*w,waveY(t));}
        const auto mark=assigned?(fill.getPerceivedBrightness()>.55f?ink:paper):legend.withAlpha(over||down?1.f:.7f);
        g.setColour(mark);g.strokePath(wave,PathStrokeType(1.4f));
        if(assigned&&number>0){
            g.setFont(font(10.f).boldened());
            g.drawText(String(number),r.toNearestInt().reduced(3,0),Justification::centredRight);
            if(phase>=0){
                const float px=x0+phase*w, py=waveY(phase);
                g.setColour(ink);g.fillEllipse(px-2.8f,py-2.8f,5.6f,5.6f);
                g.setColour(Colours::white);g.fillEllipse(px-1.8f,py-1.8f,3.6f,3.6f);
            }
        }
        if(sources>1&&number==0){g.fillEllipse(r.getRight()-6.5f,y-3.5f,2.5f,2.5f);g.fillEllipse(r.getRight()-6.5f,y+1,2.5f,2.5f);}
        if(hasKeyboardFocus(false)){g.setColour(orange);g.drawRoundedRectangle(r.expanded(1),radius+1,1.2f);}
    }
};
// One parameter: label, knob (or trim slider / switch) and value, stacked tightly.
struct Control : juce::Component {
    static constexpr int labelHeight=15,valueHeight=18;
    Label label;Slider slider;ToggleButton toggle;bool isSwitch=false;
    Parameter param;
    Control(const Parameter& p,float value,std::function<void(float)> changed,KnobLook& look,Colour text,bool fader=false,bool compactTrim=false):param(p) {
        label.setText(String(p.label).toUpperCase(),dontSendNotification);label.setFont(font(text::label));label.setColour(Label::textColourId,text);label.setJustificationType(Justification::centred);
        label.setBorderSize({0,0,0,0});label.setMinimumHorizontalScale(.85f);
        const bool trim=compactTrim||p.id.find("_curve")!=std::string::npos||p.id=="fade"||p.id=="mod_rate_hz"||p.id=="mod_depth"||p.id=="start"||p.id=="end";
        slider.setScrollWheelEnabled(false);
        slider.setSliderStyle(fader?Slider::LinearVertical:trim?Slider::LinearHorizontal:Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(Slider::TextBoxBelow,false,80,valueHeight);
        slider.setColour(Slider::textBoxTextColourId,text);slider.setColour(Slider::textBoxBackgroundColourId,Colours::transparentBlack);
        slider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);slider.setColour(Slider::trackColourId,teal);
        slider.setColour(Slider::thumbColourId,paper);slider.setColour(Slider::backgroundColourId,Colour(0xff151718));
        slider.setRange(p.minimum,p.maximum,p.step);
        if(p.logarithmic)slider.setSkewFactorFromMidPoint(p.minimum>0?std::sqrt(p.minimum*p.maximum):.3);
        slider.setTextValueSuffix(p.unit.empty()?"":" "+String(p.unit));slider.setNumDecimalPlacesToDisplay(p.step>=1?0:p.unit=="Hz"?1:p.unit=="st"?2:3);
        if(p.id=="root_note")slider.setTextValueSuffix(" MIDI");
        if(p.unit=="Hz")slider.textFromValueFunction=[](double v){return String(v,v<1?3:v<10?2:1);};   // slow LFOs read 0.030 Hz (suffix added by Slider)
        if(!p.choices.empty()){
            slider.setTextValueSuffix("");
            slider.textFromValueFunction=[choices=p.choices](double v){return String(choices[size_t(std::clamp(int(std::lround(v)),0,int(choices.size())-1))]);};
            slider.valueFromTextFunction=[choices=p.choices](const String& t){for(size_t i=0;i<choices.size();++i)if(t.equalsIgnoreCase(String(choices[i])))return double(i);return t.getDoubleValue();};
        }
        if(p.id=="wave"){
            slider.textFromValueFunction=[](double v){const char* names[]={"Sine","Triangle","Ramp","Pulse"};return String(names[std::clamp(int(std::round(v)),0,3)]);};
            slider.valueFromTextFunction=[](const String& t){return t.equalsIgnoreCase("Triangle")?1.:t.equalsIgnoreCase("Ramp")?2.:t.equalsIgnoreCase("Pulse")?3.:0.;};
        }
        slider.setLookAndFeel(&look);
        slider.setValue(value,dontSendNotification);slider.updateText();slider.setDoubleClickReturnValue(true,p.initial);
        slider.setTooltip(p.label+" / double-click to reset");
        slider.onValueChange=[this,changed]{toggle.setToggleState(slider.getValue()>=.5,dontSendNotification);changed(float(slider.getValue()));};addAndMakeVisible(label);addAndMakeVisible(slider);
        const bool loopSwitch=p.id=="freeze"||p.id=="a_hold"||p.id=="b_hold"||p.id=="a_reverse"||p.id=="b_reverse";
        if(loopSwitch||p.id=="loop"||p.id=="retrigger"){
            isSwitch=true;label.setVisible(false);slider.setVisible(false);addAndMakeVisible(toggle);toggle.setButtonText(loopSwitch?String(p.label):p.id=="loop"?"Loop while held":"Reset on note");
            toggle.setColour(ToggleButton::textColourId,text);toggle.setColour(ToggleButton::tickColourId,text);toggle.setColour(ToggleButton::tickDisabledColourId,text.withAlpha(.5f));
            toggle.setToggleState(value>=.5f,dontSendNotification);toggle.onClick=[this]{slider.setValue(toggle.getToggleState()?1:0,sendNotificationSync);};
        }
    }
    // Movement: the button, the sources reaching this parameter, and their extent arcs.
    MovementButton movement;bool movementAvailable=false;int badgeWidth=0;
    std::vector<MovementLink> links;
    std::function<void(Control&)> movementClicked;
    std::function<std::optional<MovementSample>(const MovementLink&)> liveOffset;   // a route's current normalised contribution
    std::optional<float> live;float cableOffset=0;
    void bindMovement(const std::string& model,const std::string& node,const GraphSpec& graph){
        const bool available=!isSwitch && slider.getSliderStyle()==Slider::RotaryHorizontalVerticalDrag && movableParameter(model,param);
        links=available?movementLinks(graph,model,node,param.id):std::vector<MovementLink>{};
        const String name=String(param.label);
        movement.legend=label.findColour(Label::textColourId);movement.assigned=!links.empty();movement.sources=int(links.size());
        movement.shape=links.empty()?0:links.front().shape;
        movement.number=links.empty()||!links.front().lfo?0:links.front().number;
        movement.phase=-1;
        if(!links.empty())movement.fill=links.front().colour();
        String tip;
        if(links.empty())tip="Add movement to "+name+": a free-running LFO, edited here";
        else{
            tip=name+" movement";
            for(const auto& link:links){
                tip+="\n"+link.name()+(link.route<0?String(" (CV cable)"):" "+String(link.amount>=0?"+":"")+String(link.amount,2));
                if(link.destinations.size()>1)tip+=" / shared by "+String(int(link.destinations.size()))+" controls";
            }
            tip+="\nClick to edit";
        }
        movement.setTooltip(tip);movement.setTitle(links.empty()?"Add movement to "+name:"Edit "+name+" movement");
        movement.onClick=[this]{if(movementClicked)movementClicked(*this);};
        if(available!=movementAvailable){
            movementAvailable=available;badgeWidth=available?38:0;
            if(available)addAndMakeVisible(movement);else movement.setVisible(false);
            resized();
        }
        if(links.empty())live.reset();
        movement.repaint();repaint();
    }
    void tickMovement(){
        std::optional<float> next;
        if(!links.empty()&&liveOffset){
            float sum=0,cable=0;bool any=false;
            float phase=-1;
            for(const auto& link:links)if(auto v=liveOffset(link)){
                if(link.route<0)cable+=v->offset;else sum+=v->offset;
                any=true;
                if(&link==&links.front())phase=v->phase;
            }
            if(movement.phase!=phase){movement.phase=phase;movement.repaint();}
            if(any)next=sum;
            if(cableOffset!=cable){cableOffset=cable;repaint();}
        }
        if(next!=live){live=next;repaint();}
    }
    // The value the engine reaches for a normalised offset (Runtime::computeParameters).
    float modulatedValue(float base,float offset) const {
        return movementValue(param,base,offset);
    }
    float knobProportion(float v) const {return float(const_cast<Slider&>(slider).valueToProportionOfLength(v));}
    struct KnobGeometry{juce::Point<float> centre;float radius,start,end;};
    std::optional<KnobGeometry> knobGeometry() const {
        if(!slider.isVisible()||slider.getSliderStyle()!=Slider::RotaryHorizontalVerticalDrag)return {};
        auto& s=const_cast<Slider&>(slider);
        auto r=s.getLookAndFeel().getSliderLayout(s).sliderBounds.toFloat().translated(float(s.getX()),float(s.getY())).reduced(2);
        const float d=std::min(r.getWidth(),r.getHeight());r=r.withSizeKeepingCentre(d,d);
        const auto rotary=s.getRotaryParameters();
        return KnobGeometry{r.getCentre(),d*.5f,rotary.startAngleRadians,rotary.endAngleRadians};
    }
    void paintMovement(Graphics& g){
        const auto k=knobGeometry();if(!k||links.empty())return;
        const float base=float(slider.getValue());int ring=0;
        auto angle=[&](float proportion){return k->start+std::clamp(proportion,0.f,1.f)*(k->end-k->start);};
        auto arcFor=[&](float low,float high,Colour colour){
            const float lo=knobProportion(low),hi=knobProportion(high);
            if(hi<=lo)return;
            const float radius=k->radius-1.5f-ring*3.2f;
            Path arc;arc.addCentredArc(k->centre.x,k->centre.y,radius,radius,0,angle(lo),angle(hi),true);
            g.setColour(Colours::black.withAlpha(.35f));g.strokePath(arc,PathStrokeType(4.f,PathStrokeType::curved,PathStrokeType::rounded));
            g.setColour(colour);g.strokePath(arc,PathStrokeType(2.6f,PathStrokeType::curved,PathStrokeType::rounded));
            ++ring;
        };
        if(links.size()>1){
            const auto range=movementRange(param,base,links);
            arcFor(range.first,range.second,Colours::white);
        }
        for(const auto& link:links){
            if(link.bypassed||ring>=4)continue;
            const float extent=std::abs(link.amount*link.sourceDepth);
            arcFor(modulatedValue(base,-extent),modulatedValue(base,extent),link.colour());
        }
        if(live){
            const float a=angle(knobProportion(std::clamp(modulatedValue(base,*live)+cableOffset,param.minimum,param.maximum))),radius=k->radius-1.5f;
            const juce::Point<float> p{k->centre.x+std::sin(a)*radius,k->centre.y-std::cos(a)*radius};
            g.setColour(ink);g.fillEllipse(p.x-3.4f,p.y-3.4f,6.8f,6.8f);
            g.setColour(Colours::white);g.fillEllipse(p.x-2.3f,p.y-2.3f,4.6f,4.6f);
        }
    }
    enum class Activity { unspecified, ready, waiting };
    Activity activity=Activity::unspecified;
    static void drawActivityLamp(Graphics& g,Rectangle<float> bounds,bool ready,Colour legend){
        if(ready){
            g.setColour(Colour(0xff244a3e));g.fillEllipse(bounds);
            g.setColour(Colour(0xffb9f29a));g.fillEllipse(bounds.reduced(1.4f));
        }else{
            g.setColour(legend.withAlpha(.85f));g.drawEllipse(bounds.reduced(.8f),1.4f);
        }
    }
    void showActivity(const String& reason){
        activity=reason.isEmpty()?Activity::ready:Activity::waiting;
        slider.setAlpha(activity==Activity::ready?1.f:.55f);
        const auto tip=label.getText()+" — "+(reason.isEmpty()?String("Ready"):reason);
        label.setTooltip(tip);slider.setTooltip(tip+" / double-click to reset");
        repaint();
    }
    void paintOverChildren(Graphics& g) override {
        paintMovement(g);
        if(activity!=Activity::unspecified)
            drawActivityLamp(g,{3.f,float(labelHeight+2),8.f,8.f},activity==Activity::ready,label.findColour(Label::textColourId));
    }
    String patchedFmLabel;
    void showFilterFmSource(const GraphSpec& graph,const std::string& node){
        const auto link=std::find_if(graph.connections.begin(),graph.connections.end(),[&](const auto& c){
            return c.destination.node==node && c.destination.port=="fm";
        });
        const bool patched=link!=graph.connections.end();
        const String tip=patched?"Filter FM source: "+String(link->source.node)+"."+String(link->source.port)
                                :"Patch an audio output to the OB filter FM input in Modular.";
        if(patchedFmLabel.isEmpty())patchedFmLabel=label.getText();
        label.setText(patched?patchedFmLabel:"FM / NO INPUT",dontSendNotification);
        label.setTooltip(tip);slider.setTooltip(tip);slider.setEnabled(patched);setAlpha(patched?1.f:.6f);
    }
    void setValue(float v){slider.setValue(v,dontSendNotification);toggle.setToggleState(v>=.5f,dontSendNotification);}
    ~Control() override {slider.setLookAndFeel(nullptr);}
    // Widest text this control must show: its label and its current value.
    // Covers the range ends, every stepped value (choices) and the current value, so text never clips.
    int textWidth() const {
        if(isSwitch)return int(std::ceil(GlyphArrangement::getStringWidth(font(text::label),toggle.getButtonText())))+28;
        float w=GlyphArrangement::getStringWidth(font(text::label),label.getText())+float(badgeWidth);
        auto& s=const_cast<Slider&>(slider);   // getTextFromValue is not const in JUCE; it does not modify the slider
        auto measure=[&](double v){w=std::max(w,GlyphArrangement::getStringWidth(font(text::value),s.getTextFromValue(v)));};
        const auto range=slider.getRange();measure(range.getStart());measure(range.getEnd());measure(slider.getValue());
        if(slider.getInterval()>=1&&range.getLength()<=16)for(double v=range.getStart();v<=range.getEnd();v+=slider.getInterval())measure(v);
        return int(std::ceil(w))+4;   // a little air either side so text never touches the next column
    }
    void resized() override {
        if(isSwitch){
            toggle.setBounds(getLocalBounds());
            return;
        }
        if(slider.getTextBoxWidth()!=getWidth())slider.setTextBoxStyle(Slider::TextBoxBelow,false,getWidth(),valueHeight);
        const int buttonWidth=badgeWidth;
        if(movementAvailable)movement.setBounds(getWidth()-buttonWidth,0,buttonWidth,labelHeight);
        label.setBounds(0,0,getWidth()-buttonWidth,labelHeight);slider.setBounds(0,labelHeight-1,getWidth(),getHeight()-labelHeight+1);
    }
};
struct Jack : Button {
    static constexpr int size=38;
    Port port;bool output=false,connected=false;Colour text=paper;
    Jack(Port p,bool out):Button(String(p.id)+(out?" output":" input")),port(std::move(p)),output(out){}
    juce::Point<int> socket() const {return {getWidth()/2,12};}
    std::function<void(Jack&, juce::Point<int>, bool)> cableDrag;
    std::function<void(Jack*)> cableHover;
    bool pulling=false;
    void mouseDrag(const MouseEvent& e) override {
        Button::mouseDrag(e);
        if(cableDrag && e.mods.isLeftButtonDown() && !e.mods.isPopupMenu() &&
           (pulling || e.getDistanceFromDragStart()>=5)){
            pulling=true;
            beginDragAutoRepeat(55);
            cableDrag(*this,e.getScreenPosition(),false);
        }
    }
    void mouseUp(const MouseEvent& e) override {
        Button::mouseUp(e); // onClick checks pulling while the button releases its state.
        if(pulling){
            pulling=false;
            cableDrag(*this,e.getScreenPosition(),true);
        }
    }
    void mouseEnter(const MouseEvent& e) override {Button::mouseEnter(e);if(cableHover)cableHover(this);}
    void mouseExit(const MouseEvent& e) override {Button::mouseExit(e);if(cableHover)cableHover(nullptr);}
    void paintButton(Graphics& g,bool over,bool down) override {
        const auto centre=socket();const float cx=float(centre.x),cy=float(centre.y);
        if(output){g.setColour(text);g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1),4);}
        g.setColour(Colour(0xff16181a));g.fillEllipse(cx-10,cy-10,20,20);
        g.setColour(over||down?Colour(0xfff0c76c):Colour(0xffc2c3bd));g.drawEllipse(cx-8.5f,cy-8.5f,17,17,2.4f);
        g.setColour(connected?(port.type==SignalType::Control?Colour(0xfff0c76c):teal):Colour(0xff080a0c));g.fillEllipse(cx-4,cy-4,8,8);
        g.setColour(output?(text==ink?paper:ink):text);g.setFont(font(text::jack));g.drawFittedText(String(port.id).toUpperCase(),0,23,getWidth(),13,Justification::centred,1,.8f);
        if(output){Path arrow;arrow.addTriangle(cx+10,9,cx+10,15,cx+14,12);g.fillPath(arrow);}
    }
};
// Rows follow the playing gesture. Diameter is expressed as a percentage of rack density.
struct FaceRow { const char* legend; int diameterPercent; std::vector<std::string> parameters; };
using Faceplate=std::vector<FaceRow>;
inline const Faceplate* faceplate(const std::string& type) {
    static const std::map<std::string,Faceplate> plates{
        {"analog.dual_lpg",{{"Gate opening",145,{"a_control","b_control"}},{"Damping",95,{"a_damp","b_damp"}},
                             {"Key strike",70,{"a_keys","b_keys"}}}},
        {"fx.dual_loop",{{"Channel clocks",145,{"a_time","b_time"}},
                         {"Loop A",85,{"a_feed","a_feedback","a_mix"}},{"Loop B",85,{"b_feed","b_feedback","b_mix"}},
                         {"Playback",70,{"a_hold","a_reverse","b_hold","b_reverse"}}}},
        {"modal.voice",{{"Struck body",145,{"structure","decay"}},{"Strike colour",90,{"transpose","brightness"}}}},
        {"modal.resonator",{{"Resonant body",145,{"transpose","structure"}},{"Response",95,{"brightness","decay"}},{"Input trim",65,{"input_gain"}}}},
        {"drum.analog",{{"Drum head",145,{"transpose","decay"}},{"Attack",80,{"tone","attack_fm","self_fm"}}}},
        {"osc.variable",{{"Waveform",145,{"transpose","shape"}},{"Pulse and sync",85,{"pulse_width","sync","level"}}}},
        {"osc.sine",{{"Frequency",145,{"transpose"}},{"Output level",80,{"level"}}}},
        {"fx.prism_tank",{{"Space",135,{"size","decay","diffusion","freeze"}},
                          {"Echo",90,{"sync","predelay","feedback","echo"}},
                          {"Pitch and motion",80,{"pitch","shift","rate","motion"}},
                          {"Return",80,{"tone","tank","mix"}}}}
    };
    const auto it=plates.find(type);
    return it==plates.end()?nullptr:&it->second;
}
// Parameter rows per module, read left to right. Modules without an entry flow into rows of three.
inline std::vector<std::vector<std::string>> controlRows(const std::string& type) {
    static const std::map<std::string,std::vector<std::vector<std::string>>> rows{
        {"analog.vco",{{"transpose","fine","drift"},{"saw","pulse","triangle"},{"pulse_width","pwm_depth","fm_depth","level"}}},
        {"analog.filter",{{"cutoff_hz","resonance","env_octaves","key_track"},{"mode","drive","fm_depth","quality"},{"band","poles","pole_mix"}}},
        {"analog.env",{{"attack","decay","sustain"},{"release","velocity"}}},
        {"voice.mix",{{"voices","unison","detune"},{"glide","spread","level"},{"tuning","root","steal","keyboard"}}},
        {"spectral.adsr",{{"attack","decay"},{"sustain","release"}}},
        {"spectral.ahdsfr",{{"attack","hold","decay"},{"sustain","release","fade"},{"attack_curve","decay_curve","release_curve"}}},
                                {"mod.lfo",{{"rate","depth"},{"wave","retrigger"}}},
    };
    const auto& ps=panel(type).parameters;std::vector<std::vector<std::string>> out;std::set<std::string> used;
    if(auto it=rows.find(type);it!=rows.end())
        for(const auto& row:it->second){std::vector<std::string> kept;for(const auto& id:row)if(std::any_of(ps.begin(),ps.end(),[&](const Parameter& x){return x.id==id;})){kept.push_back(id);used.insert(id);}if(!kept.empty())out.push_back(kept);}
    std::vector<std::string> rest;for(const auto& x:ps)if(!used.count(x.id))rest.push_back(x.id);
    const size_t width=rest.size()==4?2:3;
    for(size_t i=0;i<rest.size();i+=width)out.emplace_back(rest.begin()+long(i),rest.begin()+long(std::min(rest.size(),i+width)));
    return out;
}
struct ModulePanel : juce::Component, FileDragAndDropTarget {
    static constexpr int pad=8,gap=4,headerHeight=50,footerHeight=16;
    std::unique_ptr<optiui::SequencerEditor> sequence;
    std::unique_ptr<optiui::PerformanceLauncher> performance;
    std::unique_ptr<optiui::ReferenceSequenceLauncher> referenceSequence;
    ComboBox choice;KnobLook look;std::vector<std::unique_ptr<Control>> knobs;
    ComboBox section;TextButton presetButton{"Load Spectral patch..."},more{"More"};Label presetName;int subpage=0;
    std::function<void()> choosePreset;std::vector<std::vector<int>> pages;
    std::vector<std::unique_ptr<Jack>> jacks;std::vector<std::string> types;
    std::string current,family,instance;bool routed=true,highlighted=false;
    std::function<void()> selectionChanged,activated,rowMenu;
    std::function<void(const Port&,bool,juce::Component*)> portClicked;
    TextButton sampleButton{"Load WAV..."},menuButton{String::fromUTF8("\xe2\x8b\xaf")};Label sampleName,strokeRole;SamplePtr sample;std::string samplePath;
    std::function<void()> chooseSample;std::function<void(const File&)> sampleDropped;
    int knob=44;   // density: knob diameter in pixels
    Rectangle<int> content,waveArea;int dividerY=0,jackTop=headerHeight;
    const Faceplate* plate=nullptr;
    static constexpr int legendHeight=16;
    int rowDiameter(const FaceRow& row) const {return knob*row.diameterPercent/100;}
    int faceRowHeight(const FaceRow& row) const {return legendHeight+Control::labelHeight+Control::valueHeight+rowDiameter(row);}
    int faceCellWidth(const std::string& id,const FaceRow& row) const {
        return std::max(control(id)->textWidth()+8,rowDiameter(row)+12);
    }
    ModulePanel(std::string group):family(std::move(group)) {
        choice.setColour(ComboBox::backgroundColourId,Colour(0xff232527));choice.setColour(ComboBox::textColourId,paper);
        choice.setColour(ComboBox::outlineColourId,Colours::transparentBlack);
        for(const auto& p:panels())if(family=="all"||p.family==family){types.push_back(p.type);choice.addItem(p.name,int(types.size()));}
        addAndMakeVisible(choice);choice.onChange=[this]{if(selectionChanged)selectionChanged();};addMouseListener(this,true);
        addChildComponent(menuButton);menuButton.setTooltip("Rows: start a new row here, move this module left or right");menuButton.onClick=[this]{if(rowMenu)rowMenu();};
        addChildComponent(sampleButton);addChildComponent(sampleName);addChildComponent(strokeRole);strokeRole.setFont(font(text::label));strokeRole.setColour(Label::textColourId,ink);sampleButton.onClick=[this]{if(chooseSample)chooseSample();};
        sampleName.setFont(font(text::label));sampleName.setColour(Label::textColourId,ink);
        addChildComponent(section);addChildComponent(presetButton);addChildComponent(presetName);addChildComponent(more);
        more.onClick=[this]{++subpage;resized();repaint();};
        presetButton.onClick=[this]{if(choosePreset)choosePreset();};section.onChange=[this]{subpage=0;resized();repaint();};
        presetName.setFont(font(12));presetName.setColour(Label::textColourId,ink);
    }
    ~ModulePanel() override {removeMouseListener(this);}
    void mouseDown(const MouseEvent& e) override {
        if(activated && !dynamic_cast<Jack*>(e.eventComponent))activated();
        if(e.mods.isPopupMenu()&&e.eventComponent==this&&rowMenu)rowMenu();   // right-click on the card body or header
    }
    bool isInterestedInFileDrag(const StringArray& files) override {return current=="sample.player"&&files.size()==1&&files[0].endsWithIgnoreCase(".wav");}
    void filesDropped(const StringArray& files,int,int) override {
        if(!isInterestedInFileDrag(files))return;
        // Loading can replace this panel. Dispatch after the drag callback returns.
        auto callback=sampleDropped;auto file=File(files[0]);MessageManager::callAsync([safe=SafePointer<ModulePanel>(this),callback,file]{if(safe&&callback)callback(file);});
    }
    void setSample(const std::string& path,SamplePtr data){samplePath=path;sample=std::move(data);
        const String name=path.empty()?"Drop a WAV here":path=="builtin:click"?"Built-in percussive click":File(String(path)).getFileName();
        strokeRole.setText(sample?"Suggested: "+String(sample->strike.role):String(),dontSendNotification);
        if(sample){
            const auto& profile=sample->strike;
            String details=String(profile.character);
            if(profile.durationMs>0)details+="\nAttack to peak "+String(profile.attackMs,0)+" ms / active span "+String(profile.durationMs,0)+" ms";
            if(profile.bodyHz>0)details+="\nEstimated ringing body "+String(profile.bodyHz,0)+" Hz";
            strokeRole.setTooltip(details);
        }else strokeRole.setTooltip({});
        sampleName.setText(name,dontSendNotification);sampleName.setTooltip(String(path)+(sample?"\n"+String(sample->mono.size()/sample->rate,2)+" s / "+String(sample->sourceChannels)+" channel(s) to mono\nWAV files stay external to the saved patch.":"\nSample loads when this module is connected."));repaint();
    }
    std::string selected() const {int i=choice.getSelectedId()-1;return i>=0&&i<int(types.size())?types[size_t(i)]:types.front();}
    void select(const std::string& type) {auto it=std::find(types.begin(),types.end(),type);if(it==types.end())throw std::invalid_argument("Wrong module family: "+type);choice.setSelectedId(int(it-types.begin())+1,dontSendNotification);}
    void show(const std::string& type,Values& state,std::shared_ptr<Controls> controls,std::function<void(const std::string&,float)> changed={}) {
        current=type;plate=faceplate(type);family=panel(type).family;routed=bool(controls);select(type);sequence.reset();performance.reset();referenceSequence.reset();knobs.clear();jacks.clear();look.markings=panelInk(type);
        sampleButton.setVisible(type=="sample.player");sampleName.setVisible(type=="sample.player");strokeRole.setVisible(type=="sample.player");
        const bool spectral=type=="spectral.engine";section.setVisible(spectral);presetButton.setVisible(spectral);presetName.setVisible(spectral);more.setVisible(spectral);subpage=0;
        const auto& d=panel(type);
        for(size_t i=0;i<d.parameters.size();++i){auto p=d.parameters[i];auto& saved=state[type];if(!saved.count(p.id))saved[p.id]=p.initial;
            if(isSequencer(type)||type=="seq.performance"||isReferenceSequencer(type))continue;
            auto knobControl=std::make_unique<Control>(p,saved[p.id],[&,controls,i,type,id=p.id,changed](float v){state[type][id]=v;if(controls)controls->values[i].store(v,std::memory_order_relaxed);if(changed)changed(id,v);},look,panelInk(type),false,family=="filter"&&type.rfind("spectral.",0)==0&&p.id=="env_octaves");
            addAndMakeVisible(*knobControl);knobs.push_back(std::move(knobControl));}
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
                jack->onClick=[this,p,output,ptr]{if(!ptr->pulling&&portClicked)portClicked(p,output,ptr);};addAndMakeVisible(*jack);jacks.push_back(std::move(jack));};
            for(const auto& p:t.inputs)add(p,false);for(const auto& p:t.outputs)add(p,true);}
        menuButton.setVisible(bool(rowMenu));
        spectralTextWidth=0;if(spectral)for(const auto& k:knobs)spectralTextWidth=std::max(spectralTextWidth,k->textWidth());
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
    // Movement on a parameter opens its anchored editor; liveOffset feeds the knob's effective-value tick.
    std::function<void(const std::string&,const std::string&)> openMovement;
    std::function<std::optional<MovementSample>(const MovementLink&)> movementOffset;
    void bindMovement(const GraphSpec& graph){
        for(auto& k:knobs){
            k->movementClicked=[this](Control& c){if(openMovement)openMovement(instance,c.param.id);};
            k->liveOffset=movementOffset;k->bindMovement(current,instance,graph);
        }
    }
    void tickMovement(){for(auto& k:knobs)k->tickMovement();}
    void setConnections(const GraphSpec& graph) {
        bindMovement(graph);
        if(current=="analog.filter")if(auto* fm=control("fm_depth"))fm->showFilterFmSource(graph,instance);
        for(auto& j:jacks){j->connected=false;String links;
            for(const auto& c:graph.connections){const auto& e=j->output?c.source:c.destination;if(e.node==instance&&e.port==j->port.id){j->connected=true;const auto& other=j->output?c.destination:c.source;links+="\n"+String(other.node)+"."+String(other.port);}}
            if(j->output)for(const auto& c:graph.modulations)if(c.source.node==instance&&c.source.port==j->port.id){j->connected=true;links+="\nModulates "+String(c.destination.node)+"."+String(c.destination.port);}
            if(j->output)for(const auto& e:graph.outputs)if(e.node==instance&&e.port==j->port.id){j->connected=true;links+="\nInstrument output";}
            j->setTooltip(String(instance)+"."+String(j->port.id)+(j->output?" / output":" / input")+(links.isEmpty()?"\nNot connected":links)+(j->cableDrag?"\nDrag a patched input to move its cable; drop on empty rack space to unplug. Click for routing.":"\nClick to patch"));j->repaint();}
    }
    bool sequencerWindowOpen() const {
        return (referenceSequence && referenceSequence->isEditorOpen())
            || (performance && performance->isEditorOpen()) || (sequence && sequence->isEditorOpen());
    }
    void openSequencerWindow() {
        if (referenceSequence) referenceSequence->openEditor();
        else if (performance) performance->openEditor();
        else if (sequence) sequence->openPerformanceEditor();
    }
    void setValue(const std::string& id,float value){if(referenceSequence){referenceSequence->setValue(id,value);return;}if(performance){performance->setValue(id,value);return;}if(sequence){sequence->setValue(id,value);return;}const auto& ps=panel(current).parameters;for(size_t i=0;i<ps.size();++i)if(ps[i].id==id)knobs[i]->setValue(value);if(current=="sample.player")repaint();}
    // Cell size follows density, but never below what the label and value text need.
    int spectralTextWidth=0;   // widest label/value over all paged Spectral controls, measured once in show()
    int cellWidth() const {return current=="spectral.engine"?std::max(knob+12,spectralTextWidth+6):std::max(knob+12,64);}   // Spectral: one width for every page
    // Each grid column is as wide as the widest text in it (labels, values, choices), so text never clips
    // and narrow columns stay narrow.
    std::vector<int> columnWidths(const std::vector<std::vector<Control*>>& rows) const {
        std::vector<int> widths;
        for(const auto& r:rows){if(widths.size()<r.size())widths.resize(r.size(),cellWidth());
            for(size_t c=0;c<r.size();++c)if(r[c]&&current!="spectral.engine")widths[c]=std::max(widths[c],r[c]->textWidth()+6);}
        return widths;
    }
    std::vector<std::vector<Control*>> gridRows() const {
        if(current=="sample.player")return {{control("transpose"),control("root_note"),control("level")},{control("start"),control("end")}};
        std::vector<std::vector<Control*>> rows;for(const auto& r:controlRows(current)){rows.emplace_back();for(const auto& id:r)rows.back().push_back(control(id));}
        return rows;
    }
    static int total(const std::vector<int>& widths){int w=0;for(int x:widths)w+=x;return w+std::max(0,int(widths.size())-1)*gap;}
    int cellHeight() const {return knob+Control::labelHeight+Control::valueHeight;}
    bool editorContent() const {return sequence||performance||referenceSequence;}
    Control* control(const std::string& id) const {const auto& ps=panel(current).parameters;for(size_t i=0;i<ps.size()&&i<knobs.size();++i)if(ps[i].id==id)return knobs[i].get();return nullptr;}
    Rectangle<int> contentSize() const {
        const int cw=cellWidth(),ch=cellHeight();
        if(plate){
            int width=0,height=0;
            for(const auto& row:*plate){
                int rowWidth=0;
                for(const auto& id:row.parameters)rowWidth+=faceCellWidth(id,row)+gap;
                width=std::max(width,rowWidth-gap);height+=faceRowHeight(row)+gap;
            }
            return {0,0,width,height-gap};
        }
        if(editorContent())return {0,0,panelWidth(current)-2*pad,300};   // sequencer editors keep their own layouts
        if(current=="spectral.engine")return {0,0,3*cw+2*gap,28+gap+28+gap+2*ch+gap};
        if(current=="sample.player")return {0,0,std::max(total(columnWidths(gridRows())),3*cw+2*gap),26+gap+36+gap+44+gap+2*ch+gap};
        if(knobs.empty())return {0,0,150,48};
        const auto rows=gridRows();
        return {0,0,total(columnWidths(rows)),int(rows.size())*ch+int(rows.size()-1)*gap};
    }
    int innerWidth() const {return std::max({contentSize().getWidth(),156,std::min(int(jacks.size()),current=="spectral.engine"?10:8)*Jack::size});}
    struct JackBank { int inputs, inputColumns, outputColumns, rows; };
    JackBank jackBank(int inner) const {
        const int count=int(jacks.size());
        const int ins=int(std::count_if(jacks.begin(),jacks.end(),[](const auto& j){return !j->output;}));
        const int outs=count-ins,slots=std::max(2,inner/Jack::size);
        const int left=outs==0?slots:ins==0?0:std::clamp((slots*ins+count/2)/count,1,slots-1);
        const int right=slots-left;
        return {ins,left,right,std::max(ins?(ins+left-1)/left:0,outs?(outs+right-1)/right:0)};
    }
    int jackRows(int inner) const {return jackBank(inner).rows;}
    static constexpr int cableLane=12, jackPitch=Jack::size+cableLane;
    Rectangle<int> naturalSize() const {
        const int inner=innerWidth();
        return {0,0,inner+2*pad,headerHeight+cableLane+jackRows(inner)*jackPitch+gap+4+contentSize().getHeight()+gap+footerHeight+4};
    }
    void paint(Graphics& g) override {
        if(current.empty())return;auto r=getLocalBounds().toFloat();const auto ink2=panelInk(current);
        g.setColour(panelColour(current));g.fillRoundedRectangle(r,3);g.setColour(highlighted?Colour(0xfff0c76c):ink2.withAlpha(.6f));g.drawRoundedRectangle(r.reduced(1),3,highlighted?2:1);
        g.setFont(font(text::header));g.setColour(ink2);g.drawText(String(instance).toUpperCase()+"  /  "+String(family).toUpperCase(),pad,4,getWidth()-2*pad-14,15,Justification::centredLeft,true);
        g.setColour(routed?Colour(0xffcbe0a8):Colour(0xff7b7b70));g.fillEllipse(float(getWidth()-pad-6),8,6,6);
        g.setColour(ink2.withAlpha(.35f));g.drawHorizontalLine(dividerY,float(pad),float(getWidth()-pad));
        if(plate){
            int y=content.getY();
            for(const auto& row:*plate){
                const String title=String(row.legend).toUpperCase();
                g.setFont(font(text::header));g.setColour(ink2);g.drawText(title,pad+2,y,getWidth()-2*pad,legendHeight,Justification::centredLeft);
                const int x=pad+10+int(std::ceil(GlyphArrangement::getStringWidth(font(text::header),title)));
                g.setColour(ink2.withAlpha(.36f));g.drawHorizontalLine(y+legendHeight/2,float(x),float(getWidth()-pad));
                y+=faceRowHeight(row)+gap;
            }
        }
        if(current=="sample.player"){
            const auto wave=waveArea.toFloat();g.setColour(ink);g.fillRoundedRectangle(wave,2);
            if(sample&&knobs.size()>3){const float start=float(knobs[2]->slider.getValue()),end=float(knobs[3]->slider.getValue());const float mid=wave.getCentreY(),half=wave.getHeight()*.45f;
                g.setColour(Colour(0xffd7ca96).withAlpha(.18f));g.fillRect(wave.withX(wave.getX()+start*wave.getWidth()).withWidth(std::max(0.f,end-start)*wave.getWidth()));
                g.setColour(Colour(0xffd7ca96));for(size_t i=0;i<sample->peaks.size();++i){float x=wave.getX()+(i+.5f)*wave.getWidth()/128,amplitude=std::min(1.f,sample->peaks[i])*half;g.drawLine(x,mid-amplitude,x,mid+amplitude,1);}
                g.setColour(teal);g.drawVerticalLine(int(wave.getX()+start*wave.getWidth()),wave.getY(),wave.getBottom());g.drawVerticalLine(int(wave.getX()+end*wave.getWidth()),wave.getY(),wave.getBottom());
            }else{g.setColour(paper.withAlpha(.6f));g.setFont(font(text::label));g.drawText(samplePath.empty()?"LOAD OR DROP A WAV":"CONNECT TO LOAD SAMPLE",waveArea,Justification::centred);}
        }
        if(knobs.empty()&&!editorContent()){g.setColour(ink2);g.setFont(font(13));g.drawFittedText(current=="utility.vca"?"AUDIO x CONTROL":current=="utility.downmix"?"(L + R) / 2":current=="utility.stereo"?"LEFT / RIGHT":"SIGNAL THROUGH",content,Justification::centred,2);}
        g.setColour(ink2.withAlpha(.8f));g.setFont(font(text::footer));
        if(current=="analog.vco"){
            const float y=float(getHeight()-footerHeight-3);
            Control::drawActivityLamp(g,{float(pad),y+4,8,8},true,ink2);
            Control::drawActivityLamp(g,{float(pad+55),y+4,8,8},false,ink2);
            g.setColour(ink2);g.drawText("Ready",pad+12,int(y),42,footerHeight,Justification::centredLeft);
            g.drawText(routed?"Waiting":"Waiting / oscillator unrouted",pad+67,int(y),getWidth()-pad-70,footerHeight,Justification::centredLeft);
            return;
        }
        g.drawText(routed?panel(current).description:"UNROUTED / connect to activate",pad,getHeight()-footerHeight-3,getWidth()-2*pad,footerHeight,Justification::centred,true);
    }
    void resized() override {
        const int inner=getWidth()-2*pad;
        choice.setBounds(pad,21,inner-(menuButton.isVisible()?28:0),24);menuButton.setBounds(getWidth()-pad-24,21,24,24);
        const auto bank=jackBank(inner);
        jackTop=getHeight()-footerHeight-4-gap-bank.rows*jackPitch;
        dividerY=jackTop;
        for(int i=0;i<int(jacks.size());++i){
            const bool output=jacks[size_t(i)]->output;
            const int columns=output?bank.outputColumns:bank.inputColumns;
            const int index=output?i-bank.inputs:i;
            const int row=index/columns;
            const int rowCount=output?std::min(columns,int(jacks.size())-bank.inputs-row*columns):0;
            const int start=output?inner-rowCount*Jack::size:0;
            jacks[size_t(i)]->setBounds(pad+start+index%columns*Jack::size,jackTop+row*jackPitch+cableLane,Jack::size,Jack::size);
        }
        const auto size=contentSize();
        content={pad+std::max(0,(inner-size.getWidth())/2),headerHeight+cableLane+4,std::min(inner,size.getWidth()),size.getHeight()};
        if(current.empty())return;
        if(plate){
            int y=content.getY();
            for(const auto& row:*plate){
                FlexBox controls;controls.justifyContent=FlexBox::JustifyContent::spaceAround;
                for(const auto& id:row.parameters)
                    controls.items.add(FlexItem(*control(id)).withWidth(float(faceCellWidth(id,row))));
                controls.performLayout(Rectangle<int>(pad,y+legendHeight,inner,faceRowHeight(row)-legendHeight).toFloat());
                y+=faceRowHeight(row)+gap;
            }
            return;
        }
        if(referenceSequence){referenceSequence->setBounds(content);return;}
        if(performance){performance->setBounds(content);return;}
        if(sequence){sequence->setBounds(content);return;}
        const int ch=cellHeight();
        auto grid=[&](const std::vector<std::vector<Control*>>& rows,Rectangle<int> area){
            Grid g;g.columnGap=g.rowGap=Grid::Px(gap);
            for(int w:columnWidths(rows))g.templateColumns.add(Grid::TrackInfo(Grid::Px(w)));
            for(size_t r=0;r<rows.size();++r)g.templateRows.add(Grid::TrackInfo(Grid::Px(ch)));
            for(size_t r=0;r<rows.size();++r)for(size_t c=0;c<rows[r].size();++c)if(rows[r][c]){rows[r][c]->setVisible(true);g.items.add(GridItem(*rows[r][c]).withArea(int(r)+1,int(c)+1));}
            g.performLayout(area);
        };
        if(current=="spectral.engine"){
            auto area=content;presetButton.setBounds(area.removeFromTop(28).removeFromLeft(area.getWidth()*45/100));
            presetName.setBounds(presetButton.getRight()+gap,content.getY(),content.getRight()-presetButton.getRight()-gap,28);area.removeFromTop(gap);
            auto row=area.removeFromTop(28);more.setBounds(row.removeFromRight(70));row.removeFromRight(gap);section.setBounds(row);area.removeFromTop(gap);
            for(auto& k:knobs)k->setVisible(false);
            const int page=section.getSelectedId()-1;
            if(page>=0&&page<int(pages.size())){
                const auto& visible=pages[size_t(page)];const int parts=std::max(1,(int(visible.size())+5)/6);subpage%=parts;
                more.setEnabled(parts>1);more.setButtonText(String(subpage+1)+" / "+String(parts)+" >");
                std::vector<std::vector<Control*>> rows(2);
                for(int i=0;i<6&&subpage*6+i<int(visible.size());++i)rows[size_t(i/3)].push_back(knobs[size_t(visible[size_t(subpage*6+i)])].get());
                if(rows[1].empty())rows.pop_back();grid(rows,area);
            }
            return;
        }
        if(current=="sample.player"){
            auto area=content;auto top=area.removeFromTop(26);
            if(auto* loop=control("loop")){const int w=std::min(top.getWidth()/2,loop->textWidth());loop->setBounds(top.removeFromRight(w));top.removeFromRight(gap);}
            sampleButton.setBounds(top);
            area.removeFromTop(gap);sampleName.setBounds(area.removeFromTop(18));strokeRole.setBounds(area.removeFromTop(18));area.removeFromTop(gap);waveArea=area.removeFromTop(44);area.removeFromTop(gap);
            grid(gridRows(),area);
            return;
        }
        grid(gridRows(),content);
    }
};
// Request 72: the rack is a JUCE FlexBox. Cards keep their natural size (knob density plus
// fixed, readable text) and wrap into rows; a patch can instead fix its rows by hand.
struct HardwareRack : juce::Component, private AsyncUpdater {
    static constexpr int gap=6,margin=6;
    std::map<std::string,Values> values;
    std::vector<std::unique_ptr<ModulePanel>> modules;
    std::vector<std::vector<std::string>> rows;   // empty: wrap to the window width
    int knob=44;std::function<void(float)> magnified;
    struct Cable {Jack* source; Jack* destination; Path path;};
    std::vector<Cable> cables;
    Jack* held=nullptr;
    Jack* originalSource=nullptr; // Set when lifting an occupied input.
    Jack* target=nullptr;
    Jack* hovered=nullptr;
    juce::Point<int> looseEnd;
    bool showCables=true;
    struct CableEdit { Endpoint unplug; Connection plug; };
    std::function<void(const CableEdit&)> connect;
    CableEdit pending;
    void clear(){
        cancelPendingUpdate();
        held=originalSource=target=hovered=nullptr;
        cables.clear();modules.clear();values.clear();
    }
    static Colour cableColour(SignalType type){
        if(type==SignalType::Event)return Colour(0xff92b5ed);
        if(type==SignalType::Control)return Colour(0xffe2b275);
        return Colour(0xff85c2be);
    }
    void wireJacks(const GraphSpec& graph){
        using Key=std::pair<std::string,std::string>;
        std::map<Key,Jack*> inputs,outputs;
        for(auto& card:modules)for(auto& j:card->jacks){
            (j->output?outputs:inputs)[{card->instance,j->port.id}]=j.get();
            j->setViewportIgnoreDragFlag(true);
            j->cableHover=[this](Jack* over){hovered=over;repaint();};
            j->cableDrag=[this](Jack& start,juce::Point<int> screen,bool finish){dragCable(start,screen,finish);};
        }
        for(auto& card:modules)card->setConnections(graph);
        for(const auto& c:graph.connections){
            const auto src=outputs.find({c.source.node,c.source.port});
            const auto dst=inputs.find({c.destination.node,c.destination.port});
            if(src!=outputs.end() && dst!=inputs.end())cables.push_back({src->second,dst->second});
        }
        repaint();
    }
    juce::Point<int> socketPosition(Jack& j) const {return getLocalPoint(&j,j.socket());}
    static bool matches(const Jack& a,const Jack& b){
        return a.getParentComponent()!=b.getParentComponent() && a.output!=b.output &&
               a.port.type==b.port.type && a.port.channels==b.port.channels;
    }
    static Endpoint endpoint(Jack& jack){
        return {static_cast<ModulePanel*>(jack.getParentComponent())->instance,jack.port.id};
    }
    void dragCable(Jack& start,juce::Point<int> screen,bool finish){
        auto* view=findParentComponentOfClass<Viewport>();
        const auto inView=view?view->getLocalPoint(nullptr,screen):juce::Point<int>();
        if(view && !finish && view->getLocalBounds().contains(inView))
            view->autoScroll(inView.x,inView.y,24,16);
        looseEnd=getLocalPoint(nullptr,screen);
        auto* component=getComponentAt(looseEnd);
        auto* over=dynamic_cast<Jack*>(component);
        if(!held){
            held=&start;
            for(const auto& c:cables)if(c.destination==held){originalSource=c.source;break;}
        }
        const bool inside=view?Rectangle<int>(0,0,view->getMaximumVisibleWidth(),view->getMaximumVisibleHeight()).contains(inView):getLocalBounds().contains(looseEnd);
        target=inside && over && canDrop(*over)?over:nullptr;
        if(finish){
            const bool unplug=inside && originalSource && (!component || component==this);
            if(target || unplug){
                CableEdit edit{};
                if(originalSource)edit.unplug=endpoint(start);
                if(target){
                    auto& anchor=anchorFor(*target);
                    edit.plug={endpoint(anchor.output?anchor:*target),endpoint(anchor.output?*target:anchor)};
                }
                pending=std::move(edit);
                triggerAsyncUpdate();
            }
            held=originalSource=target=nullptr;
        }
        repaint();
    }
    void handleAsyncUpdate() override {
        const auto route=pending;
        if(connect)connect(route);
    }
    void flushCableEdit(){handleUpdateNowIfNeeded();}
    Jack& anchorFor(const Jack& end) const {return originalSource && !end.output?*originalSource:*held;}
    bool canDrop(const Jack& j) const {
        return &j!=held && &j!=originalSource && matches(anchorFor(j),j);
    }
    // Use a horizontal strip above each jack row. Cross-row connections use the rack margin.
    // Wrapped socket banks first reach their own card edge, then join the common row strip.
    using WirePoint=juce::Point<float>;
    std::vector<WirePoint> approach(Jack& j,float lane) const {
        const auto socket=socketPosition(j).toFloat();
        const auto& card=*j.getParentComponent();
        const float localTrack=card.getY()+j.getY()-ModulePanel::cableLane+lane;
        const float rowTrack=card.getY()+ModulePanel::headerHeight+lane;
        std::vector<WirePoint> path{socket,{socket.x,localTrack}};
        if(localTrack!=rowTrack){
            const float edge=card.getX()-gap*.5f;
            path.push_back({edge,localTrack});path.push_back({edge,rowTrack});
        }
        return path;
    }
    Path leadPath(Jack& start,Jack* end) const {
        const float lane=start.port.type==SignalType::Control?8.f:start.port.type==SignalType::Event?6.f:3.f;
        auto points=approach(start,lane);
        if(end){
            auto other=approach(*end,lane);
            if(points.back().y!=other.back().y){
                points.push_back({4.f,points.back().y});points.push_back({4.f,other.back().y});
            }
            points.insert(points.end(),other.rbegin(),other.rend());
        }else points.push_back(looseEnd.toFloat());
        Path path;path.startNewSubPath(points.front());
        for(size_t i=1;i<points.size();++i)if(points[i]!=points[i-1])path.lineTo(points[i]);
        return path.createPathWithRoundedCorners(4);
    }
    void drawLead(Graphics& g,const Path& path,Jack& start,Jack* end,float opacity) const {
        g.setColour(Colour(0xff182023).withAlpha(opacity));
        g.strokePath(path,PathStrokeType(4.f));
        g.setColour(cableColour(start.port.type).withAlpha(opacity));
        g.strokePath(path,PathStrokeType(2.2f));
        for(auto* j:{&start,end})if(j){
            const auto p=socketPosition(*j).toFloat();g.fillEllipse(p.x-3,p.y-3,6,6);
        }
    }
    void paintOverChildren(Graphics& g) override {
        auto* trace=held?held:hovered;
        if(showCables){
            for(bool highlight:{false,true})for(const auto& cable:cables){
                if(originalSource && cable.destination==held)continue;
                const bool related=trace && (trace==cable.source || trace==cable.destination);
                if(related==highlight)drawLead(g,cable.path,*cable.source,cable.destination,related?1.f:trace?.18f:.8f);
            }
        }
        if(!held)return;
        for(const auto& card:modules)for(const auto& j:card->jacks)if(canDrop(*j)){
            const auto centre=socketPosition(*j).toFloat();
            g.setColour(cableColour(j->port.type));
            g.drawEllipse(centre.x-11,centre.y-11,22,22,j.get()==target?3.f:1.6f);
        }
        if(!target){
            // While the plug follows the mouse, keep every control and jack legend readable.
            for(const auto& card:modules){
                auto bounds=card->getBounds();
                g.excludeClipRegion(bounds.removeFromTop(card->jackTop));
                for(int row=0;row<card->jackRows(card->getWidth()-2*ModulePanel::pad);++row){
                    bounds.removeFromTop(ModulePanel::cableLane+23);
                    g.excludeClipRegion(bounds.removeFromTop(Jack::size-23));
                }
                g.excludeClipRegion(bounds);
            }
        }
        auto& anchor=target?anchorFor(*target):(originalSource?*originalSource:*held);
        drawLead(g,leadPath(anchor,target),anchor,target,1.f);
    }
    ModulePanel* find(const std::string& id){for(auto& m:modules)if(m->instance==id)return m.get();return nullptr;}
    // Lays the cards out for a view of this width; returns the rack's size.
    Rectangle<int> arrange(int width){
        for(auto& m:modules){m->knob=knob;}
        auto item=[&](ModulePanel& m){const auto s=m.naturalSize();return FlexItem(m).withWidth(float(s.getWidth())).withMinHeight(float(s.getHeight())).withMargin(FlexItem::Margin(gap*.5f));};
        int right=0,bottom=0;
        if(rows.empty()){
            FlexBox box;box.flexWrap=FlexBox::Wrap::wrap;box.justifyContent=FlexBox::JustifyContent::flexStart;
            box.alignContent=FlexBox::AlignContent::flexStart;box.alignItems=FlexBox::AlignItems::stretch;
            for(auto& m:modules)box.items.add(item(*m));
            box.performLayout(Rectangle<float>(float(margin),float(margin),float(std::max(200,width-2*margin)),1.0e6f));
        }else{
            float y=float(margin);
            for(const auto& row:rows){
                FlexBox box;box.flexWrap=FlexBox::Wrap::noWrap;box.justifyContent=FlexBox::JustifyContent::flexStart;box.alignItems=FlexBox::AlignItems::stretch;
                float height=0;for(const auto& id:row)if(auto* m=find(id)){box.items.add(item(*m));height=std::max(height,float(m->naturalSize().getHeight())+gap);}
                box.performLayout(Rectangle<float>(float(margin),y,1.0e6f,height));y+=height;
            }
        }
        for(const auto& m:modules){right=std::max(right,m->getRight());bottom=std::max(bottom,m->getBottom());}
        setSize(std::max(width,right+margin+gap/2),bottom+margin+gap/2);
        for(auto& c:cables)c.path=leadPath(*c.source,c.destination);
        repaint();
        return getLocalBounds();
    }
    // The rows as currently shown (used to start editing rows from the automatic layout).
    std::vector<std::vector<std::string>> shownRows() const {
        std::vector<std::vector<std::string>> out;int top=-1;
        std::vector<const ModulePanel*> sorted;for(const auto& m:modules)sorted.push_back(m.get());
        std::stable_sort(sorted.begin(),sorted.end(),[](const ModulePanel* a,const ModulePanel* b){return a->getY()!=b->getY()?a->getY()<b->getY():a->getX()<b->getX();});
        for(const auto* m:sorted){if(m->getY()!=top){out.emplace_back();top=m->getY();}out.back().push_back(m->instance);}
        return out;
    }
    void highlight(const std::string& id){for(auto& m:modules){m->highlighted=m->instance==id;m->repaint();}}
    void mouseMagnify(const MouseEvent&,float scale) override {if(magnified)magnified(scale);}
    void paint(Graphics& g) override {g.fillAll(Colour(0xff17191b));}
};
}
