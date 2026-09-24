#include "EditorContent.h"
#include <TeratoamorAssets.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>

namespace teratoamor::ui
{
namespace
{
using State = juce::AudioProcessorValueTreeState;

void caption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> bounds,
              float size = 14, juce::Colour colour = Theme::mauve,
              juce::Justification alignment = juce::Justification::centredLeft)
{
    g.setColour (colour);
    g.setFont (juce::FontOptions (size));
    g.drawText (text, bounds, alignment);
}

// Parameter definitions supply ranges, steps, skew, defaults and text conversion.
class ParameterControl final : public juce::Component
{
public:
    ParameterControl (State& state, const juce::String& id, const juce::String& name,
                      bool vertical = false, bool showLabel = true) : displayLabel (showLabel)
    {
        auto* p = state.getParameter (id);
        jassert (p != nullptr);
        setComponentID (id);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (13));
        slider.setName (p->getName (100));
        slider.setComponentID (id);
        slider.setTooltip (p->getName (100) + ". Shift-drag for fine adjustment; double-click to reset.");
        slider.setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
        // JUCE's value label captures colours when it is constructed, before this
        // control joins the themed parent. Set these on the slider explicitly.
        slider.setColour (juce::Slider::textBoxTextColourId, Theme::blush);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setScrollWheelEnabled (false);
        slider.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider.setWantsKeyboardFocus (true);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        if (displayLabel) addAndMakeVisible (label);
        addAndMakeVisible (slider);
        attachment = std::make_unique<State::SliderAttachment> (state, id, slider);
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        // Envelope times get a slider-only skew so short times have more travel. The
        // parameter (host automation, saved state) stays linear. Set after the
        // attachment, which copies the parameter's own range onto the slider.
        const bool envelopeTime = ! id.startsWith ("delay")
                                  && (id.endsWith ("_attack") || id.endsWith ("_decay") || id.endsWith ("_release"));
        if (envelopeTime)
        {
            slider.setSkewFactorFromMidPoint (5.0);
            slider.setSliderSnapsToMousePosition (false); // dragging starts from the current value, no jump on click
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        if (displayLabel) label.setBounds (r.removeFromBottom (22));
        slider.setBounds (r);
    }
    void available (bool enabled, const juce::String& reason = {})
    {
        slider.setEnabled (enabled);
        setAlpha (enabled ? 1.0f : 0.55f);
        if (reason.isNotEmpty()) slider.setTooltip (slider.getName() + ". " + reason);
    }
    juce::Slider slider;
private:
    bool displayLabel;
    juce::Label label;
    std::unique_ptr<State::SliderAttachment> attachment;
};

// A number-only control (Octave, Semitone, Detune): a draggable value with a small
// caption, no rotary or track graphic, for rows too short for a full knob.
class CompactNumber final : public juce::Component
{
public:
    CompactNumber (State& state, const juce::String& id, const juce::String& name)
    {
        auto* p = state.getParameter (id);
        jassert (p != nullptr);
        setComponentID (id);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (11));
        label.setColour (juce::Label::textColourId, Theme::mauve);
        slider.setName (p->getName (100));
        slider.setComponentID (id);
        slider.setTooltip (p->getName (100) + ". Drag to change; shift-drag for fine adjustment; double-click to reset.");
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.getProperties().set ("hideTrack", true);
        slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 74, 20);
        slider.setColour (juce::Slider::textBoxTextColourId, Theme::text);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setScrollWheelEnabled (false);
        slider.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider.setWantsKeyboardFocus (true);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        attachment = std::make_unique<State::SliderAttachment> (state, id, slider);
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }
    void resized() override
    {
        // Caption on top, value below: matches the Filter Mode dropdown this shares a row with.
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }
    juce::Slider slider;
private:
    juce::Label label;
    std::unique_ptr<State::SliderAttachment> attachment;
};

class Choice final : public juce::Component
{
public:
    Choice (State& state, const juce::String& id, const juce::String& name)
    {
        auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id));
        jassert (p != nullptr);
        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::FontOptions (12));
        label.setColour (juce::Label::textColourId, Theme::mauve);
        box.setName (p->getName (100));
        box.setComponentID (id);
        box.addItemList (p->choices, 1);
        box.setScrollWheelEnabled (false);
        addAndMakeVisible (label);
        addAndMakeVisible (box);
        attachment = std::make_unique<State::ComboBoxAttachment> (state, id, box);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (20));
        box.setBounds (r.removeFromTop (32));
    }
    juce::ComboBox box;
private:
    juce::Label label;
    std::unique_ptr<State::ComboBoxAttachment> attachment;
};

// The host's target choice indices are already in saved projects. Give the
// dropdown a friendlier order while its item IDs retain those original indices.
class ModTargetChoice final : public juce::Component
{
public:
    ModTargetChoice (State& state, int section)
        : parameter (*state.getParameter (ParamIDs::mod (section, "target"))),
          attachment (parameter, [this] (float value)
          {
              box.setSelectedId (juce::roundToInt (value) + 1, juce::dontSendNotification);
          }, state.undoManager)
    {
        label.setText ("DESTINATION", juce::dontSendNotification);
        label.setFont (juce::FontOptions (12));
        label.setColour (juce::Label::textColourId, Theme::mauve);
        box.setName (parameter.getName (100));
        box.setComponentID (ParamIDs::mod (section, "target"));
        box.setScrollWheelEnabled (false);

        const auto targets = ModTargets::listFor (section);
        for (int i = 0; i < targets.size; ++i)
        {
            const auto target = targets.items[(size_t) i];
            if (target == ModTarget::el1Width || target == ModTarget::el2Width || target == ModTarget::el3Width)
            {
                const auto volume = target == ModTarget::el1Width ? ModTarget::el1Volume
                                  : target == ModTarget::el2Width ? ModTarget::el2Volume : ModTarget::el3Volume;
                for (int j = 0; j < targets.size; ++j)
                    if (targets.items[(size_t) j] == volume)
                        box.addItem (ModTargets::names[(size_t) volume], j + 1);
            }
            if (target == ModTarget::el1Volume || target == ModTarget::el2Volume || target == ModTarget::el3Volume)
                continue;
            box.addItem (ModTargets::names[(size_t) target], i + 1);
        }

        addAndMakeVisible (label);
        addAndMakeVisible (box);
        box.onChange = [this]
        {
            if (box.getSelectedId() > 0)
                attachment.setValueAsCompleteGesture (float (box.getSelectedId() - 1));
        };
        attachment.sendInitialUpdate();
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (20));
        box.setBounds (r.removeFromTop (32));
    }
    juce::ComboBox box;
