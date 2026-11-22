#pragma once
#include "Panel.h"

// UI-thread projection of the validated patch. Reading the graph never touches DSP state.
namespace {
class SignalFlow final : public juce::Component, private ListBoxModel {
    enum class Kind { audio, control, notes, modulation };
    struct Node {
        String id, name;
        bool active;
        int column=0;
        Rectangle<int> bounds;
        std::vector<int> incoming,outgoing;
    };
    struct Wire {
        int from,to;
        String source,destination;
        Kind kind;
        int modulation=-1;
        float depth=0;
    };
    struct Canvas final : juce::Component {
        SignalFlow& owner;
        explicit Canvas(SignalFlow& o):owner(o){}
        void paint(Graphics& g) override {owner.drawGraph(g);}
        void mouseDown(const MouseEvent& e) override {
            for(size_t i=0;i<owner.nodes.size();++i)
                if(owner.nodes[i].bounds.contains(e.getPosition())){owner.select(int(i));return;}
        }
    };
    static Colour colour(Kind k){
        switch(k){
            case Kind::audio:return Colour(0xff85c2be);
            case Kind::control:return Colour(0xffe2b275);
            case Kind::notes:return Colour(0xff92b5ed);
            case Kind::modulation:return Colour(0xffc9a1d5);
        }
        return paper;
    }
    static String label(Kind k){
        switch(k){
            case Kind::audio:return "Audio";case Kind::control:return "Control";
            case Kind::notes:return "Notes";case Kind::modulation:return "Matrix";
        }
        return {};
    }
public:
    SignalFlow():canvas(*this),list("Selected module connections",this){
        addAndMakeVisible(view);addAndMakeVisible(list);
        view.setViewedComponent(&canvas,false);view.setScrollBarsShown(true,true);
        list.setRowHeight(23);list.setColour(ListBox::backgroundColourId,Colour(0xff202528));
        list.setWantsKeyboardFocus(false);
    }
    ~SignalFlow() override {list.setModel(nullptr);view.setViewedComponent(nullptr,false);}
    void rebuild(const GraphSpec& graph,const Registry& registry,const std::string& preferred){
        const auto plan=compileGraph(graph,registry);
        const String previous=selected>=0?nodes[size_t(selected)].id:String();
        nodes.clear();wires.clear();selected=-1;
        std::map<std::string,int> index;
        for(const auto& spec:graph.nodes){
            index[spec.id]=int(nodes.size());
            const bool active=std::find(plan.unusedNodes.begin(),plan.unusedNodes.end(),spec.id)==plan.unusedNodes.end();
            nodes.push_back({String(spec.id),String(panel(spec.type).name),active});
        }
        const int output=int(nodes.size());
        nodes.push_back({{},"Rack output",true});
        auto addWire=[&](Wire edge){
            nodes[edge.from].outgoing.push_back(int(wires.size()));nodes[edge.to].incoming.push_back(int(wires.size()));
            wires.push_back(std::move(edge));
        };
        auto signalKind=[&](const Endpoint& ep){
            const auto& type=graph.nodes[index.at(ep.node)].type;
            for(const auto& m:registry)if(m.id==type)for(const auto& p:m.outputs)if(p.id==ep.port){
                if(p.type==SignalType::Event)return Kind::notes;
                if(p.type==SignalType::Control)return Kind::control;
            }
            return Kind::audio;
        };
        for(const auto& link:graph.connections)
            addWire({index.at(link.source.node),index.at(link.destination.node),String(link.source.port),
                     String(link.destination.port),signalKind(link.source)});
        for(size_t i=0;i<graph.modulations.size();++i){
            const auto& m=graph.modulations[i];
            addWire({index.at(m.source.node),index.at(m.destination.node),String(m.source.port),
                     String(m.destination.port),Kind::modulation,int(i),m.amount});
        }
        for(const auto& root:graph.outputs)
            addWire({index.at(root.node),output,String(root.port),"Main out",Kind::audio});
        // All connections, including those between dormant modules, form a DAG.
        std::vector<int> pending,ready;
        for(size_t i=0;i<nodes.size();++i){
            pending.push_back(int(nodes[i].incoming.size()));if(!pending.back())ready.push_back(int(i));
        }
        for(size_t head=0;head<ready.size();++head){
            const int src=ready[head];
            for(int e:nodes[src].outgoing){const int dst=wires[e].to;
                nodes[dst].column=std::max(nodes[dst].column,nodes[src].column+1);
                if(--pending[dst]==0)ready.push_back(dst);}
        }
        // Place a short side chain beside the module it feeds. Otherwise a
        // direct sub or envelope wire can look as if it passes through a filter.
        for(auto it=ready.rbegin();it!=ready.rend();++it){
            auto& n=nodes[*it];
            if(n.outgoing.empty())continue;
            int nearest=int(nodes.size());
            for(int edge:n.outgoing)nearest=std::min(nearest,nodes[wires[edge].to].column);
            n.column=nearest-1;
        }
        const auto kept=index.find(previous.toStdString()),initial=index.find(preferred);
        select(kept!=index.end()?kept->second:initial!=index.end()?initial->second:output);
        layoutGraph();
    }
    void updateDepth(int index,float value){
        for(auto& w:wires)if(w.modulation==index){w.depth=value;list.repaint();return;}
    }
    int connectionCount() const {return int(wires.size());}
    void resized() override {
        auto r=getLocalBounds().reduced(10);
        r.removeFromTop(36);list.setBounds(r.removeFromBottom(92));
        r.removeFromBottom(32);view.setBounds(r);layoutGraph();
    }
    void paint(Graphics& g) override {
        g.fillAll(Colour(0xff191e21));g.setColour(paper);g.setFont(font(17).boldened());
        g.drawText("Signal flow",16,7,140,28,Justification::centredLeft);
        int x=167;
        for(Kind k:{Kind::audio,Kind::control,Kind::notes,Kind::modulation}){
            g.setColour(colour(k));g.fillRect(x,20,18,3);g.setFont(font(13));
            g.drawText(label(k),x+24,8,65,27,Justification::centredLeft);x+=94;
        }
        if(selected>=0){
            const auto& n=nodes[selected];
            g.setColour(paper);g.setFont(font(14).boldened());
            g.drawText((n.id.isEmpty()?n.name:n.id)+"   /   Connections",16,list.getY()-29,getWidth()-32,27,Justification::centredLeft);
        }
    }
private:
    void layoutGraph(){
        if(nodes.empty())return;
        int cols=1;for(const auto& n:nodes)cols=std::max(cols,n.column+1);
        std::vector<int> counts(size_t(cols),0),rows(size_t(cols),0);
        for(const auto& n:nodes)++counts[n.column];
        const int gap=40,margin=16,step=78;
        const int width=std::clamp((view.getWidth()-2*margin-(cols-1)*gap-15)/cols,128,192);
        const bool hasBypass=std::any_of(wires.begin(),wires.end(),[&](const Wire& w){
            return nodes[w.to].column>nodes[w.from].column+1;
        });
        const int lanes=hasBypass?44:0;
        const int height=std::max(view.getHeight()-14,lanes+2*margin+step*(*std::max_element(counts.begin(),counts.end())));
        for(auto& n:nodes){
            const int y=lanes+(height-lanes-counts[n.column]*step)/2+rows[n.column]++*step;
            n.bounds={margin+n.column*(width+gap),y+8,width,55};
        }
        canvas.setSize(std::max(view.getWidth()-14,2*margin+cols*width+(cols-1)*gap),height);canvas.repaint();
    }
    void select(int i){
        selected=i;details=nodes[i].incoming;details.insert(details.end(),nodes[i].outgoing.begin(),nodes[i].outgoing.end());
        list.updateContent();list.getViewport()->setViewPosition(0,0);list.repaint();canvas.repaint();repaint();
    }
    void drawGraph(Graphics& g){
        g.fillAll(Colour(0xff14191c));
        for(size_t i=0;i<wires.size();++i){
            const auto& w=wires[i];const auto& a=nodes[w.from];const auto& b=nodes[w.to];
            auto socketY=[i](const Node& n,const auto& ports){
                const auto j=std::find(ports.begin(),ports.end(),int(i))-ports.begin();
                return n.bounds.getY()+8.f+39.f*(float(j)+.5f)/float(ports.size());
            };
            const float x=a.bounds.getRight(),u=b.bounds.getX(),y=socketY(a,a.outgoing),v=socketY(b,b.incoming);
            Path line;line.startNewSubPath(x,y);
            if(b.column==a.column+1)line.cubicTo(x+23,y,u-23,v,u-5,v);
            else {
                const float lane=7.f+6.f*float(i%6);
                line.lineTo(x+12,y);line.lineTo(x+12,lane);line.lineTo(u-14,lane);
                line.lineTo(u-14,v);line.lineTo(u-5,v);line=line.createPathWithRoundedCorners(4.f);
            }
            const bool related=w.from==selected||w.to==selected;
            g.setColour(colour(w.kind).withAlpha(!b.active?.24f:related?1.f:.62f));g.strokePath(line,PathStrokeType(related?2.5f:1.5f));
            Path tip;tip.addTriangle(u,v,u-7,v-3.5f,u-7,v+3.5f);g.fillPath(tip);
        }
        for(size_t i=0;i<nodes.size();++i){
            const auto& n=nodes[i];const auto r=n.bounds.toFloat();const bool chosen=int(i)==selected;
            g.setColour(chosen?Colour(0xff344a50):Colour(0xff292f33));g.fillRoundedRectangle(r,5);
            g.setColour(chosen?teal:Colour(0xff58636a));g.drawRoundedRectangle(r.reduced(.6f),5,chosen?2:1);
            g.setColour(paper.withAlpha(n.active?1.f:.55f));g.setFont(font(14).boldened());
            g.drawFittedText(n.id.isEmpty()?n.name:n.id,n.bounds.reduced(10,4).removeFromTop(23),Justification::centredLeft,1);
            g.setFont(font(12));g.drawFittedText(n.id.isEmpty()?"Main output":n.active?n.name:"Unrouted / "+n.name,
                n.bounds.reduced(10,5).removeFromBottom(20),Justification::centredLeft,1);
        }
    }
    int getNumRows() override{return int(details.size());}
    void paintListBoxItem(int row,Graphics& g,int width,int height,bool) override{
        if(row<0||row>=int(details.size()))return;
        const auto& w=wires[details[row]];g.setColour(colour(w.kind));g.setFont(font(13));
        g.drawText(label(w.kind),8,0,70,height,Justification::centredLeft);
        const auto& a=nodes[w.from];const auto& b=nodes[w.to];
        const String dest=b.id.isEmpty()?w.destination:b.id+"."+w.destination;
        g.setColour(paper);g.drawFittedText(a.id+"."+w.source+String::fromUTF8("  →  ")+dest+
            (w.modulation>=0?"   ("+String(w.depth*100,1)+"%)":""),84,0,width-92,height,Justification::centredLeft,1);
    }
    std::vector<Node> nodes;std::vector<Wire> wires;std::vector<int> details;int selected=-1;
    Canvas canvas;Viewport view;ListBox list;
};
}
