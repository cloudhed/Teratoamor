#pragma once

#include <juce_core/juce_core.h>

// Host parameter IDs. Once a build has been released these strings must never change,
// because hosts store them in saved projects and automation lanes.
namespace ParamIDs
{
    constexpr int numElements = 3;

    inline const juce::String masterLevel { "master_level" };

    // Global filter. Added in parameter version 6.
    inline const juce::String filterType   { "gf_type" };
    inline const juce::String filterCutoff { "gf_cutoff" };
    inline const juce::String filterQ      { "gf_q" };

    inline const juce::String distortionType { "dist_type" };
    inline const juce::String distortionCrush { "dist_crush" };
    inline const juce::String distortionTone  { "dist_tone" };

    inline const juce::String delayMix { "delay_mix" };
    inline const juce::String delayCut { "delay_cut" };
    inline juce::String delayOn (int d) { return "delay" + juce::String (d + 1) + "_on"; }
    inline juce::String delayRate (int d) { return "delay" + juce::String (d + 1) + "_rate"; }
    inline juce::String delayDecay (int d) { return "delay" + juce::String (d + 1) + "_decay"; }
    inline juce::String delayPan (int d) { return "delay" + juce::String (d + 1) + "_pan"; }

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
    // Added in parameter version 3 (Clip exists on Elements 1 and 3 only).
    inline juce::String warp     (int e) { return element (e, "warp"); }
    inline juce::String clip     (int e) { return element (e, "clip"); }
    // Added in parameter version 4 (envelope stages).
    // Added in parameter version 5 (Elements 2 and 3 only).
    inline juce::String link     (int e) { return element (e, "link"); }
    inline juce::String time     (int e) { return element (e, "time"); }
    inline juce::String decay    (int e) { return element (e, "decay"); }
    inline juce::String sustain  (int e) { return element (e, "sustain"); }
    inline juce::String width    (int e) { return element (e, "width"); }
    inline juce::String attack   (int e) { return element (e, "attack"); }
    inline juce::String release  (int e) { return element (e, "release"); }
    inline juce::String level    (int e) { return element (e, "level"); }
    inline juce::String pan      (int e) { return element (e, "pan"); }
}
