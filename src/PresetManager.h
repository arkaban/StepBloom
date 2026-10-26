#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager final
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& stateToManage);

    juce::Result savePreset(const juce::String& presetName) const;
    juce::Result savePresetToFile(const juce::File& presetFile) const;
    juce::Result saveDefaultPreset() const;
    juce::Result loadPreset(const juce::File& presetFile) const;
    juce::Array<juce::File> getAvailablePresets() const;
    juce::String getActivePresetName() const;
    juce::File getDefaultPresetFile() const;
    juce::File getPresetsDirectory() const;
    juce::Result setPresetsDirectory(const juce::File& directory) const;
    float getUserDefaultZoom() const;
    juce::Result setUserDefaultZoom(float scale) const;

private:
    juce::AudioProcessorValueTreeState& state;
    mutable juce::ApplicationProperties applicationProperties;
    mutable juce::String activePresetName{"Current Settings"};
};
