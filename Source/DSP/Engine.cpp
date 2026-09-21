#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float smoothingSeconds = 0.010f;
    constexpr float pi = 3.14159265358979f;
}

void Engine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // Coefficient for a one-pole smoother stepped every Voice::maxChunk samples.
    chunkCoefficient = 1.0f - std::exp (-static_cast<float> (Voice::maxChunk)
                                        / (smoothingSeconds * static_cast<float> (sampleRate)));

    for (int v = 0; v < numVoices; ++v)
        voices[(size_t) v].prepare (sampleRate, v);

    noteCounter = 0;
    snapOnNextParams = true;
}

void Engine::reset()
{
    for (auto& v : voices)
        v.kill();

    releaseDeferred.fill (false);
    sustainDown = false;
    pitchBend.snap (0.0f);
    snapOnNextParams = true;
}

void Engine::setParams (const EngineParams& newParams)
{
    params = newParams;

    master.target = std::clamp (params.master, 0.0f, 100.0f) / 100.0f;

    for (int e = 0; e < EngineParams::numElements; ++e)
    {
        const auto& p = params.elements[(size_t) e];
        auto& s = smoothers[(size_t) e];

        s.pitch.target = static_cast<float> (p.octave * 12 + p.semitone) + p.fineCents / 100.0f;
        s.width.target = std::clamp (p.width, 0.0f, 100.0f) / 100.0f;
        s.gain.target  = p.enabled ? std::clamp (p.level, 0.0f, 100.0f) / 100.0f : 0.0f;
        s.pan.target   = std::clamp (p.pan, -100.0f, 100.0f) / 100.0f;
    }

    if (snapOnNextParams)
    {
        master.snap (master.target);
        pitchBend.snap (pitchBend.target);
        for (auto& s : smoothers)
        {
            s.pitch.snap (s.pitch.target);
            s.width.snap (s.width.target);
            s.gain.snap (s.gain.target);
            s.pan.snap (s.pan.target);
        }
        snapOnNextParams = false;
    }
}

Voice* Engine::chooseVoice (int midiNote) noexcept
{
    for (auto& v : voices)
        if (v.isHeld() && v.getNote() == midiNote)
            return &v;

    for (auto& v : voices)
        if (! v.isActive())
            return &v;

    Voice* oldestReleasing = nullptr;
    Voice* oldest = nullptr;

    for (auto& v : voices)
    {
        if (! v.isHeld() && (oldestReleasing == nullptr || v.getAge() < oldestReleasing->getAge()))
            oldestReleasing = &v;

        if (oldest == nullptr || v.getAge() < oldest->getAge())
            oldest = &v;
    }

    return oldestReleasing != nullptr ? oldestReleasing : oldest;
}

void Engine::noteOn (int midiNote, float velocity01)
{
    if (auto* v = chooseVoice (midiNote))
    {
        releaseDeferred[(size_t) (v - voices.data())] = false;
        v->noteOn (midiNote, std::clamp (velocity01, 0.0f, 1.0f), ++noteCounter, params);
    }
}

void Engine::noteOff (int midiNote)
{
    for (size_t i = 0; i < voices.size(); ++i)
    {
        auto& v = voices[i];

        if (! (v.isHeld() && v.getNote() == midiNote))
            continue;

        if (sustainDown)
            releaseDeferred[i] = true;   // keep sounding until the pedal is lifted
        else
            v.noteOff (params);
    }
}

void Engine::allNotesOff()
{
    releaseDeferred.fill (false);

    for (auto& v : voices)
        if (v.isHeld())
            v.noteOff (params);
}

void Engine::setSustainPedal (bool down)
{
    sustainDown = down;

    if (down)
        return;

    for (size_t i = 0; i < voices.size(); ++i)
    {
        if (releaseDeferred[i] && voices[i].isHeld())
            voices[i].noteOff (params);

        releaseDeferred[i] = false;
    }
}

void Engine::setPitchBend (float position)
{
    pitchBend.target = std::clamp (position, -1.0f, 1.0f) * pitchBendRangeSemitones;
}

ElementFrames Engine::nextFrames() noexcept
{
    ElementFrames frames;
    pitchBend.advance (chunkCoefficient);

    for (int e = 0; e < EngineParams::numElements; ++e)
    {
        auto& s = smoothers[(size_t) e];
        s.pitch.advance (chunkCoefficient);
        s.width.advance (chunkCoefficient);
        s.gain.advance (chunkCoefficient);
        s.pan.advance (chunkCoefficient);

        // Constant-power pan, scaled so the centre position has unity gain in each channel.
        const float angle = (s.pan.current + 1.0f) * (pi * 0.25f);
        const float sqrt2 = 1.41421356f;

        auto& f = frames[(size_t) e];
        f.pitchOffsetSemitones = s.pitch.current + pitchBend.current;
        f.width01 = s.width.current;
        f.gainL = s.gain.current * std::cos (angle) * sqrt2;
        f.gainR = s.gain.current * std::sin (angle) * sqrt2;
    }

    return frames;
}

void Engine::render (float* left, float* right, int numSamples)
{
    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);

    for (int start = 0; start < numSamples; start += Voice::maxChunk)
    {
        const int n = std::min (Voice::maxChunk, numSamples - start);
        const auto frames = nextFrames();
        master.advance (chunkCoefficient);

        for (auto& v : voices)
            v.render (left + start, right + start, n, frames);

        // Master level, then a hard safety clamp that also removes any NaN/infinity.
        for (int i = start; i < start + n; ++i)
        {
            const float l = left[i] * master.current;
            const float r = right[i] * master.current;
            left[i]  = std::abs (l) < 1.0f ? l : (l > 0.0f ? 1.0f : (l < 0.0f ? -1.0f : 0.0f));
            right[i] = std::abs (r) < 1.0f ? r : (r > 0.0f ? 1.0f : (r < 0.0f ? -1.0f : 0.0f));
        }
    }
}
