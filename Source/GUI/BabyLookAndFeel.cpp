#include "BabyLookAndFeel.h"
using namespace babyair::ui;

BabyLookAndFeel::BabyLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, colours::lime);
    setColour (juce::Slider::thumbColourId, colours::lime);
}

juce::Font BabyLookAndFeel::boldFont (float size, float kerning)
{
    juce::Font f (juce::FontOptions (size, juce::Font::bold));
    f.setExtraKerningFactor (kerning);
    return f;
}

static void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, bool enabled = true)
{
    if (enabled)
    {
        g.setColour (c.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (width * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.22f)); g.strokePath (p, juce::PathStrokeType (width * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void BabyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float, float, juce::Slider& slider)
{
    auto* ls = dynamic_cast<LinkableSlider*> (&slider);
    const bool small = ls != nullptr && ls->isSmall;

    // geometry is defined for a reference size and scaled
    const float refW = small ? 88.0f : 160.0f;
    const float s = (float) width / refW;
    g.saveState();
    g.addTransform (juce::AffineTransform::scale (s).translated ((float) x, (float) y)); // local origin = slider top-left

    const float cx = small ? 44.0f : 80.0f;
    const float cy = small ? 36.0f : 92.0f;
    const float outerR = small ? 30.0f : 76.0f;
    const float capR   = small ? 21.0f : 58.0f;
    const float arcR   = small ? 27.0f : 69.0f;
    const float arcW   = small ? 4.0f  : 7.5f;

    const float angle = kArcStart + sliderPos * (kArcEnd - kArcStart);
    const bool enabled = slider.isEnabled();
    const bool active = slider.isMouseOverOrDragging();

    // --- socket / bezel
    {
        juce::ColourGradient bez (juce::Colour (0xff3a3d3a), cx - outerR * 0.5f, cy - outerR, juce::Colour (0xff070807), cx + outerR * 0.5f, cy + outerR, false);
        g.setGradientFill (bez);
        g.fillEllipse (cx - outerR, cy - outerR, outerR * 2, outerR * 2);
        g.setColour (juce::Colour (0xff050605));
        g.fillEllipse (cx - outerR + 3.0f * (small ? 0.5f : 1.0f), cy - outerR + 3.0f * (small ? 0.5f : 1.0f),
                       (outerR - 3.0f * (small ? 0.5f : 1.0f)) * 2, (outerR - 3.0f * (small ? 0.5f : 1.0f)) * 2);
    }

    // --- arc track + value arc (with glow)
    {
        juce::Path track; track.addCentredArc (cx, cy, arcR, arcR, 0.0f, kArcStart, kArcEnd, true);
        g.setColour (juce::Colour (0xff1b2418));
        g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.001f)
        {
            juce::Path val; val.addCentredArc (cx, cy, arcR, arcR, 0.0f, kArcStart, angle, true);
            glowStroke (g, val, colours::lime, arcW * 0.82f, enabled);
        }
    }

    // --- knob cap
    {
        // drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (cx - capR + 1.5f, cy - capR + 4.0f, capR * 2, capR * 2);

        juce::ColourGradient cap (juce::Colour (active ? 0xff3b3d3c : 0xff303231), cx - capR * 0.55f, cy - capR * 0.85f,
                                  juce::Colour (0xff0c0d0c), cx + capR * 0.6f, cy + capR, false);
        g.setGradientFill (cap);
        g.fillEllipse (cx - capR, cy - capR, capR * 2, capR * 2);

        // knurled rim: fine ticks around the cap
        const int ticks = small ? 24 : 48;
        g.setColour (juce::Colour (0x22ffffff));
        for (int i = 0; i < ticks; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ticks;
            const float r1 = capR - (small ? 1.8f : 3.2f), r2 = capR - 0.4f;
            g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1, cx + std::sin (a) * r2, cy - std::cos (a) * r2, small ? 0.6f : 0.9f);
        }

        // top face (slightly concave look)
        const float faceR = capR * 0.80f;
        juce::ColourGradient face (juce::Colour (0xff17181a), cx - faceR * 0.4f, cy - faceR * 0.8f, juce::Colour (0xff222423), cx + faceR * 0.5f, cy + faceR, false);
        g.setGradientFill (face);
        g.fillEllipse (cx - faceR, cy - faceR, faceR * 2, faceR * 2);
        g.setColour (juce::Colour (0x33ffffff));
        g.drawEllipse (cx - capR + 0.6f, cy - capR + 0.6f, capR * 2 - 1.2f, capR * 2 - 1.2f, 0.9f);

        // glossy highlight
        juce::Path hi; hi.addCentredArc (cx, cy, capR * 0.9f, capR * 0.9f, 0.0f, -2.5f, -0.9f, true);
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.strokePath (hi, juce::PathStrokeType (small ? 1.4f : 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // indicator
        const float r1 = capR * 0.32f, r2 = capR * 0.78f;
        juce::Path ind; ind.startNewSubPath (cx + std::sin (angle) * r1, cy - std::cos (angle) * r1);
        ind.lineTo (cx + std::sin (angle) * r2, cy - std::cos (angle) * r2);
        glowStroke (g, ind, colours::lime, small ? 1.8f : 3.2f, enabled);
    }

    g.restoreState();

    // --- value read-out (drawn in the component's own space so text stays crisp)
    juce::String text;
    if (small)
        text = juce::String (slider.getValue(), 1) + " dB";
    else
        text = juce::String (juce::roundToInt (slider.getValue())) + "%";

    g.setColour (colours::text.withAlpha (0.92f));
    g.setFont (boldFont (small ? 11.5f * s : 25.0f * s, 0.04f));
    const float ty = small ? (cy + outerR + 1.0f) : (cy + outerR + 4.0f);
    g.drawText (text, juce::Rectangle<float> ((float) x, (float) y + ty * s, (float) width, (small ? 14.0f : 30.0f) * s), juce::Justification::centred, false);
}

void BabyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const juce::String kind = b.getComponentID();

    if (kind == "link")
    {
        const float s = bounds.getWidth() / 84.0f;
        g.saveState();
        g.addTransform (juce::AffineTransform::scale (s, s));
        const juce::Rectangle<float> track (4.0f, 8.0f, 76.0f, 28.0f);

        // recessed slot
        g.setColour (juce::Colour (0xff050605)); g.fillRoundedRectangle (track.expanded (2.0f), 16.0f);
        juce::ColourGradient slot (juce::Colour (0xff101211), 0, track.getY(), juce::Colour (0xff1e211f), 0, track.getBottom(), false);
        g.setGradientFill (slot); g.fillRoundedRectangle (track, 14.0f);
        g.setColour (juce::Colour (0x40ffffff)); g.drawRoundedRectangle (track.expanded (1.5f), 15.0f, 1.0f);

        const float cx = on ? track.getX() + 16.0f : track.getRight() - 16.0f;
        const float cy = track.getCentreY();
        if (on)
        {
            g.setColour (colours::lime.withAlpha (0.16f)); g.fillEllipse (cx - 21.0f, cy - 21.0f, 42.0f, 42.0f);
            g.setColour (colours::lime.withAlpha (0.28f)); g.fillEllipse (cx - 15.0f, cy - 15.0f, 30.0f, 30.0f);
        }
        juce::ColourGradient bead (on ? colours::limeSoft : juce::Colour (0xff6a6e69), cx - 6.0f, cy - 8.0f,
                                   on ? colours::green : juce::Colour (0xff2c2f2c), cx + 6.0f, cy + 8.0f, false);
        g.setGradientFill (bead); g.fillEllipse (cx - 11.0f, cy - 11.0f, 22.0f, 22.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.65f : 0.25f)); g.fillEllipse (cx - 6.0f, cy - 8.0f, 7.0f, 4.0f);
        if (highlighted) { g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (track, 14.0f); }
        g.restoreState();
    }
    else // bypass
    {
        const float s = bounds.getWidth() / 100.0f;
        g.saveState();
        g.addTransform (juce::AffineTransform::scale (s, s));
        juce::Rectangle<float> r (6.0f, 4.0f, 88.0f, 40.0f);
        const float press = on ? 2.0f : 0.0f;

        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 3.0f - press), 20.0f);
        juce::ColourGradient body (juce::Colour (on ? 0xff1b1c1b : 0xff333634), 0, r.getY(), juce::Colour (0xff0d0e0d), 0, r.getBottom(), false);
        g.setGradientFill (body); g.fillRoundedRectangle (r.translated (0, press), 20.0f);
        g.setColour (juce::Colour (0x33ffffff)); g.drawRoundedRectangle (r.translated (0, press).reduced (0.6f), 19.5f, 1.0f);

        // power icon: lit (cream) while processing, dim + amber ring while bypassed
        const float ix = r.getCentreX(), iy = r.getCentreY() + press;
        const auto col = on ? colours::amber : colours::text;
        juce::Path icon; icon.addCentredArc (ix, iy + 1.0f, 9.0f, 9.0f, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
        icon.startNewSubPath (ix, iy - 11.0f); icon.lineTo (ix, iy - 2.0f);
        if (on) { g.setColour (col.withAlpha (0.18f)); g.strokePath (icon, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        g.setColour (on ? col : col.withAlpha (0.92f));
        g.strokePath (icon, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (highlighted) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r.translated (0, press), 20.0f); }
        g.restoreState();
    }
}
