#include "PresetManager.h"

namespace
{
juce::File getPluginDataDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("StepBloom");
}

juce::String sanitisePresetName(juce::String name)
{
    name = name.trim().replaceCharacters("\\/:*?\"<>|", "---------");
    return name.isEmpty() ? "Preset" : name;
}
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& stateToManage)
    : state(stateToManage)
{
}

juce::Result PresetManager::savePreset(const juce::String& presetName) const
{
    const auto file = getPresetsDirectory().getChildFile(sanitisePresetName(presetName))
        .withFileExtension(".xml");
    return savePresetToFile(file);
}

juce::Result PresetManager::savePresetToFile(const juce::File& presetFile) const
{
    if (! presetFile.getParentDirectory().createDirectory())
        return juce::Result::fail("Could not create preset directory.");

    const auto xml = state.copyState().createXml();
    if (xml == nullptr)
        return juce::Result::fail("Could not serialize plugin state.");

    if (! xml->writeTo(presetFile))
        return juce::Result::fail("Could not write preset file: " + presetFile.getFullPathName());

    activePresetName = presetFile.getFileNameWithoutExtension();
    return juce::Result::ok();
}

juce::Result PresetManager::saveDefaultPreset() const
{
    const auto file = getDefaultPresetFile();
    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail("Could not create StepBloom settings directory.");

    const auto xml = state.copyState().createXml();
    if (xml == nullptr)
        return juce::Result::fail("Could not serialize plugin state.");

    if (! xml->writeTo(file))
        return juce::Result::fail("Could not write default preset file: " + file.getFullPathName());

    activePresetName = "Default";
    return juce::Result::ok();
}

juce::Result PresetManager::loadPreset(const juce::File& presetFile) const
{
    if (! presetFile.existsAsFile())
        return juce::Result::fail("Preset file does not exist: " + presetFile.getFullPathName());

    const auto xml = juce::XmlDocument::parse(presetFile);
    if (xml == nullptr)
        return juce::Result::fail("Could not parse preset file: " + presetFile.getFullPathName());
    if (! xml->hasTagName(state.state.getType()))
        return juce::Result::fail("Preset file has an unexpected state format.");

    state.replaceState(juce::ValueTree::fromXml(*xml));
    activePresetName = presetFile.getFileNameWithoutExtension();
    return juce::Result::ok();
}

juce::Array<juce::File> PresetManager::getAvailablePresets() const
{
    juce::Array<juce::File> files;
    getPresetsDirectory().findChildFiles(files, juce::File::findFiles, false, "*.xml");
    files.sort();
    return files;
}

juce::String PresetManager::getActivePresetName() const
{
    return activePresetName;
}

juce::File PresetManager::getDefaultPresetFile() const
{
    return getPluginDataDirectory().getChildFile("default.xml");
}

juce::File PresetManager::getPresetsDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("StepBloom")
        .getChildFile("Presets");
}
