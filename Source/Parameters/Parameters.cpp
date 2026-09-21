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
            makeID (ParamIDs::warp (e), versionHint3), prefix + "Warp",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        if (e != 1)   // Element 2 has no Clip control
            layout.add (std::make_unique<AudioParameterFloat> (
                makeID (ParamIDs::clip (e), versionHint3), prefix + "Clip",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::width (e)), prefix + "Width",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 90.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::time (e), versionHint4), prefix + "Time",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::decay (e), versionHint4), prefix + "Decay",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::sustain (e), versionHint4), prefix + "Sustain",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::attack (e)), prefix + "Attack",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::release (e)), prefix + "Release",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 5.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::level (e)), prefix + "Level",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            makeID (ParamIDs::pan (e)), prefix + "Pan",
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    }

    return layout;
}
