#include "PluginEditor.h"
#include "TypingKeyboard.h"
#include <cmath>
#include <functional>
#if JUCE_MAC
 #include <ApplicationServices/ApplicationServices.h>
#endif

namespace {
using namespace juce;
constexpr int panelWidth=1200, panelHeight=774;
const Colour cyan(0xff8bd9f4), dark(0xff22292c), light(0xffd3d3c8), ink(0xff252a2b);
Font uiFont(float size, bool bold=false) { return Font(FontOptions("Arial Narrow",size,bold?Font::bold:Font::plain)); }

class SpectralLook : public LookAndFeel_V4 {
public:
    SpectralLook() {
        setColour(ComboBox::backgroundColourId,Colour(0xff171d1f));
        setColour(ComboBox::textColourId,Colour(0xffe0e6e6));
        setColour(ComboBox::outlineColourId,Colour(0xff4a5558));
        setColour(PopupMenu::backgroundColourId,Colour(0xff252e31));
        setColour(PopupMenu::highlightedBackgroundColourId,Colour(0xff426474));
        setColour(TextButton::buttonColourId,Colour(0xff303a3e));
        setColour(TextButton::buttonOnColourId,cyan);
        setColour(TextButton::textColourOnId,Colour(0xff112027));
        setColour(TextButton::textColourOffId,Colour(0xffcedce1));
        setColour(Slider::textBoxTextColourId,Colour(0xffdce8ea));
        setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
        setColour(Slider::textBoxBackgroundColourId,Colours::transparentBlack);
        setColour(ToggleButton::textColourId,Colour(0xffdce8ea));
        setColour(TextEditor::backgroundColourId,Colour(0xff151c1f));
        setColour(TextEditor::textColourId,Colour(0xffe2f4fa));
    }
    Font getComboBoxFont(ComboBox&) override { return uiFont(12); }
    Font getTextButtonFont(TextButton&,int) override { return uiFont(12); }
    Font getLabelFont(Label& l) override { return l.getFont(); }
    void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float position,
                          float start,float end,Slider& s) override {
        const bool pale=(bool)s.getProperties()["pale"];
        auto r=Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(5);
        const float diameter=jmin(r.getWidth(),r.getHeight()), radius=diameter*.5f;
        const auto c=r.getCentre(); const float angle=start+position*(end-start);
        g.setColour(pale?Colour(0xffa4a69d):Colour(0xff111719));
        g.fillEllipse(c.x-radius+1,c.y-radius+2,diameter,diameter);
        ColourGradient face(pale?Colour(0xffeeeeDF):Colour(0xff4c5559),c.x-radius,c.y-radius,
                            pale?Colour(0xffb4b6ac):Colour(0xff242b2e),c.x+radius,c.y+radius,false);
        g.setGradientFill(face); g.fillEllipse(c.x-radius+3,c.y-radius+3,diameter-6,diameter-6);
        Path ring; ring.addCentredArc(c.x,c.y,radius,radius,0,start,end,true);
        g.setColour(pale?Colour(0xffb5b8af):Colour(0xff4a5559));
        g.strokePath(ring,PathStrokeType(1.6f));
        Path value; value.addCentredArc(c.x,c.y,radius,radius,0,start,angle,true);
        g.setColour(s.isEnabled()?cyan:Colours::grey);
        g.strokePath(value,PathStrokeType(2.5f));
        g.setColour(pale?Colour(0xff454a49):Colour(0xffbecacf));
        g.drawLine(c.x+std::sin(angle)*radius*.38f,c.y-std::cos(angle)*radius*.38f,
                   c.x+std::sin(angle)*radius*.78f,c.y-std::cos(angle)*radius*.78f,2);
    }
    void drawButtonBackground(Graphics& g,Button& b,const Colour&,bool over,bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(.8f);
        Colour c=b.getToggleState()?cyan:Colour(0xff323d41);
        if(!b.isEnabled()) c=Colour(0xff303638);
        if(over) c=c.brighter(.15f);
        if(down) c=c.darker(.15f);
        g.setGradientFill(ColourGradient(c.brighter(.12f),0,0,c.darker(.25f),0,r.getBottom(),false));
        g.fillRoundedRectangle(r,2);
        g.setColour(Colour(0xff101719)); g.drawRoundedRectangle(r,2,1);
    }
};

class Knob : public juce::Component {
public:
    bool dragging=false;
    Slider slider{Slider::RotaryHorizontalVerticalDrag,Slider::TextBoxBelow};
    Label caption;
    Knob(const String& title,bool pale) {
        addAndMakeVisible(slider); addAndMakeVisible(caption);
        slider.setTextBoxStyle(Slider::TextBoxBelow,false,62,14);
        slider.getProperties().set("pale",pale);
        if(pale) slider.setColour(Slider::textBoxTextColourId,ink);
        slider.setRotaryParameters(MathConstants<float>::pi*1.22f,MathConstants<float>::pi*2.78f,true);
        caption.setText(title,dontSendNotification); caption.setFont(uiFont(11));
        caption.setJustificationType(Justification::centred);
        caption.setColour(Label::textColourId,pale?ink:Colour(0xffc7d0d2));
        caption.setInterceptsMouseClicks(false,false);
    }
    void resized() override {
        slider.setBounds(0,0,getWidth(),getHeight()-13);
        caption.setBounds(0,getHeight()-13,getWidth(),13);
    }
};

class Display : public juce::Component {
public:
    std::function<void(Graphics&,Rectangle<float>)> draw;
    std::function<void(const MouseEvent&)> edit;
    std::function<void()> click;
    void paint(Graphics& g) override {
        auto r=getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(Colour(0xff353f43),0,0,Colour(0xff1a2023),0,r.getHeight(),false));
        g.fillRoundedRectangle(r,3); g.setColour(Colour(0xff0c1113));g.drawRoundedRectangle(r.reduced(1),3,2);
        if(draw) draw(g,r.reduced(8));
    }
    void mouseDown(const MouseEvent& e) override { if(edit) edit(e); else if(click) click(); }
    void mouseDrag(const MouseEvent& e) override { if(edit) edit(e); }
};

class Surface : public juce::Component {
public:
    std::function<void(Graphics&)> draw;
    std::function<void()> focus;
    void paint(Graphics& g) override { if(draw) draw(g); }
    void mouseDown(const MouseEvent&) override { if(focus) focus(); }
};