private:
    juce::RangedAudioParameter& parameter;
    juce::Label label;
    juce::ParameterAttachment attachment;
};

class Toggle final : public juce::ToggleButton
{
public:
    Toggle (State& state, const juce::String& id, const juce::String& name) : juce::ToggleButton (name)
    {
        setName (state.getParameter (id)->getName (100));
        setComponentID (id);
        attachment = std::make_unique<State::ButtonAttachment> (state, id, *this);
    }
private:
    std::unique_ptr<State::ButtonAttachment> attachment;
};

class Panel : public juce::Component
{
public:
    explicit Panel (juce::String name) : title (std::move (name)) {}
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1);
        g.setGradientFill (juce::ColourGradient (Theme::deep.withMultipliedBrightness (0.55f), r.getTopLeft(),
                                               Theme::panel, r.getBottomRight(), false));
        g.fillRoundedRectangle (r, Layout::radius);
        // A restrained petal-like fold echoes the logo without baking the panel into a bitmap.
        juce::Path fold;
        fold.startNewSubPath (r.getX() + Layout::radius, r.getY() + 1);
        fold.cubicTo (r.getWidth() * 0.4f, 5, r.getWidth() * 0.55f, 100, r.getRight() - 2, 66);
        fold.lineTo (r.getRight() - 2, 30);
        fold.quadraticTo (r.getRight() - 2, 2, r.getRight() - 30, 2);
        fold.closeSubPath();
        g.setColour (Theme::violet.withAlpha (0.10f));
        g.fillPath (fold);
        g.setColour (Theme::border.withAlpha (0.7f));
        g.drawRoundedRectangle (r, Layout::radius, Theme::borderWidth);
        caption (g, title, { Layout::padding, 17, getWidth() - 2 * Layout::padding, 24 }, 17, Theme::text);
    }
    juce::Rectangle<int> body() const { return getLocalBounds().reduced (Layout::padding).withTrimmedTop (34); }
private:
    juce::String title;
};

class EnvelopeDisplay final : public juce::Component, private juce::Timer
{
public:
    EnvelopeDisplay (State& s, int index, bool mod = false) : state (s), element (index), modulation (mod)
    {
        setTooltipText();
        startTimerHz (15);
        timerCallback();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (Theme::background.withAlpha (0.7f));
        g.fillRoundedRectangle (r, 8);
        r.reduce (9, 12);
        // A normalised shape preview, not a running voice or an absolute time axis.
        // Time scales decay/release only, matching the current engine.
        const auto scale = juce::jmax (0.1f, values[4] / 50.0f);
        const float a = juce::jmax (0.002f, values[0] * 0.1f);
        const float d = juce::jmax (0.002f, values[1] * 0.1f * scale);
        const float rel = juce::jmax (0.002f, values[3] * 0.1f * scale);
        const float sustain = values[2] / 100.0f;
        const float level = std::pow (sustain, 1.95f);
        const float hold = juce::jmax (0.2f, (a + d + rel) * 0.2f);
        const float total = a + d + rel + hold;
        auto point = [&] (float t, float value) { return juce::Point<float> (r.getX() + t / total * r.getWidth(), r.getBottom() - value * r.getHeight()); };
        juce::Path path;
        path.startNewSubPath (point (0, 0));
        for (int i = 1; i <= 24; ++i)
        {
            float t = float (i) / 24;
            path.lineTo (point (a * t, t * t * t));
        }
        for (int i = 1; i <= 24; ++i)
        {
            float t = float (i) / 24;
            path.lineTo (point (a + d * t, std::pow (1 - (1 - sustain) * t, 1.95f)));
        }
        path.lineTo (point (a + d + hold, level));
        path.lineTo (point (total, 0));
        g.setColour (linked ? Theme::mauve : Theme::coral);
        g.strokePath (path, juce::PathStrokeType (Theme::accentBorderWidth));
        path.lineTo (r.getBottomLeft());
        path.closeSubPath();
        g.setColour (Theme::mauve.withAlpha (0.08f));
        g.fillPath (path);
    }
private:
    void setTooltipText() { setTitle ("Envelope shape preview; sustain duration is illustrative"); }
    void timerCallback() override
    {
        if (! isShowing()) return;
        bool nextLinked = ! modulation && element > 0 && state.getRawParameterValue (ParamIDs::link (element))->load() > 0.5f;
        const int source = nextLinked ? 0 : element;
        std::array<float, 5> next {};
        const char* names[] { "attack", "decay", "sustain", "release", "time" };
        for (size_t i = 0; i < next.size(); ++i)
            next[i] = state.getRawParameterValue (modulation ? ParamIDs::mod (source, names[i]) : ParamIDs::element (source, names[i]))->load();
        if (next != values || nextLinked != linked) { values = next; linked = nextLinked; repaint(); }
    }
    State& state;
    int element;
    bool modulation, linked = false;
    std::array<float, 5> values { 0, 0, 100, 5, 50 };
};

