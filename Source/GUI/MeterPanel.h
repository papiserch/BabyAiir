#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Layout.h"

// Real LED meter panel: PEAK / RMS read-outs, L/R segmented bars and CLIP lamp.
// Clicking the panel switches between OUTPUT and INPUT metering (small "OUT"/"IN" tag).
// All values come from the audio thread through lock-free atomics (SharedMeters).
class MeterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MeterPanel (BabyAirAudioProcessor& p);
    ~MeterPanel() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    // Test hook: drive the meter with explicit values instead of reading the processor
    void setManualValues (float peakDbL, float peakDbR, float peakHoldDb, float rmsDb, bool clip)
    {
        manual = true; barDb[0] = peakDbL; barDb[1] = peakDbR; peakHold = peakHoldDb; rmsDbShown = rmsDb; clipLit = clip; repaint();
    }

    static constexpr int kSegments = 10;

private:
    void timerCallback() override;
    static float toDb (float lin) noexcept { return lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -120.0f; }

    BabyAirAudioProcessor& proc;
    bool showInput = false;
    bool manual = false;

    float barDb[2] { -120.0f, -120.0f };
    float peakHold = -120.0f, rmsDbShown = -120.0f;
    double peakHoldTime = 0.0, clipTime = -10.0;
    bool clipLit = false;
    double lastTick = 0.0;
};
