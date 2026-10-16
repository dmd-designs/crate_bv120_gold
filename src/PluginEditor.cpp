#include "PluginEditor.h"
#include "dsp/Presets.h"

namespace
{
const juce::Colour kTolex = juce::Colour (0xff0b3d86);
const juce::Colour kTolexDeep = juce::Colour (0xff062652);
const juce::Colour kPanel = juce::Colour (0xff141414);
const juce::Colour kPanelInset = juce::Colour (0xff1c1c1c);
const juce::Colour kGold = juce::Colour (0xffd4b25c);
const juce::Colour kGoldBright = juce::Colour (0xfff3d98a);
const juce::Colour kAmber = juce::Colour (0xffffb423);
const juce::Colour kRed = juce::Colour (0xffff3b30);
const juce::Colour kSilver = juce::Colour (0xffd5dbe3);

juce::Font goldFont (float height, bool bold)
{
    auto options = juce::FontOptions (height);
    if (bold)
        options = options.withStyle ("Bold");
    return juce::Font (options);
}
} // namespace

class GoldLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoldLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff101010));
        setColour (juce::ComboBox::textColourId, kGoldBright);
        setColour (juce::ComboBox::outlineColourId, kGold);
        setColour (juce::ComboBox::arrowColourId, kGold);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff161616));
        setColour (juce::PopupMenu::textColourId, kGoldBright);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff1d4e8f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Label::textColourId, kGold);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff101010));
        setColour (juce::TooltipWindow::textColourId, kGoldBright);
        setColour (juce::TooltipWindow::outlineColourId, kGold);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                              static_cast<float> (width), static_cast<float> (height))
                          .reduced (6.f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);
        const float arcR = radius - 1.f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2c2c2c));
        g.strokePath (track, juce::PathStrokeType (3.2f));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, angle, true);
        g.setColour (kGold);
        g.strokePath (value, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float knobR = radius - 10.f;
        g.setColour (kSilver);
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.f, knobR * 2.f);
        g.setColour (juce::Colour (0xff101010));
        g.fillEllipse (centre.x - knobR + 3.f, centre.y - knobR + 3.f, (knobR - 3.f) * 2.f, (knobR - 3.f) * 2.f);
        g.setColour (kGold.withAlpha (0.9f));
        g.drawEllipse (centre.x - knobR + 3.f, centre.y - knobR + 3.f, (knobR - 3.f) * 2.f, (knobR - 3.f) * 2.f, 1.2f);

        const auto pointer = juce::Point<float> (0.f, -(knobR - 8.f)).rotatedAboutOrigin (angle) + centre;
        g.setColour (kGoldBright);
        g.drawLine (centre.x, centre.y, pointer.x, pointer.y, 2.3f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.f);
        const bool on = button.getToggleState();
        const auto id = button.getComponentID();

        if (id == "channel")
        {
            g.setColour (juce::Colour (0xff0c0c0c));
            g.fillRoundedRectangle (bounds, 8.f);
            g.setColour (on ? kRed : kAmber);
            g.drawRoundedRectangle (bounds.reduced (1.f), 8.f, highlighted || down ? 2.5f : 1.5f);

            auto led = bounds.removeFromTop (bounds.getHeight() * 0.42f).reduced (16.f, 8.f);
            g.setColour ((on ? kRed : kAmber).withAlpha (on || ! on ? 0.95f : 0.4f));
            g.fillEllipse (led.withSizeKeepingCentre (18.f, 18.f));

            g.setColour (kGoldBright);
            g.setFont (goldFont (15.f, true));
            g.drawFittedText (on ? "CHANNEL 2" : "CHANNEL 1", button.getLocalBounds().reduced (6, 4).withTrimmedTop (28),
                              juce::Justification::centred, 2);
            return;
        }

        g.setColour (on ? kGold.withAlpha (0.22f) : juce::Colour (0xff101010));
        g.fillRoundedRectangle (bounds, 5.f);
        g.setColour (on ? kGold : juce::Colour (0xff6a5a32));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.f, highlighted ? 2.f : 1.2f);
        g.setColour (on ? kGoldBright : juce::Colour (0xffb7aa86));
        g.setFont (goldFont (13.f, true));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
    }
};

