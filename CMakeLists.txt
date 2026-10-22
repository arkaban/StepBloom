cmake_minimum_required(VERSION 3.22)

project(StepBloom VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StepBloom
    COMPANY_NAME "StepBloom Audio"
    PRODUCT_NAME "StepBloom"
    PLUGIN_MANUFACTURER_CODE StBp
    PLUGIN_CODE StB1
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(StepBloom)

target_sources(StepBloom PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PresetManager.cpp
    src/PresetManager.h
)

target_compile_features(StepBloom PRIVATE cxx_std_17)

target_compile_definitions(StepBloom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(StepBloom PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
