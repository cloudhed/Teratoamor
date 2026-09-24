#pragma once

#include "DelayRates.h"
#include "ElementEnvelope.h"
#include "EngineParams.h"
#include "NoiseSource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Six modulation sections: 1-3 are envelopes (Attack, Decay, Sustain, Release, Time), 4-6 are
// tempo-synced oscillators (wavetable, Soft, Rate, Gate Trig). Every section has a bipolar Depth
// and one target.
//
// Units: a section adds "units" to its target. For 0..100 controls (and -100..100 Pan) one unit is
// one control step, so Depth 100 can sweep the whole control. Pitch is 0.36 semitones per unit
// (Depth 100 = three octaves; the original measured 0.48, but that was too extreme to play with,
// so Depth 75 in the first version is now Depth 100). Rate is 4 octaves per 100 units.
// Envelopes give 0..1 times Depth; oscillators give -1..1 times Depth.
namespace Modulation
{
    constexpr float pitchSemitonesPerUnit = 0.36f;   // Depth 100 = three octaves
    constexpr float rateOctavesPerHundredUnits = 4.0f;

    using Units = std::array<float, ModTargets::count>;

    inline constexpr size_t idx (ModTarget t) noexcept { return static_cast<size_t> (t); }

    enum class Kind { width, pitch, pan, warp, clip, volume };

    // Modulation of one Element control: its own target plus the matching "All" target.
    inline float elementUnits (const Units& u, int element, Kind kind) noexcept
    {
        static constexpr ModTarget own[3][6] = {
            { ModTarget::el1Width, ModTarget::el1Pitch, ModTarget::el1Pan, ModTarget::el1Warp, ModTarget::el1Clip, ModTarget::el1Volume },
            { ModTarget::el2Width, ModTarget::el2Pitch, ModTarget::el2Pan, ModTarget::el2Warp, ModTarget::count, ModTarget::el2Volume },
            { ModTarget::el3Width, ModTarget::el3Pitch, ModTarget::el3Pan, ModTarget::el3Warp, ModTarget::count, ModTarget::el3Volume } };

        float sum = 0.0f;
        const int k = static_cast<int> (kind);

        if (own[element][k] != ModTarget::count)
            sum += u[idx (own[element][k])];

        switch (kind)
        {
            case Kind::width:  sum += u[idx (ModTarget::allWidth)]; break;
            case Kind::pitch:  sum += u[idx (ModTarget::allPitch)]; break;
            case Kind::pan:    sum += u[idx (ModTarget::allPan)];   break;
            case Kind::warp:   sum += u[idx (ModTarget::allWarp)];  break;
            case Kind::volume: sum += u[idx (ModTarget::allVolume)]; break;
            case Kind::clip:   break;
        }

        return sum;
    }

    // One period of a wavetable: phase 0..1 in, about -1..1 out. Off and Random are handled elsewhere.
    inline float waveValue (ModWave wave, float p) noexcept
    {
        constexpr float twoPi = 6.28318530718f;

        switch (wave)
        {
            case ModWave::sine:     return std::sin (twoPi * p);
            case ModWave::triangle: return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case ModWave::saw:      return 2.0f * p - 1.0f;                     // rising, then drops
            case ModWave::ramp:     return 1.0f - 2.0f * p;                     // falling, then jumps
            case ModWave::peak:     return 2.0f * std::max (0.0f, 1.0f - std::abs (p - 0.5f) * 8.0f) - 1.0f;
            case ModWave::dip:      return 1.0f - 2.0f * std::max (0.0f, 1.0f - std::abs (p - 0.5f) * 8.0f);
            case ModWave::hump:     return std::sin (3.14159265359f * p);                        // positive arch only (0..1)
            case ModWave::ripSaw1:  return std::clamp ((2.0f * p - 1.0f) + 0.25f * std::sin (twoPi * 4.0f * p), -1.0f, 1.0f);
            case ModWave::ripSaw2:  return std::clamp ((2.0f * p - 1.0f) + 0.25f * std::sin (twoPi * 8.0f * p), -1.0f, 1.0f);
            case ModWave::ripRamp1: return std::clamp ((1.0f - 2.0f * p) + 0.25f * std::sin (twoPi * 4.0f * p), -1.0f, 1.0f);
            case ModWave::ripRamp2: return std::clamp ((1.0f - 2.0f * p) + 0.25f * std::sin (twoPi * 8.0f * p), -1.0f, 1.0f);
            case ModWave::sharkR:   return 2.0f * std::pow (p, 2.5f) - 1.0f;           // slow convex rise, sharp drop
            case ModWave::sharkL:   return 2.0f * std::pow (1.0f - p, 2.5f) - 1.0f;    // sharp rise, slow convex fall
            case ModWave::pulse100: return p < 0.5f ? 1.0f : -1.0f;                    // starts positive
            case ModWave::pulse50:  return p < 0.75f ? 1.0f : -1.0f;                    // high 75%, low 25%
            case ModWave::pulse25:  return p < 0.875f ? 1.0f : -1.0f;                   // high 87.5%, low 12.5%
            case ModWave::off:
            case ModWave::random:
            case ModWave::count:    break;
        }

        return 0.0f;
    }

    // One low-frequency oscillator, advanced once per control chunk.
    class Oscillator
    {
    public:
        void seed (std::uint64_t s) noexcept { noise.seed (s); restart(); }

        // "Gate Trig": start the pattern over from zero (smoothing also starts from zero).
        void restart() noexcept
        {
            phase = 0.0f;
            smoothed = 0.0f;
            held = noise.next();
        }

