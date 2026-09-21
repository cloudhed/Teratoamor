#include "PluginProcessor.h"
#include "PluginEditor.h"

TeratoamorAudioProcessor::TeratoamorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void TeratoamorAudioProcessor::prepareToPlay (double, int)
{
}

void TeratoamorAudioProcessor::releaseResources()
{
}

bool TeratoamorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Instrument: stereo output only, no inputs.
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void TeratoamorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // No engine yet: MIDI is accepted and ignored, output is silent.
    midiMessages.clear();
    buffer.clear();
}

juce::AudioProcessorEditor* TeratoamorAudioProcessor::createEditor()
{
    return new TeratoamorAudioProcessorEditor (*this);
}

namespace
{
    const juce::Identifier stateType { "TeratoamorState" };
    const juce::Identifier stateVersion { "version" };
}

void TeratoamorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Minimal valid state: an empty, versioned tree. Parameters will be added later.
    juce::ValueTree state (stateType);
    state.setProperty (stateVersion, 1, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TeratoamorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);

        if (! state.isValid() || state.getType() != stateType)
            return;

        // Nothing to restore yet.
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TeratoamorAudioProcessor();
}
