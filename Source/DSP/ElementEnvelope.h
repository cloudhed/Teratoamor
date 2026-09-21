#pragma once

#include <algorithm>

// Attack / sustain / release envelope for one Element.
//  - Attack follows a cubic curve (measured shape is strongly curved, roughly cubic).
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

    void noteOn (float attackSeconds) noexcept
    {
        phaseIncrement = 1.0f / (std::max (attackSeconds, minSeconds) * static_cast<float> (sampleRate));
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
                if (phase >= 1.0f) { phase = 1.0f; stage = Stage::Sustain; }
                level = phase * phase * phase;
                return level;

            case Stage::Sustain:
                return level = 1.0f;

            case Stage::Release:
            case Stage::Fade:
                phase += phaseIncrement;
                if (phase >= 1.0f) { stage = Stage::Idle; return level = 0.0f; }
                return level = rampStart * (1.0f - phase);
        }

        return 0.0f;
    }

private:
    enum class Stage { Idle, Attack, Sustain, Release, Fade };

    static constexpr float minSeconds  = 0.002f;
    static constexpr float fadeSeconds = 0.005f;

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
};
