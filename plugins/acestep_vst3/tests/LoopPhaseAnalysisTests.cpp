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
    // Moving the crop earlier by 110 ms should move attacks onto the grid.
    const auto adjusted = estimateBeatPhase(candidates, start - 0.11, beat * 16.0, bpm);
    assert(adjusted.reliable);
    assert(std::abs(adjusted.offsetSeconds) < 0.001);

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
