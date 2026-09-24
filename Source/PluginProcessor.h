#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Engine.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/Parameters.h"
#include <array>
#include <cstdint>

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
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void resetToInitialState();
    bool savePresetToFile (const juce::File& file);
    bool loadPresetFromFile (const juce::File& file);
    juce::String getPresetName() const;
    static juce::File defaultPresetFolder();

    juce::AudioProcessorValueTreeState apvts;

    // Maximum output magnitude since the editor last consumed it (may exceed 1 at clipping).
    // The audio thread accumulates; the GUI exchanges with zero. Never persisted in patch state.
    std::atomic<float> outputPeakLeft { 0.0f }, outputPeakRight { 0.0f };
    std::atomic<int> activeVoices { 0 };
    // Increments once for each audio block containing incoming MIDI, so the
    // editor can briefly illuminate the activity light even for short events.
    std::atomic<uint32_t> midiActivityCounter { 0 };

    static constexpr size_t spectrumSize = 2048;
    // Atomic ring of final stereo output. The GUI copies it and performs the FFT;
    // the audio thread only stores samples and never waits for the GUI.
    void copySpectrumSamples (std::array<float, spectrumSize>& destination) const noexcept;
    double getSpectrumSampleRate() const noexcept { return spectrumSampleRate.load (std::memory_order_relaxed); }
    double getCurrentBpm() const noexcept { return hostBpm.load (std::memory_order_relaxed); }

private:
    std::array<std::atomic<float>, spectrumSize> spectrumSamples {};
    std::atomic<uint64_t> spectrumCursor { 0 };
    std::atomic<double> spectrumSampleRate { 44100.0 };
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
        std::atomic<float>* link = nullptr;   // null on Element 1
        std::atomic<float>* time = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
    };

    struct DelayParamPointers
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* pan = nullptr;
    };
    std::array<DelayParamPointers, 2> delayParams;
    std::atomic<float>* delayMixParam = nullptr;
    std::atomic<float>* delayCutParam = nullptr;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> resetEngineOnNextBlock { false };

    std::atomic<float>* distortionTypeParam = nullptr;
    std::atomic<float>* distortionCrushParam = nullptr;
    std::atomic<float>* distortionToneParam = nullptr;
    std::atomic<float>* masterLevelParam = nullptr;
    std::atomic<float>* masterPanParam = nullptr;
    std::atomic<float>* glideParam = nullptr;
    std::atomic<float>* glideModeParam = nullptr;
    std::atomic<float>* tempoSyncParam = nullptr;
    std::atomic<float>* tempoBpmParam = nullptr;

    struct ModParamPointers
    {
        std::atomic<float>* target = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* attack = nullptr;    // envelopes (sections 1-3)
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* time = nullptr;
        std::atomic<float>* wave = nullptr;      // oscillators (sections 4-6)
        std::atomic<float>* gate = nullptr;
        std::atomic<float>* soft = nullptr;
        std::atomic<float>* rate = nullptr;
    };
    std::array<ModParamPointers, ParamIDs::numMods> modParams;
    std::atomic<float>* filterTypeParam = nullptr;
    std::atomic<float>* filterCutoffParam = nullptr;
    std::atomic<float>* filterQParam = nullptr;
    std::array<ElementParamPointers, ParamIDs::numElements> elementParams;

    Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeratoamorAudioProcessor)
};
