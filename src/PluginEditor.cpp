#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
const juce::Colour backgroundColour{0xff090a08};
const juce::Colour panelColour{0xff171810};
const juce::Colour panelHighlightColour{0xff39391f};
const juce::Colour gridColour{0xff48421c};
const juce::Colour accentColour{0xffffd429};
const juce::Colour accentHighlightColour{0xfffff0a0};
const juce::Colour textColour{0xfffff7d7};
constexpr int baseWidth = 840;
constexpr int baseHeightSingle = 480;
constexpr int baseHeightMulti = 680;

struct ShapeProfile
{
    const char* name;
    float attackMilliseconds;
    float floor;
    float width;
    float tension;
    float tailOffset;
    float endGain;
};

constexpr std::array<ShapeProfile, 14> shapeProfiles{{
    {"Standard 4/4 Duck", 3.0f, 0.12f, 0.58f, 0.8f, 0.96f, 1.0f},
    {"Punchy Exponential", 1.0f, 0.04f, 0.42f, 1.5f, 0.96f, 1.0f},
    {"Sub Clean", 2.0f, 0.16f, 0.82f, 0.6f, 0.98f, 1.0f},
    {"Tight Off-Beat", 0.5f, 0.03f, 0.28f, 1.25f, 0.9f, 1.0f},
    {"Hard Square Drop", 0.0f, 0.0f, 0.35f, 1.5f, 0.8f, 1.0f},
    {"Inverse Duck / Gate", 0.0f, 0.95f, 0.45f, 0.5f, 0.82f, 0.0f},
    {"Double Duck", 2.0f, 0.08f, 0.36f, 1.2f, 0.91f, 0.22f},
    {"Logarithmic Smooth", 6.0f, 0.18f, 0.72f, 0.35f, 0.98f, 1.0f},
    {"S-Curve Rise", 3.0f, 0.1f, 0.62f, 1.0f, 0.97f, 1.0f},
    {"Mid-Scoop / Dip", 2.0f, 0.65f, 0.55f, 1.0f, 0.94f, 0.65f},
    {"Long Tail Release", 4.0f, 0.12f, 0.68f, 0.55f, 0.8f, 1.0f},
    {"Aggressive Transient Scoop", 0.0f, 0.0f, 0.22f, 1.5f, 0.9f, 1.0f},
    {"Subtle Wobble", 3.0f, 0.4f, 0.45f, 0.8f, 0.9f, 0.55f},
    {"Flat / Bypass Shape", 3.0f, 1.0f, 0.6f, 1.0f, 0.95f, 1.0f}
}};
}

juce::Font StepBloomLookAndFeel::getCustomFont(float size, int styleFlags)
{
    return juce::Font{juce::FontOptions(size, styleFlags)};
}

juce::Font StepBloomLookAndFeel::getTextButtonFont(
    juce::TextButton&, int buttonHeight)
{
    return getCustomFont(juce::jlimit(8.0f, 12.0f,
        static_cast<float>(buttonHeight) * 0.45f), juce::Font::bold);
}

juce::Font StepBloomLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return getCustomFont(12.0f);
}

juce::Font StepBloomLookAndFeel::getLabelFont(juce::Label&)
{
    return getCustomFont(10.0f);
}

juce::Font StepBloomLookAndFeel::getPopupMenuFont()
{
    return getCustomFont(12.0f);
}

void StepBloomLookAndFeel::drawRotarySlider(
    juce::Graphics& graphics, int x, int y, int width, int height,
    float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider&)
{
    const auto diameter = static_cast<float>(juce::jmin(width, height)) * 0.82f;
    const auto radius = diameter * 0.5f;
    const auto centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.48f;
    const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);

    graphics.setColour(juce::Colours::black.withAlpha(0.8f));
    graphics.fillEllipse(centreX - radius, centreY - radius + 4.0f,
                         diameter, diameter);

    graphics.setGradientFill(juce::ColourGradient(
        panelHighlightColour, centreX, centreY - radius,
        backgroundColour, centreX, centreY + radius, false));
    graphics.fillEllipse(centreX - radius, centreY - radius, diameter, diameter);
    graphics.setColour(accentColour.withAlpha(0.85f));
    graphics.drawEllipse(centreX - radius, centreY - radius, diameter, diameter, 1.5f);

    const auto faceRadius = radius * 0.82f;
    graphics.setGradientFill(juce::ColourGradient(
        juce::Colour{0xff57502a}, centreX - faceRadius * 0.35f, centreY - faceRadius * 0.7f,
        juce::Colour{0xff10110d}, centreX + faceRadius * 0.45f,
        centreY + faceRadius, true));
    graphics.fillEllipse(centreX - faceRadius, centreY - faceRadius,
                         faceRadius * 2.0f, faceRadius * 2.0f);

    juce::Path arc;
    arc.addCentredArc(centreX, centreY, radius * 0.94f, radius * 0.94f,
                      0.0f, rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour(juce::Colours::black.withAlpha(0.9f));
    graphics.strokePath(arc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, radius * 0.94f, radius * 0.94f,
                           0.0f, rotaryStartAngle, angle, true);
    graphics.setColour(accentColour);
    graphics.strokePath(valueArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

    const auto pointerLength = faceRadius * 0.68f;
    const auto pointerEnd = juce::Point<float>{
        centreX + std::cos(angle - juce::MathConstants<float>::halfPi) * pointerLength,
        centreY + std::sin(angle - juce::MathConstants<float>::halfPi) * pointerLength
    };
    graphics.setColour(juce::Colours::black.withAlpha(0.8f));
    graphics.drawLine(centreX + 1.0f, centreY + 2.0f,
                      pointerEnd.x + 1.0f, pointerEnd.y + 2.0f, 4.0f);
    graphics.setColour(accentHighlightColour);
    graphics.drawLine(centreX, centreY, pointerEnd.x, pointerEnd.y, 2.5f);
    graphics.setColour(accentColour);
    graphics.fillEllipse(centreX - 3.5f, centreY - 3.5f, 7.0f, 7.0f);
}

void StepBloomLookAndFeel::drawComboBox(
    juce::Graphics& graphics, int width, int height, bool isButtonDown,
    int, int, int, int, juce::ComboBox&)
{
    const auto bounds = juce::Rectangle<float>{
        1.0f, 1.0f, static_cast<float>(width) - 2.0f, static_cast<float>(height) - 3.0f
    };
    graphics.setColour(juce::Colours::black.withAlpha(0.9f));
    graphics.fillRoundedRectangle(bounds.translated(0.0f, 3.0f), 7.0f);
    graphics.setGradientFill(juce::ColourGradient(
        isButtonDown ? juce::Colour{0xff4a421d} : panelHighlightColour,
        bounds.getCentreX(), bounds.getY(),
        backgroundColour, bounds.getCentreX(), bounds.getBottom(), false));
    graphics.fillRoundedRectangle(bounds, 7.0f);
    graphics.setColour(accentColour.withAlpha(0.85f));
    graphics.drawRoundedRectangle(bounds, 7.0f, 1.3f);

    juce::Path arrow;
    const auto arrowX = bounds.getRight() - 17.0f;
    const auto arrowY = bounds.getCentreY() - 2.0f;
    arrow.startNewSubPath(arrowX - 4.0f, arrowY);
    arrow.lineTo(arrowX, arrowY + 4.0f);
    arrow.lineTo(arrowX + 4.0f, arrowY);
    graphics.setColour(accentHighlightColour);
    graphics.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
}

StepBloomAudioProcessorEditor::SettingsButton::SettingsButton()
    : juce::Button("Settings")
{
    setTooltip("Settings");
}

void StepBloomAudioProcessorEditor::SettingsButton::paintButton(
    juce::Graphics& graphics, bool isMouseOver, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(5.0f);
    if (isMouseOver || isButtonDown)
    {
        graphics.setColour(accentColour.withAlpha(isButtonDown ? 0.22f : 0.12f));
        graphics.fillEllipse(bounds);
    }

    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.35f;
    graphics.setColour(accentHighlightColour);
    for (int tooth = 0; tooth < 8; ++tooth)
    {
        const auto angle = juce::MathConstants<float>::twoPi
            * static_cast<float>(tooth) / 8.0f;
        const auto inner = juce::Point<float>{
            centre.x + std::cos(angle) * radius * 0.75f,
            centre.y + std::sin(angle) * radius * 0.75f
        };
        const auto outer = juce::Point<float>{
            centre.x + std::cos(angle) * radius,
            centre.y + std::sin(angle) * radius
        };
        graphics.drawLine(inner.x, inner.y, outer.x, outer.y, 3.0f);
    }
    graphics.drawEllipse(centre.x - radius * 0.72f, centre.y - radius * 0.72f,
                         radius * 1.44f, radius * 1.44f, 2.2f);
    graphics.setColour(backgroundColour);
    graphics.fillEllipse(centre.x - radius * 0.24f, centre.y - radius * 0.24f,
                         radius * 0.48f, radius * 0.48f);
}

StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::ThreeNodeEnvelopeEditor(
    StepBloomAudioProcessor& audioProcessor)
    : processor(audioProcessor),
      parameters(audioProcessor.parameters)
{
    constexpr std::array<const char*, 4> profileNames{"MASTER", "LOW", "MID", "HIGH"};
    for (auto* button : {&copyCurveButton, &pasteCurveButton})
    {
        button->setColour(juce::TextButton::buttonColourId, panelColour);
        button->setColour(juce::TextButton::buttonOnColourId, accentColour.withAlpha(0.55f));
        button->setColour(juce::TextButton::textColourOffId, textColour);
        button->setColour(juce::TextButton::textColourOnId, accentHighlightColour);
        addAndMakeVisible(*button);
    }
    copyCurveButton.onClick = [this] { copyActiveCurve(); };
    pasteCurveButton.onClick = [this] { pasteCurveToActiveProfile(); };
    pasteCurveButton.setEnabled(false);
    for (size_t profile = 0; profile < profileTabs.size(); ++profile)
    {
        auto& tab = profileTabs[profile];
        tab.setButtonText(profileNames[profile]);
        tab.setColour(juce::TextButton::buttonColourId, panelColour);
        tab.setColour(juce::TextButton::buttonOnColourId, accentColour.withAlpha(0.55f));
        tab.setColour(juce::TextButton::textColourOffId, textColour);
        tab.setColour(juce::TextButton::textColourOnId, accentHighlightColour);
        tab.setClickingTogglesState(true);
        tab.setRadioGroupId(29);
        tab.onClick = [this, profile] { setActiveProfile(static_cast<int>(profile)); };
        addAndMakeVisible(tab);
    }
    profileTabs[0].setToggleState(true, juce::dontSendNotification);
    const auto latestSequence = processor.getLatestWaveformSequence();
    nextWaveformSequence = latestSequence
        > static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity)
        ? latestSequence - static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity)
        : 0;
    clearWaveformBins();
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(30);
}

