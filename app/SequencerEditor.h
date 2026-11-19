#pragma once
#include <JuceHeader.h>
#include "../src/modular/sequencer.h"
#include <map>
#include "PatternEditor.h"

namespace optiui {
// Same parameter document as the DSP. Selection is local UI state; the playhead
// is read from the audio thread. Editing never advances the sequence.
class SequencerEditor final:public juce::Component,private juce::Timer {
    struct Field:juce::Component {
        juce::Label label;juce::Slider slider;
        Field(){addAndMakeVisible(label);addAndMakeVisible(slider);label.setFont(juce::Font(juce::FontOptions(11)));label.setColour(juce::Label::textColourId,juce::Colour(0xff202224));
            slider.setScrollWheelEnabled(false);slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,20);
            slider.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xff202224));slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::trackColourId,juce::Colour(0xff46595b));}
        void resized()override{label.setBounds(0,0,getWidth(),18);slider.setBounds(0,18,getWidth(),getHeight()-18);}
    };
public:
    std::function<void()> onPlay,onRestart;
    std::function<double()> getTempo;std::function<void(double)> changeTempo;
    SequencerEditor(const std::string& type,const std::map<std::string,float>& values,
                    std::shared_ptr<optisynth::Controls> handle,std::function<void(const std::string&,float)> changed)
        :type_(type),values_(values),handle_(std::move(handle)),changed_(std::move(changed)){
        setWantsKeyboardFocus(true);
        for(const auto& p:optisynth::panel(type_).parameters)if(!values_.count(p.id))values_[p.id]=p.initial;
        for(auto* c:std::initializer_list<juce::Component*>{&enabled,&division,&length,&gate,&velocity,&pitch,&detail,&mode,&on,&hits,&rotation})addAndMakeVisible(c);
        enabled.setButtonText("Enabled");enabled.setColour(juce::ToggleButton::textColourId,juce::Colour(0xff202224));enabled.setToggleState(get("enabled")>=.5f,juce::dontSendNotification);
        enabled.onClick=[this]{change("enabled",enabled.getToggleState()?1:0);};
        division.addItemList({"1/32","1/16","1/8","1/4","1/2","1 bar"},1);division.setSelectedId(int(get("division"))+1,juce::dontSendNotification);
        division.setTooltip("Shared transport division (quarter-note beats)");division.onChange=[this]{change("division",float(division.getSelectedId()-1));};
        bind(length,"length","LENGTH");bind(gate,"gate","GATE");bind(velocity,"velocity","VELOCITY");
        if(euclidean()){bind(hits,"hits","HITS (capped by length)");bind(rotation,"rotation","ROTATION");}
        mode.addItemList({"Repeat","Hold","Rest"},1);mode.onChange=[this]{change(stepKey("mode"),float(mode.getSelectedId()-1));};
        mode.setTooltip("Repeat: every pulse. Hold: one sustained note. Rest: no note.");
        on.setButtonText("Note on");on.setColour(juce::ToggleButton::textColourId,juce::Colour(0xff202224));on.onClick=[this]{change(stepKey("on"),on.getToggleState()?1:0);};
        if(type_=="seq.pattern"){addAndMakeVisible(expand_);expand_.setButtonText("Open performance editor / 64 steps");expand_.onClick=[this]{openPerformanceEditor();};}
        refreshInspector();startTimerHz(30);
    }
    void setValue(const std::string& id,float value){values_[id]=value;if(fullEditor_)fullEditor_->setValue(id,value);refreshing_=true;
        if(id=="enabled")enabled.setToggleState(value>=.5f,juce::dontSendNotification);
        if(id=="division")division.setSelectedId(int(value)+1,juce::dontSendNotification);
        for(auto* f:{&length,&gate,&velocity,&hits,&rotation,&pitch,&detail})if(f->getComponentID()==juce::String(id))f->slider.setValue(value,juce::dontSendNotification);
        const int selection=std::min(selected_,int(get("length"))-1);
        if(selection!=selected_){selected_=selection;refreshInspector();}
        if(stage())mode.setSelectedId(int(get(stepKey("mode")))+1,juce::dontSendNotification);
        if(!stage()&&!euclidean())on.setToggleState(get(stepKey("on"))>=.5f,juce::dontSendNotification);
        refreshing_=false;repaint();
    }
    int selectedStep()const{return selected_;}
    int displayedStep()const{return playing_;}
    void pollPlayhead(){const int p=handle_&&transportActive_?handle_->playhead.load(std::memory_order_relaxed):-1;if(p!=playing_){playing_=p;repaint();}}
    void setTransportActive(bool active){transportActive_=active;if(fullEditor_)fullEditor_->setTransportActive(active);pollPlayhead();}
    void selectStep(int step){selected_=std::clamp(step,0,int(get("length"))-1);refreshInspector();repaint();}
    // Exposed to native checks through the same user-facing widgets.
    juce::Slider& pitchControl(){return pitch.slider;}
    bool performanceHasFocus()const{return fullEditor_&&fullEditor_->hasKeyboardFocus(true);}
    PatternEditor* openPerformanceEditor(bool visible=true){
        if(type_!="seq.pattern")return nullptr;
        if(!window_){fullEditor_=new PatternEditor(values_,handle_,[this](const std::string& id,float value){change(id,value);});fullEditor_->getTempo=[this]{return getTempo?getTempo():120.;};fullEditor_->changeTempo=[this](double v){if(changeTempo)changeTempo(v);};fullEditor_->onPlay=[this]{if(onPlay)onPlay();};fullEditor_->onRestart=[this]{if(onRestart)onRestart();};window_=std::make_unique<PatternWindow>(fullEditor_);}
        fullEditor_->setTempoDisplay(getTempo?getTempo():120.);fullEditor_->setTransportActive(transportActive_);window_->setVisible(visible);if(visible)window_->toFront(true);return fullEditor_;
    }
    void resized()override{
        const int w=getWidth(),cell=(w-168)/3;
        enabled.setBounds(0,10,82,30);division.setBounds(84,15,80,28);
        length.setBounds(168,0,cell,61);gate.setBounds(168+cell,0,cell,61);velocity.setBounds(168+2*cell,0,cell,61);
        canvas_={4,68,w-8,std::max(type_=="seq.pattern"?48:64,getHeight()-169-(type_=="seq.pattern"?36:0))};
        const int y=canvas_.getBottom()+25;
        expand_.setBounds(4,getHeight()-32,w-8,28);
        if(euclidean()){
            canvas_={4,68,std::min(220,w*44/100),std::max(128,getHeight()-96)};
            const int x=canvas_.getRight()+16,field=std::min(65,(getHeight()-96)/3);
            hits.setBounds(x,68,w-x,field);rotation.setBounds(x,68+field,w-x,field);pitch.setBounds(x,68+2*field,w-x,field);
        }
        else {pitch.setBounds(0,y,w/3,type_=="seq.pattern"?56:65);detail.setBounds(w/3,y,w/3,type_=="seq.pattern"?56:65);on.setBounds(2*w/3+10,y+19,w/3-10,30);mode.setBounds(2*w/3+10,y+22,w/3-14,28);}
    }
    void paint(juce::Graphics& g)override{
        const juce::Colour ink(0xff202224),paper(0xfff0e9d8),accent(0xfff1ca73);
        g.setColour(ink);g.fillRoundedRectangle(canvas_.toFloat(),5);
        const int length=int(get("length"));
        if(euclidean()){
            const auto centre=canvas_.getCentre().toFloat();const float radius=std::min(canvas_.getHeight()*.37f,canvas_.getWidth()*.32f);
            for(int i=0;i<length;++i){const float angle=float(i)*juce::MathConstants<float>::twoPi/length;const auto c=centre+juce::Point<float>(std::sin(angle)*radius,-std::cos(angle)*radius);
                const bool hit=optisynth::euclideanHit(i,length,int(get("hits")),int(get("rotation")));
                const float diameter=length>24?7:10;auto r=juce::Rectangle<float>(diameter,diameter).withCentre(c);
                g.setColour(hit?paper:juce::Colour(0xff506063));g.fillEllipse(r);if(i==playing_){g.setColour(accent);g.drawEllipse(r.expanded(3),2);}}
            g.setFont(juce::Font(juce::FontOptions(15)));g.setColour(paper);g.drawText(juce::String(std::min(int(get("hits")),length))+" / "+juce::String(length),canvas_,juce::Justification::centred);
        }else for(int i=0;i<(stage()?length:std::min(length,16));++i){auto r=cellBounds(i).reduced(2);const auto key="step"+std::to_string(i+1)+"_";
            const bool active=stage()?get(key+"mode")<2:get(key+"on")>=.5f;
            g.setColour(active?juce::Colour(0xff4d6262):juce::Colour(0xff313d3f));g.fillRoundedRectangle(r.toFloat(),3);
            if(selected_==i){g.setColour(paper);g.drawRoundedRectangle(r.toFloat().reduced(1),3,1.5f);}
            if(playing_==i){g.setColour(accent);g.fillRect(r.getX()+3,r.getY()+3,std::max(1,r.getWidth()-6),4);}
            g.setColour(active?paper:paper.withAlpha(.45f));g.setFont(juce::Font(juce::FontOptions(11)));
            const auto note=juce::MidiMessage::getMidiNoteName(int(get(key+"pitch")),true,true,4);
            const char* modes[]={"REP","HOLD","REST"};
            if(stage()){
                g.drawText(juce::String(i+1)+" / "+juce::String(int(get(key+"pulses")))+"x",r.reduced(2).withY(r.getY()+8).withHeight(17),juce::Justification::centred);
                g.drawText(note,r,juce::Justification::centred);
                g.drawText(modes[int(get(key+"mode"))],r.reduced(2).removeFromBottom(20),juce::Justification::centred);
            }else{
                g.setFont(juce::Font(juce::FontOptions(r.getHeight()<40?9.5f:11.f)));
                const auto header=r.withY(r.getY()+7).withHeight(std::max(10,r.getHeight()/2-7));
                const auto footer=r.withY(r.getBottom()-std::max(10,r.getHeight()/2-2)).withHeight(std::max(10,r.getHeight()/2-2));
                g.drawText(juce::String(i+1)+" "+note,header,juce::Justification::centred);
                g.drawText(active?juce::String(int(get(key+"velocity")*100))+"%":"REST",footer,juce::Justification::centred);
            }
        }
        g.setColour(ink);g.setFont(juce::Font(juce::FontOptions(11)));
        const auto state=!handle_?"UNROUTED":get("enabled")<.5f?"MUTED":playing_<0?"STOPPED / awaiting grid":"PLAYING "+juce::String(playing_+1);
        g.drawText((euclidean()?"GENERATED RHYTHM":juce::String(stage()?"STAGE ":"STEP ")+juce::String(selected_+1)+" SELECTED")+"   |   "+state,4,canvas_.getBottom()+3,getWidth()-8,19,juce::Justification::centredLeft);
    }
    void mouseDown(const juce::MouseEvent& e)override{
        if(euclidean())return;
        grabKeyboardFocus();
        for(int i=0;i<(stage()?int(get("length")):std::min(16,int(get("length"))));++i)if(cellBounds(i).contains(e.getPosition())){selectStep(i);if(e.mods.isRightButtonDown()&&!stage())change(stepKey("on"),get(stepKey("on"))<.5f?1:0);break;}
    }
    void mouseDoubleClick(const juce::MouseEvent& e)override{if(!euclidean()&&!stage()&&canvas_.contains(e.getPosition()))change(stepKey("on"),get(stepKey("on"))<.5f?1:0);}
    bool keyPressed(const juce::KeyPress& key)override{
        if(euclidean())return false;
        if(key.getKeyCode()==juce::KeyPress::leftKey||key.getKeyCode()==juce::KeyPress::rightKey){selectStep(selected_+(key.getKeyCode()==juce::KeyPress::rightKey?1:-1));return true;}
        return false;
    }
