#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>
#include <utility>

namespace
{
constexpr double minimumBpm = 20.0;
constexpr double maximumBpm = 300.0;
}

float StepBloomAudioProcessor::evaluateEnvelope(
    float phase, float attackPhase, float duckWidth,
    float startGain, float curveTension,
    float endFadeOffset, float endGain) noexcept
{
    if (phase < attackPhase && attackPhase > 0.0f)
    {
        const auto progress = juce::jlimit(0.0f, 1.0f, phase / attackPhase);
        const auto smoothProgress = progress * progress * (3.0f - 2.0f * progress);
        return endGain + (startGain - endGain) * smoothProgress;
    }

    if (phase <= duckWidth)
    {
        const auto recoveryLength = juce::jmax(duckWidth - attackPhase, 1.0e-6f);
        const auto progress = juce::jlimit(0.0f, 1.0f,
            (phase - attackPhase) / recoveryLength);
        const auto tension = juce::jlimit(0.2f, 1.5f, curveTension);
        const auto shape = tension <= 1.0f
            ? tension
            : 1.0f + (tension - 1.0f) * 3.0f;
        const auto shapedProgress = std::pow(progress, shape);
        return startGain + (1.0f - startGain) * shapedProgress;
    }

    const auto tailStart = juce::jmin(juce::jmax(endFadeOffset, duckWidth), 0.999f);
    if (phase < tailStart)
        return 1.0f;

    const auto fadeLength = juce::jmax(1.0f - tailStart, 1.0e-6f);
    const auto progress = juce::jlimit(0.0f, 1.0f,
        (phase - tailStart) / fadeLength);
    const auto smoothProgress = progress * progress * (3.0f - 2.0f * progress);
    return 1.0f + (endGain - 1.0f) * smoothProgress;
}

float StepBloomAudioProcessor::evaluateProfileEnvelope(
    float phase, float attackPhase, float duckWidth,
    float startGain, float curveTension,
    float endFadeOffset, float endGain,
    const std::array<ExtraEnvelopeNode, 2>& extras) noexcept
{
    if (!extras[0].active && !extras[1].active)
        return evaluateEnvelope(phase, attackPhase, duckWidth, startGain,
                                curveTension, endFadeOffset, endGain);

    struct Point
    {
        float phase;
        float gain;
    };
    std::array<Point, 7> points{};
    size_t pointCount = 0;
    const auto addPoint = [&points, &pointCount](float pointPhase, float gain)
    {
        points[pointCount++] = {juce::jlimit(0.0f, 1.0f, pointPhase),
                                juce::jlimit(0.0f, 1.0f, gain)};
    };
    addPoint(0.0f, evaluateEnvelope(0.0f, attackPhase, duckWidth, startGain,
                                    curveTension, endFadeOffset, endGain));
    addPoint(attackPhase, startGain);
    addPoint(duckWidth, 1.0f);
    for (const auto& extra : extras)
        if (extra.active)
            addPoint(extra.phase, extra.gain);
    addPoint(endFadeOffset, endGain);
    addPoint(1.0f, endGain);

    for (size_t i = 1; i < pointCount; ++i)
    {
        auto point = points[i];
        auto position = i;
        while (position > 0 && points[position - 1].phase > point.phase)
        {
            points[position] = points[position - 1];
            --position;
        }
        points[position] = point;
    }

    auto uniquePointCount = size_t{0};
    for (size_t i = 0; i < pointCount; ++i)
    {
        if (uniquePointCount > 0
            && points[i].phase - points[uniquePointCount - 1].phase <= 1.0e-6f)
            points[uniquePointCount - 1] = points[i];
        else
            points[uniquePointCount++] = points[i];
    }
    pointCount = uniquePointCount;

    size_t left = 0;
    while (left + 1 < pointCount && phase > points[left + 1].phase)
        ++left;
    if (left + 1 >= pointCount)
        return points[pointCount - 1].gain;

    const auto& first = points[left];
    const auto& second = points[left + 1];
    const auto segmentLength = second.phase - first.phase;
    if (segmentLength <= 1.0e-6f)
        return second.gain;

    const auto progress = juce::jlimit(0.0f, 1.0f,
        (phase - first.phase) / segmentLength);
    auto shapedProgress = progress * progress * (3.0f - 2.0f * progress);
    if (first.phase >= attackPhase && second.phase <= duckWidth)
    {
        const auto tension = juce::jlimit(0.2f, 1.5f, curveTension);
        const auto shape = tension <= 1.0f ? tension : 1.0f + (tension - 1.0f) * 3.0f;
        shapedProgress = std::pow(progress, shape);
    }
    return first.gain + (second.gain - first.gain) * shapedProgress;
}

