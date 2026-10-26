#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace acestep::vst3::promptbuilder
{
struct Choice
{
    const char* label;
    const char* phrase;
};

// IDs are 1-based, matching JUCE ComboBox IDs. The first entry in optional
// categories is "Any", which leaves that category out of the prompt.
inline constexpr Choice styles[] = {
    {"Electro", "electro music"}, {"Techno", "techno music"},
    {"House", "house music"}, {"Melodic Techno", "melodic techno"},
    {"Synthwave", "synthwave"}, {"Trance", "trance music"},
    {"Drum & Bass", "drum and bass"}, {"Breakbeat", "breakbeat electronic music"},
    {"Ambient", "ambient electronic music"}, {"Downtempo", "downtempo electronic music"}};

inline constexpr Choice moods[] = {
    {"Any", ""}, {"Dark", "dark and tense atmosphere"},
    {"Euphoric", "euphoric atmosphere"}, {"Dreamy", "dreamy atmosphere"},
    {"Melancholic", "melancholic atmosphere"}, {"Hypnotic", "hypnotic atmosphere"},
    {"Cinematic", "cinematic atmosphere"}, {"Uplifting", "uplifting mood"},
    {"Mysterious", "mysterious atmosphere"}};

inline constexpr Choice drums[] = {
    {"Any", ""}, {"4-on-the-floor", "steady four-on-the-floor kick drum"},
    {"Punchy Electro", "punchy electronic drums and crisp claps"},
    {"Driving Techno", "driving techno kick and tight hi-hats"},
    {"Breakbeat", "syncopated breakbeat drums"},
    {"Minimal", "minimal drum machine groove"},
    {"Half-time", "heavy half-time drum groove"},
    {"No Drums", "no drums or percussion"}};

inline constexpr Choice basses[] = {
    {"Any", ""}, {"Analog Synth", "warm analog synth bassline"},
    {"Acid 303", "resonant acid 303 bassline"},
    {"Deep Sub", "deep sub bass"}, {"Reese", "wide reese bass"},
    {"Funky", "funky syncopated bassline"},
    {"Distorted", "distorted aggressive synth bass"}, {"No Bass", "no bassline"}};

inline constexpr Choice leads[] = {
    {"Any", ""}, {"Arpeggiator", "arpeggiated synthesizer melody"},
    {"Synth Pluck", "bright plucked synth lead"},
    {"Supersaw", "wide supersaw synth lead"},
    {"Analog Mono", "expressive analog mono synth lead"},
    {"Piano", "melodic piano phrases"},
    {"Electric Guitar", "electric guitar melody"},
    {"No Lead", "no prominent lead melody"}};

inline constexpr Choice textures[] = {
    {"Any", ""}, {"Warm Pads", "warm sustained synth pads"},
    {"Spacey Pads", "spacious atmospheric pads"},
    {"Analog", "vintage analog synth textures"},
    {"Glitch", "glitchy digital textures"},
    {"Percussive", "metallic percussion accents"},
    {"Orchestral", "cinematic string layers"},
    {"Minimal", "sparse minimal arrangement"}};

inline constexpr Choice energies[] = {
    {"Any", ""}, {"Chill", "low energy and relaxed dynamics"},
    {"Groovy", "groovy and danceable energy"},
    {"Driving", "driving, energetic groove"},
    {"Peak Time", "high-energy peak-time dancefloor sound"},
    {"Intense", "intense and powerful dynamics"}};

inline constexpr Choice vocals[] = {
    {"Instrumental", "instrumental, no vocals"},
    {"Any", ""}, {"Female Voice", "female sung vocals"},
    {"Male Voice", "male sung vocals"},
    {"Vocal Chops", "chopped vocal samples"},
    {"Choir", "ethereal choir vocals"},
    {"Spoken Word", "spoken-word vocals"}};

struct Selection
{
    int style = 1;
    int mood = 1;
    int drums = 1;
    int bass = 1;
    int lead = 1;
    int texture = 1;
    int energy = 1;
    int vocals = 1;
};

template <std::size_t N>
constexpr int validId(const Choice (&)[N], int id, int fallback = 1)
{
    return id >= 1 && id <= static_cast<int>(N) ? id : fallback;
}

template <std::size_t N>
constexpr std::string_view phrase(const Choice (&choices)[N], int id)
{
    return choices[static_cast<std::size_t>(validId(choices, id) - 1)].phrase;
}

inline std::string build(const Selection& selection)
{
    std::vector<std::string_view> parts;
    parts.reserve(8);
    for (const auto part : {phrase(styles, selection.style), phrase(moods, selection.mood),
                            phrase(drums, selection.drums), phrase(basses, selection.bass),
                            phrase(leads, selection.lead), phrase(textures, selection.texture),
                            phrase(energies, selection.energy), phrase(vocals, selection.vocals)})
    {
        if (!part.empty())
            parts.push_back(part);
    }

    std::string result;
    for (const auto part : parts)
    {
        if (!result.empty())
            result += ", ";
        result += part;
    }
    return result;
}
}  // namespace acestep::vst3::promptbuilder
