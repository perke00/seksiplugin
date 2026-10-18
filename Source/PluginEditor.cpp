#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
constexpr int kBaseW = 980, kBaseH = 640;
}

KitsuneEditor::Knob& KitsuneEditor::addKnob (const char* id, const juce::String& title)
{
    Knob* k = nullptr;
    if (juce::String (id) == ids::depth) k = &depth;
    else if (juce::String (id) == ids::trigger) k = &trigger;
    else if (juce::String (id) == ids::focus) k = &focus;
    else if (juce::String (id) == ids::sharpness) k = &sharpness;
    else if (juce::String (id) == ids::tilt) k = &tilt;
    else if (juce::String (id) == ids::guard) k = &guard;
    else if (juce::String (id) == ids::attack) k = &attack;
    else if (juce::String (id) == ids::release) k = &release;
    else if (juce::String (id) == ids::lowHz) k = &lowHz;
    else if (juce::String (id) == ids::highHz) k = &highHz;
    else if (juce::String (id) == ids::link) k = &link;
    else if (juce::String (id) == ids::mix) k = &mix;
    else k = &output;

    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
    k->slider.setScrollWheelEnabled (true);
    if (auto* p = proc.apvts.getParameter (id))
        k->slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    k->label.setText (title, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::Font (11.f, juce::Font::bold));
    k->label.setColour (juce::Label::textColourId, kcol::lilac);

    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attach = std::make_unique<SliderAttachment> (proc.apvts, id, k->slider);
    return *k;
}

KitsuneEditor::KitsuneEditor (KitsuneProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), spectrum (p.getEngine(), p.apvts)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BinaryData::fox_png, BinaryData::fox_pngSize);

    addAndMakeVisible (spectrum);

    addKnob (ids::depth, "DEPTH");
    addKnob (ids::trigger, "TRIGGER");
    addKnob (ids::focus, "FOCUS");
    addKnob (ids::sharpness, "SHARPNESS");
    addKnob (ids::tilt, "TILT");
    addKnob (ids::guard, "TRANSIENT GUARD");
    addKnob (ids::attack, "ATTACK");
    addKnob (ids::release, "RELEASE");
    addKnob (ids::lowHz, "RANGE LOW");
    addKnob (ids::highHz, "RANGE HIGH");
    addKnob (ids::link, "STEREO LINK");
    addKnob (ids::mix, "MIX");
    addKnob (ids::output, "OUTPUT");

    modeBox.addItemList ({ "Soft", "Hard" }, 1);
    stereoBox.addItemList ({ "Left / Right", "Mid / Side" }, 1);
    addAndMakeVisible (modeBox);
    addAndMakeVisible (stereoBox);
    modeAttach = std::make_unique<ComboAttachment> (proc.apvts, ids::mode, modeBox);
    stereoAttach = std::make_unique<ComboAttachment> (proc.apvts, ids::stereo, stereoBox);

    for (auto* l : { &modeLabel, &stereoLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::Font (11.f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, kcol::lilac);
        addAndMakeVisible (*l);
    }
    modeLabel.setText ("MODE", juce::dontSendNotification);
    stereoLabel.setText ("STEREO", juce::dontSendNotification);

    addAndMakeVisible (deltaButton);
    addAndMakeVisible (bypassButton);
    deltaAttach = std::make_unique<ButtonAttachment> (proc.apvts, ids::delta, deltaButton);
    bypassAttach = std::make_unique<ButtonAttachment> (proc.apvts, ids::bypass, bypassButton);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setResizable (true, true);
    setResizeLimits (kBaseW * 3 / 4, kBaseH * 3 / 4, kBaseW * 3 / 2, kBaseH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setSize (kBaseW, kBaseH);
}

KitsuneEditor::~KitsuneEditor() { setLookAndFeel (nullptr); }

void KitsuneEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float u = b.getWidth() / (float) kBaseW;

    // background
    g.setGradientFill (juce::ColourGradient (kcol::bgTop, 0.f, 0.f, kcol::bgBottom, 0.f, b.getHeight(), false));
    g.fillAll();

    // retro sun glow behind the header
    g.setGradientFill (juce::ColourGradient (kcol::pink.withAlpha (0.22f), b.getWidth() * 0.5f, 0.f, juce::Colours::transparentBlack, b.getWidth() * 0.5f, 260.f * u, true));
    g.fillRect (b.withHeight (300.f * u));

    // perspective grid in the control area
    const float gy = 372.f * u;
    const float vx = b.getWidth() * 0.5f, vy = gy - 30.f * u;
    g.setColour (kcol::purple.withAlpha (0.16f));
    for (int i = -14; i <= 14; ++i)
        g.drawLine (vx, vy, vx + (float) i * b.getWidth() * 0.12f, b.getBottom(), 1.f);
    for (int i = 1; i < 9; ++i)
    {
        const float t = (float) i / 9.f, y = vy + (b.getBottom() - vy) * t * t;
        g.drawHorizontalLine ((int) y, 0.f, b.getRight());
    }

    // header: logo + title
    const auto logoArea = juce::Rectangle<float> (22.f * u, 10.f * u, 62.f * u, 62.f * u);
    if (logo.isValid())
    {
        g.setColour (kcol::pink.withAlpha (0.16f));
        g.fillEllipse (logoArea.expanded (8.f * u));
        g.drawImage (logo, logoArea, juce::RectanglePlacement::centred);
    }

    juce::Font title (38.f * u, juce::Font::bold | juce::Font::italic);
    title.setExtraKerningFactor (0.06f);
    const auto titleArea = juce::Rectangle<int> ((int) (96.f * u), (int) (6.f * u), (int) (420.f * u), (int) (46.f * u));
    g.setFont (title);
    g.setColour (kcol::pink.withAlpha (0.25f));
    g.drawText ("KITSUNE EQ", titleArea.translated (0, (int) (2.f * u)), juce::Justification::centredLeft);
    g.setGradientFill (juce::ColourGradient (kcol::hotPink, (float) titleArea.getX(), (float) titleArea.getY(),
                                             kcol::purple, (float) titleArea.getX() + 260.f * u, (float) titleArea.getBottom(), false));
    g.drawText ("KITSUNE EQ", titleArea, juce::Justification::centredLeft);

    juce::Font tag (11.f * u, juce::Font::bold);
    tag.setExtraKerningFactor (0.22f);
    g.setFont (tag);
    g.setColour (kcol::lilac.withAlpha (0.85f));
    g.drawText ("INTELLIGENT SPECTRAL PROCESSOR", juce::Rectangle<int> ((int) (98.f * u), (int) (50.f * u), (int) (420.f * u), (int) (18.f * u)),
                juce::Justification::centredLeft);

    // control panel backdrop
    auto panel = juce::Rectangle<float> (14.f * u, 372.f * u, b.getWidth() - 28.f * u, 238.f * u);
    g.setColour (kcol::panel.withAlpha (0.72f));
    g.fillRoundedRectangle (panel, 14.f * u);
    g.setColour (kcol::purple.withAlpha (0.45f));
    g.drawRoundedRectangle (panel, 14.f * u, 1.2f);

    // signature, bottom-right corner
    juce::Font sig (16.f * u, juce::Font::bold | juce::Font::italic);
    sig.setExtraKerningFactor (0.04f);
    g.setFont (sig);
    const auto sigArea = juce::Rectangle<int> ((int) (b.getWidth() - 230.f * u), (int) (b.getHeight() - 28.f * u), (int) (216.f * u), (int) (22.f * u));
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (kcol::pink.withAlpha (0.07f * (float) (4 - i)));
        g.drawText ("By prodbyperke", sigArea.expanded (i, i), juce::Justification::centredRight);
    }
    g.setGradientFill (juce::ColourGradient (kcol::purple, (float) sigArea.getX(), 0.f, kcol::hotPink, (float) sigArea.getRight(), 0.f, false));
    g.drawText ("By prodbyperke", sigArea, juce::Justification::centredRight);
}

