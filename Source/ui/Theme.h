#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace teratoamor::ui
{
namespace Theme
{
    inline const juce::Colour background { 0xff0e0f14 }, panel { 0xff1b1921 }, deep { 0xff44344e };
    inline const juce::Colour border { 0xff6c5076 }, violet { 0xff856489 }, mauve { 0xffae85a2 };
    inline const juce::Colour blush { 0xffe4a9a9 }, coral { 0xfff5a399 }, text { 0xffede3eb };
    // Brighter peach-coral follows the supplied swatch; retain the warm toggle rim.
    inline const juce::Colour warmOutline { 0xffd67a7b };
    constexpr float borderWidth = 1.6f, accentBorderWidth = 2.0f;
}
// Design coordinates: the editor scales this canvas as a whole. Panels lay out
// their own controls from bounds, so these tokens can change independently.
namespace Layout
{
    constexpr int width = 1600, height = 1000;
    constexpr int defaultWidth = 1440, minimumWidth = 1200, maximumWidth = 2400;
    constexpr int margin = 20, gap = 14, padding = 22;
    constexpr int header = 100, elements = 510, ribbon = 30, tabs = 48, footer = 24;
    constexpr float radius = 22.0f;
    constexpr float pitchFraction = 0.23f, widthFraction = 0.43f, envelopeFraction = 0.60f;
    constexpr float soundHeightFraction = 0.60f;
    constexpr int workspaceTabWidth = 220, modDepthWidth = 90, modGraphWidth = 100;
    constexpr float modSelectorsFraction = 0.52f, delayMixFraction = 0.23f, reverbFraction = 0.32f;
}
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
};
}
