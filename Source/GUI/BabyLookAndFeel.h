#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Layout.h"

// Knob that knows how to link itself to a partner (LINK button). The editor wires the partner.
class LinkableSlider : public juce::Slider
{
public:
    LinkableSlider() { setSliderStyle (RotaryVerticalDrag); setTextBoxStyle (NoTextBox, true, 0, 0); setPopupDisplayEnabled (false, false, nullptr); }

    bool isUserChanging() const noexcept { return userGesture; }
    void setUserGestureForTest (bool b) noexcept { userGesture = b; } // lets tests emulate a mouse drag

    void mouseDown (const juce::MouseEvent& e) override       { userGesture = true;  juce::Slider::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override         { juce::Slider::mouseUp (e); userGesture = false; }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const bool was = userGesture; userGesture = true;
        juce::Slider::mouseWheelMove (e, w);
        userGesture = was;
    }

    bool isSmall = false; // TRIM uses the small style

private:
    bool userGesture = false;
};

class BabyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BabyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    static juce::Font boldFont (float size, float kerning = 0.06f);
};
