#include "PluginEditor.h"

TeratoamorAudioProcessorEditor::TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (480, 240);
}

void TeratoamorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));

    auto area = getLocalBounds().reduced (20);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText ("Teratoamor", area.removeFromTop (area.getHeight() / 2), juce::Justification::centredBottom);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("Engine not connected yet", area, juce::Justification::centredTop);
}

void TeratoamorAudioProcessorEditor::resized()
{
}
