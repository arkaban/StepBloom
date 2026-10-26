#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "PresetManager.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

class StepBloomAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int waveformFifoCapacity = 524288;
    static constexpr uint64_t spectrumFifoCapacity = 4096;
    using Parameters = juce::AudioProcessorValueTreeState;

    enum class TriggerMode
    {
        Sync = 0,
        MidiTrigger,
        AudioTrigger
    };

    struct WaveformFrame
    {
        float sidechain = 0.0f;
        float processedOutput = 0.0f;
        float phase = 0.0f;
        uint32_t cycleId = 0;
    };

    StepBloomAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;
    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    uint64_t getLatestWaveformSequence() const noexcept;
    bool readWaveformFrame(uint64_t sequence, WaveformFrame& frame) const noexcept;
    uint64_t getLatestSpectrumSequence() const noexcept;
    bool readSpectrumSample(uint64_t sequence, float& sample) const noexcept;
    void resetCrossoverFilters() noexcept;
    float getCycleDurationMilliseconds() const noexcept;
    PresetManager& getPresetManager() noexcept;
    static float evaluateEnvelope(float phase, float attackPhase, float duckWidth,
                                  float startGain, float curveTension,
                                  float endFadeOffset, float endGain) noexcept;
    struct ExtraEnvelopeNode
    {
        bool active = false;
        float phase = 0.5f;
        float gain = 0.5f;
    };
    static float evaluateProfileEnvelope(float phase, float attackPhase, float duckWidth,
                                         float startGain, float curveTension,
                                         float endFadeOffset, float endGain,
                                         const std::array<ExtraEnvelopeNode, 2>& extras) noexcept;
    static void migrateLegacyEnvelopeState(juce::ValueTree state);

    Parameters parameters;
    PresetManager presetManager;

private:
    static Parameters::ParameterLayout createParameterLayout();

    double envelopePhase = 0.0;
    std::atomic<float> cycleDurationMilliseconds{1000.0f};
    bool isPlayingEnvelope = false;
    TriggerMode lastTriggerMode = TriggerMode::Sync;
    bool hasTriggerModeState = false;
    bool hasCrossoverModeState = false;
    bool lastRequestedMultiband = false;
    bool wasTransportPlaying = false;
    bool hasPreviousHostPpq = false;
    double previousHostPpq = 0.0;
    bool sidechainWasAboveThreshold = false;
    int64_t triggerHoldOffSamplesRemaining = 0;
    uint32_t waveformCycleId = 0;
    std::atomic<float>* depthParameter = nullptr;
    std::atomic<float>* cycleParameter = nullptr;
    std::atomic<float>* thresholdParameter = nullptr;
    std::atomic<float>* holdOffParameter = nullptr;
    std::atomic<float>* triggerModeParameter = nullptr;
    std::atomic<float>* processingModeParameter = nullptr;
    struct EnvelopeProfileParameters
    {
        std::array<std::atomic<float>*, 6> fixedNodes{};
        std::array<std::atomic<float>*, 2> extraActive{};
        std::array<std::atomic<float>*, 2> extraPhase{};
        std::array<std::atomic<float>*, 2> extraGain{};
    };
    std::array<EnvelopeProfileParameters, 4> envelopeProfiles{};
    std::array<std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 6>, 4>
        smoothedEnvelopeProfiles;
    std::atomic<float>* crossoverLowMidParameter = nullptr;
    std::atomic<float>* crossoverMidHighParameter = nullptr;
    std::array<std::atomic<float>*, 3> duckAmountParameters{};
    std::array<std::atomic<float>*, 3> bandBypassParameters{};
    std::array<std::atomic<float>*, 3> bandSoloParameters{};
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 3>
        smoothedBandGains;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedModeBlend;
    using CrossoverFilter = juce::dsp::LinkwitzRileyFilter<float>;
    std::array<CrossoverFilter, 2> lowMidFilters;
    std::array<CrossoverFilter, 2> midHighFilters;
    struct SpectrumSlot
    {
        std::atomic<float> sample{0.0f};
        std::atomic<uint64_t> sequence{0};
    };
    std::array<SpectrumSlot, spectrumFifoCapacity> spectrumFifo{};
    std::atomic<uint64_t> spectrumWriteSequence{0};
    struct WaveformSlot
    {
        std::atomic<float> sidechain{0.0f};
        std::atomic<float> processedOutput{0.0f};
        std::atomic<float> phase{0.0f};
        std::atomic<uint32_t> cycleId{0};
        std::atomic<uint64_t> sequence{0};
    };
    std::unique_ptr<WaveformSlot[]> waveformFifo;
    std::atomic<uint64_t> waveformWriteSequence{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepBloomAudioProcessor)
};