class ElementPanel final : public Panel, private juce::Timer
{
public:
    ElementPanel (State& s, int e) : Panel ("ELEMENT " + juce::String (e + 1)), state (s), index (e),
        on (s, ParamIDs::enabled (e), "On"), mode (s, ParamIDs::filter (e), "FILTER MODE"), graph (s, e)
    {
        addAndMakeVisible (on); addAndMakeVisible (mode); addAndMakeVisible (graph);
        octave = std::make_unique<CompactNumber> (s, ParamIDs::element (e, "octave"), "OCT");
        semitone = std::make_unique<CompactNumber> (s, ParamIDs::element (e, "semitone"), "SEMI");
        detune = std::make_unique<CompactNumber> (s, ParamIDs::element (e, "fine"), "DETUNE");
        for (auto* c : { octave.get(), semitone.get(), detune.get() }) addAndMakeVisible (*c);
        // Control order is local to this reusable panel, independent of host parameter order.
        const char* ids[] { "width", "warp", "clip", "pan", "level" };
        const char* labels[] { "WIDTH", "WARP", "CLIP", "PAN", "LEVEL" };
        for (int i = 0; i < 5; ++i)
        {
            if (i == 2 && e == 1) continue; // Element 2 has no Clip parameter
            tone[size_t (i)] = std::make_unique<ParameterControl> (s, ParamIDs::element (e, ids[i]), labels[i]);
            addAndMakeVisible (*tone[size_t (i)]);
        }
        const char* env[] { "attack", "decay", "sustain", "release", "time" };
        const char* names[] { "A", "D", "S", "R", "TIME" };
        for (size_t i = 0; i < envelope.size(); ++i)
        {
            envelope[i] = std::make_unique<ParameterControl> (s, ParamIDs::element (e, env[i]), names[i], true);
            addAndMakeVisible (*envelope[i]);
        }
        if (e > 0)
        {
            link = std::make_unique<Toggle> (s, ParamIDs::link (e), "Link envelope");
            link->setTooltip ("Use Element 1's Attack, Decay, Sustain, Release and Time. Your own settings are retained.");
            addAndMakeVisible (*link);
        }
        timerCallback();
        startTimerHz (15);
    }
    void resized() override
    {
        on.setBounds (getWidth() - 94, 15, 74, 28);
        if (link) link->setBounds (getWidth() - 258, 15, 150, 28);
        auto r = body();

        // Row 1: the filter mode dropdown, shrunk, with Octave/Semitone/Detune as compact
        // draggable numbers sharing its row.
        auto row1 = r.removeFromTop (54);
        mode.setBounds (row1.removeFromLeft (juce::roundToInt (row1.getWidth() * Layout::modeFraction)));
        row1.removeFromLeft (10);
        const int pitchCell = row1.getWidth() / 3;
        octave->setBounds (row1.removeFromLeft (pitchCell).reduced (4, 0));
        semitone->setBounds (row1.removeFromLeft (pitchCell).reduced (4, 0));
        detune->setBounds (row1.reduced (4, 0));
        r.removeFromTop (18);

        // Large Width on the left, with smaller, top-aligned Warp/Clip beside it.
        auto sound = r.withHeight (Layout::elementWidthKnobHeight);
        tone[0]->setBounds (sound.removeFromLeft (Layout::elementWidthKnobWidth));
        sound.removeFromLeft (Layout::elementShapeGap);
        tone[1]->setBounds (sound.removeFromLeft (Layout::elementShapeWidth).withHeight (Layout::elementShapeHeight));
        sound.removeFromLeft (4);
        if (tone[2]) tone[2]->setBounds (sound.removeFromLeft (Layout::elementShapeWidth).withHeight (Layout::elementShapeHeight));

        // The output column begins above the envelope row, as in the mockup.
        auto sideColumn = r.removeFromRight (Layout::elementOutputWidth);
        sideColumn = sideColumn.removeFromBottom (2 * Layout::elementOutputHeight + Layout::elementOutputGap);
        tone[3]->setBounds (sideColumn.removeFromTop (Layout::elementOutputHeight));
        sideColumn.removeFromTop (Layout::elementOutputGap);
        tone[4]->setBounds (sideColumn);
        r.removeFromRight (10);

        // Fixed-height faders and a small preview, with its bottom aligned to the
        // slider tracks (above their values/labels), rather than a stretched plot.
        r = r.removeFromBottom (Layout::elementEnvelopeHeight);
        auto graphArea = r.removeFromRight (Layout::elementGraphWidth + 6).reduced (3, 0);
        graphArea.removeFromBottom (44);
        graph.setBounds (graphArea.removeFromBottom (Layout::elementGraphHeight));
        r.removeFromRight (10);
        const int cell = r.getWidth() / 5;
        for (auto& control : envelope) control->setBounds (r.removeFromLeft (cell));
    }
private:
    void timerCallback() override
    {
        const bool linked = index > 0 && state.getRawParameterValue (ParamIDs::link (index))->load() >= 0.5f;
        for (auto& control : envelope)
            control->available (! linked, linked ? "Linked: this Element uses Element 1's envelope." :
                                "Element envelope. Time scales Decay and Release; 50 is normal speed. Double-click to reset.");
    }
    State& state;
    int index;
    Toggle on;
    Choice mode;
    EnvelopeDisplay graph;
    std::unique_ptr<Toggle> link;
    std::unique_ptr<CompactNumber> octave, semitone, detune;
    std::array<std::unique_ptr<ParameterControl>, 5> tone;   // width, warp, clip (optional), pan, level
    std::array<std::unique_ptr<ParameterControl>, 5> envelope;
};

// Compact processing panels share a row layout. Their contents are specified by IDs,
// so changing the visual order never changes saved state or automation identifiers.
class ControlPanel final : public Panel
{
public:
    ControlPanel (State& s, const juce::String& title) : Panel (title), state (s) {}
    ParameterControl& knob (const juce::String& id, const juce::String& label)
    {
        knobs.push_back (std::make_unique<ParameterControl> (state, id, label));
        addAndMakeVisible (*knobs.back()); return *knobs.back();
    }
    void choice (const juce::String& id, const juce::String& label)
    {
        choices.push_back (std::make_unique<Choice> (state, id, label));
        addAndMakeVisible (*choices.back());
    }
    void toggle (const juce::String& id, const juce::String& label)
    {
        toggles.push_back (std::make_unique<Toggle> (state, id, label));
        addAndMakeVisible (*toggles.back());
    }
    void resized() override
    {
        auto r = body();
        if (choices.size() > 1)
        {
            auto row = r.removeFromTop (60);
            const int cell = row.getWidth() / int (choices.size());
            for (auto& c : choices) c->setBounds (row.removeFromLeft (cell).withTrimmedRight (12));
            auto switches = getLocalBounds().removeFromTop (48).withTrimmedLeft (getWidth() - 160);
            for (auto& t : toggles) t->setBounds (switches.removeFromRight (150).reduced (0, 8));
        }
        else if (! choices.empty() || ! toggles.empty())
        {
            auto selectors = r.removeFromLeft (juce::jmin (220, r.getWidth() / 2));
            r.removeFromLeft (12);
            for (auto& c : choices) c->setBounds (selectors.removeFromTop (60));
            for (auto& t : toggles) t->setBounds (selectors.removeFromTop (30));
        }
        const int cell = knobs.empty() ? 0 : r.getWidth() / int (knobs.size());
        for (auto& k : knobs) k->setBounds (r.removeFromLeft (cell).reduced (3, 0));
    }
    std::vector<std::unique_ptr<ParameterControl>> knobs;
private:
    State& state;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Toggle>> toggles;
};

