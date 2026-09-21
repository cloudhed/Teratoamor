#pragma once
#include "EngineParams.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Two independent mono feedback lines fed from the same stereo-to-mono send.
// Buffers are allocated only in prepare; reset invalidates history in O(1).
class ParallelDelay
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        smoothing = static_cast<float> (1.0 - std::exp (-1.0 / (0.010 * sampleRate)));
        fadeSamples = std::max (1, static_cast<int> (0.020 * sampleRate));
        const auto size = static_cast<size_t> (std::ceil (DelayRates::maximumSeconds * sampleRate)) + 2;
        for (auto& line : lines) line.buffer.assign (size, 0.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines)
        {
            line.write = line.valid = 0;
            line.fade = 0;
        }
        for (auto& channel : filters)
            for (auto& state : channel) state = 0.0f;
        snap = true;
    }

    void setParameters (const DelayParams& p) noexcept
    {
        mix.set (control (p.mix, 0.0f, 100.0f, 0.0f) * 0.01f, snap);
        const float cut = (control (p.cut, 0.0f, 100.0f, 50.0f) - 50.0f) * 0.02f;
        highCut.set (std::max (0.0f, -cut), snap);
        lowCut.set (std::max (0.0f, cut), snap);
        lp.set (coefficient (20000.0 * std::pow (0.025, std::max (0.0f, -cut))), snap);
        hp.set (coefficient (20.0 * std::pow (100.0, std::max (0.0f, cut))), snap);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto& line = lines[i];
            const auto& params = p.lines[i];
            line.enabled.set (params.enabled ? 1.0f : 0.0f, snap);
            line.feedback.set (control (params.decay, 0.0f, 100.0f, 0.0f) * 0.0095f, snap);
            const float pan = control (params.pan, -100.0f, 100.0f, 0.0f) * 0.01f;
            // Constant-power pan, unity in both channels at centre (same as Elements).
            const float angle = (pan + 1.0f) * 0.78539816339f;
            line.panL.set (pan >= 1.0f ? 0.0f : 1.41421356237f * std::cos (angle), snap);
            line.panR.set (pan <= -1.0f ? 0.0f : 1.41421356237f * std::sin (angle), snap);
            line.requested = std::clamp (DelayRates::seconds (params.rate, p.bpm) * sampleRate,
                                         1.0, DelayRates::maximumSeconds * sampleRate);
            if (snap) line.current = line.next = line.requested;
        }
        snap = false;
    }

    void process (float* left, float* right, int count) noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            mix.advance (smoothing); highCut.advance (smoothing); lowCut.advance (smoothing);
            lp.advance (smoothing); hp.advance (smoothing);
            const float dryL = std::isfinite (left[i]) ? left[i] : 0.0f;
            const float dryR = std::isfinite (right[i]) ? right[i] : 0.0f;
            const float send = 0.5f * dryL + 0.5f * dryR;
            float wetL = 0.0f, wetR = 0.0f, active = 0.0f;
            for (auto& line : lines)
            {
                line.enabled.advance (smoothing); line.feedback.advance (smoothing);
                line.panL.advance (smoothing); line.panR.advance (smoothing);
                active += line.enabled.value;
                if (line.enabled.value == 0.0f || line.buffer.empty())
                {
                    line.valid = 0; line.fade = 0;
                    line.current = line.next = line.requested;
                    continue;
                }
                // Crossfade two fixed read heads on Rate/tempo changes, no pitch glide.
                // If changes arrive during a fade, finish it and then use the latest request.
                if (line.fade == 0 && std::abs (line.requested - line.current) > 0.001)
                {
                    line.next = line.requested;
                    line.fade = fadeSamples;
                }
                float echo = read (line, line.current);
                if (line.fade > 0)
                {
                    const float blend = 1.0f - static_cast<float> (line.fade) / static_cast<float> (fadeSamples);
                    echo += blend * (read (line, line.next) - echo);
                    if (--line.fade == 0) line.current = line.next;
                }
                float stored = send * line.enabled.value + echo * line.feedback.value;
                // Bound hot input/feedback before it can poison future repeats.
                stored = std::isfinite (stored) ? std::clamp (stored, -8.0f, 8.0f) : 0.0f;
                if (std::abs (stored) < 1.0e-20f) stored = 0.0f;
                line.buffer[line.write] = stored;
                line.write = (line.write + 1) % line.buffer.size();
                line.valid = std::min (line.valid + 1, line.buffer.size() - 1);
                wetL += echo * line.enabled.value * line.panL.value;
                wetR += echo * line.enabled.value * line.panR.value;
            }
            // Average parallel returns when both are on, avoiding a doubled identical echo.
            const float normalise = 1.0f / std::max (1.0f, active);
            if (active == 0.0f)
            {
                for (auto& channel : filters)
                    for (auto& state : channel) state = 0.0f;
            }
            wetL = filter (wetL * normalise, 0);
            wetR = filter (wetR * normalise, 1);
            const float dryGain = 1.0f - mix.value * std::min (1.0f, active);
            left[i] = dryL * dryGain + mix.value * wetL;
            right[i] = dryR * dryGain + mix.value * wetR;
        }
    }

private:
    struct Smooth
    {
        float value = 0.0f, target = 0.0f;
        void set (float v, bool snapNow) noexcept { target = v; if (snapNow) value = v; }
        void advance (float c) noexcept
        {
            value += c * (target - value);
            if (std::abs (target - value) < 1.0e-7f) value = target;
        }
    };
    struct Line
    {
        std::vector<float> buffer;
        size_t write = 0, valid = 0;
        double current = 1.0, next = 1.0, requested = 1.0;
        int fade = 0;
        Smooth enabled, feedback, panL, panR;
    };
    static float control (float v, float lo, float hi, float fallback) noexcept
    {
        return std::isfinite (v) ? std::clamp (v, lo, hi) : fallback;
    }
    float coefficient (double frequency) const noexcept
    {
        const double g = std::tan (3.14159265358979 * std::min (frequency, sampleRate * 0.45) / sampleRate);
        return static_cast<float> (g / (1.0 + g));
    }
    static float read (const Line& line, double delay) noexcept
    {
        const auto age = static_cast<size_t> (delay);
        const float fraction = static_cast<float> (delay - static_cast<double> (age));
        const auto a = (line.write + line.buffer.size() - age) % line.buffer.size();
        const auto b = (a + line.buffer.size() - 1) % line.buffer.size();
        const float recent = age <= line.valid ? line.buffer[a] : 0.0f;
        const float older = age + 1 <= line.valid ? line.buffer[b] : 0.0f;
        return recent + fraction * (older - recent);
    }
    static float pole (float input, float coefficient, float& state) noexcept
    {
        const float v = (input - state) * coefficient;
        const float low = v + state;
        state = low + v;
        if (std::abs (state) < 1.0e-20f) state = 0.0f;
        return low;
    }
    float filter (float wet, size_t channel) noexcept
    {
        auto& state = filters[channel];
        const float low = pole (pole (wet, lp.value, state[0]), lp.value, state[1]);
        const float high1 = wet - pole (wet, hp.value, state[2]);
        const float high = high1 - pole (high1, hp.value, state[3]);
        return wet + highCut.value * (low - wet) + lowCut.value * (high - wet);
    }

    double sampleRate = 44100.0;
    float smoothing = 0.002f;
    int fadeSamples = 882;
    bool snap = true;
    Smooth mix, highCut, lowCut, lp, hp;
    std::array<Line, 2> lines;
    std::array<std::array<float, 4>, 2> filters {};
};
