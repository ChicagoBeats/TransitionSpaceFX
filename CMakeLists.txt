cmake_minimum_required(VERSION 3.22)
project(TransitionSpaceFX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TransitionSpaceFX
    COMPANY_NAME "YourName"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Ynam
    PLUGIN_CODE Tsfx
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx
    PRODUCT_NAME "Transition Space FX"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TransitionSpaceFX PRIVATE
    Source/Dsp.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/Visuals.h
    Source/Visuals.cpp
    Source/VisualsCommon.h)

target_compile_definitions(TransitionSpaceFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TransitionSpaceFX PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Optional: DSP unit test (no JUCE needed)
add_executable(dsp_test tests/dsp_test.cpp)
set_target_properties(dsp_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
