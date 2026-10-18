#pragma once
#include "Common.h"

// KITSUNE EQ visual identity: 80s synthwave, hot pink + electric purple on deep violet.
namespace kcol
{
const juce::Colour bgTop     { 0xff0a0118 };
const juce::Colour bgBottom  { 0xff2b0a47 };
const juce::Colour panel     { 0xff14052a };
const juce::Colour pink      { 0xffff3fa4 };
const juce::Colour hotPink   { 0xffff6ec7 };
const juce::Colour purple    { 0xff9a5cff };
const juce::Colour lilac     { 0xffd9b3ff };
const juce::Colour text      { 0xfff4e9ff };
const juce::Colour dim       { 0xff8f6cc0 };
}

class KitsuneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KitsuneLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, kcol::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, kcol::panel);
        setColour (juce::ComboBox::textColourId, kcol::text);
        setColour (juce::ComboBox::outlineColourId, kcol::purple.withAlpha (0.6f));
        setColour (juce::ComboBox::arrowColourId, kcol::pink);
        setColour (juce::PopupMenu::backgroundColourId, kcol::panel);
        setColour (juce::PopupMenu::textColourId, kcol::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kcol::pink.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Label::textColourId, kcol::lilac);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
        const float d = std::min (b.getWidth(), b.getHeight()), r = d * 0.5f;
        const auto c = b.getCentre();
        const float ang = startAngle + pos * (endAngle - startAngle);

        // body
        const float rb = r * 0.76f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff33135c), c.x, c.y - rb, juce::Colour (0xff0d0320), c.x, c.y + rb, false));
        g.fillEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f);
        g.setColour (kcol::purple.withAlpha (0.45f));
        g.drawEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f, 1.2f);

        // track
        const float ra = r * 0.92f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, ra, ra, 0.f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff3b1d66));
        g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc with neon glow
        juce::Path val;
        val.addCentredArc (c.x, c.y, ra, ra, 0.f, startAngle, ang, true);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (kcol::pink.withAlpha (0.09f * (float) (4 - i)));
            g.strokePath (val, juce::PathStrokeType (3.f + (float) i * 3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setGradientFill (juce::ColourGradient (kcol::purple, c.x - ra, c.y, kcol::hotPink, c.x + ra, c.y, false));
        g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // pointer
        const juce::Point<float> p1 (c.x + std::sin (ang) * rb * 0.35f, c.y - std::cos (ang) * rb * 0.35f);
        const juce::Point<float> p2 (c.x + std::sin (ang) * rb * 0.9f,  c.y - std::cos (ang) * rb * 0.9f);
        g.setColour (juce::Colours::white);
        g.drawLine (juce::Line<float> (p1, p2), 2.6f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& bt, bool highlighted, bool) override
    {
        auto b = bt.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = bt.getToggleState();
        if (on)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (kcol::pink.withAlpha (0.07f * (float) (4 - i)));
                g.fillRoundedRectangle (b.expanded ((float) i * 1.5f), b.getHeight() * 0.5f);
            }
            g.setGradientFill (juce::ColourGradient (kcol::purple, b.getX(), b.getY(), kcol::pink, b.getRight(), b.getBottom(), false));
            g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        }
        else
        {
            g.setColour (kcol::panel);
            g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
            g.setColour (kcol::purple.withAlpha (highlighted ? 0.95f : 0.6f));
            g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.3f);
        }
        g.setColour (on ? juce::Colours::white : kcol::lilac);
        g.setFont (juce::Font (12.f, juce::Font::bold));
        g.drawText (bt.getButtonText(), bt.getLocalBounds(), juce::Justification::centred);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (13.f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::Font (14.f); }
};