BV120Editor::BV120Editor (BV120Processor& owner)
    : AudioProcessorEditor (owner), processor (owner)
{
    lookAndFeel = std::make_unique<GoldLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    configureKnob (presence, "PRESENCE", "presence", "Power-amp presence. Boost only, up to +5 dB at 10 kHz.", false);
    configureKnob (ch1Bass, "BASS", "ch1Bass", "Channel 1 bass. 16 dB range at 100 Hz. Noon is flat.", false);
    configureKnob (ch1Mid, "MID", "ch1Mid", "Channel 1 mid. 20 dB range at 300 Hz. Noon is flat.", false);
    configureKnob (ch1Treble, "TREBLE", "ch1Treble", "Channel 1 treble. 10 dB range at 10 kHz. Noon is flat.", false);
    configureKnob (ch1Level, "LEVEL", "ch1Level", "Channel 1 level into the 6L6 power stage.", false);
    configureKnob (ch1Reverb, "REVERB", "ch1Reverb", "Channel 1 spring reverb. Zero is dry.", false);

    configureKnob (ch2Gain, "GAIN", "ch2Gain", "Channel 2 gain. Low is almost clean, noon is already breaking up, 10 is saturated.", false);
    configureKnob (ch2Bass, "BASS", "ch2Bass", "Channel 2 bass. 15 dB range at 100 Hz. Sits after the clippers.", false);
    configureKnob (ch2Mid, "MID", "ch2Mid", "Channel 2 mid. 13 dB range at 500 Hz. This is the voice control.", false);
    configureKnob (ch2Treble, "TREBLE", "ch2Treble", "Channel 2 treble. 10 dB range at 10 kHz.", false);
    configureKnob (ch2Level, "LEVEL", "ch2Level", "Channel 2 level into the 6L6 power stage.", false);
    configureKnob (ch2Reverb, "REVERB", "ch2Reverb", "Channel 2 spring reverb. Zero is dry.", false);

    configureKnob (input, "INPUT", "input", "Interface trim, +/-12 dB. Noon matches a typical instrument input.", true);
    configureKnob (gate, "GATE", "gate", "Noise gate keyed from the input. Zero is off.", false);
    configureKnob (output, "OUTPUT", "output", "Plugin output trim, +/-12 dB. Noon is unity.", true);
    configureKnob (mic, "MIC", "mic", "Vintage 30 mic position. Left is dust-cap center, right is the cone edge.", false);
    configureKnob (width, "WIDTH", "width", "Stereo mic offset. Mono sum stays the same cab.", false);

    channelButton.setComponentID ("channel");
    channelButton.setClickingTogglesState (true);
    channelButton.setTooltip ("Footswitch between the rhythm channel and the lead channel.");
    addAndMakeVisible (channelButton);
    channelAttachment = std::make_unique<ButtonAttachment> (processor.getApvts(), "channel", channelButton);

    cabButton.setButtonText ("CAB");
    cabButton.setComponentID ("cab");
    cabButton.setClickingTogglesState (true);
    cabButton.setTooltip ("Oversized sealed 4x12 with Celestion Vintage 30s. Off is the head only.");
    addAndMakeVisible (cabButton);
    cabAttachment = std::make_unique<ButtonAttachment> (processor.getApvts(), "cab", cabButton);

    slantButton.setButtonText ("SLANT");
    slantButton.setComponentID ("slant");
    slantButton.setClickingTogglesState (true);
    slantButton.setTooltip ("Off is the straight oversized cab. On aims the top speakers and tightens the low end a little.");
    addAndMakeVisible (slantButton);
    slantAttachment = std::make_unique<ButtonAttachment> (processor.getApvts(), "slant", slantButton);

    for (int i = 0; i < bv::kNumPatches; ++i)
        presets.addItem (bv::kPatches[i].name, i + 1);

    presets.setSelectedItemIndex (processor.getCurrentProgram(), juce::dontSendNotification);
    presets.setTooltip ("Suggested settings from the 120H manual.");
    presets.onChange = [this] {
        if (suppressPreset)
            return;
        processor.setCurrentProgram (presets.getSelectedItemIndex());
    };
    addAndMakeVisible (presets);

    setSize (1360, 760);
    setResizable (false, false);
    startTimerHz (30);
}

BV120Editor::~BV120Editor()
{
    setLookAndFeel (nullptr);
    for (auto* knob : { &presence, &input, &gate, &output, &ch1Bass, &ch1Mid, &ch1Treble, &ch1Level, &ch1Reverb,
                        &ch2Gain, &ch2Bass, &ch2Mid, &ch2Treble, &ch2Level, &ch2Reverb, &mic, &width })
        knob->slider.setLookAndFeel (nullptr);
}

