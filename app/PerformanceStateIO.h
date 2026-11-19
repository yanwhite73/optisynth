#pragma once
#include <JuceHeader.h>
#include "../src/modular/performance.h"

namespace optisynth {
inline juce::var performanceJSON(const PerformanceData& d){
    validatePerformance(d);auto* root=new juce::DynamicObject;
    root->setProperty("version",1);root->setProperty("seed",juce::int64(d.seed));root->setProperty("first",d.chainFirst);root->setProperty("last",d.chainLast);
    juce::Array<juce::var> tracks,chain;
    for(const auto& track:d.tracks){auto* t=new juce::DynamicObject;t->setProperty("name",juce::String(track.name));t->setProperty("initial",track.initial);juce::Array<juce::var> patterns;
        for(const auto& pattern:track.patterns){auto* p=new juce::DynamicObject;
            p->setProperty("first",pattern.first);p->setProperty("last",pattern.last);p->setProperty("division",pattern.division);p->setProperty("traversal",pattern.traversal);
            p->setProperty("rotation",pattern.rotation);p->setProperty("scale",pattern.scale);p->setProperty("root",pattern.root);p->setProperty("swing",pattern.swing);
            juce::Array<juce::var> steps;
            for(const auto& s:pattern.steps)steps.add(juce::Array<juce::var>{s.pitch,s.velocity,s.gate,s.chance,s.ratchets,s.condition,s.cv,s.slide,s.on,s.tie,s.protect});
            p->setProperty("steps",steps);patterns.add(juce::var(p));}
        t->setProperty("patterns",patterns);tracks.add(juce::var(t));}
    for(const auto& row:d.chain)chain.add(juce::Array<juce::var>{row.bars,row.patterns[0],row.patterns[1],row.patterns[2],row.patterns[3]});
    root->setProperty("tracks",tracks);root->setProperty("chain",chain);return root;
}
inline PerformancePtr readPerformance(const juce::var& value){
    auto integer=[](const juce::var& v){if(!v.isInt()&&!v.isInt64())throw std::invalid_argument("Expected performance integer");const auto n=juce::int64(v);if(n<-2147483647LL||n>2147483647LL)throw std::invalid_argument("Performance integer overflow");return int(n);};
    auto real=[](const juce::var& v){if(!v.isInt()&&!v.isInt64()&&!v.isDouble())throw std::invalid_argument("Expected performance number");const auto f=float(v);if(!std::isfinite(f))throw std::invalid_argument("Nonfinite performance value");return f;};
    auto boolean=[](const juce::var& v){if(!v.isBool())throw std::invalid_argument("Expected performance switch");return bool(v);};
    auto array=[](const juce::var& v,int count)->const juce::Array<juce::var>&{auto* a=v.getArray();if(!a||a->size()!=count)throw std::invalid_argument("Invalid performance array size");return *a;};
    if(!value.isObject()||integer(value["version"])!=1)throw std::invalid_argument("Unsupported performance data");
    auto data=std::make_shared<PerformanceData>();data->chainFirst=integer(value["first"]);data->chainLast=integer(value["last"]);
    const auto seed=value["seed"];if((!seed.isInt()&&!seed.isInt64())||juce::int64(seed)<1||juce::int64(seed)>4294967295LL)throw std::invalid_argument("Invalid performance seed");data->seed=uint32_t(juce::int64(seed));
    const auto& tracks=array(value["tracks"],4);
    for(int t=0;t<4;++t){auto& track=data->tracks[t];const auto& v=tracks[t];if(!v["name"].isString())throw std::invalid_argument("Missing track name");track.name=v["name"].toString().toStdString();track.initial=integer(v["initial"]);
        const auto& patterns=array(v["patterns"],8);
        for(int bank=0;bank<8;++bank){auto& p=track.patterns[bank];const auto& saved=patterns[bank];
            p.first=integer(saved["first"]);p.last=integer(saved["last"]);p.division=integer(saved["division"]);p.traversal=integer(saved["traversal"]);p.rotation=integer(saved["rotation"]);
            p.scale=integer(saved["scale"]);p.root=integer(saved["root"]);p.swing=real(saved["swing"]);const auto& steps=array(saved["steps"],64);
            for(int i=0;i<64;++i){auto& s=p.steps[i];const auto& a=array(steps[i],11);s.pitch=integer(a[0]);s.velocity=real(a[1]);s.gate=real(a[2]);s.chance=real(a[3]);s.ratchets=integer(a[4]);s.condition=integer(a[5]);s.cv=real(a[6]);s.slide=real(a[7]);s.on=boolean(a[8]);s.tie=boolean(a[9]);s.protect=boolean(a[10]);}}
    }
    const auto& chain=array(value["chain"],16);for(int i=0;i<16;++i){const auto& a=array(chain[i],5);data->chain[i].bars=integer(a[0]);for(int t=0;t<4;++t)data->chain[i].patterns[t]=integer(a[t+1]);}
    validatePerformance(*data);return data;
}
}