juce::String StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::profileParameterId(
    const juce::String& suffix, int profile) const
{
    constexpr std::array<const char*, 4> profileNames{"Master", "Low", "Mid", "High"};
    const auto index = profile < 0 ? getActiveProfile() : profile;
    return juce::String(profileNames[static_cast<size_t>(juce::jlimit(0, 3, index))])
        + "_" + suffix;
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::setActiveProfile(int profile)
{
    activeProfile = juce::jlimit(0, 3, profile);
    for (size_t index = 0; index < profileTabs.size(); ++index)
        profileTabs[index].setToggleState(static_cast<int>(index) == activeProfile,
                                          juce::dontSendNotification);
    pasteCurveButton.setEnabled(curveClipboard.valid);
    repaint();
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::updateProfileTabs()
{
    const auto* mode = parameters.getRawParameterValue("processingMode");
    const auto multiband = mode != nullptr && mode->load(std::memory_order_relaxed) >= 0.5f;
    if (multiband && activeProfile == 0)
        setActiveProfile(1);
    else if (!multiband && activeProfile != 0)
        setActiveProfile(0);

    profileTabs[0].setVisible(!multiband);
    for (size_t index = 1; index < profileTabs.size(); ++index)
        profileTabs[index].setVisible(multiband);
    resized();
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::resized()
{
    auto area = getLocalBounds().reduced(18, 8);
    constexpr int tabWidth = 64;
    const auto* mode = parameters.getRawParameterValue("processingMode");
    const auto firstVisibleTab = mode != nullptr && mode->load(std::memory_order_relaxed) >= 0.5f
        ? 1 : 0;
    for (size_t index = static_cast<size_t>(firstVisibleTab); index < profileTabs.size(); ++index)
        profileTabs[index].setBounds(area.removeFromLeft(tabWidth).withHeight(25));
    const auto visibleTabCount = firstVisibleTab == 0 ? 1 : 3;
    area.removeFromLeft(visibleTabCount * tabWidth + 4);
    copyCurveButton.setBounds(area.removeFromLeft(54).withHeight(25));
    area.removeFromLeft(3);
    pasteCurveButton.setBounds(area.removeFromLeft(58).withHeight(25));
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::copyActiveCurve()
{
    constexpr std::array<const char*, 6> nodeIds{
        "Node1_X", "Node1_Y", "Node2_X", "Node2_Y", "Node3_X", "Node3_Y"};
    for (size_t node = 0; node < nodeIds.size(); ++node)
    {
        const auto* parameter = parameters.getRawParameterValue(profileParameterId(nodeIds[node]));
        if (parameter == nullptr)
            return;
        curveClipboard.fixedNodes[node] = parameter->load();
    }
    for (size_t extra = 0; extra < curveClipboard.extraNodes.size(); ++extra)
    {
        const auto prefix = "Extra" + juce::String(extra + 1);
        const auto* active = parameters.getRawParameterValue(
            profileParameterId(prefix + "_Active"));
        const auto* phase = parameters.getRawParameterValue(profileParameterId(prefix + "_X"));
        const auto* gain = parameters.getRawParameterValue(profileParameterId(prefix + "_Y"));
        if (active == nullptr || phase == nullptr || gain == nullptr)
            return;
        curveClipboard.extraNodes[extra] = {
            active->load() >= 0.5f, phase->load(), gain->load()};
    }
    curveClipboard.valid = true;
    pasteCurveButton.setEnabled(true);
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::pasteCurveToActiveProfile()
{
    if (!curveClipboard.valid)
        return;

    constexpr std::array<const char*, 6> nodeIds{
        "Node1_X", "Node1_Y", "Node2_X", "Node2_Y", "Node3_X", "Node3_Y"};
    const auto setParameter = [this](const juce::String& id, float value)
    {
        if (auto* parameter = parameters.getParameter(profileParameterId(id)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    for (size_t node = 0; node < nodeIds.size(); ++node)
        setParameter(nodeIds[node], curveClipboard.fixedNodes[node]);

    for (size_t extra = 0; extra < curveClipboard.extraNodes.size(); ++extra)
    {
        const auto prefix = "Extra" + juce::String(extra + 1);
        const auto& node = curveClipboard.extraNodes[extra];
        setParameter(prefix + "_X", node.phase);
        setParameter(prefix + "_Y", node.gain);
        setParameter(prefix + "_Active", node.active ? 1.0f : 0.0f);
    }
    repaint();
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    plotArea = bounds.reduced(18.0f, 16.0f).withTrimmedTop(30.0f);

    graphics.setColour(juce::Colours::black.withAlpha(0.9f));
    graphics.fillRoundedRectangle(bounds.translated(0.0f, 4.0f), 12.0f);
    graphics.setGradientFill(juce::ColourGradient(
        panelHighlightColour, bounds.getCentreX(), bounds.getY(),
        panelColour, bounds.getCentreX(), bounds.getBottom(), false));
    graphics.fillRoundedRectangle(bounds, 12.0f);
    graphics.setColour(accentColour.withAlpha(0.6f));
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

    graphics.saveState();
    graphics.reduceClipRegion(plotArea.toNearestInt());

    graphics.setColour(gridColour.withAlpha(0.55f));
    graphics.drawHorizontalLine(static_cast<int>(plotArea.getCentreY()),
                                plotArea.getX(), plotArea.getRight());

    auto peak = 0.0f;
    for (int i = 0; i < waveformBinCount; ++i)
    {
        if (waveformCounts[static_cast<size_t>(i)] == 0)
            continue;
        peak = juce::jmax(peak,
            juce::jmax(std::abs(sidechainMinimum[static_cast<size_t>(i)]),
                       std::abs(sidechainMaximum[static_cast<size_t>(i)])));
        peak = juce::jmax(peak,
            juce::jmax(std::abs(outputMinimum[static_cast<size_t>(i)]),
                       std::abs(outputMaximum[static_cast<size_t>(i)])));
    }
    const auto amplitudeScale = plotArea.getHeight() * 0.40f
        / juce::jmax(peak, 0.01f);
    drawWaveformLayer(graphics, juce::Colour{0xffe58a45}, sidechainMinimum,
                      sidechainMaximum, waveformCounts, amplitudeScale);
    drawWaveformLayer(graphics, juce::Colour{0xff63b3d6}, outputMinimum,
                      outputMaximum, waveformCounts, amplitudeScale);

    drawAdaptiveGrid(graphics);

    juce::Path curve;
    const auto* widthParameter = parameters.getRawParameterValue(profileParameterId("Node2_X"));
    const auto* attackParameter = parameters.getRawParameterValue(profileParameterId("Node1_X"));
    const auto* floorParameter = parameters.getRawParameterValue(profileParameterId("Node1_Y"));
    const auto* tensionParameter = parameters.getRawParameterValue(profileParameterId("Node2_Y"));
    const auto* endFadeParameter = parameters.getRawParameterValue(profileParameterId("Node3_X"));
    const auto* endGainParameter = parameters.getRawParameterValue(profileParameterId("Node3_Y"));
    const auto duckWidth = widthParameter != nullptr ? widthParameter->load() : 0.6f;
    const auto attackTime = attackParameter != nullptr ? attackParameter->load() : 3.0f;
    const auto minGain = floorParameter != nullptr ? floorParameter->load() : 0.2f;
    const auto releaseTension = tensionParameter != nullptr ? tensionParameter->load() : 1.0f;
    const auto endFadeOffset = endFadeParameter != nullptr ? endFadeParameter->load() : 0.95f;
    const auto endGain = endGainParameter != nullptr ? endGainParameter->load() : 1.0f;
    const auto attackPhase = getAttackPhase(attackTime, duckWidth);
    std::array<StepBloomAudioProcessor::ExtraEnvelopeNode, 2> extraNodes{};
    for (size_t extra = 0; extra < extraNodes.size(); ++extra)
    {
        const auto suffix = "Extra" + juce::String(extra + 1);
        const auto* active = parameters.getRawParameterValue(
            profileParameterId(suffix + "_Active"));
        const auto* phase = parameters.getRawParameterValue(profileParameterId(suffix + "_X"));
        const auto* gain = parameters.getRawParameterValue(profileParameterId(suffix + "_Y"));
        extraNodes[extra].active = active != nullptr && active->load() >= 0.5f;
        extraNodes[extra].phase = phase != nullptr ? phase->load() : 0.5f;
        extraNodes[extra].gain = gain != nullptr ? gain->load() : 0.5f;
    }
    constexpr int curveSamples = 160;
    for (int i = 0; i <= curveSamples; ++i)
    {
        const auto phase = static_cast<float>(i) / static_cast<float>(curveSamples);
        const auto gain = StepBloomAudioProcessor::evaluateProfileEnvelope(
            phase, attackPhase, duckWidth, minGain, releaseTension,
            endFadeOffset, endGain, extraNodes);
        const auto point = juce::Point<float>{
            plotArea.getX() + phase * plotArea.getWidth(),
            plotArea.getBottom() - gain * plotArea.getHeight()
        };
        if (i == 0)
            curve.startNewSubPath(point);
        else
            curve.lineTo(point);
    }

    const auto attackX = plotArea.getX() + attackPhase * plotArea.getWidth();
    const auto duckWidthX = plotArea.getX() + duckWidth * plotArea.getWidth();
    graphics.setColour(juce::Colour{0xffe58a45}.withAlpha(0.11f));
    graphics.fillRect(juce::Rectangle<float>{plotArea.getX(), plotArea.getY(),
                                              juce::jmax(0.0f, attackX - plotArea.getX()),
                                              plotArea.getHeight()});
    graphics.setColour(accentColour.withAlpha(0.08f));
    graphics.fillRect(juce::Rectangle<float>{attackX, plotArea.getY(),
                                              juce::jmax(0.0f, duckWidthX - attackX),
                                              plotArea.getHeight()});
    juce::Path fill;
    fill.startNewSubPath(plotArea.getX(), plotArea.getBottom());
    fill.addPath(curve);
    fill.lineTo(plotArea.getRight(), plotArea.getBottom());
    fill.closeSubPath();
    graphics.setColour(accentColour.withAlpha(0.10f));
    graphics.fillPath(fill);
    graphics.setColour(accentColour);
    graphics.strokePath(curve, juce::PathStrokeType(2.5f));

    const auto startNode = getStartNodePosition(attackTime, minGain, duckWidth);
    const auto recoveryNode = getEndNodePosition(duckWidth, releaseTension);
    const auto tailNode = getTailNodePosition(endFadeOffset, endGain);
    graphics.setColour(accentHighlightColour.withAlpha(0.65f));
    graphics.drawLine(duckWidthX, plotArea.getY(), recoveryNode.x, recoveryNode.y, 1.0f);

    const auto drawNode = [&graphics](juce::Point<float> node, juce::Colour colour)
    {
        graphics.setColour(juce::Colours::black.withAlpha(0.95f));
        graphics.fillEllipse(node.x - 9.0f, node.y - 9.0f, 18.0f, 18.0f);
        graphics.setGradientFill(juce::ColourGradient(
            colour.brighter(0.35f), node.x - 3.0f, node.y - 5.0f,
            colour, node.x + 5.0f, node.y + 6.0f, true));
        graphics.fillEllipse(node.x - 6.0f, node.y - 6.0f, 12.0f, 12.0f);
    };
    drawNode(startNode, juce::Colour{0xffe58a45});
    drawNode(recoveryNode, accentColour);
    drawNode(tailNode, juce::Colour{0xffa8d76a});
    for (const auto& extra : extraNodes)
    {
        if (!extra.active)
            continue;
        const juce::Point<float> node{
            plotArea.getX() + extra.phase * plotArea.getWidth(),
            plotArea.getBottom() - extra.gain * plotArea.getHeight()};
        graphics.setColour(juce::Colours::black.withAlpha(0.95f));
        graphics.fillEllipse(node.x - 10.0f, node.y - 10.0f, 20.0f, 20.0f);
        graphics.setColour(juce::Colour{0xff63d9c3});
        graphics.drawEllipse(node.x - 8.0f, node.y - 8.0f, 16.0f, 16.0f, 2.0f);
        graphics.setColour(accentHighlightColour);
        graphics.fillEllipse(node.x - 4.0f, node.y - 4.0f, 8.0f, 8.0f);
    }

    if (showSnapFeedback && draggedNode != DraggedNode::None)
    {
        graphics.setColour(accentHighlightColour);
        graphics.setFont(StepBloomLookAndFeel::getCustomFont(10.0f, juce::Font::bold));
        juce::Rectangle<float> tooltip{snappedGridX + 10.0f, snappedGridY - 28.0f,
                                       210.0f, 22.0f};
        tooltip = tooltip.withPosition(
            juce::jlimit(plotArea.getX(), plotArea.getRight() - tooltip.getWidth(), tooltip.getX()),
            juce::jlimit(plotArea.getY(), plotArea.getBottom() - tooltip.getHeight(), tooltip.getY()));
        graphics.setColour(backgroundColour.withAlpha(0.92f));
        graphics.fillRoundedRectangle(tooltip, 4.0f);
        graphics.setColour(accentHighlightColour);
        graphics.drawRoundedRectangle(tooltip, 4.0f, 1.0f);
        graphics.drawText(dragTooltip, tooltip.toNearestInt().reduced(5, 1),
                          juce::Justification::centredLeft, false);
    }

    graphics.restoreState();
    graphics.setColour(accentHighlightColour.withAlpha(0.9f));
    graphics.setFont(StepBloomLookAndFeel::getCustomFont(10.0f, juce::Font::bold));
    graphics.drawText("START: ATTACK / GAIN     KNEE: WIDTH / TENSION     END: TAIL / GAIN",
                      plotArea.toNearestInt().reduced(2, 2),
                      juce::Justification::topRight, false);

    graphics.setColour(juce::Colour{0xffe58a45});
    graphics.fillEllipse(plotArea.getX() + 5.0f, plotArea.getY() + 4.0f, 6.0f, 6.0f);
    graphics.setColour(textColour);
    graphics.setFont(StepBloomLookAndFeel::getCustomFont(9.0f, juce::Font::bold));
    graphics.drawText("SIDECHAIN", static_cast<int>(plotArea.getX() + 14.0f),
                      static_cast<int>(plotArea.getY() + 1.0f), 68, 12,
                      juce::Justification::centredLeft, false);
    graphics.setColour(juce::Colour{0xff63b3d6});
    graphics.fillEllipse(plotArea.getX() + 84.0f, plotArea.getY() + 4.0f, 6.0f, 6.0f);
    graphics.setColour(textColour);
    graphics.drawText("MAIN OUTPUT", static_cast<int>(plotArea.getX() + 93.0f),
                      static_cast<int>(plotArea.getY() + 1.0f), 72, 12,
                      juce::Justification::centredLeft, false);
    graphics.setColour(accentColour);
    graphics.fillEllipse(plotArea.getX() + 172.0f, plotArea.getY() + 4.0f, 6.0f, 6.0f);
    graphics.setColour(textColour);
    graphics.drawText("ENVELOPE", static_cast<int>(plotArea.getX() + 181.0f),
                      static_cast<int>(plotArea.getY() + 1.0f), 68, 12,
                      juce::Justification::centredLeft, false);
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::drawWaveformLayer(
    juce::Graphics& graphics, juce::Colour colour,
    const std::array<float, waveformBinCount>& minimum,
    const std::array<float, waveformBinCount>& maximum,
    const std::array<uint32_t, waveformBinCount>& counts,
    float amplitudeScale) const
{
    const auto width = juce::jmax(1, static_cast<int>(plotArea.getWidth()));
    const auto centerY = plotArea.getCentreY();
    const auto denominator = static_cast<float>(juce::jmax(1, width - 1));
    juce::Path waveform;
    juce::Path fill;

    for (int x = 0; x < width; ++x)
    {
        const auto firstBin = x * waveformBinCount / width;
        const auto lastBin = juce::jmax(firstBin + 1, (x + 1) * waveformBinCount / width);
        auto binMinimum = 0.0f;
        auto binMaximum = 0.0f;
        auto hasBin = false;
        for (int bin = firstBin; bin < juce::jmin(lastBin, waveformBinCount); ++bin)
        {
            const auto index = static_cast<size_t>(bin);
            if (counts[index] == 0)
                continue;
            if (!hasBin)
            {
                binMinimum = minimum[index];
                binMaximum = maximum[index];
                hasBin = true;
            }
            else
            {
                binMinimum = juce::jmin(binMinimum, minimum[index]);
                binMaximum = juce::jmax(binMaximum, maximum[index]);
            }
        }

        const auto xPosition = plotArea.getX()
            + (static_cast<float>(x) / denominator) * plotArea.getWidth();
        const auto upperY = centerY - binMaximum * amplitudeScale;
        const auto lowerY = centerY - binMinimum * amplitudeScale;
        if (x == 0)
        {
            waveform.startNewSubPath(xPosition, upperY);
            fill.startNewSubPath(xPosition, upperY);
        }
        else
        {
            waveform.lineTo(xPosition, upperY);
            fill.lineTo(xPosition, upperY);
        }
        graphics.setColour(colour.withAlpha(0.6f));
        graphics.drawVerticalLine(static_cast<int>(xPosition), upperY, lowerY);
    }

    for (int x = width - 1; x >= 0; --x)
    {
        const auto firstBin = x * waveformBinCount / width;
        const auto lastBin = juce::jmax(firstBin + 1, (x + 1) * waveformBinCount / width);
        auto binMinimum = 0.0f;
        auto hasBin = false;
        for (int bin = firstBin; bin < juce::jmin(lastBin, waveformBinCount); ++bin)
        {
            const auto index = static_cast<size_t>(bin);
            if (counts[index] == 0)
                continue;
            binMinimum = hasBin ? juce::jmin(binMinimum, minimum[index]) : minimum[index];
            hasBin = true;
        }
        const auto xPosition = plotArea.getX()
            + (static_cast<float>(x) / denominator) * plotArea.getWidth();
        fill.lineTo(xPosition, centerY - binMinimum * amplitudeScale);
    }
    fill.closeSubPath();
    graphics.setColour(colour.withAlpha(0.12f));
    graphics.fillPath(fill);
    graphics.setColour(colour.withAlpha(0.75f));
    graphics.strokePath(waveform, juce::PathStrokeType(1.2f));
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::timerCallback()
{
    updateProfileTabs();
    const auto latestSequence = processor.getLatestWaveformSequence();
    if (latestSequence < nextWaveformSequence)
    {
        nextWaveformSequence = latestSequence;
        clearWaveformBins();
        hasDisplayCycle = false;
    }

    if (latestSequence - nextWaveformSequence
        > static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity))
    {
        nextWaveformSequence = latestSequence
            - static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity);
        clearWaveformBins();
        hasDisplayCycle = false;
    }

    auto changed = false;
    StepBloomAudioProcessor::WaveformFrame frame;
    while (nextWaveformSequence < latestSequence)
    {
        if (!processor.readWaveformFrame(nextWaveformSequence, frame))
        {
            nextWaveformSequence = latestSequence;
            clearWaveformBins();
            hasDisplayCycle = false;
            break;
        }

        addWaveformFrame(frame);
        ++nextWaveformSequence;
        changed = true;
    }

    if (changed)
        repaint();
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::clearWaveformBins()
{
    sidechainMinimum.fill(std::numeric_limits<float>::max());
    sidechainMaximum.fill(std::numeric_limits<float>::lowest());
    outputMinimum.fill(std::numeric_limits<float>::max());
    outputMaximum.fill(std::numeric_limits<float>::lowest());
    waveformCounts.fill(0);
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::addWaveformFrame(
    const StepBloomAudioProcessor::WaveformFrame& frame)
{
    if (!hasDisplayCycle || frame.cycleId != displayCycleId)
    {
        clearWaveformBins();
        displayCycleId = frame.cycleId;
        hasDisplayCycle = true;
    }

    const auto phase = juce::jlimit(0.0f, 1.0f, frame.phase);
    const auto bin = juce::jlimit(0, waveformBinCount - 1,
        static_cast<int>(phase * static_cast<float>(waveformBinCount)));
    const auto index = static_cast<size_t>(bin);
    sidechainMinimum[index] = juce::jmin(sidechainMinimum[index], frame.sidechain);
    sidechainMaximum[index] = juce::jmax(sidechainMaximum[index], frame.sidechain);
    outputMinimum[index] = juce::jmin(outputMinimum[index], frame.processedOutput);
    outputMaximum[index] = juce::jmax(outputMaximum[index], frame.processedOutput);
    ++waveformCounts[index];
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::mouseDown(
    const juce::MouseEvent& event)
{
    if (event.getNumberOfClicks() > 1)
        return;

    draggedProfile = activeProfile;
    const auto* attackParameter = parameters.getRawParameterValue(profileParameterId("Node1_X"));
    const auto* floorParameter = parameters.getRawParameterValue(profileParameterId("Node1_Y"));
    const auto* widthParameter = parameters.getRawParameterValue(profileParameterId("Node2_X"));
    const auto* tensionParameter = parameters.getRawParameterValue(profileParameterId("Node2_Y"));
    const auto* fadeParameter = parameters.getRawParameterValue(profileParameterId("Node3_X"));
    const auto* endGainParameter = parameters.getRawParameterValue(profileParameterId("Node3_Y"));
    const auto attackTime = attackParameter != nullptr ? attackParameter->load() : 3.0f;
    const auto minGain = floorParameter != nullptr ? floorParameter->load() : 0.2f;
    const auto duckWidth = widthParameter != nullptr ? widthParameter->load() : 0.6f;
    const auto releaseTension = tensionParameter != nullptr ? tensionParameter->load() : 1.0f;
    const auto endFadeOffset = fadeParameter != nullptr ? fadeParameter->load() : 0.95f;
    const auto endGain = endGainParameter != nullptr ? endGainParameter->load() : 1.0f;
    const auto startNode = getStartNodePosition(attackTime, minGain, duckWidth);
    const auto recoveryNode = getEndNodePosition(duckWidth, releaseTension);
    const auto tailNode = getTailNodePosition(endFadeOffset, endGain);

    auto hitExtra = -1;
    for (int extra = 0; extra < 2; ++extra)
    {
        const auto suffix = "Extra" + juce::String(extra + 1);
        const auto* active = parameters.getRawParameterValue(
            profileParameterId(suffix + "_Active"));
        const auto* phase = parameters.getRawParameterValue(profileParameterId(suffix + "_X"));
        const auto* gain = parameters.getRawParameterValue(profileParameterId(suffix + "_Y"));
        if (active == nullptr || active->load() < 0.5f)
            continue;
        const juce::Point<float> position{
            plotArea.getX() + (phase != nullptr ? phase->load() : 0.5f) * plotArea.getWidth(),
            plotArea.getBottom() - (gain != nullptr ? gain->load() : 0.5f) * plotArea.getHeight()};
        if (event.position.getDistanceFrom(position) <= 16.0f)
        {
            hitExtra = extra;
            break;
        }
    }

    if (hitExtra >= 0)
    {
        draggedExtraIndex = hitExtra;
        draggedNode = hitExtra == 0 ? DraggedNode::Extra1 : DraggedNode::Extra2;
        const auto suffix = "Extra" + juce::String(hitExtra + 1);
        for (const auto& parameterSuffix : {"_X", "_Y"})
            if (auto* parameter = parameters.getParameter(
                    profileParameterId(suffix + parameterSuffix, draggedProfile)))
                parameter->beginChangeGesture();
    }
    else if (event.position.getDistanceFrom(startNode) <= 16.0f)
    {
        draggedNode = DraggedNode::Start;
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node1_X", draggedProfile)))
            parameter->beginChangeGesture();
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node1_Y", draggedProfile)))
            parameter->beginChangeGesture();
    }
    else if (event.position.getDistanceFrom(recoveryNode) <= 16.0f)
    {
        draggedNode = DraggedNode::Recovery;
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node2_X", draggedProfile)))
            parameter->beginChangeGesture();
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node2_Y", draggedProfile)))
            parameter->beginChangeGesture();
    }
    else if (event.position.getDistanceFrom(tailNode) <= 16.0f)
    {
        draggedNode = DraggedNode::End;
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node3_X", draggedProfile)))
            parameter->beginChangeGesture();
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node3_Y", draggedProfile)))
            parameter->beginChangeGesture();
    }

    if (draggedNode != DraggedNode::None)
    {
        showSnapFeedback = false;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::mouseDoubleClick(
    const juce::MouseEvent& event)
{
    if (!plotArea.contains(event.position) || plotArea.getWidth() <= 0.0f
        || plotArea.getHeight() <= 0.0f)
        return;

    auto deleteIndex = -1;
    for (int extra = 0; extra < 2; ++extra)
    {
        const auto suffix = "Extra" + juce::String(extra + 1);
        const auto* active = parameters.getRawParameterValue(
            profileParameterId(suffix + "_Active"));
        const auto* phase = parameters.getRawParameterValue(profileParameterId(suffix + "_X"));
        const auto* gain = parameters.getRawParameterValue(profileParameterId(suffix + "_Y"));
        if (active == nullptr || active->load() < 0.5f)
            continue;
        const juce::Point<float> node{
            plotArea.getX() + (phase != nullptr ? phase->load() : 0.5f) * plotArea.getWidth(),
            plotArea.getBottom() - (gain != nullptr ? gain->load() : 0.5f) * plotArea.getHeight()};
        if (event.position.getDistanceFrom(node) <= 16.0f)
        {
            deleteIndex = extra;
            break;
        }
    }

    const auto extraName = "Extra" + juce::String(deleteIndex >= 0 ? deleteIndex + 1 : 1);
    const auto activeId = profileParameterId(extraName + "_Active");
    if (deleteIndex >= 0)
    {
        if (auto* parameter = parameters.getParameter(activeId))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(0.0f));
            parameter->endChangeGesture();
        }
        repaint();
        return;
    }

    const auto* attackParameter = parameters.getRawParameterValue(profileParameterId("Node1_X"));
    const auto* floorParameter = parameters.getRawParameterValue(profileParameterId("Node1_Y"));
    const auto* widthParameter = parameters.getRawParameterValue(profileParameterId("Node2_X"));
    const auto* tensionParameter = parameters.getRawParameterValue(profileParameterId("Node2_Y"));
    const auto* fadeParameter = parameters.getRawParameterValue(profileParameterId("Node3_X"));
    const auto* endGainParameter = parameters.getRawParameterValue(profileParameterId("Node3_Y"));
    const auto attackTime = attackParameter != nullptr ? attackParameter->load() : 3.0f;
    const auto minGain = floorParameter != nullptr ? floorParameter->load() : 0.2f;
    const auto duckWidth = widthParameter != nullptr ? widthParameter->load() : 0.6f;
    const auto tension = tensionParameter != nullptr ? tensionParameter->load() : 1.0f;
    const auto tailOffset = fadeParameter != nullptr ? fadeParameter->load() : 0.95f;
    const auto endGain = endGainParameter != nullptr ? endGainParameter->load() : 1.0f;
    if (event.position.getDistanceFrom(getStartNodePosition(attackTime, minGain, duckWidth))
            <= 16.0f
        || event.position.getDistanceFrom(getEndNodePosition(duckWidth, tension)) <= 16.0f
        || event.position.getDistanceFrom(getTailNodePosition(tailOffset, endGain)) <= 16.0f)
        return;

    auto availableIndex = -1;
    for (int extra = 0; extra < 2; ++extra)
    {
        const auto* active = parameters.getRawParameterValue(
            profileParameterId("Extra" + juce::String(extra + 1) + "_Active"));
        if (active == nullptr || active->load() < 0.5f)
        {
            availableIndex = extra;
            break;
        }
    }
    if (availableIndex < 0)
        return;

    const auto phase = juce::jlimit(0.05f, 0.95f,
        (event.position.x - plotArea.getX()) / plotArea.getWidth());
    const auto gain = juce::jlimit(0.0f, 1.0f,
        (plotArea.getBottom() - event.position.y) / plotArea.getHeight());
    const auto nodePrefix = "Extra" + juce::String(availableIndex + 1);
    for (const auto& update : {std::pair<juce::String, float>{nodePrefix + "_X", phase},
                               {nodePrefix + "_Y", gain},
                               {nodePrefix + "_Active", 1.0f}})
    {
        if (auto* parameter = parameters.getParameter(profileParameterId(update.first)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(update.second));
            parameter->endChangeGesture();
        }
    }
    repaint();
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::mouseDrag(
    const juce::MouseEvent& event)
{
    if (draggedNode != DraggedNode::None)
        updateNode(event.position, event.mods.isCtrlDown() || event.mods.isCommandDown());
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::mouseUp(const juce::MouseEvent&)
{
    endGesture();
}

float StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::getAttackPhase(
    float attackTime, float duckWidth) const
{
    const auto cycleMilliseconds = juce::jmax(
        processor.getCycleDurationMilliseconds(), 1.0e-3f);
    return juce::jmin(attackTime / cycleMilliseconds, duckWidth * 0.5f);
}

juce::Point<float> StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::getStartNodePosition(
    float attackTime, float minGain, float duckWidth) const
{
    const auto attackPhase = getAttackPhase(attackTime, duckWidth);
    return {
        plotArea.getX() + attackPhase * plotArea.getWidth(),
        plotArea.getBottom() - minGain * plotArea.getHeight()
    };
}

juce::Point<float> StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::getEndNodePosition(
    float duckWidth, float releaseTension) const
{
    const auto tensionPosition = 1.0f - (releaseTension - 0.2f) / 1.3f;
    return {
        plotArea.getX() + duckWidth * plotArea.getWidth(),
        plotArea.getY() + tensionPosition * plotArea.getHeight()
    };
}

juce::Point<float> StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::getTailNodePosition(
    float endFadeOffset, float endGain) const
{
    return {
        plotArea.getX() + endFadeOffset * plotArea.getWidth(),
        plotArea.getBottom() - endGain * plotArea.getHeight()
    };
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::updateNode(
    juce::Point<float> position, bool shouldSnap)
{
    if (plotArea.getWidth() <= 0.0f || plotArea.getHeight() <= 0.0f)
        return;

    showSnapFeedback = shouldSnap;
    snappedXActive = shouldSnap;
    snappedYActive = shouldSnap;
    const auto divisions = gridDivisionCount();
    auto phase = juce::jlimit(0.0f, 1.0f,
        (position.x - plotArea.getX()) / plotArea.getWidth());
    auto gainPosition = juce::jlimit(0.0f, 1.0f,
        (plotArea.getBottom() - position.y) / plotArea.getHeight());
    if (shouldSnap)
    {
        constexpr std::array<float, 5> gainStops{0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        gainPosition = *std::min_element(gainStops.begin(), gainStops.end(),
            [gainPosition](float a, float b)
            {
                return std::abs(a - gainPosition) < std::abs(b - gainPosition);
            });
        snappedGridY = plotArea.getBottom() - gainPosition * plotArea.getHeight();
    }

    const auto snapPhaseWithinRange = [divisions, shouldSnap](float value,
                                                               float minimum,
                                                               float maximum)
    {
        value = juce::jlimit(minimum, maximum, value);
        if (! shouldSnap)
            return value;

        const auto minIndex = static_cast<int>(std::ceil(
            minimum * static_cast<float>(divisions)));
        const auto maxIndex = static_cast<int>(std::floor(
            maximum * static_cast<float>(divisions)));
        if (minIndex > maxIndex)
            return minimum;

        const auto index = juce::jlimit(minIndex, maxIndex,
            juce::roundToInt(value * static_cast<float>(divisions)));
        return static_cast<float>(index) / static_cast<float>(divisions);
    };
    auto setParameter = [this](const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter(profileParameterId(id, draggedProfile)))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    auto snappedGainDb = gainPosition > 0.0f
        ? juce::String(20.0f * std::log10(gainPosition), 1) + " dB"
        : juce::String("-inf dB");

    if (draggedNode == DraggedNode::Extra1 || draggedNode == DraggedNode::Extra2)
    {
        const auto nodePrefix = "Extra" + juce::String(draggedExtraIndex + 1);
        phase = snapPhaseWithinRange(phase, 0.05f, 0.95f);
        snappedGridX = plotArea.getX() + phase * plotArea.getWidth();
        const auto activeProfileId = profileParameterId(nodePrefix + "_X", draggedProfile);
        const auto gainProfileId = profileParameterId(nodePrefix + "_Y", draggedProfile);
        if (auto* parameter = parameters.getParameter(activeProfileId))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(phase));
        if (auto* parameter = parameters.getParameter(gainProfileId))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(gainPosition));
        dragTooltip = "Phase: " + juce::String(phase, 2) + " (grid 1/"
            + juce::String(divisions) + ") | Gain: " + snappedGainDb;
    }
    else if (draggedNode == DraggedNode::Start)
    {
        const auto* widthParameter = parameters.getRawParameterValue(
            profileParameterId("Node2_X", draggedProfile));
        const auto duckWidth = widthParameter != nullptr ? widthParameter->load() : 0.6f;
        const auto maxAttackPhase = juce::jmin(duckWidth * 0.5f,
            15.0f / juce::jmax(processor.getCycleDurationMilliseconds(), 1.0e-3f));
        const auto attackPhase = snapPhaseWithinRange(phase, 0.0f, maxAttackPhase);
        const auto attackTime = attackPhase * processor.getCycleDurationMilliseconds();
        snappedGridX = plotArea.getX() + attackPhase * plotArea.getWidth();
        setParameter("Node1_X", attackTime);
        setParameter("Node1_Y", gainPosition);
        dragTooltip = "Phase: " + juce::String(attackPhase, 2) + " (grid 1/"
            + juce::String(divisions) + ") | Gain: " + snappedGainDb;
    }
    else if (draggedNode == DraggedNode::Recovery)
    {
        const auto duckWidth = snapPhaseWithinRange(phase, 0.1f, 0.95f);
        const auto curveTension = juce::jlimit(0.2f, 1.5f,
            0.2f + 1.3f * gainPosition);
        snappedGridX = plotArea.getX() + duckWidth * plotArea.getWidth();
        setParameter("Node2_X", duckWidth);
        setParameter("Node2_Y", curveTension);
        dragTooltip = "Phase: " + juce::String(duckWidth, 2) + " (grid 1/"
            + juce::String(divisions) + ") | Tension: "
            + juce::String(curveTension, 2);
    }
    else if (draggedNode == DraggedNode::End)
    {
        const auto endFadeOffset = snapPhaseWithinRange(phase, 0.8f, 1.0f);
        snappedGridX = plotArea.getX() + endFadeOffset * plotArea.getWidth();
        setParameter("Node3_X", endFadeOffset);
        setParameter("Node3_Y", gainPosition);
        dragTooltip = "Phase: " + juce::String(endFadeOffset, 2) + " (grid 1/"
            + juce::String(divisions) + ") | Gain: " + snappedGainDb;
    }
    repaint();
}

int StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::gridDivisionCount() const
{
    constexpr std::array<int, 6> candidateDivisions{32, 16, 8, 4, 2, 1};
    const auto width = juce::jmax(1.0f, plotArea.getWidth());
    const auto cycleMilliseconds = processor.getCycleDurationMilliseconds();
    for (const auto divisions : candidateDivisions)
        if (width / static_cast<float>(divisions) >= 18.0f
            && cycleMilliseconds / static_cast<float>(divisions) >= 10.0f)
            return divisions;
    return 1;
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::drawAdaptiveGrid(
    juce::Graphics& graphics) const
{
    const auto divisions = gridDivisionCount();
    const auto primaryEvery = juce::jmax(1, divisions / 4);
    for (int i = 0; i <= divisions; ++i)
    {
        const auto x = plotArea.getX() + plotArea.getWidth()
            * static_cast<float>(i) / static_cast<float>(divisions);
        const auto highlighted = showSnapFeedback && snappedXActive
            && std::abs(x - snappedGridX) < 1.0f;
        graphics.setColour(highlighted ? accentHighlightColour.withAlpha(0.95f)
            : gridColour.withAlpha(i % primaryEvery == 0 ? 0.72f : 0.30f));
        graphics.drawVerticalLine(juce::roundToInt(x), plotArea.getY(), plotArea.getBottom());
    }

    constexpr std::array<float, 6> gainStops{0.0f, 0.25f, 0.5f, 0.7071f, 0.75f, 1.0f};
    for (const auto gain : gainStops)
    {
        const auto y = plotArea.getBottom() - gain * plotArea.getHeight();
        const auto highlighted = showSnapFeedback && snappedYActive
            && std::abs(y - snappedGridY) < 1.0f;
        graphics.setColour(highlighted ? accentHighlightColour.withAlpha(0.95f)
                                      : gridColour.withAlpha(gain == 0.5f || gain == 1.0f
                                            ? 0.72f : 0.42f));
        graphics.drawHorizontalLine(juce::roundToInt(y), plotArea.getX(), plotArea.getRight());

        const auto label = gain == 1.0f ? "0 dB"
            : gain == 0.7071f ? "-3 dB"
            : gain == 0.5f ? "-6 dB"
            : gain == 0.25f ? "-12 dB"
            : gain == 0.0f ? "-inf"
            : "-2.5 dB";
        graphics.setColour(textColour.withAlpha(0.68f));
        graphics.setFont(StepBloomLookAndFeel::getCustomFont(8.0f));
        graphics.drawText(label, juce::Rectangle<int>{
            juce::roundToInt(plotArea.getX() + 3.0f),
            juce::roundToInt(y - 9.0f), 38, 12},
            juce::Justification::centredLeft, false);
    }
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::endGesture()
{
    if (draggedNode == DraggedNode::Start)
    {
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node1_X", draggedProfile)))
            parameter->endChangeGesture();
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node1_Y", draggedProfile)))
            parameter->endChangeGesture();
    }
    else if (draggedNode == DraggedNode::Recovery)
    {
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node2_X", draggedProfile)))
            parameter->endChangeGesture();
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node2_Y", draggedProfile)))
            parameter->endChangeGesture();
    }
    else if (draggedNode == DraggedNode::End)
    {
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node3_X", draggedProfile)))
            parameter->endChangeGesture();
        if (auto* parameter = parameters.getParameter(
                profileParameterId("Node3_Y", draggedProfile)))
            parameter->endChangeGesture();
    }
    else if (draggedNode == DraggedNode::Extra1 || draggedNode == DraggedNode::Extra2)
    {
        const auto prefix = "Extra" + juce::String(draggedExtraIndex + 1);
        for (const auto& suffix : {"_X", "_Y"})
            if (auto* parameter = parameters.getParameter(
                    profileParameterId(prefix + suffix, draggedProfile)))
                parameter->endChangeGesture();
        draggedExtraIndex = -1;
    }
    draggedNode = DraggedNode::None;
    showSnapFeedback = false;
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    repaint();
}

StepBloomAudioProcessorEditor::ShapeProfileGridComponent::ShapeProfileGridComponent(
    StepBloomAudioProcessor& audioProcessor, ThreeNodeEnvelopeEditor& editor)
    : processor(audioProcessor),
      envelopeEditor(editor)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

juce::String StepBloomAudioProcessorEditor::ShapeProfileGridComponent::parameterId(
    const juce::String& suffix) const
{
    constexpr std::array<const char*, 4> profileNames{"Master", "Low", "Mid", "High"};
    const auto* mode = processor.parameters.getRawParameterValue("processingMode");
    auto profile = mode != nullptr && mode->load(std::memory_order_relaxed) >= 0.5f
        ? envelopeEditor.getActiveProfile() : 0;
    profile = juce::jlimit(0, 3, profile);
    return juce::String(profileNames[static_cast<size_t>(profile)]) + "_" + suffix;
}

void StepBloomAudioProcessorEditor::ShapeProfileGridComponent::resized()
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    constexpr int columns = 7;
    constexpr int rows = 2;
    constexpr float gap = 4.0f;
    const auto cellWidth = (bounds.getWidth() - gap * (columns - 1)) / columns;
    const auto cellHeight = (bounds.getHeight() - gap * (rows - 1)) / rows;
    for (size_t index = 0; index < buttonBounds.size(); ++index)
    {
        const auto row = static_cast<int>(index) / columns;
        const auto column = static_cast<int>(index) % columns;
        buttonBounds[index] = {
            bounds.getX() + static_cast<float>(column) * (cellWidth + gap),
            bounds.getY() + static_cast<float>(row) * (cellHeight + gap),
            cellWidth, cellHeight
        };
    }
}

bool StepBloomAudioProcessorEditor::ShapeProfileGridComponent::profileMatches(int index) const
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(shapeProfiles.size())))
        return false;
    const auto& preset = shapeProfiles[static_cast<size_t>(index)];
    constexpr std::array<const char*, 6> suffixes{
        "Node1_X", "Node1_Y", "Node2_X", "Node2_Y", "Node3_X", "Node3_Y"};
    const std::array<float, 6> values{
        preset.attackMilliseconds, preset.floor, preset.width,
        preset.tension, preset.tailOffset, preset.endGain};
    constexpr std::array<float, 6> tolerances{0.011f, 0.0011f, 0.0011f,
                                              0.0011f, 0.0011f, 0.0011f};
    for (size_t node = 0; node < suffixes.size(); ++node)
    {
        const auto* parameter = processor.parameters.getRawParameterValue(
            parameterId(suffixes[node]));
        if (parameter == nullptr || std::abs(parameter->load() - values[node]) > tolerances[node])
            return false;
    }
    for (int extra = 1; extra <= 2; ++extra)
    {
        const auto* active = processor.parameters.getRawParameterValue(
            parameterId("Extra" + juce::String(extra) + "_Active"));
        if (active != nullptr && active->load() >= 0.5f)
            return false;
    }
    return true;
}

void StepBloomAudioProcessorEditor::ShapeProfileGridComponent::paint(
    juce::Graphics& graphics)
{
    for (size_t index = 0; index < buttonBounds.size(); ++index)
    {
        const auto bounds = buttonBounds[index];
        const auto selected = profileMatches(static_cast<int>(index));
        graphics.setColour(selected ? accentColour.withAlpha(0.42f)
                                    : panelColour.withAlpha(0.95f));
        graphics.fillRoundedRectangle(bounds, 3.0f);
        graphics.setColour(selected ? accentHighlightColour : accentColour.withAlpha(0.65f));
        graphics.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, selected ? 1.5f : 0.8f);

        const auto& preset = shapeProfiles[index];
        const auto cycleMilliseconds = juce::jmax(
            1.0f, processor.getCycleDurationMilliseconds());
        const auto attackPhase = preset.attackMilliseconds / cycleMilliseconds;
        juce::Path path;
        constexpr int samples = 24;
        for (int sample = 0; sample <= samples; ++sample)
        {
            const auto phase = static_cast<float>(sample) / static_cast<float>(samples);
            const auto gain = StepBloomAudioProcessor::evaluateEnvelope(
                phase, attackPhase, preset.width, preset.floor, preset.tension,
                preset.tailOffset, preset.endGain);
            const auto point = juce::Point<float>{
                bounds.getX() + 3.0f + phase * (bounds.getWidth() - 6.0f),
                bounds.getY() + 3.0f + (1.0f - gain) * (bounds.getHeight() - 10.0f)};
            if (sample == 0)
                path.startNewSubPath(point);
            else
                path.lineTo(point);
        }
        graphics.setColour(selected ? accentHighlightColour : textColour);
        graphics.strokePath(path, juce::PathStrokeType(1.2f));
        graphics.setColour(textColour.withAlpha(0.72f));
        graphics.setFont(StepBloomLookAndFeel::getCustomFont(7.0f, juce::Font::bold));
        graphics.drawText(juce::String(static_cast<int>(index) + 1),
                          bounds.withHeight(11.0f).removeFromBottom(11.0f).toNearestInt(),
                          juce::Justification::centred, false);
    }
}

void StepBloomAudioProcessorEditor::ShapeProfileGridComponent::mouseDown(
    const juce::MouseEvent& event)
{
    for (size_t index = 0; index < buttonBounds.size(); ++index)
        if (buttonBounds[index].contains(event.position))
        {
            applyProfile(static_cast<int>(index));
            return;
        }
}

juce::String StepBloomAudioProcessorEditor::ShapeProfileGridComponent::getTooltip()
{
        const auto mousePosition = getMouseXYRelative().toFloat();
        for (size_t index = 0; index < buttonBounds.size(); ++index)
            if (buttonBounds[index].contains(mousePosition))
                return shapeProfiles[index].name;
        return {};
}

void StepBloomAudioProcessorEditor::ShapeProfileGridComponent::applyProfile(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(shapeProfiles.size())))
        return;
    const auto& preset = shapeProfiles[static_cast<size_t>(index)];
    constexpr std::array<const char*, 6> suffixes{
        "Node1_X", "Node1_Y", "Node2_X", "Node2_Y", "Node3_X", "Node3_Y"};
    const std::array<float, 6> values{
        preset.attackMilliseconds, preset.floor, preset.width,
        preset.tension, preset.tailOffset, preset.endGain};
    const auto setParameter = [this](const juce::String& id, float value)
    {
        if (auto* parameter = processor.parameters.getParameter(parameterId(id)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    for (size_t node = 0; node < suffixes.size(); ++node)
        setParameter(suffixes[node], values[node]);
    setParameter("Extra1_Active", 0.0f);
    setParameter("Extra2_Active", 0.0f);
    repaint();
}

StepBloomAudioProcessorEditor::CrossoverPanelComponent::CrossoverPanelComponent(
    StepBloomAudioProcessor& audioProcessor)
    : processor(audioProcessor)
{
    constexpr std::array<const char*, 3> names{"LOW AMOUNT", "MID AMOUNT", "HIGH AMOUNT"};
    constexpr std::array<const char*, 3> ids{"low", "mid", "high"};
    for (size_t band = 0; band < names.size(); ++band)
    {
        bandLabels[band].setText(names[band], juce::dontSendNotification);
        bandLabels[band].setColour(juce::Label::textColourId, accentHighlightColour);
        bandLabels[band].setFont(StepBloomLookAndFeel::getCustomFont(
            10.0f, juce::Font::bold));
        addAndMakeVisible(bandLabels[band]);

        auto& slider = duckAmountSliders[band];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
        slider.setNumDecimalPlacesToDisplay(0);
        slider.textFromValueFunction = [](double value)
        {
            return juce::String(juce::roundToInt(value * 100.0)) + "%";
        };
        slider.valueFromTextFunction = [](const juce::String& text)
        {
            return text.getFloatValue() * 0.01;
        };
        slider.setColour(juce::Slider::trackColourId, accentColour);
        slider.setColour(juce::Slider::thumbColourId, accentHighlightColour);
        slider.setColour(juce::Slider::textBoxTextColourId, accentHighlightColour);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, backgroundColour);
        slider.setColour(juce::Slider::textBoxOutlineColourId, accentColour.withAlpha(0.7f));
        addAndMakeVisible(slider);
        duckAmountAttachments[band] =
            std::make_unique<StepBloomAudioProcessor::Parameters::SliderAttachment>(
                processor.parameters,
                "duckAmount" + juce::String(ids[band]).substring(0, 1).toUpperCase()
                    + juce::String(ids[band]).substring(1), slider);

        auto& bypass = bypassButtons[band];
        bypass.setButtonText("BYP");
        bypass.setClickingTogglesState(true);
        bypass.setColour(juce::TextButton::buttonColourId, panelColour);
        bypass.setColour(juce::TextButton::buttonOnColourId, accentColour.withAlpha(0.45f));
        bypass.setColour(juce::TextButton::textColourOffId, textColour);
        bypass.setColour(juce::TextButton::textColourOnId, accentHighlightColour);
        addAndMakeVisible(bypass);
        bypassAttachments[band] =
            std::make_unique<StepBloomAudioProcessor::Parameters::ButtonAttachment>(
                processor.parameters, juce::String(ids[band]) + "Bypass", bypass);

        auto& solo = soloButtons[band];
        solo.setButtonText("SOLO");
        solo.setClickingTogglesState(true);
        solo.setColour(juce::TextButton::buttonColourId, panelColour);
        solo.setColour(juce::TextButton::buttonOnColourId, accentColour.withAlpha(0.45f));
        solo.setColour(juce::TextButton::textColourOffId, textColour);
        solo.setColour(juce::TextButton::textColourOnId, accentHighlightColour);
        addAndMakeVisible(solo);
        soloAttachments[band] =
            std::make_unique<StepBloomAudioProcessor::Parameters::ButtonAttachment>(
                processor.parameters, juce::String(ids[band]) + "Solo", solo);
    }

    lastSpectrumSequence = processor.getLatestSpectrumSequence();
    startTimerHz(30);
}

StepBloomAudioProcessorEditor::CrossoverPanelComponent::~CrossoverPanelComponent() = default;

void StepBloomAudioProcessorEditor::CrossoverPanelComponent::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colours::black.withAlpha(0.9f));
    graphics.fillRoundedRectangle(bounds.translated(0.0f, 3.0f), 10.0f);
    graphics.setGradientFill(juce::ColourGradient(
        panelHighlightColour, bounds.getCentreX(), bounds.getY(),
        panelColour, bounds.getCentreX(), bounds.getBottom(), false));
    graphics.fillRoundedRectangle(bounds, 10.0f);
    graphics.setColour(accentColour.withAlpha(0.55f));
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

    spectrumArea = bounds.reduced(14.0f, 12.0f);
    spectrumArea.removeFromBottom(52.0f);
    graphics.setColour(gridColour.withAlpha(0.55f));
    for (int db = 0; db >= -60; db -= 20)
    {
        const auto y = spectrumArea.getBottom()
            - static_cast<float>(-db) / 60.0f * spectrumArea.getHeight();
        graphics.drawHorizontalLine(juce::roundToInt(y), spectrumArea.getX(),
                                    spectrumArea.getRight());
        graphics.setColour(textColour.withAlpha(0.55f));
        graphics.setFont(StepBloomLookAndFeel::getCustomFont(8.0f));
        graphics.drawText(juce::String(db) + " dB",
            juce::Rectangle<int>{juce::roundToInt(spectrumArea.getX() + 2.0f),
                juce::roundToInt(y - 8.0f), 34, 12},
            juce::Justification::centredLeft, false);
        graphics.setColour(gridColour.withAlpha(0.55f));
    }

    const auto sampleRate = juce::jmax(1.0, processor.getSampleRate());
    constexpr auto minimumFrequency = 20.0f;
    const auto maximumFrequency = juce::jmin(20000.0f,
        static_cast<float>(sampleRate * 0.5));
    const auto logRange = std::log(maximumFrequency / minimumFrequency);
    juce::Path spectrum;
    spectrum.startNewSubPath(spectrumArea.getX(), spectrumArea.getBottom());
    for (int x = 0; x < juce::jmax(1, static_cast<int>(spectrumArea.getWidth())); ++x)
    {
        const auto progress = static_cast<float>(x)
            / juce::jmax(1.0f, spectrumArea.getWidth());
        const auto frequency = minimumFrequency * std::exp(logRange * progress);
        const auto bin = juce::jlimit(1, spectrumBinCount - 1,
            juce::roundToInt(frequency * static_cast<float>(fftSize) / sampleRate));
        const auto magnitude = magnitudes[static_cast<size_t>(bin)];
        const auto decibels = juce::jlimit(-72.0f, 0.0f,
            juce::Decibels::gainToDecibels(magnitude, -72.0f));
        const auto y = spectrumArea.getBottom()
            - (decibels + 72.0f) / 72.0f * spectrumArea.getHeight();
        spectrum.lineTo(spectrumArea.getX() + static_cast<float>(x), y);
    }
    spectrum.lineTo(spectrumArea.getRight(), spectrumArea.getBottom());
    spectrum.closeSubPath();
    graphics.setColour(juce::Colour{0xff63b3d6}.withAlpha(0.22f));
    graphics.fillPath(spectrum);
    graphics.setColour(juce::Colour{0xff8dd8f2});
    graphics.strokePath(spectrum, juce::PathStrokeType(1.5f));

    const auto* lowParameter = processor.parameters.getRawParameterValue("crossoverLowMid");
    const auto* highParameter = processor.parameters.getRawParameterValue("crossoverMidHigh");
    const auto lowFrequency = lowParameter != nullptr ? lowParameter->load() : 200.0f;
    const auto highFrequency = highParameter != nullptr ? highParameter->load() : 3000.0f;
    const auto lowX = frequencyToX(lowFrequency);
    const auto highX = frequencyToX(highFrequency);
    graphics.setColour(juce::Colour{0xffffaa55});
    graphics.drawVerticalLine(juce::roundToInt(lowX), spectrumArea.getY(), spectrumArea.getBottom());
    graphics.fillEllipse(lowX - 5.0f, spectrumArea.getY() - 1.0f, 10.0f, 10.0f);
    graphics.setColour(accentColour);
    graphics.drawVerticalLine(juce::roundToInt(highX), spectrumArea.getY(), spectrumArea.getBottom());
    graphics.fillEllipse(highX - 5.0f, spectrumArea.getY() - 1.0f, 10.0f, 10.0f);

    graphics.setColour(textColour.withAlpha(0.75f));
    graphics.setFont(StepBloomLookAndFeel::getCustomFont(8.0f));
    for (const auto frequency : {20, 100, 1000, 10000, 20000})
    {
        if (static_cast<float>(frequency) > maximumFrequency)
            continue;
        const auto x = frequencyToX(static_cast<float>(frequency));
        graphics.drawText(frequency >= 1000 ? juce::String(frequency / 1000) + "k"
                                            : juce::String(frequency) + "Hz",
            juce::Rectangle<int>{juce::roundToInt(x - 18.0f),
                juce::roundToInt(spectrumArea.getBottom() - 12.0f), 36, 11},
            juce::Justification::centred, false);
    }
    graphics.setColour(accentHighlightColour);
    graphics.setFont(StepBloomLookAndFeel::getCustomFont(10.0f, juce::Font::bold));
    graphics.drawText("MULTIBAND CROSSOVER / INPUT SPECTRUM",
        juce::Rectangle<int>{14, 2, getWidth() - 28, 12},
        juce::Justification::centredLeft, false);
}

void StepBloomAudioProcessorEditor::CrossoverPanelComponent::paintOverChildren(
    juce::Graphics& graphics)
{
    if (isEnabled())
        return;

    graphics.setColour(juce::Colours::black.withAlpha(0.65f));
    graphics.fillAll();
    graphics.setColour(textColour.withAlpha(0.82f));
    graphics.setFont(StepBloomLookAndFeel::getCustomFont(12.0f, juce::Font::bold));
    graphics.drawText("MULTIBAND DISABLED - SWITCH TO MULTI MODE TO ENABLE",
                      getLocalBounds(), juce::Justification::centred, true);
}

void StepBloomAudioProcessorEditor::CrossoverPanelComponent::resized()
{
    auto controls = getLocalBounds().reduced(12);
    controls.removeFromTop(controls.getHeight() - 48);
    const auto width = controls.getWidth() / 3;
    for (int band = 0; band < 3; ++band)
    {
        auto group = controls.removeFromLeft(width).reduced(3, 0);
        bandLabels[static_cast<size_t>(band)].setBounds(group.removeFromTop(14));
        auto buttons = group.removeFromRight(82);
        bypassButtons[static_cast<size_t>(band)].setBounds(
            buttons.removeFromLeft(38).reduced(1, 3));
        soloButtons[static_cast<size_t>(band)].setBounds(
            buttons.removeFromLeft(44).reduced(1, 3));
        duckAmountSliders[static_cast<size_t>(band)].setBounds(group.reduced(0, 1));
    }
}

void StepBloomAudioProcessorEditor::CrossoverPanelComponent::mouseDown(
    const juce::MouseEvent& event)
{
    const auto* lowParameter = processor.parameters.getRawParameterValue("crossoverLowMid");
    const auto* highParameter = processor.parameters.getRawParameterValue("crossoverMidHigh");
    const auto lowX = frequencyToX(lowParameter != nullptr ? lowParameter->load() : 200.0f);
    const auto highX = frequencyToX(highParameter != nullptr ? highParameter->load() : 3000.0f);
    if (std::abs(event.position.x - lowX) <= 10.0f)
        draggedCrossover = 1;
    else if (std::abs(event.position.x - highX) <= 10.0f)
        draggedCrossover = 2;

    if (draggedCrossover != 0)
    {
        const auto id = draggedCrossover == 1 ? "crossoverLowMid" : "crossoverMidHigh";
        if (auto* parameter = processor.parameters.getParameter(id))
            parameter->beginChangeGesture();
    }
}

void StepBloomAudioProcessorEditor::CrossoverPanelComponent::mouseDrag(
    const juce::MouseEvent& event)
{
    if (draggedCrossover == 0)
        return;
    const auto* lowParameter = processor.parameters.getRawParameterValue("crossoverLowMid");
    const auto* highParameter = processor.parameters.getRawParameterValue("crossoverMidHigh");
    const auto lowFrequency = lowParameter != nullptr ? lowParameter->load() : 200.0f;
    const auto highFrequency = highParameter != nullptr ? highParameter->load() : 3000.0f;
    auto* parameter = processor.parameters.getParameter(
        draggedCrossover == 1 ? "crossoverLowMid" : "crossoverMidHigh");
    if (parameter == nullptr)
        return;
    auto frequency = xToFrequency(event.position.x);
    if (draggedCrossover == 1)
        frequency = juce::jlimit(20.0f, juce::jmin(2000.0f, highFrequency / 1.2f), frequency);
    else
        frequency = juce::jlimit(juce::jmax(1000.0f, lowFrequency * 1.2f),
                                 20000.0f, frequency);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(frequency));
    repaint();
}

void StepBloomAudioProcessorEditor::CrossoverPanelComponent::mouseUp(
    const juce::MouseEvent&)
{
    if (draggedCrossover != 0)
    {
        const auto id = draggedCrossover == 1 ? "crossoverLowMid" : "crossoverMidHigh";
        if (auto* parameter = processor.parameters.getParameter(id))
            parameter->endChangeGesture();
    }
    draggedCrossover = 0;
}

void StepBloomAudioProcessorEditor::CrossoverPanelComponent::timerCallback()
{
    const auto latest = processor.getLatestSpectrumSequence();
    if (latest < lastSpectrumSequence)
        lastSpectrumSequence = latest;
    const auto first = latest > static_cast<uint64_t>(fftSize)
        ? latest - static_cast<uint64_t>(fftSize) : 0;
    fftData.fill(0.0f);
    const auto available = static_cast<int>(latest - first);
    const auto offset = fftSize - available;
    auto readSucceeded = true;
    for (int index = 0; index < available; ++index)
    {
        float sample = 0.0f;
        if (! processor.readSpectrumSample(first + static_cast<uint64_t>(index), sample))
        {
            readSucceeded = false;
            break;
        }
        fftData[static_cast<size_t>(offset + index)] = sample;
    }
    if (readSucceeded && available > 0)
    {
        window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        const auto normalisation = 2.0f / static_cast<float>(fftSize);
        for (int bin = 0; bin < spectrumBinCount; ++bin)
            magnitudes[static_cast<size_t>(bin)] =
                fftData[static_cast<size_t>(bin)] * normalisation;
        lastSpectrumSequence = latest;
        repaint();
    }
}

float StepBloomAudioProcessorEditor::CrossoverPanelComponent::frequencyToX(
    float frequency) const
{
    constexpr auto minimum = 20.0f;
    const auto maximum = juce::jmax(minimum + 1.0f,
        juce::jmin(20000.0f, static_cast<float>(processor.getSampleRate() * 0.5)));
    const auto bounded = juce::jlimit(minimum, maximum, frequency);
    const auto progress = std::log(bounded / minimum) / std::log(maximum / minimum);
    const auto area = getLocalBounds().toFloat().reduced(14.0f, 12.0f)
        .withTrimmedBottom(52.0f);
    return area.getX() + progress * area.getWidth();
}

float StepBloomAudioProcessorEditor::CrossoverPanelComponent::xToFrequency(float x) const
{
    constexpr auto minimum = 20.0f;
    const auto maximum = juce::jmax(minimum + 1.0f,
        juce::jmin(20000.0f, static_cast<float>(processor.getSampleRate() * 0.5)));
    const auto area = getLocalBounds().toFloat().reduced(14.0f, 12.0f)
        .withTrimmedBottom(52.0f);
    const auto progress = juce::jlimit(0.0f, 1.0f,
        (x - area.getX()) / juce::jmax(1.0f, area.getWidth()));
    return minimum * std::pow(maximum / minimum, progress);
}

StepBloomAudioProcessorEditor::StepBloomAudioProcessorEditor(
    StepBloomAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor),
      processor(audioProcessor),
      envelopeEditor(audioProcessor),
      shapeProfileGrid(audioProcessor, envelopeEditor),
      crossoverPanel(audioProcessor),
      tooltipWindow(this, 700)
{
    setLookAndFeel(&lookAndFeel);
    processingModeValue = processor.parameters.getRawParameterValue("processingMode");
    isMultibandLayout = processingModeValue == nullptr
        || processingModeValue->load(std::memory_order_relaxed) >= 0.5f;
    const auto* scaleParameter = processor.parameters.getRawParameterValue("userScaleFactor");
    uiScaleFactor = scaleParameter != nullptr
        ? scaleParameter->load(std::memory_order_relaxed)
        : processor.getPresetManager().getUserDefaultZoom();

    singlebandModeButton.setClickingTogglesState(true);
    multibandModeButton.setClickingTogglesState(true);
    singlebandModeButton.setRadioGroupId(17);
    multibandModeButton.setRadioGroupId(17);
    for (auto* button : {&singlebandModeButton, &multibandModeButton})
    {
        button->setColour(juce::TextButton::buttonColourId, panelColour);
        button->setColour(juce::TextButton::buttonOnColourId, accentColour.withAlpha(0.55f));
        button->setColour(juce::TextButton::textColourOffId, textColour);
        button->setColour(juce::TextButton::textColourOnId, accentHighlightColour);
        addAndMakeVisible(*button);
    }
    singlebandModeButton.onClick = [this] { setProcessingMode(false); };
    multibandModeButton.onClick = [this] { setProcessingMode(true); };
    startTimerHz(20);

    previousPresetButton.setColour(juce::TextButton::buttonColourId, panelColour);
    previousPresetButton.setColour(juce::TextButton::textColourOffId, accentHighlightColour);
    nextPresetButton.setColour(juce::TextButton::buttonColourId, panelColour);
    nextPresetButton.setColour(juce::TextButton::textColourOffId, accentHighlightColour);
    addAndMakeVisible(previousPresetButton);
    addAndMakeVisible(nextPresetButton);

    presetBox.setLookAndFeel(&lookAndFeel);
    presetBox.setColour(juce::ComboBox::backgroundColourId, panelColour);
    presetBox.setColour(juce::ComboBox::textColourId, textColour);
    presetBox.setColour(juce::ComboBox::outlineColourId, accentColour);
    presetBox.setTextWhenNothingSelected("Current Settings");
    addAndMakeVisible(presetBox);
    presetBox.onChange = [this]
    {
        const auto selectedIndex = presetBox.getSelectedId() - 1;
        const auto presets = processor.getPresetManager().getAvailablePresets();
        if (juce::isPositiveAndBelow(selectedIndex, presets.size()))
        {
            const auto result = processor.getPresetManager().loadPreset(presets[selectedIndex]);
            showPresetResult(result);
            if (result.wasOk())
                refreshPresetList();
        }
    };
    previousPresetButton.onClick = [this]
    {
        const auto presets = processor.getPresetManager().getAvailablePresets();
        if (presets.isEmpty())
            return;
        const auto selected = juce::jmax(0, presetBox.getSelectedId() - 1);
        const auto next = (selected + presets.size() - 1) % presets.size();
        const auto result = processor.getPresetManager().loadPreset(presets[next]);
        showPresetResult(result);
        if (result.wasOk())
            refreshPresetList();
    };
    nextPresetButton.onClick = [this]
    {
        const auto presets = processor.getPresetManager().getAvailablePresets();
        if (presets.isEmpty())
            return;
        const auto selected = juce::jmax(0, presetBox.getSelectedId() - 1);
        const auto next = (selected + 1) % presets.size();
        const auto result = processor.getPresetManager().loadPreset(presets[next]);
        showPresetResult(result);
        if (result.wasOk())
            refreshPresetList();
    };

    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible(settingsButton);
    refreshPresetList();
    addAndMakeVisible(crossoverPanel);
    addAndMakeVisible(shapeProfileGrid);

    depthLabel.setText("MIX", juce::dontSendNotification);
    depthLabel.setColour(juce::Label::textColourId, accentHighlightColour);
    depthLabel.setFont(StepBloomLookAndFeel::getCustomFont(12.0f, juce::Font::bold));
    addAndMakeVisible(depthLabel);

    depthSlider.setLookAndFeel(&lookAndFeel);
    depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 20);
    depthSlider.setNumDecimalPlacesToDisplay(0);
    depthSlider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    depthSlider.setColour(juce::Slider::rotarySliderOutlineColourId, backgroundColour);
    depthSlider.setColour(juce::Slider::thumbColourId, accentHighlightColour);
    depthSlider.setColour(juce::Slider::textBoxTextColourId, accentHighlightColour);
    depthSlider.setColour(juce::Slider::textBoxBackgroundColourId, backgroundColour);
    depthSlider.setColour(juce::Slider::textBoxOutlineColourId, accentColour.withAlpha(0.7f));
    addAndMakeVisible(depthSlider);
    depthAttachment = std::make_unique<StepBloomAudioProcessor::Parameters::SliderAttachment>(
        processor.parameters, "depth", depthSlider);

    thresholdLabel.setText("SIDECHAIN THRESHOLD", juce::dontSendNotification);
    thresholdLabel.setColour(juce::Label::textColourId, accentHighlightColour);
    thresholdLabel.setFont(StepBloomLookAndFeel::getCustomFont(10.0f, juce::Font::bold));
    addAndMakeVisible(thresholdLabel);

    thresholdSlider.setLookAndFeel(&lookAndFeel);
    thresholdSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 20);
    thresholdSlider.setNumDecimalPlacesToDisplay(0);
    thresholdSlider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    thresholdSlider.setColour(juce::Slider::rotarySliderOutlineColourId, backgroundColour);
    thresholdSlider.setColour(juce::Slider::thumbColourId, accentHighlightColour);
    thresholdSlider.setColour(juce::Slider::textBoxTextColourId, accentHighlightColour);
    thresholdSlider.setColour(juce::Slider::textBoxBackgroundColourId, backgroundColour);
    thresholdSlider.setColour(juce::Slider::textBoxOutlineColourId, accentColour.withAlpha(0.7f));
    addAndMakeVisible(thresholdSlider);
    thresholdAttachment =
        std::make_unique<StepBloomAudioProcessor::Parameters::SliderAttachment>(
            processor.parameters, "threshold", thresholdSlider);

    triggerModeLabel.setText("TRIGGER MODE", juce::dontSendNotification);
    triggerModeLabel.setColour(juce::Label::textColourId, accentHighlightColour);
    triggerModeLabel.setFont(StepBloomLookAndFeel::getCustomFont(10.0f, juce::Font::bold));
    addAndMakeVisible(triggerModeLabel);

    triggerModeBox.addItemList(
        juce::StringArray{"Sync", "MIDI Trigger", "Audio Trigger"}, 1);
    triggerModeBox.setLookAndFeel(&lookAndFeel);
    triggerModeBox.setColour(juce::ComboBox::backgroundColourId, panelColour);
    triggerModeBox.setColour(juce::ComboBox::textColourId, textColour);
    triggerModeBox.setColour(juce::ComboBox::outlineColourId, accentColour);
    addAndMakeVisible(triggerModeBox);
    triggerModeAttachment =
        std::make_unique<StepBloomAudioProcessor::Parameters::ComboBoxAttachment>(
            processor.parameters, "triggerMode", triggerModeBox);

    holdOffLabel.setText("HOLD-OFF", juce::dontSendNotification);
    holdOffLabel.setColour(juce::Label::textColourId, accentHighlightColour);
    holdOffLabel.setFont(StepBloomLookAndFeel::getCustomFont(10.0f, juce::Font::bold));
    addAndMakeVisible(holdOffLabel);

    holdOffSlider.setLookAndFeel(&lookAndFeel);
    holdOffSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    holdOffSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 20);
    holdOffSlider.setNumDecimalPlacesToDisplay(0);
    holdOffSlider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    holdOffSlider.setColour(juce::Slider::rotarySliderOutlineColourId, backgroundColour);
    holdOffSlider.setColour(juce::Slider::thumbColourId, accentHighlightColour);
    holdOffSlider.setColour(juce::Slider::textBoxTextColourId, accentHighlightColour);
    holdOffSlider.setColour(juce::Slider::textBoxBackgroundColourId, backgroundColour);
    holdOffSlider.setColour(juce::Slider::textBoxOutlineColourId, accentColour.withAlpha(0.7f));
    addAndMakeVisible(holdOffSlider);
    holdOffAttachment =
        std::make_unique<StepBloomAudioProcessor::Parameters::SliderAttachment>(
            processor.parameters, "triggerHoldOff", holdOffSlider);

    triggerModeBox.onChange = [this]
    {
        const auto audioTriggerSelected = triggerModeBox.getSelectedId() == 3;
        thresholdLabel.setVisible(audioTriggerSelected);
        thresholdSlider.setVisible(audioTriggerSelected);
        holdOffLabel.setVisible(audioTriggerSelected);
        holdOffSlider.setVisible(audioTriggerSelected);
        resized();
    };
    const auto audioTriggerSelected = triggerModeBox.getSelectedId() == 3;
    thresholdLabel.setVisible(audioTriggerSelected);
    thresholdSlider.setVisible(audioTriggerSelected);
    holdOffLabel.setVisible(audioTriggerSelected);
    holdOffSlider.setVisible(audioTriggerSelected);

    cycleLabel.setText("CYCLE", juce::dontSendNotification);
    cycleLabel.setColour(juce::Label::textColourId, accentHighlightColour);
    cycleLabel.setFont(StepBloomLookAndFeel::getCustomFont(12.0f, juce::Font::bold));
    addAndMakeVisible(cycleLabel);

    cycleBox.addItemList(juce::StringArray{"1/2", "1/4", "1/8", "1/16"}, 1);
    cycleBox.setLookAndFeel(&lookAndFeel);
    cycleBox.setColour(juce::ComboBox::backgroundColourId, panelColour);
    cycleBox.setColour(juce::ComboBox::textColourId, textColour);
    cycleBox.setColour(juce::ComboBox::outlineColourId, accentColour);
    addAndMakeVisible(cycleBox);
    cycleAttachment = std::make_unique<StepBloomAudioProcessor::Parameters::ComboBoxAttachment>(
        processor.parameters, "cycle", cycleBox);

    addAndMakeVisible(envelopeEditor);
    singlebandModeButton.setToggleState(!isMultibandLayout, juce::dontSendNotification);
    multibandModeButton.setToggleState(isMultibandLayout, juce::dontSendNotification);
    crossoverPanel.setEnabled(isMultibandLayout);
    crossoverPanel.setVisible(isMultibandLayout);
    setResizeLimits(juce::roundToInt(baseWidth * 0.75f),
                    juce::roundToInt(baseHeightSingle * 0.75f),
                    juce::roundToInt(baseWidth * 2.0f),
                    juce::roundToInt(baseHeightMulti * 2.0f));
    setUiScaleFactor(uiScaleFactor, false);
}

StepBloomAudioProcessorEditor::~StepBloomAudioProcessorEditor()
{
    depthSlider.setLookAndFeel(nullptr);
    thresholdSlider.setLookAndFeel(nullptr);
    holdOffSlider.setLookAndFeel(nullptr);
    triggerModeBox.setLookAndFeel(nullptr);
    cycleBox.setLookAndFeel(nullptr);
    presetBox.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
    stopTimer();
}

void StepBloomAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.setGradientFill(juce::ColourGradient(
        juce::Colour{0xff202014}, 0.0f, 0.0f,
        backgroundColour, 0.0f, static_cast<float>(getHeight()), false));
    graphics.fillRect(getLocalBounds().toFloat());

    graphics.setColour(accentColour);
    graphics.setFont(StepBloomLookAndFeel::getCustomFont(21.0f, juce::Font::bold));
    graphics.drawText("STEPBLOOM", 24, 10, 220, 26, juce::Justification::centredLeft);
    graphics.setColour(textColour);
    graphics.setFont(StepBloomLookAndFeel::getCustomFont(11.0f, juce::Font::bold));
    graphics.drawText("TEMPO-SYNCED VOLUME SHAPER", 25, 36, 260, 15,
                      juce::Justification::centredLeft);
    graphics.setColour(accentColour.withAlpha(0.7f));
    graphics.drawHorizontalLine(64, 24.0f, static_cast<float>(getWidth() - 24));
}

void StepBloomAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    bounds.removeFromTop(52);

    const auto modeButtonsX = getWidth() - 230;
    const auto presetNextX = juce::jmax(455, modeButtonsX - 32);
    previousPresetButton.setBounds(286, 15, 30, 30);
    presetBox.setBounds(322, 15, juce::jmax(130, presetNextX - 322 - 4), 30);
    nextPresetButton.setBounds(presetNextX, 15, 28, 30);
    singlebandModeButton.setBounds(modeButtonsX, 15, 58, 30);
    multibandModeButton.setBounds(modeButtonsX + 61, 15, 54, 30);
    settingsButton.setBounds(getWidth() - 58, 12, 34, 34);

    auto controls = bounds.removeFromTop(112);
    auto mixArea = controls.removeFromLeft(72);
    depthLabel.setBounds(mixArea.removeFromTop(18));
    depthSlider.setBounds(mixArea.reduced(2, 0));

    controls.removeFromLeft(4);
    auto modeArea = controls.removeFromLeft(96);
    triggerModeLabel.setBounds(modeArea.removeFromTop(18));
    triggerModeBox.setBounds(modeArea.removeFromTop(36).reduced(0, 2));

    controls.removeFromLeft(4);
    auto cycleArea = controls.removeFromLeft(64);
    cycleLabel.setBounds(cycleArea.removeFromTop(18));
    cycleBox.setBounds(cycleArea.removeFromTop(36).reduced(0, 2));

    controls.removeFromLeft(4);
    const auto reservedForRightControls = 70 + 2 + 70;
    const auto gridWidth = juce::jmax(250, controls.getWidth() - reservedForRightControls);
    auto shapeArea = controls.removeFromLeft(gridWidth);
    shapeProfileGrid.setBounds(shapeArea.reduced(0, 1));

    controls.removeFromLeft(2);
    auto thresholdArea = controls.removeFromLeft(70);
    thresholdLabel.setBounds(thresholdArea.removeFromTop(18));
    thresholdSlider.setBounds(thresholdArea.reduced(2, 0));

    controls.removeFromLeft(2);
    auto holdOffArea = controls.removeFromLeft(70);
    holdOffLabel.setBounds(holdOffArea.removeFromTop(18));
    holdOffSlider.setBounds(holdOffArea.reduced(2, 0));

    bounds.removeFromTop(4);
    if (isMultibandLayout)
    {
        auto crossoverBounds = bounds.removeFromBottom(190);
        crossoverPanel.setBounds(crossoverBounds);
        bounds.removeFromBottom(6);
    }
    crossoverPanel.setVisible(isMultibandLayout);
    envelopeEditor.setBounds(bounds);
}

void StepBloomAudioProcessorEditor::setScaleFactor(float newScale)
{
    hostScaleFactor = juce::jmax(0.01f, newScale);
    juce::AudioProcessorEditor::setScaleFactor(hostScaleFactor * uiScaleFactor);
}

void StepBloomAudioProcessorEditor::timerCallback()
{
    const auto isMultiband = processingModeValue != nullptr
        && processingModeValue->load(std::memory_order_relaxed) >= 0.5f;
    singlebandModeButton.setToggleState(! isMultiband, juce::dontSendNotification);
    multibandModeButton.setToggleState(isMultiband, juce::dontSendNotification);
    crossoverPanel.setEnabled(isMultiband);
    updateModeLayout(isMultiband);
    shapeProfileGrid.repaint();
}

void StepBloomAudioProcessorEditor::setProcessingMode(bool multiband)
{
    if (auto* parameter = processor.parameters.getParameter("processingMode"))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(multiband ? 1.0f : 0.0f));
    singlebandModeButton.setToggleState(! multiband, juce::dontSendNotification);
    multibandModeButton.setToggleState(multiband, juce::dontSendNotification);
    crossoverPanel.setEnabled(multiband);
    updateModeLayout(multiband);
}

void StepBloomAudioProcessorEditor::updateModeLayout(bool multiband)
{
    if (isMultibandLayout == multiband)
        return;

    isMultibandLayout = multiband;
    crossoverPanel.setVisible(multiband);
    setSize(baseWidth, multiband ? baseHeightMulti : baseHeightSingle);
}

void StepBloomAudioProcessorEditor::setUiScaleFactor(float scale, bool updateParameter)
{
    constexpr std::array<float, 4> supportedScales{0.75f, 1.0f, 1.5f, 2.0f};
    uiScaleFactor = supportedScales.front();
    for (const auto supportedScale : supportedScales)
        if (std::abs(scale - supportedScale) < std::abs(scale - uiScaleFactor))
            uiScaleFactor = supportedScale;

    juce::AudioProcessorEditor::setScaleFactor(hostScaleFactor * uiScaleFactor);
    setSize(baseWidth, isMultibandLayout ? baseHeightMulti : baseHeightSingle);
    resized();
    repaint();
    if (updateParameter)
    {
        if (auto* parameter = processor.parameters.getParameter("userScaleFactor"))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(uiScaleFactor));
            parameter->endChangeGesture();
        }
    }
}

