#include "MeterPanel.h"
#include "BabyLookAndFeel.h"
using namespace babyair::ui;

MeterPanel::MeterPanel (BabyAirAudioProcessor& p) : proc (p)
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    // restore the (non-automatable) view mode saved with the project
    showInput = (bool) proc.apvts.state.getProperty ("meterShowsInput", false);
    lastTick = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
}

void MeterPanel::visibilityChanged()
{
    // Do no graphics work while the editor is hidden: wake up slowly, repaint only when visible
    startTimerHz (isShowing() ? 60 : 4);
}

void MeterPanel::mouseDown (const juce::MouseEvent&)
{
    showInput = ! showInput;
    proc.apvts.state.setProperty ("meterShowsInput", showInput, nullptr);
    barDb[0] = barDb[1] = -120.0f; peakHold = -120.0f;
    repaint();
}

void MeterPanel::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.001, 0.2, now - lastTick);
    lastTick = now;

    if (! isShowing()) { startTimerHz (4); return; }
    if (getTimerInterval() > 100) startTimerHz (60);

    if (! manual)
    {
        auto& m = proc.meters;
        const float pkL = (showInput ? m.inPeak[0] : m.outPeak[0]).exchange (0.0f, std::memory_order_relaxed);
        const float pkR = (showInput ? m.inPeak[1] : m.outPeak[1]).exchange (0.0f, std::memory_order_relaxed);
        // keep the unused side from accumulating forever
        (showInput ? m.outPeak[0] : m.inPeak[0]).store (0.0f, std::memory_order_relaxed);
        (showInput ? m.outPeak[1] : m.inPeak[1]).store (0.0f, std::memory_order_relaxed);

        const float fall = 26.0f * dt; // bar ballistics: instant attack, 26 dB/s release
        const float tgt[2] = { toDb (pkL), toDb (pkR) };
        for (int c = 0; c < 2; ++c)
            barDb[c] = tgt[c] > barDb[c] ? tgt[c] : std::max (tgt[c], barDb[c] - fall);

        const float pk = std::max (tgt[0], tgt[1]);
        if (pk >= peakHold)                     { peakHold = pk; peakHoldTime = now; }                   // new peak: hold it
        else if (now - peakHoldTime > 2.0)      { peakHold = std::max (pk, peakHold - 18.0f * dt); }     // after 2 s fall at 18 dB/s
        rmsDbShown = toDb ((showInput ? m.inRms : m.outRms).load (std::memory_order_relaxed));

        if (m.outClipEvent.exchange (false, std::memory_order_relaxed)) clipTime = now;
        clipLit = (now - clipTime) < 1.5; // CLIP stays lit 1.5 s after the last clip
    }
    repaint();
}

void MeterPanel::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 86.0f;
    g.addTransform (juce::AffineTransform::scale (s));

    // recessed glass window
    juce::Rectangle<float> win (1.0f, 1.0f, 84.0f, 202.0f);
    g.setColour (juce::Colour (0xff050605)); g.fillRoundedRectangle (win, 16.0f);
    juce::ColourGradient glass (juce::Colour (0xff121513), 0, 0, juce::Colour (0xff0a0b0a), 0, 202, false);
    g.setGradientFill (glass); g.fillRoundedRectangle (win.reduced (2.0f), 14.0f);
    g.setColour (juce::Colour (0x30ffffff)); g.drawRoundedRectangle (win.reduced (1.0f), 15.0f, 1.0f);

    const auto txt = colours::text;
    auto fmt = [] (float db) { return db <= -99.0f ? juce::String ("-inf") : juce::String (db, 1); };

    // IN/OUT tag
    g.setColour (colours::limeSoft.withAlpha (0.75f));
    g.setFont (BabyLookAndFeel::boldFont (8.5f, 0.12f));
    g.drawText (showInput ? "IN" : "OUT", juce::Rectangle<float> (0, 6, 86, 10), juce::Justification::centred, false);

    // PEAK
    g.setColour (txt.withAlpha (0.65f)); g.setFont (BabyLookAndFeel::boldFont (10.0f, 0.08f));
    g.drawText ("PEAK", juce::Rectangle<float> (0, 17, 86, 12), juce::Justification::centred, false);
    g.setColour (peakHold > -0.05f ? colours::clipRed : txt); g.setFont (BabyLookAndFeel::boldFont (14.0f, 0.0f));
    g.drawText (fmt (peakHold) + " dB", juce::Rectangle<float> (0, 29, 86, 17), juce::Justification::centred, false);
    g.setColour (juce::Colour (0x22ffffff)); g.drawLine (12, 50, 74, 50, 1.0f);

    // RMS
    g.setColour (txt.withAlpha (0.65f)); g.setFont (BabyLookAndFeel::boldFont (10.0f, 0.08f));
    g.drawText ("RMS", juce::Rectangle<float> (0, 54, 86, 12), juce::Justification::centred, false);
    g.setColour (txt); g.setFont (BabyLookAndFeel::boldFont (14.0f, 0.0f));
    g.drawText (fmt (rmsDbShown) + " dB", juce::Rectangle<float> (0, 66, 86, 17), juce::Justification::centred, false);
    g.setColour (juce::Colour (0x22ffffff)); g.drawLine (12, 88, 74, 88, 1.0f);

    // L / R segmented bars: -54 .. 0 dBFS
    static constexpr float thresholds[kSegments] = { -54, -42, -33, -26, -20, -15, -11, -7, -3.5f, -0.5f };
    const float barW = 17.0f, barTop = 96.0f, segH = 6.6f, gap = 2.2f;
    for (int c = 0; c < 2; ++c)
    {
        const float bx = c == 0 ? 20.0f : 49.0f;
        for (int i = 0; i < kSegments; ++i)
        {
            const int idx = kSegments - 1 - i; // top segment = loudest
            const float yy = barTop + (float) i * (segH + gap);
            const bool lit = barDb[c] >= thresholds[idx];
            juce::Colour col = idx >= 9 ? colours::clipRed : idx >= 8 ? juce::Colour (0xffe8f23a) : idx >= 6 ? juce::Colour (0xffc7f53a) : colours::lime;
            g.setColour (lit ? col : juce::Colour (0xff1c231a));
            g.fillRoundedRectangle (bx, yy, barW, segH, 2.2f);
            if (lit) { g.setColour (col.withAlpha (0.20f)); g.fillRoundedRectangle (bx - 1.5f, yy - 1.2f, barW + 3.0f, segH + 2.4f, 3.0f); }
        }
    }

    // CLIP lamp
    g.setColour (txt.withAlpha (0.65f)); g.setFont (BabyLookAndFeel::boldFont (10.0f, 0.08f));
    g.drawText ("CLIP", juce::Rectangle<float> (6, 188, 50, 12), juce::Justification::centredRight, false);
    const float lx = 66.0f, ly = 194.0f;
    if (clipLit) { g.setColour (colours::clipRed.withAlpha (0.28f)); g.fillEllipse (lx - 9, ly - 9, 18, 18); }
    juce::ColourGradient led (clipLit ? juce::Colour (0xffff9a8c) : juce::Colour (0xff3b5a2d), lx - 2, ly - 3, clipLit ? colours::clipRed : juce::Colour (0xff1c2e14), lx + 3, ly + 4, true);
    g.setGradientFill (led); g.fillEllipse (lx - 4.5f, ly - 4.5f, 9.0f, 9.0f);
}