void BV120Editor::configureKnob (Knob& knob, const juce::String& name, const juce::String& paramId,
                                 const juce::String& tip, bool asDecibels)
{
    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (goldFont (12.5f, true));
    knob.label.setColour (juce::Label::textColourId, kGold);
    knob.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (knob.label);

    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                     juce::MathConstants<float>::pi * 2.8f,
                                     true);
    knob.slider.setPopupDisplayEnabled (true, false, this);
    knob.slider.setTooltip (tip);
    knob.slider.setLookAndFeel (lookAndFeel.get());

    if (asDecibels)
    {
        knob.slider.textFromValueFunction = [] (double value) {
            return juce::String ((value - 0.5) * 24.0, 1) + " dB";
        };
        knob.slider.valueFromTextFunction = [] (const juce::String& text) {
            return text.getDoubleValue() / 24.0 + 0.5;
        };
    }
    else if (paramId == "gate")
    {
        knob.slider.textFromValueFunction = [] (double value) {
            return value < 0.01 ? juce::String ("off") : juce::String (value * 10.0, 1);
        };
        knob.slider.valueFromTextFunction = [] (const juce::String& text) {
            return text.getDoubleValue() / 10.0;
        };
    }
    else
    {
        knob.slider.textFromValueFunction = [] (double value) { return juce::String (value * 10.0, 1); };
        knob.slider.valueFromTextFunction = [] (const juce::String& text) {
            return text.getDoubleValue() / 10.0;
        };
    }

    addAndMakeVisible (knob.slider);
    knob.attachment = std::make_unique<SliderAttachment> (processor.getApvts(), paramId, knob.slider);
}

void BV120Editor::paint (juce::Graphics& g)
{
    juce::ColourGradient tolex (kTolexDeep, 0.f, 0.f, kTolex, 0.f, static_cast<float> (getHeight()), false);
    tolex.addColour (0.45, juce::Colour (0xff1457ad));
    g.setGradientFill (tolex);
    g.fillAll();

    g.setColour (kGold.withAlpha (0.35f));
    g.drawRect (getLocalBounds(), 3);

    g.setColour (kPanel);
    g.fillRoundedRectangle (panelBounds.toFloat(), 10.f);
    g.setColour (kGold);
    g.drawRoundedRectangle (panelBounds.toFloat().reduced (1.f), 10.f, 2.f);
    g.setColour (kGold.withAlpha (0.45f));
    g.drawRoundedRectangle (panelBounds.toFloat().reduced (7.f), 7.f, 1.f);

    auto screw = [&g] (float x, float y) {
        g.setColour (kGold);
        g.fillEllipse (x, y, 12.f, 12.f);
        g.setColour (kTolexDeep);
        g.drawLine (x + 2.f, y + 6.f, x + 10.f, y + 6.f, 1.2f);
    };
    screw (panelBounds.getX() + 14.f, panelBounds.getY() + 14.f);
    screw (panelBounds.getRight() - 26.f, panelBounds.getY() + 14.f);
    screw (panelBounds.getX() + 14.f, panelBounds.getBottom() - 26.f);
    screw (panelBounds.getRight() - 26.f, panelBounds.getBottom() - 26.f);

    g.setColour (kGoldBright);
    g.setFont (goldFont (42.f, true));
    g.drawText ("BV-120 GOLD", panelBounds.getX() + 36, panelBounds.getY() + 18, 460, 46, juce::Justification::left);

    g.setColour (kGold);
    g.setFont (goldFont (14.f, false));
    g.drawText ("BLUE VOODOO 120H  ·  GOLD COSMETICS  ·  4x12AX7  ·  4x6L6",
                panelBounds.getX() + 40, panelBounds.getY() + 62, 640, 20, juce::Justification::left);

    auto drawWell = [&g] (juce::Rectangle<int> area, const juce::String& title, juce::Colour led, bool lit) {
        g.setColour (kPanelInset);
        g.fillRoundedRectangle (area.toFloat(), 8.f);
        g.setColour (led.withAlpha (lit ? 0.85f : 0.28f));
        g.drawRoundedRectangle (area.toFloat(), 8.f, 1.4f);
        g.setColour (lit ? led : led.withAlpha (0.45f));
        g.fillEllipse (static_cast<float> (area.getX() + 14), static_cast<float> (area.getY() + 12), 10.f, 10.f);
        g.setColour (kGoldBright);
        g.setFont (goldFont (13.f, true));
        g.drawText (title, area.getX() + 30, area.getY() + 6, area.getWidth() - 40, 20, juce::Justification::left);
    };

    const bool lead = processor.getApvts().getRawParameterValue ("channel")->load() >= 0.5f;
    drawWell (ch1Bounds, "CHANNEL 1   RHYTHM", kAmber, ! lead);
    drawWell (ch2Bounds, "CHANNEL 2   LEAD", kRed, lead);
    drawWell (cabBounds, "OVERSIZED 4x12   CELESTION VINTAGE 30", kGold, processor.getApvts().getRawParameterValue ("cab")->load() >= 0.5f);

    g.setColour (juce::Colour (0xffc8c8c8));
    g.setFont (goldFont (12.5f, false));
    g.drawText ("Sealed straight cab unless Slant is on. Mic travels from cap center to cone edge.",
                cabBounds.getX() + 16, cabBounds.getBottom() - 28, cabBounds.getWidth() - 32, 18, juce::Justification::left);

    g.setColour (kGold.withAlpha (0.8f));
    g.setFont (goldFont (11.5f, false));
    g.drawText ("Unofficial recreation. Not affiliated with Crate, Mesa/Boogie, or Celestion.",
                24, getHeight() - 28, getWidth() - 48, 16, juce::Justification::centred);

    const float level = juce::jlimit (0.f, 1.f, lamp);
    g.setColour (juce::Colour (0xff101010));
    g.fillEllipse (lampBounds.toFloat());
    juce::Colour glow = level > 0.92f ? kRed : (level > 0.45f ? kGold : juce::Colour (0xff7dba6a));
    g.setColour (glow.withAlpha (0.25f + 0.75f * level));
    g.fillEllipse (lampBounds.toFloat().reduced (3.f));
}

