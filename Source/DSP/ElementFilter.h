#pragma once

#include "EngineParams.h"
#include "ResonantBandpass.h"

#include <algorithm>
#include <array>
#include <cmath>

// The filter section of one Element: turns white noise (uniform, variance 1/3) into the
// selected tonal character. Internal compensation prevents uncontrolled level changes, followed
// by a measured per-mode Width gain curve that preserves the reference instrument's musical rise.
//
// The narrow and peak variants are hypotheses fitted to spectra of the reference recordings
// (see docs/CHIMERA_REFERENCE.md); they are original, general-purpose filter structures.
class ElementFilter
{
public:
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
    void setParameters (float frequencyHz, float width01, double sampleRate) noexcept
    {
        width01 = std::clamp (width01, 0.0f, 1.0f);
        const float baseQ = ResonantBandpass::widthToQ (width01);
        outputGain = widthGain (mode, width01);
        dryGain = (mode == FilterMode::peakWide || mode == FilterMode::peakNarrow)
            ? peakDryGain (mode, width01) : 0.0f;

        switch (mode)
        {
            case FilterMode::bpWide:
                // The measured skirts change by only 3-4 dB between Width 90 and 100, so the
                // resonance stops narrowing well before the base mapping's maximum.
                stageA.setParameters (frequencyHz, std::min (baseQ, 400.0f), sampleRate);
                break;

            case FilterMode::peakWide:
                // The measured Peak Wide response stops narrowing above about Width 90.
                stageA.setParameters (frequencyHz, std::min (baseQ, 100.0f), sampleRate);
                break;

            case FilterMode::bpNarrow:
            case FilterMode::peakNarrow:
            {
                // Narrow modes use independent measured curves. A constant multiplier matches
                // near Width 90 but is much too broad below it and much too narrow at 100.
                const float stageQ = narrowStageQ (mode, width01);
                stageA.setParameters (frequencyHz, stageQ, sampleRate);
                stageB.setParameters (frequencyHz, stageQ, sampleRate);

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
            case FilterMode::bpWide:     return stageA.process (noise) * outputGain;
            case FilterMode::bpNarrow:   return cascade (noise) * outputGain;
            case FilterMode::peakWide:   return (stageA.process (noise) + dryGain * unitRmsScale * noise) * outputGain;
            // The two-stage cascade is ~180 degrees out of phase with the input on both skirts,
            // so the dry noise is subtracted: adding it cancels the skirts into a notch.
            case FilterMode::peakNarrow: return (dryGain * unitRmsScale * noise - cascade (noise)) * outputGain;
        }

        return 0.0f;
    }

private:
    static float interpolate (float width01, const std::array<float, 6>& values, bool logarithmic) noexcept
    {
        constexpr std::array<float, 6> points {{ 0.0f, 0.25f, 0.50f, 0.75f, 0.90f, 1.0f }};
        const auto upper = std::upper_bound (points.begin(), points.end(), width01);
        if (upper == points.begin()) return values.front();
        if (upper == points.end()) return values.back();

        const size_t high = static_cast<size_t> (upper - points.begin());
        const size_t low = high - 1;
        const float t = (width01 - points[low]) / (points[high] - points[low]);
        if (logarithmic)
            return std::exp (std::log (values[low]) + t * (std::log (values[high]) - std::log (values[low])));
        return values[low] + t * (values[high] - values[low]);
    }

    static float narrowStageQ (FilterMode filterMode, float width01) noexcept
    {
        // Equivalent per-stage Q fitted at Width 0, 25, 50, 75, 90 and 100.
        static constexpr std::array<float, 6> bp {{ 1.05f, 1.75f, 3.0f, 8.1f, 37.0f, 150.0f }};
        static constexpr std::array<float, 6> peak {{ 2.8f, 7.1f, 18.0f, 45.0f, 100.0f, 180.0f }};
        return interpolate (width01, filterMode == FilterMode::bpNarrow ? bp : peak, true);
    }

    static float peakDryGain (FilterMode filterMode, float width01) noexcept
    {
        // The unfiltered component was refitted after the resonance curves. Peak Narrow's
        // reference floor rises around Width 90 and falls again at 100; one constant could
        // not reproduce both points. Peak Wide needs only smaller corrections.
        static constexpr std::array<float, 6> wide {{ 0.375f, 0.317f, 0.446f, 0.249f, 0.296f, 0.375f }};
        static constexpr std::array<float, 6> narrow {{ 0.53f, 0.154f, 0.0376f, 0.0092f, 0.0023f, 0.0014f }};
        return interpolate (width01, filterMode == FilterMode::peakWide ? wide : narrow, true);
    }

    static float widthGain (FilterMode filterMode, float width01) noexcept
    {
        // dB correction relative to Width 90. This retains the accepted default-patch level
        // while reproducing the measured change in loudness as Width moves away from 90.
        static constexpr std::array<float, 6> bpWideDb {{ -6.3f, -4.6f, -3.8f, 0.2f, 0.0f, 4.4f }};
        static constexpr std::array<float, 6> bpNarrowDb {{ -6.8f, -3.1f, -2.2f, -4.4f, 0.0f, 2.3f }};
        static constexpr std::array<float, 6> peakWideDb {{ -5.0f, -5.8f, -5.3f, -1.9f, 0.0f, -0.2f }};
        static constexpr std::array<float, 6> peakNarrowDb {{ -8.7f, -5.5f, -3.4f, 0.5f, 0.0f, -4.8f }};

        const std::array<float, 6>* curve = nullptr;
        switch (filterMode)
        {
            case FilterMode::bpWide:     curve = &bpWideDb; break;
            case FilterMode::bpNarrow:   curve = &bpNarrowDb; break;
            case FilterMode::peakWide:   curve = &peakWideDb; break;
            case FilterMode::peakNarrow: curve = &peakNarrowDb; break;
            case FilterMode::off:
            case FilterMode::bypass:     return 1.0f;
        }

        const float decibels = interpolate (width01, *curve, false);
        return std::pow (10.0f, decibels / 20.0f);
    }

    float cascade (float noise) noexcept
    {
        return stageB.processUnity (stageA.processUnity (noise)) * cascadeMakeUp;
    }

    FilterMode mode = FilterMode::bpWide;
    ResonantBandpass stageA, stageB;
    float cascadeMakeUp = 1.0f;
    float outputGain = 1.0f;
    float dryGain = 0.0f;
};
