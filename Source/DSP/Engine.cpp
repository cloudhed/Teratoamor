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

    globalFilter.reset();
    freeOscillators.prepare (sampleRate);
    idleMods.prepare (sampleRate, numVoices);
    freeRateUnits.fill (0.0f);
    distortion.prepare (sampleRate);
    delay.prepare (sampleRate);
    noteCounter = 0;
    lastNote = -1;
    snapOnNextParams = true;
}

void Engine::reset()
{
    for (auto& v : voices)
        v.kill();

    releaseDeferred.fill (false);
    sustainDown = false;
    pitchBend.snap (0.0f);
    freeOscillators.reset();
    idleMods.kill();
    freeRateUnits.fill (0.0f);
    distortion.reset();
    delay.reset();
    globalFilter.reset();
    snapOnNextParams = true;
}

void Engine::setParams (const EngineParams& newParams)
{
    params = newParams;
    delay.setParameters (params.delay);
    distortion.setParameters (params.distortion.crush, params.distortion.tone, params.distortion.type);

    // Linked Elements (2 and 3) take the envelope settings of Element 1.
    for (int e = 1; e < EngineParams::numElements; ++e)
    {
        auto& p = params.elements[(size_t) e];

        if (p.link)
        {
            const auto& first = params.elements[0];
            p.attack = first.attack;
            p.decay = first.decay;
            p.sustain = first.sustain;
            p.release = first.release;
            p.time = first.time;
        }
    }

    master.target = std::clamp (params.master, 0.0f, 100.0f) / 100.0f;
    masterPan.target = std::clamp (params.masterPan, -100.0f, 100.0f) / 100.0f;
    filterCutoff.target = std::clamp (params.globalFilter.cutoff, 0.0f, 100.0f) / 100.0f;
    filterQ.target = std::clamp (params.globalFilter.q, 0.0f, 100.0f) / 100.0f;

    for (int e = 0; e < EngineParams::numElements; ++e)
    {
        const auto& p = params.elements[(size_t) e];
        auto& s = smoothers[(size_t) e];

        s.pitch.target = static_cast<float> (p.octave * 12 + p.semitone) + p.fineCents / 100.0f;
        s.width.target = std::clamp (p.width, 0.0f, 100.0f) / 100.0f;
        s.warp.target  = std::clamp (p.warp, 0.0f, 100.0f) / 100.0f;
        s.clip.target  = std::clamp (p.clip, 0.0f, 100.0f) / 100.0f;
        s.gain.target  = (p.enabled && p.filterMode != FilterMode::off) ? std::clamp (p.level, 0.0f, 100.0f) / 100.0f : 0.0f;
        s.pan.target   = std::clamp (p.pan, -100.0f, 100.0f) / 100.0f;
    }

    if (snapOnNextParams)
    {
        master.snap (master.target);
        masterPan.snap (masterPan.target);
        pitchBend.snap (pitchBend.target);
        filterCutoff.snap (filterCutoff.target);
        filterQ.snap (filterQ.target);
        for (auto& s : smoothers)
        {
            s.pitch.snap (s.pitch.target);
            s.width.snap (s.width.target);
            s.warp.snap (s.warp.target);
            s.clip.snap (s.clip.target);
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
        v->noteOn (midiNote, std::clamp (velocity01, 0.0f, 1.0f), ++noteCounter, params, lastNote);
        lastNote = midiNote;
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
        s.warp.advance (chunkCoefficient);
        s.clip.advance (chunkCoefficient);
        s.gain.advance (chunkCoefficient);
        s.pan.advance (chunkCoefficient);

        auto& f = frames[(size_t) e];
        f.pitchOffsetSemitones = s.pitch.current + pitchBend.current;
        f.mode = params.elements[(size_t) e].filterMode;
        f.width01 = s.width.current;
        f.warp01 = s.warp.current;
        f.clip01 = s.clip.current;
        f.level01 = s.gain.current;
        f.pan = s.pan.current;
        f.active = params.elements[(size_t) e].enabled && f.mode != FilterMode::off;
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
        masterPan.advance (chunkCoefficient);

        freeOscillators.advance (params, freeRateUnits, n);

        for (auto& v : voices)
            v.render (left + start, right + start, n, frames, params, freeOscillators.get());

        // Effects and Master cannot be modulated per note: they follow the newest sounding note,
        // or the free-running oscillators alone when nothing is sounding.
        const Voice* newest = nullptr;

        for (const auto& v : voices)
            if (v.isActive() && (newest == nullptr || v.getAge() > newest->getAge()))
                newest = &v;

        if (newest == nullptr)
            idleMods.update (n, params, freeOscillators.get(), false);

        const auto& mod = newest != nullptr ? newest->getMods().get() : idleMods.get();

        for (size_t k = 0; k < freeRateUnits.size(); ++k)
            freeRateUnits[k] = mod[Modulation::idx (ModTarget::mod4Rate) + k];

        using Modulation::idx;
        const auto units = [&mod] (ModTarget t) { return mod[idx (t)]; };

        // Global filter on the summed Elements, before the master level.
        filterCutoff.advance (chunkCoefficient);
        filterQ.advance (chunkCoefficient);
        globalFilter.setParameters (params.globalFilter.type,
                                    std::clamp (filterCutoff.current + units (ModTarget::filterCutoff) * 0.01f, 0.0f, 1.0f),
                                    std::clamp (filterQ.current + units (ModTarget::filterQ) * 0.01f, 0.0f, 1.0f), sampleRate);
        globalFilter.process (left + start, right + start, n);

        distortion.setParameters (params.distortion.crush + units (ModTarget::distortCrush),
                                  params.distortion.tone + units (ModTarget::distortTone), params.distortion.type);
        distortion.process (left + start, right + start, n);

        auto delayParams = params.delay;
        delayParams.cut += units (ModTarget::delayFilter);
        for (auto& line : delayParams.lines)
            line.pan += units (ModTarget::delayPan);
        delay.setParameters (delayParams);
        delay.process (left + start, right + start, n);

        // Master level and pan (same constant-power law as the Elements), then a hard safety
        // clamp that also removes any NaN/infinity.
        const float level = std::clamp (master.current + units (ModTarget::masterVolume) * 0.01f, 0.0f, 1.0f);
        const float panPosition = std::clamp (masterPan.current + units (ModTarget::masterPan) * 0.01f, -1.0f, 1.0f);
        const float angle = (panPosition + 1.0f) * (pi * 0.25f);
        const float gainL = level * std::cos (angle) * 1.41421356f;
        const float gainR = level * std::sin (angle) * 1.41421356f;

        for (int i = start; i < start + n; ++i)
        {
            const float l = left[i] * gainL;
            const float r = right[i] * gainR;
            left[i]  = std::abs (l) < 1.0f ? l : (l > 0.0f ? 1.0f : (l < 0.0f ? -1.0f : 0.0f));
            right[i] = std::abs (r) < 1.0f ? r : (r > 0.0f ? 1.0f : (r < 0.0f ? -1.0f : 0.0f));
        }
    }
}
