#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"
namespace teratoamor::ui
{
class EditorContent final : public juce::Component
{
public:
    explicit EditorContent (TeratoamorAudioProcessor&);
    ~EditorContent() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    LookAndFeel lookAndFeel;
    struct Impl;
    std::unique_ptr<Impl> impl;
    juce::TooltipWindow tooltip { this, 600 };
};
}