private:
    juce::TextButton expand_;
    PatternEditor* fullEditor_=nullptr;std::unique_ptr<PatternWindow> window_;
    std::string type_;std::map<std::string,float> values_;std::shared_ptr<optisynth::Controls> handle_;
    std::function<void(const std::string&,float)> changed_;int selected_=0,playing_=-1;bool refreshing_=false,transportActive_=false;
    juce::ToggleButton enabled,on;juce::ComboBox division,mode;
    Field length,gate,velocity,pitch,detail,hits,rotation;juce::Rectangle<int> canvas_;
    bool stage()const{return type_=="seq.stage";}bool euclidean()const{return type_=="seq.euclidean";}
    float get(const std::string& id)const{auto i=values_.find(id);return i==values_.end()?0:i->second;}
    std::string stepKey(const char* suffix)const{return "step"+std::to_string(selected_+1)+"_"+suffix;}
    void change(const std::string& id,float value){if(refreshing_)return;setValue(id,value);if(changed_)changed_(id,value);}
    void bind(Field& field,const std::string& id,const char* name){
        field.setComponentID(id);field.label.setText(name,juce::dontSendNotification);
        const auto& ps=optisynth::panel(type_).parameters;const auto p=std::find_if(ps.begin(),ps.end(),[&](const auto& p){return p.id==id;});
        field.slider.onValueChange=nullptr;field.slider.setRange(p->minimum,p->maximum,p->step);field.slider.setValue(get(id),juce::dontSendNotification);
        field.slider.setNumDecimalPlacesToDisplay(p->step>=1?0:2);field.slider.onValueChange=[this,&field,id]{change(id,float(field.slider.getValue()));};
    }
    void refreshInspector(){
        bind(pitch,euclidean()?"pitch":stepKey("pitch"),"PITCH (MIDI)");
        if(!euclidean())bind(detail,stepKey(stage()?"pulses":"velocity"),stage()?"DURATION (PULSES)":"STEP VELOCITY");
        detail.setVisible(!euclidean());on.setVisible(!euclidean()&&!stage());mode.setVisible(stage());hits.setVisible(euclidean());rotation.setVisible(euclidean());
        if(stage())mode.setSelectedId(int(get(stepKey("mode")))+1,juce::dontSendNotification);
        if(!stage()&&!euclidean())on.setToggleState(get(stepKey("on"))>=.5f,juce::dontSendNotification);
    }
    juce::Rectangle<int> cellBounds(int i)const{
        if(!stage()){const int columns=8;return {canvas_.getX()+i%columns*canvas_.getWidth()/columns,canvas_.getY()+i/columns*canvas_.getHeight()/2,canvas_.getWidth()/columns,canvas_.getHeight()/2};}
        float total=0,start=0;for(int s=0;s<int(get("length"));++s){const float pulses=get("step"+std::to_string(s+1)+"_pulses");if(s<i)start+=pulses;total+=pulses;}
        const float pulses=get("step"+std::to_string(i+1)+"_pulses");
        // A readable minimum for short stages; remaining width shows relative duration.
        const int minimum=36,extra=std::max(0,canvas_.getWidth()-minimum*int(get("length")));
        return {canvas_.getX()+i*minimum+int(start/total*extra),canvas_.getY(),minimum+int(pulses/total*extra),canvas_.getHeight()};
    }
    void timerCallback()override{pollPlayhead();}
};
}
