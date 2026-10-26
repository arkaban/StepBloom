#include "PresetManager.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
juce::String sanitisePresetName(juce::String name)
{
    name = name.trim().replaceCharacters("\\/:*?\"<>|", "---------");
    return name.isEmpty() ? "Preset" : name;
}
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& stateToManage)
    : state(stateToManage)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "StepBloom";
    options.filenameSuffix = ".settings";
    options.folderName = "StepBloom";
    options.osxLibrarySubFolder = "Application Support";
    applicationProperties.setStorageParameters(options);
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

    auto restoredState = juce::ValueTree::fromXml(*xml);
    StepBloomAudioProcessor::migrateLegacyEnvelopeState(restoredState);
    state.replaceState(restoredState);
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
    return getPresetsDirectory().getChildFile("default.preset");
}

juce::File PresetManager::getPresetsDirectory() const
{
    if (const auto* settings = applicationProperties.getUserSettings())
    {
        const auto configuredPath = settings->getValue("userPresetDirectory").trim();
        if (configuredPath.isNotEmpty())
            return juce::File(configuredPath);
    }

    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("StepBloom")
        .getChildFile("Presets");
}

juce::Result PresetManager::setPresetsDirectory(const juce::File& directory) const
{
    if (directory == juce::File{})
        return juce::Result::fail("No preset directory was selected.");

    const auto createResult = directory.createDirectory();
    if (createResult.failed())
        return createResult;

    auto* settings = applicationProperties.getUserSettings();
    if (settings == nullptr)
        return juce::Result::fail("Could not open StepBloom application settings.");

    settings->setValue("userPresetDirectory", directory.getFullPathName());
    if (!settings->saveIfNeeded())
        return juce::Result::fail("Could not save the preset directory preference.");

    return juce::Result::ok();
}

float PresetManager::getUserDefaultZoom() const
{
    constexpr std::array<float, 4> zoomLevels{0.75f, 1.0f, 1.5f, 2.0f};
    const auto* settings = applicationProperties.getUserSettings();
    const auto storedZoom = settings != nullptr
        ? settings->getValue("userDefaultZoom", "1.0") : juce::String{"1.0"};
    const auto requestedZoom = storedZoom.getFloatValue();
    const auto isSupportedZoom = std::any_of(zoomLevels.begin(), zoomLevels.end(),
        [requestedZoom](float zoom) { return std::abs(requestedZoom - zoom) < 0.001f; });
    if (!isSupportedZoom)
        return 1.0f;

    auto closestZoom = zoomLevels.front();
    for (const auto zoom : zoomLevels)
        if (std::abs(requestedZoom - zoom) < std::abs(requestedZoom - closestZoom))
            closestZoom = zoom;
    return closestZoom;
}

juce::Result PresetManager::setUserDefaultZoom(float scale) const
{
    constexpr std::array<float, 4> zoomLevels{0.75f, 1.0f, 1.5f, 2.0f};
    auto closestZoom = zoomLevels.front();
    for (const auto zoom : zoomLevels)
        if (std::abs(scale - zoom) < std::abs(scale - closestZoom))
            closestZoom = zoom;

    auto* settings = applicationProperties.getUserSettings();
    if (settings == nullptr)
        return juce::Result::fail("Could not open StepBloom application settings.");

    settings->setValue("userDefaultZoom", closestZoom);
    if (!settings->saveIfNeeded())
        return juce::Result::fail("Could not save the default UI scale.");
    return juce::Result::ok();
}
