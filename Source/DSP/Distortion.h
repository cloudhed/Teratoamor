#pragma once

#include "EngineParams.h"
#include <algorithm>
#include <cmath>

// Independent soft saturation, after the global filter.
// Per-sample smoothing applies to both knobs and the mode crossfade.
class Distortion
{
public:
    void prepare (double sampleRate) noexcept
    {
        smoothing = 1.0f - std::exp (-1.0f / (0.010f * static_cast<float> (sampleRate)));
        // TPT one-pole split: the high band tends to unity at Nyquist.
        const float g = std::tan (3.14159265359f * 700.0f / static_cast<float> (sampleRate));
        toneCoefficient = g / (1.0f + g);
        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : lowState) channel = 0.0f;
        snap = true;
    }

    void setParameters (float driveControl, float toneControl, DistortionType type) noexcept
    {
        targetAmount = std::isfinite (driveControl) ? std::clamp (driveControl / 100.0f, 0.0f, 1.0f) : 0.0f;
        targetTone = std::isfinite (toneControl) ? std::clamp (toneControl / 50.0f - 1.0f, -1.0f, 1.0f) : 0.0f;
        targetDriveMix = type == DistortionType::drive ? 1.0f : 0.0f;
        if (snap)
        {
            amount = targetAmount;
            tone = targetTone;
            driveMix = targetDriveMix;
            snap = false;
        }
    }

    void process (float* left, float* right, int count) noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            advance (amount, targetAmount);
            advance (tone, targetTone);
            advance (driveMix, targetDriveMix);
            const float drive = 1.0f + 9.0f * amount; // maximum 10x, previously 8x
            const float gain = (1.0f + 0.45f * amount) / drive;
            const float trebleGain = std::pow (10.0f, 0.6f * tone); // +/-12 dB
            left[i] = sample (left[i], 0, drive, gain, trebleGain);
            right[i] = sample (right[i], 1, drive, gain, trebleGain);
        }
    }

private:
    void advance (float& value, float target) const noexcept
    {
        value += smoothing * (target - value);
        if (std::abs (target - value) < 1.0e-6f) value = target;
    }

    float colour (float input, int channel, float trebleGain) noexcept
    {
        auto& state = lowState[channel];
        const float v = toneCoefficient * (input - state);
        const float low = v + state;
        state = low + v;
        if (std::abs (state) < 1.0e-20f) state = 0.0f;
        return input + (trebleGain - 1.0f) * (input - low);
    }

    float sample (float input, int channel, float drive, float gain, float trebleGain) noexcept
    {
        if (! std::isfinite (input))
        {
            lowState[channel] = 0.0f;
            return 0.0f;
        }
        const float saturated = std::tanh (input * drive) * gain;
        // Keep the tone filter warm during bypass to avoid stale tails on activation.
        const float driven = colour (saturated, channel, trebleGain);
        if (amount == 0.0f || driveMix == 0.0f) return input;
        return input + amount * driveMix * (driven - input);
    }

    float smoothing = 0.002f, toneCoefficient = 0.05f;
    float amount = 0.0f, tone = 0.0f, targetAmount = 0.0f, targetTone = 0.0f;
    float driveMix = 0.0f, targetDriveMix = 0.0f;
    float lowState[2] {};
    bool snap = true;
};
