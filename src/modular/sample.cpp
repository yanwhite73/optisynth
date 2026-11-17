#include "sample.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace optisynth {
namespace {
void need(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
void cancel(const CancelSample& f){if(f&&f())throw std::runtime_error("Sample load cancelled");}
uint16_t u16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
void preview(SampleData& s){for(size_t i=0;i<s.mono.size();++i){auto bin=std::min(size_t(127),i*128/s.mono.size());s.peaks[bin]=std::max(s.peaks[bin],std::abs(s.mono[i]));}}
}
SamplePtr decodeWave(const std::vector<uint8_t>& bytes,const CancelSample& cancelled){
    need(bytes.size()>=12&&bytes.size()<=maximumSampleBytes,"WAV must be between 12 bytes and 32 MiB");
    const auto* b=bytes.data();need(!std::memcmp(b,"RIFF",4)&&!std::memcmp(b+8,"WAVE",4),"Expected little-endian RIFF WAV");
    const uint64_t end=uint64_t(u32(b+4))+8;need(end>=12&&end<=bytes.size(),"Truncated RIFF chunk");
    const uint8_t *fmt=nullptr,*data=nullptr;size_t fmtSize=0,dataSize=0;
    for(uint64_t p=12;p<end;){
        cancel(cancelled);need(end-p>=8,"Truncated WAV chunk header");const auto size=u32(b+p+4);const uint64_t next=p+8+size+(size&1);
        need(next<=end,"Truncated WAV chunk");
        if(!std::memcmp(b+p,"fmt ",4)){need(!fmt,"Duplicate WAV format chunk");fmt=b+p+8;fmtSize=size;}
        if(!std::memcmp(b+p,"data",4)){need(!data,"Duplicate WAV audio chunk");data=b+p+8;dataSize=size;}
        p=next;
    }
    need(fmt&&fmtSize>=16&&data&&dataSize,"WAV needs format and nonempty audio chunks");
    int tag=u16(fmt),channels=u16(fmt+2),bits=u16(fmt+14),valid=bits;const uint32_t rate=u32(fmt+4);const auto align=u16(fmt+12);
    if(tag==0xfffe){
        need(fmtSize>=40&&u16(fmt+16)>=22&&size_t(u16(fmt+16))+18<=fmtSize,"Invalid extensible WAV header");
        const uint8_t tail[]={0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};
        need(!std::memcmp(fmt+28,tail,12),"Unsupported WAV subformat");tag=int(u32(fmt+24));valid=u16(fmt+18);
    }
    need(channels==1||channels==2,"Use a mono or stereo WAV");need(rate>=8000&&rate<=384000,"WAV sample rate must be 8–384 kHz");
    need((tag==1&&(bits==8||bits==16||bits==24||bits==32))||(tag==3&&(bits==32||bits==64)),"Use PCM or floating-point WAV");
    need(valid>0&&valid<=bits&&(tag!=3||valid==bits),"Invalid WAV valid-bit count");
    need(align==channels*bits/8&&dataSize%align==0,"Invalid WAV frame alignment");
    need(u32(fmt+8)==uint64_t(rate)*align,"Invalid WAV byte rate");
    const auto frames=dataSize/align;need(frames<=maximumSampleFrames,"Decoded sample exceeds 32 MiB");
    auto s=std::make_shared<SampleData>();s->rate=rate;s->sourceChannels=channels;s->mono.resize(frames);
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<bytes.size();++i){if((i&65535)==0)cancel(cancelled);hash=(hash^bytes[i])*1099511628211ull;}
    std::ostringstream hex;hex<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;s->fingerprint=hex.str();
    for(size_t i=0;i<frames;++i){
        if((i&4095)==0)cancel(cancelled);double sum=0;
        for(int channel=0;channel<channels;++channel){const auto* p=data+i*align+channel*bits/8;double value=0;
            if(tag==3){if(bits==32){auto word=u32(p);float f;std::memcpy(&f,&word,4);value=f;}else{uint64_t word=uint64_t(u32(p))|(uint64_t(u32(p+4))<<32);std::memcpy(&value,&word,8);}}
            else if(bits==8)value=(int(p[0])-128)/128.;
            else{uint32_t word=0;for(int byte=0;byte<bits/8;++byte)word|=uint32_t(p[byte])<<(8*byte);
                const int64_t signedValue=(word&(uint32_t(1)<<(bits-1)))?int64_t(word)-(int64_t(1)<<bits):word;
                value=double(signedValue)/double(int64_t(1)<<(bits-1));}
            need(std::isfinite(value)&&std::abs(value)<=16,"WAV contains nonfinite or extreme sample values");sum+=value;
        }
        s->mono[i]=float(sum/channels);
    }
    preview(*s);return s;
}
SamplePtr loadWave(const std::string& path,const CancelSample& cancelled){
    cancel(cancelled);std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Cannot open sample: "+path);
    const auto size=file.tellg();need(size>=12&&size<=std::streamoff(maximumSampleBytes),"WAV exceeds 32 MiB or is empty");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));file.seekg(0);
    for(size_t p=0;p<bytes.size();){cancel(cancelled);auto n=std::min(size_t(65536),bytes.size()-p);file.read(reinterpret_cast<char*>(bytes.data()+p),std::streamsize(n));need(bool(file),"Could not read complete WAV");p+=n;}
    return decodeWave(bytes,cancelled);
}
SamplePtr demoSample(){
    auto s=std::make_shared<SampleData>();s->fingerprint="builtin-click-v1";s->mono.resize(4800);uint32_t noise=17;
    for(size_t i=0;i<s->mono.size();++i){noise=noise*1664525u+1013904223u;const double t=i/48000.;
        s->mono[i]=float((.5*std::sin(6.283185307179586*(1800*t-3500*t*t))+.3*(double(noise)/4294967295.*2-1))*std::exp(-t*95));}
    preview(*s);return s;
}
}