void StepBloomAudioProcessorEditor::refreshPresetList()
{
    const auto presets = processor.getPresetManager().getAvailablePresets();
    const auto activeName = processor.getPresetManager().getActivePresetName();
    presetBox.clear(juce::dontSendNotification);

    auto activeId = 0;
    for (int index = 0; index < presets.size(); ++index)
    {
        const auto name = presets.getReference(index).getFileNameWithoutExtension();
        presetBox.addItem(name, index + 1);
        if (name == activeName)
            activeId = index + 1;
    }
    presetBox.setSelectedId(activeId, juce::dontSendNotification);
    presetBox.setTextWhenNothingSelected(activeName);
}

void StepBloomAudioProcessorEditor::showPresetResult(const juce::Result& result)
{
    if (result.failed())
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, "StepBloom Presets",
            result.getErrorMessage());
}

void StepBloomAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Save as Default Preset");
    menu.addItem(2, "Save Preset...");
    menu.addItem(3, "Set Default Preset Location...");
    menu.addItem(4, "Open Presets Folder");
    juce::PopupMenu scaleMenu;
    scaleMenu.addItem(10, "75%", true, uiScaleFactor == 0.75f);
    scaleMenu.addItem(11, "100% (Default)", true, uiScaleFactor == 1.0f);
    scaleMenu.addItem(12, "150%", true, uiScaleFactor == 1.5f);
    scaleMenu.addItem(13, "200%", true, uiScaleFactor == 2.0f);
    menu.addSubMenu("UI Scale", scaleMenu);
    menu.addItem(5, "Set Current Zoom as Default");
    const auto safeThis = juce::Component::SafePointer<StepBloomAudioProcessorEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton),
        [safeThis](int selection)
        {
            if (safeThis == nullptr)
                return;

            if (selection >= 10 && selection <= 13)
            {
                constexpr std::array<float, 4> scales{0.75f, 1.0f, 1.5f, 2.0f};
                safeThis->setUiScaleFactor(scales[static_cast<size_t>(selection - 10)]);
            }
            else if (selection == 5)
            {
                const auto result = safeThis->processor.getPresetManager()
                    .setUserDefaultZoom(safeThis->uiScaleFactor);
                safeThis->showPresetResult(result);
            }
            else if (selection == 1)
            {
                const auto result = safeThis->processor.getPresetManager().saveDefaultPreset();
                safeThis->showPresetResult(result);
                if (result.wasOk())
                    safeThis->refreshPresetList();
            }
            else if (selection == 2)
            {
                const auto suggestedFile = safeThis->processor.getPresetManager()
                    .getPresetsDirectory().getChildFile("New Preset.xml");
                safeThis->presetFileChooser = std::make_unique<juce::FileChooser>(
                    "Save StepBloom Preset", suggestedFile, "*.xml");
                const auto safeEditor = juce::Component::SafePointer<
                    StepBloomAudioProcessorEditor>(safeThis.getComponent());
                safeThis->presetFileChooser->launchAsync(
                    juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectFiles
                        | juce::FileBrowserComponent::warnAboutOverwriting,
                    [safeEditor](const juce::FileChooser& chooser)
                    {
                        if (safeEditor == nullptr)
                            return;
                        const auto selectedFile = chooser.getResult();
                        if (selectedFile == juce::File{})
                            return;
                        auto file = selectedFile;
                        if (file.getFileExtension().isEmpty())
                            file = file.withFileExtension(".xml");
                        const auto saveResult = safeEditor->processor.getPresetManager()
                            .savePresetToFile(file);
                        safeEditor->showPresetResult(saveResult);
                        if (saveResult.wasOk())
                            safeEditor->refreshPresetList();
                    });
            }
            else if (selection == 3)
            {
                const auto currentDirectory = safeThis->processor.getPresetManager()
                    .getPresetsDirectory();
                safeThis->presetFileChooser = std::make_unique<juce::FileChooser>(
                    "Choose StepBloom Preset Folder", currentDirectory, juce::String{});
                const auto safeEditor = juce::Component::SafePointer<
                    StepBloomAudioProcessorEditor>(safeThis.getComponent());
                safeThis->presetFileChooser->launchAsync(
                    juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectDirectories,
                    [safeEditor](const juce::FileChooser& chooser)
                    {
                        if (safeEditor == nullptr)
                            return;
                        const auto selectedDirectory = chooser.getResult();
                        if (selectedDirectory == juce::File{})
                            return;
                        const auto result = safeEditor->processor.getPresetManager()
                            .setPresetsDirectory(selectedDirectory);
                        safeEditor->showPresetResult(result);
                        if (result.wasOk())
                            safeEditor->refreshPresetList();
                    });
            }
            else if (selection == 4)
            {
                const auto directory = safeThis->processor.getPresetManager()
                    .getPresetsDirectory();
                const auto result = directory.createDirectory();
                if (result.failed())
                {
                    safeThis->showPresetResult(result);
                    return;
                }
                directory.revealToUser();
            }
        });
}
