#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace acestep::vst3
{
// Deliberately JUCE-free: phase estimation can be tested without a VST host.
// onsetPerMs is a non-negative attack-strength envelope sampled at 1 kHz.
struct BeatAlignmentEstimate final
{
    bool confident = false;
    int beatOffsetMs = 0;  // Positive: attacks are late versus the DAW grid.
};

// For negative delayFrames, the export reads ahead (advances the music).
// Wrapping rather than padding keeps the loop sample count unchanged.
inline std::int64_t wrappedReadOffset(std::int64_t outputFrame,
                                      std::int64_t totalFrames,
                                      std::int64_t delayFrames)
{
    if (totalFrames <= 0)
        return 0;
    const auto normalized = (delayFrames % totalFrames + totalFrames) % totalFrames;
    return (outputFrame + totalFrames - normalized) % totalFrames;
}

inline std::vector<float> makeRiseOnset(const std::vector<float>& energyPerMs)
{
    std::vector<float> onset(energyPerMs.size(), 0.0f);
    for (size_t frame = 10; frame < energyPerMs.size(); ++frame)
    {
        double before = 0.0;
        for (size_t previous = frame - 10; previous < frame; ++previous)
            before += energyPerMs[previous];
        onset[frame] = std::max(0.0f, energyPerMs[frame]
                                        - static_cast<float>(before / 10.0));
    }
    return onset;
}

inline BeatAlignmentEstimate estimateBeatOffsetMs(const std::vector<float>& onsetPerMs,
                                                  double beatPeriodMs,
                                                  int maxOffsetMs = 80)
{
    BeatAlignmentEstimate result;
    if (!std::isfinite(beatPeriodMs) || beatPeriodMs < 150.0
        || beatPeriodMs > 2100.0 || maxOffsetMs < 1 || maxOffsetMs > 100)
        return result;

    // Leave a margin at both ends; an interior four-bar section contains
    // plenty of beats at normal dance-music tempos.
    const auto beatCount = static_cast<int>(
        std::floor((static_cast<double>(onsetPerMs.size()) - maxOffsetMs - 1)
                   / beatPeriodMs));
    if (beatCount < 6)
        return result;

    const auto strengthAt = [&](int index) -> float {
        return index >= 0 && index < static_cast<int>(onsetPerMs.size())
                   ? std::max(0.0f, onsetPerMs[static_cast<size_t>(index)])
                   : 0.0f;
    };

    float bestScore = 0.0f;
    float sumScores = 0.0f;
    int bestOffset = 0;
    for (int offset = -maxOffsetMs; offset <= maxOffsetMs; ++offset)
    {
        float score = 0.0f;
        for (int beat = 1; beat <= beatCount; ++beat)
        {
            const auto centre = static_cast<int>(std::llround(beat * beatPeriodMs)) + offset;
            score += strengthAt(centre);
        }

        sumScores += score;
        if (score > bestScore)
        {
            bestScore = score;
            bestOffset = offset;
        }
    }

    const auto candidateCount = 2 * maxOffsetMs + 1;
    const auto meanScore = sumScores / candidateCount;
    // A broad pad, a weak transient pattern or silence must not cause a shift.
    if (meanScore <= 0.0f || bestScore < 2.0f * meanScore)
        return result;

    // Reject false peaks dominated by only one or two unusually loud hits:
    // the same phase should recur across most of the beats.
    int supportingBeats = 0;
    const auto backgroundMean = std::accumulate(onsetPerMs.begin(), onsetPerMs.end(), 0.0)
                                / onsetPerMs.size();
    for (int beat = 1; beat <= beatCount; ++beat)
    {
        const auto centre = static_cast<int>(std::llround(beat * beatPeriodMs));
        float localMax = 0.0f;
        for (int offset = -maxOffsetMs; offset <= maxOffsetMs; ++offset)
            localMax = std::max(localMax, strengthAt(centre + offset));

        float match = 0.0f;
        for (int d = -2; d <= 2; ++d)
            match = std::max(match, strengthAt(centre + bestOffset + d));
        if (localMax > 2.0f * backgroundMean && match >= 0.6f * localMax)
            ++supportingBeats;
    }

    if (supportingBeats * 10 < beatCount * 6)
        return result;

    result.confident = true;
    result.beatOffsetMs = bestOffset;
    return result;
}
}  // namespace acestep::vst3
