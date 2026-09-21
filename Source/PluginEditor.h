#pragma once

#include "PluginProcessor.h"

// Temporary functional GUI: a generic list of every parameter. The designed GUI comes later.
class TeratoamorAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor&);

    void resized() override;

private:
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeratoamorAudioProcessorEditor)
};
