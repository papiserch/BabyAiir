#include "DeviceArt.h"
#include "BabyLookAndFeel.h"
using namespace babyair::ui;
using juce::Colour;
using juce::ColourGradient;
using juce::Path;
using juce::Rectangle;

namespace
{
inline float rad (float deg) { return deg * juce::MathConstants<float>::pi / 180.0f; }

Path heartShape (float cx, float cy, float size)
{
    Path p;
    p.startNewSubPath (0.0f, 0.95f);
    p.cubicTo (-1.35f, 0.05f, -0.95f, -0.95f, 0.0f, -0.38f);
    p.cubicTo (0.95f, -0.95f, 1.35f, 0.05f, 0.0f, 0.95f);
    p.closeSubPath();
    p.applyTransform (juce::AffineTransform::scale (size).translated (cx, cy));
    return p;
}

Path sparkle (float cx, float cy, float r)
{
    Path p;
    p.startNewSubPath (cx, cy - r);
    p.quadraticTo (cx, cy, cx + r * 0.62f, cy);
    p.quadraticTo (cx, cy, cx, cy + r);
    p.quadraticTo (cx, cy, cx - r * 0.62f, cy);
    p.quadraticTo (cx, cy, cx, cy - r);
    p.closeSubPath();
    return p;
}

void glow (juce::Graphics& g, const Path& p, Colour c, float w)
{
    g.setColour (c.withAlpha (0.07f)); g.strokePath (p, juce::PathStrokeType (w * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.14f)); g.strokePath (p, juce::PathStrokeType (w * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);                   g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// translucent moulded-plastic bumper
void bumper (juce::Graphics& g, Rectangle<float> r, float corner)
{
    g.setColour (colours::lime.withAlpha (0.10f)); g.fillRoundedRectangle (r.expanded (7.0f), corner + 7.0f);
    g.setColour (colours::lime.withAlpha (0.18f)); g.fillRoundedRectangle (r.expanded (3.5f), corner + 3.5f);
    ColourGradient body (colours::limeSoft, r.getX(), r.getY(), colours::greenDark, r.getRight(), r.getBottom(), false);
    body.addColour (0.35, colours::lime);
    body.addColour (0.7, colours::green);
    g.setGradientFill (body); g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::white.withAlpha (0.38f));
    g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.18f, 3.0f).withHeight (r.getHeight() * 0.34f), corner * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (r, corner, 1.2f);
}

// recessed black panel with a bevelled lip
void recessedPanel (juce::Graphics& g, Rectangle<float> r, float corner)
{
    g.setColour (juce::Colours::white.withAlpha (0.55f)); g.drawRoundedRectangle (r.expanded (1.6f).translated (0.0f, 1.2f), corner + 1.5f, 1.4f); // lit lower lip
    g.setColour (colours::creamDark.darker (0.45f));      g.drawRoundedRectangle (r.expanded (2.4f), corner + 2.4f, 2.6f);                         // shadowed groove
    ColourGradient fill (Colour (0xff1c1e1d), r.getX(), r.getY(), Colour (0xff090a09), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill); g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black.withAlpha (0.65f)); g.drawRoundedRectangle (r.reduced (1.0f), corner - 1.0f, 2.0f);                           // inner shadow
    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (r.reduced (2.4f).translated (0.0f, 0.8f), corner - 2.4f, 1.0f);   // inner rim light
}

void scratches (juce::Graphics& g, Rectangle<float> r, juce::Random& rnd, int n, float alpha)
{
    for (int i = 0; i < n; ++i)
    {
        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
        const float y = r.getY() + rnd.nextFloat() * r.getHeight();
        const float len = 6.0f + rnd.nextFloat() * 26.0f;
        const float a = rad (-35.0f + rnd.nextFloat() * 20.0f);
        g.setColour (juce::Colours::white.withAlpha (alpha * (0.4f + rnd.nextFloat())));
        g.drawLine (x, y, x + std::cos (a) * len, y + std::sin (a) * len, 0.6f);
    }
}

