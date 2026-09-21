#pragma once

#include <algorithm>
#include <cmath>

// The Warp and Clip stages of one Element. They sit after the filter and before the envelope,
// working on a signal that is normalised to roughly unit RMS, so their character does not
// change with velocity, level, or the envelope position.
//
// Both curves are original designs fitted to measurements of the reference recordings
// (see docs/CHIMERA_REFERENCE.md):
//   Warp: asymmetric waveshaper  y = x + a * (x^2 - mean(x^2)), which adds even harmonics.
//   Clip: drive followed by a hard clip at a fixed ceiling; barely active below Clip 75.
class ElementShaper
{
public:
    // Warp adds even harmonics with two terms: a squared term (mostly the 2nd harmonic) and a
    // rectified term |x| (2nd, 4th, 6th... a fuller ladder). Both are deliberately stronger than
    // the reference recordings, and the curve is concave so the lower half of the control
    // already does something audible.
    static constexpr float warpAmountAtMax = 0.8f;
    static constexpr float warpCurve = 0.75f;
    static constexpr float warpRectifiedShare = 0.5f;   // 0 = squared only, 1 = rectified only
    static constexpr float rectifiedScale = 2.35f;      // makes |x| as large as x^2 for unit-RMS noise

    // Clip drive in dB is clipMaxDriveDb * clip^clipCurve (clip 0..1). Peaks first reach the
    // ceiling at about +6 dB of drive, so a linear curve starts clipping near Clip 45. (The
    // reference stays clean until about Clip 90; this is deliberately earlier.)
    // The ceiling is in units of the filter's RMS output.
    static constexpr float clipMaxDriveDb = 14.0f;
    static constexpr float clipCurve = 1.0f;
    static constexpr float clipCeiling = 5.7f;

    // Loudness compensation: the output is scaled by 1 / (RMS after clipping)^clipCompensation,
    // where that RMS is calculated for Gaussian-like input. 1 would hold the level exactly
    // constant; a little less lets heavy clipping stay slightly louder, as it does in the
    // reference (+12 dB there, deliberately reduced to about +2 dB here).
    static constexpr float clipCompensation = 0.8f;

    void prepare (double sampleRate) noexcept
    {
        // A slow one-pole tracks the average of x^2 so the squared term carries no DC offset.
        meanCoefficient = 1.0f - std::exp (-2.0f * 3.14159265f * 8.0f / static_cast<float> (sampleRate));
        reset();
    }

    // Mean of x^2 (1) and of |x| (sqrt(2/pi)) for a unit-RMS signal.
    void reset() noexcept { mean = 1.0f; meanAbs = 0.7978846f; }

    // warp01 and clip01 are 0..1. Call once per control chunk.
    void setParameters (float warp01, float clip01) noexcept
    {
        warpAmount = warpAmountAtMax * std::pow (std::clamp (warp01, 0.0f, 1.0f), warpCurve);
        warpNormalise = 1.0f / std::sqrt (1.0f + 2.0f * warpAmount * warpAmount);   // keep RMS steady

        const float c = std::clamp (clip01, 0.0f, 1.0f);

        if (c != lastClip)   // the maths below is only redone when the control moves
        {
            lastClip = c;
            clipGain = std::pow (10.0f, clipMaxDriveDb * std::pow (c, clipCurve) / 20.0f);

            // RMS of clamp (gain * x, +/-ceiling) for x ~ N(0, 1), with T = ceiling / gain:
            //   E[y^2] = gain^2 * (erf (T/sqrt2) - sqrt (2/pi) * T * exp (-T^2/2)) + ceiling^2 * (1 - erf (T/sqrt2))
            const float t = clipCeiling / clipGain;
            const float inside = std::erf (t * 0.70710678f);
            const float meanSquare = clipGain * clipGain * (inside - 0.79788456f * t * std::exp (-0.5f * t * t))
                                   + clipCeiling * clipCeiling * (1.0f - inside);
            clipMakeUp = std::pow (std::max (meanSquare, 1.0e-6f), -0.5f * clipCompensation);
        }
    }

    float process (float x) noexcept
    {
        const float squared = x * x;
        const float magnitude = std::abs (x);
        mean += (squared - mean) * meanCoefficient;
        meanAbs += (magnitude - meanAbs) * meanCoefficient;

        const float bend = (1.0f - warpRectifiedShare) * (squared - mean)
                         + warpRectifiedShare * rectifiedScale * (magnitude - meanAbs);
        float y = (x + warpAmount * bend) * warpNormalise;

        y = std::clamp (y * clipGain, -clipCeiling, clipCeiling) * clipMakeUp;

        if (! (std::abs (mean) < 1.0e6f && std::abs (meanAbs) < 1.0e6f))   // NaN/infinity guard
        {
            reset();
            return 0.0f;
        }

        return y;
    }

private:
    float meanCoefficient = 0.001f, mean = 1.0f, meanAbs = 0.7978846f;
    float warpAmount = 0.0f, warpNormalise = 1.0f, clipGain = 1.0f, clipMakeUp = 1.0f, lastClip = 0.0f;
};
