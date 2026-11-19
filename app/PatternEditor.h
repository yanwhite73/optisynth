#pragma once
#include <JuceHeader.h>
#include "../src/modular/pattern.h"
#include <map>
#include <set>

namespace optiui {
// Desktop editing surface for the Pattern engine. All clocks remain in DSP.
class PatternEditor final:public juce::Component,private juce::Timer {
    using Values=std::map<std::string,float>;
    struct Batch { std::shared_ptr<optisynth::Controls> c; explicit Batch(std::shared_ptr<optisynth::Controls> controls):c(std::move(controls)){if(c)c->editRevision.fetch_add(1);} ~Batch(){if(c)c->editRevision.fetch_add(1);} };
    struct Numeric:juce::Component {
        juce::Label name;juce::Slider value;
        Numeric(){addAndMakeVisible(name);addAndMakeVisible(value);name.setFont(juce::Font(juce::FontOptions(10)));name.setColour(juce::Label::textColourId,juce::Colour(0xffd4cebc));
            value.setSliderStyle(juce::Slider::LinearHorizontal);value.setTextBoxStyle(juce::Slider::TextBoxRight,false,45,22);value.setColour(juce::Slider::trackColourId,juce::Colour(0xff76c8bb));}
        void resized()override{name.setBounds(0,0,getWidth(),19);value.setBounds(0,19,getWidth(),26);}
    };
public:
    std::function<void()> onPlay,onRestart;
    std::function<double()> getTempo;std::function<void(double)> changeTempo;
    PatternEditor(Values values,std::shared_ptr<optisynth::Controls> controls,std::function<void(const std::string&,float)> changed)
        :values_(std::move(values)),controls_(std::move(controls)),changed_(std::move(changed)){
        for(const auto& p:optisynth::panel("seq.pattern").parameters)if(!values_.count(p.id))values_[p.id]=p.initial;
        setWantsKeyboardFocus(true);selection_.insert(0);
        addAndMakeVisible(tempo_);tempo_.name.setText("TEMPO / BPM",juce::dontSendNotification);tempo_.value.setRange(20,300,.1);tempo_.value.setValue(120,juce::dontSendNotification);tempo_.value.onValueChange=[this]{if(changeTempo)changeTempo(tempo_.value.getValue());};
        button(play_,"Play",[this]{if(onPlay)onPlay();grabKeyboardFocus();});button(restart_,"Restart",[this]{if(onRestart)onRestart();});
        button(undo_,"Undo",[this]{undo();});button(redo_,"Redo",[this]{redo();});
        button(copy_,"Copy",[this]{copy();});button(paste_,"Paste",[this]{paste();});
        button(duplicate_,"Duplicate",[this]{duplicate();});button(left_,"Shift <",[this]{shift(-1);});button(right_,"Shift >",[this]{shift(1);});
        button(all_,"Select loop",[this]{selection_.clear();for(int s=first();s<=last();++s)selection_.insert(s);repaint();});
        button(generate_,"New melody",[this]{generate(0);});button(rhythm_,"Euclidean gates",[this]{generate(1);});button(curves_,"New CV curves",[this]{generate(2);});
        button(snapshot_,"Store snapshot",[this]{snapshotValues_=values_;snapshotName_="Snapshot stored";repaint();});
        button(revert_,"Recall snapshot",[this]{if(!snapshotValues_.empty()){beginEdit();apply(snapshotValues_);endEdit();snapshotName_="Snapshot recalled";}});
        for(int i=0;i<4;++i){button(pages_[i],juce::String(i*16+1)+" - "+juce::String(i*16+16),[this,i]{page_=i;repaint();});}
        combo(division_,{"1/32","1/16","1/8","1/4","1/2","1 bar"},"division");
        combo(run_,{"Forward","Backward","Pendulum","Ping-pong","Random","Random walk"},"run_mode");
        combo(scale_,{"Chromatic","Major","Natural minor","Dorian","Major pent.","Minor pent.","Mixolydian"},"scale");
        combo(root_,{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},"root");
        combo(condition_,{"Always","Fill","Not fill","Previous","Not previous","First pass","Not first","1 : 2","2 : 2","1 : 3","2 : 3","3 : 3","1 : 4","2 : 4","4 : 4"},"");
        condition_.onChange=[this]{if(!refreshing_)editSelection(optisynth::Condition,float(condition_.getSelectedId()-1));};
        toggle(enabled_,"Enabled","enabled");toggle(fill_,"FILL","fill");toggle(keyboard_,"Keys transpose","keyboard_transpose");
        addAndMakeVisible(on_);on_.setButtonText("Notes on");on_.onClick=[this]{editSelection(optisynth::On,on_.getToggleState()?1.f:0.f);};
        addAndMakeVisible(lock_);lock_.setButtonText("Protect steps");lock_.onClick=[this]{editSelection(optisynth::Locked,lock_.getToggleState()?1.f:0.f);};
        const char* keys[]={"first_step","length","pitch_length","velocity_length","cv1_length","cv2_length","transpose","rotate","swing","cv_slew","gate","velocity","seed","density","generate_range","generate_root"};
        const char* names[]={"GATE FIRST","GATE LAST","PITCH LOOP *","VELOCITY LOOP *","CV 1 LOOP *","CV 2 LOOP *","TRANSPOSE","ROTATION","SWING","CV GLIDE (s)","GATE SCALE","VEL SCALE","RANDOM SEED","GEN DENSITY","GEN RANGE (st)","GEN BASE (MIDI)"};
        for(int i=0;i<16;++i){auto n=std::make_unique<Numeric>();n->setComponentID(keys[i]);n->name.setText(names[i],juce::dontSendNotification);const auto& d=definition(keys[i]);n->value.setRange(d.minimum,d.maximum,d.step);n->value.setValue(get(keys[i]),juce::dontSendNotification);
            n->value.setNumDecimalPlacesToDisplay(d.step>=1?0:2);n->value.onDragStart=[this]{beginEdit();};n->value.onDragEnd=[this]{endEdit();};
            n->value.onValueChange=[this,key=std::string(keys[i]),ptr=n.get()]{if(refreshing_)return;const bool single=!editing_;if(single)beginEdit();{Batch batch(controls_);const float v=float(ptr->value.getValue());change(key,v);if(key=="length"&&get("first_step")>v)change("first_step",v);if(key=="first_step"&&get("length")<v)change("length",v);}if(single)endEdit();};
            addAndMakeVisible(*n);numbers_.push_back(std::move(n));}
        addAndMakeVisible(pitch_);pitch_.name.setText("SELECTED PITCH",juce::dontSendNotification);pitch_.value.setRange(0,127,1);
        pitch_.value.onDragStart=[this]{beginEdit();};pitch_.value.onDragEnd=[this]{endEdit();};pitch_.value.onValueChange=[this]{if(!refreshing_)editSelection(optisynth::Pitch,float(pitch_.value.getValue()));};
        refresh();setSize(1220,850);startTimerHz(30);
    }
    void setValue(const std::string& id,float value){values_[id]=value;refresh();}
    void setTempoDisplay(double tempo){tempo_.value.setValue(tempo,juce::dontSendNotification);}
    void setTransportActive(bool active){if(active_==active)return;active_=active;play_.setButtonText(active?"Stop":"Play");repaint();}
    void selectStep(int s){selected_=std::clamp(s,0,63);selection_={selected_};page_=selected_/16;refresh();}
    juce::Slider& pitchControl(){return pitch_.value;}
    int selectedStep()const{return selected_;}
    void undo(){if(undoStack_.empty())return;auto v=std::move(undoStack_.back());undoStack_.pop_back();Values inverse;for(const auto& kv:v)inverse[kv.first]=get(kv.first);redoStack_.push_back(std::move(inverse));apply(v);}
    void redo(){if(redoStack_.empty())return;auto v=std::move(redoStack_.back());redoStack_.pop_back();Values inverse;for(const auto& kv:v)inverse[kv.first]=get(kv.first);undoStack_.push_back(std::move(inverse));apply(v);}
    void copy(){clipboard_.clear();for(int s:selection_){std::array<float,optisynth::patternStride> step{};for(int f=0;f<optisynth::patternStride;++f)step[f]=get(optisynth::patternKey(s,f));clipboard_.push_back(step);}refresh();}
    void paste(){if(clipboard_.empty())return;beginEdit();Batch batch(controls_);size_t k=0;
        if(selection_.size()==1){for(const auto& values:clipboard_){const int s=selected_+int(k++);if(s>63)break;for(int f=0;f<optisynth::patternStride;++f)change(optisynth::patternKey(s,f),values[f]);}}
        else for(int s:selection_){const auto& values=clipboard_[k++%clipboard_.size()];for(int f=0;f<optisynth::patternStride;++f)change(optisynth::patternKey(s,f),values[f]);}endEdit();}
    void duplicate(){const int start=first(),end=last(),length=end-start+1;if(end==63)return;beginEdit();Batch batch(controls_);const auto previous=values_;
        for(int s=end+1;s<=std::min(63,end+length);++s)for(int f=0;f<optisynth::patternStride;++f)change(optisynth::patternKey(s,f),previous.at(optisynth::patternKey(start+s-end-1,f)));
        change("length",float(std::min(64,end+length+1)));endEdit();}
    void shift(int direction){if(selection_.size()<2)return;beginEdit();Batch batch(controls_);const auto old=values_;std::vector<int> selected(selection_.begin(),selection_.end());
        for(size_t i=0;i<selected.size();++i){int source=(int(i)-direction+int(selected.size()))%int(selected.size());for(int f=0;f<optisynth::patternStride;++f)change(optisynth::patternKey(selected[i],f),old.at(optisynth::patternKey(selected[size_t(source)],f)));}endEdit();}
    void generate(int mode){beginEdit();Batch batch(controls_);uint32_t random=uint32_t(get("seed"));auto rng=[&]{random^=random<<13;random^=random>>17;random^=random<<5;return random;};
        const int begin=first(),length=last()-begin+1,hits=int(std::round(get("density")*length));
        for(int s=begin;s<begin+length;++s){if(get(optisynth::patternKey(s,optisynth::Locked))>.5f)continue;
            if(mode==0){int note=int(get("generate_root"))+int(rng()%uint32_t(int(get("generate_range"))+1));note=optisynth::patternQuantize(note,int(get("scale")),int(get("root")));change(optisynth::patternKey(s,optisynth::Pitch),float(note));}
            if(mode==1)change(optisynth::patternKey(s,optisynth::On),((s-begin)*hits)%length<hits?1.f:0.f);
            if(mode==2)for(int f:{optisynth::CV1,optisynth::CV2})change(optisynth::patternKey(s,f),float(rng()%201)/100.f-1.f);
        }
        change("seed",float(int(get("seed"))%65535+1));endEdit();}
    void resized()override{
        tempo_.setBounds(366,4,120,48);
        int x=getWidth()-620;for(auto* b:{&undo_,&redo_,&copy_,&paste_,&duplicate_,&left_,&right_}){b->setBounds(x,16,82,28);x+=87;}
        enabled_.setBounds(16,62,86,26);division_.setBounds(108,62,80,27);run_.setBounds(198,62,130,27);scale_.setBounds(338,62,130,27);root_.setBounds(478,62,60,27);
        fill_.setBounds(548,62,64,26);keyboard_.setBounds(620,62,132,26);all_.setBounds(getWidth()-160,62,142,27);play_.setBounds(getWidth()-348,62,82,27);restart_.setBounds(getWidth()-254,62,82,27);
        const int w=(getWidth()-32)/8;for(int i=0;i<16;++i)numbers_[i]->setBounds(16+(i%8)*w,101+(i/8)*52,w-8,48);
        for(int i=0;i<4;++i)pages_[i].setBounds(130+i*94,211,88,25);
        grid_={130,263,getWidth()-148,std::max(252,getHeight()-440)};
        const int bottom=grid_.getBottom()+22;
        pitch_.setBounds(16,bottom,170,48);condition_.setBounds(203,bottom+17,155,27);on_.setBounds(371,bottom+15,102,28);lock_.setBounds(482,bottom+15,145,28);
        snapshot_.setBounds(getWidth()-300,bottom+15,137,28);revert_.setBounds(getWidth()-153,bottom+15,137,28);
        generate_.setBounds(16,getHeight()-83,145,29);rhythm_.setBounds(171,getHeight()-83,156,29);curves_.setBounds(337,getHeight()-83,156,29);
    }
    void paint(juce::Graphics& g)override{
        const juce::Colour ink(0xff192729),paper(0xffeee4cc),teal(0xff80d2bf),amber(0xffefbd6b),dim(0xff81918e);
        g.fillAll(ink);g.setColour(paper);g.setFont(juce::Font(juce::FontOptions(23).withStyle("Bold")));g.drawText("PATTERN / MOTION",17,12,340,29,juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions(11)));g.setColour(dim);g.drawText("64 STEPS   /   NOTES + 2 CV LANES",18,40,340,15,juce::Justification::centredLeft);
        const int playing=controls_&&active_?controls_->playhead.load():-1;
        juce::String status=playing<0?"STOPPED":("STEP "+juce::String(playing+1)+"   /   PASS "+juce::String(controls_->cycle.load()+1)+(controls_->stepFired.load()?"   NOTE":"   SKIP"));
        g.setColour(amber);g.drawText(status,getWidth()-440,211,421,24,juce::Justification::centredRight);
        constexpr const char* names[]={"PITCH","VELOCITY","GATE","CHANCE","RATCHETS","CV 1","CV 2"};
        constexpr int fields[]={optisynth::Pitch,optisynth::Velocity,optisynth::Gate,optisynth::Chance,optisynth::Ratchet,optisynth::CV1,optisynth::CV2};
        for(int row=0;row<7;++row){const int field=fields[row];const int y=grid_.getY()+row*grid_.getHeight()/7;g.setFont(juce::Font(juce::FontOptions(12).withStyle("Bold")));g.setColour(row>=5?teal:paper);g.drawText(names[row],16,y,108,25,juce::Justification::centredLeft);
            if(row==0||row==1||row>=5){const int head=row==0?0:row==1?1:row-3;int lane=controls_&&active_?controls_->laneHeads[head].load():-1;
                g.setColour(dim);g.setFont(juce::Font(juce::FontOptions(10)));g.drawText(lane<0?"* 0 = gate loop":"PLAYING "+juce::String(lane+1),16,y+25,108,19,juce::Justification::centredLeft);}
            for(int c=0;c<16;++c){const int s=page_*16+c;auto r=cell(c,row).reduced(2);const bool inLoop=s>=first()&&s<=last(),on=get(optisynth::patternKey(s,optisynth::On))>.5f;
                g.setColour(juce::Colour(inLoop?0xff2d4142:0xff223133));g.fillRoundedRectangle(r.toFloat(),3);
                const float v=get(optisynth::patternKey(s,field));const auto& d=definition(optisynth::patternKey(s,field));float norm=(v-d.minimum)/(d.maximum-d.minimum);
                if(row==0)norm=std::clamp((v-36)/48.f,0.f,1.f);
                const auto colour=row>=5?teal:paper;g.setColour(colour.withAlpha(on?.38f:.12f));const int h=int((r.getHeight()-6)*norm);g.fillRect(r.getX()+3,r.getBottom()-h-3,r.getWidth()-6,h);
                if(selection_.count(s)){g.setColour(teal.withAlpha(.7f));g.drawRoundedRectangle(r.toFloat(),3,1);}
                const int laneHead=(row==0?0:row==1?1:row>=5?row-3:-1);const int head=laneHead>=0&&controls_&&active_?controls_->laneHeads[laneHead].load():playing;
                if(head==s){g.setColour(amber);g.fillRect(r.getX()+2,r.getY()+2,r.getWidth()-4,3);}
                juce::String text=row==0?juce::MidiMessage::getMidiNoteName(int(v),true,true,4):row==4?juce::String(int(v))+"x":row>=5?juce::String(v,2):juce::String(int(std::round(v*100)))+"%";
                if(row==0&&!on)text="REST";g.setColour(on?paper:dim);g.setFont(juce::Font(juce::FontOptions(11)));g.drawText(text,r,juce::Justification::centred);
                if(row==0){g.setColour(get(optisynth::patternKey(s,optisynth::Locked))>.5f?amber:dim);g.drawText(juce::String(s+1)+(get(optisynth::patternKey(s,optisynth::Locked))>.5f?" *":""),r.getX(),grid_.getY()-23,r.getWidth(),18,juce::Justification::centred);}
            }
        }
        g.setColour(paper);g.setFont(juce::Font(juce::FontOptions(11)));g.drawText("STEP "+juce::String(selected_+1)+"  /  "+juce::String(int(selection_.size()))+" SELECTED     CONDITION",16,grid_.getBottom()+4,600,18,juce::Justification::centredLeft);
        g.setColour(dim);g.drawText(snapshotName_,getWidth()-300,getHeight()-82,282,27,juce::Justification::centredRight);
        g.drawText("Drag a lane to draw values  ·  Shift-click selects a range  ·  Right-click a step toggles its gate  ·  Cmd-Z undo / Cmd-C,V copy,paste",16,getHeight()-45,getWidth()-32,18,juce::Justification::centredLeft);
        g.drawText("Generators affect the gate loop and respect protected steps. * Lane length 0 follows gate traversal. CV ports route through the modulation matrix.",16,getHeight()-26,getWidth()-32,18,juce::Justification::centredLeft);
    }
    void mouseDown(const juce::MouseEvent& e)override{
        if(!grid_.contains(e.getPosition()))return;grabKeyboardFocus();const int c=std::clamp((e.x-grid_.getX())*16/grid_.getWidth(),0,15),s=page_*16+c;
        if(e.mods.isShiftDown()){selection_.clear();for(int i=std::min(selected_,s);i<=std::max(selected_,s);++i)selection_.insert(i);refresh();return;}
        selected_=s;selection_={s};refresh();beginEdit();
        if(e.mods.isRightButtonDown()){change(optisynth::patternKey(s,optisynth::On),get(optisynth::patternKey(s,optisynth::On))>.5f?0.f:1.f);endEdit();return;}
        drawing_=true;drawAt(e.getPosition());
    }
    void mouseDrag(const juce::MouseEvent& e)override{if(drawing_)drawAt(e.getPosition());}
    void mouseUp(const juce::MouseEvent&)override{drawing_=false;endEdit();}
    bool keyPressed(const juce::KeyPress& key)override{
        if(key.getModifiers().isCommandDown()){const auto c=key.getTextCharacter();if(c=='z'||c=='Z'){if(key.getModifiers().isShiftDown())redo();else undo();return true;}if(c=='c'){copy();return true;}if(c=='v'){paste();return true;}if(c=='a'){selection_.clear();for(int i=first();i<=last();++i)selection_.insert(i);refresh();return true;}}
        if(key.getKeyCode()==juce::KeyPress::leftKey||key.getKeyCode()==juce::KeyPress::rightKey){selectStep(selected_+(key.getKeyCode()==juce::KeyPress::rightKey?1:-1));return true;}
        if(key.getKeyCode()==juce::KeyPress::deleteKey||key.getKeyCode()==juce::KeyPress::backspaceKey){editSelection(optisynth::On,0);return true;}return false;
    }
private:
    Values values_,before_,snapshotValues_;std::vector<Values> undoStack_,redoStack_;
    std::vector<std::array<float,optisynth::patternStride>> clipboard_;std::set<int> selection_;
    std::shared_ptr<optisynth::Controls> controls_;std::function<void(const std::string&,float)> changed_;
    std::vector<std::unique_ptr<Numeric>> numbers_;Numeric pitch_,tempo_;
    juce::TextButton play_,restart_,undo_,redo_,copy_,paste_,duplicate_,left_,right_,all_,generate_,rhythm_,curves_,snapshot_,revert_,pages_[4];
    juce::ComboBox division_,run_,scale_,root_,condition_;juce::ToggleButton enabled_,fill_,keyboard_,on_,lock_;
    juce::Rectangle<int> grid_;int selected_=0,page_=0;bool refreshing_=false,editing_=false,drawing_=false,active_=false;
    juce::String snapshotName_="Snapshot is temporary; Save stores the rack";
    float get(const std::string& id)const{auto i=values_.find(id);return i==values_.end()?0:i->second;}
    int last()const{return int(get("length"))-1;}int first()const{return std::min(last(),int(get("first_step"))-1);}
    const optisynth::Parameter& definition(const std::string& key)const{const auto& ps=optisynth::panel("seq.pattern").parameters;auto i=std::find_if(ps.begin(),ps.end(),[&](const auto& p){return p.id==key;});jassert(i!=ps.end());return *i;}
    void button(juce::TextButton& b,const juce::String& name,std::function<void()> callback){b.setButtonText(name);b.onClick=std::move(callback);b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff3b5352));addAndMakeVisible(b);}
    void combo(juce::ComboBox& c,const juce::StringArray& names,const std::string& id){addAndMakeVisible(c);c.addItemList(names,1);c.setComponentID(id);if(!id.empty())c.onChange=[this,&c,id]{if(!refreshing_){beginEdit();change(id,float(c.getSelectedId()-1));endEdit();}};}
    void toggle(juce::ToggleButton& b,const juce::String& name,const std::string& id){addAndMakeVisible(b);b.setButtonText(name);b.onClick=[this,&b,id]{beginEdit();change(id,b.getToggleState()?1.f:0.f);endEdit();};}
    void beginEdit(){if(editing_)return;editing_=true;before_=values_;}
    void endEdit(){if(!editing_)return;editing_=false;if(before_!=values_){Values delta;for(const auto& kv:before_)if(get(kv.first)!=kv.second)delta[kv.first]=kv.second;undoStack_.push_back(std::move(delta));if(undoStack_.size()>64)undoStack_.erase(undoStack_.begin());redoStack_.clear();}refresh();}
    void change(const std::string& id,float value){values_[id]=value;if(changed_)changed_(id,value);refresh();}
    void apply(const Values& values){if(controls_)controls_->editRevision.fetch_add(1);const auto target=values;for(const auto& kv:target)if(get(kv.first)!=kv.second)change(kv.first,kv.second);if(controls_)controls_->editRevision.fetch_add(1);refresh();}
    void editSelection(int field,float value){const bool single=!editing_;if(single)beginEdit();if(controls_)controls_->editRevision.fetch_add(1);for(int s:selection_)change(optisynth::patternKey(s,field),value);if(controls_)controls_->editRevision.fetch_add(1);if(single)endEdit();}
    void refresh(){refreshing_=true;
        for(auto& n:numbers_)n->value.setValue(get(n->getComponentID().toStdString()),juce::dontSendNotification);
        for(auto* c:{&division_,&run_,&scale_,&root_})c->setSelectedId(int(get(c->getComponentID().toStdString()))+1,juce::dontSendNotification);
        enabled_.setToggleState(get("enabled")>.5f,juce::dontSendNotification);fill_.setToggleState(get("fill")>.5f,juce::dontSendNotification);keyboard_.setToggleState(get("keyboard_transpose")>.5f,juce::dontSendNotification);
        on_.setToggleState(get(optisynth::patternKey(selected_,optisynth::On))>.5f,juce::dontSendNotification);lock_.setToggleState(get(optisynth::patternKey(selected_,optisynth::Locked))>.5f,juce::dontSendNotification);
        pitch_.value.setValue(get(optisynth::patternKey(selected_,optisynth::Pitch)),juce::dontSendNotification);condition_.setSelectedId(int(get(optisynth::patternKey(selected_,optisynth::Condition)))+1,juce::dontSendNotification);
        undo_.setEnabled(!undoStack_.empty());redo_.setEnabled(!redoStack_.empty());paste_.setEnabled(!clipboard_.empty());refreshing_=false;repaint();}
    juce::Rectangle<int> cell(int c,int row)const{const int x=grid_.getX()+c*grid_.getWidth()/16,y=grid_.getY()+row*grid_.getHeight()/7;return {x,y,(c+1)*grid_.getWidth()/16-c*grid_.getWidth()/16,(row+1)*grid_.getHeight()/7-row*grid_.getHeight()/7};}
    void drawAt(juce::Point<int> point){const int c=std::clamp((point.x-grid_.getX())*16/grid_.getWidth(),0,15),row=std::clamp((point.y-grid_.getY())*7/grid_.getHeight(),0,6);constexpr int fields[]={optisynth::Pitch,optisynth::Velocity,optisynth::Gate,optisynth::Chance,optisynth::Ratchet,optisynth::CV1,optisynth::CV2};
        const auto r=cell(c,row).reduced(3);const float norm=std::clamp(float(r.getBottom()-point.y)/std::max(1,r.getHeight()),0.f,1.f);const int s=page_*16+c,field=fields[row];const auto key=optisynth::patternKey(s,field);const auto& d=definition(key);
        float value=row==0?36+48*norm:d.minimum+(d.maximum-d.minimum)*norm;if(d.step>=1)value=std::round(value);change(key,value);selected_=s;selection_.insert(s);refresh();}
    void timerCallback()override{if(getTempo)tempo_.value.setValue(getTempo(),juce::dontSendNotification);repaint();}
};
class PatternWindow final:public juce::DocumentWindow {
public:
    PatternWindow(PatternEditor* editor):DocumentWindow("Optisynth 0.0.8 / Pattern performance editor",juce::Colour(0xff192729),DocumentWindow::closeButton){setUsingNativeTitleBar(true);setContentOwned(editor,true);setResizable(true,false);setResizeLimits(1120,800,1800,1200);centreWithSize(1220,850);}
    void closeButtonPressed()override{setVisible(false);}
};
}