struct FxParam { const char* key; const char* label; float lo,hi,value; const char* unit; };
std::vector<FxParam> effectParameters(const std::string& type) {
    using P=FxParam; std::vector<P> p;
    if(type=="reverb") p={{"predelay_ms","PRE DELAY",0,2000,25," ms"},{"time_s","TIME",.1,50,2.5," s"},
        {"damping","DAMP",0,.95,.15,""},{"diffusors","DIFFUSORS",2,6,4,""},{"modulation","MOD ON",0,1,0,""},
        {"mod_depth","MOD DEPTH",0,20,10,""},{"predelay_beats","PRE SYNC",0,16,0," beats"}};
    else if(type=="delay_stereo" || type=="delay_pingpong") {
        p={{"time_l_ms","TIME LEFT",1,8000,300," ms"},{"time_r_ms","TIME RIGHT",1,8000,450," ms"},
           {"feedback","FEEDBACK",0,.99,.4,""},{"sync_l_beats","SYNC LEFT",0,16,0," beats"},
           {"sync_r_beats","SYNC RIGHT",0,16,0," beats"}};
        if(type=="delay_pingpong") {
            p.insert(p.end(),{{"time_c_ms","TIME PRE",0,8000,0," ms"},{"sync_c_beats","SYNC PRE",0,16,0," beats"},
                {"balance","PRE L/R",0,1,.5,""},{"pan_l","PAN LEFT",-1,1,-1,""},{"pan_r","PAN RIGHT",-1,1,1,""}});
        } else p.insert(p.end(),{{"highpass_hz","HIGH PASS",20,20480,20," Hz"},
            {"lowpass_hz","LOW PASS",20,20480,20480," Hz"},{"cross","CROSS",0,1,0,""},
            {"filters_in_feedback","FILTER LOOP",0,1,1,""},{"stereo","STEREO",-1,1,0,""}});
    } else if(type=="delay_mod") p={{"time_ms","TIME",1,8000,300," ms"},{"feedback","FEEDBACK",0,.99,.4,""},
        {"mod_depth","MOD DEPTH",0,1,0,""},{"mod_rate_hz","MOD RATE",.01,20,.5," Hz"},
        {"stereo","STEREO",-1,1,0,""},{"highpass_hz","HIGH PASS",20,20480,20," Hz"},
        {"lowpass_hz","LOW PASS",20,20480,20480," Hz"},{"sync_beats","SYNC",0,16,0," beats"},
        {"mod_sync_beats","MOD SYNC",0,16,0," beats"}};
    else if(type=="chorus") p={{"time_ms","TIME",.5,40,8," ms"},{"rate_hz","RATE",.01,10,.5," Hz"},
        {"rate_spread","RATE SPREAD",0,1,.3,""},{"stereo","STEREO",0,1,.5,""},{"invert","INVERT",0,1,0,""}};
    else if(type=="phaser") p={{"stages","STAGES",1,24,6,""},{"center_hz","FREQUENCY",50,12000,800," Hz"},
        {"depth","DEPTH",0,1,.5,""},{"feedback","FEEDBACK",0,.95,0,""},{"fb_invert","INVERT",0,1,0,""},
        {"stereo","STEREO",0,1,.5,""},{"rate_hz","RATE",.01,20,.3," Hz"},{"sync_beats","SYNC",0,16,0," beats"}};
    else if(type=="flanger") p={{"time_ms","TIME",.05,85,2," ms"},{"time_depth","DEPTH",0,1,.5,""},
        {"feedback","FEEDBACK",0,.95,.5,""},{"invert","INVERT",0,1,0,""},{"pan_depth","PAN DEPTH",0,1,0,""},
        {"stereo","STEREO",0,1,.5,""},{"rate_hz","RATE",.01,20,.2," Hz"},{"sync_beats","SYNC",0,16,0," beats"}};
    else if(type=="filter") p={{"type","TYPE",0,3,0,""},{"cutoff_hz","CUTOFF",20,20480,2000," Hz"},
        {"resonance","RESONANCE",0,1,0,""},{"saturation","SATURATION",0,1,0,""},
        {"mod_rate_hz","MOD RATE",.01,20,.5," Hz"},{"mod_depth","MOD DEPTH",0,1,0,""},
        {"level_db","LEVEL",-60,24,0," dB"},{"sync_beats","SYNC",0,16,0," beats"}};
    else if(type=="compressor") p={{"ratio","RATIO",1,40,2,":1"},{"threshold_db","THRESHOLD",-60,0,-12," dB"},
        {"attack_ms","ATTACK",.05,1000,10," ms"},{"release_ms","RELEASE",1,5000,200," ms"},{"out_db","OUT VOL",-60,24,0," dB"}};
    else if(type=="crusher") p={{"bits","RESOLUTION",1,16.01,16," bit"},{"divider","DIVIDER",1,32,1,""},
        {"divider_mode","DIV MODE",0,2,1,""},{"dither","DITHER",0,1,0,""},{"auto_volume","AUTO VOL",0,1,0,""},
        {"filter_type","FILTER TYPE",0,3,0,""},{"cutoff_hz","CUTOFF",20,20480,20480," Hz"},
        {"resonance","RESONANCE",0,1,0,""},{"filter_post","POST FILTER",0,1,0,""}};
    else if(type=="gator") p={{"pattern_l","PATTERN LEFT",0,65535,43690,""},{"pattern_r","PATTERN RIGHT",0,65535,21845,""},
        {"link","LINK",0,2,0,""},{"smooth","SMOOTH",0,1,.1,""},{"step_beats","STEP",.015625,64,.25," beats"}};
    else if(type=="eq") p={{"freq1_hz","FREQ 1",20,20480,500," Hz"},{"gain1_db","GAIN 1",-24,24,0," dB"},{"q1","Q 1",.08,12.5,1,""},
        {"freq2_hz","FREQ 2",20,20480,2000," Hz"},{"gain2_db","GAIN 2",-24,24,0," dB"},{"q2","Q 2",.08,12.5,1,""},
        {"low_hz","LOW FREQ",20,1000,100," Hz"},{"low_db","LOW GAIN",-24,24,0," dB"},
        {"high_hz","HIGH FREQ",440,11000,8000," Hz"},{"high_db","HIGH GAIN",-24,24,0," dB"}};
    else if(type=="wah") p={{"cutoff_hz","FREQUENCY",20,20480,400," Hz"},{"resonance","RESONANCE",0,1,.5,""},
        {"sensitivity","SENSITIVITY",-1,1,.01,""},{"attack_s","ATTACK",.001,8,.01," s"},
        {"release_s","RELEASE",.001,8,.1," s"},{"level","LEVEL",0,10,1,""}};
    if(type!="none") p.push_back({"wet","WET",0,1,type=="reverb"?.25f:type.find("delay")==0?.3f:
        (type=="chorus"||type=="phaser"||type=="flanger")?.5f:1.f,""});
    for(auto& item:p)for(const auto& spec:partials::effectControlSpecs)
        if(type==spec.type&&std::string(item.key)==spec.key){item.lo=spec.minimum;item.hi=spec.maximum;item.value=spec.initial;break;}
    return p;
}
} // namespace