void StepBloomAudioProcessor::migrateLegacyEnvelopeState(juce::ValueTree state)
{
    if (state.getChildWithProperty("id", "Master_Node1_X").isValid())
        return;

    constexpr std::array<std::pair<const char*, const char*>, 6> legacyMappings{{
        {"attackTime", "Master_Node1_X"},
        {"minGain", "Master_Node1_Y"},
        {"duckWidth", "Master_Node2_X"},
        {"releaseTension", "Master_Node2_Y"},
        {"endFadeOffset", "Master_Node3_X"},
        {"endGain", "Master_Node3_Y"}
    }};
    for (const auto& mapping : legacyMappings)
    {
        const auto oldParameter = state.getChildWithProperty("id", mapping.first);
        if (!oldParameter.isValid())
            continue;

        juce::ValueTree migratedParameter{oldParameter.getType()};
        migratedParameter.setProperty("id", mapping.second, nullptr);
        migratedParameter.setProperty("value", oldParameter.getProperty("value"), nullptr);
        state.appendChild(migratedParameter, nullptr);
    }
}

float StepBloomAudioProcessor::getCycleDurationMilliseconds() const noexcept
{
    return cycleDurationMilliseconds.load(std::memory_order_relaxed);
}

PresetManager& StepBloomAudioProcessor::getPresetManager() noexcept
{
    return presetManager;
}

StepBloomAudioProcessor::Parameters::ParameterLayout StepBloomAudioProcessor::createParameterLayout()
{
    Parameters::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"depth", 1}, "Mix", juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f},
        100.0f, juce::AudioParameterFloatAttributes{}
                    .withLabel("%")
                    .withStringFromValueFunction([](float value, int maximumLength)
                    {
                        const auto text = juce::String(juce::roundToInt(value)) + "%";
                        return maximumLength > 0 ? text.substring(0, maximumLength) : text;
                    })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"threshold", 1}, "Trigger Threshold",
        juce::NormalisableRange<float>{-60.0f, 0.0f, 1.0f}, -36.0f,
        juce::AudioParameterFloatAttributes{}
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int maximumLength)
            {
                const auto text = juce::String(juce::roundToInt(value)) + " dB";
                return maximumLength > 0 ? text.substring(0, maximumLength) : text;
            })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"triggerHoldOff", 1}, "Trigger Hold-Off",
        juce::NormalisableRange<float>{10.0f, 200.0f, 1.0f}, 80.0f,
        juce::AudioParameterFloatAttributes{}
            .withLabel("ms")
            .withStringFromValueFunction([](float value, int maximumLength)
            {
                const auto text = juce::String(juce::roundToInt(value)) + " ms";
                return maximumLength > 0 ? text.substring(0, maximumLength) : text;
            })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"triggerMode", 1}, "Trigger Mode",
        juce::StringArray{"Sync", "MIDI Trigger", "Audio Trigger"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"cycle", 1}, "Cycle",
        juce::StringArray{"1/2", "1/4", "1/8", "1/16"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"processingMode", 1}, "Processing Mode",
        juce::StringArray{"Singleband", "Multiband"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"userScaleFactor", 1}, "UI Scale",
        juce::NormalisableRange<float>{0.75f, 2.0f, 0.25f}, 1.0f,
        juce::AudioParameterFloatAttributes{}.withAutomatable(false)));
    constexpr std::array<const char*, 4> profileNames{"Master", "Low", "Mid", "High"};
    for (const auto* profileName : profileNames)
    {
        const auto prefix = juce::String(profileName) + "_";
        const auto addFloat = [&layout, &prefix](const juce::String& suffix,
                                                  const juce::String& name,
                                                  juce::NormalisableRange<float> range,
                                                  float defaultValue)
        {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{prefix + suffix, 1}, prefix + name, range, defaultValue));
        };
        addFloat("Node1_X", "Attack Time",
                 juce::NormalisableRange<float>{0.0f, 15.0f, 0.01f}, 3.0f);
        addFloat("Node1_Y", "Duck Floor",
                 juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.2f);
        addFloat("Node2_X", "Duck Width",
                 juce::NormalisableRange<float>{0.1f, 0.95f, 0.001f}, 0.6f);
        addFloat("Node2_Y", "Curve Tension",
                 juce::NormalisableRange<float>{0.2f, 1.5f, 0.001f}, 1.0f);
        addFloat("Node3_X", "Tail Offset",
                 juce::NormalisableRange<float>{0.8f, 1.0f, 0.001f}, 0.95f);
        addFloat("Node3_Y", "End Gain",
                 juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 1.0f);
        for (int extra = 0; extra < 2; ++extra)
        {
            const auto extraName = "Extra" + juce::String(extra + 1);
            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{prefix + extraName + "_Active", 1},
                prefix + extraName + " Active", false));
            addFloat(extraName + "_X", extraName + " Phase",
                     juce::NormalisableRange<float>{0.05f, 0.95f, 0.001f}, 0.5f);
            addFloat(extraName + "_Y", extraName + " Gain",
                     juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.5f);
        }
    }
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"crossoverLowMid", 1}, "Low-Mid Crossover",
        juce::NormalisableRange<float>{20.0f, 2000.0f, 0.0f, 0.35f}, 200.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"crossoverMidHigh", 1}, "Mid-High Crossover",
        juce::NormalisableRange<float>{1000.0f, 20000.0f, 0.0f, 0.35f}, 3000.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("Hz")));
    constexpr std::array<const char*, 3> bandNames{"Low", "Mid", "High"};
    constexpr std::array<float, 3> defaultDuckAmounts{1.0f, 0.5f, 0.0f};
    for (size_t band = 0; band < bandNames.size(); ++band)
    {
        const auto name = juce::String(bandNames[band]);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"duckAmount" + name, 1}, name + " Duck Amount",
            juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, defaultDuckAmounts[band],
            juce::AudioParameterFloatAttributes{}.withLabel("%")
                .withStringFromValueFunction([](float value, int maximumLength)
                {
                    const auto text = juce::String(juce::roundToInt(value * 100.0f)) + "%";
                    return maximumLength > 0 ? text.substring(0, maximumLength) : text;
                })));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{name.toLowerCase() + "Bypass", 1}, name + " Bypass", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{name.toLowerCase() + "Solo", 1}, name + " Solo", false));
    }

    return layout;
}

