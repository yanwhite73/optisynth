#pragma once
#include <JuceHeader.h>
#include "../src/modular/performance.h"
#include "../src/modular/pattern.h"
#include <set>

namespace optiui {
using namespace optisynth;
class PerformanceEditor final : public juce::Component, private juce::Timer {
    struct Field : juce::Component {
        juce::Label label;
        juce::Slider slider;
        Field(){addAndMakeVisible(label);addAndMakeVisible(slider);label.setFont(juce::Font(juce::FontOptions(10)));
            label.setColour(juce::Label::textColourId,juce::Colour(0xffb8c7c3));slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,45,22);slider.setColour(juce::Slider::trackColourId,juce::Colour(0xff80d2bf));}
        void resized()override{label.setBounds(0,0,getWidth(),18);slider.setBounds(0,18,getWidth(),25);}
    };
    struct Edit { PerformanceData before,after; };
public:
    std::function<void(const PerformanceData&)> dataChanged;
    std::function<void(const std::string&,float)> parameterChanged;
    std::function<void()> onPlay,onRestart;
    std::function<double()> getTempo;
    std::function<void(double)> changeTempo;
    PerformanceEditor(PerformanceData data,std::shared_ptr<Controls> controls,std::map<std::string,float> values)
        :data_(std::move(data)),controls_(std::move(controls)),values_(std::move(values)){
        setWantsKeyboardFocus(true);
        for(const auto& p:performancePanel().parameters)if(!values_.count(p.id))values_[p.id]=p.initial;
        button(play_,"PLAY",[this]{if(onPlay)onPlay();grabKeyboardFocus();});button(restart_,"RESTART",[this]{if(onRestart)onRestart();});
        button(fill_,"HOLD FILL",[]{});fill_.onStateChange=[this]{parameter("fill",fill_.isDown()?1:0);};
        button(editTab_,"PATTERNS",[this]{chainView_=false;resized();repaint();});button(chainTab_,"CHAIN",[this]{chainView_=true;resized();repaint();});
        button(undo_,"Undo",[this]{undo();});button(redo_,"Redo",[this]{redo();});
        button(copy_,"Copy pattern",[this]{clipboard_=pattern();hasClipboard_=true;refresh();});
        button(paste_,"Paste pattern",[this]{if(hasClipboard_)edit([&]{pattern()=clipboard_;});});
        button(clone_,"Clone to next",[this]{if(bank_<7){const auto copy=pattern();edit([&]{data_.tracks[track_].patterns[bank_+1]=copy;});++bank_;selection_={0};refresh();}});
        button(variation_,"Preview variation",[this]{previewVariation();});button(apply_,"Apply variation",[this]{if(preview_){const auto next=*preview_;preview_.reset();edit([&]{pattern()=next;++data_.seed;if(!data_.seed)data_.seed=1;});}});
        button(cancel_,"Discard preview",[this]{preview_.reset();refresh();});
        button(cancelLaunch_,"Cancel queued",[this]{if(auto e=exchange())for(int t=0;t<4;++t)e->cancel(t);});
        button(launchScene_,"Launch column",[this]{if(auto e=exchange())for(int t=0;t<4;++t)e->launch(t,bank_,!active_);});
        button(octaveDown_,"Oct -",[this]{entryOctave_=std::max(0,entryOctave_-12);repaint();});button(octaveUp_,"Oct +",[this]{entryOctave_=std::min(108,entryOctave_+12);repaint();});
        for(int t=0;t<4;++t){button(mute_[t],"MUTE",[this,t]{parameter(trackKey(t,"mute"),get(trackKey(t,"mute"))>.5f?0:1);});
            button(solo_[t],"SOLO",[this,t]{parameter(trackKey(t,"solo"),get(trackKey(t,"solo"))>.5f?0:1);});
            button(reset_[t],"RESET",[this,t]{if(auto e=exchange())e->reset(t);});}
        for(int p=0;p<4;++p)button(pages_[p],juce::String(p*16+1)+"–"+juce::String(p*16+16),[this,p]{page_=p;repaint();});
        configure(tempo_,"TEMPO",20,300,.1,[this](double v){if(changeTempo)changeTempo(v);});
        tempo_.slider.setValue(120,juce::dontSendNotification);
        configure(transpose_,"MASTER TRANSPOSE",-36,36,1,[this](double v){parameter("transpose",float(v));});
        configure(trackTranspose_,"TRACK TRANSPOSE",-36,36,1,[this](double v){parameter(trackKey(track_,"transpose"),float(v));});
        configure(first_,"FIRST STEP",1,64,1,[this](double v){edit([&]{pattern().first=int(v)-1;pattern().last=std::max(pattern().last,pattern().first);});});
        configure(last_,"LAST STEP",1,64,1,[this](double v){edit([&]{pattern().last=int(v)-1;pattern().first=std::min(pattern().first,pattern().last);});});
        configure(rotation_,"ROTATION",-63,63,1,[this](double v){edit([&]{pattern().rotation=int(v);});});
        configure(swing_,"SWING",0,.45,.01,[this](double v){edit([&]{pattern().swing=float(v);});});
        configure(pitch_,"PITCH / MIDI",0,127,1,[this](double v){selectedEdit([&](PerformanceStep& s){s.pitch=int(v);});});
        configure(velocity_,"VELOCITY",.01,1,.01,[this](double v){selectedEdit([&](PerformanceStep& s){s.velocity=float(v);});});
        configure(gate_,"GATE",.01,1,.01,[this](double v){selectedEdit([&](PerformanceStep& s){s.gate=float(v);});});
        configure(chance_,"PROBABILITY",0,1,.01,[this](double v){selectedEdit([&](PerformanceStep& s){s.chance=float(v);});});
        configure(ratchet_,"RATCHETS",1,8,1,[this](double v){selectedEdit([&](PerformanceStep& s){s.ratchets=int(v);});});
        configure(slide_,"SLIDE / SECONDS",0,2,.01,[this](double v){selectedEdit([&](PerformanceStep& s){s.slide=float(v);});});
        configure(cv_,"CV",-1,1,.01,[this](double v){selectedEdit([&](PerformanceStep& s){s.cv=float(v);});});
        combo(division_,{"1/32","1/16","1/8","1/4","1/2","1 bar"},[this](int v){edit([&]{pattern().division=v;});});
        combo(traversal_,{"Forward","Backward","Pendulum","Ping-pong","Random","Random walk"},[this](int v){edit([&]{pattern().traversal=v;});});
        combo(scale_,{"Chromatic","Major","Minor","Dorian","Major pent.","Minor pent.","Mixolydian"},[this](int v){edit([&]{pattern().scale=v;});});
        combo(root_,{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},[this](int v){edit([&]{pattern().root=v;});});
        combo(launch_,{"Launch now","Next beat","Next bar","Next 2 bars"},[this](int v){parameter("launch",float(v));});
        combo(layer_,{"Velocity","Gate length","Probability","Ratchets","CV","Slide"},[this](int v){layerIndex_=v;repaint();});
        combo(condition_,{"Always","Fill","Not fill","Previous","Not previous","First pass","Not first","1 : 2","2 : 2","1 : 3","2 : 3","3 : 3","1 : 4","2 : 4","4 : 4"},[this](int v){selectedEdit([&](PerformanceStep& s){s.condition=v;});});
        toggle(on_,"Step on",[this](bool v){selectedEdit([&](PerformanceStep& s){s.on=v;});});
        toggle(tie_,"Tie to next",[this](bool v){selectedEdit([&](PerformanceStep& s){s.tie=v;});});
        toggle(protect_,"Protect",[this](bool v){selectedEdit([&](PerformanceStep& s){s.protect=v;});});
        toggle(autoStep_,"Auto step",[](bool){});toggle(chainRun_,"PLAY CHAIN",[this](bool v){parameter("chain",v?1:0);});
        configure(chainFirst_,"CHAIN FIRST",1,16,1,[this](double v){edit([&]{data_.chainFirst=int(v)-1;data_.chainLast=std::max(data_.chainLast,data_.chainFirst);});});
        configure(chainLast_,"CHAIN LAST",1,16,1,[this](double v){edit([&]{data_.chainLast=int(v)-1;data_.chainFirst=std::min(data_.chainFirst,data_.chainLast);});});
        for(int row=0;row<16;++row){auto& f=chainBars_[row];configure(f,"",1,16,1,[this,row](double v){edit([&]{data_.chain[row].bars=int(v);});});
            for(int t=0;t<4;++t)combo(chainPatterns_[row][t],{"A","B","C","D","E","F","G","H"},[this,row,t](int p){edit([&]{data_.chain[row].patterns[t]=p;});});}
        selection_={0};refresh();setSize(1260,930);startTimerHz(30);
    }
    void setData(const PerformanceData& data){data_=data;refresh();}
    void setValue(const std::string& key,float value){values_[key]=value;refresh();}
    void setActive(bool active){active_=active;play_.setButtonText(active?"STOP":"PLAY");repaint();}
    void setTempoDisplay(double tempo){tempo_.slider.setValue(tempo,juce::dontSendNotification);}
    void selectPattern(int track,int bank){track_=std::clamp(track,0,3);bank_=std::clamp(bank,0,7);preview_.reset();page_=0;selected_=0;selection_={0};refresh();}
    void selectStep(int step){selected_=std::clamp(step,0,63);selection_={selected_};page_=selected_/16;refresh();}
    juce::Slider& pitchControl(){return pitch_.slider;}
    juce::Slider& chainBarsControl(int row){return chainBars_[size_t(row)].slider;}
    int editingTrack()const{return track_;}int editingBank()const{return bank_;}
    const PerformanceData& data()const{return data_;}
    void showChain(bool show){chainView_=show;resized();repaint();}
    void launchPattern(int track,int bank){if(auto e=exchange())e->launch(track,bank,!active_);}
    void undo(){if(undo_.isEnabled()&&!history_.empty()){auto change=std::move(history_.back());history_.pop_back();applyDelta(change.after,change.before);future_.push_back(std::move(change));refresh();}}
    void redo(){if(future_.empty())return;auto change=std::move(future_.back());future_.pop_back();applyDelta(change.before,change.after);history_.push_back(std::move(change));refresh();}
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff172326));g.setColour(juce::Colour(0xff172326));
        text(g,"PERFORMANCE",18,12,240,27,23,paper());text(g,"4 TRACKS  /  8 PATTERNS  /  64 STEPS",19,43,330,17,10,dim());
        const auto ex=exchange();const int matrixW=getWidth()-440;
        for(int t=0;t<4;++t){const int y=86+t*43;auto colour=trackColour(t);g.setColour(colour);g.fillRect(18,y,4,35);
            text(g,juce::String(t+1)+"  "+juce::String(data_.tracks[t].name),30,y,125,20,13,colour);
            juce::String state="STOPPED";
            if(ex&&active_)state=ex->steps[t]<0?"WAITING FOR CLOCK":"PLAY "+letter(ex->playing[t])+" / STEP "+juce::String(ex->steps[t]+1);
            text(g,state,30,y+19,130,16,9,dim());
            for(int p=0;p<8;++p){auto cell=pad(t,p);const bool selected=t==track_&&p==bank_;const int playing=ex?ex->playing[t].load():data_.tracks[t].initial;const bool queued=ex&&ex->queued[t]==p;
                g.setColour(selected?colour.withAlpha(.23f):juce::Colour(0xff263639));g.fillRoundedRectangle(cell.toFloat(),4);
                if(selected){g.setColour(paper());g.drawRoundedRectangle(cell.toFloat(),4,1.5f);}
                text(g,letter(p),cell.getX()+7,cell.getY()+3,20,23,16,paper());
                text(g,queued?"QUEUED":active_&&playing==p?"PLAYING":"EDIT",cell.getX()+28,cell.getY()+6,cell.getWidth()-49,16,8,queued?juce::Colour(0xffefbd6b):active_&&playing==p?colour:dim());
                g.setColour(colour);const float x=float(cell.getRight()-14),cy=float(cell.getCentreY());juce::Path triangle;triangle.addTriangle(x-3,cy-5,x-3,cy+5,x+4,cy);g.fillPath(triangle);
            }
            text(g,"PASS "+juce::String(ex?ex->passes[t].load()+1:1),getWidth()-94,y+8,80,20,10,dim());
        }
        (void)matrixW;
        text(g,"Click a pattern to edit. Its triangle queues playback. Editing never launches.",18,264,getWidth()-420,19,11,dim());
        if(chainView_){paintChain(g);return;}
        const auto& p=shownPattern();const int playStep=ex&&active_&&ex->playing[track_]==bank_?ex->steps[track_].load():-1;
        text(g,"EDIT  "+juce::String(data_.tracks[track_].name)+" / "+letter(bank_)+(preview_?"  ·  VARIATION PREVIEW":""),18,303,650,28,18,trackColour(track_));
        text(g,"CLOCK",18,339,80,18,9,dim());text(g,"TRAVERSAL",108,339,130,18,9,dim());text(g,"SCALE / ROOT",248,339,220,18,9,dim());
        for(int c=0;c<16;++c){const int step=page_*16+c;const auto& s=p.steps[step];auto r=stepCell(c);const bool inLoop=step>=p.first&&step<=p.last;
            g.setColour(inLoop?juce::Colour(0xff304347):juce::Colour(0xff202f32));g.fillRoundedRectangle(r.toFloat(),4);
            if(selection_.count(step)){g.setColour(paper());g.drawRoundedRectangle(r.toFloat().reduced(1),4,1.5f);}
            if(step==playStep){g.setColour(trackColour(track_));g.fillRect(r.getX()+4,r.getY()+4,r.getWidth()-8,4);}
            text(g,juce::String(step+1),r.getX(),r.getY()+11,r.getWidth(),16,10,dim(),true);
            text(g,s.on?juce::MidiMessage::getMidiNoteName(s.pitch,true,true,4):"REST",r.getX(),r.getY()+28,r.getWidth(),24,15,s.on?paper():dim(),true);
            text(g,(s.tie?juce::String("TIE"):juce::String(s.ratchets)+"x")+(s.protect?" *":""),r.getX(),r.getY()+54,r.getWidth(),17,10,trackColour(track_),true);
                if(r.getHeight()>=95)text(g,s.condition?conditionName(s.condition):juce::String(int(s.chance*100))+"%",r.getX(),r.getY()+74,r.getWidth(),16,9,dim(),true);
            auto lane=laneCell(c);g.setColour(juce::Colour(0xff25373b));g.fillRoundedRectangle(lane.toFloat(),3);const float v=laneValue(s);
            g.setColour(trackColour(track_).withAlpha(.65f));const int h=int((lane.getHeight()-6)*v);g.fillRect(lane.getX()+4,lane.getBottom()-3-h,lane.getWidth()-8,h);
            if(step==playStep){g.setColour(paper());g.drawRect(lane,1);}
        }
        text(g,"SELECTED STEP "+juce::String(selected_+1)+" · "+juce::String(int(selection_.size()))+" SELECTED",18,laneBottom()+8,430,19,11,dim());
        const int keyY=getHeight()-109;const int keyW=29,keyX=18;
        for(int n=0;n<24;++n){const int pitch=entryOctave_+n;if(pitch>127)break;const auto r=juce::Rectangle<int>(keyX+n*keyW,keyY,keyW-2,46);const int pc=pitch%12;const bool black=pc==1||pc==3||pc==6||pc==8||pc==10;
            g.setColour(black?juce::Colour(0xff304347):juce::Colour(0xffd8d2bb));g.fillRoundedRectangle(r.toFloat(),2);text(g,juce::MidiMessage::getMidiNoteName(pitch,true,true,4),r.getX(),r.getY()+25,r.getWidth(),16,9,black?paper():juce::Colour(0xff172326),true);}
        text(g,"Click steps to select / double-click or right-click toggles a rest / Shift selects a range / drag the property lane / Cmd-Z undo",18,getHeight()-28,getWidth()-36,18,11,dim());
    }
    void resized()override{
        tempo_.setBounds(360,10,138,48);transpose_.setBounds(510,10,170,48);play_.setBounds(getWidth()-366,18,92,34);restart_.setBounds(getWidth()-264,18,100,34);fill_.setBounds(getWidth()-154,18,136,34);
        for(int t=0;t<4;++t){const int y=86+t*43;mute_[t].setBounds(getWidth()-260,y+3,53,28);solo_[t].setBounds(getWidth()-201,y+3,53,28);reset_[t].setBounds(getWidth()-142,y+3,48,28);}
        editTab_.setBounds(18,291,112,32);chainTab_.setBounds(138,291,100,32);
        launch_.setBounds(getWidth()-400,260,123,27);cancelLaunch_.setBounds(getWidth()-267,260,119,27);launchScene_.setBounds(getWidth()-138,260,120,27);
        const bool show=!chainView_;
        for(auto* c:patternComponents())c->setVisible(show);
        chainFirst_.setVisible(!show);chainLast_.setVisible(!show);chainRun_.setVisible(!show);
        for(int row=0;row<16;++row){chainBars_[row].setVisible(!show);for(auto& c:chainPatterns_[row])c.setVisible(!show);}
        undo_.setBounds(getWidth()-180,295,76,28);redo_.setBounds(getWidth()-96,295,78,28);
        if(!show){chainRun_.setBounds(270,295,150,28);chainFirst_.setBounds(440,286,160,48);chainLast_.setBounds(620,286,160,48);
            const int rowHeight=chainRowHeight();
            for(int row=0;row<16;++row){const int y=385+row*rowHeight;chainBars_[row].setBounds(85,y,135,rowHeight-2);chainBars_[row].label.setVisible(false);chainBars_[row].slider.setBounds(0,0,135,rowHeight-2);for(int t=0;t<4;++t)chainPatterns_[row][t].setBounds(250+t*180,y,155,rowHeight-3);}return;}
        editTab_.setBounds(18,291,112,0);chainTab_.setBounds(getWidth()-300,295,110,28);
        division_.setBounds(18,357,80,28);traversal_.setBounds(108,357,130,28);scale_.setBounds(248,357,130,28);root_.setBounds(388,357,60,28);
        const int settingsWidth=(getWidth()-484)/5;std::array<Field*,5> settings={&first_,&last_,&rotation_,&swing_,&trackTranspose_};
        for(int f=0;f<5;++f)settings[f]->setBounds(466+f*settingsWidth,338,settingsWidth-8,48);
        for(int p=0;p<4;++p)pages_[p].setBounds(18+p*85,397,77,25);
        layer_.setBounds(375,397,155,25);copy_.setBounds(550,397,115,25);paste_.setBounds(675,397,115,25);clone_.setBounds(800,397,115,25);
        variation_.setBounds(925,397,getWidth()-943,25);
        const int y=laneBottom()+30;const int w=(getWidth()-36)/7;
        std::array<Field*,7> fields={&pitch_,&velocity_,&gate_,&chance_,&ratchet_,&slide_,&cv_};for(int f=0;f<7;++f)fields[f]->setBounds(18+f*w,y,w-8,48);
        condition_.setBounds(18,y+58,155,27);on_.setBounds(190,y+58,90,27);tie_.setBounds(285,y+58,125,27);protect_.setBounds(415,y+58,110,27);
        apply_.setBounds(getWidth()-300,y+58,135,27);cancel_.setBounds(getWidth()-155,y+58,137,27);
        octaveDown_.setBounds(735,getHeight()-105,70,27);octaveUp_.setBounds(815,getHeight()-105,70,27);autoStep_.setBounds(900,getHeight()-105,120,27);
    }
    void mouseDown(const juce::MouseEvent& e)override{
        grabKeyboardFocus();
        for(int t=0;t<4;++t)for(int p=0;p<8;++p)if(pad(t,p).contains(e.getPosition())){if(e.x>=pad(t,p).getRight()-24)launchPattern(t,p);else selectPattern(t,p);return;}
        if(chainView_)return;
        for(int c=0;c<16;++c){const int step=page_*16+c;
            if(stepCell(c).contains(e.getPosition())){if(e.mods.isShiftDown()){selection_.clear();for(int i=std::min(selected_,step);i<=std::max(selected_,step);++i)selection_.insert(i);}else{selected_=step;selection_={step};}
                if(e.mods.isRightButtonDown())selectedEdit([](PerformanceStep& s){s.on=!s.on;});refresh();return;}
            if(laneCell(c).contains(e.getPosition())&&!preview_){beginGesture();drawing_=true;drawLane(e.getPosition());return;}}
        if(e.y>=getHeight()-109&&e.y<getHeight()-63&&e.x>=18&&e.x<714){const int pitch=std::min(127,entryOctave_+(e.x-18)/29);selectedEdit([&](PerformanceStep& s){s.pitch=pitch;s.on=true;});if(autoStep_.getToggleState())selectStep((selected_+1)%64);}
    }
    void mouseDoubleClick(const juce::MouseEvent& e)override{if(!chainView_)for(int c=0;c<16;++c)if(stepCell(c).contains(e.getPosition()))selectedEdit([](PerformanceStep& s){s.on=!s.on;});}
    void mouseDrag(const juce::MouseEvent& e)override{if(drawing_)drawLane(e.getPosition());}
    void mouseUp(const juce::MouseEvent&)override{drawing_=false;endGesture();}
    bool keyPressed(const juce::KeyPress& key)override{
        if(key.getModifiers().isCommandDown()){const auto c=key.getTextCharacter();if(c=='z'||c=='Z'){if(key.getModifiers().isShiftDown())redo();else undo();return true;}}
        if(key.getKeyCode()==juce::KeyPress::leftKey||key.getKeyCode()==juce::KeyPress::rightKey){selectStep(selected_+(key.getKeyCode()==juce::KeyPress::rightKey?1:-1));return true;}
        if(key.getKeyCode()==juce::KeyPress::deleteKey||key.getKeyCode()==juce::KeyPress::backspaceKey){selectedEdit([](PerformanceStep& s){s.on=false;});return true;}return false;
    }
