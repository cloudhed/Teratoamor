#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Engine.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/Parameters.h"

class TeratoamorAudioProcessor final : public juce::AudioProcessor
{
public:
    TeratoamorAudioProcessor();
    ~TeratoamorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    EngineParams readParams() const noexcept;

    // Raw parameter values, looked up once so the audio thread never searches by name.
    struct ElementParamPointers
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* filter = nullptr;
        std::atomic<float>* octave = nullptr;
        std::atomic<float>* semitone = nullptr;
        std::atomic<float>* fine = nullptr;
        std::atomic<float>* warp = nullptr;
        std::atomic<float>* clip = nullptr;   // null on Element 2
        std::atomic<float>* width = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
    };

    std::atomic<float>* masterLevelParam = nullptr;
    std::array<ElementParamPointers, ParamIDs::numElements> elementParams;

    Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeratoamorAudioProcessor)
};
