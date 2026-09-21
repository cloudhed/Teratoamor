cmake_minimum_required(VERSION 3.22)

project(Teratoamor VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE is pinned as a Git submodule and is never modified.
add_subdirectory(external/JUCE)

juce_add_binary_data(TeratoamorAssets
    HEADER_NAME TeratoamorAssets.h
    NAMESPACE TeratoamorAssets
    SOURCES gui/gui_designs/icon_voidpetal_smaller_margins.png)

juce_add_plugin(Teratoamor
    PRODUCT_NAME "Teratoamor"
    COMPANY_NAME "Teratoamor Project"
    BUNDLE_ID "org.teratoamor.teratoamor"
    PLUGIN_MANUFACTURER_CODE Tprj
    PLUGIN_CODE Trat
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Teratoamor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ui/Theme.h
    Source/ui/Theme.cpp
    Source/ui/EditorContent.h
    Source/ui/EditorContent.cpp
    Source/Parameters/ParameterIDs.h
    Source/Parameters/Parameters.cpp
    Source/Parameters/Parameters.h
    Source/DSP/Engine.cpp
    Source/DSP/DelayRates.h
    Source/DSP/ParallelDelay.h
    Source/DSP/Distortion.h
    Source/DSP/Engine.h
    Source/DSP/EngineParams.h
    Source/DSP/ElementEnvelope.h
    Source/DSP/ElementFilter.h
    Source/DSP/ElementShaper.h
    Source/DSP/Modulation.h
    Source/DSP/NoiseSource.h
    Source/DSP/ResonantBandpass.h
    Source/DSP/Voice.h)

target_include_directories(Teratoamor PRIVATE Source)

target_compile_definitions(Teratoamor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Teratoamor
    PRIVATE
        TeratoamorAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# DSP tests: the engine has no JUCE dependency, so it is tested as a plain console program.
enable_testing()
add_executable(TeratoamorEngineTests Tests/EngineTests.cpp Source/DSP/Engine.cpp)
target_include_directories(TeratoamorEngineTests PRIVATE Source)
add_test(NAME EngineTests COMMAND TeratoamorEngineTests)

# Offline research renderer used for matched clean-room listening and spectral comparisons.
# It uses the same JUCE-free DSP engine as the tests and writes only to a caller-selected folder.
add_executable(TeratoamorWidthRenderer Tests/WidthRenderer.cpp Source/DSP/Engine.cpp)
target_include_directories(TeratoamorWidthRenderer PRIVATE Source)

add_executable(TeratoamorDelayTests Tests/DelayTests.cpp Source/DSP/Engine.cpp)
target_include_directories(TeratoamorDelayTests PRIVATE Source)
add_test(NAME DelayTests COMMAND TeratoamorDelayTests)

add_executable(TeratoamorModulationTests Tests/ModulationTests.cpp Source/DSP/Engine.cpp)
target_include_directories(TeratoamorModulationTests PRIVATE Source)
add_test(NAME ModulationTests COMMAND TeratoamorModulationTests)

# Render and exercise the real editor without opening a desktop window or audio device.
if(WIN32)
    juce_add_console_app(TeratoamorEditorTests PRODUCT_NAME "Teratoamor Editor Tests")
    target_sources(TeratoamorEditorTests PRIVATE Tests/EditorTests.cpp)
    target_include_directories(TeratoamorEditorTests PRIVATE Source)
    target_link_libraries(TeratoamorEditorTests PRIVATE Teratoamor juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME EditorTests COMMAND TeratoamorEditorTests "${CMAKE_BINARY_DIR}/gui-previews")
endif()
