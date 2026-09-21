#include "Parameters.h"
#include "ParameterIDs.h"
#include "DSP/EngineParams.h"

namespace
{
    // Version hint 1 = first released parameter set. Parameters added later use a higher number.
    constexpr int versionHint = 1;
    constexpr int versionHint2 = 2;   // Milestone 2: Element filter mode
    constexpr int versionHint3 = 3;   // Milestone 2: Warp and Clip
    constexpr int versionHint4 = 4;   // Milestone 2: Time, Decay, Sustain
    constexpr int versionHint5 = 5;   // Milestone 2: Link
    constexpr int versionHint6 = 6;   // Milestone 3: global filter

    juce::ParameterID makeID (const juce::String& id, int version = versionHint)
    {
        return { id, version };
    }

    juce::String percentText (float value, int)
    {
        return juce::String (juce::roundToInt (value));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createLayout()
{
    using juce::AudioParameterBool;
    using juce::AudioParameterChoice;
    using juce::AudioParameterFloat;
    using juce::AudioParameterInt;
    using juce::NormalisableRange;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::masterLevel), "Master Level",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    // Global filter. Choice order matches the GlobalFilterType enum and is stored by hosts; never reorder it.
    layout.add (std::make_unique<AudioParameterChoice> (
        makeID (ParamIDs::filterType, versionHint6), "Filter Type",
        juce::StringArray { "Bypass", "Lowpass", "Highpass", "Bandpass", "Bandreject", "Peak" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::filterQ, versionHint6), "Filter Q",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::filterCutoff, versionHint6), "Filter Cutoff",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::distortionCrush, 7), "Distortion Drive",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::distortionTone, 7), "Distortion Tone",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    for (int e = 0; e < ParamIDs::numElements; ++e)
    {
        const auto prefix = "Element " + juce::String (e + 1) + " ";

        // Reference default patch: only Element 1 sounds, Width 90, Release ~0.5 s.
        layout.add (std::make_unique<AudioParameterBool> (
            makeID (ParamIDs::enabled (e)), prefix + "On", e == 0));

        // Choice order matches the FilterMode enum and is stored by hosts; never reorder it.
        layout.add (std::make_unique<AudioParameterChoice> (
            makeID (ParamIDs::filter (e), versionHint2), prefix + "Filter",
            juce::StringArray { "Off", "Bypass", "BP Wide", "BP Narrow", "Peak Wide", "Peak Narrow" },
            static_cast<int> (FilterMode::bpWide)));

        layout.add (std::make_unique<AudioParameterInt> (
            makeID (ParamIDs::octave (e)), prefix + "Octave", -4, 4, 0));

        layout.add (std::make_unique<AudioParameterInt> (
            makeID (ParamIDs::semitone (e)), prefix + "Semitone", -12, 12, 0));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::fine (e)), prefix + "Fine (cents)",
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::width (e)), prefix + "Width",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 90.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::warp (e), versionHint3), prefix + "Warp",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        if (e != 1)   // Element 2 has no Clip control
            layout.add (std::make_unique<AudioParameterFloat> (
                makeID (ParamIDs::clip (e), versionHint3), prefix + "Clip",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::level (e)), prefix + "Level",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::pan (e)), prefix + "Pan",
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));

        // Envelope, in the usual order: Link (Elements 2 and 3 only), Attack, Decay, Sustain, Release, Time.
        // Element 1 has no Link: it is the source the others follow. Link is on by default.
        if (e != 0)
            layout.add (std::make_unique<AudioParameterBool> (
                makeID (ParamIDs::link (e), versionHint5), prefix + "Link", true));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::attack (e)), prefix + "Attack",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::decay (e), versionHint4), prefix + "Decay",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::sustain (e), versionHint4), prefix + "Sustain",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::release (e)), prefix + "Release",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 5.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::time (e), versionHint4), prefix + "Time",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    }

    layout.add (std::make_unique<AudioParameterChoice> (
        makeID (ParamIDs::distortionType, 8), "Distortion Type",
        juce::StringArray { "Bypass", "Drive" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::delayMix, 9), "Delay Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::delayCut, 9), "Delay Cut", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    juce::StringArray rates;
    for (const auto& rate : DelayRates::values) rates.add (rate.name);
    for (int d = 0; d < 2; ++d)
    {
        const auto prefix = "Delay " + juce::String (d + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool> (makeID (ParamIDs::delayOn (d), 9), prefix + "On", false));
        layout.add (std::make_unique<AudioParameterChoice> (
            makeID (ParamIDs::delayRate (d), 9), prefix + "Rate", rates, DelayRates::defaultRate));
        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::delayDecay (d), 9), prefix + "Decay", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::delayPan (d), 9), prefix + "Pan", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    }

    // Master pan and the six modulation sections (parameter version 10).
    layout.add (std::make_unique<AudioParameterFloat> (
        makeID (ParamIDs::masterPan, 10), "Master Pan", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));

    const juce::StringArray waves { "Off", "Sine", "Triangle", "Saw", "Peak", "Dip", "Hump", "RipSaw1", "RipSaw2", "Ramp",
                                    "RipRamp1", "RipRamp2", "SharkR", "SharkL", "Pulse100%", "Pulse50%", "Pulse25%", "Random" };
    jassert (waves.size() == static_cast<int> (ModWave::count));

    for (int m = 0; m < ParamIDs::numMods; ++m)
    {
        const auto prefix = "Modulation " + juce::String (m + 1) + " ";
        const auto id = [m] (const char* name) { return makeID (ParamIDs::mod (m, name), 10); };
        const auto units = NormalisableRange<float> (0.0f, 100.0f, 0.1f);

        // The choice index refers to this section's own list (see ModTargets::listFor).
        const auto targets = ModTargets::listFor (m);
        const auto defaultTarget = m == 3 ? ModTarget::el1Pitch : ModTarget::el1Width;
        juce::StringArray targetNames;
        int defaultIndex = 0;

        for (int i = 0; i < targets.size; ++i)
        {
            targetNames.add (ModTargets::names[(size_t) targets.items[(size_t) i]]);
            if (targets.items[(size_t) i] == defaultTarget)
                defaultIndex = i;
        }

        layout.add (std::make_unique<AudioParameterChoice> (id ("target"), prefix + "Target", targetNames, defaultIndex));
        layout.add (std::make_unique<AudioParameterFloat> (
            id ("depth"), prefix + "Depth", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));

        if (m < 3)
        {
            layout.add (std::make_unique<AudioParameterFloat> (id ("attack"), prefix + "Attack", units, 0.0f));
            layout.add (std::make_unique<AudioParameterFloat> (id ("decay"), prefix + "Decay", units, 0.0f));
            layout.add (std::make_unique<AudioParameterFloat> (id ("sustain"), prefix + "Sustain", units, 100.0f));
            layout.add (std::make_unique<AudioParameterFloat> (id ("release"), prefix + "Release", units, 5.0f));
            layout.add (std::make_unique<AudioParameterFloat> (id ("time"), prefix + "Time", units, 50.0f));
        }
        else
        {
            layout.add (std::make_unique<AudioParameterChoice> (id ("wave"), prefix + "Wave", waves, 0));
            layout.add (std::make_unique<AudioParameterBool> (id ("gate"), prefix + "Gate Trig", false));
            layout.add (std::make_unique<AudioParameterFloat> (id ("soft"), prefix + "Soft", units, 0.0f));
            layout.add (std::make_unique<AudioParameterChoice> (id ("rate"), prefix + "Rate", rates, 7));
        }
    }

    return layout;
}