StepBloomAudioProcessor::StepBloomAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
      presetManager(parameters),
      waveformFifo(std::make_unique<WaveformSlot[]>(waveformFifoCapacity))
{
    const auto defaultPreset = presetManager.getDefaultPresetFile();
    if (defaultPreset.existsAsFile())
    {
        const auto result = presetManager.loadPreset(defaultPreset);
        if (result.failed())
            juce::Logger::writeToLog("StepBloom: " + result.getErrorMessage());
    }

    if (auto* scaleParameter = parameters.getParameter("userScaleFactor"))
    {
        const auto defaultZoom = presetManager.getUserDefaultZoom();
        scaleParameter->setValueNotifyingHost(scaleParameter->convertTo0to1(defaultZoom));
    }
}

void StepBloomAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    envelopePhase = 0.0;
    cycleDurationMilliseconds.store(1000.0f, std::memory_order_relaxed);
    isPlayingEnvelope = false;
    lastTriggerMode = TriggerMode::Sync;
    hasTriggerModeState = false;
    hasCrossoverModeState = false;
    lastRequestedMultiband = false;
    wasTransportPlaying = false;
    hasPreviousHostPpq = false;
    previousHostPpq = 0.0;
    sidechainWasAboveThreshold = false;
    triggerHoldOffSamplesRemaining = 0;
    waveformCycleId = 0;
    depthParameter = parameters.getRawParameterValue("depth");
    cycleParameter = parameters.getRawParameterValue("cycle");
    thresholdParameter = parameters.getRawParameterValue("threshold");
    holdOffParameter = parameters.getRawParameterValue("triggerHoldOff");
    triggerModeParameter = parameters.getRawParameterValue("triggerMode");
    processingModeParameter = parameters.getRawParameterValue("processingMode");
    constexpr std::array<const char*, 4> profileNames{"Master", "Low", "Mid", "High"};
    constexpr std::array<const char*, 6> fixedNodeNames{
        "Node1_X", "Node1_Y", "Node2_X", "Node2_Y", "Node3_X", "Node3_Y"};
    for (size_t profile = 0; profile < profileNames.size(); ++profile)
    {
        const auto prefix = juce::String(profileNames[profile]) + "_";
        for (size_t node = 0; node < fixedNodeNames.size(); ++node)
        {
            envelopeProfiles[profile].fixedNodes[node] = parameters.getRawParameterValue(
                prefix + fixedNodeNames[node]);
            auto& smoother = smoothedEnvelopeProfiles[profile][node];
            smoother.reset(sampleRate, 0.01);
            const auto defaultValue = node == 0 ? 3.0f
                : node == 1 ? 0.2f
                : node == 2 ? 0.6f
                : node == 3 ? 1.0f
                : node == 4 ? 0.95f : 1.0f;
            const auto initialValue = envelopeProfiles[profile].fixedNodes[node] != nullptr
                ? envelopeProfiles[profile].fixedNodes[node]->load() : defaultValue;
            smoother.setCurrentAndTargetValue(initialValue);
        }
        for (size_t extra = 0; extra < 2; ++extra)
        {
            const auto extraPrefix = prefix + "Extra" + juce::String(extra + 1);
            envelopeProfiles[profile].extraActive[extra] =
                parameters.getRawParameterValue(extraPrefix + "_Active");
            envelopeProfiles[profile].extraPhase[extra] =
                parameters.getRawParameterValue(extraPrefix + "_X");
            envelopeProfiles[profile].extraGain[extra] =
                parameters.getRawParameterValue(extraPrefix + "_Y");
        }
    }
    crossoverLowMidParameter = parameters.getRawParameterValue("crossoverLowMid");
    crossoverMidHighParameter = parameters.getRawParameterValue("crossoverMidHigh");
    constexpr std::array<const char*, 3> bandIds{"low", "mid", "high"};
    for (size_t band = 0; band < bandIds.size(); ++band)
    {
        const auto id = juce::String(bandIds[band]);
        duckAmountParameters[band] = parameters.getRawParameterValue(
            "duckAmount" + id.substring(0, 1).toUpperCase() + id.substring(1));
        bandBypassParameters[band] = parameters.getRawParameterValue(id + "Bypass");
        bandSoloParameters[band] = parameters.getRawParameterValue(id + "Solo");
        smoothedBandGains[band].reset(sampleRate, 0.003);
        smoothedBandGains[band].setCurrentAndTargetValue(1.0f);
    }
    smoothedModeBlend.reset(sampleRate, 0.003);
    smoothedModeBlend.setCurrentAndTargetValue(0.0f);
    const juce::dsp::ProcessSpec filterSpec{
        sampleRate, static_cast<uint32_t>(juce::jmax(1, samplesPerBlock)), 2};
    for (auto& filter : lowMidFilters)
    {
        filter.setType(CrossoverFilter::Type::lowpass);
        filter.prepare(filterSpec);
        filter.reset();
    }
    for (auto& filter : midHighFilters)
    {
        filter.setType(CrossoverFilter::Type::lowpass);
        filter.prepare(filterSpec);
        filter.reset();
    }
    lowMidFilters[1].setType(CrossoverFilter::Type::highpass);
    midHighFilters[1].setType(CrossoverFilter::Type::highpass);
    waveformWriteSequence.store(0, std::memory_order_relaxed);
    spectrumWriteSequence.store(0, std::memory_order_relaxed);
    for (auto& slot : spectrumFifo)
        slot.sequence.store(0, std::memory_order_relaxed);
    for (int i = 0; i < waveformFifoCapacity; ++i)
        waveformFifo[static_cast<size_t>(i)].sequence.store(0, std::memory_order_relaxed);
}

