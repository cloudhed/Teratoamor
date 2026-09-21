#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Parameters
{
    // Identifier of the root node of the saved state tree.
    inline const juce::Identifier stateType { "TeratoamorState" };

    // Builds the full list of host-visible parameters.
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
