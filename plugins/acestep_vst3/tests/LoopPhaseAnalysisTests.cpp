#include "../src/LoopPhaseAnalysis.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace acestep::vst3::phase;

int main()
{
    constexpr double bpm = 128.0;
    constexpr double beat = 60.0 / bpm;
    constexpr double start = 11.25;
    std::vector<Attack> candidates;
    for (int i = 0; i < 16; ++i)
    {
        if (i > 0)
            candidates.push_back({start + i * beat - 0.11, 0.9f});
        candidates.push_back({start + i * beat + 0.08, 0.14f}); // quieter hats
    }
    const auto detected = estimateBeatPhase(candidates, start, beat * 16.0, bpm);
    assert(detected.reliable);
    assert(std::abs(detected.offsetSeconds + 0.11) < 0.001);
    const auto proposed = suggestTotalCropShiftMs(detected, 0.0, start, start + 7.5, 30.0);
    assert(proposed.has_value() && std::abs(*proposed + 110.0) < 0.001);
    // Moving the crop earlier by 110 ms should move attacks onto the grid.
    const auto adjusted = estimateBeatPhase(candidates, start - 0.11, beat * 16.0, bpm);
    assert(adjusted.reliable);
    assert(std::abs(adjusted.offsetSeconds) < 0.001);
    assert(!suggestTotalCropShiftMs(adjusted, -110.0, start, start + 7.5, 30.0).has_value());

    // An existing -80 ms user adjustment plus an estimated -30 ms residual
    // must suggest -110 ms TOTAL (not reset the dial to -30 ms).
    Estimate residual;
    residual.reliable = true;
    residual.offsetSeconds = -0.03;
    const auto combined = suggestTotalCropShiftMs(residual, -80.0, start, start + 7.5, 30.0);
    assert(combined.has_value() && *combined == -110.0);
    assert(!suggestTotalCropShiftMs(residual, -480.0, start, start + 7.5, 30.0).has_value());
    assert(!suggestTotalCropShiftMs(residual, 0.0, 0.01, 7.51, 30.0).has_value());
    residual.reliable = false;
    assert(!suggestTotalCropShiftMs(residual, 0.0, start, start + 7.5, 30.0).has_value());

    std::vector<Attack> scattered;
    for (int i = 0; i < 16; ++i)
        scattered.push_back({start + i * beat + (i % 2 ? 0.15 : -0.15), 0.9f});
    assert(!estimateBeatPhase(scattered, start, beat * 16.0, bpm).reliable);

    std::vector<float> envelope(2000, 0.07f);
    for (int i = 20; i < 1900; i += 94)
    {
        envelope[i] = 0.95f;
        envelope[i + 1] = 0.80f;
        envelope[i + 2] = 0.55f;
    }
    const auto attacks = detectAttacks(envelope, 0.0);
    assert(attacks.size() >= 19);
    assert(attacks.size() <= 21);
    assert(std::abs(attacks[0].seconds - 0.10) <= 0.005);
    assert(detectAttacks(std::vector<float>(200, 0.0f), 0.0).empty());

    std::cout << "Loop phase analysis tests passed\n";
}
