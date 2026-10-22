#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>

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
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"duckWidth", 1}, "Duck Width",
        juce::NormalisableRange<float>{0.1f, 0.95f, 0.001f}, 0.6f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"attackTime", 1}, "Attack Time",
        juce::NormalisableRange<float>{0.0f, 15.0f, 0.01f}, 3.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"minGain", 1}, "Start Gain",
        juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.2f,
        juce::AudioParameterFloatAttributes{}.withLabel("%")
            .withStringFromValueFunction([](float value, int maximumLength)
            {
                const auto text = juce::String(juce::roundToInt(value * 100.0f)) + "%";
                return maximumLength > 0 ? text.substring(0, maximumLength) : text;
            })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"releaseTension", 1}, "Curve Tension",
        juce::NormalisableRange<float>{0.2f, 1.5f, 0.001f}, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"endFadeOffset", 1}, "End Fade Offset",
        juce::NormalisableRange<float>{0.8f, 1.0f, 0.001f}, 0.95f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"endGain", 1}, "End Gain",
        juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 1.0f));

    return layout;
}

StepBloomAudioProcessor::StepBloomAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::mono(), false)
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
}

void StepBloomAudioProcessor::prepareToPlay(double sampleRate, int)
{
    fallbackPpq = 0.0;
    envelopePhase = 1.0;
    cycleDurationMilliseconds.store(1000.0f, std::memory_order_relaxed);
    smoothedGain = 1.0;
    smoothingCoefficient = std::exp(-1.0 / (0.003 * sampleRate));
    isPlayingEnvelope = false;
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
    duckWidthParameter = parameters.getRawParameterValue("duckWidth");
    attackTimeParameter = parameters.getRawParameterValue("attackTime");
    minGainParameter = parameters.getRawParameterValue("minGain");
    releaseTensionParameter = parameters.getRawParameterValue("releaseTension");
    endFadeOffsetParameter = parameters.getRawParameterValue("endFadeOffset");
    endGainParameter = parameters.getRawParameterValue("endGain");
    const auto initialDuckWidth = duckWidthParameter != nullptr
        ? duckWidthParameter->load()
        : 0.6f;
    const auto initialAttackTime = attackTimeParameter != nullptr ? attackTimeParameter->load() : 3.0f;
    const auto initialMinGain = minGainParameter != nullptr ? minGainParameter->load() : 0.2f;
    const auto initialReleaseTension = releaseTensionParameter != nullptr
        ? releaseTensionParameter->load()
        : 0.0f;
    const auto initialEndFadeOffset = endFadeOffsetParameter != nullptr
        ? endFadeOffsetParameter->load()
        : 0.95f;
    const auto initialEndGain = endGainParameter != nullptr ? endGainParameter->load() : 1.0f;
    smoothedDuckWidth.reset(sampleRate, 0.01);
    smoothedDuckWidth.setCurrentAndTargetValue(initialDuckWidth);
    smoothedAttackTime.reset(sampleRate, 0.01);
    smoothedAttackTime.setCurrentAndTargetValue(initialAttackTime);
    smoothedMinGain.reset(sampleRate, 0.01);
    smoothedMinGain.setCurrentAndTargetValue(initialMinGain);
    smoothedReleaseTension.reset(sampleRate, 0.01);
    smoothedReleaseTension.setCurrentAndTargetValue(initialReleaseTension);
    smoothedEndFadeOffset.reset(sampleRate, 0.01);
    smoothedEndFadeOffset.setCurrentAndTargetValue(initialEndFadeOffset);
    smoothedEndGain.reset(sampleRate, 0.01);
    smoothedEndGain.setCurrentAndTargetValue(initialEndGain);
    waveformWriteSequence.store(0, std::memory_order_relaxed);
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
    const auto hasSidechain = sidechainBuffer.getNumChannels() > 0;

    smoothedDuckWidth.setTargetValue(duckWidthParameter != nullptr
        ? duckWidthParameter->load()
        : 0.6f);
    smoothedAttackTime.setTargetValue(attackTimeParameter != nullptr
        ? attackTimeParameter->load()
        : 3.0f);
    smoothedMinGain.setTargetValue(minGainParameter != nullptr
        ? minGainParameter->load()
        : 0.2f);
    smoothedReleaseTension.setTargetValue(releaseTensionParameter != nullptr
        ? releaseTensionParameter->load()
        : 0.0f);
    smoothedEndFadeOffset.setTargetValue(endFadeOffsetParameter != nullptr
        ? endFadeOffsetParameter->load()
        : 0.95f);
    smoothedEndGain.setTargetValue(endGainParameter != nullptr
        ? endGainParameter->load()
        : 1.0f);

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
        const auto globalPpq = useHostPosition
            ? ppqPosition + static_cast<double>(sample) * beatsPerSample
            : fallbackPpq;
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

        if (triggerMode == TriggerMode::MidiTrigger && midiNoteOn)
        {
            envelopePhase = 0.0;
            isPlayingEnvelope = true;
            ++waveformCycleId;
        }

        if (triggerMode == TriggerMode::AudioTrigger
            && sidechainCrossing
            && triggerHoldOffSamplesRemaining == 0)
        {
            envelopePhase = 0.0;
            isPlayingEnvelope = true;
            triggerHoldOffSamplesRemaining = holdOffSamples;
            ++waveformCycleId;
        }

        double phase = 1.0;
        if (triggerMode == TriggerMode::Sync)
        {
            phase = useHostPosition
                ? std::fmod(globalPpq / cycleBeats, 1.0)
                : std::fmod(fallbackPpq / cycleBeats, 1.0);
            if (phase < 0.0)
                phase += 1.0;
            if (phase + 1.0e-9 < envelopePhase)
                ++waveformCycleId;
            envelopePhase = phase;
        }
        else if (isPlayingEnvelope)
        {
            phase = envelopePhase;
        }

        const auto duckWidth = smoothedDuckWidth.getNextValue();
        const auto attackTime = smoothedAttackTime.getNextValue();
        const auto minGain = smoothedMinGain.getNextValue();
        const auto releaseTension = juce::jlimit(
            0.2f, 1.5f, smoothedReleaseTension.getNextValue());
        const auto endFadeOffset = smoothedEndFadeOffset.getNextValue();
        const auto endGain = smoothedEndGain.getNextValue();
        const auto attackPhase = static_cast<float>(
            static_cast<double>(attackTime) / juce::jmax(cycleMilliseconds, 1.0e-6f));
        const auto boundedAttackPhase = juce::jmin(attackPhase, duckWidth * 0.5f);
        const auto envelope = evaluateEnvelope(
            static_cast<float>(phase), boundedAttackPhase, duckWidth,
            minGain, releaseTension, endFadeOffset, endGain);
        const auto activeEnvelope = triggerMode == TriggerMode::Sync
            ? (! hasHostPpqPosition || hostIsPlaying)
            : isPlayingEnvelope;
        const auto targetGain = activeEnvelope ? 1.0f - depth + depth * envelope : 1.0f;
        smoothedGain = smoothingCoefficient * smoothedGain
            + (1.0 - smoothingCoefficient) * static_cast<double>(targetGain);

        auto processedOutputSample = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& output = outputBuffer.getWritePointer(channel)[sample];
            output *= static_cast<float>(smoothedGain);
            processedOutputSample += output;
        }
        if (numChannels > 0)
            processedOutputSample /= static_cast<float>(numChannels);

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

        if (!useHostPosition)
            fallbackPpq += beatsPerSample;
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
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StepBloomAudioProcessor();
}