class GlobalPanel final : public juce::Component, private juce::Timer
{
public:
    explicit GlobalPanel (State& s) : state (s), filter (s, "FILTER"), distortion (s, "DISTORTION"), performance (s, "PERFORMANCE")
    {
        filter.choice (ParamIDs::filterType, "TYPE");
        filter.knob (ParamIDs::filterCutoff, "CUTOFF"); filter.knob (ParamIDs::filterQ, "Q");
        distortion.choice (ParamIDs::distortionType, "TYPE");
        distortion.knob (ParamIDs::distortionCrush, "DRIVE"); distortion.knob (ParamIDs::distortionTone, "TONE");
        performance.choice (ParamIDs::glideMode, "GLIDE MODE");
        performance.knob (ParamIDs::glide, "GLIDE"); performance.knob (ParamIDs::masterPan, "MASTER PAN");
        for (auto* panel : { &filter, &distortion, &performance }) addAndMakeVisible (*panel);
        startTimerHz (10);
        timerCallback();
    }
    void resized() override
    {
        auto r = getLocalBounds(); const int w = (r.getWidth() - 2 * Layout::gap) / 3;
        filter.setBounds (r.removeFromLeft (w)); r.removeFromLeft (Layout::gap);
        distortion.setBounds (r.removeFromLeft (w)); r.removeFromLeft (Layout::gap);
        performance.setBounds (r);
    }
private:
    void timerCallback() override
    {
        for (auto& knob : filter.knobs) knob->setAlpha (state.getRawParameterValue (ParamIDs::filterType)->load() == 0 ? 0.5f : 1.0f);
        for (auto& knob : distortion.knobs) knob->setAlpha (state.getRawParameterValue (ParamIDs::distortionType)->load() == 0 ? 0.5f : 1.0f);
    }
    State& state;
    ControlPanel filter, distortion, performance;
};

// Compact delay rows keep all controls usable while sharing the Space workspace.
class DelayChannelPanel final : public Panel
{
public:
    DelayChannelPanel (State& s, int index)
        : Panel ("DELAY " + juce::String (index + 1)),
          enabled (s, ParamIDs::delayOn (index), "Enabled"),
          rate (s, ParamIDs::delayRate (index), "RATE"),
          decay (s, ParamIDs::delayDecay (index), "DECAY"),
          pan (s, ParamIDs::delayPan (index), "PAN")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &enabled, &rate, &decay, &pan })
            addAndMakeVisible (*c);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (Layout::padding, 8);
        auto heading = r.removeFromLeft (110);
        heading.removeFromTop (46);
        enabled.setBounds (heading.removeFromTop (30));
        const int rateWidth = juce::roundToInt (r.getWidth() * 0.36f);
        rate.setBounds (r.removeFromLeft (rateWidth).withSizeKeepingCentre (rateWidth, 54));
        r.removeFromLeft (12);
        decay.setBounds (r.removeFromLeft (r.getWidth() / 2));
        pan.setBounds (r);
    }
private:
    Toggle enabled;
    Choice rate;
    ParameterControl decay, pan;
};

class ReverbPlaceholder final : public Panel
{
public:
    ReverbPlaceholder() : Panel ("REVERB") {}
    void paint (juce::Graphics& g) override
    {
        Panel::paint (g);
        caption (g, "Reserved for a future update", body(), 14, Theme::mauve, juce::Justification::centred);
    }
};

class SpacePanel final : public juce::Component
{
public:
    explicit SpacePanel (State& s) : shared (s, "DELAY MIX"), first (s, 0), second (s, 1)
    {
        shared.knob (ParamIDs::delayMix, "MIX");
        shared.knob (ParamIDs::delayCut, "CUT").slider.setTooltip ("Echoes only: 0 high-cut (dark), 50 neutral, 100 low-cut (thin).");
        for (auto* p : std::initializer_list<juce::Component*> { &shared, &first, &second, &reverb })
            addAndMakeVisible (*p);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        reverb.setBounds (r.removeFromRight (juce::roundToInt (getWidth() * Layout::reverbFraction)));
        r.removeFromRight (Layout::gap);
        shared.setBounds (r.removeFromLeft (juce::roundToInt (getWidth() * Layout::delayMixFraction)));
        r.removeFromLeft (Layout::gap);
        first.setBounds (r.removeFromTop ((r.getHeight() - Layout::gap) / 2));
        r.removeFromTop (Layout::gap);
        second.setBounds (r);
    }
private:
    ControlPanel shared;
    DelayChannelPanel first, second;
    ReverbPlaceholder reverb;
};

class ModEnvelopePanel final : public Panel
{
public:
    ModEnvelopePanel (State& s, int index)
        : Panel ("MOD " + juce::String (index + 1) + " / ENVELOPE"),
          target (s, index),
          depth (s, ParamIDs::mod (index, "depth"), "DEPTH"), graph (s, index, true)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &target, &depth, &graph }) addAndMakeVisible (*c);
        const char* ids[] { "attack", "decay", "sustain", "release", "time" };
        const char* labels[] { "A", "D", "S", "R", "TIME" };
        for (size_t i = 0; i < envelope.size(); ++i)
        {
            envelope[i] = std::make_unique<ParameterControl> (s, ParamIDs::mod (index, ids[i]), labels[i], true);
            addAndMakeVisible (*envelope[i]);
        }
    }
    void resized() override
    {
        auto r = body();
        auto targetRow = r.removeFromTop (54);
        target.setBounds (targetRow.removeFromLeft (juce::roundToInt (targetRow.getWidth() * Layout::modSelectorsFraction)));
        r.removeFromTop (8);
        depth.setBounds (r.removeFromRight (Layout::modDepthWidth));
        auto plot = r.removeFromRight (Layout::modGraphWidth);
        graph.setBounds (plot.reduced (6, 18));
        const int cell = r.getWidth() / int (envelope.size());
        for (auto& control : envelope) control->setBounds (r.removeFromLeft (cell));
    }
private:
    ModTargetChoice target;
    ParameterControl depth;
    EnvelopeDisplay graph;
    std::array<std::unique_ptr<ParameterControl>, 5> envelope;
};

