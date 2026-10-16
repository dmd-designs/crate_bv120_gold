#pragma once

#include "PluginProcessor.h"

class GoldLookAndFeel;

class BV120Editor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit BV120Editor (BV120Processor&);
    ~BV120Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void configureKnob (Knob& knob, const juce::String& name, const juce::String& paramId, const juce::String& tip, bool asDecibels);

    BV120Processor& processor;
    std::unique_ptr<GoldLookAndFeel> lookAndFeel;

    Knob presence, input, gate, output;
    Knob ch1Bass, ch1Mid, ch1Treble, ch1Level, ch1Reverb;
    Knob ch2Gain, ch2Bass, ch2Mid, ch2Treble, ch2Level, ch2Reverb;
    Knob mic, width;

    juce::ToggleButton channelButton;
    juce::ToggleButton cabButton;
    juce::ToggleButton slantButton;
    std::unique_ptr<ButtonAttachment> channelAttachment;
    std::unique_ptr<ButtonAttachment> cabAttachment;
    std::unique_ptr<ButtonAttachment> slantAttachment;

    juce::ComboBox presets;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> panelBounds;
    juce::Rectangle<int> ch1Bounds;
    juce::Rectangle<int> ch2Bounds;
    juce::Rectangle<int> cabBounds;
    juce::Rectangle<int> lampBounds;
    float lamp = 0.f;
    bool suppressPreset = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BV120Editor)
};
