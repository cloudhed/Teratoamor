#pragma once

#include "EngineParams.h"

#include <algorithm>
#include <cmath>

// The global multimode filter: a stereo trapezoidal (TPT) state-variable filter that shapes
// the summed Elements. It is a textbook design, an original implementation of the standard
// low-pass, high-pass, band-pass, band-reject, and peak responses.
//
// The Cutoff and Q mappings below are first estimates from the panel's 0..100 controls, not
// fitted to recordings. The filter runs in every mode (Bypass just outputs the input), so
// switching type never starts from stale or empty state and does not click.
class GlobalFilter
{
public:
    static constexpr float minCutoffHz = 20.0f;
    static constexpr float maxCutoffHz = 20000.0f;
    static constexpr float minQ = 0.70710678f;   // Butterworth: no resonant peak
    static constexpr float maxQ = 25.0f;
    static constexpr float peakGain = 3.0f;      // Peak type: about +12 dB at the centre

    // Cutoff 0..1 -> 20 Hz .. 20 kHz, exponential so equal knob steps are equal musical steps.
    static float cutoffToHz (float cutoff01) noexcept
    {
        return minCutoffHz * std::pow (maxCutoffHz / minCutoffHz, std::clamp (cutoff01, 0.0f, 1.0f));
    }

    // Q 0..1 -> 0.71 .. 25, exponential.
    static float knobToQ (float q01) noexcept
    {
        return minQ * std::pow (maxQ / minQ, std::clamp (q01, 0.0f, 1.0f));
    }

    void reset() noexcept
    {
        for (auto& c : channel)
            c = {};
    }

    // Recomputes coefficients. Call every few samples while Cutoff/Q are moving.
    void setParameters (GlobalFilterType newType, float cutoff01, float q01, double sampleRate) noexcept
    {
        type = newType;

        const float nyquistLimit = 0.45f * static_cast<float> (sampleRate);
        const float f = std::min (cutoffToHz (cutoff01), nyquistLimit);

        const float g = std::tan (3.14159265358979f * f / static_cast<float> (sampleRate));
        k = 1.0f / knobToQ (q01);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // Filters numSamples of left/right in place.
    void process (float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  = channel[0].process (left[i], *this);
            right[i] = channel[1].process (right[i], *this);
        }
    }

private:
    struct Channel
    {
        float ic1eq = 0.0f, ic2eq = 0.0f;

        float process (float x, const GlobalFilter& f) noexcept
        {
            const float v3 = x - ic2eq;
            const float v1 = f.a1 * ic1eq + f.a2 * v3;             // band-pass
            const float v2 = ic2eq + f.a2 * ic1eq + f.a3 * v3;     // low-pass
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;

            // Guard against instability: a non-finite or runaway state is cleared.
            if (! (std::abs (ic1eq) < 1.0e6f && std::abs (ic2eq) < 1.0e6f))
            {
                *this = {};
                return 0.0f;
            }

            switch (f.type)
            {
                case GlobalFilterType::bypass:     return x;
                case GlobalFilterType::lowpass:    return v2;
                case GlobalFilterType::highpass:   return x - f.k * v1 - v2;
                case GlobalFilterType::bandpass:   return f.k * v1;              // unity gain at the centre
                case GlobalFilterType::bandreject: return x - f.k * v1;
                case GlobalFilterType::peak:       return x + peakGain * f.k * v1;
            }

            return x;
        }
    };

    GlobalFilterType type = GlobalFilterType::bypass;
    float k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    Channel channel[2];
};