class ModOscillatorPanel final : public Panel
{
public:
    ModOscillatorPanel (State& s, int index)
        : Panel ("MOD " + juce::String (index + 1) + " / OSCILLATOR"),
          target (s, index),
          wave (s, ParamIDs::mod (index, "wave"), "WAVE"),
          rate (s, ParamIDs::mod (index, "rate"), "RATE"),
          gate (s, ParamIDs::mod (index, "gate"), "Gate Trig"),
          depth (s, ParamIDs::mod (index, "depth"), "DEPTH"),
          soft (s, ParamIDs::mod (index, "soft"), "SOFT")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &target, &wave, &rate, &gate, &depth, &soft })
            addAndMakeVisible (*c);
    }
    void resized() override
    {
        gate.setBounds (getWidth() - 150, 15, 128, 28);
        auto r = body();
        auto targetRow = r.removeFromTop (54);
        r.removeFromTop (8);
        const int selectorsWidth = juce::roundToInt (r.getWidth() * Layout::modSelectorsFraction);
        target.setBounds (targetRow.removeFromLeft (selectorsWidth));
        auto selectors = r.removeFromLeft (selectorsWidth);
        wave.setBounds (selectors.removeFromTop (54));
        selectors.removeFromTop (6);
        rate.setBounds (selectors);
        r.removeFromLeft (12);
        depth.setBounds (r.removeFromLeft (r.getWidth() / 2));
        soft.setBounds (r);
    }
private:
    ModTargetChoice target;
    Choice wave, rate;
    Toggle gate;
    ParameterControl depth, soft;
};

// Each workspace shows three independent, permanently attached slots together.
class ModulationPanel final : public juce::Component
{
public:
    ModulationPanel (State& state, bool oscillators)
    {
        for (size_t i = 0; i < panels.size(); ++i)
        {
            if (oscillators) panels[i] = std::make_unique<ModOscillatorPanel> (state, int (i) + 3);
            else panels[i] = std::make_unique<ModEnvelopePanel> (state, int (i));
            addAndMakeVisible (*panels[i]);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int width = (r.getWidth() - 2 * Layout::gap) / 3;
        for (auto& panel : panels)
        {
            panel->setBounds (r.removeFromLeft (width));
            r.removeFromLeft (Layout::gap);
        }
    }
private:
    std::array<std::unique_ptr<juce::Component>, 3> panels;
};

class StereoMeter final : public juce::Component, private juce::Timer
{
public:
    explicit StereoMeter (TeratoamorAudioProcessor& p) : processor (p)
    {
        setTitle ("Stereo output peak meter in dBFS");
        processor.outputPeakLeft.exchange (0.0f, std::memory_order_relaxed);
        processor.outputPeakRight.exchange (0.0f, std::memory_order_relaxed);
        startTimerHz (30);
    }
    void paint (juce::Graphics& g) override
    {
        caption (g, clipFrames > 0 ? "OUTPUT / CLIP" : "OUTPUT", { 0, 0, getWidth(), 20 }, 12,
                 clipFrames > 0 ? Theme::coral : Theme::mauve);
        auto r = getLocalBounds().withTrimmedTop (25).withTrimmedBottom (20);
        for (int channel = 0; channel < 2; ++channel)
        {
            auto row = r.removeFromTop (16);
            caption (g, channel == 0 ? "L" : "R", row.removeFromLeft (18), 11);
            g.setColour (Theme::background); g.fillRoundedRectangle (row.toFloat(), 3);
            g.setColour (Theme::border.withAlpha (0.5f)); g.drawRoundedRectangle (row.toFloat(), 3, Theme::borderWidth);
            auto fill = row.toFloat();
            fill.setWidth (fill.getWidth() * juce::jlimit (0.0f, 1.0f, (levels[size_t (channel)] + 60) / 60));
            g.setColour (Theme::coral); g.fillRoundedRectangle (fill, 3);
            r.removeFromTop (5);
        }
        caption (g, "-60              -24          0 dBFS", getLocalBounds().removeFromBottom (18), 10);
    }
private:
    void timerCallback() override
    {
        std::array<float, 2> input { processor.outputPeakLeft.exchange (0.0f, std::memory_order_relaxed),
                                    processor.outputPeakRight.exchange (0.0f, std::memory_order_relaxed) };
        auto previous = levels;
        const int oldClip = clipFrames;
        if (clipFrames > 0) --clipFrames;
        for (size_t i = 0; i < 2; ++i)
        {
            const float peak = std::isfinite (input[i]) ? juce::jmax (0.0f, input[i]) : 0.0f;
            if (peak >= 1.0f) clipFrames = 45;
            levels[i] = juce::jmax (-60.0f, juce::jmax (juce::Decibels::gainToDecibels (peak, -60.0f), levels[i] - 1.2f));
        }
        if (previous != levels || (oldClip > 0) != (clipFrames > 0)) repaint();
    }
    TeratoamorAudioProcessor& processor;
    std::array<float, 2> levels { -60, -60 };
    int clipFrames = 0;
};

class OutputSpectrum final : public juce::Component, private juce::Timer
{
public:
    explicit OutputSpectrum (TeratoamorAudioProcessor& p) : processor (p)
    {
        setTitle ("Live output frequency spectrum");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto plot = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (plot, 6.0f);
        g.setColour (Theme::border.withAlpha (0.55f));
        g.drawRoundedRectangle (plot, 6.0f, 1.0f);
        plot = plot.reduced (7.0f, 6.0f);
        for (float fraction : { 0.25f, 0.5f, 0.75f })
        {
            const auto y = plot.getY() + plot.getHeight() * fraction;
            g.setColour (Theme::border.withAlpha (0.18f));
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        }

        juce::Path area, line;
        for (size_t i = 0; i < levels.size(); ++i)
        {
            const float x = plot.getX() + plot.getWidth() * float (i) / float (levels.size() - 1);
            const float y = plot.getBottom() - plot.getHeight() * levels[i];
            if (i == 0) { line.startNewSubPath (x, y); area.startNewSubPath (x, plot.getBottom()); area.lineTo (x, y); }
            else { line.lineTo (x, y); area.lineTo (x, y); }
        }
        area.lineTo (plot.getRight(), plot.getBottom());
        area.closeSubPath();
        g.setColour (Theme::violet.withAlpha (0.28f)); g.fillPath (area);
        g.setColour (Theme::coral); g.strokePath (line, juce::PathStrokeType (1.6f));
    }

private:
    void timerCallback() override
    {
        std::array<float, TeratoamorAudioProcessor::spectrumSize> samples;
        processor.copySpectrumSamples (samples);
        fftData.fill (0.0f);
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const float phase = float (i) / float (samples.size() - 1);
            fftData[i] = samples[i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase));
        }
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float sampleRate = static_cast<float> (processor.getSpectrumSampleRate());
        const float upper = juce::jmin (18000.0f, sampleRate * 0.45f);
        for (size_t i = 0; i < levels.size(); ++i)
        {
            const float x = float (i) / float (levels.size() - 1);
            const float frequency = 60.0f * std::pow (upper / 60.0f, x);
            const int centre = juce::jlimit (1, 1023, juce::roundToInt (frequency * float (samples.size()) / sampleRate));
            float magnitude = 0.0f;
            for (int bin = juce::jmax (1, centre - 1); bin <= juce::jmin (1023, centre + 1); ++bin)
                magnitude = juce::jmax (magnitude, fftData[static_cast<size_t> (bin)]);
            const float db = juce::Decibels::gainToDecibels (magnitude / float (samples.size()), -90.0f);
            levels[i] = juce::jmax (juce::jlimit (0.0f, 1.0f, (db + 85.0f) / 65.0f), levels[i] - 0.055f);
        }
        repaint();
    }

    TeratoamorAudioProcessor& processor;
    juce::dsp::FFT fft { 11 };
    std::array<float, TeratoamorAudioProcessor::spectrumSize * 2> fftData {};
    std::array<float, 64> levels {};
};

