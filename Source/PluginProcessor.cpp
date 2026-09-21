#include "PluginProcessor.h"
#include "PluginEditor.h"

TeratoamorAudioProcessor::TeratoamorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, Parameters::stateType, Parameters::createLayout())
{
    delayMixParam = apvts.getRawParameterValue (ParamIDs::delayMix);
    delayCutParam = apvts.getRawParameterValue (ParamIDs::delayCut);
    for (int d = 0; d < 2; ++d)
    {
        auto& p = delayParams[static_cast<size_t> (d)];
        p.on = apvts.getRawParameterValue (ParamIDs::delayOn (d));
        p.rate = apvts.getRawParameterValue (ParamIDs::delayRate (d));
        p.decay = apvts.getRawParameterValue (ParamIDs::delayDecay (d));
        p.pan = apvts.getRawParameterValue (ParamIDs::delayPan (d));
    }
    distortionTypeParam = apvts.getRawParameterValue (ParamIDs::distortionType);
    distortionCrushParam = apvts.getRawParameterValue (ParamIDs::distortionCrush);
    distortionToneParam = apvts.getRawParameterValue (ParamIDs::distortionTone);
    masterLevelParam = apvts.getRawParameterValue (ParamIDs::masterLevel);
    filterTypeParam = apvts.getRawParameterValue (ParamIDs::filterType);
    filterCutoffParam = apvts.getRawParameterValue (ParamIDs::filterCutoff);
    filterQParam = apvts.getRawParameterValue (ParamIDs::filterQ);

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
        p.link     = e == 0 ? nullptr : apvts.getRawParameterValue (ParamIDs::link (e));
        p.time     = apvts.getRawParameterValue (ParamIDs::time (e));
        p.decay    = apvts.getRawParameterValue (ParamIDs::decay (e));
        p.sustain  = apvts.getRawParameterValue (ParamIDs::sustain (e));
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
    out.delay.mix = delayMixParam->load();
    out.delay.cut = delayCutParam->load();
    out.delay.bpm = hostBpm.load();
    for (size_t d = 0; d < delayParams.size(); ++d)
    {
        const auto& p = delayParams[d];
        out.delay.lines[d] = { p.on->load() >= 0.5f, juce::roundToInt (p.rate->load()), p.decay->load(), p.pan->load() };
    }
    out.distortion = { distortionCrushParam->load(), distortionToneParam->load(),
        static_cast<DistortionType> (juce::jlimit (0, 1, juce::roundToInt (distortionTypeParam->load()))) };
    out.master = masterLevelParam->load();
    out.globalFilter.type = static_cast<GlobalFilterType> (juce::jlimit (0, 5, juce::roundToInt (filterTypeParam->load())));
    out.globalFilter.cutoff = filterCutoffParam->load();
    out.globalFilter.q = filterQParam->load();

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
        o.link      = p.link != nullptr && p.link->load() >= 0.5f;
        o.time      = p.time->load();
        o.decay     = p.decay->load();
        o.sustain   = p.sustain->load();
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

    double bpm = 120.0;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto tempo = position->getBpm()) bpm = *tempo;
    hostBpm.store (DelayRates::validBpm (bpm));
    if (resetEngineOnNextBlock.exchange (false)) engine.reset();
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

double TeratoamorAudioProcessor::getTailLengthSeconds() const
{
    // Allow the longest Element release, plus repeats down to -80 dB.
    const auto p = readParams();
    double delayTail = 0.0;
    if (p.delay.mix > 0.0f)
        for (const auto& line : p.delay.lines)
            if (line.enabled)
            {
                const double feedback = std::clamp (static_cast<double> (line.decay) * 0.0095, 0.0, 0.95);
                const double repeats = feedback > 0.0 ? 1.0 + std::ceil (std::log (0.0001) / std::log (feedback)) : 1.0;
                delayTail = std::max (delayTail, DelayRates::seconds (line.rate, p.delay.bpm) * repeats);
            }
    return 20.0 + delayTail;
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
        {
            // Earlier distortion patches had no Type choice. Preserve their active
            // Drive stage (including a zero value that may later be automated).
            if (! state.getChildWithProperty ("id", ParamIDs::distortionType).isValid())
            {
                const bool hadDistortion = state.getChildWithProperty ("id", ParamIDs::distortionCrush).isValid();
                juce::ValueTree typeState { "PARAM" };
                typeState.setProperty ("id", ParamIDs::distortionType, nullptr);
                typeState.setProperty ("value", hadDistortion ? 1.0f : 0.0f, nullptr);
                state.addChild (typeState, -1, nullptr);
            }
            // Retired type index 2 now falls back to Drive.
            auto typeState = state.getChildWithProperty ("id", ParamIDs::distortionType);
            if (static_cast<float> (typeState.getProperty ("value")) > 1.0f)
                typeState.setProperty ("value", 1.0f, nullptr);
            // Old projects must not inherit delay values from a previously loaded patch.
            auto addMissing = [&state] (const juce::String& id, float value)
            {
                if (state.getChildWithProperty ("id", id).isValid()) return;
                juce::ValueTree parameter { "PARAM" };
                parameter.setProperty ("id", id, nullptr);
                parameter.setProperty ("value", value, nullptr);
                state.addChild (parameter, -1, nullptr);
            };
            addMissing (ParamIDs::delayMix, 25.0f);
            addMissing (ParamIDs::delayCut, 50.0f);
            for (int d = 0; d < 2; ++d)
            {
                addMissing (ParamIDs::delayOn (d), 0.0f);
                addMissing (ParamIDs::delayRate (d), static_cast<float> (DelayRates::defaultRate));
                addMissing (ParamIDs::delayDecay (d), 35.0f);
                addMissing (ParamIDs::delayPan (d), 0.0f);
            }
            apvts.replaceState (state);
            resetEngineOnNextBlock.store (true);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TeratoamorAudioProcessor();
}
