#pragma once
#include "PatchIO.h"

// Request 146: hear what each movement assignment of a saved patch does.
// Renders WAV files only. It opens no audio device and never writes the preset.
//
//   Optisynth --movement-audit "<preset file or factory:N>" "<folder>" [node.parameter=value ...] [seconds=N]
//
// as-saved   the patch exactly as saved, movement running
// still      every modulation route at zero depth
// NN-low / NN-high   route NN removed, its destination parked at the two values
//                    the movement reaches (base -/+ depth), all other routes off
// NN-min / NN-max    the same destination at the ends of its whole range
// The folder's audit.json lists each route with those values for tools/movement_audit.py.
namespace optisynth {
inline void movementAudit(const juce::String& preset,const juce::File& folder,const juce::StringArray& overrides,
                          double movingSeconds=40,double stillSeconds=10) {
    auto doc=preset.startsWith("factory:")?starterDocument(preset.fromFirstOccurrenceOf(":",false,false).getIntValue(),.25f)
                                          :readRack(juce::JSON::parse(juce::File(preset)));
    for(const auto& o:overrides){
        if(o.startsWith("seconds=")){movingSeconds=o.fromFirstOccurrenceOf("=",false,false).getDoubleValue();continue;}
        const auto node=o.upToFirstOccurrenceOf(".",false,false).toStdString();
        const auto param=o.fromFirstOccurrenceOf(".",false,false).upToFirstOccurrenceOf("=",false,false).toStdString();
        const float value=o.fromFirstOccurrenceOf("=",false,false).getFloatValue();
        bool found=false;
        for(auto& n:doc.graph.nodes)if(n.id==node){
            auto it=std::find_if(n.parameters.begin(),n.parameters.end(),[&](const auto& p){return p.first==param;});
            if(it!=n.parameters.end())it->second=value;else n.parameters.push_back({param,value});
            found=true;
        }
        if(!found)throw std::runtime_error("No node for override "+o.toStdString());
    }
    prepareSamples(doc);
    if(!folder.createDirectory())throw std::runtime_error("Cannot create "+folder.getFullPathName().toStdString());
    const auto bank=sampleBank(doc);
    auto render=[&](const GraphSpec& graph,const juce::String& name,double seconds){
        Runtime r(graph,bank,doc.spectral);r.clock.tempo(doc.tempo);r.clock.play(true);
        const int frames=int(48000*seconds);
        juce::AudioBuffer<float> sound(2,frames);
        for(int at=0;at<frames;at+=512){
            const int n=std::min(512,frames-at);
            r.render(sound.getWritePointer(0)+at,sound.getWritePointer(1)+at,n,{});
        }
        sound.applyGain(doc.gain);
        auto file=folder.getChildFile(name+".wav");file.deleteFile();
        auto stream=file.createOutputStream();juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000,2,24,{},0));
        if(!writer)throw std::runtime_error("Cannot write "+file.getFullPathName().toStdString());
        stream.release();writer->writeFromAudioSampleBuffer(sound,0,frames);
    };
    auto nodeOf=[&](GraphSpec& g,const std::string& id)->NodeSpec*{
        for(auto& n:g.nodes)if(n.id==id)return &n;return nullptr;};
    auto valueOf=[](const NodeSpec& n,const Parameter& p){
        for(const auto& v:n.parameters)if(v.first==p.id)return std::clamp(v.second,p.minimum,p.maximum);
        return p.initial;};
    auto park=[&](GraphSpec g,const Endpoint& to,float value){
        for(auto& m:g.modulations)m.amount=0;
        auto* n=nodeOf(g,to.node);
        auto it=std::find_if(n->parameters.begin(),n->parameters.end(),[&](const auto& p){return p.first==to.port;});
        if(it!=n->parameters.end())it->second=value;else n->parameters.push_back({to.port,value});
        return g;
    };

    render(doc.graph,"as-saved",movingSeconds);
    auto still=doc.graph;for(auto& m:still.modulations)m.amount=0;
    render(still,"still",movingSeconds);
    juce::Array<juce::var> routes;
    for(size_t i=0;i<doc.graph.modulations.size();++i){
        const auto& m=doc.graph.modulations[i];
        auto* target=nodeOf(doc.graph,m.destination.node);auto* source=nodeOf(doc.graph,m.source.node);
        if(!target)continue;
        const auto* spec=findParameter(target->type,m.destination.port);
        if(!spec)continue;
        float depth=1,rate=0;
        if(source&&source->type=="mod.lfo")for(const auto& v:source->parameters){
            if(v.first=="depth")depth=v.second;
            if(v.first=="rate")rate=v.second;
        }
        const float base=valueOf(*target,*spec),reach=std::clamp(m.amount*depth,-1.f,1.f);
        const float low=movementValue(*spec,base,-std::abs(reach)),high=movementValue(*spec,base,std::abs(reach));
        const auto tag=juce::String(int(i)).paddedLeft('0',2);
        render(park(doc.graph,m.destination,low),tag+"-low",stillSeconds);
        render(park(doc.graph,m.destination,high),tag+"-high",stillSeconds);
        render(park(doc.graph,m.destination,spec->minimum),tag+"-min",stillSeconds);
        render(park(doc.graph,m.destination,spec->maximum),tag+"-max",stillSeconds);
        auto* o=new juce::DynamicObject();
        o->setProperty("index",int(i));o->setProperty("source",juce::String(m.source.node));
        o->setProperty("node",juce::String(m.destination.node));o->setProperty("parameter",juce::String(m.destination.port));
        o->setProperty("label",juce::String(spec->label));o->setProperty("amount",m.amount);
        o->setProperty("lfo_depth",depth);o->setProperty("lfo_rate_hz",rate);
        o->setProperty("base",base);o->setProperty("low",low);o->setProperty("high",high);
        o->setProperty("minimum",spec->minimum);o->setProperty("maximum",spec->maximum);
        routes.add(juce::var(o));
    }
    auto* report=new juce::DynamicObject();
    report->setProperty("preset",preset);report->setProperty("tempo",doc.tempo);
    report->setProperty("moving_seconds",movingSeconds);report->setProperty("still_seconds",stillSeconds);
    report->setProperty("overrides",juce::var(overrides));report->setProperty("routes",routes);
    folder.getChildFile("audit.json").replaceWithText(juce::JSON::toString(juce::var(report)));
}
}
