#include "MascotView.h"
#include "BabyLookAndFeel.h"
using namespace babyair;
using namespace babyair::ui;
using juce::Colour;
using juce::Path;

namespace
{
const Colour skin      { 0xfff3e6d3 };
const Colour skinShade { 0xffe0cdb3 };
const Colour skinLine  { 0xffb59d82 };
const Colour ink       { 0xff2a2220 };
const Colour hoodieA   { 0xff4a8a30 };
const Colour hoodieB   { 0xff2c5a22 };
const Colour hornA     { 0xff4f6f2c };
const Colour hornB     { 0xff22371a };
constexpr float kLedgeY = 151.0f; // the figure is "behind" the device's top rim

Path heartPath()
{
    Path p;
    p.startNewSubPath (0.0f, 0.95f);
    p.cubicTo (-1.35f, 0.05f, -0.95f, -0.95f, 0.0f, -0.38f);
    p.cubicTo (0.95f, -0.95f, 1.35f, 0.05f, 0.0f, 0.95f);
    p.closeSubPath();
    return p;
}
inline float rad (float deg) { return deg * juce::MathConstants<float>::pi / 180.0f; }
}

MascotView::MascotView (BabyAirAudioProcessor& p) : proc (p)
{
    setOpaque (false);
    setInterceptsMouseClicks (true, false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    lastTick = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
}

void MascotView::visibilityChanged() { startTimerHz (isShowing() ? 60 : 4); }

void MascotView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.001, 0.25, now - lastTick);
    lastTick = now;

    if (! isShowing()) { startTimerHz (4); return; }   // editor hidden: no graphics work at all
    if (getTimerInterval() > 100) startTimerHz (60);
    if (manual) { repaint(); return; }

    // Lock-free reads from the audio thread. Nothing is ever written back.
    engine.update (dt, proc.activity.getActive(), proc.activity.getLevel());

    // Fully asleep and nothing else moving: 30 fps is plenty for the breathing animation
    const bool idle = engine.state == MascotState::Sleeping && engine.hearts.get().empty();
    const int wanted = idle ? 30 : 60;
    if (std::abs (getTimerInterval() - 1000 / wanted) > 3) startTimerHz (wanted);
    repaint();
}

void MascotView::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 300.0f;
    g.addTransform (juce::AffineTransform::scale (s));
    const auto& P = engine.pose;
    const float bob = -P.bounce + P.breath * 1.3f;

    // soft contact shadow on the rim
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillEllipse (60.0f, kLedgeY - 6.0f, 180.0f, 12.0f);

    // everything except the paws sits behind the rim line
    g.saveState();
    g.reduceClipRegion (juce::Rectangle<float> (0.0f, 0.0f, 300.0f, kLedgeY).toNearestInt());
    drawTail (g, bob);
    drawWing (g, bob, true);
    drawWing (g, bob, false);
    drawHoodie (g, bob);
    drawHead (g, bob);
    g.restoreState();

    drawPaws (g, bob);
    drawHearts (g);
    drawZzz (g);
}