        // Moves on by numSamples at frequency hz; returns the smoothed value, about -1..1.
        float advance (ModWave wave, float soft01, double hz, int numSamples, double sampleRate) noexcept
        {
            if (wave == ModWave::off)
                return smoothed = 0.0f;

            const double dt = static_cast<double> (numSamples) / sampleRate;
            phase += static_cast<float> (hz * dt);

            if (phase >= 1.0f)
            {
                phase -= std::floor (phase);
                held = noise.next();
            }

            const float raw = wave == ModWave::random ? held : waveValue (wave, phase);

            // Soft: one-pole smoothing whose time scales with the period, so the same setting gives
            // the same look at any rate (Soft 100 turns a square into a rounded, sine-like wave).
            if (soft01 <= 0.0f)
                smoothed = raw;
            else
            {
                const double tau = 0.2 * soft01 / std::max (hz, 1.0e-3);
                smoothed += static_cast<float> (1.0 - std::exp (-dt / tau)) * (raw - smoothed);
            }

            return smoothed;
        }

    private:
        NoiseSource noise;
        float phase = 0.0f, smoothed = 0.0f, held = 0.0f;
    };

    inline double oscillatorHz (const ModParams& m, double bpm, float rateUnits) noexcept
    {
        return std::exp2 (static_cast<double> (rateUnits) * rateOctavesPerHundredUnits / 100.0)
               / DelayRates::seconds (m.rate, bpm);
    }

    // The engine-wide, free-running oscillators (used by sections with Gate Trig off).
    class FreeOscillators
    {
    public:
        void prepare (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate;
            for (size_t k = 0; k < oscillators.size(); ++k)
                oscillators[k].seed (0x7000u + k);
            values.fill (0.0f);
        }

        void reset() noexcept
        {
            for (auto& o : oscillators)
                o.restart();
            values.fill (0.0f);
        }

        // rateUnits: this chunk's Rate modulation for each oscillator (from the previous chunk).
        void advance (const EngineParams& p, const std::array<float, 3>& rateUnits, int numSamples) noexcept
        {
            for (size_t k = 0; k < oscillators.size(); ++k)
            {
                const auto& m = p.mods[3 + k];
                values[k] = oscillators[k].advance (m.wave, m.soft * 0.01f,
                                                    oscillatorHz (m, p.delay.bpm, rateUnits[k]), numSamples, sampleRate);
            }
        }

        const std::array<float, 3>& get() const noexcept { return values; }

    private:
        double sampleRate = 44100.0;
        std::array<Oscillator, 3> oscillators;
        std::array<float, 3> values {};
    };

    // The modulation of one voice (or, with live == false, of "no note playing").
    // Envelopes and gated oscillators belong to the voice; free oscillators come from the engine.
    class VoiceState
    {
    public:
        void prepare (double newSampleRate, int voiceIndex) noexcept
        {
            sampleRate = newSampleRate;
            for (size_t k = 0; k < oscillators.size(); ++k)
                oscillators[k].seed (static_cast<std::uint64_t> (0x8000 + voiceIndex * 8) + k);
            kill();
        }

        void kill() noexcept
        {
            for (auto& e : envelopes)
                e.kill();
            units.fill (0.0f);
            rateUnits.fill (0.0f);
        }

        void noteOn (const EngineParams& p) noexcept
        {
            for (size_t i = 0; i < envelopes.size(); ++i)
            {
                envelopes[i].prepare (sampleRate);
                const auto& m = p.mods[i];
                envelopes[i].noteOn (m.attack * 0.1f, m.decay * ElementEnvelope::decaySecondsPerUnit * timeScale (m.time),
                                     m.sustain * 0.01f);
            }

            for (auto& o : oscillators)
                o.restart();
        }

        void noteOff (const EngineParams& p) noexcept
        {
            for (size_t i = 0; i < envelopes.size(); ++i)
                envelopes[i].noteOff (p.mods[i].release * 0.1f * timeScale (p.mods[i].time));
        }

        // Works out this chunk's modulation. Call once per chunk before using units().
        void update (int numSamples, const EngineParams& p, const std::array<float, 3>& freeValues, bool live) noexcept
        {
            units.fill (0.0f);

            // Oscillators first: they can modulate the envelopes' Depth and the oscillators' Rate.
            for (size_t k = 0; k < oscillators.size(); ++k)
            {
                const auto& m = p.mods[3 + k];
                float value = 0.0f;

                if (m.gateTrig)
                {
                    if (live)
                        value = oscillators[k].advance (m.wave, m.soft * 0.01f,
                                                        oscillatorHz (m, p.delay.bpm, rateUnits[k]), numSamples, sampleRate);
                }
                else
                {
                    value = freeValues[k];
                }

                if (m.wave != ModWave::off)
                    units[idx (m.target)] += value * m.depth;
            }

            for (size_t i = 0; i < envelopes.size(); ++i)
            {
                float level = 0.0f;

                if (live)
                    for (int s = 0; s < numSamples; ++s)
                        level = envelopes[i].next();

                const auto& m = p.mods[i];
                const float depth = std::clamp (m.depth + units[idx (ModTarget::mod1Depth) + i], -100.0f, 100.0f);
                units[idx (m.target)] += level * depth;
            }

            for (size_t k = 0; k < rateUnits.size(); ++k)
                rateUnits[k] = units[idx (ModTarget::mod4Rate) + k];
        }

        const Units& get() const noexcept { return units; }

    private:
        double sampleRate = 44100.0;
        std::array<ElementEnvelope, 3> envelopes;
        std::array<Oscillator, 3> oscillators;
        Units units {};
        std::array<float, 3> rateUnits {};
    };
}