void StepBloomAudioProcessor::releaseResources()
{
}

bool StepBloomAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    const auto sidechain = layouts.getChannelSet(true, 1);
    return input == output
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo())
        && (sidechain.isDisabled()
            || sidechain == juce::AudioChannelSet::mono()
            || sidechain == juce::AudioChannelSet::stereo());
}

void StepBloomAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    auto outputBuffer = getBusBuffer(buffer, false, 0);
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);
    const auto numChannels = outputBuffer.getNumChannels();
    const auto depth = (depthParameter != nullptr ? depthParameter->load() : 100.0f) * 0.01f;
    const auto cycleChoice = cycleParameter != nullptr
        ? static_cast<int>(cycleParameter->load())
        : 0;
    const auto cycleBeats = 2.0 / static_cast<double>(1 << juce::jlimit(0, 3, cycleChoice));
    const auto thresholdDb = thresholdParameter != nullptr
        ? thresholdParameter->load()
        : -36.0f;
    const auto sidechainThreshold = std::pow(10.0f, static_cast<double>(thresholdDb) * 0.05);
    const auto holdOffMilliseconds = holdOffParameter != nullptr
        ? holdOffParameter->load()
        : 80.0f;
    const auto holdOffSamples = static_cast<int64_t>(
        std::round(static_cast<double>(holdOffMilliseconds) * 0.001 * getSampleRate()));
    const auto triggerModeIndex = triggerModeParameter != nullptr
        ? static_cast<int>(triggerModeParameter->load())
        : static_cast<int>(TriggerMode::Sync);
    const auto triggerMode = static_cast<TriggerMode>(
        juce::jlimit(static_cast<int>(TriggerMode::Sync),
                     static_cast<int>(TriggerMode::AudioTrigger), triggerModeIndex));
    if (!hasTriggerModeState || triggerMode != lastTriggerMode)
    {
        isPlayingEnvelope = triggerMode == TriggerMode::Sync;
        if (triggerMode != TriggerMode::Sync)
            envelopePhase = 1.0;
        lastTriggerMode = triggerMode;
        hasTriggerModeState = true;
    }
    const auto hasSidechain = sidechainBuffer.getNumChannels() > 0;
    const auto requestedMode = processingModeParameter != nullptr
        ? static_cast<int>(processingModeParameter->load()) : 0;
    const auto isMultibandRequested = requestedMode == 1;
    if (! hasCrossoverModeState || isMultibandRequested != lastRequestedMultiband)
    {
        if (isMultibandRequested)
            resetCrossoverFilters();
        lastRequestedMultiband = isMultibandRequested;
        hasCrossoverModeState = true;
    }
    smoothedModeBlend.setTargetValue(isMultibandRequested ? 1.0f : 0.0f);
    const auto maximumCutoff = juce::jmax(20.0f,
        static_cast<float>(getSampleRate() * 0.45));
    const auto requestedMidHigh = crossoverMidHighParameter != nullptr
        ? crossoverMidHighParameter->load() : 3000.0f;
    const auto midHighFrequency = juce::jlimit(20.0f, maximumCutoff, requestedMidHigh);
    const auto maximumLowMid = juce::jmin(2000.0f, midHighFrequency / 1.2f);
    const auto lowMidFrequency = juce::jlimit(20.0f, maximumLowMid,
        crossoverLowMidParameter != nullptr ? crossoverLowMidParameter->load() : 200.0f);
    const auto safeMidHighFrequency = juce::jmin(maximumCutoff,
        juce::jmax(midHighFrequency, lowMidFrequency * 1.2f));
    for (auto& filter : lowMidFilters)
        filter.setCutoffFrequency(lowMidFrequency);
    for (auto& filter : midHighFilters)
        filter.setCutoffFrequency(safeMidHighFrequency);

    std::array<float, 3> duckAmounts{};
    std::array<bool, 3> bandBypassed{};
    std::array<bool, 3> bandSoloed{};
    auto anyBandSoloed = false;
    for (size_t band = 0; band < duckAmounts.size(); ++band)
    {
        duckAmounts[band] = duckAmountParameters[band] != nullptr
            ? juce::jlimit(0.0f, 1.0f, duckAmountParameters[band]->load()) : 0.0f;
        bandBypassed[band] = bandBypassParameters[band] != nullptr
            && bandBypassParameters[band]->load() >= 0.5f;
        bandSoloed[band] = bandSoloParameters[band] != nullptr
            && bandSoloParameters[band]->load() >= 0.5f;
        anyBandSoloed = anyBandSoloed || bandSoloed[band];
    }

    std::array<std::array<ExtraEnvelopeNode, 2>, 4> extraNodes{};
    for (size_t profile = 0; profile < envelopeProfiles.size(); ++profile)
    {
        for (size_t node = 0; node < smoothedEnvelopeProfiles[profile].size(); ++node)
        {
            const auto* parameter = envelopeProfiles[profile].fixedNodes[node];
            if (parameter != nullptr)
                smoothedEnvelopeProfiles[profile][node].setTargetValue(parameter->load());
        }
        for (size_t extra = 0; extra < extraNodes[profile].size(); ++extra)
        {
            const auto& profileParameters = envelopeProfiles[profile];
            auto& node = extraNodes[profile][extra];
            node.active = profileParameters.extraActive[extra] != nullptr
                && profileParameters.extraActive[extra]->load() >= 0.5f;
            node.phase = profileParameters.extraPhase[extra] != nullptr
                ? juce::jlimit(0.05f, 0.95f, profileParameters.extraPhase[extra]->load())
                : 0.5f;
            node.gain = profileParameters.extraGain[extra] != nullptr
                ? juce::jlimit(0.0f, 1.0f, profileParameters.extraGain[extra]->load())
                : 0.5f;
        }
    }

    auto bpm = 120.0;
    auto ppqPosition = 0.0;
    auto useHostPosition = false;
    auto hostIsPlaying = false;
    auto hasHostPpqPosition = false;
    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            hostIsPlaying = position->getIsPlaying();
            if (const auto hostBpm = position->getBpm())
                bpm = juce::jlimit(minimumBpm, maximumBpm, *hostBpm);

            if (const auto ppq = position->getPpqPosition())
            {
                ppqPosition = *ppq;
                hasHostPpqPosition = true;
                useHostPosition = hostIsPlaying;
            }
        }
    }

    const auto beatsPerSample = bpm / (60.0 * getSampleRate());
    const auto seekTolerance = juce::jmax(beatsPerSample * 4.0, 0.02);
    const auto playheadJumped = hostIsPlaying && hasHostPpqPosition
        && hasPreviousHostPpq
        && std::abs(ppqPosition - previousHostPpq) > seekTolerance;
    const auto transportStarted = hostIsPlaying && ! wasTransportPlaying;
    if (transportStarted || playheadJumped)
        ++waveformCycleId;

    wasTransportPlaying = hostIsPlaying;
    hasPreviousHostPpq = hostIsPlaying && hasHostPpqPosition;
    if (hasPreviousHostPpq)
        previousHostPpq = ppqPosition
            + static_cast<double>(numSamples) * beatsPerSample;

    const auto samplesPerCycle = juce::jmax(1.0, cycleBeats / beatsPerSample);
    const auto phaseIncrement = 1.0 / samplesPerCycle;
    const auto cycleMilliseconds = static_cast<float>(
        samplesPerCycle * 1000.0 / getSampleRate());
    cycleDurationMilliseconds.store(cycleMilliseconds, std::memory_order_relaxed);
    auto midiEvent = midiMessages.cbegin();
    const auto midiEnd = midiMessages.cend();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (triggerHoldOffSamplesRemaining > 0)
            --triggerHoldOffSamplesRemaining;

        auto midiNoteOn = false;
        while (midiEvent != midiEnd && (*midiEvent).samplePosition <= sample)
        {
            if ((*midiEvent).getMessage().isNoteOn())
                midiNoteOn = true;
            ++midiEvent;
        }

        auto sidechainSample = 0.0f;
        auto sidechainCrossing = false;
        if (hasSidechain)
        {
            auto sidechainLevel = 0.0f;
            for (int channel = 0; channel < sidechainBuffer.getNumChannels(); ++channel)
            {
                const auto inputSample = sidechainBuffer.getReadPointer(channel)[sample];
                const auto absoluteSample = std::abs(inputSample);
                sidechainLevel = juce::jmax(sidechainLevel, absoluteSample);
                if (absoluteSample > std::abs(sidechainSample))
                    sidechainSample = inputSample;
            }

            const auto isAboveThreshold = sidechainLevel >= sidechainThreshold;
            sidechainCrossing = isAboveThreshold && !sidechainWasAboveThreshold;
            sidechainWasAboveThreshold = isAboveThreshold;
        }
        else
        {
            sidechainWasAboveThreshold = false;
        }

        double phase = 1.0;
        switch (triggerMode)
        {
            case TriggerMode::Sync:
                isPlayingEnvelope = true;
                if (useHostPosition)
                {
                    phase = std::fmod(
                        (ppqPosition + static_cast<double>(sample) * beatsPerSample)
                            / cycleBeats,
                        1.0);
                    if (phase < 0.0)
                        phase += 1.0;
                    if (phase + 1.0e-9 < envelopePhase)
                        ++waveformCycleId;
                    envelopePhase = phase;
                }
                else
                {
                    phase = envelopePhase;
                    envelopePhase += phaseIncrement;
                    if (envelopePhase >= 1.0)
                    {
                        envelopePhase -= 1.0;
                        ++waveformCycleId;
                    }
                }
                break;

            case TriggerMode::MidiTrigger:
                if (midiNoteOn)
                {
                    envelopePhase = 0.0;
                    isPlayingEnvelope = true;
                    ++waveformCycleId;
                }
                if (isPlayingEnvelope)
                    phase = envelopePhase;
                break;

            case TriggerMode::AudioTrigger:
                if (sidechainCrossing && triggerHoldOffSamplesRemaining == 0)
                {
                    envelopePhase = 0.0;
                    isPlayingEnvelope = true;
                    triggerHoldOffSamplesRemaining = holdOffSamples;
                    ++waveformCycleId;
                }
                if (isPlayingEnvelope)
                    phase = envelopePhase;
                break;
        }

        std::array<float, 4> envelopeGains{};
        for (size_t profile = 0; profile < envelopeGains.size(); ++profile)
        {
            auto& smoothers = smoothedEnvelopeProfiles[profile];
            const auto attackTime = smoothers[0].getNextValue();
            const auto startGain = smoothers[1].getNextValue();
            const auto duckWidth = smoothers[2].getNextValue();
            const auto tension = smoothers[3].getNextValue();
            const auto tailOffset = smoothers[4].getNextValue();
            const auto endGain = smoothers[5].getNextValue();
            const auto attackPhase = static_cast<float>(
                static_cast<double>(attackTime) / juce::jmax(cycleMilliseconds, 1.0e-6f));
            const auto boundedAttackPhase = juce::jmin(attackPhase, duckWidth * 0.5f);
            envelopeGains[profile] = isPlayingEnvelope
                ? evaluateProfileEnvelope(static_cast<float>(phase), boundedAttackPhase,
                    duckWidth, startGain, tension, tailOffset, endGain, extraNodes[profile])
                : 1.0f;
        }
        std::array<float, 3> effectiveBandGains{};
        for (size_t band = 0; band < effectiveBandGains.size(); ++band)
        {
            const auto isExcludedBySolo = anyBandSoloed && ! bandSoloed[band];
            const auto amount = bandBypassed[band] ? 0.0f : depth * duckAmounts[band];
            const auto targetGain = isExcludedBySolo
                ? 0.0f : 1.0f - amount * (1.0f - envelopeGains[band + 1]);
            smoothedBandGains[band].setTargetValue(targetGain);
            effectiveBandGains[band] = smoothedBandGains[band].getNextValue();
        }

        auto processedOutputSample = 0.0f;
        auto spectrumSample = 0.0f;
        const auto modeBlend = smoothedModeBlend.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& output = outputBuffer.getWritePointer(channel)[sample];
            const auto inputSample = output;
            spectrumSample += inputSample;
            const auto singlebandGain = 1.0f - depth * (1.0f - envelopeGains[0]);
            const auto singlebandOutput = inputSample * singlebandGain;
            if (isMultibandRequested || modeBlend > 0.0f)
            {
                const auto lowSample = lowMidFilters[0].processSample(channel, inputSample);
                const auto midHighSample = lowMidFilters[1].processSample(channel, inputSample);
                const auto midSample = midHighFilters[0].processSample(channel, midHighSample);
                const auto highSample = midHighFilters[1].processSample(channel, midHighSample);
                const auto multibandOutput = lowSample * effectiveBandGains[0]
                    + midSample * effectiveBandGains[1]
                    + highSample * effectiveBandGains[2];
                output = singlebandOutput + (multibandOutput - singlebandOutput) * modeBlend;
            }
            else
            {
                output = singlebandOutput;
            }
            processedOutputSample += output;
        }
        if (numChannels > 0)
        {
            processedOutputSample /= static_cast<float>(numChannels);
            spectrumSample /= static_cast<float>(numChannels);
        }

        const auto spectrumSequence = spectrumWriteSequence.load(std::memory_order_relaxed);
        auto& spectrumSlot = spectrumFifo[static_cast<size_t>(
            spectrumSequence & (spectrumFifoCapacity - 1))];
        spectrumSlot.sample.store(spectrumSample, std::memory_order_relaxed);
        spectrumSlot.sequence.store(spectrumSequence + 1, std::memory_order_release);
        spectrumWriteSequence.store(spectrumSequence + 1, std::memory_order_release);

        const auto writeSequence = waveformWriteSequence.load(std::memory_order_relaxed);
        auto& slot = waveformFifo[static_cast<size_t>(
            writeSequence & (waveformFifoCapacity - 1))];
        slot.sidechain.store(sidechainSample, std::memory_order_relaxed);
        slot.processedOutput.store(processedOutputSample, std::memory_order_relaxed);
        slot.phase.store(static_cast<float>(phase), std::memory_order_relaxed);
        slot.cycleId.store(waveformCycleId, std::memory_order_relaxed);
        slot.sequence.store(writeSequence + 1, std::memory_order_release);
        waveformWriteSequence.store(writeSequence + 1, std::memory_order_release);

        if (triggerMode != TriggerMode::Sync && isPlayingEnvelope)
        {
            envelopePhase += phaseIncrement;
            if (envelopePhase >= 1.0)
            {
                envelopePhase = 1.0;
                isPlayingEnvelope = false;
            }
        }

    }
}

