#pragma once

#include "EngineParams.h"
#include "Voice.h"

#include <array>
#include <cmath>
#include <cstdint>

// Eight-voice polyphonic engine. Allocation-free and lock-free once prepared.
//
// Voice allocation policy on note-on:
//   1. re-use the voice already holding the same note (retrigger),
//   2. else take an idle voice,
//   3. else steal the oldest voice that is already releasing,
//   4. else steal the oldest held voice.
// A stolen voice fades out over ~5 ms before the new note begins.
class Engine
{
public:
    static constexpr int numVoices = 8;
    static constexpr float pitchBendRangeSemitones = 2.0f;

    void prepare (double sampleRate);
    void reset();

    // Call once per block (and before render) with the current control values.
    void setParams (const EngineParams& newParams);

    void noteOn (int midiNote, float velocity01);
    void noteOff (int midiNote);
    void allNotesOff();

    // Sustain pedal (CC 64): while down, released keys keep sounding until the pedal is lifted.
    void setSustainPedal (bool down);

    // Pitch wheel position, -1..1, scaled by pitchBendRangeSemitones.
    void setPitchBend (float position);

    // Overwrites left/right with numSamples of output.
    void render (float* left, float* right, int numSamples);

private:
    // One-pole smoother evaluated once per control chunk.
    struct Smoothed
    {
        float current = 0.0f, target = 0.0f;
        void snap (float v) noexcept { current = target = v; }
        void advance (float coefficient) noexcept
        {
            current += (target - current) * coefficient;
            if (std::abs (target - current) < 1.0e-5f)
                current = target;
        }
    };

    struct ElementSmoothers
    {
        Smoothed pitch, width, warp, clip, gain, pan;
    };

    Voice* chooseVoice (int midiNote) noexcept;
    ElementFrames nextFrames() noexcept;

    double sampleRate = 44100.0;
    float chunkCoefficient = 0.1f;
    bool snapOnNextParams = true;

    EngineParams params;
    std::array<Voice, numVoices> voices;
    std::array<ElementSmoothers, EngineParams::numElements> smoothers;
    Smoothed master, pitchBend;
    bool sustainDown = false;
    std::array<bool, numVoices> releaseDeferred {};
    std::uint64_t noteCounter = 0;
};
