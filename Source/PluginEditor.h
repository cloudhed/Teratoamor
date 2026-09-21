#pragma once

#include "PluginProcessor.h"

class TeratoamorAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor&);
    ~TeratoamorAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TeratoamorAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeratoamorAudioProcessorEditor)
};
