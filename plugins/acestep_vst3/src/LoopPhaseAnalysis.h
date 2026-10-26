#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace acestep::vst3::phase
{
struct Attack final
{
    double seconds = 0.0;
    float strength = 0.0f;
};

struct Estimate final
{
    double offsetSeconds = 0.0; // Negative: attack precedes nearest beat.
    double dispersionSeconds = 0.0;
    int matchedBeats = 0;
    bool reliable = false;
};

// Simple onset candidates from the change in a short-time peak envelope.
// These are NOT classified kicks: hats, snares and other attacks can trigger it.
inline std::vector<Attack> detectAttacks(const std::vector<float>& envelope,
                                         double firstTimeSeconds,
                                         double stepSeconds = 0.005)
{
    if (envelope.size() < 12 || stepSeconds <= 0.0)
        return {};

    const auto peakLevel = *std::max_element(envelope.begin(), envelope.end());
    if (peakLevel < 0.005f)
        return {};

    std::vector<float> novelty(envelope.size(), 0.0f);
    auto maxNovelty = 0.0f;
    for (size_t i = 8; i < envelope.size(); ++i)
    {
        auto before = 0.0f;
        for (size_t lookback = 1; lookback <= 8; ++lookback)
            before += envelope[i - lookback];
        before /= 8.0f;
        novelty[i] = std::max(0.0f, envelope[i] - before);
        maxNovelty = std::max(maxNovelty, novelty[i]);
    }

    const auto threshold = std::max(0.015f * peakLevel, 0.23f * maxNovelty);
    const auto minDistance = std::max(1, static_cast<int>(std::round(0.085 / stepSeconds)));
    std::vector<Attack> attacks;

    for (size_t i = 9; i + 2 < novelty.size(); ++i)
    {
        if (novelty[i] < threshold || envelope[i] < peakLevel * 0.12f
            || novelty[i] < novelty[i - 1] || novelty[i] < novelty[i + 1])
            continue;

        const auto candidate = Attack {firstTimeSeconds + i * stepSeconds, novelty[i]};
        if (!attacks.empty()
            && candidate.seconds - attacks.back().seconds < minDistance * stepSeconds)
        {
            if (candidate.strength > attacks.back().strength)
                attacks.back() = candidate;
            continue;
        }
        attacks.push_back(candidate);
    }
    return attacks;
}

inline double median(std::vector<double> values)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const auto mid = values.size() / 2;
    return values.size() % 2 == 0 ? (values[mid - 1] + values[mid]) / 2.0 : values[mid];
}

// For each beat, keep the strongest candidate within +/- half a beat.
// Robust median estimates a common phase but deliberately rejects scattered
// onsets instead of confidently labelling them as a kick-offset.
inline Estimate estimateBeatPhase(const std::vector<Attack>& attacks,
                                   double selectionStartSeconds,
                                   double durationSeconds,
                                   double bpm)
{
    Estimate result;
    if (bpm <= 0.0 || durationSeconds <= 0.0)
        return result;

    const auto beatSeconds = 60.0 / bpm;
    std::vector<double> phases;
    for (int beat = 0; beat * beatSeconds < durationSeconds; ++beat)
    {
        const auto gridTime = selectionStartSeconds + beat * beatSeconds;
        const Attack* strongest = nullptr;
        for (const auto& attack : attacks)
        {
            if (std::abs(attack.seconds - gridTime) >= beatSeconds * 0.48)
                continue;
            if (strongest == nullptr || attack.strength > strongest->strength)
                strongest = &attack;
        }
        if (strongest != nullptr)
            phases.push_back(strongest->seconds - gridTime);
    }

    result.matchedBeats = static_cast<int>(phases.size());
    if (phases.size() < 4)
        return result;

    result.offsetSeconds = median(phases);
    std::vector<double> distances;
    distances.reserve(phases.size());
    for (const auto phase : phases)
        distances.push_back(std::abs(phase - result.offsetSeconds));
    result.dispersionSeconds = median(distances);
    result.reliable = result.dispersionSeconds <= 0.045
                      && phases.size() * beatSeconds >= durationSeconds * 0.45;
    return result;
}
} // namespace acestep::vst3::phase