class PresetStrip final : public juce::Component
{
public:
    explicit PresetStrip (TeratoamorAudioProcessor& p) : processor (p), folder (p.defaultPresetFolder()),
        previous ("<"), name ("Initial State"), next (">"), init ("INIT"), save ("SAVE")
    {
        for (auto* button : { &previous, &name, &next, &init, &save }) addAndMakeVisible (*button);
        previous.setComponentID ("presetPrevious"); name.setComponentID ("presetName");
        next.setComponentID ("presetNext"); init.setComponentID ("presetInit"); save.setComponentID ("presetSave");
        previous.setTooltip ("Previous preset"); next.setTooltip ("Next preset");
        name.setTooltip ("Open a preset file"); init.setTooltip ("Restore the Initial State");
        save.setTooltip ("Save the current sound as a preset file");
        previous.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        name.onClick = [this] { open(); };
        init.onClick = [this] { processor.resetToInitialState(); selected = {}; refreshName(); };
        save.onClick = [this] { saveAs(); };
        refreshName();
    }

    void refreshName()
    {
        const auto current = processor.getPresetName();
        if (name.getButtonText() != current) name.setButtonText (current);
    }

    void resized() override
    {
        auto browser = getLocalBounds();
        auto actions = browser.removeFromRight (48).reduced (0, 6);
        browser.removeFromRight (8);
        browser.setY ((getHeight() - Layout::headerBrowserHeight) / 2);
        browser.setHeight (Layout::headerBrowserHeight);
        previous.setBounds (browser.removeFromLeft (34));
        next.setBounds (browser.removeFromRight (34));
        name.setBounds (browser);
        init.setBounds (actions.removeFromTop (actions.getHeight() / 2));
        save.setBounds (actions);
    }

private:
    void scan()
    {
        files = folder.findChildFiles (juce::File::findFiles, false, "*.teratoamor");
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
        });
    }

    void load (const juce::File& file)
    {
        if (! processor.loadPresetFromFile (file))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Preset could not be loaded",
                                                      "This is not a valid Teratoamor preset file.");
            return;
        }
        folder = file.getParentDirectory();
        selected = file;
        refreshName();
    }

    void step (int direction)
    {
        scan();
        if (files.isEmpty()) return;
        const int index = files.indexOf (selected);
        if (index < 0)
        {
            load (direction > 0 ? files.getFirst() : files.getLast());
            return;
        }
        const int nextIndex = index + direction;
        if (nextIndex < 0 || nextIndex >= files.size())
        {
            processor.resetToInitialState(); selected = {}; refreshName();
            return;
        }
        load (files[nextIndex]);
    }

    void open()
    {
        const auto start = folder.isDirectory() ? folder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Open Teratoamor preset", start, "*.teratoamor");
        juce::Component::SafePointer<PresetStrip> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& dialog)
                              {
                                  if (safe == nullptr) return;
                                  const auto file = dialog.getResult();
                                  if (file != juce::File()) safe->load (file);
                              });
    }

    void saveAs()
    {
        if (folder.createDirectory().failed())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Preset could not be saved",
                                                      "The preset folder could not be created.");
            return;
        }
        const auto proposed = processor.getPresetName() == "Initial State" ? "New Preset" : processor.getPresetName();
        chooser = std::make_unique<juce::FileChooser> ("Save Teratoamor preset", folder.getChildFile (proposed + ".teratoamor"), "*.teratoamor");
        juce::Component::SafePointer<PresetStrip> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& dialog)
                              {
                                  if (safe == nullptr) return;
                                  auto file = dialog.getResult();
                                  if (file == juce::File()) return;
                                  file = file.withFileExtension (".teratoamor");
                                  if (! safe->processor.savePresetToFile (file))
                                  {
                                      juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Preset could not be saved",
                                                                                "The selected file could not be written.");
                                      return;
                                  }
                                  safe->folder = file.getParentDirectory();
                                  safe->selected = file;
                                  safe->refreshName();
                              });
    }

    TeratoamorAudioProcessor& processor;
    juce::File folder, selected;
    juce::Array<juce::File> files;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TextButton previous, name, next, init, save;
};

class VoiceCount final : public juce::Component, private juce::Timer
{
public:
    explicit VoiceCount (TeratoamorAudioProcessor& p) : processor (p),
        lastMidiActivity (p.midiActivityCounter.load (std::memory_order_relaxed)) { startTimerHz (10); }
    void paint (juce::Graphics& g) override
    {
        const auto width = float (getWidth());
        g.setColour (Theme::panel.withAlpha (0.6f));
        g.fillRoundedRectangle (4.0f, 0.0f, width - 8.0f, 37.0f, 4.0f);
        g.fillRoundedRectangle (4.0f, 41.0f, width - 8.0f, 35.0f, 4.0f);
        caption (g, "MIDI", { 0, 2, getWidth(), 13 }, 9, Theme::mauve, juce::Justification::centred);
        caption (g, "VOICES", { 0, 43, getWidth(), 13 }, 9, Theme::mauve, juce::Justification::centred);
        const float dotX = width * 0.5f, dotY = 27.0f;
        if (midiFlashTicks > 0)
        {
            g.setColour (Theme::coral.withAlpha (0.10f));
            g.fillEllipse (dotX - 9, dotY - 9, 18.0f, 18.0f);
            g.setColour (Theme::coral.withAlpha (0.25f));
            g.fillEllipse (dotX - 6, dotY - 6, 12.0f, 12.0f);
        }
        g.setColour (midiFlashTicks > 0 ? Theme::lightCore : Theme::violet);
        g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
        caption (g, juce::String (count), { 0, 56, getWidth(), 18 }, 14, Theme::coral, juce::Justification::centred);
    }
private:
    void timerCallback() override
    {
        const int current = processor.activeVoices.load (std::memory_order_relaxed);
        const auto activity = processor.midiActivityCounter.load (std::memory_order_relaxed);
        const int previousFlash = midiFlashTicks;
        if (activity != lastMidiActivity) { lastMidiActivity = activity; midiFlashTicks = 3; }
        else if (midiFlashTicks > 0) --midiFlashTicks;
        if (count != current || previousFlash != midiFlashTicks) { count = current; repaint(); }
    }
    TeratoamorAudioProcessor& processor;
    uint32_t lastMidiActivity;
    int count = 0;
    int midiFlashTicks = 0;
};

