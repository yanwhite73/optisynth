#pragma once
#include <cstring>
namespace {
using Bytes=std::vector<uint8_t>;
void putWord(Bytes& b,uint64_t v,int bytes){for(int i=0;i<bytes;++i)b.push_back(uint8_t(v>>(i*8)));}
void chunk(Bytes& b,const char* id,const Bytes& data){b.insert(b.end(),id,id+4);putWord(b,data.size(),4);b.insert(b.end(),data.begin(),data.end());if(data.size()&1)b.push_back(0);}
Bytes fixture(int bits=16,int channels=1,int rate=48000,bool floating=false,bool extended=false,bool reverse=false){
    Bytes format,data,b{'R','I','F','F',0,0,0,0,'W','A','V','E'};
    putWord(format,extended?0xfffe:floating?3:1,2);putWord(format,channels,2);putWord(format,rate,4);putWord(format,rate*channels*bits/8,4);putWord(format,channels*bits/8,2);putWord(format,bits,2);
    if(extended){putWord(format,22,2);putWord(format,bits,2);putWord(format,0,4);putWord(format,floating?3:1,4);const uint8_t tail[]={0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};format.insert(format.end(),tail,tail+12);}
    for(int i=0;i<128;++i)for(int c=0;c<channels;++c){const double x=c?-.25:.5;
        if(floating){uint64_t word=0;if(bits==32){float f=float(x);uint32_t w;std::memcpy(&w,&f,4);word=w;}else std::memcpy(&word,&x,8);putWord(data,word,bits/8);}
        else if(bits==8)putWord(data,uint8_t(x*128+128),1);
        else putWord(data,uint64_t(int64_t(x*double(uint64_t(1)<<(bits-1)))),bits/8);
    }
    chunk(b,"JUNK",{1,2,3});if(reverse){chunk(b,"data",data);chunk(b,"fmt ",format);}else{chunk(b,"fmt ",format);chunk(b,"data",data);}
    uint32_t size=uint32_t(b.size()-8);for(int i=0;i<4;++i)b[4+i]=uint8_t(size>>(i*8));return b;
}
void sampleChecks(){
    for(int bits:{8,16,24,32,64})for(int channels:{1,2})for(bool ext:{false,true}){
        const bool floating=bits==64;auto s=decodeWave(fixture(bits,channels,44100,floating,ext,true));
        check(s->rate==44100&&s->mono.size()==128&&s->sourceChannels==channels&&s->mono[50]==(channels==1?.5f:.125f),"WAV PCM/float/extensible/stereo/odd chunk/order");
    }
    check(decodeWave(fixture(32,2,48000,true))->mono[0]==.125f,"Float32 stereo WAV");
    auto rejects=[](const Bytes& b){try{decodeWave(b);return false;}catch(const std::exception&){return true;}};
    auto good=fixture();bool allRejected=true;
    for(size_t i=0;i<good.size();++i)allRejected&=rejects(Bytes(good.begin(),good.begin()+i));
    check(allRejected,"Every truncated WAV prefix rejected");
    auto bad=good;bad[32]=0xff;check(rejects(bad),"Unsupported WAV tag rejected");
    bad=good;bad[44]=1;check(rejects(bad),"Invalid frame alignment rejected");
    bad=good;bad[16]=0xff;bad[17]=0xff;bad[18]=0xff;bad[19]=0xff;check(rejects(bad),"Oversized chunk rejected before allocation");
    bad=fixture(32,1,48000,true);bad[56]=0;bad[57]=0;bad[58]=0xc0;bad[59]=0x7f;check(rejects(bad),"NaN float rejected");
    bad=fixture(32,1,48000,true);bad[56]=0;bad[57]=0;bad[58]=0x80;bad[59]=0x7f;check(rejects(bad),"Infinite float rejected");
    bool cancelled=false;try{decodeWave(good,[]{return true;});}catch(const std::exception&){cancelled=true;}check(cancelled,"Decoder cancellation");
    bad=Bytes(maximumSampleBytes+1);check(rejects(bad),"32 MiB input bound");
    auto path="build/sample-fixture.wav";{std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(good.data()),good.size());}
    check(loadWave(path)->fingerprint==decodeWave(good)->fingerprint,"File decoding/fingerprint match");
    good.back()^=1;check(decodeWave(good)->fingerprint!=loadWave(path)->fingerprint,"Changed audio changes fingerprint");
    auto asset=std::make_shared<SampleData>();asset->rate=48000;asset->mono.assign(4800,.5f);
    GraphSpec g;g.nodes={{"source","sample.player",{}}};g.outputs={{"source","out"}};SampleBank bank{{"source",asset}};
    auto play=[&](GraphSpec spec,SamplePtr data,Note note,int frames){Runtime r(spec,{{"source",data}});std::vector<float> x(size_t(frames),0);r.render(x.data(),frames,note);return x;};
    auto duration=[](const std::vector<float>& x){size_t n=0;for(size_t i=0;i<x.size();++i)if(x[i]!=0)n=i+1;return n;};
    check(duration(play(g,asset,{48,1,true,true},10000))==4800,"Root note playback length");
    check(duration(play(g,asset,{60,1,true,true},10000))==2400,"Octave doubles playback speed");
    asset->rate=24000;check(duration(play(g,asset,{48,1,true,true},10000))==9600,"Source-rate conversion");asset->rate=48000;
    auto trim=g;trim.nodes[0].parameters={{"start",.25f},{"end",.5f}};
    check(duration(play(trim,asset,{48,1,true,true},10000))==1201,"Trim sets playback region");
    trim.nodes[0].parameters={{"start",1},{"end",0}};check(duration(play(trim,asset,{48,1,true,true},10000))==1,"Crossed trims bounded to one frame");
    auto loop=g;loop.nodes[0].parameters={{"loop",1}};Runtime r(loop,bank);std::vector<float> x(12000);
    r.render(x.data(),12000,{48,1,true,true});check(x.back()>0,"Held loop repeats beyond source length");
    r.render(x.data(),12000,{48,1,false,false});check(duration(x)<=240&&energy(x)>0,"Loop releases within five milliseconds");
    Runtime one(g,bank);one.render(x.data(),24,{48,1,true,true});one.render(x.data(),6000,{48,1,false,false});x.resize(6000);check(duration(x)==4776,"One shot finishes after note release");
    Runtime retrigger(g,bank);std::vector<float> a(300),b(300);retrigger.render(a.data(),300,{48,.8f,true,true});retrigger.render(b.data(),300,{48,.8f,true,true});check(a==b,"Retrigger restarts same sample");
    Runtime silent(g);silent.render(a.data(),300,{48,1,true,true});check(energy(a)==0,"Unloaded player is silent");
    auto twin=g;twin.nodes.push_back({"second","sample.player",{{"transpose",12}}});twin.outputs.push_back({"second","out"});Runtime pair(twin,{{"source",asset},{"second",asset}});
    std::vector<float> combined(6000);pair.render(combined.data(),6000,{48,1,true,true});auto lower=play(g,asset,{48,1,true,true},6000),higher=play(g,asset,{60,1,true,true},6000);
    bool independent=true;for(size_t i=0;i<combined.size();++i)independent&=combined[i]==lower[i]+higher[i];check(independent,"Shared sample asset has independent playheads");
    allocations=0;watchAllocations=true;for(int i=0;i<100;++i)r.render(a.data(),300,{i%128,1,true,i%3==0});watchAllocations=false;check(allocations==0,"Sample loop/render/retrigger allocates nothing");
    for(int i:{4,5}){auto graph=rackPreset(i);validateRack(graph);SampleBank assets{{"source",demoSample()}};Runtime first(graph,assets),second(graph,assets);std::vector<float> y(48000),z(48000);first.render(y.data(),48000,{48,.8f,true,true});second.render(z.data(),48000,{48,.8f,true,true});check(energy(y)>1e-6&&y==z,"Sample/resonator starter audible and deterministic");wav("build/rack-"+std::to_string(i)+".wav",y);}
    asset->rate=std::numeric_limits<double>::infinity();bool invalid=false;try{Runtime unsafe(g,bank);}catch(const std::exception&){invalid=true;}check(invalid,"Invalid prepared sample rejected");
}
}
