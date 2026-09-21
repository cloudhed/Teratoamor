#include "PluginEditor.h"

namespace
{
    constexpr int margin = 10;
    constexpr int panelWidth = 300;
    constexpr int rowHeight = 26;
    constexpr int labelWidth = 70;
    constexpr int titleHeight = 44;
    constexpr int groupTopPadding = 22;
    constexpr int filterStripHeight = groupTopPadding + rowHeight + 6;   // global filter group
    constexpr int delayHeight = groupTopPadding + 3 * rowHeight + 6;
    constexpr int linesPerPanel = 16;   // on, filter, 8 tone sliders, link line, 5 envelope sliders
}

TeratoamorAudioProcessorEditor::SliderRow::SliderRow (juce::AudioProcessorValueTreeState& state,
                                                      const juce::String& parameterID,
                                                      const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);

    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);

    attachment = std::make_unique<SliderAttachment> (state, parameterID, slider);
}

TeratoamorAudioProcessorEditor::TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    auto& state = p.apvts;

    title.setText ("Teratoamor", juce::dontSendNotification);
    title.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    addAndMakeVisible (title);

    master = std::make_unique<SliderRow> (state, ParamIDs::masterLevel, "Master");
    addAndMakeVisible (master->label);
    addAndMakeVisible (master->slider);

    distortionGroup.setText ("Distortion");
    addAndMakeVisible (distortionGroup);
    distortionTypeLabel.setText ("Type", juce::dontSendNotification);
    addAndMakeVisible (distortionTypeLabel);
    addAndMakeVisible (distortionType);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (ParamIDs::distortionType)))
        distortionType.addItemList (choice->choices, 1);
    distortionTypeAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::distortionType, distortionType);
    distortionCrush = std::make_unique<SliderRow> (state, ParamIDs::distortionCrush, "Drive");
    distortionTone = std::make_unique<SliderRow> (state, ParamIDs::distortionTone, "Tone");
    for (auto* row : { distortionCrush.get(), distortionTone.get() })
    {
        addAndMakeVisible (row->label);
        addAndMakeVisible (row->slider);
    }
    distortionCrush->slider.setTooltip ("Saturation amount in Drive mode. Zero is dry.");
    distortionTone->slider.setTooltip ("Dark to bright; 50 is neutral. Shapes Drive when its amount is above zero.");

    delayGroup.setText ("Delay");
    addAndMakeVisible (delayGroup);
    delayMix = std::make_unique<SliderRow> (state, ParamIDs::delayMix, "Mix");
    delayCut = std::make_unique<SliderRow> (state, ParamIDs::delayCut, "Cut");
    for (auto* row : { delayMix.get(), delayCut.get() })
    {
        addAndMakeVisible (row->label);
        addAndMakeVisible (row->slider);
    }
    delayMix->slider.setTooltip ("0: dry, 100: echoes only. Both delays off bypasses this block.");
    delayCut->slider.setTooltip ("Echoes only: 0 high-cut (darker), 50 no cut, 100 low-cut (thinner).");
    for (int d = 0; d < 2; ++d)
    {
        auto& panel = delays[static_cast<size_t> (d)];
        panel.on.setButtonText ("Delay " + juce::String (d + 1));
        addAndMakeVisible (panel.on);
        panel.onAttachment = std::make_unique<ButtonAttachment> (state, ParamIDs::delayOn (d), panel.on);
        panel.rateLabel.setText ("Rate", juce::dontSendNotification);
        addAndMakeVisible (panel.rateLabel);
        addAndMakeVisible (panel.rate);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (ParamIDs::delayRate (d))))
            panel.rate.addItemList (choice->choices, 1);
        panel.rateAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::delayRate (d), panel.rate);
        panel.rate.setTooltip ("Host-synced note length. T: triplet, D: dotted. Without host tempo: 120 BPM.");
        panel.decay = std::make_unique<SliderRow> (state, ParamIDs::delayDecay (d), "Decay");
        panel.pan = std::make_unique<SliderRow> (state, ParamIDs::delayPan (d), "Pan");
        panel.decay->slider.setTooltip ("0: one echo; 100: long, fading repeats.");
        panel.pan->slider.setTooltip ("-100: left, 0: centred mono echo, +100: right.");
        for (auto* row : { panel.decay.get(), panel.pan.get() })
        {
            addAndMakeVisible (row->label);
            addAndMakeVisible (row->slider);
        }
    }

    filterGroup.setText ("Global filter");
    addAndMakeVisible (filterGroup);

    filterTypeLabel.setText ("Type", juce::dontSendNotification);
    addAndMakeVisible (filterTypeLabel);
    addAndMakeVisible (filterType);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (ParamIDs::filterType)))
        filterType.addItemList (choice->choices, 1);

    filterTypeAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::filterType, filterType);

    filterQ = std::make_unique<SliderRow> (state, ParamIDs::filterQ, "Q");
    addAndMakeVisible (filterQ->label);
    addAndMakeVisible (filterQ->slider);

    filterCutoff = std::make_unique<SliderRow> (state, ParamIDs::filterCutoff, "Cutoff");
    addAndMakeVisible (filterCutoff->label);
    addAndMakeVisible (filterCutoff->slider);

    for (int e = 0; e < ParamIDs::numElements; ++e)
    {
        auto& panel = panels[(size_t) e];
        panel.group.setText ("Element " + juce::String (e + 1));
        addAndMakeVisible (panel.group);

        addAndMakeVisible (panel.on);
        panel.onAttachment = std::make_unique<ButtonAttachment> (state, ParamIDs::enabled (e), panel.on);

        panel.filterLabel.setText ("Filter", juce::dontSendNotification);
        addAndMakeVisible (panel.filterLabel);
        addAndMakeVisible (panel.filter);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (ParamIDs::filter (e))))
            panel.filter.addItemList (choice->choices, 1);

        panel.filterAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::filter (e), panel.filter);

        // Octave .. Pan. Element 2 has no Clip, so it gets a gap that keeps the columns aligned.
        struct Entry { juce::String id; const char* text; };
        const Entry toneEntries[] = {
            { ParamIDs::octave (e), "Octave" },     { ParamIDs::semitone (e), "Semitone" },
            { ParamIDs::fine (e), "Fine" },         { ParamIDs::width (e), "Width" },
            { ParamIDs::warp (e), "Warp" },         { ParamIDs::clip (e), "Clip" },
            { ParamIDs::level (e), "Level" },       { ParamIDs::pan (e), "Pan" } };

        for (const auto& entry : toneEntries)
        {
            if (state.getParameter (entry.id) == nullptr)
            {
                panel.tone.push_back (nullptr);
                continue;
            }

            panel.tone.push_back (std::make_unique<SliderRow> (state, entry.id, entry.text));
            addAndMakeVisible (panel.tone.back()->label);
            addAndMakeVisible (panel.tone.back()->slider);
        }

        if (e == 0)
        {
            panel.envelopeHeader.setText ("Envelope (Elements 2 and 3 can follow it)", juce::dontSendNotification);
            addAndMakeVisible (panel.envelopeHeader);
        }
        else
        {
            addAndMakeVisible (panel.link);
            panel.linkAttachment = std::make_unique<ButtonAttachment> (state, ParamIDs::link (e), panel.link);
            panel.link.onClick = [this] { refreshLinkState(); };
        }

        const Entry envelopeEntries[] = {
            { ParamIDs::attack (e), "Attack" },   { ParamIDs::decay (e), "Decay" },
            { ParamIDs::sustain (e), "Sustain" }, { ParamIDs::release (e), "Release" },
            { ParamIDs::time (e), "Time" } };

        for (const auto& entry : envelopeEntries)
        {
            panel.envelope.push_back (std::make_unique<SliderRow> (state, entry.id, entry.text));
            addAndMakeVisible (panel.envelope.back()->label);
            addAndMakeVisible (panel.envelope.back()->slider);
        }
    }

    modulation.group.setText ("Modulation");
    addAndMakeVisible (modulation.group);
    for (int m = 0; m < ParamIDs::numMods; ++m)
        modulation.select.addItem ("Modulation " + juce::String (m + 1) + (m < 3 ? " (envelope)" : " (oscillator)"), m + 1);
    addAndMakeVisible (modulation.select);
    for (auto* box : { &modulation.target, &modulation.wave, &modulation.rate })
        addChildComponent (box);
    addChildComponent (modulation.gate);
    addAndMakeVisible (modulation.target);
    modulation.masterPan = std::make_unique<SliderRow> (state, ParamIDs::masterPan, "Master Pan");
    addAndMakeVisible (modulation.masterPan->label);
    addAndMakeVisible (modulation.masterPan->slider);
    modulation.select.onChange = [this] { showModulationSection (modulation.select.getSelectedId() - 1); };
    modulation.select.setSelectedId (1, juce::dontSendNotification);
    showModulationSection (0);

    setSize (5 * margin + 4 * panelWidth,
             titleHeight + 2 * (filterStripHeight + margin) + delayHeight + margin + groupTopPadding + linesPerPanel * rowHeight + 3 * margin);

    refreshLinkState();
    startTimerHz (10);   // picks up Link changes made by the host (automation, session load)
}

