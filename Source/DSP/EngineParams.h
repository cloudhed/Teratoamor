#pragma once

#include <algorithm>
#include <array>

// Element filter selection. The order is stored by the host as a choice index and must not change.
enum class FilterMode
{
    off = 0,        // Element is silent
    bypass,         // unfiltered white noise
    bpWide,         // resonant band-pass (the Width-derived Q)
    bpNarrow,
    peakWide,
    peakNarrow
};

// Plain-data snapshot of the user-facing controls, read once per audio block.
// It has no JUCE dependency, so the DSP can be tested without a plugin host.
// The Decay and Release lengths are multiplied by this factor (Time 50 = unchanged).
// Fitted to recordings at Time 0, 50 and 100. Release tails measured about
// 0.05 s, 0.45 s and 0.8 s at the default Release, and about 0, 2.3 s and 4.7 s at Release 25;
// a Decay 50 / Sustain 0 note faded in about 0.5 s at Time 0. No difference in onset was assumed.
inline float timeScale (float timeControl) noexcept
{
    return std::max (timeControl / 50.0f, 0.1f);   // Time 0 is not zero: about a tenth of normal
}

struct ElementParams
{
    bool  enabled  = false;
    FilterMode filterMode = FilterMode::bpWide;
    int   octave   = 0;
    int   semitone = 0;
    float fineCents = 0.0f;
    float warp     = 0.0f;     // 0..100
    float clip     = 0.0f;     // 0..100 (Element 2 has no Clip control)
    float width    = 90.0f;    // 0..100
    float time     = 50.0f;    // 0..100: Decay and Release length. 50 = normal, 0 = nearly instant, 100 = twice as long
    float decay    = 0.0f;     // 0..100 (about 0.104 seconds per unit)
    float sustain  = 100.0f;   // 0..100 (percent of full level)
    float attack   = 0.0f;     // 0..100 (control value; 10 units = 1 second)
    float release  = 5.0f;     // 0..100
    float level    = 80.0f;    // 0..100
    float pan      = 0.0f;     // -100..100
};

struct EngineParams
{
    static constexpr int numElements = 3;

    float master = 70.0f;      // 0..100
    std::array<ElementParams, numElements> elements;
};
