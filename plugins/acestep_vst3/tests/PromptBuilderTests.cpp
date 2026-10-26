#include "../src/PromptBuilder.h"

#include <cassert>
#include <string>

using namespace acestep::vst3;

int main()
{
    const auto defaults = promptbuilder::build({});
    assert(defaults == "electro music, instrumental, no vocals");

    promptbuilder::Selection selections;
    selections.style = 4;   // Melodic techno
    selections.mood = 2;    // Dark
    selections.drums = 2;   // 4-on-the-floor
    selections.bass = 3;    // Acid 303
    selections.lead = 2;    // Arpeggiator
    selections.texture = 3; // Spacey pads
    selections.energy = 5;  // Peak time
    selections.vocals = 5;  // Vocal chops
    const auto generated = promptbuilder::build(selections);
    for (const auto* word : {"melodic techno", "dark and tense", "four-on-the-floor",
                             "acid 303", "arpeggiated", "spacious atmospheric",
                             "peak-time", "chopped vocal samples"})
        assert(generated.find(word) != std::string::npos);
    assert(generated.find("no vocals") == std::string::npos);
    assert(generated.find("BPM") == std::string::npos); // DAW BPM is sent separately.

    selections.mood = 0;    // Invalid saved choice falls back to Any.
    selections.drums = 999; // Invalid saved choice falls back to Any.
    const auto robust = promptbuilder::build(selections);
    assert(robust.find("dark and tense") == std::string::npos);
    assert(robust.find("four-on-the-floor") == std::string::npos);
    assert(robust.find("melodic techno") != std::string::npos);

    selections.bass = 8;
    selections.drums = 8;
    assert(promptbuilder::build(selections).find("no bassline") != std::string::npos);
    assert(promptbuilder::build(selections).find("no drums or percussion") != std::string::npos);
}
