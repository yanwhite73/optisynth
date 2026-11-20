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
    struct Section final : juce::Component {
        std::string title;const PanelTheme* theme=nullptr;std::vector<juce::Component*> items;int columns=1;
        static constexpr int titleHeight=26,pad=8;
        void paint(Graphics& g) override {
            auto r=getLocalBounds().toFloat().reduced(.5f);
            g.setColour(theme->section);g.fillRoundedRectangle(r,4);
            g.setColour(theme->legend.withAlpha(.22f));g.drawRoundedRectangle(r,4,1);
            g.setColour(theme->accent);g.fillRect(r.getX()+pad,float(titleHeight-3),r.getWidth()-2*pad,2.f);
            g.setColour(theme->legend);g.setFont(font(11).boldened());
            g.drawText(String(title),pad,4,getWidth()-2*pad,titleHeight-8,Justification::centredLeft);
        }
        void resized() override {
            const int count=int(items.size());if(count==0)return;
            const int rows=2,w=(getWidth()-2*pad)/columns,h=(getHeight()-titleHeight-2*pad)/rows;
            if(count==1&&dynamic_cast<TextButton*>(items[0])){const int w=std::min(getWidth()-4*pad,220);
                items[0]->setBounds(getX()+(getWidth()-w)/2,getY()+titleHeight+(getHeight()-titleHeight-34)/2,w,34);return;}
            for(int i=0;i<count;++i){const int row=i/columns,column=i%columns;
                items[size_t(i)]->setBounds(getX()+pad+column*w,getY()+titleHeight+pad+row*h,w,h);}
        }
    };
    struct Face final : juce::Component {
        SynthPanelView* owner=nullptr;
        void paint(Graphics& g) override {owner->paintFace(g);}
    };
public:
    std::function<void(const std::string&,const std::string&,float)> changed;
    std::function<void(const std::string&)> openEditor;
    SynthPanelView(){face.owner=this;addAndMakeVisible(viewport);viewport.setViewedComponent(&face,false);viewport.setScrollBarsShown(true,false);viewport.setScrollBarThickness(10);}
    ~SynthPanelView() override {controls.clear();launchers.clear();sections.clear();viewport.setViewedComponent(nullptr,false);}
    void build(const RackDocument& d) {
        controls.clear();launchers.clear();sections.clear();keys.clear();
        PanelLayout scratch;const auto& layout=effectivePanel(d,scratch);
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
                face.addAndMakeVisible(*control);section->items.push_back(control.get());
                keys.push_back({c.node,c.parameter});controls.push_back(std::move(control));
            }
            if(section->items.empty())continue;
            const bool launcher=section->items.size()==1&&dynamic_cast<TextButton*>(section->items[0]);
            section->columns=launcher?3:std::max(1,(int(section->items.size())+1)/2);
            face.addAndMakeVisible(*section);section->toBack();sections.push_back(std::move(section));
        }
        resized();repaint();
    }
    void setValue(const std::string& node,const std::string& key,float value) {
        for(size_t i=0;i<keys.size();++i)if(keys[i].first==node&&keys[i].second==key)controls[i]->setValue(value);
    }
    Control* control(const std::string& node,const std::string& key) {
        for(size_t i=0;i<keys.size();++i)if(keys[i].first==node&&keys[i].second==key)return controls[i].get();return nullptr;
    }
    size_t controlCount() const {return controls.size();}
    size_t sectionCount() const {return sections.size();}
    size_t launcherCount() const {return launchers.size();}
    bool isCurated() const {return curated;}
    int knobSize() const {return knob;}
    // Layout check: every control inside its section, no overlaps, sections inside the face.
    bool layoutSound() const {
        for(const auto& s:sections){
            if(!face.getLocalBounds().contains(s->getBounds()))return false;
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
    static constexpr int header=58,margin=14,gap=10;
    void layoutFace(int width,int height) {
        const int usable=width-2*margin-10;
        std::vector<std::vector<Section*>> rows;int sectionHeight=0,total=0;
        for(knob=96;knob>=60;knob-=4){
            const int cellW=knob+12,cellH=knob+40;sectionHeight=Section::titleHeight+2*Section::pad+2*cellH;
            rows.clear();int x=0;
            for(auto& s:sections){const int w=2*Section::pad+s->columns*cellW;
                if(rows.empty()||(!rows.back().empty()&&x+gap+w>usable)){rows.emplace_back();x=-gap;}
                rows.back().push_back(s.get());x+=gap+w;}
            total=header+int(rows.size())*(sectionHeight+gap)-gap+margin;
            if(total<=height)break;
            if(knob==60)break;
        }
        // Spare height spreads the rows (up to 35% taller sections) instead of collecting at the bottom.
        if(!rows.empty()&&total<height)sectionHeight=std::min(int(sectionHeight*1.35f),(height-header-margin-gap*(int(rows.size())-1))/int(rows.size()));
        total=header+int(rows.size())*(sectionHeight+gap)-gap+margin;
        const int cellW=knob+12;int y=header;
        for(const auto& row:rows){
            int natural=0;for(auto* s:row)natural+=2*Section::pad+s->columns*cellW;
            const int gaps=gap*(int(row.size())-1);
            const float stretch=std::clamp(float(usable-gaps)/float(std::max(1,natural)),1.f,1.5f);
            int x=margin;
            for(auto* s:row){const int w=int((2*Section::pad+s->columns*cellW)*stretch);s->setBounds(x,y,w,sectionHeight);s->resized();x+=w+gap;}
            y+=sectionHeight+gap;
        }
        face.setSize(width,std::max(height,total));
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
    std::string title,subtitle;bool curated=false;int knob=96,cheek=0;
};
}
