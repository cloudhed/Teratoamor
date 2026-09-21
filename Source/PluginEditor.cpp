#include "PluginEditor.h"

TeratoamorAudioProcessorEditor::TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor& p)
    : AudioProcessorEditor (&p), generic (p)
{
    addAndMakeVisible (generic);
    setSize (520, 640);
}

void TeratoamorAudioProcessorEditor::resized()
{
    generic.setBounds (getLocalBounds());
}