void BV120Editor::resized()
{
    panelBounds = getLocalBounds().reduced (16, 14).withTrimmedBottom (18);

    presets.setBounds (panelBounds.getRight() - 280, panelBounds.getY() + 36, 240, 28);

    ch1Bounds = { panelBounds.getX() + 28, panelBounds.getY() + 108, 548, 300 };
    ch2Bounds = { panelBounds.getRight() - 576, panelBounds.getY() + 108, 548, 300 };

    const int gapX = ch1Bounds.getRight() + 8;
    const int gapW = ch2Bounds.getX() - gapX;
    channelButton.setBounds (gapX + (gapW - 120) / 2, ch1Bounds.getY() + 110, 120, 84);

    auto layoutKnobs = [] (Knob** knobs, int count, juce::Rectangle<int> row) {
        const int slot = row.getWidth() / std::max (1, count);
        for (int i = 0; i < count; ++i)
        {
            auto cell = row.withX (row.getX() + i * slot).withWidth (slot);
            knobs[i]->label.setBounds (cell.removeFromBottom (16));
            const int side = juce::jmin (cell.getWidth() - 2, cell.getHeight());
            knobs[i]->slider.setBounds (cell.withSizeKeepingCentre (side, side));
        }
    };

    Knob* rhythm[] = { &ch1Bass, &ch1Mid, &ch1Treble, &ch1Level, &ch1Reverb };
    Knob* lead[] = { &ch2Gain, &ch2Bass, &ch2Mid, &ch2Treble, &ch2Level, &ch2Reverb };
    layoutKnobs (rhythm, 5, ch1Bounds.reduced (8, 30).withTrimmedBottom (6));
    layoutKnobs (lead, 6, ch2Bounds.reduced (6, 30).withTrimmedBottom (6));

    auto lower = juce::Rectangle<int> (panelBounds.getX() + 28, ch1Bounds.getBottom() + 14, panelBounds.getWidth() - 56, 176);
    auto utility = lower.removeFromLeft (430);
    cabBounds = lower.withTrimmedLeft (12);

    Knob* utils[] = { &presence, &input, &gate, &output };
    layoutKnobs (utils, 4, utility.reduced (4, 10));
    lampBounds = { output.slider.getBounds().getCentreX() - 8, utility.getY() + 8, 16, 16 };

    auto cabInner = cabBounds.reduced (12, 32).withTrimmedBottom (22);
    auto buttons = cabInner.removeFromRight (170);
    Knob* cabControls[] = { &mic, &width };
    layoutKnobs (cabControls, 2, cabInner);
    cabButton.setBounds (buttons.getX() + 10, buttons.getY() + 16, 148, 34);
    slantButton.setBounds (buttons.getX() + 10, buttons.getY() + 60, 148, 34);
}

void BV120Editor::timerCallback()
{
    const int program = processor.getCurrentProgram();
    if (program != presets.getSelectedItemIndex() && ! presets.isPopupActive())
    {
        suppressPreset = true;
        presets.setSelectedItemIndex (program, juce::dontSendNotification);
        suppressPreset = false;
    }

    lamp = processor.getOutputLevel();
    repaint();
}
