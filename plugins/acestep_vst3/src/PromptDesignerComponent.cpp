#include "PromptDesignerComponent.h"

#include "V2Chrome.h"

namespace acestep::vst3
{
PromptDesignerComponent::PromptDesignerComponent()
{
    for (auto* label : {&sourceLabel_, &hintLabel_, &styleLabel_, &moodLabel_,
                        &drumsLabel_, &bassLabel_, &leadLabel_, &textureLabel_,
                        &energyLabel_, &vocalsLabel_})
    {
        label->setColour(juce::Label::textColourId, v2::kLabelMuted);
        addAndMakeVisible(*label);
    }
    sourceLabel_.setText("Prompt Mode", juce::dontSendNotification);
    hintLabel_.setText("Build a prompt with no typing; preview it in Compose / Engine.",
                       juce::dontSendNotification);
    styleLabel_.setText("Style", juce::dontSendNotification);
    moodLabel_.setText("Mood", juce::dontSendNotification);
    drumsLabel_.setText("Drums", juce::dontSendNotification);
    bassLabel_.setText("Bass", juce::dontSendNotification);
    leadLabel_.setText("Lead Instrument", juce::dontSendNotification);
    textureLabel_.setText("Texture / Atmosphere", juce::dontSendNotification);
    energyLabel_.setText("Energy", juce::dontSendNotification);
    vocalsLabel_.setText("Voice", juce::dontSendNotification);

    for (auto* selector : {&promptSourceBox_, &styleBox_, &moodBox_, &drumsBox_,
                           &bassBox_, &leadBox_, &textureBox_, &energyBox_, &vocalsBox_})
    {
        addAndMakeVisible(*selector);
    }
}

void PromptDesignerComponent::paint(juce::Graphics& g)
{
    v2::drawModule(g, getLocalBounds(), "Prompt Designer", v2::kAccentBlue);
}

void PromptDesignerComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    area.removeFromTop(24);
    auto modeRow = area.removeFromTop(32);
    sourceLabel_.setBounds(modeRow.removeFromLeft(110));
    promptSourceBox_.setBounds(modeRow.removeFromLeft(244));
    modeRow.removeFromLeft(16);
    hintLabel_.setBounds(modeRow);
    area.removeFromTop(12);

    const auto placeRow = [&area](juce::Label& firstLabel, juce::ComboBox& firstBox,
                                  juce::Label& secondLabel, juce::ComboBox& secondBox,
                                  juce::Label& thirdLabel, juce::ComboBox& thirdBox,
                                  juce::Label& fourthLabel, juce::ComboBox& fourthBox)
    {
        constexpr int spacing = 14;
        const int width = (area.getWidth() - 3 * spacing) / 4;
        auto labels = area.removeFromTop(18);
        auto fields = area.removeFromTop(30);
        const auto placeColumn = [&](juce::Label& label, juce::ComboBox& box)
        {
            label.setBounds(labels.removeFromLeft(width));
            box.setBounds(fields.removeFromLeft(width));
            labels.removeFromLeft(spacing);
            fields.removeFromLeft(spacing);
        };
        placeColumn(firstLabel, firstBox);
        placeColumn(secondLabel, secondBox);
        placeColumn(thirdLabel, thirdBox);
        placeColumn(fourthLabel, fourthBox);
        area.removeFromTop(8);
    };
    placeRow(styleLabel_, styleBox_, moodLabel_, moodBox_, drumsLabel_, drumsBox_,
             bassLabel_, bassBox_);
    placeRow(leadLabel_, leadBox_, textureLabel_, textureBox_, energyLabel_, energyBox_,
             vocalsLabel_, vocalsBox_);
}

void PromptDesignerComponent::setSelectorsEnabled(bool enabled)
{
    hintLabel_.setText(enabled ? "Choose parameters; the generated prompt updates live."
                               : "Free Text mode: edit the prompt under Compose / Engine.",
                       juce::dontSendNotification);
    for (auto* selector : {&styleBox_, &moodBox_, &drumsBox_, &bassBox_,
                           &leadBox_, &textureBox_, &energyBox_, &vocalsBox_})
        selector->setEnabled(enabled);
}
}  // namespace acestep::vst3