void TeratoamorAudioProcessorEditor::showModulationSection (int section)
{
    auto& state = processor.apvts;
    auto& m = modulation;
    m.section = section;

    // Drop the old attachments before the controls they point at are reused.
    m.targetAttachment.reset(); m.waveAttachment.reset(); m.rateAttachment.reset(); m.gateAttachment.reset();
    m.rows.clear();
    m.target.clear (juce::dontSendNotification);
    m.wave.clear (juce::dontSendNotification);
    m.rate.clear (juce::dontSendNotification);

    const auto fill = [&state] (juce::ComboBox& box, const juce::String& id)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
            box.addItemList (choice->choices, 1);
    };
    const auto addRow = [this, &state, &m, section] (const char* name, const char* text)
    {
        m.rows.push_back (std::make_unique<SliderRow> (state, ParamIDs::mod (section, name), text));
        addAndMakeVisible (m.rows.back()->label);
        addAndMakeVisible (m.rows.back()->slider);
    };

    fill (m.target, ParamIDs::mod (section, "target"));
    m.targetAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::mod (section, "target"), m.target);
    addRow ("depth", "Depth");

    const bool oscillator = section >= 3;
    m.wave.setVisible (oscillator); m.rate.setVisible (oscillator); m.gate.setVisible (oscillator);

    if (oscillator)
    {
        fill (m.wave, ParamIDs::mod (section, "wave"));
        fill (m.rate, ParamIDs::mod (section, "rate"));
        m.waveAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::mod (section, "wave"), m.wave);
        m.rateAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::mod (section, "rate"), m.rate);
        m.gateAttachment = std::make_unique<ButtonAttachment> (state, ParamIDs::mod (section, "gate"), m.gate);
        addRow ("soft", "Soft");
    }
    else
    {
        addRow ("attack", "Attack"); addRow ("decay", "Decay"); addRow ("sustain", "Sustain");
        addRow ("release", "Release"); addRow ("time", "Time");
    }

    resized();
}