class PartialsEditor::Impl {
public:
    using Change=std::function<void(double)>;
    PartialsEditor& owner;
    PartialsProcessor& proc;
    SpectralLook look;
    Surface surface;
    juce::MidiKeyboardComponent keyboard;
    juce::Label status,typingStatus;
    juce::TooltipWindow tips{&owner,650};
    partials::Patch patch;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> liveAttachments;
    juce::Array<juce::File> files;
    std::unique_ptr<juce::FileChooser> chooser;
    unsigned revision=0;
    int osc=0,envLeft=7,envRight=0,fx=0,matrixPage=0,arpPage=0,waveView=2;
    int spectrumMode=-1; // -1 main, 0 A, 1 B, 2 filter
    bool spectrumPhase=false,dirty=false,rebuild=false,capsEnabled=false;
    double changedAt=0,lastSubmitted=0;
    TypingKeyboard typing;
    Impl(PartialsEditor& e,PartialsProcessor& p):owner(e),proc(p),keyboard(p.keyboardState,juce::MidiKeyboardComponent::horizontalKeyboard) {
        owner.setLookAndFeel(&look);
        owner.addAndMakeVisible(surface); owner.addAndMakeVisible(keyboard);
        owner.addAndMakeVisible(status); owner.addAndMakeVisible(typingStatus);
        surface.draw=[this](juce::Graphics& g){paintPanel(g);};
        surface.focus=[this]{owner.grabKeyboardFocus();};
        status.setFont(uiFont(12)); typingStatus.setFont(uiFont(12));
        status.setColour(juce::Label::textColourId,juce::Colour(0xffd2dde1));
        typingStatus.setColour(juce::Label::textColourId,cyan);
        keyboard.clearKeyMappings(); keyboard.setLowestVisibleKey(36); keyboard.setKeyWidth(19);
        keyboard.setOctaveForMiddleC(4);
        keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,cyan.withAlpha(.6f));
        patch=proc.editablePatch(); revision=proc.patchRevision();
        scanFolder(proc.getPresetFolder());
        build();
    }
    ~Impl() {
        releaseTyping();
        submit();
        owner.setLookAndFeel(nullptr);
    }
    template<class T> T& own(std::unique_ptr<T> c,const juce::String& id,juce::Rectangle<int> bounds) {
        auto& result=*c; result.setName(id);result.setComponentID(id);result.setBounds(bounds);
        surface.addAndMakeVisible(result);controls.push_back(std::move(c));return result;
    }
    void changed() {dirty=true;changedAt=juce::Time::getMillisecondCounterHiRes();surface.repaint();}
    void submit() {if(dirty){partials::applyLiveValues(patch,proc.hostLiveValues());proc.applyEditedPatch(patch);dirty=false;lastSubmitted=juce::Time::getMillisecondCounterHiRes();}}
    std::pair<int,float> liveControl(const juce::String& id) const {
        const char* ids[]={"osc.wavemix","osc.symmetry","osc.volume","filter.cutoff","filter.resonance","filter.pan","filter.volume","filter.env",
                           "osc.xmod0.amount","osc.xmod1.amount","filter.xmod0.amount","filter.xmod1.amount"};
        const float scales[]={100,100,1,1,100,100,1,100,100,100,100,100};
        for(int i=0;i<partials::kLiveOscControls;++i)if(id==ids[i])return {osc*partials::kLiveOscControls+i,scales[i]};
        if(id=="lfo.rate")return {partials::kLiveLfoStart+envRight-4,1};
        if(id=="main.volume")return {partials::kLiveMain,1};
        if(id=="fx.wet")return {partials::kLiveWetStart+fx,1};
        if(id.startsWith("env.left.")||id.startsWith("env.right.")){
            const bool right=id.startsWith("env.right.");
            const int e=right?4+envRight:envLeft<4?envLeft:envLeft<7?8+envLeft-4:11;
            const auto field=id.substring(right?10:9);
            const juce::StringArray fields{"attack","hold","decay","sustain","fade","release","A curve","D curve","R curve"};
            const int f=fields.indexOf(field);
            if(f>=0)return {partials::kLiveEnvStart+e*partials::kEnvFields+f,f==3?100.f:1.f};
        }
        if(id.startsWith("fx.")){
            for(int f=0;f<partials::kEffectControls;++f){const auto& s=partials::effectControlSpecs[f];
                if(patch.fx[fx].type==s.type&&id.substring(3)==s.key)return {partials::kLiveFxStart+fx*partials::kEffectControls+f,1};}
        }
        return {-1,1};
    }
    void requestBuild() {rebuild=true;}
    void label(const juce::String& text,juce::Rectangle<int> b,bool pale=false,float size=12) {
        auto l=std::make_unique<juce::Label>();l->setText(text,juce::dontSendNotification);l->setFont(uiFont(size));
        l->setColour(juce::Label::textColourId,pale?ink:juce::Colour(0xffc9d3d6));
        own(std::move(l),text,b);
    }
    juce::TextButton& button(const juce::String& id,const juce::String& text,juce::Rectangle<int> b,
                            std::function<void()> cb,bool selected=false) {
        auto item=std::make_unique<juce::TextButton>(text);item->setWantsKeyboardFocus(false);
        item->setToggleState(selected,juce::dontSendNotification);
        item->onClick=[this,cb=std::move(cb)]{if(owner.getPeer()!=nullptr)owner.grabKeyboardFocus();cb();};
        return own(std::move(item),id,b);
    }
    Knob& knob(const juce::String& id,const juce::String& title,juce::Rectangle<int> b,
               double lo,double hi,double value,Change set,const juce::String& unit={},bool pale=false,double step=.001,double skew=1) {
        auto item=std::make_unique<Knob>(title,pale);
        item->slider.setRange(lo,hi,step);item->slider.setSkewFactor(skew);
        item->slider.setValue(value,juce::dontSendNotification);item->slider.setTextValueSuffix(unit);
        item->slider.setNumDecimalPlacesToDisplay(step>=1?0:step>=.1?1:step>=.01?2:3);
        item->slider.setName(id); item->slider.setComponentID(id);
        item->slider.onValueChange=[this,s=&item->slider,set]{set(s->getValue());changed();};
        item->slider.onDragStart=[this]{owner.grabKeyboardFocus();};
        item->slider.onDragEnd=[this]{submit();};
        const auto binding=liveControl(id);
        if(binding.first>=0) {
            auto* parameter=proc.params.getParameter(partials::liveControlSpecs()[binding.first].id);
            auto attachment=std::make_unique<juce::ParameterAttachment>(*parameter,
                [this,s=&item->slider,set,scale=binding.second](float value){s->setValue(value*scale,juce::dontSendNotification);set(value*scale);surface.repaint();},nullptr);
            auto* a=attachment.get();auto* k=item.get();
            item->slider.onValueChange=[k,a,scale=binding.second]{
                if(k->dragging)a->setValueAsPartOfGesture((float)k->slider.getValue()/scale);
                else a->setValueAsCompleteGesture((float)k->slider.getValue()/scale);
            };
            item->slider.onDragStart=[this,k,a]{k->dragging=true;a->beginGesture();if(owner.getPeer()!=nullptr)owner.grabKeyboardFocus();};
            item->slider.onDragEnd=[k,a]{k->dragging=false;a->endGesture();};
            a->sendInitialUpdate();liveAttachments.push_back(std::move(attachment));
        }
        item->slider.setTooltip(title+" | drag to adjust, click the value to type");
        return own(std::move(item),id+".control",b);
    }
    void fk(const juce::String& id,const juce::String& title,int x,int y,float& v,float lo,float hi,
            const juce::String& suffix={},bool pale=false,float scale=1,float skew=1,int width=64) {
        const int height=y<246 && y+72>241?241-y:72;
        knob(id,title,{x,y,width,height},lo*scale,hi*scale,v*scale,[ptr=&v,scale](double n){*ptr=(float)n/scale;},
             suffix,pale,(hi-lo)*scale>1000?1:suffix==" s"?.001:.01,skew);
    }
    juce::ComboBox& choice(const juce::String& id,juce::Rectangle<int> b,const juce::StringArray& options,int selected,
                          std::function<void(int)> cb) {
        auto c=std::make_unique<juce::ComboBox>();c->addItemList(options,1);c->setSelectedItemIndex(selected,juce::dontSendNotification);
        c->onChange=[this,ptr=c.get(),cb]{cb(ptr->getSelectedItemIndex());changed();if(owner.getPeer()!=nullptr)owner.grabKeyboardFocus();};
        return own(std::move(c),id,b);
    }
    void number(const juce::String& id,const juce::String& caption,juce::Rectangle<int> b,
                int value,int lo,int hi,std::function<void(int)> cb,bool pale=false) {
        auto s=std::make_unique<juce::Slider>(juce::Slider::LinearBar,juce::Slider::TextBoxLeft);
        s->setRange(lo,hi,1);s->setValue(value,juce::dontSendNotification);
        s->setTextValueSuffix(caption);s->setColour(juce::Slider::textBoxTextColourId,pale?ink:cyan);
        s->onValueChange=[this,ptr=s.get(),cb]{cb((int)ptr->getValue());changed();};
        own(std::move(s),id,b);
    }
    void scanFolder(juce::File folder) {
        if(!folder.isDirectory()) folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Music docs/Code/optisynth/presets-imported");
        if(folder.isDirectory()) {
            files=folder.findChildFiles(juce::File::findFiles,false,"*.ptch");
            std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.getFileName().compareNatural(b.getFileName())<0;});
            proc.setPresetFolder(folder);
        }
    }
    void chooseFolder();
    void save();
    void loadIndex(int);
    void build();
    void paintPanel(juce::Graphics&);
    void oscillatorPanel();
    void envelopePanel(bool right);
    void matrixPanel();
    void arpPanel();
    void effectsPanel();
    void spectralPanel();
    void bottomPanel();
    void tick();
    void resized();
    bool typingActive() const;
    bool key(const juce::KeyPress&);
    void pollTyping();
    void releaseTyping();
    void waveform(juce::Graphics&,juce::Rectangle<float>);
    void filterCurve(juce::Graphics&,juce::Rectangle<float>);
};

