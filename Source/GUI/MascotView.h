#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "MascotEngine.h"
#include "Layout.h"

// Baby Demon: a layered, vector-drawn collectible figure. Each layer (tail, wings, hoodie, head,
// horns, halo, eyes, eyelids, brows, mouth, paws) is animated independently from MascotEngine::pose.
// The view only READS two atomics from the audio thread (activity flag + level) - it never writes
// anything back, so it cannot affect the sound.
class MascotView : public juce::Component, private juce::Timer
{
public:
    explicit MascotView (BabyAirAudioProcessor& p);
    ~MascotView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { engine.poke(); }
    void visibilityChanged() override;

    babyair::MascotEngine& getEngine() { return engine; }

    // Test hook: stop reading the processor and drive the engine by hand
    void setManualMode (bool shouldBeManual) { manual = shouldBeManual; }

private:
    void timerCallback() override;

    void drawTail (juce::Graphics&, float bob) const;
    void drawWing (juce::Graphics&, float bob, bool left) const;
    void drawHoodie (juce::Graphics&, float bob) const;
    void drawHead (juce::Graphics&, float bob) const;
    void drawEye (juce::Graphics&, float ex, float ey, float side) const;
    void drawBrow (juce::Graphics&, float ex, float by, float side) const;
    void drawMouth (juce::Graphics&, float x, float y) const;
    void drawPaws (juce::Graphics&, float bob) const;
    void drawHearts (juce::Graphics&) const;
    void drawZzz (juce::Graphics&) const;

    BabyAirAudioProcessor& proc;
    babyair::MascotEngine engine;
    bool manual = false;
    double lastTick = 0.0;
};
