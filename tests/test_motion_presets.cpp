#include "../app/PatchIO.h"
#include <cstdio>
#include <fstream>
#include <chrono>
int main(){try{
    using namespace optisynth;
    auto render=[](const RackDocument& d){Runtime r(d.graph,{},d.spectral);r.clock.tempo(d.tempo);r.clock.play(true);std::vector<float> l(48000*12),right(l.size());r.render(l.data(),right.data(),int(l.size()),{});l.insert(l.end(),right.begin(),right.end());return l;};
    for(int i=11;i<=12;++i){auto d=starterDocument(i,.25f);prepareSamples(d);auto saved=juce::JSON::toString(rackJSON(d));auto recalled=readRack(juce::JSON::parse(saved));prepareSamples(recalled);
        auto a=render(d),b=render(recalled);if(a!=b)throw std::runtime_error("Motion stereo state recall differs");double energy=0;float peak=0;for(float x:a){if(!std::isfinite(x))throw std::runtime_error("Nonfinite output");energy+=x*x;peak=std::max(peak,std::abs(x));}if(energy<1e-4)throw std::runtime_error("Silent Motion starter");
        const std::string path="patches/"+std::string(i==11?"motion-night-current":"motion-glass-tide")+".optisynth";
        std::ofstream out(path);out<<saved.replace("\r\n","\n").toStdString()<<"\n";if(!out)throw std::runtime_error("Save Motion fixture failed");
        std::printf("PASS %s: 12 seconds stereo, exact JSON/audio recall, peak %.5f, mean square %.6f\n",path.c_str(),peak,energy/a.size());
    }
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