uint64_t StepBloomAudioProcessor::getLatestWaveformSequence() const noexcept
{
    return waveformWriteSequence.load(std::memory_order_acquire);
}

bool StepBloomAudioProcessor::readWaveformFrame(
    uint64_t sequence, WaveformFrame& frame) const noexcept
{
    const auto latestSequence = waveformWriteSequence.load(std::memory_order_acquire);
    if (sequence >= latestSequence || latestSequence - sequence > waveformFifoCapacity)
        return false;

    const auto expectedSequence = sequence + 1;
    const auto& slot = waveformFifo[static_cast<size_t>(
        sequence & (waveformFifoCapacity - 1))];
    if (slot.sequence.load(std::memory_order_acquire) != expectedSequence)
        return false;

    frame.sidechain = slot.sidechain.load(std::memory_order_relaxed);
    frame.processedOutput = slot.processedOutput.load(std::memory_order_relaxed);
    frame.phase = slot.phase.load(std::memory_order_relaxed);
    frame.cycleId = slot.cycleId.load(std::memory_order_relaxed);
    return slot.sequence.load(std::memory_order_acquire) == expectedSequence;
}

uint64_t StepBloomAudioProcessor::getLatestSpectrumSequence() const noexcept
{
    return spectrumWriteSequence.load(std::memory_order_acquire);
}

