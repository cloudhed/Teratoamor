#pragma once

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
struct ElementParams
{
    bool  enabled  = false;
    FilterMode filterMode = FilterMode::bpWide;
    int   octave   = 0;
    int   semitone = 0;
    float fineCents = 0.0f;
    float width    = 90.0f;    // 0..100
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
