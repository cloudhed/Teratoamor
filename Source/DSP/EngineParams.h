#pragma once

#include "DelayRates.h"
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

// Global filter type. The order is stored by the host as a choice index and must not change.
enum class GlobalFilterType
{
    bypass = 0,
    lowpass,
    highpass,
    bandpass,
    bandreject,
    peak
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
    bool  link     = true;     // Elements 2 and 3 only: use Element 1's Attack, Decay, Sustain, Release, and Time
    float time     = 50.0f;    // 0..100: Decay and Release length. 50 = normal, 0 = nearly instant, 100 = twice as long
    float decay    = 0.0f;     // 0..100 (0.1 seconds per unit at Time 50)
    float sustain  = 100.0f;   // 0..100 (percent of full level)
    float attack   = 0.0f;     // 0..100 (control value; 10 units = 1 second)
    float release  = 5.0f;     // 0..100
    float level    = 80.0f;    // 0..100
    float pan      = 0.0f;     // -100..100
};

struct GlobalFilterParams
{
    GlobalFilterType type = GlobalFilterType::bypass;
    float cutoff = 100.0f;   // 0..100
    float q      = 0.0f;     // 0..100
};

// Host choice order: never reorder.
enum class DistortionType { bypass = 0, drive };

struct DistortionParams
{
    float crush = 0.0f;    // 0..100; zero bypasses the whole stage
    float tone = 50.0f;    // 0..100; 50 is neutral
    DistortionType type = DistortionType::bypass;
};

struct DelayLineParams
{
    bool enabled = false;
    int rate = DelayRates::defaultRate;
    float decay = 35.0f;   // 0..100 maps to 0..0.95 feedback
    float pan = 0.0f;      // -100 left, 0 mono centre, +100 right
};

struct DelayParams
{
    float mix = 25.0f;     // 0 dry, 100 wet; both lines off bypasses the block
    float cut = 50.0f;     // 0 high-cut, 50 unchanged, 100 low-cut; wet only
    double bpm = 120.0;    // host tempo, or standalone fallback
    std::array<DelayLineParams, 2> lines;
};

// Everything a modulation section can drive. Hosts store each section's choice index,
// so new targets must be appended and listFor must preserve old indices.
enum class ModTarget
{
    el1Width = 0, el1Pitch, el1Pan, el1Warp, el1Clip,
    el2Width, el2Pitch, el2Pan, el2Warp,
    el3Width, el3Pitch, el3Pan, el3Warp,
    allWidth, allPitch, allPan, allVolume, allWarp,
    distortCrush, distortTone,
    filterCutoff, filterQ,
    delayPan, delayFilter,
    mod1Depth, mod2Depth, mod3Depth,
    mod4Rate, mod5Rate, mod6Rate,
    masterPan, masterVolume,
    el1Volume, el2Volume, el3Volume,
    count
};

// Oscillator wavetables for Modulation 4-6. Host choice order: never reorder.
enum class ModWave
{
    off = 0, sine, triangle, saw, peak, dip, hump, ripSaw1, ripSaw2, ramp,
    ripRamp1, ripRamp2, sharkR, sharkL, pulse100, pulse50, pulse25, random,
    count
};

namespace ModTargets
{
    inline constexpr int count = static_cast<int> (ModTarget::count);
    inline constexpr std::array<const char*, count> names {{
        "Element1 Width", "Element1 Pitch", "Element1 Pan", "Element1 Warp", "Element1 Clip",
        "Element2 Width", "Element2 Pitch", "Element2 Pan", "Element2 Warp",
        "Element3 Width", "Element3 Pitch", "Element3 Pan", "Element3 Warp",
        "All Width", "All Pitch", "All Pan", "All Volume", "All Warp",
        "Distort Drive", "Distort Tone", "Filter Cutoff", "Filter Q",
        "Delay Pan", "Delay Filter",
        "Modulation1 Depth", "Modulation2 Depth", "Modulation3 Depth",
        "Modulation4 Rate", "Modulation5 Rate", "Modulation6 Rate",
        "Master Pan", "Master Volume",
        "Element1 Volume", "Element2 Volume", "Element3 Volume" }};

    // The dropdown of one section (0..2 = envelopes, 3..5 = oscillators). The host stores an
    // index into this list, so the order must not change once released.
    struct List
    {
        std::array<ModTarget, count> items {};
        int size = 0;
    };

    inline List listFor (int section) noexcept
    {
        List list;
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<ModTarget> (i);

            if (section < 3)   // envelopes: Elements, All, Filter
            {
                if (t == ModTarget::distortCrush || t == ModTarget::distortTone)
                    continue;
                if (i > static_cast<int> (ModTarget::filterQ) && i < static_cast<int> (ModTarget::el1Volume))
                    continue;
            }
            else if (i == static_cast<int> (ModTarget::mod4Rate) + (section - 3))
            {
                continue;   // an oscillator cannot modulate its own Rate
            }

            list.items[(size_t) list.size++] = t;
        }
        return list;
    }
}

// One modulation section. Sections 0..2 are envelopes (attack..time), 3..5 are oscillators
// (wave, gate, soft, rate). Depth is -100..100 and is shared by both kinds.
struct ModParams
{
    ModTarget target = ModTarget::el1Width;
    float depth = 0.0f;

    float attack = 0.0f, decay = 0.0f, sustain = 100.0f, release = 5.0f, time = 50.0f;   // 0..100, as Elements

    ModWave wave = ModWave::off;
    bool gateTrig = false;   // restart the wave on every note-on; otherwise free-running
    float soft = 0.0f;       // 0..100 smoothing of the wave
    int rate = 7;            // index into DelayRates::values; 7 = 1/16
};

struct EngineParams
{
    static constexpr int numElements = 3;
    static constexpr int numMods = 6;

    float master = 70.0f;      // 0..100
    float masterPan = 0.0f;    // -100..100
    float glide = 0.0f;        // 0..100: slide from the previous note's pitch; 0 is off
    bool glideByRate = false;  // false: Glide 100 = 2 s for any jump; true: 2 s per octave, so bigger jumps take longer
    std::array<ModParams, numMods> mods;
    std::array<ElementParams, numElements> elements;
    GlobalFilterParams globalFilter;
    DistortionParams distortion;
    DelayParams delay;
};
