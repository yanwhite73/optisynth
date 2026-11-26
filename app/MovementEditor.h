#pragma once
#include "Panel.h"

// Request 120: the anchored movement editor from docs/ui/PARAMETER_MOVEMENT.md.
// It opens in a JUCE CallOutBox beside the parameter's wave button, so the player
// stays on the panel. Each source reaching the parameter gets one row: this
// route's signed Depth, the source's Rate / Shape / Reset (shared sources say so),
// Bypass for a live A/B, Make independent, and Remove.
namespace {
struct MovementActions {
    std::function<void()> add;                                       // a private LFO for this parameter
    std::function<void(const std::string&)> useExisting;            // route an LFO already in the patch
    std::function<void(const MovementLink&,float)> setDepth;         // route amount, or a cable source's level
    std::function<void(const MovementLink&,bool)> bypass;            // live only; the saved amount is kept
    std::function<bool(const MovementLink&)> bypassed;
    std::function<void(const MovementLink&)> remove;
    std::function<void(const MovementLink&)> makeIndependent;
    std::function<void(const std::string&,const std::string&,float)> setParameter;   // the source's own settings
    std::function<String(const Endpoint&)> labelOf;
    std::function<String(float)> format;                             // destination value text
};
class MovementEditor final : public juce::Component {
public:
    static constexpr int width=470,pad=10,gap=6,knob=40;
    MovementEditor(const Endpoint& target,const Parameter& destination,float baseValue,const GraphSpec& graph,
                   std::vector<MovementLink> sources,std::vector<std::pair<std::string,int>> others,MovementActions a)
        :dest(target),param(destination),base(baseValue),links(std::move(sources)),available(std::move(others)),actions(std::move(a)) {
        look.markings=paper;
        title.setText((actions.labelOf?actions.labelOf(dest):String(param.label)).toUpperCase()+"  MOVEMENT",dontSendNotification);
        title.setFont(font(text::header).boldened());title.setColour(Label::textColourId,paper);addAndMakeVisible(title);
        range.setFont(font(text::value));range.setColour(Label::textColourId,paper.withAlpha(.8f));range.setJustificationType(Justification::centredRight);
        addAndMakeVisible(range);
        for(size_t i=0;i<links.size();++i){rows.push_back(std::make_unique<Row>(*this,links[i],graph));addAndMakeVisible(*rows.back());}
        addAndMakeVisible(addButton);addButton.setTooltip("Layer another free-running LFO on this parameter");
        addButton.onClick=[this]{if(actions.add)actions.add();};
        if(!available.empty()){
            addAndMakeVisible(useButton);useButton.setTooltip("Move this parameter with an LFO that already moves other controls");
            useButton.onClick=[this]{
                PopupMenu menu;
                for(size_t i=0;i<available.size();++i)menu.addItem(int(i)+1,"LFO "+String(available[i].second));
                menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&useButton),[safe=SafePointer<MovementEditor>(this)](int r){
                    if(safe&&r>0&&safe->actions.useExisting)safe->actions.useExisting(safe->available[size_t(r-1)].first);});
            };
        }
        int height=pad+22+gap;for(auto& r:rows)height+=r->height()+gap;height+=28+pad;
        setSize(width,height);updateRange();
    }
    ~MovementEditor() override {rows.clear();}
    void paint(Graphics& g) override {
        g.fillAll(Colour(0xff232527));
        for(const auto& r:rows){
            const auto b=r->getBounds().toFloat();
            g.setColour(Colour(0xff2c2f32));g.fillRoundedRectangle(b,4);
            g.setColour(r->link.colour());g.fillRoundedRectangle(b.withWidth(4),2);
        }
    }
    void resized() override {
        auto area=getLocalBounds().reduced(pad);
        auto top=area.removeFromTop(22);title.setBounds(top.removeFromLeft(220));range.setBounds(top);
        area.removeFromTop(gap);
        for(auto& r:rows){r->setBounds(area.removeFromTop(r->height()));area.removeFromTop(gap);}
        FlexBox footer;footer.justifyContent=FlexBox::JustifyContent::flexStart;
        footer.items.add(FlexItem(addButton).withWidth(140).withHeight(26).withMargin({0,float(gap),0,0}));
        if(useButton.isVisible())footer.items.add(FlexItem(useButton).withWidth(150).withHeight(26));
        footer.performLayout(area.removeFromTop(28).toFloat());
    }
    // The range the engine reaches with every route at its extreme, in the destination's units.
    void updateRange(){
        float extent=0;
        for(const auto& r:rows)if(r->link.route>=0)extent+=std::abs(r->link.amount*r->link.sourceDepth);
        auto value=[&](float offset){
            offset=std::clamp(offset,-1.f,1.f);
            const float v=param.logarithmic&&param.minimum>0?base*std::exp(offset*std::log(param.maximum/param.minimum)):base+offset*(param.maximum-param.minimum);
            return std::clamp(v,param.minimum,param.maximum);
        };
        auto text=[&](float v){return actions.format?actions.format(v):String(v,3);};
        range.setText(text(base)+" base  /  "+text(value(-extent))+String::fromUTF8(" \xe2\x80\x93 ")+text(value(extent)),dontSendNotification);
    }
    Control* depthControl(size_t row) const {return row<rows.size()?rows[row]->depth.get():nullptr;}
    size_t rowCount() const {return rows.size();}
    void focusFirstDepth(){if(!rows.empty()&&rows.front()->depth)rows.front()->depth->slider.grabKeyboardFocus();}
