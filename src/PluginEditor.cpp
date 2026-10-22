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
    const auto latestSequence = processor.getLatestWaveformSequence();
    nextWaveformSequence = latestSequence
        > static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity)
        ? latestSequence - static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity)
        : 0;
    clearWaveformBins();
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(30);
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    plotArea = bounds.reduced(18.0f, 16.0f);

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
    const auto* widthParameter = parameters.getRawParameterValue("duckWidth");
    const auto* attackParameter = parameters.getRawParameterValue("attackTime");
    const auto* floorParameter = parameters.getRawParameterValue("minGain");
    const auto* tensionParameter = parameters.getRawParameterValue("releaseTension");
    const auto* endFadeParameter = parameters.getRawParameterValue("endFadeOffset");
    const auto* endGainParameter = parameters.getRawParameterValue("endGain");
    const auto duckWidth = widthParameter != nullptr ? widthParameter->load() : 0.6f;
    const auto attackTime = attackParameter != nullptr ? attackParameter->load() : 3.0f;
    const auto minGain = floorParameter != nullptr ? floorParameter->load() : 0.2f;
    const auto releaseTension = tensionParameter != nullptr ? tensionParameter->load() : 1.0f;
    const auto endFadeOffset = endFadeParameter != nullptr ? endFadeParameter->load() : 0.95f;
    const auto endGain = endGainParameter != nullptr ? endGainParameter->load() : 1.0f;
    const auto attackPhase = getAttackPhase(attackTime, duckWidth);
    constexpr int curveSamples = 160;
    for (int i = 0; i <= curveSamples; ++i)
    {
        const auto phase = static_cast<float>(i) / static_cast<float>(curveSamples);
        const auto gain = StepBloomAudioProcessor::evaluateEnvelope(
            phase, attackPhase, duckWidth, minGain, releaseTension,
            endFadeOffset, endGain);
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

    if (showSnapFeedback && draggedNode != DraggedNode::None)
    {
        graphics.setColour(accentHighlightColour);
        graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
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
    graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    graphics.drawText("START: ATTACK / GAIN     KNEE: WIDTH / TENSION     END: TAIL / GAIN",
                      plotArea.toNearestInt().reduced(2, 2),
                      juce::Justification::topRight, false);

    graphics.setColour(juce::Colour{0xffe58a45});
    graphics.fillEllipse(plotArea.getX() + 5.0f, plotArea.getY() + 4.0f, 6.0f, 6.0f);
    graphics.setColour(textColour);
    graphics.setFont(juce::FontOptions(9.0f, juce::Font::bold));
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
    juce::Path waveform;
    juce::Path fill;
    auto hasPoint = false;

    for (int x = 0; x < width; ++x)
    {
        const auto firstBin = x * waveformBinCount / width;
        const auto lastBin = juce::jmax(firstBin + 1, (x + 1) * waveformBinCount / width);
        auto binMinimum = std::numeric_limits<float>::max();
        auto binMaximum = std::numeric_limits<float>::lowest();
        for (int bin = firstBin; bin < juce::jmin(lastBin, waveformBinCount); ++bin)
        {
            const auto index = static_cast<size_t>(bin);
            if (counts[index] == 0)
                continue;
            binMinimum = juce::jmin(binMinimum, minimum[index]);
            binMaximum = juce::jmax(binMaximum, maximum[index]);
        }

        if (binMinimum == std::numeric_limits<float>::max())
            continue;

        const auto xPosition = plotArea.getX() + static_cast<float>(x);
        const auto upperY = centerY - binMaximum * amplitudeScale;
        const auto lowerY = centerY - binMinimum * amplitudeScale;
        if (!hasPoint)
        {
            waveform.startNewSubPath(xPosition, upperY);
            fill.startNewSubPath(xPosition, upperY);
            hasPoint = true;
        }
        else
        {
            waveform.lineTo(xPosition, upperY);
            fill.lineTo(xPosition, upperY);
        }
        graphics.setColour(colour.withAlpha(0.6f));
        graphics.drawVerticalLine(static_cast<int>(xPosition), upperY, lowerY);
    }

    if (!hasPoint)
        return;

    for (int x = width - 1; x >= 0; --x)
    {
        const auto firstBin = x * waveformBinCount / width;
        const auto lastBin = juce::jmax(firstBin + 1, (x + 1) * waveformBinCount / width);
        auto binMinimum = std::numeric_limits<float>::max();
        auto hasBin = false;
        for (int bin = firstBin; bin < juce::jmin(lastBin, waveformBinCount); ++bin)
        {
            const auto index = static_cast<size_t>(bin);
            if (counts[index] == 0)
                continue;
            binMinimum = juce::jmin(binMinimum, minimum[index]);
            hasBin = true;
        }
        if (hasBin)
            fill.lineTo(plotArea.getX() + static_cast<float>(x),
                        centerY - binMinimum * amplitudeScale);
    }
    fill.closeSubPath();
    graphics.setColour(colour.withAlpha(0.12f));
    graphics.fillPath(fill);
    graphics.setColour(colour.withAlpha(0.75f));
    graphics.strokePath(waveform, juce::PathStrokeType(1.2f));
}

void StepBloomAudioProcessorEditor::ThreeNodeEnvelopeEditor::timerCallback()
{
    const auto latestSequence = processor.getLatestWaveformSequence();
    if (latestSequence < nextWaveformSequence)
    {
        nextWaveformSequence = latestSequence;
        hasDisplayCycle = false;
        clearWaveformBins();
    }

    if (latestSequence - nextWaveformSequence
        > static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity))
    {
        nextWaveformSequence = latestSequence
            - static_cast<uint64_t>(StepBloomAudioProcessor::waveformFifoCapacity);
        hasDisplayCycle = false;
        clearWaveformBins();
    }

    auto changed = false;
    StepBloomAudioProcessor::WaveformFrame frame;
    while (nextWaveformSequence < latestSequence)
    {
        if (!processor.readWaveformFrame(nextWaveformSequence, frame))
        {
            nextWaveformSequence = latestSequence;
            hasDisplayCycle = false;
            clearWaveformBins();
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
    const auto* attackParameter = parameters.getRawParameterValue("attackTime");
    const auto* floorParameter = parameters.getRawParameterValue("minGain");
    const auto* widthParameter = parameters.getRawParameterValue("duckWidth");
    const auto* tensionParameter = parameters.getRawParameterValue("releaseTension");
    const auto* fadeParameter = parameters.getRawParameterValue("endFadeOffset");
    const auto* endGainParameter = parameters.getRawParameterValue("endGain");
    const auto attackTime = attackParameter != nullptr ? attackParameter->load() : 3.0f;
    const auto minGain = floorParameter != nullptr ? floorParameter->load() : 0.2f;
    const auto duckWidth = widthParameter != nullptr ? widthParameter->load() : 0.6f;
    const auto releaseTension = tensionParameter != nullptr ? tensionParameter->load() : 1.0f;
    const auto endFadeOffset = fadeParameter != nullptr ? fadeParameter->load() : 0.95f;
    const auto endGain = endGainParameter != nullptr ? endGainParameter->load() : 1.0f;
    const auto startNode = getStartNodePosition(attackTime, minGain, duckWidth);
    const auto recoveryNode = getEndNodePosition(duckWidth, releaseTension);
    const auto tailNode = getTailNodePosition(endFadeOffset, endGain);

    if (event.position.getDistanceFrom(startNode) <= 16.0f)
    {
        draggedNode = DraggedNode::Start;
        if (auto* parameter = parameters.getParameter("attackTime"))
            parameter->beginChangeGesture();
        if (auto* parameter = parameters.getParameter("minGain"))
            parameter->beginChangeGesture();
    }
    else if (event.position.getDistanceFrom(recoveryNode) <= 16.0f)
    {
        draggedNode = DraggedNode::Recovery;
        if (auto* parameter = parameters.getParameter("duckWidth"))
            parameter->beginChangeGesture();
        if (auto* parameter = parameters.getParameter("releaseTension"))
            parameter->beginChangeGesture();
    }
    else if (event.position.getDistanceFrom(tailNode) <= 16.0f)
    {
        draggedNode = DraggedNode::End;
        if (auto* parameter = parameters.getParameter("endFadeOffset"))
            parameter->beginChangeGesture();
        if (auto* parameter = parameters.getParameter("endGain"))
            parameter->beginChangeGesture();
    }

    if (draggedNode != DraggedNode::None)
    {
        showSnapFeedback = false;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
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
        if (auto* parameter = parameters.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    auto snappedGainDb = gainPosition > 0.0f
        ? juce::String(20.0f * std::log10(gainPosition), 1) + " dB"
        : juce::String("-inf dB");

    if (draggedNode == DraggedNode::Start)
    {
        const auto* widthParameter = parameters.getRawParameterValue("duckWidth");
        const auto duckWidth = widthParameter != nullptr ? widthParameter->load() : 0.6f;
        const auto maxAttackPhase = juce::jmin(duckWidth * 0.5f,
            15.0f / juce::jmax(processor.getCycleDurationMilliseconds(), 1.0e-3f));
        const auto attackPhase = snapPhaseWithinRange(phase, 0.0f, maxAttackPhase);
        const auto attackTime = attackPhase * processor.getCycleDurationMilliseconds();
        snappedGridX = plotArea.getX() + attackPhase * plotArea.getWidth();
        setParameter("attackTime", attackTime);
        setParameter("minGain", gainPosition);
        dragTooltip = "Phase: " + juce::String(attackPhase, 2) + " (grid 1/"
            + juce::String(divisions) + ") | Gain: " + snappedGainDb;
    }
    else if (draggedNode == DraggedNode::Recovery)
    {
        const auto duckWidth = snapPhaseWithinRange(phase, 0.1f, 0.95f);
        const auto curveTension = juce::jlimit(0.2f, 1.5f,
            0.2f + 1.3f * gainPosition);
        snappedGridX = plotArea.getX() + duckWidth * plotArea.getWidth();
        setParameter("duckWidth", duckWidth);
        setParameter("releaseTension", curveTension);
        dragTooltip = "Phase: " + juce::String(duckWidth, 2) + " (grid 1/"
            + juce::String(divisions) + ") | Tension: "
            + juce::String(curveTension, 2);
    }
    else if (draggedNode == DraggedNode::End)
    {
        const auto endFadeOffset = snapPhaseWithinRange(phase, 0.8f, 1.0f);
        snappedGridX = plotArea.getX() + endFadeOffset * plotArea.getWidth();
        setParameter("endFadeOffset", endFadeOffset);
        setParameter("endGain", gainPosition);
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
        graphics.setFont(juce::FontOptions(8.0f));
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
        if (auto* parameter = parameters.getParameter("attackTime"))
            parameter->endChangeGesture();
        if (auto* parameter = parameters.getParameter("minGain"))
            parameter->endChangeGesture();
    }
    else if (draggedNode == DraggedNode::Recovery)
    {
        if (auto* parameter = parameters.getParameter("duckWidth"))
            parameter->endChangeGesture();
        if (auto* parameter = parameters.getParameter("releaseTension"))
            parameter->endChangeGesture();
    }
    else if (draggedNode == DraggedNode::End)
    {
        if (auto* parameter = parameters.getParameter("endFadeOffset"))
            parameter->endChangeGesture();
        if (auto* parameter = parameters.getParameter("endGain"))
            parameter->endChangeGesture();
    }
    draggedNode = DraggedNode::None;
    showSnapFeedback = false;
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    repaint();
}

StepBloomAudioProcessorEditor::StepBloomAudioProcessorEditor(
    StepBloomAudioProcessor& audioProcessor)
    : AudioProcessorEditor(&audioProcessor),
      processor(audioProcessor),
      envelopeEditor(audioProcessor)
{
    setSize(800, 540);

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

    depthLabel.setText("MIX", juce::dontSendNotification);
    depthLabel.setColour(juce::Label::textColourId, accentHighlightColour);
    depthLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
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
    thresholdLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
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
    triggerModeLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
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
    holdOffLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
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
    cycleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
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
}

StepBloomAudioProcessorEditor::~StepBloomAudioProcessorEditor()
{
    depthSlider.setLookAndFeel(nullptr);
    thresholdSlider.setLookAndFeel(nullptr);
    holdOffSlider.setLookAndFeel(nullptr);
    triggerModeBox.setLookAndFeel(nullptr);
    cycleBox.setLookAndFeel(nullptr);
    presetBox.setLookAndFeel(nullptr);
}

void StepBloomAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.setGradientFill(juce::ColourGradient(
        juce::Colour{0xff202014}, 0.0f, 0.0f,
        backgroundColour, 0.0f, static_cast<float>(getHeight()), false));
    graphics.fillRect(getLocalBounds().toFloat());

    graphics.setColour(accentColour);
    graphics.setFont(juce::FontOptions(21.0f, juce::Font::bold));
    graphics.drawText("STEPBLOOM", 24, 10, 220, 26, juce::Justification::centredLeft);
    graphics.setColour(textColour);
    graphics.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    graphics.drawText("TEMPO-SYNCED VOLUME SHAPER", 25, 36, 260, 15,
                      juce::Justification::centredLeft);
    graphics.setColour(accentColour.withAlpha(0.7f));
    graphics.drawHorizontalLine(64, 24.0f, static_cast<float>(getWidth() - 24));
}

void StepBloomAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    bounds.removeFromTop(52);

    previousPresetButton.setBounds(292, 15, 30, 30);
    presetBox.setBounds(328, 15, 250, 30);
    nextPresetButton.setBounds(584, 15, 30, 30);
    settingsButton.setBounds(getWidth() - 58, 12, 34, 34);

    auto controls = bounds.removeFromTop(112);
    auto mixArea = controls.removeFromLeft(96);
    depthLabel.setBounds(mixArea.removeFromTop(18));
    depthSlider.setBounds(mixArea.reduced(2, 0));

    controls.removeFromLeft(12);
    auto modeArea = controls.removeFromLeft(136);
    triggerModeLabel.setBounds(modeArea.removeFromTop(18));
    triggerModeBox.setBounds(modeArea.removeFromTop(36).reduced(0, 2));

    controls.removeFromLeft(12);
    auto cycleArea = controls.removeFromLeft(92);
    cycleLabel.setBounds(cycleArea.removeFromTop(18));
    cycleBox.setBounds(cycleArea.removeFromTop(36).reduced(0, 2));

    controls.removeFromLeft(12);
    auto thresholdArea = controls.removeFromLeft(120);
    thresholdLabel.setBounds(thresholdArea.removeFromTop(18));
    thresholdSlider.setBounds(thresholdArea.reduced(2, 0));

    controls.removeFromLeft(8);
    auto holdOffArea = controls.removeFromLeft(104);
    holdOffLabel.setBounds(holdOffArea.removeFromTop(18));
    holdOffSlider.setBounds(holdOffArea.reduced(2, 0));

    bounds.removeFromTop(8);
    envelopeEditor.setBounds(bounds);
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
    const auto safeThis = juce::Component::SafePointer<StepBloomAudioProcessorEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton),
        [safeThis](int selection)
        {
            if (safeThis == nullptr)
                return;

            if (selection == 1)
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
        });
}
