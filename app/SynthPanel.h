#pragma once
#include "Panel.h"
#include "PatchIO.h"

// Request 71: the "synth panel" view. A fixed front panel of the patch's playing
// controls, built from the patch's curated layout or, without one, one section per
// module. Its knobs edit the same parameters as the rack cards (see RackSynth).
namespace {
struct PanelTheme { Colour face,section,legend,accent,display; bool cheeks; };
PanelTheme panelTheme(const std::string& style) {
    if(style=="slate")return {Colour(0xff35414b),Colour(0xff404d58),Colour(0xfff1f0e8),Colour(0xff9cc9e6),Colour(0xff9cc9e6),false};
    if(style=="walnut")return {Colour(0xff1c1c1d),Colour(0xff262627),Colour(0xffefe5cc),Colour(0xffd7a96b),Colour(0xffe8b56c),true};
    if(style=="ivory")return {Colour(0xffd6d0bf),Colour(0xffe2ddcf),Colour(0xff202224),Colour(0xffb5533c),Colour(0xffd9c38c),false};
    return {Colour(0xff25282b),Colour(0xff2e3236),paper,teal,Colour(0xff8fd0c2),false};
}
class SynthPanelView final : public juce::Component {
    // A titled group: JUCE Grid of two rows, each column as wide as its widest text.
    struct Section final : juce::Component {
        std::string title;const PanelTheme* theme=nullptr;std::vector<juce::Component*> items;int columns=1,knob=60;
        static constexpr int titleHeight=26,pad=8,gap=4;
        Control* controlAt(size_t i) const {return dynamic_cast<Control*>(items[i]);}
        bool isLauncher() const {return items.size()==1&&dynamic_cast<TextButton*>(items[0]);}
        std::vector<int> columnWidths() const {
            std::vector<int> w(size_t(columns),std::max(knob+12,64));
            for(size_t i=0;i<items.size();++i)if(auto* c=controlAt(i))w[i%size_t(columns)]=std::max(w[i%size_t(columns)],c->textWidth()+6);
            return w;
        }
        int rowCount() const {return (int(items.size())+columns-1)/columns;}
        int cellHeight() const {return knob+Control::labelHeight+Control::valueHeight;}
        Rectangle<int> naturalSize() const {
            if(isLauncher())return {0,0,2*pad+236,titleHeight+2*pad+34};
            int w=0;for(int x:columnWidths())w+=x;
            return {0,0,2*pad+w+(columns-1)*gap,titleHeight+2*pad+rowCount()*cellHeight()+(rowCount()-1)*gap};
        }
        void paint(Graphics& g) override {
            auto r=getLocalBounds().toFloat().reduced(.5f);
            g.setColour(theme->section);g.fillRoundedRectangle(r,4);
            g.setColour(theme->legend.withAlpha(.22f));g.drawRoundedRectangle(r,4,1);
            g.setColour(theme->accent);g.fillRect(r.getX()+pad,float(titleHeight-3),r.getWidth()-2*pad,2.f);
            g.setColour(theme->legend);g.setFont(font(11).boldened());
            g.drawText(String(title),pad,4,getWidth()-2*pad,titleHeight-8,Justification::centredLeft);
        }
        // Items are children of the face, so they are placed in the face's coordinates.
        void layoutItems() {
            if(items.empty())return;
            if(isLauncher()){items[0]->setBounds(getX()+pad*2,getY()+titleHeight+(getHeight()-titleHeight-34)/2,getWidth()-pad*4,34);return;}
            Grid grid;grid.columnGap=grid.rowGap=Grid::Px(gap);
            for(int w:columnWidths())grid.templateColumns.add(Grid::TrackInfo(Grid::Px(w)));
            for(int r=0;r<rowCount();++r)grid.templateRows.add(Grid::TrackInfo(Grid::Px(cellHeight())));
            for(size_t i=0;i<items.size();++i)grid.items.add(GridItem(*items[i]).withArea(int(i)/columns+1,int(i)%columns+1));
            grid.performLayout(getBounds().withTrimmedTop(titleHeight+pad).withTrimmedLeft(pad).withTrimmedRight(pad));
        }
    };
    struct Face final : juce::Component {
        SynthPanelView* owner=nullptr;
        void paint(Graphics& g) override {owner->paintFace(g);}
    };
public:
    std::function<void(const std::string&,const std::string&,float)> changed;
    std::function<void(const std::string&)> openEditor;
    Control::ModalModAction modalModAction;
    SynthPanelView(){face.owner=this;addAndMakeVisible(viewport);viewport.setViewedComponent(&face,false);viewport.setScrollBarsShown(true,false);viewport.setScrollBarThickness(10);}
    ~SynthPanelView() override {controls.clear();launchers.clear();sections.clear();viewport.setViewedComponent(nullptr,false);}
    void build(const RackDocument& d) {
        controls.clear();launchers.clear();sections.clear();keys.clear();
        PanelLayout scratch;auto layout=effectivePanel(d,scratch);
        // A sequencer the curated layout does not mention still drives the sound: show it first.
        for(auto n=d.graph.nodes.rbegin();n!=d.graph.nodes.rend();++n)if(isSequencerModule(n->type)){
            bool shown=false;for(const auto& sec:layout.sections)for(const auto& c:sec.controls)shown|=c.node==n->id;
            if(!shown)layout.sections.insert(layout.sections.begin(),{"SEQUENCER  /  "+panelTitle(n->id),{{n->id,"",""}}});
        }
        curated=!d.panel.sections.empty();theme=panelTheme(layout.style);look.markings=theme.legend;
        title=d.info.name.empty()?"Untitled patch":d.info.name;
        subtitle=d.info.category;for(const auto& t:d.info.tags)subtitle+=(subtitle.empty()?"":"  ·  ")+t;
        for(const auto& s:layout.sections){
            auto section=std::make_unique<Section>();section->title=s.title;section->theme=&theme;
            for(const auto& c:s.controls){
                const auto node=std::find_if(d.graph.nodes.begin(),d.graph.nodes.end(),[&](const NodeSpec& n){return n.id==c.node;});
                if(node==d.graph.nodes.end())continue;
                if(c.parameter.empty()){
                    auto button=std::make_unique<TextButton>("Open "+String(panel(node->type).name));
                    button->onClick=[this,id=c.node]{if(openEditor)openEditor(id);};
                    face.addAndMakeVisible(*button);section->items.push_back(button.get());launchers.push_back(std::move(button));continue;
                }
                const auto* spec=findParameter(node->type,c.parameter);if(!spec)continue;
                Parameter p=*spec;if(!c.label.empty())p.label=c.label;
                float value=p.initial;for(const auto& v:node->parameters)if(v.first==c.parameter)value=v.second;
                auto control=std::make_unique<Control>(p,value,[this,id=c.node,key=c.parameter](float v){if(changed)changed(id,key,v);},look,theme.legend);
                control->slider.setTooltip(String(c.node)+"."+String(c.parameter)+" / double-click to reset");
                if(node->type=="analog.filter"&&c.parameter=="fm_depth")control->showFilterFmSource(d.graph,c.node);
                control->bindModalModulation(node->type,c.parameter,c.node,d.graph,modalModAction);
                face.addAndMakeVisible(*control);section->items.push_back(control.get());
                keys.push_back({c.node,c.parameter});controls.push_back(std::move(control));
            }
            if(section->items.empty())continue;
            const bool launcher=section->items.size()==1&&dynamic_cast<TextButton*>(section->items[0]);
            section->columns=launcher?1:std::max(1,(int(section->items.size())+1)/2);
            face.addAndMakeVisible(*section);section->toBack();sections.push_back(std::move(section));
        }
        resized();repaint();
    }
    void setValue(const std::string& node,const std::string& key,float value) {
        for(size_t i=0;i<keys.size();++i)if(keys[i].first==node&&keys[i].second==key)controls[i]->setValue(value);
    }
    void showActivity(const std::string& node,const std::string& key,const String& reason){
        for(size_t i=0;i<keys.size();++i)if(keys[i].first==node && keys[i].second==key)controls[i]->showActivity(reason);
    }
    Control* control(const std::string& node,const std::string& key) {
        for(size_t i=0;i<keys.size();++i)if(keys[i].first==node&&keys[i].second==key)return controls[i].get();return nullptr;
    }
    size_t controlCount() const {return controls.size();}
    size_t sectionCount() const {return sections.size();}
    size_t launcherCount() const {return launchers.size();}
    bool isCurated() const {return curated;}
    void preferKnob(int value) {preferred=value;resized();}
    bool automaticDensity() const {return preferred==0;}
    int knobSize() const {return knob;}
    // Layout check: every control inside its section, no overlaps, sections inside the face.
    bool layoutSound() const {
        for(const auto& s:sections){
            if(!face.getLocalBounds().contains(s->getBounds()))return false;
            for(size_t i=0;i<s->items.size();++i)if(auto* c=s->controlAt(i))if(c->getWidth()<c->textWidth())return false;   // text never clips
            for(size_t i=0;i<s->items.size();++i){const auto r=s->items[i]->getBounds();
                if(!s->getBounds().contains(r)||r.getWidth()<40||r.getHeight()<30)return false;
                for(size_t j=i+1;j<s->items.size();++j)if(r.intersects(s->items[j]->getBounds()))return false;}
            for(const auto& o:sections)if(o!=s&&o->getBounds().intersects(s->getBounds()))return false;
        }
        return true;
    }
    void paint(Graphics& g) override {
        g.fillAll(theme.face);
        if(theme.cheeks){
            for(const auto r:{getLocalBounds().removeFromLeft(cheek),getLocalBounds().removeFromRight(cheek)}){
                g.setColour(Colour(0xff5d3a22));g.fillRect(r);
                g.setColour(Colour(0xff4a2d19));for(int i=0;i<7;++i){const float x=r.getX()+3.f+i*2.6f;g.drawLine(x,0,x+std::sin(i*1.3f)*2,float(getHeight()),.8f);}
                g.setColour(Colours::black.withAlpha(.35f));g.drawVerticalLine(r.getX()==0?r.getRight()-1:r.getX(),0,float(getHeight()));
            }
        }
    }
    void resized() override {
        cheek=theme.cheeks?22:0;viewport.setBounds(getLocalBounds().withTrimmedLeft(cheek).withTrimmedRight(cheek));
        layoutFace(viewport.getWidth(),viewport.getHeight());
    }
private:
    static constexpr int header=58,margin=14,gap=8;
    // Sections flow in a wrapping JUCE FlexBox. Knob size is the largest (32-72 px) that fits
    // the window; text sizes never change; spare space is not spread between controls.
    void layoutFace(int width,int height) {
        const int usable=std::max(200,width-2*margin-10);int bottom=header;
        for(knob=preferred ? preferred : 72;;knob-=4){
            FlexBox box;box.flexWrap=FlexBox::Wrap::wrap;box.justifyContent=FlexBox::JustifyContent::flexStart;box.alignContent=FlexBox::AlignContent::flexStart;
            for(auto& s:sections){s->knob=knob;
                s->columns=std::max(1,(int(s->items.size())+1)/2);
                while(s->columns>1 && s->naturalSize().getWidth()+gap>usable) --s->columns;
                const auto size=s->naturalSize();box.items.add(FlexItem(*s).withWidth(float(size.getWidth())).withHeight(float(size.getHeight())).withMargin(FlexItem::Margin(0,float(gap),float(gap),0)));}
            box.performLayout(Rectangle<float>(float(margin),float(header),float(usable),1.0e6f));
            bottom=header;for(const auto& s:sections)bottom=std::max(bottom,s->getBottom());
            if(preferred || knob<=32 || bottom+margin<=height)break;
        }
        for(auto& s:sections)s->layoutItems();
        face.setSize(width,std::max(height,bottom+margin));
    }
    void paintFace(Graphics& g) {
        g.fillAll(theme.face);
        auto display=Rectangle<float>(float(margin),10,float(std::min(560,face.getWidth()-2*margin-200)),38);
        g.setColour(Colour(0xff0d1011));g.fillRoundedRectangle(display,3);
        g.setColour(theme.legend.withAlpha(.25f));g.drawRoundedRectangle(display,3,1);
        g.setColour(theme.display);g.setFont(font(19));g.drawText(String(title).toUpperCase(),display.reduced(12,0).toNearestInt(),Justification::centredLeft);
        g.setColour(theme.legend.withAlpha(.75f));g.setFont(font(11));
        const int right=int(display.getRight())+14;
        g.drawText(String(subtitle).toUpperCase(),right,12,face.getWidth()-right-margin,17,Justification::centredLeft);
        g.setColour(theme.legend.withAlpha(.5f));
        g.drawText(curated?"SYNTH PANEL  /  patch layout":"SYNTH PANEL  /  automatic layout - one section per module",right,30,face.getWidth()-right-margin,16,Justification::centredLeft);
    }
    Viewport viewport;Face face;KnobLook look;PanelTheme theme=panelTheme("graphite");
    std::vector<std::unique_ptr<Section>> sections;std::vector<std::unique_ptr<Control>> controls;std::vector<std::unique_ptr<TextButton>> launchers;
    std::vector<std::pair<std::string,std::string>> keys;
    std::string title,subtitle;bool curated=false;int knob=72,preferred=0,cheek=0;
};
}
