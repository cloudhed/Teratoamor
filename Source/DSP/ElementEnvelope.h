#pragma once

#include <algorithm>
#include <cmath>

// Attack / decay / sustain / release envelope for one Element.
//  - Attack follows a cubic curve (measured shape is strongly curved, roughly cubic).
//  - Decay is a linear fall of a control value u from 1 to the Sustain setting, and the output
//    is u raised to decayCurve. The same curve turns Sustain into a level, so Sustain 50 sits
//    about 11 dB down. (Fitted to reference recordings to about 0.3 dB.)
//  - Release ramps linearly from wherever the envelope was when the key was released.
//  - A short "fade" quickly silences a voice that is being stolen, avoiding clicks.
//  - The caller scales the decay and release lengths by the Element's Time control (see timeScale).
class ElementEnvelope
{
public:
    static constexpr float decayCurve = 1.95f;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        stage = Stage::Idle;
        level = 0.0f;
    }

    bool isIdle() const noexcept { return stage == Stage::Idle; }

    // sustainControl is 0..1; at 1 the decay stage is skipped.
    void noteOn (float attackSeconds, float decaySeconds = 0.0f, float sustainControl = 1.0f) noexcept
    {
        const float sr = static_cast<float> (sampleRate);
        phaseIncrement = 1.0f / (std::max (attackSeconds, minSeconds) * sr);
        decayIncrement = 1.0f / (std::max (decaySeconds, minSeconds) * sr);
        sustain = std::clamp (sustainControl, 0.0f, 1.0f);
        sustainLevel = std::pow (sustain, decayCurve);
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
                if (phase >= 1.0f) { phase = 1.0f; startDecay(); }
                level = phase * phase * phase;
                return level;

            case Stage::Decay:
                phase += decayIncrement;
                if (phase >= 1.0f) { stage = Stage::Sustain; return level = sustainLevel; }
                return level = std::pow (1.0f - (1.0f - sustain) * phase, decayCurve);

            case Stage::Sustain:
                return level = sustainLevel;

            case Stage::Release:
            case Stage::Fade:
                phase += phaseIncrement;
                if (phase >= 1.0f) { stage = Stage::Idle; return level = 0.0f; }
                return level = rampStart * (1.0f - phase);
        }

        return 0.0f;
    }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release, Fade };

    static constexpr float minSeconds  = 0.002f;
    static constexpr float fadeSeconds = 0.005f;

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
    float decayIncrement = 0.0f, sustain = 1.0f, sustainLevel = 1.0f;
};