void PartialsEditor::Impl::paintPanel(juce::Graphics& g) {
    g.fillAll(dark);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff343c3f),0,0,dark,0,246,false));
    g.fillRect(0,0,panelWidth,246);
    g.setColour(light);g.fillRect(0,246,panelWidth,spectrumMode<0?250:458);
    if(spectrumMode<0){g.setColour(juce::Colour(0xff30383b));g.fillRect(410,270,370,218);}
    g.setColour(juce::Colour(0xff0e1416));g.fillRect(0,704,panelWidth,70);
    g.setColour(juce::Colour(0xff596165));
    for(int y:{25,245,495,521,703}) g.drawHorizontalLine(y,8,1192);
    g.drawVerticalLine(600,8,238);g.drawVerticalLine(600,505,698);
    g.setFont(uiFont(14));g.setColour(juce::Colour(0xffe0e5e5));
    g.drawText("OSCILLATOR",12,2,135,23,juce::Justification::centredLeft);
    g.drawText("FILTER",612,2,130,23,juce::Justification::centredLeft);
    if(spectrumMode<0){
    g.drawText("ARPEGGIATOR",12,497,140,23,juce::Justification::centredLeft);
    g.drawText("EFFECTS",612,497,100,23,juce::Justification::centredLeft);
    g.setColour(ink);g.drawText("ENVELOPE",90,247,145,22,juce::Justification::centredLeft);
    g.drawText("MOD MATRIX",418,247,145,22,juce::Justification::centredLeft);
    g.drawText(envRight<4?"ENVELOPE":"LFO",850,247,140,22,juce::Justification::centredLeft);
    }
    g.setFont(uiFont(20,true));g.setColour(juce::Colour(0xffb7c8ce));
    g.drawText("OPTISPECTRAL",14,741,145,26,juce::Justification::centredLeft);
}

void PartialsEditor::Impl::build() {
    outputAttachment.reset();
    liveAttachments.clear();
    controls.clear();
    oscillatorPanel();
    if(spectrumMode<0) {envelopePanel(false);envelopePanel(true);matrixPanel();arpPanel();effectsPanel();}
    else spectralPanel();
    bottomPanel();surface.repaint();
}

void PartialsEditor::Impl::waveform(juce::Graphics& g,juce::Rectangle<float> r) {
    auto& o=patch.osc[(size_t)osc];juce::Path path;
    std::array<float,256> values{};float peak=.001f;
    for(int i=0;i<256;++i) {
        float a=0,b=0;
        for(int h=1;h<=partials::kHarmonics;++h) {
            a+=o.waveA.amplitude(h)*std::sin(juce::MathConstants<float>::twoPi*(h*i/255.f+o.waveA.phase[(size_t)h]));
            b+=o.waveB.amplitude(h)*std::sin(juce::MathConstants<float>::twoPi*(h*i/255.f+o.waveB.phase[(size_t)h]));
        }
        values[(size_t)i]=waveView==0?a:waveView==1?b:a*(1-o.waveMix)+b*o.waveMix;
        peak=std::max(peak,std::abs(values[(size_t)i]));
    }
    g.setColour(juce::Colour(0xff586368));g.drawHorizontalLine((int)r.getCentreY(),r.getX(),r.getRight());
    for(int i=0;i<256;++i) {
        const float x=r.getX()+i*r.getWidth()/255,y=r.getCentreY()-values[(size_t)i]/peak*r.getHeight()*.43f;
        if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
    }
    g.setColour(cyan);g.strokePath(path,juce::PathStrokeType(1.2f));
}
void PartialsEditor::Impl::filterCurve(juce::Graphics& g,juce::Rectangle<float> r) {
    juce::Path p;auto& f=patch.osc[(size_t)osc].filter;
    for(int i=0;i<partials::kCurvePoints;++i) {
        float x=r.getX()+i*r.getWidth()/(partials::kCurvePoints-1),
              y=r.getBottom()-juce::jlimit(0.f,1.f,(f.curve.db[(size_t)i]+96)/96)*r.getHeight();
        if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);
    }
    g.setColour(juce::Colour(0xff586368));g.drawVerticalLine((int)r.getCentreX(),r.getY(),r.getBottom());
    g.setColour(cyan);g.strokePath(p,juce::PathStrokeType(1.3f));
}
void PartialsEditor::Impl::oscillatorPanel() {
    auto& o=patch.osc[(size_t)osc];auto& f=o.filter;
    for(int i=0;i<4;++i) button("osc.select"+juce::String(i+1),juce::String(i+1),{12,34+i*25,28,24},
        [this,i]{osc=i;requestBuild();},i==osc);
    button("osc.on",o.on?"ON":"OFF",{145,3,40,20},[this,&o]{o.on=!o.on;changed();requestBuild();},o.on);
    label("TUNE",{10,140,47,18});
    number("osc.semitone"," st",{10,160,51,22},o.semitone,-72,72,[&o](int n){o.semitone=n;});
    fk("osc.cent","CENT",7,184,o.cent,-100,100,"",false,1,1,56);
    button("osc.keytrack","TRACK",{65,217,49,23},[this,&o]{o.keyTrack=!o.keyTrack;changed();requestBuild();},o.keyTrack);
    auto wave=std::make_unique<Display>();wave->draw=[this](auto& g,auto r){waveform(g,r);};
    wave->click=[this]{spectrumMode=waveView==1?1:0;requestBuild();};
    own(std::move(wave),"wave.display",{65,34,218,175});
    for(int i=0;i<3;++i) button("wave.view"+juce::String(i),i==0?"A":i==1?"B":"MIX",{119+i*40,217,38,23},
        [this,i]{waveView=i;requestBuild();},waveView==i);
    button("wave.edit","EDIT",{244,217,39,23},[this]{spectrumMode=waveView==1?1:0;requestBuild();});
    fk("osc.wavemix","A  WAVE  B",292,30,o.waveMix,0,1,"",false,100);
    fk("osc.symmetry","SYM",292,104,o.symmetry,.01f,.99f,"%",false,100);
    fk("osc.phase","PHASE",292,178,o.startPhase,0,1,"",false,360);
    label("SPREAD",{373,30,105,19});
    number("osc.voices"," voices",{368,54,68,24},o.unisonVoices,1,6,[&o](int n){o.unisonVoices=n;});
    button("osc.layout","4b",{441,54,30,24},[this,&o]{o.unisonLayout=1-o.unisonLayout;changed();requestBuild();},o.unisonLayout==1);
    fk("osc.detune","DETUNE",363,88,o.unisonDetune,0,100," ct",false,1,.5f,57);
    fk("osc.spreadvol","VOL",420,88,o.unisonVolume,0,1,"%",false,100,1,57);
    fk("osc.stereo","STEREO",363,166,o.unisonStereo,0,1,"%",false,100,1,57);
    button("osc.free","FREE",{427,197,46,24},[this,&o]{o.freeRun=!o.freeRun;changed();requestBuild();},o.freeRun);
    const juce::StringArray sources{"OSC 1","OSC 2","OSC 3","OSC 4","FIL 1","FIL 2","FIL 3","FIL 4"};
    for(int i=0;i<2;++i) {
        auto& m=o.xmod[(size_t)i];int y=31+i*105;
        choice("osc.xmod"+juce::String(i)+".type",{480,y,60,20},{"AM","FM","PM"},m.type,[&m](int n){m.type=n;});
        fk("osc.xmod"+juce::String(i)+".amount","DEPTH",480,y+20,m.amount,0,1,"%",false,100,1,60);
        choice("osc.xmod"+juce::String(i)+".source",{480,y+88,60,20},sources,m.source,[&m](int n){m.source=n;});
    }
    fk("osc.volume","GAIN",540,89,o.volumeDb,-200,24," dB",false,1,1,58);
    button("filter.on",f.on?"ON":"OFF",{746,3,40,20},[this,&f]{f.on=!f.on;changed();requestBuild();},f.on);
    fk("filter.cutoff","FREQ",610,32,f.cutoffHz,20,20480," Hz",false,1,.25f);
    fk("filter.resonance","RESO",610,107,f.resonance,0,1,"%",false,100);
    knob("filter.lp","LP+",{610,182,64,59},0,61,f.lpOffset,[&f](double n){f.lpOffset=(float)n;}," st",false,1);
    choice("filter.lpType",{680,218,61,22},{"6 dB","12 dB","18 dB","24 dB"},f.lpType-1,[&f](int n){f.lpType=n+1;});
    auto curve=std::make_unique<Display>();curve->draw=[this](auto& g,auto r){filterCurve(g,r);};
    curve->click=[this]{spectrumMode=2;requestBuild();};own(std::move(curve),"filter.display",{684,34,250,175});
    button("filter.edit","EDIT",{745,218,43,22},[this]{spectrumMode=2;requestBuild();});
    fk("filter.track","TRACK",943,32,f.keyTrack,-1,1,"%",false,100);
    fk("filter.env","ENV",943,107,f.envDepth,-1,1,"%",false,100);
    fk("filter.vel","VEL",943,182,f.velTrack,-1,1,"%",false,100);
    for(int i=0;i<2;++i) {
        auto& m=f.xmod[(size_t)i];int y=31+i*105;
        choice("filter.xmod"+juce::String(i)+".type",{1012,y,64,20},{"FM","RM (not built)"},m.type,[this,&m](int n){m.type=n;requestBuild();});
        auto& depth=knob("filter.xmod"+juce::String(i)+".amount","DEPTH",{1012,y+20,64,72},0,100,m.amount*100,
            [&m](double n){m.amount=(float)n/100;},"%",false,.01);
        depth.setEnabled(m.type==0);depth.slider.setTooltip(m.type==0?"Filter FM depth":"Filter RM is not implemented yet; its imported value is retained.");
        choice("filter.xmod"+juce::String(i)+".source",{1012,y+88,64,20},sources,m.source,[&m](int n){m.source=n;});
    }
    fk("filter.volume","VOL",1110,32,f.volumeDb,-200,24," dB");
    fk("filter.pan","PAN",1110,107,f.pan,-1,1,"",false,100);
    fk("osc.routebalance","BAL",1110,182,o.routeBalance,-1,1,"",false,100);
    const juce::StringArray routes{"OUT","FX 1","FX 2","FX 3","FX 4","FX 5","FX 6"};
    choice("osc.route1",{792,218,68,22},routes,o.route1,[&o](int n){o.route1=n;}).setTooltip("Oscillator destination 1");
    choice("osc.route2",{864,218,68,22},routes,o.route2,[&o](int n){o.route2=n;}).setTooltip("Oscillator destination 2");
}

