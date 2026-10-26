#pragma once

#include <JuceHeader.h>
#include <vector>

#include "LoopPhaseAnalysis.h"

namespace acestep::vst3
{
class PreviewDeckComponent final : public juce::Component,
                                   private juce::ChangeListener
{
public:
    PreviewDeckComponent();
    ~PreviewDeckComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    juce::TextButton& loadButton() noexcept;
    juce::TextButton& playButton() noexcept;
    juce::TextButton& stopButton() noexcept;
    juce::TextButton& clearButton() noexcept;
    juce::TextButton& revealButton() noexcept;
    juce::TextButton& exportAlignedButton() noexcept;
    [[nodiscard]] double cropShiftMilliseconds() const noexcept;
    void setPreviewSummary(const juce::String& summary);
    // Both lanes share the original file's time axis. The extracted loop is
    // painted directly beneath its exact source selection; the third lane
    // provides a zoomable detail view for checking kick timing.
    void setWaveformFiles(const juce::String& originalFilePath,
                          const juce::String& loopFilePath,
                          double selectionStartSeconds,
                          double selectionEndSeconds,
                          int bpm,
                          int loopBars);

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void drawLane(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void paintDebugWaveforms(juce::Graphics& g);
    void rebuildTransientMarkers();
    void updateAlignmentControls();

    juce::AudioFormatManager audioFormats_;
    juce::AudioThumbnailCache thumbnailCache_ {4};
    juce::AudioThumbnail originalThumbnail_ {64, audioFormats_, thumbnailCache_};
    juce::AudioThumbnail loopThumbnail_ {64, audioFormats_, thumbnailCache_};
    juce::String originalFilePath_;
    juce::String loopFilePath_;
    juce::String requestedOriginalPath_;
    juce::String requestedLoopPath_;
    double requestedStartSeconds_ = 0.0;
    double requestedEndSeconds_ = 0.0;
    int requestedBpm_ = 0;
    int requestedBars_ = 0;
    double selectionStartSeconds_ = 0.0;
    double selectionEndSeconds_ = 0.0;
    int bpm_ = 0;
    bool inferredSelection_ = false;
    bool transientScanReady_ = false;
    std::vector<phase::Attack> transientMarkers_;
    int hoverDetailX_ = -1;
    juce::Rectangle<int> rulerBounds_;
    juce::Rectangle<int> originalLane_;
    juce::Rectangle<int> alignedLane_;
    juce::Rectangle<int> detailLane_;

    juce::Label summaryLabel_;
    juce::Label zoomLabel_;
    juce::ComboBox zoomBox_;
    juce::Slider panSlider_;
    juce::Label shiftLabel_;
    juce::Slider shiftSlider_;
    juce::TextButton resetShiftButton_ {"Reset shift"};
    juce::TextButton autoAlignButton_ {"Auto Align"};
    juce::TextButton exportAlignedButton_ {"Export adjusted WAV"};
    juce::Label phaseLabel_;
    juce::TextButton loadButton_ {"Load Preview"};
    juce::TextButton playButton_ {"Play"};
    juce::TextButton stopButton_ {"Stop"};
    juce::TextButton clearButton_ {"Clear"};
    juce::TextButton revealButton_ {"Reveal File"};
};
}  // namespace acestep::vst3

