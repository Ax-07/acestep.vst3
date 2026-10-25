#include "PreviewDeckComponent.h"

#include "V2Chrome.h"
#include <cmath>

namespace acestep::vst3
{
PreviewDeckComponent::PreviewDeckComponent()
{
    audioFormats_.registerBasicFormats();
    originalThumbnail_.addChangeListener(this);
    loopThumbnail_.addChangeListener(this);
    zoomLabel_.setText("Detail zoom", juce::dontSendNotification);
    zoomLabel_.setColour(juce::Label::textColourId, v2::kLabelMuted);
    for (const int zoom : {1, 2, 4, 8, 16})
        zoomBox_.addItem(juce::String(zoom) + "x", zoom);
    zoomBox_.setSelectedId(1, juce::dontSendNotification);
    zoomBox_.onChange = [this] {
        panSlider_.setEnabled(zoomBox_.getSelectedId() > 1);
        repaint();
    };
    zoomBox_.setTooltip("Zoom the extracted-loop detail without changing the audio.");
    panSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panSlider_.setRange(0.0, 100.0, 1.0);
    panSlider_.setTooltip("Move through the zoomed loop from its beginning to its end.");
    panSlider_.onValueChange = [this] { repaint(); };
    panSlider_.setEnabled(false);
    addAndMakeVisible(zoomLabel_);
    addAndMakeVisible(zoomBox_);
    addAndMakeVisible(panSlider_);
    summaryLabel_.setColour(juce::Label::textColourId, v2::kLabelPrimary);
    summaryLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel_);
    for (auto* button : {&loadButton_, &playButton_, &stopButton_, &clearButton_, &revealButton_})
    {
        addAndMakeVisible(*button);
    }
}

PreviewDeckComponent::~PreviewDeckComponent()
{
    originalThumbnail_.removeChangeListener(this);
    loopThumbnail_.removeChangeListener(this);
}

void PreviewDeckComponent::paint(juce::Graphics& g)
{
    v2::drawModule(g, getLocalBounds(), "Preview / Waveform Debug", v2::kAccentMint);
    paintDebugWaveforms(g);
}

void PreviewDeckComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    area.removeFromTop(24);
    summaryLabel_.setBounds(area.removeFromTop(66));
    area.removeFromTop(6);
    auto buttons = area.removeFromTop(34);
    loadButton_.setBounds(buttons.removeFromLeft(148));
    buttons.removeFromLeft(8);
    playButton_.setBounds(buttons.removeFromLeft(92));
    buttons.removeFromLeft(8);
    stopButton_.setBounds(buttons.removeFromLeft(92));
    buttons.removeFromLeft(8);
    clearButton_.setBounds(buttons.removeFromLeft(92));
    buttons.removeFromLeft(8);
    revealButton_.setBounds(buttons.removeFromLeft(136));

    area.removeFromTop(8);
    auto tools = area.removeFromTop(28);
    zoomLabel_.setBounds(tools.removeFromLeft(98));
    zoomBox_.setBounds(tools.removeFromLeft(90));
    tools.removeFromLeft(12);
    panSlider_.setBounds(tools.removeFromLeft(290));

    area.removeFromTop(12);
    rulerBounds_ = area.removeFromTop(22);
    area.removeFromTop(6);
    area.removeFromTop(18); // Source label is painted directly above the lane.
    originalLane_ = area.removeFromTop(82);
    area.removeFromTop(7);
    area.removeFromTop(18);
    alignedLane_ = area.removeFromTop(82);
    area.removeFromTop(8);
    area.removeFromTop(18);
    detailLane_ = area.removeFromTop(118);
}

