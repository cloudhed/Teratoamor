#include "PluginProcessor.h"
#include "PluginEditor.h"

TeratoamorAudioProcessor::TeratoamorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, Parameters::stateType, Parameters::createLayout())
{
    masterLevelParam = apvts.getRawParameterValue (ParamIDs::masterLevel);

    for (int e = 0; e < ParamIDs::numElements; ++e)
    {
        auto& p = elementParams[(size_t) e];
        p.enabled  = apvts.getRawParameterValue (ParamIDs::enabled (e));
        p.filter   = apvts.getRawParameterValue (ParamIDs::filter (e));
        p.octave   = apvts.getRawParameterValue (ParamIDs::octave (e));
        p.semitone = apvts.getRawParameterValue (ParamIDs::semitone (e));
        p.fine     = apvts.getRawParameterValue (ParamIDs::fine (e));
        p.warp     = apvts.getRawParameterValue (ParamIDs::warp (e));
        p.clip     = e == 1 ? nullptr : apvts.getRawParameterValue (ParamIDs::clip (e));
        p.width    = apvts.getRawParameterValue (ParamIDs::width (e));
        p.attack   = apvts.getRawParameterValue (ParamIDs::attack (e));
        p.release  = apvts.getRawParameterValue (ParamIDs::release (e));
        p.level    = apvts.getRawParameterValue (ParamIDs::level (e));
        p.pan      = apvts.getRawParameterValue (ParamIDs::pan (e));
    }
}

void TeratoamorAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void TeratoamorAudioProcessor::releaseResources()
{
    engine.reset();
}

EngineParams TeratoamorAudioProcessor::readParams() const noexcept
{
    EngineParams out;
    out.master = masterLevelParam->load();

    for (size_t e = 0; e < elementParams.size(); ++e)
    {
        const auto& p = elementParams[e];
        auto& o = out.elements[e];
        o.enabled   = p.enabled->load() >= 0.5f;
        o.filterMode = static_cast<FilterMode> (juce::jlimit (0, 5, juce::roundToInt (p.filter->load())));
        o.octave    = juce::roundToInt (p.octave->load());
        o.semitone  = juce::roundToInt (p.semitone->load());
        o.fineCents = p.fine->load();
        o.warp      = p.warp->load();
        o.clip      = p.clip != nullptr ? p.clip->load() : 0.0f;
        o.width     = p.width->load();
        o.attack    = p.attack->load();
        o.release   = p.release->load();
        o.level     = p.level->load();
        o.pan       = p.pan->load();
    }

    return out;
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

    const int numSamples = buffer.getNumSamples();
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    engine.setParams (readParams());

    // Render in slices so each MIDI event takes effect at its exact sample position.
    int rendered = 0;

    for (const auto metadata : midiMessages)
    {
        const int eventPosition = juce::jlimit (rendered, numSamples, metadata.samplePosition);

        if (eventPosition > rendered)
        {
            engine.render (left + rendered, right + rendered, eventPosition - rendered);
            rendered = eventPosition;
        }

        if (metadata.numBytes > 3)
            continue;   // ignore SysEx and other long messages

        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            engine.noteOff (message.getNoteNumber());
        else if (message.isPitchWheel())
            engine.setPitchBend (static_cast<float> (message.getPitchWheelValue() - 8192) / 8192.0f);
        else if (message.isController() && message.getControllerNumber() == 64)
            engine.setSustainPedal (message.getControllerValue() >= 64);
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            engine.allNotesOff();
    }

    if (rendered < numSamples)
        engine.render (left + rendered, right + rendered, numSamples - rendered);

    midiMessages.clear();
}

juce::AudioProcessorEditor* TeratoamorAudioProcessor::createEditor()
{
    return new TeratoamorAudioProcessorEditor (*this);
}

void TeratoamorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", 1, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TeratoamorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);

        // Ignore data that did not come from this plugin; missing parameters keep their defaults.
        if (state.isValid() && state.getType() == Parameters::stateType)
            apvts.replaceState (state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TeratoamorAudioProcessor();
}
