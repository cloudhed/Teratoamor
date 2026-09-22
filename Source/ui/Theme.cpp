#include "Theme.h"
#include "DraggableValueLabel.h"

namespace teratoamor::ui
{
namespace
{
// Nested translucent strokes approximate a soft halo without offscreen images or
// a per-repaint blur. Draw the crisp source shape afterwards. Shared tuning lives
// in Theme so a stronger glow uses exactly the same drawing path.
void drawGlow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
{
    for (int layer = Theme::glowLayers; layer > 0; --layer)
    {
        const float extent = float (layer) / float (Theme::glowLayers);
        g.setColour (colour.withAlpha (Theme::glowOpacity * (1.0f - 0.75f * extent)));
        g.strokePath (path, juce::PathStrokeType (width + 2.0f * Theme::glowSpread * extent,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
}
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    // Reuse JUCE's standard colours and typography, including editable-text colours.
    std::unique_ptr<juce::Label> standard (juce::LookAndFeel_V4::createSliderTextBox (slider));
    auto* label = new DraggableValueLabel (slider);
    standard->copyAllExplicitColoursTo (*label);
    label->setFont (standard->getFont());
    label->setBorderSize (standard->getBorderSize());
    label->setJustificationType (juce::Justification::centred);
    label->setKeyboardType (juce::TextInputTarget::decimalKeyboard);
    label->setTooltip ("Drag up/down to adjust. Shift-drag for fine adjustment. Click to type; double-click to reset.");
    return label;
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::Label::textColourId, Theme::text);
    setColour (juce::Slider::textBoxTextColourId, Theme::blush);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Theme::background);
    setColour (juce::ComboBox::outlineColourId, Theme::border);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::blush);
    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::deep);
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::blush);
    setColour (juce::TextButton::textColourOffId, Theme::mauve);
    setColour (juce::TextButton::textColourOnId, Theme::text);
    setColour (juce::TooltipWindow::backgroundColourId, Theme::panel);
    setColour (juce::TooltipWindow::textColourId, Theme::text);
    setColour (juce::TooltipWindow::outlineColourId, Theme::border);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float position,
                                    float start, float end, juce::Slider& slider)
{
    const auto r = juce::Rectangle<float> (float (x), float (y), float (w), float (h)).reduced (7);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const auto angle = start + position * (end - start);
    const float stroke = radius > 65 ? 6.0f : 4.0f;
    juce::Path track, active;
    track.addCentredArc (c.x, c.y, radius, radius, 0, start, end, true);
    g.setColour (Theme::border.withAlpha (0.65f));
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto zero = slider.getMinimum() < 0 && slider.getMaximum() > 0
        ? start + float (slider.valueToProportionOfLength (0)) * (end - start) : start;
    active.addCentredArc (c.x, c.y, radius, radius, 0, juce::jmin (zero, angle), juce::jmax (zero, angle), true);
    if (slider.isEnabled()) drawGlow (g, active, Theme::coral, stroke);
    g.setColour (slider.isEnabled() ? Theme::coral : Theme::mauve);
    g.strokePath (active, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto body = juce::Rectangle<float> (radius * 1.76f, radius * 1.76f).withCentre (c);
    g.setGradientFill (juce::ColourGradient (Theme::deep.brighter (0.05f), body.getTopLeft(),
                                           Theme::background, body.getBottomRight(), false));
    g.fillEllipse (body);
    g.setColour (slider.hasKeyboardFocus (true) ? Theme::blush :
                 slider.isMouseOverOrDragging() ? Theme::mauve : Theme::border);
    g.drawEllipse (body, Theme::borderWidth);
    const auto from = c.getPointOnCircumference (radius * 0.58f, angle);
    const auto to = c.getPointOnCircumference (radius * 0.78f, angle);
    g.setColour (Theme::text);
    g.drawLine ({ from, to }, 2.0f);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // A compact "numbers only" field (Octave/Semitone/Detune): still a draggable slider,
    // but only JUCE's own text box is shown, no track or thumb graphic.
    if (slider.getProperties().contains ("hideTrack")) return;
    const bool vertical = style == juce::Slider::LinearVertical;
    const float cx = float (x) + float (w) * 0.5f, cy = float (y) + float (h) * 0.5f;
    g.setColour (Theme::border);
    if (vertical) g.fillRoundedRectangle (cx - 2, float (y), 4, float (h), 2);
    else g.fillRoundedRectangle (float (x), cy - 2, float (w), 4, 2);
    juce::Path thumb;
    thumb.addEllipse ((vertical ? cx : pos) - 4, (vertical ? pos : cy) - 4, 8, 8);
    if (slider.isEnabled()) drawGlow (g, thumb, Theme::coral, 2.0f);
    g.setColour (slider.isEnabled() ? Theme::coral : Theme::mauve);
    g.fillEllipse ((vertical ? cx : pos) - 5, (vertical ? pos : cy) - 5, 10, 10);
    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (Theme::blush);
        g.drawRoundedRectangle (slider.getLocalBounds().toFloat().reduced (1), 4, Theme::accentBorderWidth);
    }
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    // Reserve space inside the component clip for the selected outline's halo.
    auto r = b.getLocalBounds().toFloat().reduced (Theme::glowSpread + 1.0f);
    g.setColour (b.getToggleState() ? Theme::deep : over ? Theme::panel.brighter (0.1f) : Theme::panel);
    g.fillRoundedRectangle (r, 9);
    if (b.getToggleState() && b.isEnabled())
    {
        juce::Path outline;
        outline.addRoundedRectangle (r, 9);
        drawGlow (g, outline, Theme::coral, Theme::accentBorderWidth);
    }
    g.setColour (b.hasKeyboardFocus (true) ? Theme::blush : b.getToggleState() ? Theme::coral : Theme::border);
    g.drawRoundedRectangle (r, 9, b.getToggleState() ? Theme::accentBorderWidth : Theme::borderWidth);
    if (b.getToggleState()) g.fillRoundedRectangle (r.getX() + 14, r.getBottom() - 5, r.getWidth() - 28, 3, 1);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto y = float (b.getHeight()) * 0.5f;
    g.setColour (b.getToggleState() ? Theme::warmOutline : Theme::border);
    g.drawRoundedRectangle (2, y - 9, 32, 18, 9, Theme::accentBorderWidth);
    if (b.getToggleState() && b.isEnabled())
    {
        juce::Path light;
        light.addEllipse (21.0f, y - 4, 8, 8);
        drawGlow (g, light, Theme::coral, 2.0f);
    }
    g.setColour (b.getToggleState() ? Theme::coral : Theme::mauve);
    g.fillEllipse (b.getToggleState() ? 20.0f : 6.0f, y - 5, 10, 10);
    g.setColour (over ? Theme::blush : Theme::text);
    g.setFont (juce::FontOptions (13));
    g.drawText (b.getButtonText(), 42, 0, b.getWidth() - 44, b.getHeight(), juce::Justification::centredLeft);
    if (b.hasKeyboardFocus (true)) g.drawRoundedRectangle (b.getLocalBounds().toFloat().reduced (1), 5, Theme::accentBorderWidth);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                               int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (float (width), float (height)).reduced (1);
    g.setColour (Theme::background);
    g.fillRoundedRectangle (r, 5);
    g.setColour (box.hasKeyboardFocus (true) ? Theme::coral : box.isMouseOver() ? Theme::mauve : Theme::border);
    g.drawRoundedRectangle (r, 5, Theme::borderWidth);
    const float x = float (width) - 18, y = float (height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (x - 5, y - 2);
    arrow.lineTo (x, y + 2);
    arrow.lineTo (x + 5, y - 2);
    g.setColour (Theme::coral);
    g.strokePath (arrow, juce::PathStrokeType (Theme::accentBorderWidth));
}
}
