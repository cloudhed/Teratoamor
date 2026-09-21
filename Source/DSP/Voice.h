#pragma once

#include "ElementEnvelope.h"
#include "EngineParams.h"
#include "NoiseSource.h"
#include "ResonantBandpass.h"

#include <array>
#include <cmath>
#include <cstdint>

// Per-chunk values for one Element, shared by every voice (already smoothed by the engine).
struct ElementFrame
{
    float pitchOffsetSemitones = 0.0f;
    float width01 = 0.9f;
    float gainL = 0.0f;   // level x pan, left
    float gainR = 0.0f;   // level x pan, right
};

using ElementFrames = std::array<ElementFrame, EngineParams::numElements>;

// One note: three Elements, each = white noise -> resonant band-pass -> envelope.
class Voice
{
public:
    static constexpr int   maxChunk = 16;               // control-rate update interval (samples)
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
        }

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

    bool isHeld() const noexcept          { return held; }
    int getNote() const noexcept          { return note; }
    std::uint64_t getAge() const noexcept { return age; }

    // If the voice is already sounding it is faded out quickly and the new note starts afterwards.
    void noteOn (int newNote, float newVelocity, std::uint64_t newAge, const EngineParams& params) noexcept
    {
        note = newNote;
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
            elements[(size_t) e].envelope.noteOff (params.elements[(size_t) e].release * 0.1f);
    }

    void kill() noexcept
    {
        held = false;
        hasPending = false;
        for (auto& el : elements)
        {
            el.envelope.kill();
            el.filter.reset();
        }
    }

    // Adds up to maxChunk samples of this voice into left/right.
    void render (float* left, float* right, int numSamples, const ElementFrames& frames) noexcept
    {
        if (hasPending && allEnvelopesIdle())
        {
            hasPending = false;
            start();
        }

        if (! isActive())
            return;

        for (int e = 0; e < EngineParams::numElements; ++e)
        {
            auto& el = elements[(size_t) e];

            if (el.envelope.isIdle())
                continue;

            const auto& frame = frames[(size_t) e];
            const float midiNote = static_cast<float> (playingNote) + frame.pitchOffsetSemitones;
            const float hz = 440.0f * std::exp2 ((midiNote - 69.0f) / 12.0f);
            el.filter.setParameters (hz, ResonantBandpass::widthToQ (frame.width01), sampleRate);

            const float gain = targetRms * playingVelocity;

            for (int i = 0; i < numSamples; ++i)
            {
                const float y = el.filter.process (el.noise.next()) * el.envelope.next() * gain;
                left[i]  += y * frame.gainL;
                right[i] += y * frame.gainR;
            }
        }
    }

private:
    struct Element
    {
        NoiseSource noise;
        ResonantBandpass filter;
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

        for (int e = 0; e < EngineParams::numElements; ++e)
        {
            elements[(size_t) e].filter.reset();
            elements[(size_t) e].envelope.noteOn (latestParams.elements[(size_t) e].attack * 0.1f);
        }
    }

    double sampleRate = 44100.0;
    std::array<Element, EngineParams::numElements> elements;

    int note = 60, playingNote = 60;
    float velocity = 1.0f, playingVelocity = 1.0f;
    std::uint64_t age = 0;
    bool held = false, hasPending = false;
    EngineParams latestParams;
};
