// Spectral-layout editor for our independent synth engine.
#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

class PartialsEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PartialsEditor(PartialsProcessor&);
    ~PartialsEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;

private:
    void timerCallback() override;
    class Impl;
    std::unique_ptr<Impl> ui_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PartialsEditor)
};