void PartialsEditor::Impl::envelopePanel(bool right) {
    const int x=right?786:0;const int selected=right?envRight:envLeft;
    const juce::StringArray names=right?juce::StringArray{"Cut 1","Cut 2","Cut 3","Cut 4","LFO 1","LFO 2","LFO 3","LFO 4","LFO 5"}
        :juce::StringArray{"Amp 1","Amp 2","Amp 3","Amp 4","Mod 1","Mod 2","Mod 3","Main"};
    for(int i=0;i<names.size();++i) button((right?"env.right":"env.left")+juce::String(i),names[i],
        {right?1147:10,277+i*23,43,22},[this,right,i]{if(right)envRight=i;else envLeft=i;requestBuild();},selected==i);
    const int bx=x+(right?10:65),width=right?58:63;
    const juce::String prefix=right?"env.right.":"env.left.";
    if(right && selected>=4) {
        auto& l=patch.lfo[(size_t)(selected-4)];
        choice("lfo.wave",{bx,277,142,23},{"Sine","Triangle","Saw","Pulse","Impulse","Peak","Noise","Sample & Hold","Random pulse"},l.wave,[&l](int n){l.wave=n;});
        choice("lfo.polarity",{bx+150,277,187,23},{"Bipolar","Positive","Negative","Positive half","Negative half"},l.polarity,[&l](int n){l.polarity=n;});
        fk("lfo.rate","RATE",bx,305,l.rateHz,0,200," Hz",true,1,.3f,width);
        fk("lfo.sync","SYNC",bx+width,305,l.syncBeats,0,256," beats",true,1,.5f,width);
        fk("lfo.delay","DELAY",bx+2*width,305,l.delay,0,60," s",true,1,.35f,width);
        fk("lfo.attack","ATTACK",bx+3*width,305,l.attack,0,60," s",true,1,.35f,width);
        fk("lfo.symmetry","SYM",bx+4*width,305,l.symmetry,.01f,.99f,"%",true,100,1,width);
        fk("lfo.phase","PHASE",bx,388,l.phase,0,1,"",true,360,1,width);
        fk("lfo.smooth","SMOOTH",bx+width,388,l.smooth,0,.5f,"",true,1,1,width);
        choice("lfo.free",{bx+140,410,180,25},{"Note restart","Free running","Mono restart"},l.freeRun,[&l](int n){l.freeRun=n;});
        return;
    }
    auto& e=right?patch.cutEnv[(size_t)selected]:selected<4?patch.ampEnv[(size_t)selected]:selected<7?patch.modEnv[(size_t)(selected-4)]:patch.mainEnv;
    fk(prefix+"attack","A",bx,278,e.attack,0,60," s",true,1,.3f,width);
    fk(prefix+"decay","D",bx+width,278,e.decay,0,60," s",true,1,.3f,width);
    fk(prefix+"sustain","S",bx+2*width,278,e.sustain,0,1,"%",true,100,1,width);
    fk(prefix+"release","R",bx+3*width,278,e.release,0,60," s",true,1,.3f,width);
    fk(prefix+"hold","H",bx+width/2,355,e.hold,0,60," s",true,1,.3f,width);
    fk(prefix+"fade","F",bx+5*width/2,355,e.fade,-60,60," dB/s",true,1,1,width);
    const char* titles[]={"A curve","D curve","F curve","R curve"};
    float* curves[]={&e.attackCurve,&e.decayCurve,&e.fadeCurve,&e.releaseCurve};
    for(int i=0;i<4;++i) {
        auto& control=knob(prefix+juce::String(titles[i]),titles[i],{bx+i*width,429,width,61},.05,20,*curves[i],
            [v=curves[i]](double n){*v=(float)n;},"",true,.01,.5);
        if(i==2){control.setEnabled(false);control.slider.setTooltip("Fade curve is retained in presets but is not implemented.");}
    }
    button(prefix+"oneshot",e.ignoreNoteOff?"ONE SHOT":"NOTE OFF",{bx+4*width+3,300,76,26},
        [this,&e]{e.ignoreNoteOff=!e.ignoreNoteOff;changed();requestBuild();},e.ignoreNoteOff);
}

