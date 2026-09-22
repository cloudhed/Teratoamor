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
    inline const juce::Colour lightCore { 0xffffc6b9 }, knobTop { 0xff302936 }, knobBottom { 0xff131219 };
    // Outside to inside: two orange layers, then six steps towards red-orange.
    inline const juce::Colour glowColours[] {
        juce::Colour { 0xffff965b }, juce::Colour { 0xffff965b },
        juce::Colour { 0xffff915b }, juce::Colour { 0xffff8d5b },
        juce::Colour { 0xffff885b }, juce::Colour { 0xffff835b },
        juce::Colour { 0xffff7f5b }, juce::Colour { 0xffff7a5b }
    };
    constexpr float borderWidth = 1.6f, accentBorderWidth = 2.0f;
    // Logical pixels; the editor transform scales the halo along with the controls.
    constexpr float glowSpread = 6.0f, glowOpacity = 0.055f;
    constexpr int glowLayers = 8;
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
    constexpr float modeFraction = 0.42f;   // Element panel: filter mode dropdown's share of the top row
    // Element composition in design coordinates; keep the preview square-ish and
    // control sizes independent instead of stretching them to fill spare space.
    constexpr int elementWidthKnobWidth = 120, elementWidthKnobHeight = 164;
    constexpr int elementShapeWidth = 84, elementShapeHeight = 110, elementShapeGap = 24;
    constexpr int elementEnvelopeHeight = 180, elementGraphWidth = 78, elementGraphHeight = 80;
    constexpr int elementOutputWidth = 86, elementOutputHeight = 120, elementOutputGap = 8;
    constexpr int workspaceTabWidth = 220, modDepthWidth = 90, modGraphWidth = 100;
    constexpr float modSelectorsFraction = 0.52f, delayMixFraction = 0.23f, reverbFraction = 0.32f;
}
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
};
}