void KitsuneEditor::layoutKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void KitsuneEditor::resized()
{
    auto b = getLocalBounds();
    const float u = (float) b.getWidth() / (float) kBaseW;
    auto S = [u] (int v) { return (int) std::lround ((float) v * u); };

    // header controls
    presetBox.setBounds (b.getWidth() - S (14 + 230 + 12 + 96), S (22), S (230), S (30));
    bypassButton.setBounds (b.getWidth() - S (14 + 96), S (22), S (96), S (30));

    // spectrum
    spectrum.setBounds (S (14), S (84), b.getWidth() - S (28), S (278));

    // control panel
    auto panel = juce::Rectangle<int> (S (14), S (372), b.getWidth() - S (28), S (238)).reduced (S (10), S (8));

    auto left = panel.removeFromLeft (S (210));
    layoutKnob (depth, left);

    auto right = panel.removeFromRight (S (230));
    auto grid = panel;

    const int cols = 5, rows = 2;
    const int cw = grid.getWidth() / cols, ch = grid.getHeight() / rows;
    Knob* order[] = { &trigger, &focus, &sharpness, &tilt, &guard, &attack, &release, &lowHz, &highHz, &link };
    for (int i = 0; i < 10; ++i)
        layoutKnob (*order[i], juce::Rectangle<int> (grid.getX() + (i % cols) * cw, grid.getY() + (i / cols) * ch, cw, ch).reduced (S (2)));

    // right column: mix + output on top, combos and delta below
    auto rTop = right.removeFromTop (right.getHeight() * 55 / 100);
    layoutKnob (mix, rTop.removeFromLeft (rTop.getWidth() / 2).reduced (S (2)));
    layoutKnob (output, rTop.reduced (S (2)));

    auto rBot = right.reduced (S (6), 0);
    const int half = rBot.getWidth() / 2;
    auto m = rBot.removeFromLeft (half).reduced (S (4), 0);
    auto s = rBot.reduced (S (4), 0);
    modeLabel.setBounds (m.removeFromTop (S (14)));
    modeBox.setBounds (m.removeFromTop (S (28)));
    stereoLabel.setBounds (s.removeFromTop (S (14)));
    stereoBox.setBounds (s.removeFromTop (S (28)));
    deltaButton.setBounds (juce::Rectangle<int> (right.getX() + S (10), right.getBottom() - S (30), right.getWidth() - S (20), S (28)));
}
