#include "../src/LoopAlignment.h"

#include <cassert>
#include <cmath>
#include <vector>

using acestep::vst3::estimateBeatOffsetMs;
using acestep::vst3::makeRiseOnset;
using acestep::vst3::wrappedReadOffset;

static std::vector<float> syntheticKickOnsets(int phaseMs, int beats = 16)
{
    constexpr double beatMs = 468.75; // 128 BPM
    std::vector<float> onset(8100, 0.002f);
    for (int beat = 1; beat < beats; ++beat)
    {
        const auto position = static_cast<int>(std::llround(beat * beatMs)) + phaseMs;
        for (int offset = 0; offset < 6; ++offset)
            onset[static_cast<size_t>(position + offset)] += 1.0f / (offset + 1);
    }
    return onset;
}

int main()
{
    const auto late = estimateBeatOffsetMs(syntheticKickOnsets(23), 468.75);
    assert(late.confident && late.beatOffsetMs == 23);

    const auto early = estimateBeatOffsetMs(syntheticKickOnsets(-18), 468.75);
    assert(early.confident && early.beatOffsetMs == -18);

    std::vector<float> ambience(8100, 0.01f);
    const auto noBeats = estimateBeatOffsetMs(ambience, 468.75);
    assert(!noBeats.confident);

    auto isolatedHit = ambience;
    isolatedHit[static_cast<size_t>(std::llround(5 * 468.75) + 23)] = 20.0f;
    const auto falseHit = estimateBeatOffsetMs(isolatedHit, 468.75);
    assert(!falseHit.confident);

    // Test the full envelope -> attack -> phase pipeline on a synthetic
    // four-on-the-floor kick track with attacks arriving 23 ms late.
    std::vector<float> waveformEnvelope(8100, 0.04f);
    for (int beat = 1; beat < 16; ++beat)
    {
        const auto start = static_cast<int>(std::llround(beat * 468.75)) + 23;
        for (int sample = 0; sample < 35; ++sample)
            waveformEnvelope[static_cast<size_t>(start + sample)] +=
                0.9f * std::exp(-sample / 12.0f);
    }
    const auto detectedWaveform = estimateBeatOffsetMs(makeRiseOnset(waveformEnvelope), 468.75);
    assert(detectedWaveform.confident && detectedWaveform.beatOffsetMs == 23);

    assert(!estimateBeatOffsetMs(syntheticKickOnsets(23), 0.0).confident);

    // Exact-length circular edits mirror Ableton Track Delay polarity.
    assert(wrappedReadOffset(0, 10, -2) == 2);  // 2 frames early
    assert(wrappedReadOffset(0, 10, 2) == 8);   // 2 frames late
    assert(wrappedReadOffset(8, 10, -2) == 0);  // wraps to beginning
}
