#pragma once

#include <algorithm>

// Attack / hold / decay / sustain / release envelope for one Element.
//  - Attack follows a cubic curve (measured shape is strongly curved, roughly cubic).
//  - Hold keeps full level for a set time, then Decay falls linearly to the sustain level.
//    (Hold, Decay, and Sustain are provisional: no reference recordings exist for them yet.)
//  - The caller scales the decay and release lengths by the Element's Time control (see timeScale).
//  - Release ramps linearly from wherever the envelope was when the key was released.
//  - A short "fade" quickly silences a voice that is being stolen, avoiding clicks.
class ElementEnvelope
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        stage = Stage::Idle;
        level = 0.0f;
    }

    bool isIdle() const noexcept { return stage == Stage::Idle; }

    // holdSeconds may be 0 (skipped). sustainLevel is 0..1; at 1 the decay stage is skipped.
    void noteOn (float attackSeconds, float holdSeconds = 0.0f, float decaySeconds = 0.0f, float sustainLevel = 1.0f) noexcept
    {
        const float sr = static_cast<float> (sampleRate);
        phaseIncrement = 1.0f / (std::max (attackSeconds, minSeconds) * sr);
        holdSamples = holdSeconds * sr;
        decayIncrement = 1.0f / (std::max (decaySeconds, minSeconds) * sr);
        sustain = std::clamp (sustainLevel, 0.0f, 1.0f);
        phase = 0.0f;
        level = 0.0f;
        stage = Stage::Attack;
    }

    void noteOff (float releaseSeconds) noexcept
    {
        if (stage == Stage::Idle || stage == Stage::Release || stage == Stage::Fade)
            return;

        beginRamp (std::max (releaseSeconds, minSeconds), Stage::Release);
    }

    void fastFade() noexcept
    {
        if (stage != Stage::Idle)
            beginRamp (fadeSeconds, Stage::Fade);
    }

    void kill() noexcept { stage = Stage::Idle; level = 0.0f; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                return 0.0f;

            case Stage::Attack:
                phase += phaseIncrement;
                if (phase >= 1.0f) { phase = 1.0f; startHold(); }
                level = phase * phase * phase;
                return level;

            case Stage::Hold:
                if (--holdSamples <= 0.0f) startDecay();
                return level = 1.0f;

            case Stage::Decay:
                phase += decayIncrement;
                if (phase >= 1.0f) { stage = Stage::Sustain; return level = sustain; }
                return level = 1.0f - (1.0f - sustain) * phase;

            case Stage::Sustain:
                return level = sustain;

            case Stage::Release:
            case Stage::Fade:
                phase += phaseIncrement;
                if (phase >= 1.0f) { stage = Stage::Idle; return level = 0.0f; }
                return level = rampStart * (1.0f - phase);
        }

        return 0.0f;
    }

private:
    enum class Stage { Idle, Attack, Hold, Decay, Sustain, Release, Fade };

    static constexpr float minSeconds  = 0.002f;
    static constexpr float fadeSeconds = 0.005f;

    void startHold() noexcept
    {
        if (holdSamples > 0.0f) stage = Stage::Hold;
        else                    startDecay();
    }

    void startDecay() noexcept
    {
        phase = 0.0f;
        stage = sustain < 1.0f ? Stage::Decay : Stage::Sustain;
    }

    void beginRamp (float seconds, Stage newStage) noexcept
    {
        rampStart = level;
        phase = 0.0f;
        phaseIncrement = 1.0f / (seconds * static_cast<float> (sampleRate));
        stage = newStage;
    }

    double sampleRate = 44100.0;
    Stage stage = Stage::Idle;
    float level = 0.0f, phase = 0.0f, phaseIncrement = 0.0f, rampStart = 0.0f;
    float holdSamples = 0.0f, decayIncrement = 0.0f, sustain = 1.0f;
};
