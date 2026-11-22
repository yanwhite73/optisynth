#pragma once
#include "PlayingKeys.h"
#include <JuceHeader.h>
#include "../src/modular/reference_sequencers.h"
#include <map>

namespace optiui {
using namespace optisynth;
class ReferenceSequencerEditor final:public juce::Component,private juce::Timer {
    struct Field:juce::Component {
        juce::Label label;juce::Slider value;int parameter=-1;
        Field(){addAndMakeVisible(label);addAndMakeVisible(value);label.setFont(juce::Font(juce::FontOptions(11)));label.setColour(juce::Label::textColourId,juce::Colour(0xffaebcc7));value.setSliderStyle(juce::Slider::LinearHorizontal);value.setTextBoxStyle(juce::Slider::TextBoxRight,false,50,24);value.setScrollWheelEnabled(false);value.setColour(juce::Slider::trackColourId,juce::Colour(0xffd6b16c));}
        void resized()override{label.setBounds(0,0,getWidth(),18);value.setBounds(0,20,getWidth(),26);}
    };
    struct Edit {std::vector<float> before,after;};
public:
    std::function<void(const std::string&,float)> parameterChanged;
    std::function<void()> onPlay;
    std::function<double()> getTempo;
    std::function<void(double)> changeTempo;
    ReferenceSequencerEditor(std::string type,std::map<std::string,float> values,std::shared_ptr<Controls> controls)
        :type_(std::move(type)),spec_(panel(type_)),controls_(std::move(controls)),orbit_(type_=="seq.orbit"){
        for(const auto& p:spec_.parameters){auto it=values.find(p.id);values_.push_back(it==values.end()?p.initial:it->second);}
        setWantsKeyboardFocus(true);
        button(play_,"PLAY",[this]{if(onPlay)onPlay();});button(reset_,"RESET",[this]{if(auto s=state())s->reset.fetch_add(1);});
        button(fill_,"HOLD FILL",[]{});fill_.onStateChange=[this]{set(orbit_?OrFill:PrFill,fill_.isDown()?1:0);};
        button(undo_,"Undo",[this]{undo();});button(redo_,"Redo",[this]{redo();});
        button(pages_[0],"01 - 16",[this]{page_=0;repaint();});button(pages_[1],"17 - 32",[this]{page_=1;repaint();});
        button(paint_,"Paint",[this]{tool_=0;refresh();});button(erase_,"Erase",[this]{tool_=1;refresh();});button(select_,"Select",[this]{tool_=2;refresh();});
        button(clear_,"Clear step",[this]{change([&]{for(int r=0;r<16;++r)values_[probabilityIndex(step_,r)]=0;});});
        button(capture_,"Capture played notes",[this]{capture();});button(snapshot_,"Snapshot",[this]{snapshotValues_=values_;message_="Snapshot saved for this module";refresh();});
        button(restore_,"Restore",[this]{if(!snapshotValues_.empty()){preview_.clear();change([&]{values_=snapshotValues_;});message_="Snapshot restored";}});
        button(generate_,"Preview variation",[this]{previewVariation();});button(apply_,"Apply",[this]{if(!preview_.empty()){auto next=preview_;preview_.clear();change([&]{values_=next;});message_="Variation applied";}});
        button(reseed_,"New choices",[this]{uint32_t seed=uint32_t(values_[orbit_?OrSeed:PrSeed]);seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;set(orbit_?OrSeed:PrSeed,1+float(seed%65535));message_="New choices / seed saved with the patch";refresh();});
        button(discard_,"Discard",[this]{preview_.clear();message_="Preview discarded";refresh();});
        combo(brush_,{"Weight 25","Weight 50","Weight 75","Weight 100"},[this](int n){brushValue_=(n+1)*25;});brush_.setSelectedId(4,juce::dontSendNotification);
        combo(clock_,{"1/32","1/16","1/8","1/4","1/2","1 bar"},[this](int n){set(PrDivision,float(n));});
        combo(direction_,{"Forward","Backward","Ping-pong","Random"},[this](int n){set(PrTraversal,float(n));});
        combo(scale_,{"Chromatic","Major","Minor","Dorian","Major pent.","Minor pent.","Mixolydian"},[this](int n){set(PrScale,float(n));});
        combo(root_,{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},[this](int n){set(PrRoot,float(n));});
        combo(condition_,{"Always","Fill","Not fill","1 of 2","2 of 2","1 of 3","2 of 3","3 of 3"},[this](int n){set(probabilityIndex(step_,PrStepCondition),float(n));});
        combo(templates_,{"Uneven cycles","Four on the floor","Interlocking","Slow satellites"},[this](int n){orbitTemplate(n);});templates_.setSelectedId(1,juce::dontSendNotification);
        toggle(repeat_,"REPEAT CHOICES",[this](bool v){set(PrRepeat,v?1:0);});toggle(protect_,"Protect step",[this](bool v){set(probabilityIndex(step_,PrStepLock),v?1:0);});
        for(int r=0;r<4;++r){button(mute_[r],"MUTE",[this,r]{const int k=orbitIndex(r,OrMute);set(k,values_[k]>.5f?0:1);});button(solo_[r],"SOLO",[this,r]{const int k=orbitIndex(r,OrSolo);set(k,values_[k]>.5f?0:1);});}
        const char* layers[]={"Velocity","Gate","Chance","Ratchets"};for(int i=0;i<4;++i)button(layers_[i],layers[i],[this,i]{layer_=i;refresh();});
        tempo_.label.setText("TEMPO",juce::dontSendNotification);tempo_.value.setRange(20,300,.1);tempo_.value.setValue(120);tempo_.value.onValueChange=[this]{if(!refreshing_&&changeTempo)changeTempo(tempo_.value.getValue());};addAndMakeVisible(tempo_);
        for(auto& f:fields_){addAndMakeVisible(f);f.value.onDragStart=[this]{begin();};f.value.onDragEnd=[this]{finish();};f.value.onValueChange=[this,&f]{if(!refreshing_&&f.parameter>=0)set(f.parameter,float(f.value.getValue()));};}
        setSize(1220,860);refresh();startTimerHz(30);
    }
    void setValue(const std::string& id,float v){for(size_t i=0;i<spec_.parameters.size();++i)if(spec_.parameters[i].id==id){values_[i]=v;break;}refresh();}
    void setActive(bool v){active_=v;play_.setButtonText(v?"STOP":"PLAY");repaint();}
    void setTempo(double v){tempo_.value.setValue(v,juce::dontSendNotification);}
    void undo(){if(history_.empty())return;message_="Undid last edit";preview_.clear();auto e=history_.back();history_.pop_back();applyDelta(e.after,e.before);future_.push_back(std::move(e));refresh();}
    void redo(){if(future_.empty())return;message_="Redid last edit";preview_.clear();auto e=future_.back();future_.pop_back();applyDelta(e.before,e.after);history_.push_back(std::move(e));refresh();}
    const std::vector<float>& values()const{return values_;}
    void chooseStep(int s,int row){step_=std::clamp(s,0,31);row_=std::clamp(row,0,15);page_=step_/16;refresh();}
    juce::Rectangle<float> probabilityCell(int column,int row)const{auto g=grid();return {g.getX()+column*g.getWidth()/16,g.getY()+(15-row)*g.getHeight()/16,g.getWidth()/16,g.getHeight()/16};}
    juce::Point<float> orbitPoint(int ring,int step)const{const auto c=orbitCentre();const double angle=juce::MathConstants<double>::twoPi*step/std::max(1,int(values_[orbitIndex(ring,OrLength)]))-juce::MathConstants<double>::halfPi;const float radius=orbitRadius(ring);return {c.x+radius*float(std::cos(angle)),c.y+radius*float(std::sin(angle))};}
    void paint(juce::Graphics& g)override{
        g.fillAll(bg());g.setColour(juce::Colour(0xff21303c));g.fillRect(0,0,getWidth(),72);g.setColour(accent());g.fillRect(20,20,4,31);
        text(g,orbit_?"ORBIT":"PROBABILITY",34,15,340,32,25,ink());text(g,orbit_?"FOUR CLOCKS / FOUR ROUTABLE RHYTHMS":"PITCH x TIME / WEIGHTED NOTE CHOICES",35,45,450,18,10,accent());
        if(orbit_)paintOrbit(g);else paintProbability(g);
        text(g,message_,20,getHeight()-76,getWidth()-40,20,11,accent());
    }
    void resized()override{
        tempo_.setBounds(getWidth()-480,11,145,50);play_.setBounds(getWidth()-315,19,80,33);reset_.setBounds(getWidth()-225,19,85,33);fill_.setBounds(getWidth()-130,19,110,33);
        for(auto* c:probabilityComponents())c->setVisible(!orbit_);
        templates_.setVisible(orbit_);for(auto& b:mute_)b.setVisible(orbit_);for(auto& b:solo_)b.setVisible(orbit_);
        for(auto& f:fields_)f.setVisible(false);
        undo_.setBounds(getWidth()-165,88,67,29);redo_.setBounds(getWidth()-88,88,68,29);
        if(!orbit_){clock_.setBounds(20,91,95,27);direction_.setBounds(125,91,120,27);scale_.setBounds(255,91,140,27);root_.setBounds(405,91,68,27);repeat_.setBounds(640,91,180,27);
            fields_[0].setBounds(486,78,130,48);fields_[0].setVisible(true);
            pages_[0].setBounds(20,144,86,29);pages_[1].setBounds(114,144,86,29);paint_.setBounds(220,144,65,29);erase_.setBounds(293,144,65,29);select_.setBounds(366,144,65,29);brush_.setBounds(445,144,120,29);clear_.setBounds(580,144,108,29);reseed_.setBounds(700,144,110,29);
            fields_[9].setBounds(828,78,110,48);fields_[9].setVisible(true);
            for(int i=0;i<4;++i){fields_[i+1].setBounds(getWidth()-252,197+i*51,228,48);fields_[i+1].setVisible(true);}
            for(int i=0;i<4;++i){fields_[i+5].setBounds(getWidth()-252,418+i*51,228,48);fields_[i+5].setVisible(true);}
            condition_.setBounds(getWidth()-252,633,228,28);protect_.setBounds(getWidth()-252,667,180,27);
            const auto box=grid();for(int i=0;i<4;++i)layers_[i].setBounds(int(box.getX())+i*88,int(box.getBottom())+8,80,23);
        }else{
            templates_.setBounds(20,88,220,29);reseed_.setBounds(255,88,110,29);const int x=orbitRight();const int width=getWidth()-x-20;
            for(int r=0;r<4;++r){const int y=160+r*103;mute_[r].setBounds(x+width-140,y,66,25);solo_[r].setBounds(x+width-67,y,66,25);
                for(int f=0;f<4;++f){auto& c=fields_[r*4+f];c.setBounds(x+f*(width/4),y+32,width/4-8,49);c.setVisible(true);}}
            const int w=(getWidth()-40)/9;for(int i=0;i<9;++i){fields_[16+i].setBounds(20+i*w,getHeight()-158,w-9,49);fields_[16+i].setVisible(true);}
        }
        const int count=orbit_?5:6,buttonW=(getWidth()-40)/count;std::array<juce::TextButton*,6> actions={&capture_,&snapshot_,&restore_,&generate_,&apply_,&discard_};
        for(int i=orbit_?1:0;i<6;++i){actions[i]->setBounds(20+(i-(orbit_?1:0))*buttonW,getHeight()-43,buttonW-8,29);actions[i]->setVisible(true);}
        capture_.setVisible(!orbit_);
    }
    void mouseDown(const juce::MouseEvent& e)override{
        grabKeyboardFocus();if(!preview_.empty())return;
        if(orbit_){for(int r=0;r<4;++r){const auto distance=e.position.getDistanceFrom(orbitCentre());if(std::abs(distance-orbitRadius(r))<19){ring_=r;begin();drawingRing_=true;rotateRing(e.position);refresh();return;}
                if(juce::Rectangle<int>(orbitRight(),154+r*103,getWidth()-orbitRight()-20,95).contains(e.getPosition())){ring_=r;refresh();return;}}return;}
        auto box=grid();if(e.x>=20&&e.x<box.getX()&&e.y>=box.getY()&&e.y<box.getBottom()){const int r=15-int((e.y-box.getY())*16/box.getHeight());set(probabilityMute(r),values_[probabilityMute(r)]>.5f?0:1);return;}
        if(box.contains(e.position)){const auto cell=cellAt(e.position);step_=page_*16+cell.first;row_=cell.second;
            if(tool_==2&&!e.mods.isRightButtonDown()){refresh();return;}
            strokeErase_=tool_==1||e.mods.isRightButtonDown()||int(values_[probabilityIndex(step_,row_)])==brushValue_;begin();drawingGrid_=true;paintCell(e.position);return;}
        if(laneBounds().contains(e.position)){begin();drawingLane_=true;paintLane(e.position);}
    }
    void mouseDrag(const juce::MouseEvent& e)override{if(drawingGrid_)paintCell(e.position);if(drawingLane_)paintLane(e.position);if(drawingRing_)rotateRing(e.position);}
    void mouseUp(const juce::MouseEvent&)override{drawingGrid_=drawingLane_=drawingRing_=false;finish();}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)override{if(!orbit_&&grid().contains(e.position)&&std::abs(w.deltaX)>std::abs(w.deltaY)){page_=w.deltaX<0?1:0;repaint();}else juce::Component::mouseWheelMove(e,w);}
    bool keyPressed(const juce::KeyPress& k)override{if(k.getModifiers().isCommandDown()&&(k.getTextCharacter()=='z'||k.getTextCharacter()=='Z')){if(k.getModifiers().isShiftDown())redo();else undo();return true;}return optiui::handlesPlayingKey(k);}
private:
    std::string type_;const Panel& spec_;std::shared_ptr<Controls> controls_;bool orbit_,active_=false,refreshing_=false,gesture_=false,drawingGrid_=false,drawingLane_=false,drawingRing_=false,strokeErase_=false;
    int page_=0,step_=0,row_=0,ring_=0,tool_=0,brushValue_=100,layer_=0;
    std::vector<float> values_,before_,preview_,snapshotValues_;std::vector<Edit> history_,future_;
    juce::String message_;
    Field tempo_;std::array<Field,25> fields_;
    juce::TextButton play_,reset_,fill_,undo_,redo_,paint_,erase_,select_,clear_,capture_,snapshot_,restore_,generate_,apply_,discard_,reseed_;
    std::array<juce::TextButton,2> pages_;std::array<juce::TextButton,4> layers_,mute_,solo_;
    juce::ComboBox brush_,clock_,direction_,scale_,root_,condition_,templates_;juce::ToggleButton repeat_,protect_;
    static juce::Colour bg(){return juce::Colour(0xff131e28);}static juce::Colour ink(){return juce::Colour(0xffe9edf0);}static juce::Colour dim(){return juce::Colour(0xff94a5b4);}
    juce::Colour accent()const{return orbit_?juce::Colour(0xff71d4cb):juce::Colour(0xffe8bd72);}
    static juce::Colour ringColour(int r){const uint32_t colors[]={0xfff3949e,0xffe7d27c,0xff77d5ca,0xffa7a0f6};return juce::Colour(colors[r]);}
    static void text(juce::Graphics& g,const juce::String& s,int x,int y,int w,int h,float size,juce::Colour color,juce::Justification align=juce::Justification::centredLeft){g.setColour(color);g.setFont(juce::Font(juce::FontOptions(size)));g.drawText(s,x,y,w,h,align);}
    void button(juce::TextButton& b,const juce::String& label,std::function<void()> cb){b.setButtonText(label);b.onClick=std::move(cb);b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff293c4a));addAndMakeVisible(b);}
    void combo(juce::ComboBox& b,const juce::StringArray& labels,std::function<void(int)> cb){b.addItemList(labels,1);b.onChange=[this,&b,cb]{if(!refreshing_)cb(b.getSelectedId()-1);};addAndMakeVisible(b);}
    void toggle(juce::ToggleButton& b,const juce::String& label,std::function<void(bool)> cb){b.setButtonText(label);b.onClick=[&b,cb]{cb(b.getToggleState());};addAndMakeVisible(b);}
    std::shared_ptr<ReferenceSequenceState> state()const{return controls_?controls_->referenceSequence:nullptr;}
    const std::vector<float>& shown()const{return preview_.empty()?values_:preview_;}
    juce::Rectangle<float> grid()const{return {82,214,float(getWidth()-358),float(getHeight()-410)};}
    juce::Rectangle<float> laneBounds()const{auto r=grid();return {r.getX(),r.getBottom()+37,r.getWidth(),43};}
    int orbitRight()const{return getWidth()/2+28;}
    juce::Point<float> orbitCentre()const{return {float(getWidth()/4),float((getHeight()-165+130)/2)};}
    float orbitRadius(int r)const{return std::min(float(getWidth()/4-38),float((getHeight()-360)/2))-r*43;}
    std::pair<int,int> cellAt(juce::Point<float> point)const{auto g=grid();return {std::clamp(int((point.x-g.getX())*16/g.getWidth()),0,15),15-std::clamp(int((point.y-g.getY())*16/g.getHeight()),0,15)};}
    void bind(Field& f,int index,const juce::String& label={}){f.parameter=index;const auto& p=spec_.parameters[size_t(index)];f.label.setText(label.isEmpty()?juce::String(p.label).toUpperCase():label,juce::dontSendNotification);f.value.textFromValueFunction=nullptr;f.value.valueFromTextFunction=nullptr;
        if(orbit_&&(index-orbitGlobals)%orbitStride==OrDivision){
            f.value.textFromValueFunction=[](double v){const char* labels[]={"1/32","1/16","1/8","1/4","1/2","1 bar"};return juce::String(labels[std::clamp(int(v),0,5)]);};
            f.value.valueFromTextFunction=[](const juce::String& s){const char* labels[]={"1/32","1/16","1/8","1/4","1/2","1 bar"};for(int i=0;i<6;++i)if(s.trim()==labels[i])return double(i);return s.getDoubleValue();};
        }else if(!orbit_&&index==PrOctave){
            // Stored octave is MIDI/12; display the same C4=60 convention as row labels.
            f.value.textFromValueFunction=[](double v){return juce::String(int(v)-1);};
            f.value.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue()+1;};
        }
        f.value.setRange(p.minimum,p.maximum,p.step);f.value.setNumDecimalPlacesToDisplay(p.step>=1?0:2);f.value.setValue(shown()[size_t(index)],juce::dontSendNotification);f.setEnabled(preview_.empty());}
    void refresh(){refreshing_=true;
        if(orbit_){for(int r=0;r<4;++r){const int fields[]={OrLength,OrHits,OrRotation,OrDivision};for(int f=0;f<4;++f)bind(fields_[r*4+f],orbitIndex(r,fields[f]));mute_[r].setButtonText(values_[orbitIndex(r,OrMute)]>.5f?"MUTED":"MUTE");solo_[r].setButtonText(values_[orbitIndex(r,OrSolo)]>.5f?"SOLOED":"SOLO");}
            const int fields[]={OrPitch,OrVelocity,OrGate,OrChance,OrRatchet,OrSwing,OrAccentEvery,OrAccentAmount,OrDrift};for(int f=0;f<9;++f)bind(fields_[16+f],orbitIndex(ring_,fields[f]));
        }else{bind(fields_[9],PrSeed);bind(fields_[0],PrOctave,"BASE OCTAVE");bind(fields_[1],PrFirst);bind(fields_[2],PrLength);bind(fields_[3],PrSwing);bind(fields_[4],PrDensity);bind(fields_[5],probabilityIndex(step_,row_),"SELECTED CELL WEIGHT");bind(fields_[6],probabilityIndex(step_,PrStepVelocity));bind(fields_[7],probabilityIndex(step_,PrStepGate));bind(fields_[8],probabilityIndex(step_,PrStepChance));
            clock_.setSelectedId(int(values_[PrDivision])+1,juce::dontSendNotification);direction_.setSelectedId(int(values_[PrTraversal])+1,juce::dontSendNotification);scale_.setSelectedId(int(values_[PrScale])+1,juce::dontSendNotification);root_.setSelectedId(int(values_[PrRoot])+1,juce::dontSendNotification);condition_.setSelectedId(int(values_[probabilityIndex(step_,PrStepCondition)])+1,juce::dontSendNotification);repeat_.setToggleState(values_[PrRepeat]>.5f,juce::dontSendNotification);protect_.setToggleState(values_[probabilityIndex(step_,PrStepLock)]>.5f,juce::dontSendNotification);
            for(int i=0;i<4;++i)layers_[i].setColour(juce::TextButton::buttonColourId,layer_==i?juce::Colour(0xff67563b):juce::Colour(0xff293c4a));
            paint_.setColour(juce::TextButton::buttonColourId,tool_==0?juce::Colour(0xff67563b):juce::Colour(0xff293c4a));erase_.setColour(juce::TextButton::buttonColourId,tool_==1?juce::Colour(0xff67563b):juce::Colour(0xff293c4a));select_.setColour(juce::TextButton::buttonColourId,tool_==2?juce::Colour(0xff67563b):juce::Colour(0xff293c4a));}
        undo_.setEnabled(!history_.empty());redo_.setEnabled(!future_.empty());restore_.setEnabled(!snapshotValues_.empty());apply_.setEnabled(!preview_.empty());discard_.setEnabled(!preview_.empty());refreshing_=false;repaint();
    }
    void publish(const std::vector<float>& old){if(controls_)controls_->editRevision.fetch_add(1);for(size_t i=0;i<values_.size();++i)if(old[i]!=values_[i]){if(controls_)controls_->values[i]=values_[i];if(parameterChanged)parameterChanged(spec_.parameters[i].id,values_[i]);}if(controls_)controls_->editRevision.fetch_add(1);}
    void begin(){if(!gesture_){gesture_=true;before_=values_;}}
    void finish(){if(!gesture_)return;gesture_=false;if(before_!=values_){history_.push_back({before_,values_});if(history_.size()>48)history_.erase(history_.begin());future_.clear();}refresh();}
    template<class F>void change(F fn){if(!preview_.empty())return;const bool single=!gesture_;if(single)begin();auto old=values_;fn();publish(old);if(single)finish();else refresh();}
    void set(int i,float v){change([&]{values_[size_t(i)]=v;});}
    void applyDelta(const std::vector<float>& from,const std::vector<float>& to){auto old=values_;for(size_t i=0;i<to.size();++i)if(from[i]!=to[i])values_[i]=to[i];publish(old);}
    void paintCell(juce::Point<float> pos){if(!grid().contains(pos))return;const auto c=cellAt(pos);step_=page_*16+c.first;row_=c.second;set(probabilityIndex(step_,row_),strokeErase_?0:float(brushValue_));}
    float layerValue(int step)const{const auto& p=shown();switch(layer_){case 0:return p[probabilityIndex(step,PrStepVelocity)];case 1:return p[probabilityIndex(step,PrStepGate)];case 2:return p[probabilityIndex(step,PrStepChance)];default:return (p[probabilityIndex(step,PrStepRatchet)]-1)/7;}}
    void paintLane(juce::Point<float> pos){auto r=laneBounds();const int c=std::clamp(int((pos.x-r.getX())*16/r.getWidth()),0,15);step_=page_*16+c;const float v=std::clamp((r.getBottom()-pos.y)/r.getHeight(),0.f,1.f);const int f=layer_==0?PrStepVelocity:layer_==1?PrStepGate:layer_==2?PrStepChance:PrStepRatchet;set(probabilityIndex(step_,f),layer_==3?1+std::round(v*7):layer_==2?v:std::max(.01f,v));}
    void rotateRing(juce::Point<float> point){const auto c=orbitCentre();double angle=std::atan2(point.y-c.y,point.x-c.x)+juce::MathConstants<double>::halfPi;if(angle<0)angle+=juce::MathConstants<double>::twoPi;const int length=int(values_[orbitIndex(ring_,OrLength)]);set(orbitIndex(ring_,OrRotation),float(int(std::round(angle/juce::MathConstants<double>::twoPi*length))%length));}
    void capture(){auto s=state();if(!s)return;int captured=0;change([&]{for(int step=0;step<32;++step){const int row=s->outcomes[step];if(row<-1)continue;++captured;for(int r=0;r<16;++r)values_[probabilityIndex(step,r)]=r==row?100:0;values_[probabilityIndex(step,PrStepChance)]=1;}});message_=juce::String(captured)+" played columns captured as fixed notes";refresh();}
    void previewVariation(){preview_=values_;uint32_t seed=uint32_t(values_[orbit_?OrSeed:PrSeed]);auto random=[&]{seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
        if(orbit_){for(int r=0;r<4;++r){const int length=int(values_[orbitIndex(r,OrLength)]);preview_[orbitIndex(r,OrHits)]=float(1+random()%uint32_t(length));preview_[orbitIndex(r,OrRotation)]=float(random()%uint32_t(length));}}
        else for(int step=0;step<32;++step){if(values_[probabilityIndex(step,PrStepLock)]>.5f)continue;const int base=int(random()%9);for(int row=0;row<16;++row)preview_[probabilityIndex(step,row)]=row==base?75:row==base+4?35:row==base+7?15:0;}
        preview_[orbit_?OrSeed:PrSeed]=1+float(random()%65535);message_="VARIATION PREVIEW - playback is unchanged until Apply";refresh();}
    void orbitTemplate(int n){if(!orbit_)return;change([&]{const int length[][4]={{16,13,9,7},{16,16,16,16},{16,12,9,7},{23,19,13,11}},hits[][4]={{5,4,3,2},{4,2,8,3},{7,5,4,3},{5,4,3,2}};for(int r=0;r<4;++r){values_[orbitIndex(r,OrLength)]=float(length[n][r]);values_[orbitIndex(r,OrHits)]=float(hits[n][r]);values_[orbitIndex(r,OrRotation)]=float(r);values_[orbitIndex(r,OrDivision)]=n==3?float(r%2+2):1;}});}
    void paintProbability(juce::Graphics& g){const auto& p=shown();const auto display=state();const int playing=active_&&display?display->heads[0].load():-1;const auto area=grid();
        text(g,preview_.empty()?"PAINT RELATIVE WEIGHTS / ONE PITCH IS CHOSEN PER STEP":"VARIATION PREVIEW / APPLY OR DISCARD",82,182,int(area.getWidth()),22,11,accent());
        for(int row=0;row<16;++row){const auto c=probabilityCell(0,row);const int pitch=probabilityPitch(row,int(p[PrScale]),int(p[PrRoot]),int(p[PrOctave]));const bool muted=p[probabilityMute(row)]>.5f;text(g,juce::MidiMessage::getMidiNoteName(pitch,true,true,4)+(muted?" x":""),20,int(c.getY()),55,int(c.getHeight()),12,muted?dim():ink());}
        for(int column=0;column<16;++column){const int step=page_*16+column;const bool in=step>=int(p[PrFirst])-1&&step<int(p[PrLength]);const auto top=probabilityCell(column,15);text(g,juce::String(step+1),int(top.getX()),196,int(top.getWidth()),18,10,step==playing?accent():dim(),juce::Justification::centred);
            for(int row=0;row<16;++row){auto c=probabilityCell(column,row).reduced(1);const float weight=p[probabilityIndex(step,row)];const bool muted=p[probabilityMute(row)]>.5f;g.setColour(juce::Colour(column%4==0?0xff243542:0xff1c2b36));g.fillRoundedRectangle(c,2);
                if(weight>0){const auto color=juce::Colour::fromHSV(.105f+row*.013f,.45f,.95f,.22f+.78f*weight/100);g.setColour(color.withMultipliedAlpha(in&&!muted?1.f:.22f));g.fillRoundedRectangle(c.reduced(2),2);text(g,juce::String(int(weight)),int(c.getX()),int(c.getY()),int(c.getWidth()),int(c.getHeight()),11,juce::Colour(0xff101b23),juce::Justification::centred);}
                if(step==step_&&row==row_){g.setColour(ink());g.drawRoundedRectangle(c.reduced(1),2,1.5f);}
                if(step==playing&&display&&display->outcomes[step]==row){g.setColour(juce::Colours::white);g.fillEllipse(c.getRight()-10,c.getY()+4,5,5);}
            }
            if(step==playing){g.setColour(accent().withAlpha(.55f));g.drawRect(juce::Rectangle<float>(top.getX(),area.getY(),top.getWidth(),area.getHeight()),1.5f);}
            auto lane=laneBounds();const float w=lane.getWidth()/16;juce::Rectangle<float> cell{lane.getX()+column*w,lane.getY(),w-3,lane.getHeight()};g.setColour(juce::Colour(0xff21313c));g.fillRoundedRectangle(cell,2);g.setColour(accent().withAlpha(.8f));g.fillRect(cell.withTop(cell.getBottom()-layerValue(step)*cell.getHeight()).reduced(2,0));
        }
        text(g,"WEIGHT / LOOP",getWidth()-252,173,230,20,12,accent());text(g,"STEP "+juce::String(step_+1)+" / "+juce::MidiMessage::getMidiNoteName(probabilityPitch(row_,int(p[PrScale]),int(p[PrRoot]),int(p[PrOctave])),true,true,4),getWidth()-252,390,230,22,14,ink());
        text(g,"Weight is relative within a column.",getWidth()-252,704,232,18,10,dim());text(g,"Chance controls whether it plays.",getWidth()-252,722,232,18,10,dim());
    }
    void paintOrbit(juce::Graphics& g){const auto& p=shown();const auto display=state();const auto center=orbitCentre();
        text(g,"Drag a ring to rotate / click its row to edit articulation",20,123,getWidth()-40,24,12,dim());
        for(int r=0;r<4;++r){const int k=orbitIndex(r,0),length=int(p[k+OrLength]),hits=std::min(length,int(p[k+OrHits]));const int rotation=preview_.empty()&&active_&&display?display->rotations[r].load():int(p[k+OrRotation]);const float radius=orbitRadius(r);const auto color=ringColour(r);g.setColour(color.withAlpha(r==ring_?.65f:.3f));g.drawEllipse(center.x-radius,center.y-radius,radius*2,radius*2,r==ring_?2.5f:1.2f);
            for(int s=0;s<length;++s){const double angle=juce::MathConstants<double>::twoPi*s/length-juce::MathConstants<double>::halfPi;const juce::Point<float> point{center.x+radius*float(std::cos(angle)),center.y+radius*float(std::sin(angle))};const bool hit=euclideanHit(s,length,hits,rotation);const bool live=active_&&display&&display->heads[r]==s;g.setColour(color.withAlpha(p[k+OrMute]>.5f?.2f:hit?1.f:.28f));const float size=hit?12.f:5.f;g.fillEllipse(point.x-size/2,point.y-size/2,size,size);if(live){g.setColour(ink());g.drawEllipse(point.x-10,point.y-10,20,20,2);g.setColour(color.withAlpha(.45f));g.drawLine(center.x,center.y,point.x,point.y,1);}}
            const int x=orbitRight(),y=154+r*103;g.setColour(r==ring_?juce::Colour(0xff263b48):juce::Colour(0xff1c2b36));g.fillRoundedRectangle(float(x-10),float(y-3),float(getWidth()-x-10),96,5);g.setColour(color);g.fillRect(x-10,y-3,3,96);text(g,"RING "+juce::String(r+1)+"  /  "+juce::String(hits)+" IN "+juce::String(length),x,y+3,230,23,14,color);
            const int pass=display?display->passes[r].load()+1:1;text(g,"CYCLE "+juce::String(pass)+" / ROT "+juce::String(rotation),x+220,y+5,145,18,9,dim());
        }
        g.setColour(bg());g.fillEllipse(center.x-48,center.y-48,96,96);text(g,active_?"RUNNING":"READY",int(center.x)-46,int(center.y)-15,92,25,13,accent(),juce::Justification::centred);
        text(g,"RING "+juce::String(ring_+1)+" / ARTICULATION & EVOLUTION",20,getHeight()-187,getWidth()-40,25,14,ringColour(ring_));
    }
    std::vector<juce::Component*> probabilityComponents(){std::vector<juce::Component*> c={&clock_,&direction_,&scale_,&root_,&repeat_,&brush_,&paint_,&erase_,&select_,&clear_,&condition_,&protect_};for(auto& p:pages_)c.push_back(&p);for(auto& p:layers_)c.push_back(&p);return c;}
    void timerCallback()override{if(getTempo)setTempo(getTempo());repaint();}
};

