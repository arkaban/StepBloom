#pragma once

#include "PluginProcessor.h"

#include <array>

class StepBloomLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int,
                      juce::ComboBox&) override;
};

class StepBloomAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit StepBloomAudioProcessorEditor(StepBloomAudioProcessor&);
    ~StepBloomAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class SettingsButton final : public juce::Button
    {
    public:
        SettingsButton();
        void paintButton(juce::Graphics&, bool, bool) override;
    };

    class CrossoverPanelComponent final : public juce::Component, private juce::Timer
    {
    public:
        explicit CrossoverPanelComponent(StepBloomAudioProcessor&);
        ~CrossoverPanelComponent() override;
        void paint(juce::Graphics&) override;
        void paintOverChildren(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        static constexpr int fftOrder = 11;
        static constexpr int fftSize = 1 << fftOrder;
        static constexpr int spectrumBinCount = fftSize / 2;

        void timerCallback() override;
        float frequencyToX(float frequency) const;
        float xToFrequency(float x) const;

        StepBloomAudioProcessor& processor;
        juce::Rectangle<float> spectrumArea;
        juce::dsp::FFT fft{fftOrder};
        juce::dsp::WindowingFunction<float> window{
            fftSize, juce::dsp::WindowingFunction<float>::hann, false};
        std::array<float, fftSize * 2> fftData{};
        std::array<float, spectrumBinCount> magnitudes{};
        uint64_t lastSpectrumSequence = 0;
        std::array<juce::Slider, 3> duckAmountSliders;
        std::array<juce::Label, 3> bandLabels;
        std::array<juce::TextButton, 3> bypassButtons;
        std::array<juce::TextButton, 3> soloButtons;
        std::array<std::unique_ptr<StepBloomAudioProcessor::Parameters::SliderAttachment>, 3>
            duckAmountAttachments;
        std::array<std::unique_ptr<StepBloomAudioProcessor::Parameters::ButtonAttachment>, 3>
            bypassAttachments;
        std::array<std::unique_ptr<StepBloomAudioProcessor::Parameters::ButtonAttachment>, 3>
            soloAttachments;
        int draggedCrossover = 0;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossoverPanelComponent)
    };

    class ThreeNodeEnvelopeEditor final : public juce::Component, private juce::Timer
    {
    public:
        ThreeNodeEnvelopeEditor(StepBloomAudioProcessor&);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        static constexpr int waveformBinCount = 1024;

        void timerCallback() override;
        int gridDivisionCount() const;
        void drawAdaptiveGrid(juce::Graphics&) const;
        void clearWaveformBins();
        void addWaveformFrame(const StepBloomAudioProcessor::WaveformFrame&);
        void drawWaveformLayer(juce::Graphics&, juce::Colour,
                               const std::array<float, waveformBinCount>&,
                               const std::array<float, waveformBinCount>&,
                               const std::array<uint32_t, waveformBinCount>&,
                               float amplitudeScale) const;
        juce::Point<float> getStartNodePosition(float attackTime, float minGain,
                                                float duckWidth) const;
        juce::Point<float> getEndNodePosition(float duckWidth, float releaseTension) const;
        juce::Point<float> getTailNodePosition(float endFadeOffset, float endGain) const;
        float getAttackPhase(float attackTime, float duckWidth) const;
        void updateNode(juce::Point<float>, bool shouldSnap);
        void endGesture();

        StepBloomAudioProcessor& processor;
        StepBloomAudioProcessor::Parameters& parameters;
        juce::Rectangle<float> plotArea;
        std::array<float, waveformBinCount> sidechainMinimum{};
        std::array<float, waveformBinCount> sidechainMaximum{};
        std::array<float, waveformBinCount> outputMinimum{};
        std::array<float, waveformBinCount> outputMaximum{};
        std::array<uint32_t, waveformBinCount> waveformCounts{};
        uint64_t nextWaveformSequence = 0;
        uint32_t displayCycleId = 0;
        bool hasDisplayCycle = false;
        enum class DraggedNode
        {
            None,
            Start,
            Recovery,
            End
        };
        DraggedNode draggedNode = DraggedNode::None;
        bool showSnapFeedback = false;
        bool snappedXActive = false;
        bool snappedYActive = false;
        float snappedGridX = 0.0f;
        float snappedGridY = 0.0f;
        juce::String dragTooltip;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreeNodeEnvelopeEditor)
    };

    StepBloomAudioProcessor& processor;
    StepBloomLookAndFeel lookAndFeel;
    ThreeNodeEnvelopeEditor envelopeEditor;
    CrossoverPanelComponent crossoverPanel;
    juce::TextButton previousPresetButton{"<"};
    juce::TextButton nextPresetButton{">"};
    juce::TextButton singlebandModeButton{"SINGLE"};
    juce::TextButton multibandModeButton{"MULTI"};
    juce::ComboBox presetBox;
    SettingsButton settingsButton;
    juce::Slider depthSlider;
    juce::Slider thresholdSlider;
    juce::Slider holdOffSlider;
    juce::ComboBox triggerModeBox;
    juce::ComboBox cycleBox;
    juce::Label depthLabel;
    juce::Label thresholdLabel;
    juce::Label holdOffLabel;
    juce::Label triggerModeLabel;
    juce::Label cycleLabel;
    std::unique_ptr<StepBloomAudioProcessor::Parameters::SliderAttachment> depthAttachment;
    std::unique_ptr<StepBloomAudioProcessor::Parameters::SliderAttachment> thresholdAttachment;
    std::unique_ptr<StepBloomAudioProcessor::Parameters::SliderAttachment> holdOffAttachment;
    std::unique_ptr<StepBloomAudioProcessor::Parameters::ComboBoxAttachment> triggerModeAttachment;
    std::unique_ptr<StepBloomAudioProcessor::Parameters::ComboBoxAttachment> cycleAttachment;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    std::atomic<float>* processingModeValue = nullptr;

    void timerCallback() override;
    void refreshPresetList();
    void showPresetResult(const juce::Result&);
    void showSettingsMenu();
    void setProcessingMode(bool multiband);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepBloomAudioProcessorEditor)
};
