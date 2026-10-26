#include "PluginPreview.h"

#include <cmath>

namespace acestep::vst3
{
PluginPreview::PluginPreview()
{
    formatManager_.registerBasicFormats();
}

PluginPreview::~PluginPreview()
{
    clear();
}

void PluginPreview::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock lock(lock_);
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    transportSource_.prepareToPlay(samplesPerBlock, sampleRate_);
}

void PluginPreview::releaseResources()
{
    const juce::ScopedLock lock(lock_);
    transportSource_.releaseResources();
}

void PluginPreview::render(juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedTryLock lock(lock_);
    if (!lock.isLocked() || readerSource_ == nullptr)
    {
        return;
    }

    juce::AudioSourceChannelInfo channelInfo(&buffer, 0, buffer.getNumSamples());
    transportSource_.getNextAudioBlock(channelInfo);
}

bool PluginPreview::loadFile(const juce::File& file, juce::String& errorMessage)
{
    errorMessage = {};
    if (!file.existsAsFile())
    {
        errorMessage = "Preview file not found.";
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
    if (reader == nullptr)
    {
        errorMessage = "Preview file format is not supported.";
        return false;
    }

    const auto readerSampleRate = reader->sampleRate;
    auto nextReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);

    const juce::ScopedLock lock(lock_);
    transportSource_.stop();
    transportSource_.setSource(nullptr);
    readerSource_.reset();
    transportSource_.setSource(nextReaderSource.get(), 0, nullptr, readerSampleRate);
    readerSource_ = std::move(nextReaderSource);
    currentFile_ = file;
    auditioningSubsection_ = false;
    return true;
}

bool PluginPreview::auditionSubsection(const juce::File& sourceFile,
                                      double startSeconds,
                                      double lengthSeconds,
                                      juce::String& errorMessage)
{
    errorMessage = {};
    if (!sourceFile.existsAsFile() || !std::isfinite(startSeconds)
        || !std::isfinite(lengthSeconds) || startSeconds < 0.0 || lengthSeconds <= 0.0)
    {
        errorMessage = "Invalid original WAV or proposed selection.";
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(sourceFile));
    if (reader == nullptr || reader->sampleRate <= 0.0)
    {
        errorMessage = "Could not open the original WAV for audition.";
        return false;
    }

    const auto startSample = static_cast<juce::int64>(std::llround(startSeconds * reader->sampleRate));
    const auto sampleCount = static_cast<juce::int64>(std::llround(lengthSeconds * reader->sampleRate));
    if (sampleCount <= 0 || startSample < 0
        || startSample + sampleCount > reader->lengthInSamples)
    {
        errorMessage = "The proposed loop extends past the original WAV.";
        return false;
    }

    const auto fileSampleRate = reader->sampleRate;
    auto subsection = std::make_unique<juce::AudioSubsectionReader>(
        reader.release(), startSample, sampleCount, true);
    auto nextReaderSource = std::make_unique<juce::AudioFormatReaderSource>(
        subsection.release(), true);
    nextReaderSource->setLooping(true);

    const juce::ScopedLock lock(lock_);
    transportSource_.stop();
    transportSource_.setSource(nullptr);
    readerSource_.reset();
    transportSource_.setSource(nextReaderSource.get(), 0, nullptr, fileSampleRate);
    readerSource_ = std::move(nextReaderSource);
    auditioningSubsection_ = true;
    transportSource_.setPosition(0.0);
    transportSource_.start();
    return true;
}

void PluginPreview::clear()
{
    const juce::ScopedLock lock(lock_);
    transportSource_.stop();
    transportSource_.setSource(nullptr);
    readerSource_.reset();
    currentFile_ = juce::File();
    auditioningSubsection_ = false;
}

void PluginPreview::play()
{
    const juce::ScopedLock lock(lock_);
    if (readerSource_ == nullptr)
    {
        return;
    }

    transportSource_.setPosition(0.0);
    transportSource_.start();
}

void PluginPreview::stop()
{
    const juce::ScopedLock lock(lock_);
    transportSource_.stop();
}

void PluginPreview::revealToUser() const
{
    const juce::ScopedLock lock(lock_);
    if (currentFile_.existsAsFile())
    {
        currentFile_.revealToUser();
    }
}

bool PluginPreview::hasLoadedFile() const
{
    const juce::ScopedLock lock(lock_);
    return readerSource_ != nullptr && currentFile_.existsAsFile();
}

bool PluginPreview::isPlaying() const
{
    const juce::ScopedLock lock(lock_);
    return transportSource_.isPlaying();
}

bool PluginPreview::isAuditioningSubsection() const
{
    const juce::ScopedLock lock(lock_);
    return auditioningSubsection_;
}

juce::String PluginPreview::getDisplayName() const
{
    const juce::ScopedLock lock(lock_);
    return currentFile_.existsAsFile() ? currentFile_.getFileName() : juce::String();
}

juce::String PluginPreview::getFilePath() const
{
    const juce::ScopedLock lock(lock_);
    return currentFile_.getFullPathName();
}
}  // namespace acestep::vst3