class ReferenceSequenceWindow final:public juce::DocumentWindow {
public:
    ReferenceSequenceWindow(const juce::String& title,ReferenceSequencerEditor* editor):DocumentWindow(title,juce::Colour(0xff131e28),DocumentWindow::closeButton){setUsingNativeTitleBar(true);setContentOwned(editor,true);setResizable(true,false);setResizeLimits(1120,820,1800,1200);centreWithSize(1220,860);}
    void closeButtonPressed()override{setVisible(false);}
};
class ReferenceSequenceLauncher final:public juce::Component,private juce::Timer {
public:
    std::function<void(const std::string&,float)> parameterChanged;std::function<void()> onPlay;std::function<double()> getTempo;std::function<void(double)> changeTempo;
    ReferenceSequenceLauncher(std::string type,std::map<std::string,float> values,std::shared_ptr<Controls> controls):type_(std::move(type)),values_(std::move(values)),controls_(std::move(controls)){
        for(const auto& p:panel(type_).parameters)if(!values_.count(p.id))values_[p.id]=p.initial;addAndMakeVisible(open_);open_.setButtonText(type_=="seq.orbit"?"OPEN ORBIT":"OPEN PROBABILITY GRID");open_.onClick=[this]{openEditor();};startTimerHz(20);
    }
    void setValue(const std::string& id,float v){values_[id]=v;if(editor_)editor_->setValue(id,v);repaint();}
    void setActive(bool a){active_=a;if(editor_)editor_->setActive(a);repaint();}
    bool isEditorOpen() const { return window_ && window_->isVisible(); }
    bool hasEditorFocus()const{return editor_&&editor_->hasKeyboardFocus(true);}
    ReferenceSequencerEditor* openEditor(){if(!window_){editor_=new ReferenceSequencerEditor(type_,values_,controls_);editor_->parameterChanged=[this](const std::string& id,float v){values_[id]=v;if(parameterChanged)parameterChanged(id,v);};editor_->onPlay=[this]{if(onPlay)onPlay();};editor_->getTempo=[this]{return getTempo?getTempo():120.;};editor_->changeTempo=[this](double v){if(changeTempo)changeTempo(v);};window_=std::make_unique<ReferenceSequenceWindow>(type_=="seq.orbit"?"Optisynth / Orbit":"Optisynth / Probability",editor_);}editor_->setActive(active_);editor_->setTempo(getTempo?getTempo():120.);window_->setVisible(true);window_->toFront(true);return editor_;}
    void resized()override{open_.setBounds(5,getHeight()-40,getWidth()-10,35);}
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff14232b));const auto display=controls_?controls_->referenceSequence:nullptr;
        if(type_=="seq.probability"){const float w=float(getWidth()-12)/16,h=float(getHeight()-50)/16;const int step=active_&&display?display->heads[0].load():-1;for(int s=0;s<16;++s)for(int r=0;r<16;++r){float weight=values_[probabilityKey(s,r)];g.setColour(juce::Colour(0xffe8bd72).withAlpha(weight>0?.2f+.8f*weight/100:.06f));g.fillRect(6+s*w,3+(15-r)*h,w-2,h-1);}if(step>=0&&step<16){g.setColour(juce::Colours::white);g.drawRect(6+step*w,3.f,w,float(getHeight()-50),1.f);}}
        else{const float cx=getWidth()*.5f,cy=(getHeight()-45)*.5f;const uint32_t colors[]={0xfff3949e,0xffe7d27c,0xff77d5ca,0xffa7a0f6};for(int r=0;r<4;++r){const float radius=std::max(12.f,cy-12-r*19);const int length=int(values_[orbitKey(r,OrLength)]),hits=int(values_[orbitKey(r,OrHits)]),rotation=active_&&display?display->rotations[r].load():int(values_[orbitKey(r,OrRotation)]);g.setColour(juce::Colour(colors[r]).withAlpha(.4f));g.drawEllipse(cx-radius,cy-radius,radius*2,radius*2,1);for(int s=0;s<length;++s){const double a=juce::MathConstants<double>::twoPi*s/length-juce::MathConstants<double>::halfPi;const float x=cx+radius*float(std::cos(a)),y=cy+radius*float(std::sin(a));g.setColour(active_&&display&&display->heads[r]==s?juce::Colours::white:juce::Colour(colors[r]).withAlpha(euclideanHit(s,length,hits,rotation)?1.f:.15f));g.fillEllipse(x-3,y-3,6,6);}}}
    }
private:std::string type_;std::map<std::string,float> values_;std::shared_ptr<Controls> controls_;bool active_=false;juce::TextButton open_;ReferenceSequencerEditor* editor_=nullptr;std::unique_ptr<ReferenceSequenceWindow> window_;
    void timerCallback()override{repaint();}
};
}