void PreviewDeckComponent::setWaveformFiles(const juce::String& originalFilePath,
                                            const juce::String& loopFilePath,
                                            double selectionStartSeconds,
                                            double selectionEndSeconds,
                                            int bpm,
                                            int loopBars)
{
    if (requestedOriginalPath_ == originalFilePath
        && requestedLoopPath_ == loopFilePath
        && std::abs(requestedStartSeconds_ - selectionStartSeconds) < 0.000001
        && std::abs(requestedEndSeconds_ - selectionEndSeconds) < 0.000001
        && requestedBpm_ == bpm && requestedBars_ == loopBars
        && (loopFilePath_.isNotEmpty() || loopFilePath.isEmpty()))
        return;

    requestedOriginalPath_ = originalFilePath;
    requestedLoopPath_ = loopFilePath;
    requestedStartSeconds_ = selectionStartSeconds;
    requestedEndSeconds_ = selectionEndSeconds;
    requestedBpm_ = bpm;
    requestedBars_ = loopBars;

    const auto validOriginal = originalFilePath.isNotEmpty()
                                   && juce::File(originalFilePath).existsAsFile()
                                   ? originalFilePath : juce::String();
    const auto validLoop = loopFilePath.isNotEmpty()
                               && juce::File(loopFilePath).existsAsFile()
                               ? loopFilePath : juce::String();
    auto sourcePath = validOriginal.isNotEmpty() ? validOriginal : validLoop;
    auto inferred = false;

    // Previous plugin versions saved the original beside "-4bars.wav"
    // without storing the crop start. Recover that known naming/layout so
    // Xavier can inspect existing renders without generating them again.
    if (validOriginal.isEmpty() && validLoop.isNotEmpty() && loopBars > 0)
    {
        const auto loopFile = juce::File(validLoop);
        const auto suffix = "-" + juce::String(loopBars) + "bars";
        const auto stem = loopFile.getFileNameWithoutExtension();
        if (stem.endsWith(suffix))
        {
            const auto candidate = loopFile.getSiblingFile(
                stem.dropLastCharacters(suffix.length()) + ".wav");
            if (candidate.existsAsFile())
                sourcePath = candidate.getFullPathName();
        }
    }

    if (loopBars > 0 && sourcePath != validLoop
        && selectionEndSeconds <= selectionStartSeconds)
    {
        std::unique_ptr<juce::AudioFormatReader> originalReader(
            audioFormats_.createReaderFor(juce::File(sourcePath)));
        std::unique_ptr<juce::AudioFormatReader> loopReader(
            audioFormats_.createReaderFor(juce::File(validLoop)));
        if (originalReader != nullptr && loopReader != nullptr
            && originalReader->sampleRate > 0 && loopReader->sampleRate > 0)
        {
            const auto loopSeconds = loopReader->lengthInSamples / loopReader->sampleRate;
            const auto secondsPerBar = loopSeconds / loopBars;
            const auto sourceSeconds = originalReader->lengthInSamples / originalReader->sampleRate;
            if (secondsPerBar > 0.0)
            {
                const auto availableBars = static_cast<juce::int64>(
                    std::floor(sourceSeconds / secondsPerBar + 1.0e-6));
                const auto startBar = juce::jmax<juce::int64>(
                    0, (availableBars - static_cast<juce::int64>(loopBars)) / 2);
                const auto loopSamples = static_cast<juce::int64>(
                    std::llround(loopSeconds * originalReader->sampleRate));
                const auto maxStart = juce::jmax<juce::int64>(
                    0, originalReader->lengthInSamples - loopSamples);
                const auto startSample = juce::jmin(maxStart,
                    static_cast<juce::int64>(std::llround(
                        startBar * secondsPerBar * originalReader->sampleRate)));
                selectionStartSeconds = startSample / originalReader->sampleRate;
                selectionEndSeconds = (startSample + loopSamples) / originalReader->sampleRate;
                inferred = true;
            }
        }
    }
    if (originalFilePath_ == sourcePath && loopFilePath_ == validLoop
        && std::abs(selectionStartSeconds_ - selectionStartSeconds) < 0.000001
        && std::abs(selectionEndSeconds_ - selectionEndSeconds) < 0.000001
        && bpm_ == bpm && inferredSelection_ == inferred)
        return;

    originalFilePath_ = sourcePath;
    loopFilePath_ = validLoop;
    selectionStartSeconds_ = selectionStartSeconds;
    selectionEndSeconds_ = selectionEndSeconds;
    bpm_ = bpm;
    inferredSelection_ = inferred;
    hoverDetailX_ = -1;
    panSlider_.setValue(0.0, juce::dontSendNotification);

    originalThumbnail_.clear();
    loopThumbnail_.clear();
    if (sourcePath.isNotEmpty())
        originalThumbnail_.setSource(new juce::FileInputSource(juce::File(sourcePath)));
    if (validLoop.isNotEmpty())
        loopThumbnail_.setSource(new juce::FileInputSource(juce::File(validLoop)));
    repaint();
}

void PreviewDeckComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}

void PreviewDeckComponent::mouseMove(const juce::MouseEvent& event)
{
    const auto nextX = detailLane_.contains(event.getPosition()) ? event.x : -1;
    if (nextX != hoverDetailX_)
    {
        hoverDetailX_ = nextX;
        repaint(detailLane_.expanded(4, 23));
    }
}

void PreviewDeckComponent::mouseExit(const juce::MouseEvent&)
{
    hoverDetailX_ = -1;
    repaint(detailLane_.expanded(4, 23));
}