void demonHeartSticker (juce::Graphics& g, float cx, float cy)
{
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (rad (-9.0f), cx, cy));
    const float s = 25.0f;
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillPath (heartShape (cx + 1.5f, cy + 2.5f, s * 1.2f));
    g.setColour (juce::Colour (0xfff4f2ea));             g.fillPath (heartShape (cx, cy, s * 1.2f));       // white die-cut border
    g.setColour (juce::Colour (0xff10200e));             g.fillPath (heartShape (cx, cy, s * 1.0f));
    // little horns
    Path horns;
    horns.addTriangle (cx - 15.0f, cy - 17.0f, cx - 8.0f, cy - 13.0f, cx - 17.0f, cy - 25.0f);
    horns.addTriangle (cx + 15.0f, cy - 17.0f, cx + 8.0f, cy - 13.0f, cx + 17.0f, cy - 25.0f);
    g.setColour (juce::Colour (0xfff4f2ea)); g.fillPath (horns);
    g.setColour (juce::Colour (0xff10200e)); g.fillPath (horns.createPathWithRoundedCorners (1.0f), juce::AffineTransform::scale (0.62f, 0.62f, cx, cy - 18.0f));
    // angry grinning face
    Path eyeL, eyeR;
    eyeL.addTriangle (cx - 15.0f, cy - 8.0f, cx - 4.0f, cy - 3.0f, cx - 14.0f, cy + 0.5f);
    eyeR.addTriangle (cx + 15.0f, cy - 8.0f, cx + 4.0f, cy - 3.0f, cx + 14.0f, cy + 0.5f);
    g.setColour (colours::lime); g.fillPath (eyeL); g.fillPath (eyeR);
    Path mouth;
    mouth.startNewSubPath (cx - 12.0f, cy + 6.0f);
    for (int i = 0; i < 6; ++i) mouth.lineTo (cx - 12.0f + 4.8f * (float) (i + 1), cy + 6.0f + ((i % 2 == 0) ? 5.0f : 0.0f));
    g.strokePath (mouth, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered));
    g.setColour (colours::lime.withAlpha (0.9f));
    g.fillEllipse (cx - 1.8f, cy + 14.0f, 3.6f, 6.0f);   // slime drip
    g.restoreState();
}

void skullSticker (juce::Graphics& g, float cx, float cy)
{
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (rad (8.0f), cx, cy));
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (cx - 25.0f + 1.5f, cy - 25.0f + 2.5f, 50.0f, 50.0f);
    g.setColour (juce::Colour (0xfff4f2ea));             g.fillEllipse (cx - 25.0f, cy - 25.0f, 50.0f, 50.0f);
    g.setColour (juce::Colour (0xff141614));             g.fillEllipse (cx - 22.0f, cy - 22.0f, 44.0f, 44.0f);
    // skull
    g.setColour (juce::Colour (0xfff4f2ea));
    g.fillEllipse (cx - 14.0f, cy - 17.0f, 28.0f, 25.0f);
    g.fillRoundedRectangle (cx - 8.5f, cy + 2.0f, 17.0f, 14.0f, 3.0f);
    g.setColour (juce::Colour (0xff141614));
    for (int side = -1; side <= 1; side += 2)   // X eyes
    {
        const float ex = cx + (float) side * 6.2f, ey = cy - 4.0f;
        g.drawLine (ex - 3.2f, ey - 3.2f, ex + 3.2f, ey + 3.2f, 2.0f);
        g.drawLine (ex - 3.2f, ey + 3.2f, ex + 3.2f, ey - 3.2f, 2.0f);
    }
    g.fillRect (cx - 1.0f, cy + 3.0f, 2.0f, 3.0f);                                  // nose
    for (int i = -2; i <= 2; ++i) g.drawLine (cx + (float) i * 3.2f, cy + 9.0f, cx + (float) i * 3.2f, cy + 15.0f, 1.0f); // teeth
    g.restoreState();
}

void screw (juce::Graphics& g, float cx, float cy, float turn)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (cx - 4.5f, cy - 3.2f, 9.0f, 9.0f);
    ColourGradient head (Colour (0xffd9d4c4), cx - 3.0f, cy - 4.0f, Colour (0xff6e6a5e), cx + 3.0f, cy + 4.0f, false);
    g.setGradientFill (head); g.fillEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
    g.setColour (Colour (0xff2a2924));
    g.drawLine (cx + std::cos (turn) * 3.0f, cy + std::sin (turn) * 3.0f, cx - std::cos (turn) * 3.0f, cy - std::sin (turn) * 3.0f, 1.1f);
    g.drawLine (cx + std::cos (turn + 1.5708f) * 3.0f, cy + std::sin (turn + 1.5708f) * 3.0f, cx - std::cos (turn + 1.5708f) * 3.0f, cy - std::sin (turn + 1.5708f) * 3.0f, 1.1f);
}
} // namespace