void PartialsEditor::Impl::matrixPanel() {
    button("matrix.page",matrixPage==0?"1-12  >":"<  13-24",{682,247,89,21},[this]{matrixPage=1-matrixPage;requestBuild();});
    juce::StringArray sources{"none","modwheel","aftertouch","velocity","note","pitchwheel+","pitchwheel-","mainenv","alternate","constant","random_bi","random_uni","arp_velocity","arp_transpose","arp_length"};
    for(const auto& p:std::vector<std::pair<juce::String,int>>{{"lfo",5},{"modenv",3},{"ampenv",4},{"cutenv",4}})
        for(int i=1;i<=p.second;++i)sources.add(p.first+juce::String(i));
    for(int i=16;i<=19;++i)sources.add("cc"+juce::String(i));
    juce::StringArray dests{"none","main.volume","main.pitch"};
    for(int i=1;i<=4;++i) {
        for(auto s:{"pitch","symmetry","wavemix","volume","xmod1","xmod2"})dests.add("osc"+juce::String(i)+"."+s);
        for(auto s:{"cutoff","resonance","volume","pan","envdepth","xmod1","xmod2"})dests.add("filter"+juce::String(i)+"."+s);
    }
    for(int i=1;i<=5;++i)dests.add("lfo"+juce::String(i)+".speed");
    for(int i=1;i<=24;++i)dests.add("matrix.depth"+juce::String(i));
    for(int row=0;row<12;++row) {
        const int index=matrixPage*12+row,y=273+row*18;auto& m=patch.matrix[(size_t)index];
        auto src=sources,dst=dests;
        if(!src.contains(m.source))src.add(m.source);if(!dst.contains(m.dest))dst.add(m.dest);
        label(juce::String(index+1),{412,y,20,18},false,10);
        choice("matrix.source"+juce::String(index),{433,y,114,18},src,src.indexOf(m.source),[&m,src](int n){m.source=src[n].toStdString();});
        auto s=std::make_unique<juce::Slider>(juce::Slider::LinearBar,juce::Slider::TextBoxLeft);
        s->setRange(-1,1,.0001);s->setValue(m.amount,juce::dontSendNotification);s->setNumDecimalPlacesToDisplay(4);
        auto* parameter=proc.params.getParameter(partials::liveControlSpecs()[partials::kLiveMatrixStart+index].id);
        auto attachment=std::make_unique<juce::ParameterAttachment>(*parameter,
            [this,&m,ptr=s.get()](float v){m.amount=v;ptr->setValue(v,juce::dontSendNotification);surface.repaint();},nullptr);
        auto* a=attachment.get();
        s->onValueChange=[a,ptr=s.get()]{a->setValueAsCompleteGesture((float)ptr->getValue());};
        a->sendInitialUpdate();liveAttachments.push_back(std::move(attachment));
        own(std::move(s),"matrix.amount"+juce::String(index),{550,y,65,18});
        choice("matrix.dest"+juce::String(index),{618,y,156,18},dst,dst.indexOf(m.dest),[&m,dst](int n){m.dest=dst[n].toStdString();});
    }
}
void PartialsEditor::Impl::arpPanel() {
    auto& a=patch.arp;
    choice("arp.mode",{158,498,158,22},{"OFF","MOD ONLY","UP","DOWN","UP / DOWN","UP / DOWN +","DOWN / UP","DOWN / UP +","RANDOM","CHORD","AS PLAYED"},a.mode,[&a](int n){a.mode=n;});
    auto rate=std::make_unique<juce::Slider>(juce::Slider::LinearBar,juce::Slider::TextBoxLeft);
    rate->setRange(.015625,8,.015625);rate->setValue(a.resolutionBeats,juce::dontSendNotification);
    rate->setTextValueSuffix(" beats");rate->onValueChange=[this,&a,p=rate.get()]{a.resolutionBeats=(float)p->getValue();changed();};
    own(std::move(rate),"arp.resolution",{326,498,112,22});
    number("arp.steps"," steps",{12,528,68,24},a.length,1,32,[&a](int n){a.length=n;});
    number("arp.octaves"," oct",{85,528,62,24},a.octaves,1,4,[&a](int n){a.octaves=n;});
    fk("arp.gate","GATE",10,555,a.noteLength,0,1,"%",false,100);
    fk("arp.swing","SWING",80,555,a.swing,0,1,"%",false,100);
    fk("arp.velblend","A VEL P",10,628,a.veloBlend,0,1,"%",false,100);
    button("arp.page",arpPage==0?"1-16  >":"<  17-32",{85,652,66,26},[this]{arpPage=1-arpPage;requestBuild();});
    label("TRANS / VEL     LENGTH / ADVANCE     T = LEGATO",{159,526,425,20},false,10);
    for(int i=0;i<16;++i) {
        const int index=arpPage*16+i,x=160+(i%4)*108,y=551+(i/4)*36;auto& st=patch.arp.steps[(size_t)index];
        auto small=[this,index](const juce::String& key,juce::Rectangle<int> b,double value,double lo,double hi,double step,Change cb) {
            auto s=std::make_unique<juce::Slider>(juce::Slider::LinearBar,juce::Slider::TextBoxLeft);
            s->setRange(lo,hi,step);s->setValue(value,juce::dontSendNotification);
            s->setNumDecimalPlacesToDisplay(0);s->onValueChange=[this,p=s.get(),cb]{cb(p->getValue());changed();};
            s->setTooltip("Step "+juce::String(index+1)+" "+key+(key=="velocity"?" (0 rest, 128 hold)":""));
            own(std::move(s),"arp.step"+juce::String(index)+"."+key,b);
        };
        small("transpose",{x,y,43,16},st.transpose,-48,48,1,[&st](double n){st.transpose=(int)n;});
        small("velocity",{x+44,y,43,16},st.velocity,0,128,1,[&st](double n){st.velocity=(int)n;});
        small("length",{x,y+17,43,16},st.length*100,0,100,1,[&st](double n){st.length=(float)n/100;});
        small("advance",{x+44,y+17,43,16},st.advance,0,32,1,[&st](double n){st.advance=(int)n;});
        button("arp.step"+juce::String(index)+".legato","T",{x+88,y,17,33},[this,&st]{st.legato=!st.legato;changed();requestBuild();},st.legato);
    }
}