class HostSyncButton final : public juce::ToggleButton
{
public:
    explicit HostSyncButton (State& state)
    {
        setComponentID (ParamIDs::tempoSync);
        setButtonText ("HOST SYNC");
        setTooltip ("Follow the host tempo. Click again to use the manual BPM.");
        attachment = std::make_unique<State::ButtonAttachment> (state, ParamIDs::tempoSync, *this);
    }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool host = getToggleState();
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (host ? Theme::coral.withAlpha (0.32f) : Theme::deep.withAlpha (0.42f));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
        g.setColour (host ? Theme::coral.withAlpha (0.9f) : Theme::border.withAlpha (0.7f));
        g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.2f);

        const float centreY = float (getHeight()) * 0.5f;
        g.setColour (host ? Theme::lightCore : Theme::mauve);
        g.drawRoundedRectangle (10.0f, centreY - 4.0f, 13.0f, 8.0f, 4.0f, 1.5f);
        g.drawRoundedRectangle (17.0f, centreY - 4.0f, 13.0f, 8.0f, 4.0f, 1.5f);
        if (host)
        {
            g.setColour (Theme::coral.withAlpha (0.24f));
            g.fillEllipse (31.0f, centreY - 7.0f, 14.0f, 14.0f);
        }
        g.setColour (host ? Theme::lightCore : Theme::violet);
        g.fillEllipse (34.0f, centreY - 4.0f, 8.0f, 8.0f);
        caption (g, "HOST SYNC", { 47, 0, getWidth() - 50, getHeight() }, 10,
                 host ? Theme::lightCore : over ? Theme::blush : Theme::mauve);
        if (hasKeyboardFocus (true))
        {
            g.setColour (Theme::lightCore);
            g.drawRoundedRectangle (bounds.reduced (2.0f), bounds.getHeight() * 0.5f, 1.0f);
        }
    }
private:
    std::unique_ptr<State::ButtonAttachment> attachment;
};

class HeaderKnobCard final : public juce::Component, private juce::Timer
{
public:
    enum class Kind { tempo, master };
    HeaderKnobCard (TeratoamorAudioProcessor& p, Kind type)
        : processor (p), state (p.apvts), kind (type),
          control (state, type == Kind::tempo ? ParamIDs::tempoBpm : ParamIDs::masterLevel,
                   type == Kind::tempo ? "BPM" : "dB", false, false)
    {
        setComponentID (kind == Kind::tempo ? "headerTempoCard" : "headerMasterCard");
        control.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
        addAndMakeVisible (control);
        if (kind == Kind::tempo)
        {
            control.slider.textFromValueFunction = [this] (double value)
            {
                const bool host = state.getRawParameterValue (ParamIDs::tempoSync)->load() >= 0.5f;
                return juce::String (juce::roundToInt (host ? processor.getCurrentBpm() : value));
            };
            hostSync = std::make_unique<HostSyncButton> (state);
            addAndMakeVisible (*hostSync);
            hostSync->onClick = [this] { refreshTempo(); };
            startTimerHz (20);
            refreshTempo();
        }
        else
        {
            control.slider.textFromValueFunction = [] (double value)
            {
                if (value <= 0.0) return juce::String::fromUTF8 (u8"\u2212\u221e");
                return juce::String (20.0 * std::log10 (value / 100.0), 1)
                    .replace ("-", juce::String::fromUTF8 (u8"\u2212"));
            };
            control.slider.valueFromTextFunction = [] (const juce::String& text)
            {
                const auto normalised = text.trim().replace (juce::String::fromUTF8 (u8"\u2212"), "-");
                if (normalised == juce::String::fromUTF8 (u8"-\u221e")
                    || normalised.equalsIgnoreCase ("-inf")) return 0.0;
                const double db = normalised.getDoubleValue();
                return juce::jlimit (0.0, 100.0, 100.0 * std::pow (10.0, db / 20.0));
            };
            control.slider.updateText();
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto box = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (Theme::border.withAlpha (0.55f));
        g.drawRoundedRectangle (box, 6.0f, 1.0f);
        caption (g, kind == Kind::tempo ? "BPM" : "dB", { getWidth() - 56, 38, 50, 16 },
                 10, Theme::mauve, juce::Justification::centred);
        if (kind == Kind::master)
            caption (g, "MASTER", { 4, Layout::headerSectionFooterTop, getWidth() - 8,
                                     Layout::headerSectionFooterHeight }, 10, Theme::mauve,
                     juce::Justification::centred);
    }
    void resized() override
    {
        control.setBounds (4, 0, getWidth() - 8, 58);
        if (hostSync != nullptr)
            hostSync->setBounds (6, Layout::headerSectionFooterTop,
                                 getWidth() - 12, Layout::headerSectionFooterHeight);
    }
private:
    void timerCallback() override { refreshTempo(); }
    void refreshTempo()
    {
        const bool host = state.getRawParameterValue (ParamIDs::tempoSync)->load() >= 0.5f;
        const double bpm = host ? processor.getCurrentBpm() : control.slider.getValue();
        if (firstRefresh || host != lastHost)
        {
            control.slider.setEnabled (! host);
            control.slider.setTooltip (host ? "Host BPM. Click HOST SYNC to edit the manual tempo."
                                            : "Manual BPM for delays and modulation.");
        }
        if (firstRefresh || host != lastHost || bpm != lastBpm) control.slider.updateText();
        firstRefresh = false;
        lastHost = host;
        lastBpm = bpm;
    }
    TeratoamorAudioProcessor& processor;
    State& state;
    Kind kind;
    ParameterControl control;
    std::unique_ptr<HostSyncButton> hostSync;
    bool firstRefresh = true, lastHost = false;
    double lastBpm = -1.0;
};

class Header final : public juce::Component, private juce::Timer
{
public:
    explicit Header (TeratoamorAudioProcessor& p) : tempo (p, HeaderKnobCard::Kind::tempo),
        master (p, HeaderKnobCard::Kind::master), meter (p), spectrum (p), presets (p), voices (p)
    {
        logo = juce::ImageCache::getFromMemory (TeratoamorAssets::icon_voidpetal_smaller_margins_png,
                                              TeratoamorAssets::icon_voidpetal_smaller_margins_pngSize);
        for (auto* c : std::initializer_list<juce::Component*> { &presets, &voices, &spectrum, &tempo, &master, &meter }) addAndMakeVisible (*c);
        startTimerHz (10); timerCallback();
    }
    void paint (juce::Graphics& g) override
    {
        g.drawImageWithin (logo, 0, 8, 78, getHeight() - 16, juce::RectanglePlacement::centred);
        caption (g, "T E R A T O A M O R", { 92, 23, 250, 32 }, 25, Theme::blush);
        g.setColour (Theme::border.withAlpha (0.4f));
        g.drawHorizontalLine (getHeight() - 1, 0, float (getWidth()));
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 7);
        meter.setBounds (r.removeFromRight (Layout::headerMeterWidth)); r.removeFromRight (Layout::headerControlGap);
        auto masterColumn = r.removeFromRight (Layout::headerMasterWidth); r.removeFromRight (Layout::headerControlGap);
        master.setBounds (masterColumn.withY (Layout::headerSectionTop).withHeight (Layout::headerSectionHeight));
        auto tempoColumn = r.removeFromRight (Layout::headerTempoWidth); r.removeFromRight (Layout::headerControlGap);
        tempo.setBounds (tempoColumn.withY (Layout::headerSectionTop).withHeight (Layout::headerSectionHeight));
        spectrum.setBounds (r.removeFromRight (Layout::headerSpectrumWidth).reduced (0, 3));
        r.removeFromRight (Layout::headerControlGap);
        voices.setBounds (r.removeFromRight (Layout::headerVoicesWidth).reduced (0, 5));
        presets.setBounds (Layout::headerPresetLeft, 7, Layout::headerPresetWidth, getHeight() - 14);
    }
private:
    void timerCallback() override
    {
        presets.refreshName();
    }
    juce::Image logo;
    HeaderKnobCard tempo, master;
    StereoMeter meter;
    OutputSpectrum spectrum;
    PresetStrip presets;
    VoiceCount voices;
};
}

