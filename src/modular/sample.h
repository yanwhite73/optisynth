#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace optisynth {
constexpr std::size_t maximumSampleBytes=32*1024*1024;
constexpr std::size_t maximumSampleFrames=8*1024*1024;
constexpr std::size_t maximumBankBytes=128*1024*1024;
struct StrikeProfile {
    std::string role="Unclear";
    std::string character;
    float attackMs=0, durationMs=0, lowFraction=0, brightFraction=0, bodyHz=0;
};
struct SampleData {
    double rate=48000;
    int sourceChannels=1;
    std::string fingerprint;
    std::vector<float> mono;
    std::array<float,128> peaks{};
    size_t strikeStart=0;float strikePeak=1;
    StrikeProfile strike;
};
using SamplePtr=std::shared_ptr<const SampleData>;
using SampleBank=std::map<std::string,SamplePtr>;
using CancelSample=std::function<bool()>;
StrikeProfile describeStrike(const std::vector<float>& mono,double rate,const CancelSample& cancelled={});
// Preparation only. WAV bytes and decoded samples are bounded; never call in render.
SamplePtr decodeWave(const std::vector<uint8_t>&,const CancelSample& cancelled={});
SamplePtr loadWave(const std::string& path,const CancelSample& cancelled={});
SamplePtr demoSample();
}