bool StepBloomAudioProcessor::readSpectrumSample(uint64_t sequence,
                                                 float& sample) const noexcept
{
    const auto latestSequence = spectrumWriteSequence.load(std::memory_order_acquire);
    if (sequence >= latestSequence || latestSequence - sequence > spectrumFifoCapacity)
        return false;

    const auto expectedSequence = sequence + 1;
    const auto& slot = spectrumFifo[static_cast<size_t>(
        sequence & (spectrumFifoCapacity - 1))];
    if (slot.sequence.load(std::memory_order_acquire) != expectedSequence)
        return false;

    sample = slot.sample.load(std::memory_order_relaxed);
    return slot.sequence.load(std::memory_order_acquire) == expectedSequence;
}

void StepBloomAudioProcessor::resetCrossoverFilters() noexcept
{
    for (auto& filter : lowMidFilters)
        filter.reset();
    for (auto& filter : midHighFilters)
        filter.reset();
}

juce::AudioProcessorEditor* StepBloomAudioProcessor::createEditor()
{
    return new StepBloomAudioProcessorEditor(*this);
}

bool StepBloomAudioProcessor::hasEditor() const { return true; }
const juce::String StepBloomAudioProcessor::getName() const { return JucePlugin_Name; }
bool StepBloomAudioProcessor::acceptsMidi() const { return true; }
bool StepBloomAudioProcessor::producesMidi() const { return false; }
bool StepBloomAudioProcessor::isMidiEffect() const { return false; }
double StepBloomAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int StepBloomAudioProcessor::getNumPrograms() { return 1; }
int StepBloomAudioProcessor::getCurrentProgram() { return 0; }
void StepBloomAudioProcessor::setCurrentProgram(int) {}
const juce::String StepBloomAudioProcessor::getProgramName(int) { return {}; }
void StepBloomAudioProcessor::changeProgramName(int, const juce::String&) {}

void StepBloomAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    const auto state = parameters.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void StepBloomAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml(*xml);
            migrateLegacyEnvelopeState(restoredState);
            parameters.replaceState(restoredState);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StepBloomAudioProcessor();
}