struct EditorContent::Impl
{
    explicit Impl (EditorContent& owner, TeratoamorAudioProcessor& p)
        : processor (p), header (p), global (p.apvts), modEnvelopes (p.apvts, false), modOscillators (p.apvts, true), space (p.apvts)
    {
        owner.addAndMakeVisible (header);
        for (int i = 0; i < 3; ++i)
        {
            elements[size_t (i)] = std::make_unique<ElementPanel> (p.apvts, i);
            owner.addAndMakeVisible (*elements[size_t (i)]);
        }
        const char* names[] { "GLOBAL", "MODULATION (ENV)", "MODULATION (OSC)", "SPACE" };
        for (size_t i = 0; i < tabs.size(); ++i)
        {
            tabs[i].setButtonText (names[i]);
            tabs[i].setComponentID (names[i]);
            tabs[i].onClick = [this, i] { select (int (i)); };
            owner.addAndMakeVisible (tabs[i]);
        }
        owner.addAndMakeVisible (global); owner.addChildComponent (modEnvelopes);
        owner.addChildComponent (modOscillators); owner.addChildComponent (space);
        select (0);
    }
    void select (int i)
    {
        global.setVisible (i == 0); modEnvelopes.setVisible (i == 1);
        modOscillators.setVisible (i == 2); space.setVisible (i == 3);
        for (size_t t = 0; t < tabs.size(); ++t) tabs[t].setToggleState (int (t) == i, juce::dontSendNotification);
    }
    TeratoamorAudioProcessor& processor;
    Header header;
    std::array<std::unique_ptr<ElementPanel>, 3> elements;
    std::array<juce::TextButton, 4> tabs;
    GlobalPanel global;
    ModulationPanel modEnvelopes, modOscillators;
    SpacePanel space;
};

EditorContent::EditorContent (TeratoamorAudioProcessor& p)
{
    impl = std::make_unique<Impl> (*this, p);
    // Apply after constructing the children so JUCE rebuilds every value label
    // with our shared draggable implementation, including initially hidden tabs.
    setLookAndFeel (&lookAndFeel);
    setSize (Layout::width, Layout::height);
}
EditorContent::~EditorContent()
{
    impl.reset();
    setLookAndFeel (nullptr);
}
void EditorContent::resized()
{
    auto r = getLocalBounds().reduced (Layout::margin, 0);
    impl->header.setBounds (r.removeFromTop (Layout::header));
    r.removeFromTop (Layout::gap);
    auto row = r.removeFromTop (Layout::elements);
    const int w = (row.getWidth() - 2 * Layout::gap) / 3;
    for (auto& e : impl->elements) { e->setBounds (row.removeFromLeft (w)); row.removeFromLeft (Layout::gap); }
    r.removeFromTop (Layout::ribbon);
    auto tabs = r.removeFromTop (Layout::tabs);
    for (auto& tab : impl->tabs) { tab.setBounds (tabs.removeFromLeft (Layout::workspaceTabWidth).reduced (0, 5)); tabs.removeFromLeft (10); }
    r.removeFromTop (8);
    r.removeFromBottom (Layout::footer + 8);
    for (auto* page : std::initializer_list<juce::Component*> { &impl->global, &impl->modEnvelopes, &impl->modOscillators, &impl->space }) page->setBounds (r);
}
void EditorContent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
    const float baseline = float (Layout::header + Layout::gap + Layout::elements + Layout::ribbon / 2);
    // Static ornament, deliberately not labelled as an analyser or audio visualisation.
    for (int i = 0; i < 3; ++i)
    {
        juce::Path curve;
        curve.startNewSubPath (float (Layout::margin), baseline + 5);
        curve.cubicTo (400, baseline - float (15 + i * 4), 750, baseline + float (22 - i * 5), 1150, baseline - 4);
        curve.quadraticTo (1350, baseline - float (12 - i * 4), float (getWidth() - Layout::margin), baseline + 5);
        g.setColour ((i == 1 ? Theme::blush : Theme::violet).withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (1));
    }
    caption (g, "TERATOAMOR  /  " + juce::String (JucePlugin_VersionString),
             { Layout::margin, getHeight() - Layout::footer, 400, Layout::footer }, 11);
}
}
