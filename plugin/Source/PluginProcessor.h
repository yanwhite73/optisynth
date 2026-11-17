// partials as an AU / Standalone instrument (JUCE).
//
// Real-time rules (the host must never be put at risk):
//  - processBlock never allocates, never waits on a lock and never touches files. It takes the
//    engine with a try-lock; if a preset is being swapped in at that moment it outputs silence.
//  - Presets are read and their tables built on a background thread, then swapped in under a
//    short lock. The old engine is destroyed on that background thread, not the audio thread.
//  - Any failure to read a preset is reported in the status line and the old sound stays.
#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>

#include "../../src/core/engine.h"
#include "../../src/core/patch.h"

class PartialsProcessor : public juce::AudioProcessor, private juce::MidiKeyboardStateListener,
                          private juce::AudioProcessorValueTreeState::Listener {
public:
    PartialsProcessor();
    ~PartialsProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Optisynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void* data, int size) override;

    // Message thread.
    void loadPresetFile(const juce::File& file);
    juce::File getPresetFile() const;
    juce::File getPresetFolder() const;
    void setPresetFolder(const juce::File& folder);
    juce::String getStatus() const;
    void refreshKeyboardDisplay();  // message thread only; audio publishes atomic key states
    bool isLoading() const { return loading_.load() > 0; }
    partials::Patch editablePatch() const;
    void applyEditedPatch(partials::Patch patch);
    void restoreLoadedPatch();
    bool saveEditedPatch(const juce::File& file);
    unsigned patchRevision() const { return patchRevision_.load(); }
    bool isEdited() const { return edited_.load(); }
    partials::LiveValues hostLiveValues() const;

    juce::MidiKeyboardState keyboardState;
    juce::AudioProcessorValueTreeState params;

private:
    void startLoad(const juce::File& file);
    void enqueuePatch(partials::Patch patch, int generation, bool preserve, bool measure);
    void buildPatch(partials::Patch patch, int generation, bool preserve, bool measure);
    void setStatus(const juce::String& s);
    void handleMidi(const juce::MidiMessage& m, partials::Engine& engine);
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void queueKeyboard(int note, float velocity);
    void consumeKeyboard(partials::Engine& engine);
    void publishLiveParameters(const partials::Patch& patch,bool preserveInactive=false);
    void parameterChanged(const juce::String&,float) override;

    // One producer (the UI keyboard/message thread), one consumer (audio). Fixed storage;
    // overflow requests an audio-thread reset so a dropped note-off cannot leave a stuck note.
    struct KeyboardEvent { int note = 0; float velocity = 0; };
    static constexpr unsigned kKeyboardQueueSize = 512;
    std::array<KeyboardEvent, kKeyboardQueueSize> keyboardEvents_{};
    std::atomic<unsigned> keyboardWrite_{0}, keyboardRead_{0};
    std::atomic<bool> keyboardOverflow_{false};
    std::array<std::atomic<bool>, 128> displayKeys_;
    std::array<bool, 128> displayedKeys_{};  // message thread only

    juce::SpinLock engineLock_;                  // guards engine_ between the audio and loader threads
    std::unique_ptr<partials::Engine> engine_;
    std::atomic<float> trimDb_{0.0f};             // per-preset level trim from the auto-level measurement
    std::atomic<double> sampleRate_{48000.0};
    std::atomic<int> generation_{0};              // newest load request; older results are discarded
    std::atomic<int> loading_{0};

    juce::CriticalSection fileLock_;              // presetFile_, presetFolder_, status_ (never used on the audio thread)
    juce::File presetFile_, presetFolder_;
    juce::String status_{"No preset loaded: playing a sine."};
    partials::Patch editablePatch_ = partials::defaultPatch(), loadedPatch_ = partials::defaultPatch();
    std::atomic<unsigned> patchRevision_{1};
    std::atomic<bool> edited_{false};

    std::atomic<float>* outputDb_ = nullptr;
    std::atomic<float>* autoLevel_ = nullptr;
    std::array<std::atomic<float>*,partials::kLiveControls> liveParameters_{};
    std::atomic<bool> publishingLiveParameters_{false};
    juce::SmoothedValue<float> gain_;
    bool sustain_ = false;
    std::array<bool, 128> sustained_{};

    juce::ThreadPool loader_{1};                  // declared last: destroyed first
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PartialsProcessor)
};
