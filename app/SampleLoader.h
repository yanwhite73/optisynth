#pragma once
#include "PatchIO.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace optisynth {
// One worker and one replaceable request/result. No reference to the UI or player.
class SampleLoader {
public:
    struct Result { RackDocument document;std::string preferred,error;std::unique_ptr<Runtime> runtime;uint64_t generation=0; };
    SampleLoader():worker([this]{run();}){}
    ~SampleLoader(){stop.store(true);cancel();wake.notify_one();worker.join();}
    void cancel(){++generation;std::lock_guard<std::mutex> lock(mutex);queued.reset();ready.reset();}
    void submit(RackDocument d,std::string preferred){
        cancel();std::lock_guard<std::mutex> lock(mutex);
        queued=Result{std::move(d),std::move(preferred),{},{},generation.load()};wake.notify_one();
    }
    std::optional<Result> take(){
        std::lock_guard<std::mutex> lock(mutex);
        if(!ready||ready->generation!=generation.load())return {};
        auto r=std::move(ready);ready.reset();return r;
    }
private:
    std::atomic<uint64_t> generation{0};std::atomic<bool> stop{false};
    std::mutex mutex;std::condition_variable wake;std::optional<Result> queued,ready;
    std::thread worker;
    void run(){while(!stop.load()){
        std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[this]{return stop.load()||queued.has_value();});
        if(stop.load())return;auto job=std::move(*queued);queued.reset();lock.unlock();
        auto cancelled=[this,g=job.generation]{return stop.load()||generation.load()!=g;};
        try{prepareSamples(job.document,cancelled);if(!cancelled())job.runtime=std::make_unique<Runtime>(job.document.graph,sampleBank(job.document));}
        catch(const std::exception& e){job.error=e.what();}
        lock.lock();if(!cancelled())ready=std::move(job);
    }}
};
}
