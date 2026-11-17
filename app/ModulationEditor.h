#pragma once
#include "Panel.h"
#include "PatchIO.h"

namespace {
class ModulationEditor final:public juce::Component {
public:
    std::function<bool(const Endpoint&,const Endpoint&,float)> addRoute;
    std::function<void(int,float)> setDepth;
    std::function<void(int)> removeRoute;
    ModulationEditor(){
        setSize(960,560);for(auto* c:std::initializer_list<juce::Component*>{&source,&target,&amount,&add,&help,&viewport})addAndMakeVisible(c);
        viewport.setViewedComponent(&rows,false);amount.setRange(-1,1,.001);amount.setValue(.15);amount.setTextBoxStyle(Slider::TextBoxRight,false,65,24);
        add.onClick=[this]{int s=source.getSelectedId()-1,t=target.getSelectedId()-1;if(s>=0&&t>=0&&addRoute)addRoute(sources[size_t(s)],targets[size_t(t)],float(amount.getValue()));};
        help.setText("Depth adds movement around the saved knob setting. Spectral sources follow its newest sounding voice.",dontSendNotification);
        help.setColour(Label::textColourId,paper);help.setFont(font(13));
    }
    ~ModulationEditor() override {viewport.setViewedComponent(nullptr,false);}
    void editDepth(int index,float depth){if(index>=0&&index<int(controls.size()))controls[size_t(index)]->depth.setValue(depth,sendNotificationSync);}
    void refresh(const RackDocument& d){
        const auto oldSource=source.getText(),oldTarget=target.getText();source.clear(dontSendNotification);target.clear(dontSendNotification);sources.clear();targets.clear();
        auto registry=moduleRegistry();
        for(const auto& node:d.graph.nodes){
            const auto spectral=node.type=="spectral.engine"?effectiveSpectral(d,node.id):SpectralPtr{};
            const auto it=std::find_if(registry.begin(),registry.end(),[&](const ModuleType& t){return t.id==node.type;});
            for(const auto& port:it->outputs)if(port.type==SignalType::Control){sources.push_back({node.id,port.id});source.addItem(node.id+" / "+port.id,int(sources.size()));}
            const auto& ps=panel(node.type).parameters;
            for(size_t i=0;i<ps.size();++i){
                bool eligible=true;
                if(node.type=="spectral.engine"&&i<partials::kLiveControls){eligible=spectral&&spectralParameterActive(spectral->patch,int(i));
                    if(i>=48&&i<53&&spectral&&spectral->patch.lfo[i-48].syncBeats==0){const auto port="lfo"+std::to_string(i-47);for(const auto& route:d.graph.modulations)if(route.source.node==node.id&&route.source.port==port)eligible=true;for(const auto& route:d.graph.connections)if(route.source.node==node.id&&route.source.port==port)eligible=true;}
                }
                if(eligible){targets.push_back({node.id,ps[i].id});target.addItem(node.id+" / "+ps[i].label,int(targets.size()));}
            }
        }
        source.setTextWhenNothingSelected("Choose modulation source");target.setTextWhenNothingSelected("Choose destination parameter");
        for(int i=0;i<source.getNumItems();++i)if(source.getItemText(i)==oldSource)source.setSelectedItemIndex(i,dontSendNotification);
        for(int i=0;i<target.getNumItems();++i)if(target.getItemText(i)==oldTarget)target.setSelectedItemIndex(i,dontSendNotification);
        controls.clear();
        for(size_t i=0;i<d.graph.modulations.size();++i){
            const auto& m=d.graph.modulations[i];auto row=std::make_unique<Row>();std::string targetName=m.destination.port;
            for(const auto& n:d.graph.nodes)if(n.id==m.destination.node)for(const auto& p:panel(n.type).parameters)if(p.id==m.destination.port)targetName=p.label;
            row->label.setText(m.source.node+" / "+m.source.port+"  >  "+m.destination.node+" / "+targetName,dontSendNotification);
            row->label.setColour(Label::textColourId,paper);row->label.setFont(font(13));row->depth.setRange(-1,1,.001);row->depth.setValue(m.amount);row->depth.setTextBoxStyle(Slider::TextBoxRight,false,65,24);
            row->depth.onValueChange=[this,index=int(i),p=row.get()]{if(setDepth)setDepth(index,float(p->depth.getValue()));};
            row->remove.onClick=[this,index=int(i)]{if(removeRoute)removeRoute(index);};
            rows.addAndMakeVisible(row->label);rows.addAndMakeVisible(row->depth);rows.addAndMakeVisible(row->remove);controls.push_back(std::move(row));
        }
        resized();repaint();
    }
    void paint(Graphics& g) override {g.fillAll(Colour(0xff222426));g.setColour(paper);g.setFont(font(23));g.drawText("MODULATION MATRIX",20,14,getWidth()-40,32,Justification::centredLeft);if(controls.empty()){g.setFont(font(15));g.drawText("Add a route to connect modules. Base knob values remain unchanged.",22,170,getWidth()-44,30,Justification::centredLeft);}}
    void resized() override {
        source.setBounds(20,60,260,32);target.setBounds(292,60,360,32);amount.setBounds(663,60,174,32);add.setBounds(850,60,90,32);help.setBounds(20,100,getWidth()-40,48);
        viewport.setBounds(20,155,getWidth()-40,getHeight()-175);rows.setSize(getWidth()-55,std::max(viewport.getHeight(),int(controls.size())*52));
        for(size_t i=0;i<controls.size();++i){const int y=int(i)*52;auto& r=*controls[i];r.label.setBounds(0,y,rows.getWidth()-320,44);r.depth.setBounds(rows.getWidth()-310,y+5,205,32);r.remove.setBounds(rows.getWidth()-95,y+5,90,32);}
    }
private:
    struct Row {Label label;Slider depth;TextButton remove{"Remove"};};
    ComboBox source,target;Slider amount;TextButton add{"Add route"};Label help;Viewport viewport;juce::Component rows;
    std::vector<Endpoint> sources,targets;std::vector<std::unique_ptr<Row>> controls;
};
class MatrixWindow final:public DocumentWindow {
public:
    ModulationEditor* editor;
    MatrixWindow():DocumentWindow("Optisynth modulation",ink,DocumentWindow::closeButton){setUsingNativeTitleBar(true);editor=new ModulationEditor;setContentOwned(editor,true);centreWithSize(960,560);}
    void closeButtonPressed() override {setVisible(false);}
};
}
