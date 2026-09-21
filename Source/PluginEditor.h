#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

// Simple functional editor: a master control and one column of controls per Element.
// While an Element's Link is on, its envelope controls (Attack to Time) are greyed out
// because Element 1's settings are used instead. The designed GUI comes later.
class TeratoamorAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // A label, a slider, and the attachment that keeps the slider tied to a host parameter.
    struct SliderRow
    {
        SliderRow (juce::AudioProcessorValueTreeState&, const juce::String& parameterID, const juce::String& text);

        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;   // declared last: destroyed first
    };

    struct ElementPanel
    {
        juce::GroupComponent group;
        juce::ToggleButton on { "On" };
        juce::Label filterLabel;
        juce::ComboBox filter;
        std::vector<std::unique_ptr<SliderRow>> tone;       // Octave .. Pan; a null entry is a gap (Element 2 has no Clip)
        juce::Label envelopeHeader;                         // Element 1 only
        juce::ToggleButton link { "Use Element 1 envelope" };   // Elements 2 and 3 only
        std::vector<std::unique_ptr<SliderRow>> envelope;   // Attack, Decay, Sustain, Release, Time
        std::unique_ptr<ButtonAttachment> onAttachment, linkAttachment;
        std::unique_ptr<ComboBoxAttachment> filterAttachment;
        bool linked = false;
    };

    void timerCallback() override;
    void refreshLinkState();

    TeratoamorAudioProcessor& processor;

    juce::Label title;
    std::unique_ptr<SliderRow> master;
    std::array<ElementPanel, ParamIDs::numElements> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeratoamorAudioProcessorEditor)
};
