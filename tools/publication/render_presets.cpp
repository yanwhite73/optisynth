// Offline renders of saved Optisynth presets for publication (request 77).
// Loads each .optisynth file exactly as the app does (readRack + prepareSamples),
// runs the transport at the preset's tempo, stops it, lets the tails ring, and
// writes a 48 kHz stereo float WAV. No audio device is opened.
//
// render_presets <out.wav> <preset.optisynth> <play seconds> <tail seconds>
#include "../../app/PatchIO.h"
#include <cstdio>
#include <fstream>
using namespace optisynth;

static void writeWav(const std::string& path,const std::vector<float>& l,const std::vector<float>& r){
    std::ofstream o(path,std::ios::binary);
    auto u16=[&](uint16_t v){o.put(char(v&255));o.put(char(v>>8));};auto u32=[&](uint32_t v){u16(uint16_t(v&65535));u16(uint16_t(v>>16));};
    const uint32_t bytes=uint32_t(l.size()*8);
    o.write("RIFF",4);u32(36+bytes);o.write("WAVEfmt ",8);u32(16);u16(3);u16(2);u32(48000);u32(48000*8);u16(8);u16(32);o.write("data",4);u32(bytes);
    for(size_t i=0;i<l.size();++i){o.write(reinterpret_cast<const char*>(&l[i]),4);o.write(reinterpret_cast<const char*>(&r[i]),4);}
}
int main(int argc,char** argv){
    if(argc<5){std::fprintf(stderr,"usage: render_presets <out.wav> <preset> <play s> <tail s>\n");return 2;}
    try{
        auto document=readRack(juce::JSON::parse(juce::File(juce::String(argv[2]))));
        pruneSampleReferences(document);prepareSamples(document);
        Runtime runtime(document.graph,sampleBank(document),document.spectral);
        runtime.clock.tempo(document.tempo);runtime.clock.play(true);
        const int play=int(std::atof(argv[3])*48000)/quantum*quantum,tail=int(std::atof(argv[4])*48000)/quantum*quantum;
        std::vector<float> l(size_t(play+tail)),r(size_t(play+tail));
        for(int i=0;i<play+tail;i+=quantum){
            if(i==play)runtime.clock.play(false);
            runtime.render(l.data()+i,r.data()+i,quantum,Note{});
        }
        float peak=0;for(size_t i=0;i<l.size();++i)peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});
        if(!(peak>1e-5f)){std::fprintf(stderr,"silent render\n");return 1;}
        const float gain=std::pow(10.f,-1.f/20)/peak;   // peak-levelled to -1 dBFS; no other processing
        const size_t fade=size_t(48000*3);               // 3 s fade at the very end of the tail
        for(size_t i=0;i<l.size();++i){float g=gain;if(i+fade>l.size())g*=float(l.size()-i)/float(fade);l[i]*=g;r[i]*=g;}
        writeWav(argv[1],l,r);
        std::printf("{\"preset\":\"%s\",\"tempo\":%.1f,\"seconds\":%.1f,\"raw_peak\":%.4f}\n",argv[2],document.tempo,double(l.size())/48000,double(peak));
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
    return 0;
}
