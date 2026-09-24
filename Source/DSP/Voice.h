#pragma once

#include "ElementEnvelope.h"
#include "ElementFilter.h"
#include "ElementShaper.h"
#include "EngineParams.h"
#include "Modulation.h"
#include "NoiseSource.h"

#include <array>
#include <cmath>
#include <cstdint>

// Per-chunk values for one Element, shared by every voice (already smoothed by the engine).
struct ElementFrame
{
    float pitchOffsetSemitones = 0.0f;
    FilterMode mode = FilterMode::bpWide;
    float width01 = 0.9f;
    float warp01 = 0.0f;
    float clip01 = 0.0f;
    float level01 = 0.0f;   // smoothed Level (already 0 when the Element is off)
    float pan = 0.0f;       // -1..1
    bool active = false;    // Element on and not in Off filter mode: modulation may not revive it
};

using ElementFrames = std::array<ElementFrame, EngineParams::numElements>;

// One note: three Elements, each = white noise -> filter -> warp/clip -> envelope.
class Voice
{
public:
    static constexpr int   maxChunk = 16;               // control-rate update interval (samples)
    static constexpr float decaySecondsPerUnit = ElementEnvelope::decaySecondsPerUnit;
    static constexpr float targetRms = 0.15f;           // level of one full-scale Element before gain

    void prepare (double newSampleRate, int voiceIndex) noexcept
    {
        sampleRate = newSampleRate;

        for (int e = 0; e < EngineParams::numElements; ++e)
        {
            // Fixed, unique seed per voice and Element: independent and repeatable.
            elements[(size_t) e].noise.seed (static_cast<std::uint64_t> (voiceIndex * 16 + e + 1));
            elements[(size_t) e].envelope.prepare (sampleRate);
            elements[(size_t) e].filter.reset();
            elements[(size_t) e].shaper.prepare (sampleRate);
        }

        mods.prepare (sampleRate, voiceIndex);
        held = false;
        hasPending = false;
    }

    bool isActive() const noexcept
    {
        if (hasPending)
            return true;

        for (const auto& el : elements)
            if (! el.envelope.isIdle())
                return true;

        return false;
    }

    const Modulation::VoiceState& getMods() const noexcept { return mods; }

    bool isHeld() const noexcept          { return held; }
    int getNote() const noexcept          { return note; }
    std::uint64_t getAge() const noexcept { return age; }

    // If the voice is already sounding it is faded out quickly and the new note starts afterwards.
    // glideFromNote is the previously played note (-1 for none); see EngineParams::glide.
    void noteOn (int newNote, float newVelocity, std::uint64_t newAge, const EngineParams& params, int glideFromNote = -1) noexcept
    {
        note = newNote;
        pendingGlideFrom = glideFromNote;
        velocity = newVelocity;
        age = newAge;
        held = true;
        latestParams = params;

        if (isActive())
        {
            hasPending = true;
            for (auto& el : elements)
                el.envelope.fastFade();
        }
        else
        {
            start();
        }
    }

    void noteOff (const EngineParams& params) noexcept
    {
        held = false;

        if (hasPending)
        {
            hasPending = false;   // key released before the stolen voice restarted
            return;
        }

        for (int e = 0; e < EngineParams::numElements; ++e)
        {
            const auto& p = params.elements[(size_t) e];
            elements[(size_t) e].envelope.noteOff (p.release * 0.1f * timeScale (p.time));
        }

        mods.noteOff (params);
    }

    void kill() noexcept
    {
        held = false;
        hasPending = false;
        for (auto& el : elements)
        {
            el.envelope.kill();
            el.filter.reset();
            el.shaper.reset();
        }
        mods.kill();
    }

