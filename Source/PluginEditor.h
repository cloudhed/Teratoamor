#pragma once
#include "PluginProcessor.h"
namespace teratoamor::ui { class EditorContent; }

class TeratoamorAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor&);
    ~TeratoamorAudioProcessorEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    std::unique_ptr<teratoamor::ui::EditorContent> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeratoamorAudioProcessorEditor)
};