void PartialsEditor::Impl::effectsPanel() {
    static const std::vector<std::string> types{"none","reverb","delay_stereo","delay_mod","delay_pingpong","chorus","phaser","flanger","filter","compressor","gator","crusher","eq","wah"};
    const juce::StringArray names{"NONE","REVERB","STEREO DELAY","MOD DELAY","PING PONG DELAY","CHORUS","PHASER","FLANGER","FILTER","COMPRESSOR","GATOR","CRUSHER","EQ","WAH"};
    auto& slot=patch.fx[(size_t)fx];
    button("fx.on",slot.on?"ON":"OFF",{699,498,42,21},[this,&slot]{slot.on=!slot.on;changed();requestBuild();},slot.on);
    auto it=std::find(types.begin(),types.end(),slot.type);
    choice("fx.type",{752,498,232,22},names,it==types.end()?0:(int)std::distance(types.begin(),it),[this,&slot](int n){
        slot.type=types[(size_t)n];slot.on=n!=0;slot.params.clear();
        for(const auto& p:effectParameters(slot.type))slot.params[p.key]=p.value;
        const auto values=partials::liveValues(patch);
        for(int f=0;f<partials::kEffectControls;++f)if(slot.type==partials::effectControlSpecs[f].type){
            const int index=partials::kLiveFxStart+fx*partials::kEffectControls+f;
            auto* parameter=proc.params.getParameter(partials::liveControlSpecs()[index].id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(values[index]));
        }
        auto* wet=proc.params.getParameter(partials::liveControlSpecs()[partials::kLiveWetStart+fx].id);
        wet->setValueNotifyingHost(partials::effectDefaultWet(slot.type));
        requestBuild();
    });
    juce::StringArray routes{"OUT"};std::vector<int> values{0};
    for(int i=fx+2;i<=6;++i){routes.add("FX "+juce::String(i));values.push_back(i);}
    auto route=std::find(values.begin(),values.end(),slot.route);
    choice("fx.route",{1040,498,86,22},routes,route==values.end()?0:(int)std::distance(values.begin(),route),[&slot,values](int n){slot.route=values[(size_t)n];});
    for(int i=0;i<6;++i) {
        const auto& s=patch.fx[(size_t)i];
        button("fx.select"+juce::String(i),juce::String(i+1)+" "+juce::String(s.type).replace("delay_","D ").substring(0,8),{1135,531+i*26,58,25},
            [this,i]{fx=i;requestBuild();},fx==i);
    }
    const auto params=effectParameters(slot.type);
    for(size_t i=0;i<params.size();++i) {
        auto p=params[i];
        for(const auto& spec:partials::effectControlSpecs)if(slot.type==spec.type&&std::string(p.key)==spec.key){p.value=spec.initial;p.lo=spec.minimum;p.hi=spec.maximum;break;}
        const auto found=slot.params.find(p.key);
        const float value=found==slot.params.end()?p.value:found->second;
        const int x=611+(int)(i%6)*86,y=530+(int)(i/6)*84;
        juce::StringArray options;
        const juce::String key=p.key;
        if(key=="type" || key=="filter_type")options={"Low pass","High pass","Band pass","Notch"};
        else if(key=="divider_mode")options={"Average","Step","Slide"};
        else if(key=="link")options={"Independent","Linked","Cross link"};
        else if(key=="invert"||key=="fb_invert"||key=="filter_post"||key=="auto_volume"||key=="modulation"||key=="filters_in_feedback"||key=="cross"||key=="dither")options={"Off","On"};
        if(!options.isEmpty()) {
            label(p.label,{x,y+53,84,18},false,10);
            choice("fx."+key,{x+3,y+18,78,28},options,(int)value,[&slot,p](int n){slot.params[p.key]=(float)n;});
        } else knob("fx."+key,p.label,{x,y,83,77},p.lo,p.hi,value,[&slot,p](double n){slot.params[p.key]=(float)n;},p.unit,false,
                    key=="diffusors"||key=="stages"||key.startsWith("pattern")?1:.01,
                    key.endsWith("_hz")?.3:1);
    }
    if(slot.type=="none")label("Choose an effect above. Route oscillators to this slot.",{633,566,477,60});
}

void PartialsEditor::Impl::spectralPanel() {
    const bool filter=spectrumMode==2;auto& o=patch.osc[(size_t)osc];auto& spectrum=spectrumMode==1?o.waveB:o.waveA;
    auto title=filter?"FILTER SPECTRAL EDITOR":"OSCILLATOR "+juce::String(osc+1)+(spectrumMode==1?" / WAVE B":" / WAVE A");
    label(title,{20,249,550,27},true,17);
    button("spectral.close","RETURN",{1075,250,110,26},[this]{spectrumMode=-1;requestBuild();});
    if(!filter) {
        button("spectral.phase",spectrumPhase?"PHASE":"AMPLITUDE",{830,250,110,26},[this]{spectrumPhase=!spectrumPhase;requestBuild();},spectrumPhase);
        button("spectral.other",spectrumMode==0?"WAVE A":"WAVE B",{951,250,110,26},[this]{spectrumMode=1-spectrumMode;requestBuild();});
    }
    auto d=std::make_unique<Display>();
    d->draw=[this,filter](juce::Graphics& g,juce::Rectangle<float> r){
        const auto& oscParam=patch.osc[(size_t)osc];const auto& wave=spectrumMode==1?oscParam.waveB:oscParam.waveA;
        const int n=filter?partials::kCurvePoints:partials::kHarmonics;
        for(int row=0;row<5;++row){float y=r.getY()+r.getHeight()*row/4;g.setColour(juce::Colour(0xff4a555a));g.drawHorizontalLine((int)y,r.getX(),r.getRight());}
        for(int i=0;i<n;++i){
            float value=filter?oscParam.filter.curve.db[(size_t)i]:spectrumPhase?wave.phase[(size_t)i+1]:wave.db[(size_t)i+1];
            float height=juce::jlimit(0.f,1.f,!filter&&spectrumPhase?value:(value+96)/96)*r.getHeight();
            float x=r.getX()+i*r.getWidth()/n;
            g.setColour(cyan.withAlpha(i%12==0?.95f:.65f));g.fillRect(x,r.getBottom()-height,std::max(1.f,r.getWidth()/n-1),std::max(1.f,height));
        }
    };
    d->edit=[this,filter,ptr=d.get()](const juce::MouseEvent& e){
        auto r=ptr->getLocalBounds().reduced(8).toFloat();const int n=filter?partials::kCurvePoints:partials::kHarmonics;
        const int i=juce::jlimit(0,n-1,(int)((e.position.x-r.getX())/r.getWidth()*n));
        const float value=juce::jlimit(0.f,1.f,(r.getBottom()-e.position.y)/r.getHeight());
        auto& current=patch.osc[(size_t)osc];auto& wave=spectrumMode==1?current.waveB:current.waveA;
        if(filter)current.filter.curve.db[(size_t)i]=value==0?partials::kSilentDb:value*96-96;
        else if(spectrumPhase)wave.phase[(size_t)i+1]=value;
        else wave.db[(size_t)i+1]=value==0?partials::kSilentDb:value*96-96;
        changed();ptr->repaint();
    };
    own(std::move(d),"spectral.canvas",{20,287,1165,340});
    label(filter?"-120 semitones                                  CUTOFF                                  +120 semitones"
        :"1                                                             HARMONICS                                                             256",{20,630,1150,24},true);
    if(filter) {
        button("spectral.flat","FLAT",{20,664,90,27},[this,&o]{o.filter.curve=partials::FilterCurve::flat();changed();});
        button("spectral.normalise","NORMALISE",{120,664,110,27},[this,&o]{o.filter.curve.normalise();changed();});
    } else {
        button("spectral.sine","SINE",{20,664,85,27},[this,&spectrum]{spectrum=partials::Spectrum::sine();changed();});
        button("spectral.saw","SAW",{115,664,85,27},[this,&spectrum]{
            for(int i=1;i<=partials::kHarmonics;++i){spectrum.db[(size_t)i]=-20*std::log10((float)i);spectrum.phase[(size_t)i]=0;}changed();});
        button("spectral.square","SQUARE",{210,664,85,27},[this,&spectrum]{
            for(int i=1;i<=partials::kHarmonics;++i){spectrum.db[(size_t)i]=i%2?-20*std::log10((float)i):partials::kSilentDb;spectrum.phase[(size_t)i]=0;}changed();});
        button("spectral.clear","CLEAR",{305,664,85,27},[this,&spectrum]{spectrum=partials::Spectrum();changed();});
        button("spectral.normalise","NORMALISE",{400,664,110,27},[this,&spectrum]{spectrum.normalise();changed();});
    }
    label("Drag to draw. Changes are heard while you play.",{660,665,510,25},true);
}