    // Adds up to maxChunk samples of this voice into left/right.
    // frames are the smoothed knob values; this voice's modulation is added on top of them.
    void render (float* left, float* right, int numSamples, const ElementFrames& frames,
                 const EngineParams& params, const std::array<float, 3>& freeOscillators) noexcept
    {
        if (hasPending && allEnvelopesIdle())
        {
            hasPending = false;
            start();
        }

        if (! isActive())
            return;

        mods.update (numSamples, params, freeOscillators, true);
        const auto& units = mods.get();

        // Glide: a constant-time slide (in semitones) from the previous note to this one.
        const float glideSemitones = glideOffset;
        const float glideMove = glideStep * static_cast<float> (numSamples);
        glideOffset = std::abs (glideOffset) <= glideMove ? 0.0f : glideOffset - std::copysign (glideMove, glideOffset);

        for (int e = 0; e < EngineParams::numElements; ++e)
        {
            auto& el = elements[(size_t) e];

            if (el.envelope.isIdle())
                continue;

            using Modulation::Kind;
            using Modulation::elementUnits;
            const auto& frame = frames[(size_t) e];
            const float pitch = frame.pitchOffsetSemitones + glideSemitones + elementUnits (units, e, Kind::pitch) * Modulation::pitchSemitonesPerUnit;
            const float width = std::clamp (frame.width01 + elementUnits (units, e, Kind::width) * 0.01f, 0.0f, 1.0f);
            const float warp  = std::clamp (frame.warp01  + elementUnits (units, e, Kind::warp)  * 0.01f, 0.0f, 1.0f);
            const float clip  = std::clamp (frame.clip01  + elementUnits (units, e, Kind::clip)  * 0.01f, 0.0f, 1.0f);
            const float level = frame.active ? std::clamp (frame.level01 + elementUnits (units, e, Kind::volume) * 0.01f, 0.0f, 1.0f)
                                             : frame.level01;
            const float pan   = std::clamp (frame.pan + elementUnits (units, e, Kind::pan) * 0.01f, -1.0f, 1.0f);

            // Constant-power pan, scaled so the centre position has unity gain in each channel.
            const float angle = (pan + 1.0f) * (3.14159265359f * 0.25f);
            const float gainL = level * std::cos (angle) * 1.41421356f;
            const float gainR = level * std::sin (angle) * 1.41421356f;

            const float midiNote = static_cast<float> (playingNote) + pitch;
            const float hz = 440.0f * std::exp2 ((midiNote - 69.0f) / 12.0f);
            el.filter.setMode (frame.mode);
            el.filter.setParameters (hz, width, sampleRate);
            el.shaper.setParameters (warp, clip, hz, sampleRate);

            const float gain = targetRms * playingVelocity;

            for (int i = 0; i < numSamples; ++i)
            {
                const float y = el.shaper.process (el.filter.process (el.noise.next())) * el.envelope.next() * gain;
                left[i]  += y * gainL;
                right[i] += y * gainR;
            }
        }
    }

private:
    struct Element
    {
        NoiseSource noise;
        ElementFilter filter;
        ElementShaper shaper;
        ElementEnvelope envelope;
    };

    bool allEnvelopesIdle() const noexcept
    {
        for (const auto& el : elements)
            if (! el.envelope.isIdle())
                return false;
        return true;
    }

    void start() noexcept
    {
        playingNote = note;
        playingVelocity = velocity;
        mods.noteOn (latestParams);

        glideOffset = glideStep = 0.0f;

        if (pendingGlideFrom >= 0 && pendingGlideFrom != note && latestParams.glide > 0.0f)
        {
            glideOffset = static_cast<float> (pendingGlideFrom - note);
            const float octaves = latestParams.glideByRate ? std::abs (glideOffset) / 12.0f : 1.0f;
            glideStep = std::abs (glideOffset) / (latestParams.glide * 0.02f * octaves * static_cast<float> (sampleRate));
        }

        for (int e = 0; e < EngineParams::numElements; ++e)
        {
            elements[(size_t) e].filter.reset();
            elements[(size_t) e].shaper.reset();
            const auto& p = latestParams.elements[(size_t) e];
            elements[(size_t) e].envelope.noteOn (p.attack * 0.1f,
                                                  p.decay * decaySecondsPerUnit * timeScale (p.time), p.sustain * 0.01f);
        }
    }

    double sampleRate = 44100.0;
    std::array<Element, EngineParams::numElements> elements;

    int note = 60, playingNote = 60;
    float velocity = 1.0f, playingVelocity = 1.0f;
    std::uint64_t age = 0;
    bool held = false, hasPending = false;
    EngineParams latestParams;
    Modulation::VoiceState mods;
    int pendingGlideFrom = -1;
    float glideOffset = 0.0f, glideStep = 0.0f;   // semitones still to slide, and the slide per sample
};
