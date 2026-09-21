#pragma once

#include "EngineParams.h"
#include "ResonantBandpass.h"

#include <cmath>

// The filter section of one Element: turns white noise (uniform, variance 1/3) into the
// selected tonal character. Every mode is level-normalised to roughly unit RMS.
//
// The narrow and peak variants are hypotheses fitted to spectra of the reference recordings
// (see docs/CHIMERA_REFERENCE.md); they are original, general-purpose filter structures.
class ElementFilter
{
public:
    // Narrow modes: two cascaded band-passes (steeper skirts). The stage Q is a fraction of the
    // Width-derived Q, fitted to the reference skirts.
    static constexpr float bpNarrowStageQScale   = 0.3f;
    static constexpr float peakNarrowStageQScale = 0.8f;

    // Peak modes: band-pass plus some unfiltered noise, as a gain on unit-RMS white noise.
    static constexpr float peakWideDryGain   = 0.36f;
    static constexpr float peakNarrowDryGain = 0.003f;

    void reset() noexcept
    {
        stageA.reset();
        stageB.reset();
    }

    // Switching modes clears the filter state so no stale resonance leaks into the new mode.
    void setMode (FilterMode newMode) noexcept
    {
        if (newMode == mode)
            return;

        mode = newMode;
        reset();
    }

    FilterMode getMode() const noexcept { return mode; }

    // Recomputes coefficients. Call every few samples while frequency/width are moving.
    void setParameters (float frequencyHz, float q, double sampleRate) noexcept
    {
        switch (mode)
        {
            case FilterMode::bpWide:
            case FilterMode::peakWide:
                stageA.setParameters (frequencyHz, q, sampleRate);
                break;

            case FilterMode::bpNarrow:
            case FilterMode::peakNarrow:
            {
                const float scale = mode == FilterMode::bpNarrow ? bpNarrowStageQScale : peakNarrowStageQScale;
                stageA.setParameters (frequencyHz, q * scale, sampleRate);
                stageB.setParameters (frequencyHz, q * scale, sampleRate);

                // Two identical stages halve the noise bandwidth, so the level compensation
                // grows by sqrt(2) relative to a single stage.
                cascadeMakeUp = stageA.getMakeUp() * 1.41421356f;
                break;
            }

            case FilterMode::off:
            case FilterMode::bypass:
                break;
        }
    }

    float process (float noise) noexcept
    {
        constexpr float unitRmsScale = 1.7320508f;   // sqrt(3): unit-RMS white noise

        switch (mode)
        {
            case FilterMode::off:        return 0.0f;
            case FilterMode::bypass:     return noise * unitRmsScale;
            case FilterMode::bpWide:     return stageA.process (noise);
            case FilterMode::bpNarrow:   return cascade (noise);
            case FilterMode::peakWide:   return stageA.process (noise) + peakWideDryGain * unitRmsScale * noise;
            case FilterMode::peakNarrow: return cascade (noise) + peakNarrowDryGain * unitRmsScale * noise;
        }

        return 0.0f;
    }

private:
    float cascade (float noise) noexcept
    {
        return stageB.processUnity (stageA.processUnity (noise)) * cascadeMakeUp;
    }

    FilterMode mode = FilterMode::bpWide;
    ResonantBandpass stageA, stageB;
    float cascadeMakeUp = 1.0f;
};