private:
    struct Row final : juce::Component {
        MovementEditor& owner;MovementLink link;Label name,role;
        std::unique_ptr<Control> depth,level,rate,shape,reset;
        TextButton bypass{"Bypass"},independent{"Make independent"},remove{"Remove"};
        Row(MovementEditor& editor,const MovementLink& l,const GraphSpec& graph):owner(editor),link(l){
            name.setText(link.name(),dontSendNotification);name.setFont(font(text::header).boldened());name.setColour(Label::textColourId,link.colour());
            String who;
            if(link.route<0)who="CV cable. Level sets the depth";
            else if(link.destinations.size()<=1)who=privateMovementSource(link.source)?"private to this control":"moves only this control";
            else{
                StringArray others;
                for(const auto& d:link.destinations)if(!(d.node==owner.dest.node&&d.port==owner.dest.port))
                    others.addIfNotAlreadyThere(owner.actions.labelOf?owner.actions.labelOf(d):String(d.port));
                who="shared with "+others.joinIntoString(", ")+". Rate and shape move all of them";
            }
            role.setText(who,dontSendNotification);role.setFont(font(text::label));role.setColour(Label::textColourId,paper.withAlpha(.75f));
            role.setMinimumHorizontalScale(.8f);
            for(auto* c:std::initializer_list<juce::Component*>{&name,&role,&remove})addAndMakeVisible(c);
            const auto& lfoSpec=panel("mod.lfo").parameters;
            auto lfoParam=[&](const char* key)->const Parameter&{return *std::find_if(lfoSpec.begin(),lfoSpec.end(),[&](const Parameter& p){return p.id==key;});};
            auto sourceValue=[&](const char* key){return nodeParameter(graph,link.source,key,lfoParam(key).initial);};
            auto sourceControl=[&](const char* key,const String& label){
                Parameter p=lfoParam(key);p.label=label.toStdString();
                auto c=std::make_unique<Control>(p,sourceValue(key),[this,id=std::string(key)](float v){
                    if(id=="depth"){link.sourceDepth=v;owner.updateRange();}
                    if(owner.actions.setParameter)owner.actions.setParameter(link.source,id,v);},owner.look,paper);
                addAndMakeVisible(*c);return c;
            };
            if(link.route>=0){
                const Parameter amount{"amount","Depth","",-1,1,0,false,.001};
                depth=std::make_unique<Control>(amount,link.amount,[this](float v){
                    link.amount=v;owner.updateRange();if(owner.actions.setDepth)owner.actions.setDepth(link,v);},owner.look,paper);
                depth->slider.setTooltip("This control's depth only. Negative moves it the opposite way");
                addAndMakeVisible(*depth);
                addAndMakeVisible(bypass);bypass.setClickingTogglesState(true);
                bypass.setToggleState(owner.actions.bypassed&&owner.actions.bypassed(link),dontSendNotification);
                bypass.setColour(TextButton::buttonOnColourId,orange);bypass.setColour(TextButton::textColourOnId,ink);
                bypass.setTooltip("Hear the control without this movement; the depth is kept");
                bypass.onClick=[this]{if(owner.actions.bypass)owner.actions.bypass(link,bypass.getToggleState());};
                // A source saved before unit amplitude keeps its own level; show it rather than hide a second depth.
                if(link.lfo&&std::abs(link.sourceDepth-1)>.0005f)level=sourceControl("depth","Source level");
            }else if(link.lfo)depth=sourceControl("depth","Level");
            if(link.lfo){
                rate=sourceControl("rate","Rate");
                rate->slider.textFromValueFunction=[](double v){return String(v,v<1?3:v<10?2:1)+" Hz / "+(v<1?String(1/v,1)+" s":String(1000/v,0)+" ms");};
                rate->slider.updateText();
                shape=sourceControl("wave","Shape");reset=sourceControl("retrigger","Reset on note");
                if(link.destinations.size()>1&&link.route>=0){
                    addAndMakeVisible(independent);independent.setTooltip("Copy this LFO so only this control follows the copy");
                    independent.onClick=[this]{if(owner.actions.makeIndependent)owner.actions.makeIndependent(link);};
                }
            }
            remove.setTooltip("Remove this movement only; other controls keep theirs");
            remove.onClick=[this]{if(owner.actions.remove)owner.actions.remove(link);};
        }
        std::vector<Control*> cells() const {
            std::vector<Control*> c;for(auto* p:{depth.get(),level.get(),rate.get(),shape.get(),reset.get()})if(p)c.push_back(p);return c;
        }
        static int cellHeight(){return knob+Control::labelHeight+Control::valueHeight;}
        int height() const {return 6+20+2+16+4+cellHeight()+6;}
        void resized() override {
            auto area=getLocalBounds().withTrimmedLeft(10).withTrimmedRight(8).withTrimmedTop(6).withTrimmedBottom(6);
            auto head=area.removeFromTop(20);
            FlexBox buttons;buttons.flexDirection=FlexBox::Direction::rowReverse;
            buttons.items.add(FlexItem(remove).withWidth(70).withHeight(20));
            if(independent.isVisible())buttons.items.add(FlexItem(independent).withWidth(122).withHeight(20).withMargin({0,4,0,0}));
            if(bypass.isVisible())buttons.items.add(FlexItem(bypass).withWidth(64).withHeight(20).withMargin({0,4,0,0}));
            buttons.performLayout(head.toFloat());
            name.setBounds(head.removeFromLeft(56));
            area.removeFromTop(2);role.setBounds(area.removeFromTop(16));area.removeFromTop(4);
            Grid grid;grid.columnGap=Grid::Px(gap);
            const auto c=cells();
            for(auto* cell:c)grid.templateColumns.add(Grid::TrackInfo(Grid::Px(std::max(knob+14,cell->textWidth()+6))));
            grid.templateRows.add(Grid::TrackInfo(Grid::Px(cellHeight())));
            for(auto* cell:c)grid.items.add(GridItem(*cell));
            grid.performLayout(area.removeFromTop(cellHeight()));
        }
    };
    friend struct Row;
    Endpoint dest;Parameter param;float base;std::vector<MovementLink> links;
    std::vector<std::pair<std::string,int>> available;MovementActions actions;
    KnobLook look;Label title,range;std::vector<std::unique_ptr<Row>> rows;
    TextButton addButton{"+ Add movement"},useButton{"Use existing LFO..."};
};
}
