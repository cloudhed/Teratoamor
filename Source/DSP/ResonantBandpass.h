#pragma once

#include <algorithm>
#include <cmath>

// Resonant band-pass built as a trapezoidal (TPT) state-variable filter.
// This topology stays stable for any positive frequency/Q, which matters because Width
// near 100 maps to extremely high Q. Non-finite state is detected and reset.
class ResonantBandpass
{
public:
    // Width 0..1 -> Q = 1 / (1 - width)^2 (measured mapping), with the denominator clamped
    // so Q never exceeds maxQ.
    static constexpr float minOneMinusWidth = 0.025f;   // -> maxQ = 1600
    static constexpr float maxQ = 1.0f / (minOneMinusWidth * minOneMinusWidth);

    static float widthToQ (float width01) noexcept
    {
        const float d = std::max (minOneMinusWidth, 1.0f - std::min (width01, 1.0f));
        return 1.0f / (d * d);
    }

    void reset() noexcept { ic1eq = 0.0f; ic2eq = 0.0f; }

    // Recomputes coefficients. Call every few samples while frequency/width are moving.
    void setParameters (float frequencyHz, float q, double sampleRate) noexcept
    {
        const float nyquistLimit = 0.45f * static_cast<float> (sampleRate);
        const float f = std::min (std::max (frequencyHz, 10.0f), nyquistLimit);
        q = std::min (std::max (q, 0.5f), maxQ);

        const float g = std::tan (3.14159265358979f * f / static_cast<float> (sampleRate));
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;

        // Noise passed through a unity-peak band-pass loses level as the band narrows.
        // Compensate so the output RMS stays roughly constant for a unit-variance-1/3 noise input.
        const float noiseBandwidth = std::min (1.0f, 3.14159265f * f / (q * static_cast<float> (sampleRate)));
        makeUp = 1.7320508f / std::sqrt (noiseBandwidth);   // sqrt(3) / sqrt(bandwidth)
    }

    // Returns the band-pass output scaled to a level-normalised result.
    float process (float x) noexcept
    {
        const float v3 = x - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        if (! (std::abs (ic1eq) < 1.0e6f && std::abs (ic2eq) < 1.0e6f))   // also catches NaN
        {
            reset();
            return 0.0f;
        }

        return k * v1 * makeUp;
    }

private:
    float k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, makeUp = 1.0f;
    float ic1eq = 0.0f, ic2eq = 0.0f;
};