private:
    PerformanceData data_,before_;
    std::shared_ptr<Controls> controls_;
    std::map<std::string,float> values_;
    std::vector<Edit> history_,future_;
    PerformancePattern clipboard_;
    std::unique_ptr<PerformancePattern> preview_;
    std::set<int> selection_;
    int track_=0,bank_=0,page_=0,selected_=0,entryOctave_=48,layerIndex_=0;
    bool active_=false,refreshing_=false,gesturing_=false,drawing_=false,chainView_=false,hasClipboard_=false;
    juce::TextButton play_,restart_,fill_,editTab_,chainTab_,undo_,redo_,copy_,paste_,clone_,variation_,apply_,cancel_,cancelLaunch_,launchScene_,octaveDown_,octaveUp_;
    std::array<juce::TextButton,4> mute_,solo_,reset_,pages_;
    Field tempo_,transpose_,trackTranspose_,first_,last_,rotation_,swing_,pitch_,velocity_,gate_,chance_,ratchet_,slide_,cv_,chainFirst_,chainLast_;
    std::array<Field,16> chainBars_;
    std::array<std::array<juce::ComboBox,4>,16> chainPatterns_;
    juce::ComboBox division_,traversal_,scale_,root_,launch_,layer_,condition_;
    juce::ToggleButton on_,tie_,protect_,autoStep_,chainRun_;
    static juce::Colour paper(){return juce::Colour(0xffeee6cf);}static juce::Colour dim(){return juce::Colour(0xff98aaa6);}
    static juce::Colour trackColour(int t){const uint32_t c[]={0xffe8ac74,0xff80d2bf,0xffa3b4ef,0xffdca0be};return juce::Colour(c[t]);}
    static juce::String letter(int bank){return juce::String::charToString(char('A'+std::clamp(bank,0,7)));}
    static juce::String conditionName(int c){const char* names[]={"ALWAYS","FILL","! FILL","PREV","! PREV","FIRST","! FIRST","1:2","2:2","1:3","2:3","3:3","1:4","2:4","4:4"};return names[std::clamp(c,0,14)];}
    static std::string trackKey(int t,const char* field){return "track"+std::to_string(t+1)+"_"+field;}
    float get(const std::string& key)const{auto it=values_.find(key);return it==values_.end()?0:it->second;}
    std::shared_ptr<PerformanceExchange> exchange()const{return controls_?controls_->performance:nullptr;}
    PerformancePattern& pattern(){return data_.tracks[track_].patterns[bank_];}
    const PerformancePattern& shownPattern()const{return preview_?*preview_:data_.tracks[track_].patterns[bank_];}
    int gridY()const{return 438;}
    int stepHeight()const{return std::clamp(76+(getHeight()-800)/6,76,98);}
    int laneHeight()const{return std::clamp(48+(getHeight()-800)/4,48,82);}
    int laneBottom()const{return gridY()+stepHeight()+10+laneHeight();}
    int chainRowHeight()const{return (getHeight()-410)/16;}
    juce::Rectangle<int> pad(int t,int p)const{const int w=(getWidth()-440)/8;return {170+p*w,86+t*43,w-6,35};}
    juce::Rectangle<int> stepCell(int c)const{const int w=(getWidth()-36)/16;return {18+c*w,gridY(),w-4,stepHeight()};}
    juce::Rectangle<int> laneCell(int c)const{auto r=stepCell(c);return {r.getX(),gridY()+stepHeight()+10,r.getWidth(),laneHeight()};}
    static void text(juce::Graphics& g,const juce::String& s,int x,int y,int w,int h,float size,juce::Colour colour,bool centre=false){g.setColour(colour);g.setFont(juce::Font(juce::FontOptions(size)));g.drawText(s,x,y,w,h,centre?juce::Justification::centred:juce::Justification::centredLeft);}
    void button(juce::TextButton& b,const juce::String& name,std::function<void()> action){b.setButtonText(name);b.onClick=std::move(action);b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff354b4c));addAndMakeVisible(b);}
    void configure(Field& f,const juce::String& name,double lo,double hi,double step,std::function<void(double)> change){f.label.setText(name,juce::dontSendNotification);f.slider.setRange(lo,hi,step);f.slider.setNumDecimalPlacesToDisplay(step>=1?0:2);f.slider.onValueChange=[this,&f,change]{if(!refreshing_)change(f.slider.getValue());};f.slider.onDragStart=[this]{beginGesture();};f.slider.onDragEnd=[this]{endGesture();};addAndMakeVisible(f);}
    void combo(juce::ComboBox& box,const juce::StringArray& names,std::function<void(int)> change){box.addItemList(names,1);box.onChange=[this,&box,change]{if(!refreshing_)change(box.getSelectedId()-1);};addAndMakeVisible(box);}
    void toggle(juce::ToggleButton& b,const juce::String& name,std::function<void(bool)> change){b.setButtonText(name);b.onClick=[&b,change]{change(b.getToggleState());};addAndMakeVisible(b);}
    void parameter(const std::string& key,float value){values_[key]=value;if(parameterChanged)parameterChanged(key,value);refresh();}
    void publish(){validatePerformance(data_);if(dataChanged)dataChanged(data_);else if(auto e=exchange())e->publish(data_);}
    void beginGesture(){if(!gesturing_){before_=data_;gesturing_=true;}}
    void endGesture(){if(!gesturing_)return;gesturing_=false;if(!(before_==data_)){history_.push_back({before_,data_});if(history_.size()>64)history_.erase(history_.begin());future_.clear();}refresh();}
    template<class F>void edit(F change){const bool single=!gesturing_;if(single)beginGesture();change();publish();if(single)endGesture();else refresh();}
    template<class F>void selectedEdit(F change){if(preview_)return;edit([&]{for(int s:selection_)change(pattern().steps[s]);});}
    void applyDelta(const PerformanceData& from,const PerformanceData& to){
        auto set=[](auto& dst,const auto& a,const auto& b){if(!(a==b))dst=b;};
        set(data_.seed,from.seed,to.seed);set(data_.chainFirst,from.chainFirst,to.chainFirst);set(data_.chainLast,from.chainLast,to.chainLast);
        for(int t=0;t<4;++t){auto& dst=data_.tracks[t];const auto& a=from.tracks[t];const auto& b=to.tracks[t];set(dst.name,a.name,b.name);set(dst.initial,a.initial,b.initial);
            for(int p=0;p<8;++p){auto& out=dst.patterns[p];const auto& x=a.patterns[p];const auto& y=b.patterns[p];
#define PERF_RESTORE(field) set(out.field,x.field,y.field)
                PERF_RESTORE(first);PERF_RESTORE(last);PERF_RESTORE(division);PERF_RESTORE(traversal);PERF_RESTORE(rotation);PERF_RESTORE(scale);PERF_RESTORE(root);PERF_RESTORE(swing);
#undef PERF_RESTORE
                for(int s=0;s<64;++s){auto& outStep=out.steps[s];const auto& xStep=x.steps[s];const auto& yStep=y.steps[s];
#define PERF_STEP_RESTORE(field) set(outStep.field,xStep.field,yStep.field)
                    PERF_STEP_RESTORE(pitch);PERF_STEP_RESTORE(velocity);PERF_STEP_RESTORE(gate);PERF_STEP_RESTORE(chance);PERF_STEP_RESTORE(ratchets);PERF_STEP_RESTORE(condition);PERF_STEP_RESTORE(cv);PERF_STEP_RESTORE(slide);PERF_STEP_RESTORE(on);PERF_STEP_RESTORE(tie);PERF_STEP_RESTORE(protect);
#undef PERF_STEP_RESTORE
                }}}
        for(int r=0;r<16;++r){set(data_.chain[r].bars,from.chain[r].bars,to.chain[r].bars);for(int t=0;t<4;++t)set(data_.chain[r].patterns[t],from.chain[r].patterns[t],to.chain[r].patterns[t]);}publish();
    }
    void previewVariation(){preview_=std::make_unique<PerformancePattern>(pattern());uint32_t rng=data_.seed;auto next=[&]{rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
        for(int s=preview_->first;s<=preview_->last;++s){auto& step=preview_->steps[s];if(step.protect)continue;const int offsets[]={-7,-3,0,0,2,5,7};step.pitch=patternQuantize(step.pitch+offsets[next()%7],preview_->scale,preview_->root);step.velocity=std::clamp(step.velocity+(int(next()%21)-10)*.01f,.1f,1.f);step.cv=std::clamp(step.cv+(int(next()%41)-20)*.01f,-1.f,1.f);}refresh();}
    float laneValue(const PerformanceStep& s)const{switch(layerIndex_){case 0:return s.velocity;case 1:return s.gate;case 2:return s.chance;case 3:return (s.ratchets-1)/7.f;case 4:return (s.cv+1)*.5f;default:return s.slide/2;}}
    void drawLane(juce::Point<int> point){const int c=std::clamp((point.x-18)*16/(getWidth()-36),0,15);const auto r=laneCell(c);const float value=std::clamp(float(r.getBottom()-point.y)/r.getHeight(),0.f,1.f);selected_=page_*16+c;selection_.insert(selected_);auto& step=pattern().steps[selected_];
        switch(layerIndex_){case 0:step.velocity=std::max(.01f,value);break;case 1:step.gate=std::max(.01f,value);break;case 2:step.chance=value;break;case 3:step.ratchets=1+int(std::round(value*7));break;case 4:step.cv=value*2-1;break;default:step.slide=value*2;}publish();refresh();}
    std::vector<juce::Component*> patternComponents(){std::vector<juce::Component*> c={&trackTranspose_,&first_,&last_,&rotation_,&swing_,&pitch_,&velocity_,&gate_,&chance_,&ratchet_,&slide_,&cv_,&division_,&traversal_,&scale_,&root_,&layer_,&condition_,&on_,&tie_,&protect_,&autoStep_,&copy_,&paste_,&clone_,&variation_,&apply_,&cancel_,&octaveDown_,&octaveUp_};for(auto& p:pages_)c.push_back(&p);return c;}
    void refresh(){refreshing_=true;const auto& p=shownPattern();const auto& s=p.steps[selected_];
        auto set=[](Field& f,double v){f.slider.setValue(v,juce::dontSendNotification);};
        set(transpose_,get("transpose"));set(trackTranspose_,get(trackKey(track_,"transpose")));set(first_,p.first+1);set(last_,p.last+1);set(rotation_,p.rotation);set(swing_,p.swing);
        set(pitch_,s.pitch);set(velocity_,s.velocity);set(gate_,s.gate);set(chance_,s.chance);set(ratchet_,s.ratchets);set(slide_,s.slide);set(cv_,s.cv);
        division_.setSelectedId(p.division+1,juce::dontSendNotification);traversal_.setSelectedId(p.traversal+1,juce::dontSendNotification);scale_.setSelectedId(p.scale+1,juce::dontSendNotification);root_.setSelectedId(p.root+1,juce::dontSendNotification);
        launch_.setSelectedId(int(get("launch"))+1,juce::dontSendNotification);layer_.setSelectedId(layerIndex_+1,juce::dontSendNotification);condition_.setSelectedId(s.condition+1,juce::dontSendNotification);
        on_.setToggleState(s.on,juce::dontSendNotification);tie_.setToggleState(s.tie,juce::dontSendNotification);protect_.setToggleState(s.protect,juce::dontSendNotification);chainRun_.setToggleState(get("chain")>.5f,juce::dontSendNotification);
        set(chainFirst_,data_.chainFirst+1);set(chainLast_,data_.chainLast+1);
        for(int row=0;row<16;++row){set(chainBars_[row],data_.chain[row].bars);for(int t=0;t<4;++t)chainPatterns_[row][t].setSelectedId(data_.chain[row].patterns[t]+1,juce::dontSendNotification);}
        for(int t=0;t<4;++t){mute_[t].setColour(juce::TextButton::buttonColourId,get(trackKey(t,"mute"))>.5f?juce::Colour(0xffa34f4b):juce::Colour(0xff354b4c));solo_[t].setColour(juce::TextButton::buttonColourId,get(trackKey(t,"solo"))>.5f?trackColour(t):juce::Colour(0xff354b4c));}
        for(auto* field:{&first_,&last_,&rotation_,&swing_,&pitch_,&velocity_,&gate_,&chance_,&ratchet_,&slide_,&cv_})field->setEnabled(!preview_);
        for(auto* box:{&division_,&traversal_,&scale_,&root_,&condition_})box->setEnabled(!preview_);
        on_.setEnabled(!preview_);tie_.setEnabled(!preview_);protect_.setEnabled(!preview_);
        undo_.setEnabled(!history_.empty());redo_.setEnabled(!future_.empty());paste_.setEnabled(hasClipboard_&&!preview_);apply_.setEnabled(bool(preview_));cancel_.setEnabled(bool(preview_));refreshing_=false;repaint();
    }
    void paintChain(juce::Graphics& g){text(g,"ROW",30,355,50,22,11,dim());text(g,"BARS",90,355,130,22,11,dim());for(int t=0;t<4;++t)text(g,juce::String(data_.tracks[t].name),250+t*180,355,155,22,12,trackColour(t));
        const auto e=exchange();const int h=chainRowHeight();
        if(e&&active_&&e->chainRow>=0)text(g,"PLAYING ROW "+juce::String(e->chainRow+1)+" / "+juce::String(e->chainRemaining.load(),1)+" BEATS LEFT",790,341,getWidth()-808,22,11,paper());
        for(int row=0;row<16;++row){const int y=385+row*h;const bool in=row>=data_.chainFirst&&row<=data_.chainLast,playing=e&&active_&&e->chainRow==row;
            g.setColour(playing?juce::Colour(0xff42665b):in?juce::Colour(0xff26383b):juce::Colour(0xff1c2b2e));g.fillRoundedRectangle(18,float(y-1),float(getWidth()-36),float(h-2),3);
            text(g,juce::String(row+1)+(playing?" >":""),30,y,50,h-3,12,playing?paper():dim());}
        text(g,"Each row launches all four patterns and lasts the stated number of bars. The first/last rows form a repeating chain.",18,getHeight()-30,getWidth()-36,20,11,dim());
    }
    void timerCallback()override{if(auto e=exchange())e->collect();if(getTempo)tempo_.slider.setValue(getTempo(),juce::dontSendNotification);repaint();}
};
class PerformanceWindow final : public juce::DocumentWindow {
public:
    explicit PerformanceWindow(PerformanceEditor* editor):DocumentWindow("Optisynth 0.0.9 / Performance sequencer",juce::Colour(0xff172326),DocumentWindow::closeButton){setUsingNativeTitleBar(true);setContentOwned(editor,true);setResizable(true,false);setResizeLimits(1120,800,1800,1200);centreWithSize(1260,900);}
    void closeButtonPressed()override{setVisible(false);}
};
class PerformanceLauncher final : public juce::Component,private juce::Timer {
public:
    std::function<void(const PerformanceData&)> dataChanged;
    std::function<void(const std::string&,float)> parameterChanged;
    std::function<void()> onPlay,onRestart;
    std::function<double()> getTempo;
    std::function<void(double)> changeTempo;
    PerformanceLauncher(std::shared_ptr<Controls> controls,std::map<std::string,float> values):controls_(std::move(controls)),values_(std::move(values)),data_(defaultPerformance()){
        addAndMakeVisible(open_);open_.setButtonText("OPEN PERFORMANCE SEQUENCER");open_.onClick=[this]{openEditor();};startTimerHz(20);
    }
    void setData(const PerformanceData& data){data_=data;if(editor_)editor_->setData(data);repaint();}
    void setValue(const std::string& key,float value){values_[key]=value;if(editor_)editor_->setValue(key,value);}
    void setActive(bool active){active_=active;if(editor_)editor_->setActive(active);repaint();}
    bool hasEditorFocus()const{return editor_&&editor_->hasKeyboardFocus(true);}
    PerformanceEditor* openEditor(bool visible=true){if(!window_){editor_=new PerformanceEditor(data_,controls_,values_);editor_->dataChanged=[this](const PerformanceData& d){data_=d;if(dataChanged)dataChanged(d);};editor_->parameterChanged=[this](const std::string& key,float value){values_[key]=value;if(parameterChanged)parameterChanged(key,value);};
            editor_->onPlay=[this]{if(onPlay)onPlay();};editor_->onRestart=[this]{if(onRestart)onRestart();};editor_->getTempo=[this]{return getTempo?getTempo():120.;};editor_->changeTempo=[this](double v){if(changeTempo)changeTempo(v);};window_=std::make_unique<PerformanceWindow>(editor_);}
        editor_->setTempoDisplay(getTempo?getTempo():120.);editor_->setActive(active_);window_->setVisible(visible);if(visible)window_->toFront(true);return editor_;}
    void resized()override{open_.setBounds(8,getHeight()-43,getWidth()-16,35);}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff26383b));g.setColour(juce::Colour(0xffeee6cf));g.setFont(juce::Font(juce::FontOptions(13)));
        auto e=controls_?controls_->performance:nullptr;for(int t=0;t<4;++t){const int y=12+t*35;g.drawText(juce::String(t+1)+"  "+juce::String(data_.tracks[t].name),12,y,145,22,juce::Justification::centredLeft);
            const int p=e?e->playing[t].load():data_.tracks[t].initial;g.drawText("PATTERN "+juce::String::charToString(char('A'+p))+"  /  "+(active_&&e?"STEP "+juce::String(e->steps[t]+1):"STOPPED"),160,y,getWidth()-172,22,juce::Justification::centredLeft);}
        g.setFont(juce::Font(juce::FontOptions(11)));g.drawText("Independent NOTES / GATE / BEND / CV per track",12,157,getWidth()-24,25,juce::Justification::centredLeft);}
private:
    std::shared_ptr<Controls> controls_;std::map<std::string,float> values_;PerformanceData data_;bool active_=false;juce::TextButton open_;
    PerformanceEditor* editor_=nullptr;std::unique_ptr<PerformanceWindow> window_;
    void timerCallback()override{if(controls_&&controls_->performance)controls_->performance->collect();repaint();}
};
}
