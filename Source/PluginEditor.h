#pragma once
#include "PluginProcessor.h"
#include "KitsuneLookAndFeel.h"
#include "SpectrumView.h"

class KitsuneEditor : public juce::AudioProcessorEditor
{
public:
    explicit KitsuneEditor (KitsuneProcessor&);
    ~KitsuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attach;
    };

    Knob& addKnob (const char* id, const juce::String& title);
    void layoutKnob (Knob& k, juce::Rectangle<int> area);

    KitsuneProcessor& proc;
    KitsuneLookAndFeel lnf;
    SpectrumView spectrum;

    Knob depth, trigger, focus, sharpness, tilt, guard, attack, release, lowHz, highHz, link, mix, output;
    juce::ComboBox modeBox, stereoBox, presetBox;
    juce::ToggleButton deltaButton { "DELTA" }, bypassButton { "BYPASS" };
    juce::Label modeLabel, stereoLabel;
    std::unique_ptr<ComboAttachment> modeAttach, stereoAttach;
    std::unique_ptr<ButtonAttachment> deltaAttach, bypassAttach;
    juce::Image logo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitsuneEditor)
};
