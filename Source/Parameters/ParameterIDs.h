#pragma once

#include <juce_core/juce_core.h>

// Host parameter IDs. Once a build has been released these strings must never change,
// because hosts store them in saved projects and automation lanes.
namespace ParamIDs
{
    constexpr int numElements = 3;

    inline const juce::String masterLevel { "master_level" };

    // Per-Element IDs are "el<N>_<name>", where N is 1..3.
    inline juce::String element (int elementIndex, const char* name)
    {
        return "el" + juce::String (elementIndex + 1) + "_" + name;
    }

    inline juce::String enabled  (int e) { return element (e, "on"); }
    inline juce::String octave   (int e) { return element (e, "octave"); }
    inline juce::String semitone (int e) { return element (e, "semitone"); }
    inline juce::String fine     (int e) { return element (e, "fine"); }
    // Added in parameter version 2.
    inline juce::String filter   (int e) { return element (e, "filter"); }
    inline juce::String width    (int e) { return element (e, "width"); }
    inline juce::String attack   (int e) { return element (e, "attack"); }
    inline juce::String release  (int e) { return element (e, "release"); }
    inline juce::String level    (int e) { return element (e, "level"); }
    inline juce::String pan      (int e) { return element (e, "pan"); }
}
