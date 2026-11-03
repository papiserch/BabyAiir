#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Layout.h"

// The physical "toy": moulded cream shell, lime bumpers, black glossy logo cap, recessed panels,
// stickers, scuffs and the engraved labels. It is static artwork, so it is rendered once per size
// into a cached image (no per-frame cost) and never touches the audio thread.
class DeviceArt : public juce::Component
{
public:
    DeviceArt() { setOpaque (true); setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics&) override;

    // Draws the whole device on the logical 768 x 512 canvas (the caller sets the scale transform).
    static void paintDevice (juce::Graphics&);

private:
    juce::Image cache;
    float cachedScale = 0.0f;
};