void PartialsEditor::Impl::chooseFolder() {
    chooser=std::make_unique<juce::FileChooser>("Choose Optisynth presets",proc.getPresetFolder(),"*");
    const juce::Component::SafePointer<PartialsEditor> safe(&owner);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
        [safe](const juce::FileChooser& c){if(safe!=nullptr && c.getResult().isDirectory()){safe->ui_->scanFolder(c.getResult());safe->ui_->requestBuild();}});
}
void PartialsEditor::Impl::save() {
    submit();
    chooser=std::make_unique<juce::FileChooser>("Save edited Optisynth preset",proc.getPresetFolder().getChildFile(juce::String(patch.name)+" edited.ptch"),"*.ptch");
    const juce::Component::SafePointer<PartialsEditor> safe(&owner);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& c){if(safe!=nullptr && c.getResult()!=juce::File()){
            auto& ui=*safe->ui_;ui.submit();
            if(ui.proc.saveEditedPatch(c.getResult().withFileExtension("ptch"))){ui.scanFolder(c.getResult().getParentDirectory());ui.requestBuild();}
        }});
}
void PartialsEditor::Impl::loadIndex(int index) {
    if(files.isEmpty())return;
    dirty=false;releaseTyping();proc.keyboardState.allNotesOff(1);
    proc.loadPresetFile(files[juce::jlimit(0,files.size()-1,index)]);
}
void PartialsEditor::Impl::bottomPanel() {
    label("PRESET",{14,710,90,23});
    auto presets=std::make_unique<juce::ComboBox>();
    for(int i=0;i<files.size();++i)presets->addItem(files[i].getFileNameWithoutExtension(),i+1);
    int selected=files.indexOf(proc.getPresetFile());presets->setSelectedItemIndex(selected,juce::dontSendNotification);
    if(selected<0)presets->setText(patch.name,juce::dontSendNotification);
    presets->onChange=[this,p=presets.get()]{loadIndex(p->getSelectedItemIndex());};
    own(std::move(presets),"preset.select",{105,710,473,29});
    button("preset.previous","<",{588,710,31,29},[this]{loadIndex(files.indexOf(proc.getPresetFile())-1);});
    button("preset.next",">",{625,710,31,29},[this]{loadIndex(files.indexOf(proc.getPresetFile())+1);});
    button("preset.folder","FOLDER",{665,710,67,29},[this]{chooseFolder();});
    button("preset.save","SAVE AS",{740,710,67,29},[this]{save();});
    button("preset.restore","RESTORE",{815,710,73,29},[this]{dirty=false;proc.restoreLoadedPatch();});
    label(proc.isEdited()||dirty?"EDITED":"",{180,744,60,20});
    auto* autoLevel=proc.params.getParameter("autolevel");
    auto& b=button("output.autolevel","AUTO LEVEL",{252,745,98,22},[this,autoLevel]{
        autoLevel->beginChangeGesture();autoLevel->setValueNotifyingHost(autoLevel->getValue()>.5f?0.f:1.f);autoLevel->endChangeGesture();requestBuild();
    },autoLevel->getValue()>.5f);
    b.setTooltip("Use the level trim measured when this preset loaded. The trim stays fixed while you edit.");
    button("keyboard.panic","ALL NOTES OFF",{360,745,117,22},[this]{releaseTyping();proc.keyboardState.allNotesOff(1);});
    auto& output=knob("output.gain","OUTPUT",{900,704,77,69},-48,12,proc.params.getRawParameterValue("output")->load(),
        [](double){}," dB",false,.1);
    output.slider.onValueChange=nullptr;
    outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.params,"output",output.slider);
    fk("main.volume","MAIN VOL",1000,701,patch.mainVolumeDb,-200,24," dB",false,1,1,78);
    label("SPECTRAL\nSYNTHESIS",{1090,718,103,45},false,12);
}

bool PartialsEditor::Impl::typingActive() const {
    if(!owner.isShowing() || !owner.hasKeyboardFocus(true) || !juce::Process::isForegroundProcess())return false;
    const auto mods=juce::ModifierKeys::getCurrentModifiersRealtime();
    if(mods.isCommandDown()||mods.isCtrlDown()||mods.isAltDown())return false;
    for(auto* c=juce::Component::getCurrentlyFocusedComponent();c!=nullptr;c=c->getParentComponent())
        if(dynamic_cast<juce::TextEditor*>(c)!=nullptr || dynamic_cast<juce::ComboBox*>(c)!=nullptr)return false;
   #if JUCE_MAC
    return (CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState)&kCGEventFlagMaskAlphaShift)!=0;
   #else
    return false;
   #endif
}
void PartialsEditor::Impl::releaseTyping() {
    typing.release([this](int n){proc.keyboardState.noteOff(1,n,0);});
}
void PartialsEditor::Impl::pollTyping() {
    capsEnabled=typingActive();
    std::array<bool,18> down{};
    if(capsEnabled)for(size_t i=0;i<down.size();++i)down[i]=juce::KeyPress::isKeyCurrentlyDown(TypingKeyboard::keys[i]);
    const int oldBase=typing.base;
    typing.update(capsEnabled,down,juce::KeyPress::isKeyCurrentlyDown('Z'),juce::KeyPress::isKeyCurrentlyDown('X'),
        [this](int n){proc.keyboardState.noteOn(1,n,.8f);},[this](int n){proc.keyboardState.noteOff(1,n,0);});
    if(oldBase!=typing.base)keyboard.setLowestVisibleKey(std::max(0,typing.base-12));
}
bool PartialsEditor::Impl::key(const juce::KeyPress& k) {
    if(!typingActive())return false;
    const juce::String keys="AWSEDFTGYHUJKOLP;'ZX";
    if(keys.containsChar((juce::juce_wchar)std::toupper(k.getKeyCode()))){pollTyping();return true;}
    return false;
}
void PartialsEditor::Impl::tick() {
    proc.refreshKeyboardDisplay();pollTyping();
    if(revision!=proc.patchRevision()){
        revision=proc.patchRevision();patch=proc.editablePatch();dirty=false;requestBuild();
    }
    const double now=juce::Time::getMillisecondCounterHiRes();
    if(dirty && now-lastSubmitted>120)submit();  // update during a continuous drag, not only after it stops
    if(rebuild){submit();rebuild=false;build();}
    status.setText(proc.getStatus(),juce::dontSendNotification);
    typingStatus.setText((capsEnabled?"CAPS LOCK ON  /  ":"Caps Lock: play keys  /  ")+
        juce::MidiMessage::getMidiNoteName(typing.base,true,true,4)+"  /  A W S E D F T G Y H U J K O L P ; '   /   Z/X octave",juce::dontSendNotification);
}
void PartialsEditor::Impl::resized() {
    const float scale=std::min(owner.getWidth()/(float)panelWidth,(owner.getHeight()-80)/(float)panelHeight);
    surface.setTransform(juce::AffineTransform());surface.setBounds(0,0,panelWidth,panelHeight);
    surface.setTransform(juce::AffineTransform::scale(scale));
    const int y=juce::roundToInt(panelHeight*scale);
    keyboard.setBounds(8,y+4,owner.getWidth()-16,49);
    status.setBounds(8,y+55,owner.getWidth()/2-8,22);
    typingStatus.setBounds(owner.getWidth()/2,y+55,owner.getWidth()/2-8,22);
}

PartialsEditor::PartialsEditor(PartialsProcessor& p):juce::AudioProcessorEditor(p) {
    setWantsKeyboardFocus(true);setResizable(true,true);setResizeLimits(960,700,1680,1170);
    ui_=std::make_unique<Impl>(*this,p);setSize(1200,854);startTimerHz(30);
}
PartialsEditor::~PartialsEditor(){stopTimer();ui_.reset();}
void PartialsEditor::paint(juce::Graphics& g){g.fillAll(juce::Colour(0xff151c20));}
void PartialsEditor::resized(){if(ui_)ui_->resized();}
void PartialsEditor::timerCallback(){ui_->tick();}
bool PartialsEditor::keyPressed(const juce::KeyPress& key){return ui_->key(key);}
bool PartialsEditor::keyStateChanged(bool){ui_->pollTyping();return ui_->capsEnabled;}
