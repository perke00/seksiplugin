#pragma once
#include "Common.h"
#include "KitsuneLookAndFeel.h"

// Live spectrum (pink) with the gain-reduction curve (purple) hanging from the top edge,
// plus the processing range markers. Reads display data from the audio engine.
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    SpectrumView (const kit::Engine& e, juce::AudioProcessorValueTreeState& s) : engine (e), apvts (s)
    {
        inDb.fill (-120.f); grDb.fill (0.f);
        startTimerHz (30);
        setOpaque (false);
    }
    ~SpectrumView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0220), 0.f, area.getY(), juce::Colour (0xff1c0838), 0.f, area.getBottom(), false));
        g.fillRoundedRectangle (area, 12.f);

        auto plot = area.reduced (12.f, 10.f);
        plot.removeFromBottom (16.f);
        const float W = plot.getWidth(), H = plot.getHeight();
        auto xOf = [&] (float hz) { return plot.getX() + W * std::log10 (std::max (20.f, hz) / 20.f) / 3.f; };

        // grid
        g.setFont (juce::Font (10.f));
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            const float x = xOf (f);
            g.setColour (kcol::purple.withAlpha (0.16f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (kcol::dim);
            g.drawText (f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x - 20.f, plot.getBottom() + 1.f, 40.f, 14.f), juce::Justification::centred);
        }
        for (int i = 1; i < 5; ++i)
        {
            g.setColour (kcol::purple.withAlpha (0.10f));
            g.drawHorizontalLine ((int) (plot.getY() + H * (float) i / 5.f), plot.getX(), plot.getRight());
        }

        // range markers
        const float lo = apvts.getRawParameterValue ("low")->load(), hi = apvts.getRawParameterValue ("high")->load();
        const float xl = xOf (lo), xh = xOf (hi);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getY(), std::max (0.f, xl - plot.getX()), H));
        g.fillRect (juce::Rectangle<float> (xh, plot.getY(), std::max (0.f, plot.getRight() - xh), H));
        g.setColour (kcol::hotPink.withAlpha (0.7f));
        g.drawVerticalLine ((int) xl, plot.getY(), plot.getBottom());
        g.drawVerticalLine ((int) xh, plot.getY(), plot.getBottom());

        // spectrum
        float mx = -120.f;
        for (auto v : inDb) mx = std::max (mx, v);
        refDb += (mx - refDb) * 0.05f;
        const float top = refDb + 4.f, range = 72.f;

        juce::Path line;
        for (int i = 0; i < kit::kDisplayPoints; ++i)
        {
            const float x = plot.getX() + W * ((float) i + 0.5f) / (float) kit::kDisplayPoints;
            const float y = plot.getBottom() - H * std::clamp ((inDb[(size_t) i] - (top - range)) / range, 0.f, 1.f);
            if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
        }
        juce::Path fill (line);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (kcol::pink.withAlpha (0.50f), 0.f, plot.getY(), kcol::purple.withAlpha (0.04f), 0.f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (kcol::hotPink);
        g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

        // gain reduction, hanging from the top (0..-24 dB over 55% of the height)
        juce::Path gr;
        gr.startNewSubPath (plot.getX(), plot.getY());
        for (int i = 0; i < kit::kDisplayPoints; ++i)
        {
            const float x = plot.getX() + W * ((float) i + 0.5f) / (float) kit::kDisplayPoints;
            gr.lineTo (x, plot.getY() + H * 0.55f * std::clamp (-grDb[(size_t) i] / 24.f, 0.f, 1.f));
        }
        juce::Path grLine (gr);
        gr.lineTo (plot.getRight(), plot.getY());
        gr.closeSubPath();
        g.setGradientFill (juce::ColourGradient (kcol::purple.withAlpha (0.55f), 0.f, plot.getY(), kcol::purple.withAlpha (0.08f), 0.f, plot.getY() + H * 0.55f, false));
        g.fillPath (gr);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (kcol::lilac.withAlpha (0.06f * (float) (4 - i)));
            g.strokePath (grLine, juce::PathStrokeType (1.5f + (float) i * 2.f));
        }
        g.setColour (kcol::lilac);
        g.strokePath (grLine, juce::PathStrokeType (1.6f));

        g.setColour (kcol::dim);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText ("REDUCTION", juce::Rectangle<float> (plot.getX() + 6.f, plot.getY() + 2.f, 90.f, 14.f), juce::Justification::centredLeft);
        g.drawText ("INPUT SPECTRUM", juce::Rectangle<float> (plot.getRight() - 120.f, plot.getBottom() - 16.f, 114.f, 14.f), juce::Justification::centredRight);

        g.setColour (kcol::purple.withAlpha (0.6f));
        g.drawRoundedRectangle (area.reduced (0.5f), 12.f, 1.2f);
    }

private:
    void timerCallback() override
    {
        for (int i = 0; i < kit::kDisplayPoints; ++i)
        {
            const float in = engine.getDisplayIn (i), gr = engine.getDisplayGr (i);
            auto& a = inDb[(size_t) i]; a += (in - a) * (in > a ? 0.6f : 0.18f);
            auto& b = grDb[(size_t) i]; b += (gr - b) * (gr < b ? 0.6f : 0.2f);
        }
        repaint();
    }

    const kit::Engine& engine;
    juce::AudioProcessorValueTreeState& apvts;
    std::array<float, kit::kDisplayPoints> inDb {}, grDb {};
    float refDb = 40.f;
};
