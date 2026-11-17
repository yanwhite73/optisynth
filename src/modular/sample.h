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
struct SampleData {
    double rate=48000;
    int sourceChannels=1;
    std::string fingerprint;
    std::vector<float> mono;
    std::array<float,128> peaks{};
};
using SamplePtr=std::shared_ptr<const SampleData>;
using SampleBank=std::map<std::string,SamplePtr>;
using CancelSample=std::function<bool()>;
// Preparation only. WAV bytes and decoded samples are bounded; never call in render.
SamplePtr decodeWave(const std::vector<uint8_t>&,const CancelSample& cancelled={});
SamplePtr loadWave(const std::string& path,const CancelSample& cancelled={});
SamplePtr demoSample();
}