// ----------------------------------------------------------------------------------------------
void MascotView::drawTail (juce::Graphics& g, float bob) const
{
    const auto& P = engine.pose;
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (rad (P.tailAmp * std::sin (P.tailPhase)) * 1.0f, 0.0f, 0.0f)
                        .translated (206.0f, 140.0f + bob));
    Path stem;
    stem.startNewSubPath (0.0f, 0.0f);
    stem.cubicTo (20.0f, 4.0f, 40.0f, -12.0f, 33.0f, -34.0f);
    stem.cubicTo (29.0f, -47.0f, 40.0f, -54.0f, 46.0f, -64.0f);
    g.setColour (juce::Colour (0xff1c3a17));
    g.strokePath (stem, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff3f7a2c));
    g.strokePath (stem, juce::PathStrokeType (4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::lime.withAlpha (0.55f));
    g.strokePath (stem, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // arrow-head tip
    Path tip; tip.startNewSubPath (50.0f, -76.0f); tip.lineTo (38.0f, -64.0f); tip.lineTo (44.0f, -62.0f);
    tip.lineTo (50.0f, -50.0f); tip.lineTo (53.0f, -60.0f); tip.lineTo (60.0f, -66.0f); tip.closeSubPath();
    g.setColour (colours::lime.withAlpha (0.22f)); g.fillPath (tip, juce::AffineTransform::scale (1.35f, 1.35f, 48.0f, -64.0f));
    g.setColour (juce::Colour (0xff6fd04a)); g.fillPath (tip);
    g.setColour (juce::Colour (0xff1c3a17)); g.strokePath (tip, juce::PathStrokeType (0.9f));
    g.restoreState();
}

void MascotView::drawWing (juce::Graphics& g, float bob, bool left) const
{
    const auto& P = engine.pose;
    const float flap = P.wingAmp * (0.5f + 0.5f * std::sin (P.wingPhase + (left ? 0.0f : 0.35f)));
    const float angle = rad (P.wingBase - flap);        // positive = drooping, negative = raised

    g.saveState();
    juce::AffineTransform t = juce::AffineTransform::rotation (angle);
    if (left) t = t.scaled (-1.0f, 1.0f);
    g.addTransform (t.translated (left ? 112.0f : 188.0f, 100.0f + bob));

    Path w;
    w.startNewSubPath (0.0f, -4.0f);
    w.cubicTo (18.0f, -36.0f, 56.0f, -50.0f, 86.0f, -32.0f);
    w.cubicTo (95.0f, -26.0f, 96.0f, -14.0f, 90.0f, -8.0f);
    w.cubicTo (90.0f, 0.0f, 84.0f, 6.0f, 76.0f, 4.0f);
    w.cubicTo (76.0f, 13.0f, 68.0f, 19.0f, 60.0f, 15.0f);
    w.cubicTo (58.0f, 23.0f, 48.0f, 27.0f, 40.0f, 21.0f);
    w.cubicTo (36.0f, 27.0f, 24.0f, 27.0f, 17.0f, 17.0f);
    w.cubicTo (9.0f, 14.0f, 2.0f, 9.0f, 0.0f, 4.0f);
    w.closeSubPath();

    g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillPath (w, juce::AffineTransform::translation (1.5f, 3.0f));
    juce::ColourGradient grad (juce::Colour (0xfffffdf6), 0.0f, -30.0f, juce::Colour (0xffd9ead0), 90.0f, 6.0f, false);
    g.setGradientFill (grad); g.fillPath (w);
    // feather veins
    g.setColour (juce::Colour (0x40708a60));
    for (int i = 0; i < 4; ++i)
    {
        const float yy = -18.0f + (float) i * 9.0f;
        Path v; v.startNewSubPath (8.0f, -2.0f + (float) i * 3.0f); v.quadraticTo (46.0f, yy - 6.0f, 84.0f - (float) i * 7.0f, yy + 4.0f);
        g.strokePath (v, juce::PathStrokeType (1.0f));
    }
    g.setColour (juce::Colour (0xff9fb890)); g.strokePath (w, juce::PathStrokeType (1.1f));
    // lime-tinted tip
    g.setColour (colours::lime.withAlpha (0.18f)); g.fillEllipse (66.0f, -30.0f, 26.0f, 22.0f);
    g.restoreState();
}

void MascotView::drawHoodie (juce::Graphics& g, float bob) const
{
    juce::ColourGradient grad (hoodieA, 150.0f, 112.0f + bob, hoodieB, 150.0f, 160.0f + bob, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (82.0f, 112.0f + bob, 136.0f, 70.0f, 30.0f);
    g.setColour (juce::Colour (0xff1c3a17)); g.drawRoundedRectangle (82.0f, 112.0f + bob, 136.0f, 70.0f, 30.0f, 1.2f);
    // hood collar + strings
    Path collar; collar.startNewSubPath (112.0f, 120.0f + bob); collar.quadraticTo (150.0f, 146.0f + bob, 188.0f, 120.0f + bob);
    g.setColour (juce::Colour (0xff6fc04a)); g.strokePath (collar, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xfff1ead8));
    g.drawLine (138.0f, 134.0f + bob, 136.0f, 150.0f + bob, 1.6f); g.drawLine (162.0f, 134.0f + bob, 164.0f, 150.0f + bob, 1.6f);
}

void MascotView::drawEye (juce::Graphics& g, float ex, float ey, float side) const
{
    const auto& P = engine.pose;
    const float w = 15.5f;
    const float h = 12.0f * (0.92f + 0.12f * P.sparkle);
    const float open = juce::jlimit (0.0f, 1.0f, P.eyeOpen);

    if (open < 0.07f) // eyelids closed: peaceful lash line
    {
        Path l; l.startNewSubPath (ex - w * 0.95f, ey - 1.0f); l.quadraticTo (ex, ey + 9.0f, ex + w * 0.95f, ey - 1.0f);
        g.setColour (ink); g.strokePath (l, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (int i = 0; i < 3; ++i)
        {
            const float lx = ex + side * (w * 0.62f + (float) i * 2.8f);
            g.drawLine (lx, ey + 4.2f - (float) i * 0.8f, lx + side * 2.6f, ey + 7.2f - (float) i * 0.6f, 1.5f);
        }
        return;
    }

    Path almond; almond.addEllipse (ex - w, ey - h, w * 2, h * 2);

    g.saveState();
    g.reduceClipRegion (almond);
    // sclera
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffffcf3), ex, ey - h, juce::Colour (0xffe6dcc8), ex, ey + h, false));
    g.fillRect (ex - w, ey - h, w * 2, h * 2);
    // iris
    const float ix = ex + P.lookX * 3.0f, iy = ey + 1.0f, ir = 10.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb6ff7a), ix, iy - 3.0f, juce::Colour (0xff2e7a2b), ix, iy + ir, true));
    g.fillEllipse (ix - ir, iy - ir, ir * 2, ir * 2);
    g.setColour (juce::Colour (0xff17411a)); g.drawEllipse (ix - ir, iy - ir, ir * 2, ir * 2, 1.4f);
    const float pr = 4.4f + 1.4f * P.sparkle;
    g.setColour (ink); g.fillEllipse (ix - pr, iy - pr, pr * 2, pr * 2);
    g.setColour (juce::Colours::white); g.fillEllipse (ix - 5.2f, iy - 6.0f, 4.6f, 4.6f); g.fillEllipse (ix + 2.4f, iy + 2.2f, 2.4f, 2.4f);
    if (P.sparkle > 0.3f) // excited glint
    {
        const float k = P.sparkle * 3.4f; Path st;
        st.startNewSubPath (ix + 4.5f, iy - 2.0f - k); st.lineTo (ix + 5.6f, iy - 3.4f); st.lineTo (ix + 4.5f + k, iy - 2.0f);
        st.lineTo (ix + 5.6f, iy - 0.6f); st.lineTo (ix + 4.5f, iy + k - 2.0f); st.lineTo (ix + 3.4f, iy - 0.6f);
        st.lineTo (ix + 4.5f - k, iy - 2.0f); st.lineTo (ix + 3.4f, iy - 3.4f); st.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.95f)); g.fillPath (st);
    }
    // upper eyelid (skin colour) - covers the eye down to lidY
    const float lidY = ey - h + (1.0f - open) * (h * 2.0f) * 1.04f;
    Path lid; lid.startNewSubPath (ex - w - 3.0f, ey - h - 6.0f); lid.lineTo (ex + w + 3.0f, ey - h - 6.0f);
    lid.lineTo (ex + w + 3.0f, lidY); lid.quadraticTo (ex, lidY + 3.4f * open + 1.2f, ex - w - 3.0f, lidY); lid.closeSubPath();
    g.setColour (skin); g.fillPath (lid);
    // lower lid pushed up by a big smile (happy squint)
    const float squint = juce::jlimit (0.0f, 1.0f, (P.smile - 0.2f)) * 0.30f * (1.0f - P.sparkle * 0.7f);
    if (squint > 0.01f)
    {
        const float cy2 = ey + h - squint * h * 1.6f;
        Path low; low.startNewSubPath (ex - w - 3.0f, ey + h + 6.0f); low.lineTo (ex - w - 3.0f, cy2 + 2.0f);
        low.quadraticTo (ex, cy2 - 3.5f, ex + w + 3.0f, cy2 + 2.0f); low.lineTo (ex + w + 3.0f, ey + h + 6.0f); low.closeSubPath();
        g.setColour (skin); g.fillPath (low);
    }
    g.restoreState();

    // eyeliner along the lid + lashes at the outer corner
    Path line; line.startNewSubPath (ex - w * 1.02f, juce::jmax (lidY, ey - h) + 0.5f);
    line.quadraticTo (ex, juce::jmax (lidY, ey - h) + 3.4f * open + 0.4f, ex + w * 1.02f, juce::jmax (lidY, ey - h) + 0.5f);
    g.setColour (ink); g.strokePath (line, juce::PathStrokeType (2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float oy = juce::jmax (lidY, ey - h);
    g.drawLine (ex + side * w * 0.95f, oy + 0.5f, ex + side * (w + 4.2f), oy - 2.6f, 1.7f);
    g.drawLine (ex + side * w * 0.78f, oy + 0.2f, ex + side * (w + 2.4f), oy - 4.0f, 1.5f);
    g.setColour (ink.withAlpha (0.45f)); g.drawEllipse (ex - w, ey - h, w * 2, h * 2, 0.8f);
}

void MascotView::drawBrow (juce::Graphics& g, float ex, float by, float side) const
{
    const auto& P = engine.pose;
    const float extra = side > 0 ? 0.22f : 0.0f; // mischievous: right brow a little higher
    const float raise = P.browRaise + extra * (P.eyeOpen > 0.3f ? 1.0f : 0.0f);
    const float tilt  = P.browTilt;
    const float xIn = ex - side * 9.0f, xOut = ex + side * 17.0f;
    const float yIn = by + 1.0f + tilt * 2.6f - raise * 3.0f;
    const float yOut = by - tilt * 2.6f - raise * 4.6f + (1.0f - P.eyeOpen) * 3.0f;
    Path b; b.startNewSubPath (xIn, yIn); b.quadraticTo (ex + side * 4.0f, juce::jmin (yIn, yOut) - 2.4f, xOut, yOut);
    g.setColour (juce::Colour (0xff7a6a58)); g.strokePath (b, juce::PathStrokeType (2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void MascotView::drawMouth (juce::Graphics& g, float x, float y) const
{
    const auto& P = engine.pose;
    const float sm = P.smile, op = P.mouthOpen;
    const float w = 8.0f + 6.0f * juce::jmax (0.0f, sm) + 2.0f * op;

    if (op < 0.06f)
    {
        Path m; m.startNewSubPath (x - w, y); m.quadraticTo (x, y + 8.0f * sm, x + w, y);
        g.setColour (juce::Colour (0xff6b3a32)); g.strokePath (m, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (sm > 0.45f) // little fang while smirking
        {
            Path f; f.startNewSubPath (x - w * 0.55f, y + 2.2f * sm); f.lineTo (x - w * 0.55f + 3.2f, y + 2.4f * sm + 0.2f); f.lineTo (x - w * 0.55f + 1.4f, y + 2.4f * sm + 5.0f); f.closeSubPath();
            g.setColour (juce::Colours::white); g.fillPath (f); g.setColour (juce::Colour (0xff6b3a32)); g.strokePath (f, juce::PathStrokeType (0.7f));
        }
        return;
    }
    // open mouth: top lip curve + deep lower bowl
    const float depth = 4.0f + 11.0f * op;
    Path m; m.startNewSubPath (x - w, y);
    m.quadraticTo (x, y + 7.0f * sm - 1.0f, x + w, y);
    m.quadraticTo (x + w * 0.5f, y + depth * 1.5f, x, y + depth * 1.5f - 1.0f);
    m.quadraticTo (x - w * 0.5f, y + depth * 1.5f, x - w, y); m.closeSubPath();
    g.setColour (juce::Colour (0xff5a1e22)); g.fillPath (m);
    g.saveState(); g.reduceClipRegion (m);
    g.setColour (juce::Colour (0xffe8768a).withAlpha (0.9f)); g.fillEllipse (x - w * 0.5f, y + depth * 0.7f, w, depth * 0.9f); // tongue
    g.restoreState();
    g.setColour (juce::Colour (0xff3a1416)); g.strokePath (m, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
    Path f; f.startNewSubPath (x - w * 0.62f, y + 0.6f); f.lineTo (x - w * 0.62f + 3.6f, y + 0.9f); f.lineTo (x - w * 0.62f + 1.6f, y + 5.6f); f.closeSubPath();
    g.setColour (juce::Colours::white); g.fillPath (f);
}

void MascotView::drawHead (juce::Graphics& g, float bob) const
{
    const auto& P = engine.pose;
    const float breathScale = 1.0f + P.breath * 0.010f;
    g.saveState();
    g.addTransform (juce::AffineTransform::scale (breathScale, breathScale)
                        .rotated (P.headTilt)
                        .translated (150.0f, 86.0f + bob - P.headLift));

    // ---- horns (behind the head) with ridges
    auto horn = [&] (bool left, float scale, float rot)
    {
        g.saveState();
        juce::AffineTransform t = juce::AffineTransform::scale (scale).rotated (rot);
        if (left) t = t.scaled (-1.0f, 1.0f);
        g.addTransform (t);
        Path h; h.startNewSubPath (26.0f, -42.0f);
        h.cubicTo (30.0f, -62.0f, 44.0f, -76.0f, 66.0f, -82.0f);
        h.cubicTo (58.0f, -68.0f, 56.0f, -54.0f, 54.0f, -38.0f); h.closeSubPath();
        g.setGradientFill (juce::ColourGradient (hornA, 30.0f, -80.0f, hornB, 54.0f, -38.0f, false)); g.fillPath (h);
        g.setColour (juce::Colour (0xff142410)); g.strokePath (h, juce::PathStrokeType (1.1f));
        g.setColour (juce::Colour (0x66b6f080));
        for (int i = 0; i < 3; ++i) { const float k = 0.28f + 0.2f * (float) i; g.drawLine (30.0f + 24.0f * k, -52.0f - 18.0f * k - 4.0f, 40.0f + 20.0f * k, -46.0f - 18.0f * k + 4.0f, 1.2f); }
        g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawLine (36.0f, -58.0f, 52.0f, -74.0f, 1.3f);
        g.restoreState();
    };
    horn (true, 0.92f, -0.05f);
    horn (false, 1.0f, 0.0f);

    // ---- head
    Path head; head.addRoundedRectangle (-70.0f, -52.0f, 140.0f, 106.0f, 52.0f);
    g.setColour (juce::Colours::black.withAlpha (0.16f)); g.fillPath (head, juce::AffineTransform::translation (0.0f, 3.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff9ee), -30.0f, -40.0f, skinShade, 50.0f, 50.0f, true));
    g.fillPath (head);
    g.setColour (skinLine.withAlpha (0.65f)); g.strokePath (head, juce::PathStrokeType (1.3f));

    // ---- hair tuft + side locks
    g.setColour (juce::Colour (0xfffbf7ec));
    for (int i = 0; i < 6; ++i)
    {
        const float x0 = -26.0f + (float) i * 10.0f, hgt = 12.0f + (float) ((i * 5) % 7);
        Path t; t.startNewSubPath (x0 - 5.0f, -47.0f); t.quadraticTo (x0 + ((i % 2) ? 5.0f : -5.0f), -47.0f - hgt, x0 + 9.0f * ((i % 2) ? 1.0f : 0.6f), -50.0f - hgt * 0.8f);
        t.quadraticTo (x0 + 6.0f, -46.0f, x0 + 5.0f, -45.0f); t.closeSubPath();
        g.fillPath (t); g.setColour (skinLine.withAlpha (0.5f)); g.strokePath (t, juce::PathStrokeType (0.8f)); g.setColour (juce::Colour (0xfffbf7ec));
    }

    // ---- halo
    {
        const float hy = -76.0f + std::sin (engine.time * 1.8f) * 1.6f;
        Path ring; ring.addEllipse (-27.0f, hy - 6.5f, 54.0f, 13.0f);
        g.saveState(); g.addTransform (juce::AffineTransform::rotation (-0.12f, 0.0f, hy));
        const float glow = 0.4f + 0.6f * P.haloGlow;
        g.setColour (colours::lime.withAlpha (0.10f * glow)); g.strokePath (ring, juce::PathStrokeType (11.0f));
        g.setColour (colours::lime.withAlpha (0.24f * glow)); g.strokePath (ring, juce::PathStrokeType (6.5f));
        g.setColour (colours::limeSoft.withAlpha (0.55f + 0.45f * glow)); g.strokePath (ring, juce::PathStrokeType (2.6f));
        g.restoreState();
    }

    // ---- cheeks / band-aid
    const float bl = juce::jlimit (0.0f, 1.0f, P.blush);
    g.setColour (juce::Colour (0xffef9f7a).withAlpha (0.18f + 0.34f * bl));
    g.fillEllipse (-58.0f, 17.0f, 24.0f, 14.0f); g.fillEllipse (34.0f, 17.0f, 24.0f, 14.0f);
    {
        g.saveState(); g.addTransform (juce::AffineTransform::rotation (-0.5f, -26.0f, -32.0f));
        g.setColour (juce::Colour (0xffdba479)); g.fillRoundedRectangle (-40.0f, -37.0f, 28.0f, 11.0f, 3.0f);
        g.setColour (juce::Colour (0xfff2cfa3)); g.fillRoundedRectangle (-33.0f, -37.0f, 14.0f, 11.0f, 1.5f);
        g.setColour (juce::Colour (0x558a5a30)); g.fillEllipse (-30.0f, -33.0f, 1.6f, 1.6f); g.fillEllipse (-25.0f, -33.0f, 1.6f, 1.6f); g.fillEllipse (-30.0f, -29.5f, 1.6f, 1.6f);
        g.setColour (juce::Colour (0x66603a1a)); g.drawRoundedRectangle (-40.0f, -37.0f, 28.0f, 11.0f, 3.0f, 0.8f);
        g.restoreState();
    }

    // ---- face
    drawBrow (g, -27.0f, -15.0f, -1.0f);
    drawBrow (g,  27.0f, -15.0f,  1.0f);
    drawEye (g, -27.0f, 5.0f, -1.0f);
    drawEye (g,  27.0f, 5.0f,  1.0f);
    g.setColour (juce::Colour (0xffd6b293)); g.fillEllipse (-2.2f, 17.0f, 4.4f, 3.4f);   // nose
    drawMouth (g, 0.0f, 31.0f);

    g.restoreState();
}

void MascotView::drawPaws (juce::Graphics& g, float bob) const
{
    const auto& P = engine.pose;
    for (int i = 0; i < 2; ++i)
    {
        const float x = i == 0 ? 112.0f : 188.0f;
        const float pat = P.pawPat * std::max (0.0f, std::sin (P.wingPhase * 0.5f + (float) i * 3.14f)) * 2.2f;
        const float y = 146.0f + bob * 0.25f - pat;
        // sleeve
        g.setGradientFill (juce::ColourGradient (hoodieA, x, y + 6.0f, hoodieB, x, y + 26.0f, false));
        g.fillRoundedRectangle (x - 17.0f, y + 6.0f, 34.0f, 24.0f, 11.0f);
        g.setColour (juce::Colour (0xff1c3a17)); g.drawRoundedRectangle (x - 17.0f, y + 6.0f, 34.0f, 24.0f, 11.0f, 1.0f);
        // mitt
        g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillEllipse (x - 14.0f, y - 1.0f, 28.0f, 17.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff7ea), x - 8.0f, y - 6.0f, skinShade, x + 8.0f, y + 12.0f, false));
        g.fillEllipse (x - 14.0f, y - 4.0f, 28.0f, 18.0f);
        g.setColour (skinLine.withAlpha (0.8f)); g.drawEllipse (x - 14.0f, y - 4.0f, 28.0f, 18.0f, 1.0f);
        for (int k = -1; k <= 1; ++k) g.drawLine (x + (float) k * 6.0f, y + 1.0f, x + (float) k * 6.5f, y + 5.5f, 0.9f);
    }
}

void MascotView::drawHearts (juce::Graphics& g) const
{
    static const Path heart = heartPath();
    for (const auto& h : engine.hearts.get())
    {
        const float a = HeartSystem::alphaOf (h);
        if (a <= 0.01f) continue;
        const auto xf = juce::AffineTransform::scale (h.size * 0.5f).rotated (h.rot).translated (h.x, h.y);
        g.setColour (colours::lime.withAlpha (0.14f * a));  g.fillPath (heart, juce::AffineTransform::scale (h.size * 0.5f * 1.65f).rotated (h.rot).translated (h.x, h.y));
        g.setGradientFill (juce::ColourGradient (colours::limeSoft.withAlpha (0.92f * a), h.x - h.size * 0.2f, h.y - h.size * 0.4f,
                                                 colours::green.withAlpha (0.62f * a), h.x + h.size * 0.3f, h.y + h.size * 0.5f, false));
        g.fillPath (heart, xf);
        g.setColour (juce::Colours::white.withAlpha (0.55f * a)); g.strokePath (heart, juce::PathStrokeType (0.08f), xf);
        g.setColour (juce::Colours::white.withAlpha (0.5f * a));
        g.fillEllipse (h.x - h.size * 0.36f, h.y - h.size * 0.30f, h.size * 0.17f, h.size * 0.11f);
    }
}

void MascotView::drawZzz (juce::Graphics& g) const
{
    const float a = engine.pose.zAlpha;
    if (a < 0.02f) return;
    for (int i = 0; i < 3; ++i)
    {
        const float ph = std::fmod (engine.time * 0.45f + (float) i * 0.33f, 1.0f);
        const float x = 214.0f + ph * 22.0f + (float) i * 3.0f, y = 62.0f - ph * 44.0f;
        const float alpha = a * std::sin (ph * 3.14159f);
        g.setColour (colours::limeSoft.withAlpha (0.8f * alpha));
        g.setFont (BabyLookAndFeel::boldFont (11.0f + ph * 9.0f, 0.0f));
        g.drawText ("z", juce::Rectangle<float> (x - 10.0f, y - 10.0f, 20.0f, 20.0f), juce::Justification::centred, false);
    }
}