void DeviceArt::paint (juce::Graphics& g)
{
    // Render the static artwork once per window size (and per display scale) and just blit it afterwards.
    const float px = g.getInternalContext().getPhysicalPixelScaleFactor() * (float) getWidth() / (float) kW;
    if (! cache.isValid() || std::abs (px - cachedScale) > 0.001f)
    {
        cache = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) kW * px)),
                             juce::jmax (1, juce::roundToInt ((float) kH * px)), true);
        juce::Graphics cg (cache);
        cg.addTransform (juce::AffineTransform::scale (px));
        paintDevice (cg);
        cachedScale = px;
    }
    g.drawImage (cache, getLocalBounds().toFloat());
}

void DeviceArt::paintDevice (juce::Graphics& g)
{
    using babyair::ui::colours::black;

    // ------------------------------------------------------------------ studio backdrop
    g.fillAll (black);
    {
        g.setGradientFill (ColourGradient (colours::lime.withAlpha (0.22f), 384.0f, 520.0f, Colour (0x00000000), 384.0f, 150.0f, true));
        g.fillRect (0, 0, kW, kH);
        g.setGradientFill (ColourGradient (Colour (0xff1e2a1c).withAlpha (0.6f), 384.0f, 40.0f, Colour (0x00000000), 384.0f, 330.0f, true));
        g.fillRect (0, 0, kW, kH);
    }

    // contact shadow + green under-glow on the "table"
    g.setColour (colours::lime.withAlpha (0.10f)); g.fillEllipse (40.0f, 470.0f, 688.0f, 38.0f);
    g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillEllipse (60.0f, 474.0f, 648.0f, 22.0f);

    // ------------------------------------------------------------------ shell
    Path shell; shell.addRoundedRectangle (30.0f, 150.0f, 708.0f, 332.0f, 44.0f);

    // outer green glow, like the lit edge of the reference
    glow (g, shell, colours::lime.withAlpha (0.45f), 1.6f);

    // side bumpers first (they sit slightly behind the shell edge)
    bumper (g, { 18.0f, 196.0f, 36.0f, 168.0f }, 17.0f);
    bumper (g, { 714.0f, 196.0f, 36.0f, 168.0f }, 17.0f);
    bumper (g, { 24.0f, 398.0f, 28.0f, 62.0f }, 13.0f);
    bumper (g, { 716.0f, 398.0f, 28.0f, 62.0f }, 13.0f);

    // moulded cream plastic
    {
        ColourGradient body (colours::creamWarm, 0.0f, 150.0f, colours::creamDark, 0.0f, 482.0f, false);
        body.addColour (0.45, colours::cream);
        g.setGradientFill (body); g.fillPath (shell);

        g.saveState();
        g.reduceClipRegion (shell);
        // edge shading: darker toward the rim, as if the shell curves away
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.045f));
            g.strokePath (shell, juce::PathStrokeType (3.0f + (float) i * 4.0f));
        }
        // soft studio highlight across the upper-left
        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.45f), 150.0f, 160.0f, juce::Colours::white.withAlpha (0.0f), 330.0f, 260.0f, true));
        g.fillRect (30.0f, 150.0f, 708.0f, 200.0f);

        // wear: dirt, scuffs, paint flecks (fixed seed, so the toy always looks the same)
        juce::Random rnd (1999);
        for (int i = 0; i < 70; ++i)
        {
            const float x = 34.0f + rnd.nextFloat() * 700.0f, y = 154.0f + rnd.nextFloat() * 326.0f, r = 0.5f + rnd.nextFloat() * 1.8f;
            g.setColour (Colour (0xff6d6350).withAlpha (0.05f + rnd.nextFloat() * 0.14f));
            g.fillEllipse (x, y, r * 1.6f, r);
        }
        for (int i = 0; i < 9; ++i)
        {
            g.setColour (colours::green.withAlpha (0.25f + rnd.nextFloat() * 0.3f));
            g.fillEllipse (40.0f + rnd.nextFloat() * 680.0f, 156.0f + rnd.nextFloat() * 320.0f, 1.2f + rnd.nextFloat() * 1.6f, 1.0f + rnd.nextFloat() * 1.4f);
        }
        scratches (g, { 34.0f, 154.0f, 700.0f, 326.0f }, rnd, 26, 0.55f);
        for (int i = 0; i < 12; ++i)   // dark hairline scratches
        {
            const float x = 40.0f + rnd.nextFloat() * 680.0f, y = 160.0f + rnd.nextFloat() * 310.0f;
            g.setColour (Colour (0xff5a5140).withAlpha (0.22f));
            g.drawLine (x, y, x + 5.0f + rnd.nextFloat() * 12.0f, y + rnd.nextFloat() * 6.0f - 3.0f, 0.5f);
        }
        g.restoreState();
    }
    g.setColour (juce::Colours::white.withAlpha (0.85f)); g.strokePath (shell, juce::PathStrokeType (1.2f));    // specular edge
    {
        Path lip; lip.startNewSubPath (74.0f, 153.5f); lip.lineTo (694.0f, 153.5f);
        g.setColour (juce::Colours::black.withAlpha (0.16f)); g.strokePath (lip, juce::PathStrokeType (1.4f));      // the ledge the Baby Demon leans on
        g.setColour (juce::Colours::white.withAlpha (0.7f));  g.strokePath (lip.createPathWithRoundedCorners (0), juce::PathStrokeType (0.8f), juce::AffineTransform::translation (0.0f, -1.6f));
    }

    // ------------------------------------------------------------------ black glossy logo cap (top-left)
    {
        Path cap;
        cap.startNewSubPath (50.0f, 168.0f);
        cap.cubicTo (46.0f, 120.0f, 86.0f, 92.0f, 146.0f, 90.0f);
        cap.cubicTo (202.0f, 88.0f, 232.0f, 112.0f, 262.0f, 140.0f);
        cap.cubicTo (274.0f, 151.0f, 290.0f, 154.0f, 308.0f, 154.0f);
        cap.lineTo (308.0f, 158.0f);
        cap.cubicTo (230.0f, 168.0f, 130.0f, 170.0f, 50.0f, 168.0f);
        cap.closeSubPath();

        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillPath (cap, juce::AffineTransform::translation (2.0f, 4.0f));
        glow (g, cap, colours::lime.withAlpha (0.55f), 1.0f);
        ColourGradient gloss (Colour (0xff343836), 120.0f, 92.0f, Colour (0xff060706), 220.0f, 170.0f, false);
        g.setGradientFill (gloss); g.fillPath (cap);
        // glossy reflection streak
        Path hi; hi.startNewSubPath (66.0f, 150.0f); hi.cubicTo (64.0f, 124.0f, 92.0f, 104.0f, 138.0f, 102.0f);
        g.setColour (juce::Colours::white.withAlpha (0.30f)); g.strokePath (hi, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.strokePath (hi, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // BABY AIR - hand-lettered, slightly tilted
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (rad (-6.0f), 158.0f, 128.0f));
        juce::Font f (juce::FontOptions (33.0f, juce::Font::bold | juce::Font::italic));
        f.setExtraKerningFactor (0.02f);
        g.setFont (f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawText ("BABY AIR", Rectangle<float> (74.0f, 114.0f, 170.0f, 40.0f).translated (1.6f, 2.2f), juce::Justification::centred, false);
        g.setColour (colours::creamWarm);                     g.drawText ("BABY AIR", Rectangle<float> (74.0f, 114.0f, 170.0f, 40.0f), juce::Justification::centred, false);

        // lime brush underline
        Path brush; brush.startNewSubPath (88.0f, 152.0f); brush.cubicTo (130.0f, 145.0f, 190.0f, 144.0f, 236.0f, 140.0f);
        g.setColour (colours::lime); g.strokePath (brush, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::lime.withAlpha (0.5f)); g.strokePath (brush, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (4.0f, 4.0f));

        // little crown over the first B
        Path crown;
        crown.startNewSubPath (86.0f, 118.0f); crown.lineTo (84.0f, 104.0f); crown.lineTo (92.0f, 111.0f); crown.lineTo (98.0f, 100.0f);
        crown.lineTo (104.0f, 111.0f); crown.lineTo (112.0f, 104.0f); crown.lineTo (110.0f, 118.0f);
        g.setColour (colours::lime); g.strokePath (crown, juce::PathStrokeType (1.7f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        g.restoreState();

        g.setColour (colours::limeSoft.withAlpha (0.9f));
        g.setFont (BabyLookAndFeel::boldFont (8.0f, 0.22f));
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (rad (-3.0f), 160.0f, 160.0f));
        g.drawText ("BY PAPI SERCH", Rectangle<float> (92.0f, 152.0f, 140.0f, 10.0f), juce::Justification::centred, false);
        g.restoreState();
    }

    // small dark shoulder cap, top-right
    {
        Rectangle<float> r (640.0f, 138.0f, 92.0f, 20.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r.translated (1.0f, 3.0f), 10.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3d3b), r.getX(), r.getY(), Colour (0xff060706), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f)); g.fillRoundedRectangle (r.reduced (10.0f, 3.0f).withHeight (3.0f), 1.5f);
        g.setColour (colours::lime.withAlpha (0.5f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }

    // ------------------------------------------------------------------ recessed panels
    recessedPanel (g, { 72.0f, 170.0f, 644.0f, 210.0f }, 40.0f);
    recessedPanel (g, { 72.0f, 390.0f, 644.0f, 84.0f }, 32.0f);

    {   // faint scratches and dust on the black glass
        juce::Random rnd (777);
        g.saveState();
        Path clip; clip.addRoundedRectangle (72.0f, 170.0f, 644.0f, 210.0f, 40.0f); clip.addRoundedRectangle (72.0f, 390.0f, 644.0f, 84.0f, 32.0f);
        g.reduceClipRegion (clip);
        scratches (g, { 80.0f, 176.0f, 630.0f, 296.0f }, rnd, 22, 0.07f);
        g.restoreState();
    }

    // ------------------------------------------------------------------ engraved labels
    g.setColour (colours::text.withAlpha (0.93f));
    g.setFont (BabyLookAndFeel::boldFont (17.5f, 0.09f));
    g.drawText ("MID AIR",  Rectangle<float> (125.0f, 171.0f, 200.0f, 20.0f), juce::Justification::centred, false);
    g.drawText ("HIGH AIR", Rectangle<float> (432.0f, 171.0f, 200.0f, 20.0f), juce::Justification::centred, false);

    g.setFont (BabyLookAndFeel::boldFont (13.0f, 0.10f));
    g.drawText ("LINK", Rectangle<float> (327.0f, 226.0f, 100.0f, 16.0f), juce::Justification::centred, false);

    g.setColour (colours::text.withAlpha (0.88f));
    g.setFont (BabyLookAndFeel::boldFont (12.5f, 0.10f));
    g.drawText ("TRIM",   Rectangle<float> (178.0f, 413.0f, 50.0f, 16.0f), juce::Justification::centredLeft, false);
    g.drawText ("BYPASS", Rectangle<float> (586.0f, 446.0f, 100.0f, 16.0f), juce::Justification::centred, false);

    // little "air" accents next to the big knobs
    {
        g.setColour (colours::lime.withAlpha (0.85f));
        for (int i = 0; i < 3; ++i)
        {
            const float o = (float) i * 6.5f;
            g.drawLine (128.0f + o, 232.0f - o * 0.45f, 140.0f + o, 214.0f - o * 0.45f, 1.7f);          // /// beside MID AIR
            g.drawLine (435.0f + o, 232.0f - o * 0.45f, 447.0f + o, 214.0f - o * 0.45f, 1.7f);          // /// beside HIGH AIR
        }
        g.setColour (colours::lime.withAlpha (0.55f)); g.fillPath (sparkle (100.0f, 214.0f, 12.0f));
        g.setColour (colours::lime.withAlpha (0.35f)); g.fillPath (sparkle (112.0f, 328.0f, 8.0f));
        g.setColour (colours::limeSoft.withAlpha (0.8f)); g.fillPath (sparkle (100.0f, 214.0f, 4.0f));
    }

    // speaker grille + tagline in the lower strip
    {
        g.setColour (Colour (0xff050605));
        for (int row = 0; row < 3; ++row)
            for (int i = 0; i < 24; ++i)
            {
                const float x = 242.0f + (float) i * 12.0f + (row % 2 ? 6.0f : 0.0f), y = 404.0f + (float) row * 10.0f;
                g.fillEllipse (x, y, 5.0f, 5.0f);
                g.setColour (juce::Colours::white.withAlpha (0.10f)); g.fillEllipse (x, y + 4.2f, 5.0f, 1.0f); g.setColour (Colour (0xff050605));
            }
        g.setColour (colours::textDim.withAlpha (0.75f));
        g.setFont (BabyLookAndFeel::boldFont (7.6f, 0.17f));
        g.drawText ("UN JUGUETE POR FUERA. UN PROCESADOR PROFESIONAL POR DENTRO.", Rectangle<float> (214.0f, 446.0f, 340.0f, 12.0f), juce::Justification::centred, false);
    }

    // ------------------------------------------------------------------ stickers, doodles, screws
    demonHeartSticker (g, 63.0f, 300.0f);
    skullSticker (g, 724.0f, 410.0f);
    {   // marker scribble bottom-left
        g.setColour (Colour (0xff111311).withAlpha (0.88f));
        const float pts[][4] = { { 44, 372, 58, 394 }, { 52, 370, 64, 392 }, { 58, 376, 50, 396 }, { 62, 372, 70, 384 }, { 40, 382, 56, 380 } };
        for (auto& p : pts) g.drawLine (p[0], p[1], p[2], p[3], 1.5f);
    }
    screw (g, 52.0f, 470.0f, 0.5f);
    screw (g, 716.0f, 470.0f, 1.9f);
    screw (g, 44.0f, 170.0f, 1.1f);
}