void TeratoamorAudioProcessorEditor::refreshLinkState()
{
    for (int e = 1; e < ParamIDs::numElements; ++e)
    {
        auto& panel = panels[(size_t) e];
        const bool linked = processor.apvts.getRawParameterValue (ParamIDs::link (e))->load() >= 0.5f;

        if (linked == panel.linked && panel.envelope.front()->slider.isEnabled() == ! linked)
            continue;

        panel.linked = linked;

        // Greyed out: the Element is using Element 1's envelope, so its own controls do nothing.
        for (auto& row : panel.envelope)
        {
            row->slider.setEnabled (! linked);
            row->label.setEnabled (! linked);
            row->slider.setAlpha (linked ? 0.35f : 1.0f);
            row->label.setAlpha (linked ? 0.35f : 1.0f);
        }
    }
}

void TeratoamorAudioProcessorEditor::timerCallback()
{
    refreshLinkState();
}

void TeratoamorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void TeratoamorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (margin);

    auto top = area.removeFromTop (titleHeight - margin);
    title.setBounds (top.removeFromLeft (300));
    auto masterArea = top.removeFromRight (360);
    master->label.setBounds (masterArea.removeFromLeft (labelWidth));
    master->slider.setBounds (masterArea);

    area.removeFromTop (margin);

    auto strip = area.removeFromTop (filterStripHeight);
    filterGroup.setBounds (strip);
    auto stripInner = strip.reduced (margin, 0).withTrimmedTop (groupTopPadding);
    auto typeArea = stripInner.removeFromLeft (200);
    filterTypeLabel.setBounds (typeArea.removeFromLeft (50));
    filterType.setBounds (typeArea.reduced (0, 2));
    stripInner.removeFromLeft (margin);
    auto qArea = stripInner.removeFromLeft (stripInner.getWidth() / 2);
    filterQ->label.setBounds (qArea.removeFromLeft (30));
    filterQ->slider.setBounds (qArea);
    stripInner.removeFromLeft (margin);
    filterCutoff->label.setBounds (stripInner.removeFromLeft (50));
    filterCutoff->slider.setBounds (stripInner);

    area.removeFromTop (margin);

    auto placeRow = [] (juce::Rectangle<int> line, SliderRow* row)
    {
        if (row == nullptr)
            return;

        row->label.setBounds (line.removeFromLeft (labelWidth));
        row->slider.setBounds (line);
    };

    auto distortionStrip = area.removeFromTop (filterStripHeight);
    distortionGroup.setBounds (distortionStrip);
    auto distortionInner = distortionStrip.reduced (margin, 0).withTrimmedTop (groupTopPadding);
    auto distortionTypeArea = distortionInner.removeFromLeft (200);
    distortionTypeLabel.setBounds (distortionTypeArea.removeFromLeft (50));
    distortionType.setBounds (distortionTypeArea.reduced (0, 2));
    distortionInner.removeFromLeft (margin);
    auto crushArea = distortionInner.removeFromLeft (distortionInner.getWidth() / 2);
    placeRow (crushArea.withTrimmedRight (margin), distortionCrush.get());
    placeRow (distortionInner, distortionTone.get());
    area.removeFromTop (margin);

    auto delayArea = area.removeFromTop (delayHeight);
    delayGroup.setBounds (delayArea);
    auto delayInner = delayArea.reduced (margin, 0).withTrimmedTop (groupTopPadding);
    auto sharedLine = delayInner.removeFromTop (rowHeight);
    placeRow (sharedLine.removeFromLeft (sharedLine.getWidth() / 2).withTrimmedRight (margin), delayMix.get());
    placeRow (sharedLine, delayCut.get());
    for (auto& panel : delays)
    {
        auto line = delayInner.removeFromTop (rowHeight);
        panel.on.setBounds (line.removeFromLeft (95));
        panel.rateLabel.setBounds (line.removeFromLeft (40));
        panel.rate.setBounds (line.removeFromLeft (130).reduced (0, 2));
        line.removeFromLeft (margin);
        placeRow (line.removeFromLeft (line.getWidth() / 2).withTrimmedRight (margin), panel.decay.get());
        placeRow (line, panel.pan.get());
    }
    area.removeFromTop (margin);

    for (size_t e = 0; e < panels.size(); ++e)
    {
        auto& panel = panels[e];
        auto column = area.removeFromLeft (panelWidth);
        area.removeFromLeft (margin);

        panel.group.setBounds (column);
        auto inner = column.reduced (margin, 0).withTrimmedTop (groupTopPadding);
        auto nextLine = [&inner] { return inner.removeFromTop (rowHeight); };

        panel.on.setBounds (nextLine());

        auto filterLine = nextLine();
        panel.filterLabel.setBounds (filterLine.removeFromLeft (labelWidth));
        panel.filter.setBounds (filterLine.reduced (0, 2));

        for (auto& row : panel.tone)
            placeRow (nextLine(), row.get());

        auto linkLine = nextLine();

        if (e == 0)
            panel.envelopeHeader.setBounds (linkLine);
        else
            panel.link.setBounds (linkLine);

        for (auto& row : panel.envelope)
            placeRow (nextLine(), row.get());
    }

    // Modulation column: selector, target, then the section's controls.
    auto column = area.removeFromLeft (panelWidth);
    modulation.group.setBounds (column);
    auto inner = column.reduced (margin, 0).withTrimmedTop (groupTopPadding);
    auto nextLine = [&inner] { return inner.removeFromTop (rowHeight); };
    modulation.select.setBounds (nextLine().reduced (0, 2));
    modulation.target.setBounds (nextLine().reduced (0, 2));

    if (modulation.section >= 3)
    {
        auto line = nextLine();
        modulation.wave.setBounds (line.removeFromLeft (line.getWidth() / 2).reduced (0, 2).withTrimmedRight (margin / 2));
        modulation.rate.setBounds (line.reduced (0, 2).withTrimmedLeft (margin / 2));
        modulation.gate.setBounds (nextLine());
    }

    for (auto& row : modulation.rows)
        placeRow (nextLine(), row.get());

    inner.removeFromTop (margin);
    placeRow (nextLine(), modulation.masterPan.get());
}
