#pragma once

#include <JuceHeader.h>

namespace acestep::vst3
{
class PromptDesignerComponent final : public juce::Component
{
public:
    PromptDesignerComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setSelectorsEnabled(bool enabled);

    juce::ComboBox& promptSourceBox() noexcept { return promptSourceBox_; }
    juce::ComboBox& styleBox() noexcept { return styleBox_; }
    juce::ComboBox& moodBox() noexcept { return moodBox_; }
    juce::ComboBox& drumsBox() noexcept { return drumsBox_; }
    juce::ComboBox& bassBox() noexcept { return bassBox_; }
    juce::ComboBox& leadBox() noexcept { return leadBox_; }
    juce::ComboBox& textureBox() noexcept { return textureBox_; }
    juce::ComboBox& energyBox() noexcept { return energyBox_; }
    juce::ComboBox& vocalsBox() noexcept { return vocalsBox_; }

private:
    juce::Label sourceLabel_;
    juce::Label hintLabel_;
    juce::Label styleLabel_;
    juce::Label moodLabel_;
    juce::Label drumsLabel_;
    juce::Label bassLabel_;
    juce::Label leadLabel_;
    juce::Label textureLabel_;
    juce::Label energyLabel_;
    juce::Label vocalsLabel_;
    juce::ComboBox promptSourceBox_;
    juce::ComboBox styleBox_;
    juce::ComboBox moodBox_;
    juce::ComboBox drumsBox_;
    juce::ComboBox bassBox_;
    juce::ComboBox leadBox_;
    juce::ComboBox textureBox_;
    juce::ComboBox energyBox_;
    juce::ComboBox vocalsBox_;
};
}  // namespace acestep::vst3