void PreviewDeckComponent::drawLane(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    g.setColour(v2::kDisplayFill);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    g.setColour(v2::kModuleStroke.withAlpha(0.85f));
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.0f);
    g.setColour(v2::kLabelMuted.withAlpha(0.25f));
    g.drawHorizontalLine(bounds.getCentreY(), static_cast<float>(bounds.getX()),
                         static_cast<float>(bounds.getRight()));
}

void PreviewDeckComponent::paintDebugWaveforms(juce::Graphics& g)
{
    drawLane(g, originalLane_);
    drawLane(g, alignedLane_);
    drawLane(g, detailLane_);

    const auto sourceDuration = originalThumbnail_.getTotalLength();
    const auto loopDuration = loopThumbnail_.getTotalLength();
    if (originalFilePath_.isEmpty() || loopFilePath_.isEmpty()
        || sourceDuration <= 0.0 || loopDuration <= 0.0)
    {
        g.setColour(v2::kLabelMuted);
        g.drawFittedText("Generate a bar-length WAV to see its original source and the extracted loop.",
                         originalLane_.reduced(12), juce::Justification::centred, 2);
        return;
    }

    const auto hasExactSelection = selectionEndSeconds_ > selectionStartSeconds_
                                   && selectionEndSeconds_ <= sourceDuration + 0.02;
    const auto start = hasExactSelection ? juce::jlimit(0.0, sourceDuration, selectionStartSeconds_)
                                         : 0.0;
    const auto end = hasExactSelection ? juce::jlimit(0.0, sourceDuration, selectionEndSeconds_)
                                       : sourceDuration;
    const auto sourceX = [this, sourceDuration](double time) {
        return originalLane_.getX() + juce::roundToInt(
            originalLane_.getWidth() * time / sourceDuration);
    };
    const auto left = sourceX(start);
    const auto right = juce::jmax(left + 1, sourceX(end));
    const auto alignedRange = juce::Rectangle<int>(left, alignedLane_.getY(),
                                                   right - left, alignedLane_.getHeight());

    g.setColour(v2::kAccentAmber.withAlpha(0.17f));
    g.fillRect(juce::Rectangle<int>(left, originalLane_.getY(),
                                   right - left, originalLane_.getHeight()));
    g.fillRect(alignedRange);

    // Both overview lanes share the *original* timeline, so the lower
    // waveform can only appear directly beneath the exact cropped interval.
    for (int tick = 0; tick <= 8; ++tick)
    {
        const auto time = sourceDuration * tick / 8.0;
        const auto x = sourceX(time);
        g.setColour(v2::kModuleStroke.withAlpha(0.7f));
        g.drawVerticalLine(x, static_cast<float>(originalLane_.getY()),
                           static_cast<float>(alignedLane_.getBottom()));
        const auto labelRect = juce::Rectangle<int>(x - 30, rulerBounds_.getY(), 60, rulerBounds_.getHeight());
        g.setColour(v2::kLabelMuted);
        g.drawFittedText(juce::String(time, 1) + "s", labelRect,
                         juce::Justification::centred, 1);
    }

    {
        juce::Graphics::ScopedSaveState scope(g);
        g.reduceClipRegion(originalLane_);
        g.setColour(v2::kAccentMint);
        originalThumbnail_.drawChannels(g, originalLane_.reduced(2),
                                        0.0, sourceDuration, 0.90f);
    }
    {
        juce::Graphics::ScopedSaveState scope(g);
        g.reduceClipRegion(alignedRange);
        g.setColour(v2::kAccentAmber);
        loopThumbnail_.drawChannels(g, alignedRange.reduced(1, 2),
                                    0.0, loopDuration, 0.92f);
    }

    g.setColour(v2::kAccentAmber);
    for (const auto x : {left, right})
        g.drawVerticalLine(x, static_cast<float>(originalLane_.getY()),
                           static_cast<float>(alignedLane_.getBottom()));

    g.setColour(v2::kLabelPrimary);
    g.drawText("ORIGINAL  //  " + juce::File(originalFilePath_).getFileName()
               + "  //  " + juce::String(sourceDuration, 2) + " s",
               originalLane_.withY(originalLane_.getY() - 18).withHeight(18),
               juce::Justification::centredLeft);
    g.drawText(hasExactSelection
                   ? "EXTRACTED LOOP  //  source " + juce::String(start, 3)
                         + " -> " + juce::String(end, 3) + " s  //  "
                         + juce::String(loopDuration, 3) + " s"
                         + (inferredSelection_ ? " (inferred for older file)" : "")
                   : "CLIP  //  exact source selection unavailable for this older file",
               alignedLane_.withY(alignedLane_.getY() - 18).withHeight(18),
               juce::Justification::centredLeft);

    const auto zoom = juce::jmax(1, zoomBox_.getSelectedId());
    const auto detailSpan = loopDuration / zoom;
    const auto detailStart = juce::jmax(0.0, loopDuration - detailSpan)
                             * panSlider_.getValue() / 100.0;
    const auto detailEnd = detailStart + detailSpan;
    const auto detailX = [this, detailStart, detailSpan](double seconds) {
        return detailLane_.getX() + juce::roundToInt(
            (seconds - detailStart) * detailLane_.getWidth() / detailSpan);
    };

    if (bpm_ > 0)
    {
        const auto beatPeriod = 60.0 / bpm_;
        const auto firstBeat = juce::jmax(0, static_cast<int>(std::ceil(detailStart / beatPeriod)));
        for (int beat = firstBeat; beat * beatPeriod <= detailEnd && beat < firstBeat + 300; ++beat)
        {
            const auto x = detailX(beat * beatPeriod);
            g.setColour(v2::kAccentBlue.withAlpha(beat % 4 == 0 ? 0.75f : 0.30f));
            g.drawVerticalLine(x, static_cast<float>(detailLane_.getY()),
                               static_cast<float>(detailLane_.getBottom()));
        }
    }
    {
        juce::Graphics::ScopedSaveState scope(g);
        g.reduceClipRegion(detailLane_);
        // Overlay the precise source interval in blue with the exported loop
        // in mint. Any change introduced by the export becomes visible here.
        if (hasExactSelection)
        {
            g.setColour(v2::kAccentBlue.withAlpha(0.45f));
            originalThumbnail_.drawChannels(g, detailLane_.reduced(2),
                                            start + detailStart, start + detailEnd, 0.95f);
        }
        g.setColour(v2::kAccentMint.withAlpha(0.85f));
        loopThumbnail_.drawChannels(g, detailLane_.reduced(2),
                                    detailStart, detailEnd, 0.95f);
    }
    g.setColour(v2::kLabelPrimary);
    g.drawText("LOOP DETAIL  //  " + juce::String(zoom) + "x  //  "
               + juce::String(detailStart, 3) + " - " + juce::String(detailEnd, 3) + " s"
               + (hasExactSelection ? "  //  blue: source / mint: export" : "")
               + (bpm_ > 0 ? "  //  beat grid " + juce::String(bpm_) + " BPM" : ""),
               detailLane_.withY(detailLane_.getY() - 18).withHeight(18),
               juce::Justification::centredLeft);

    if (hoverDetailX_ >= detailLane_.getX() && hoverDetailX_ < detailLane_.getRight())
    {
        const auto timeOnLoop = detailStart + detailSpan
                               * (hoverDetailX_ - detailLane_.getX()) / detailLane_.getWidth();
        g.setColour(v2::kAccentAmber);
        g.drawVerticalLine(hoverDetailX_, static_cast<float>(detailLane_.getY()),
                           static_cast<float>(detailLane_.getBottom()));
        auto tooltip = "loop " + juce::String(timeOnLoop, 3) + "s";
        if (hasExactSelection)
            tooltip += "  |  original " + juce::String(start + timeOnLoop, 3) + "s";
        const auto width = 242;
        const auto popupX = juce::jlimit(detailLane_.getX(), detailLane_.getRight() - width,
                                        hoverDetailX_ - width / 2);
        const auto popup = juce::Rectangle<int>(popupX, detailLane_.getY() + 3, width, 22);
        g.setColour(v2::kChassisOuter.withAlpha(0.94f));
        g.fillRoundedRectangle(popup.toFloat(), 4.0f);
        g.setColour(v2::kLabelPrimary);
        g.drawFittedText(tooltip, popup, juce::Justification::centred, 1);
    }
}

juce::TextButton& PreviewDeckComponent::loadButton() noexcept { return loadButton_; }
juce::TextButton& PreviewDeckComponent::playButton() noexcept { return playButton_; }
juce::TextButton& PreviewDeckComponent::stopButton() noexcept { return stopButton_; }
juce::TextButton& PreviewDeckComponent::clearButton() noexcept { return clearButton_; }
juce::TextButton& PreviewDeckComponent::revealButton() noexcept { return revealButton_; }

void PreviewDeckComponent::setPreviewSummary(const juce::String& summary)
{
    summaryLabel_.setText(summary, juce::dontSendNotification);
}
}  // namespace acestep::vst3
