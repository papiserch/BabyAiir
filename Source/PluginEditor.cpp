#include "PluginEditor.h"
using namespace babyair;
using namespace babyair::ui;

BabyAirAudioProcessorEditor::BabyAirAudioProcessorEditor (BabyAirAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), mascot (p), meters (p)
{
    setLookAndFeel (&laf);
    setOpaque (true);

    addAndMakeVisible (art);
    addAndMakeVisible (meters);
    addAndMakeVisible (midKnob);
    addAndMakeVisible (highKnob);
    addAndMakeVisible (trimKnob);
    addAndMakeVisible (linkButton);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (mascot);          // last: the Baby Demon (and his hearts) draw above everything

    trimKnob.isSmall = true;
    linkButton.setComponentID ("link");
    bypassButton.setComponentID ("bypass");
    linkButton.setClickingTogglesState (true);
    bypassButton.setClickingTogglesState (true);
    linkButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    bypassButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    linkButton.setTooltip ("LINK: MID AIR and HIGH AIR move together, keeping their relationship");
    bypassButton.setTooltip ("BYPASS");

    // double click = back to default
    midKnob.setDoubleClickReturnValue (true, 40.0);
    highKnob.setDoubleClickReturnValue (true, 50.0);
    trimKnob.setDoubleClickReturnValue (true, 0.0);
    midKnob.setTitle ("Mid Air");  highKnob.setTitle ("High Air");  trimKnob.setTitle ("Trim");
    linkButton.setTitle ("Link");  bypassButton.setTitle ("Bypass");

    auto& s = proc.apvts;
    midAtt    = std::make_unique<SliderAttachment> (s, ids::midAir,  midKnob);
    highAtt   = std::make_unique<SliderAttachment> (s, ids::highAir, highKnob);
    trimAtt   = std::make_unique<SliderAttachment> (s, ids::trim,    trimKnob);
    linkAtt   = std::make_unique<ButtonAttachment> (s, ids::link,    linkButton);
    bypassAtt = std::make_unique<ButtonAttachment> (s, ids::bypass,  bypassButton);

    captureLinkRelation();

    auto hook = [this] (LinkableSlider& me, LinkableSlider& other)
    {
        me.onValueChange = [this, &me, &other]
        {
            if (applyingLink) return;
            if (linkButton.getToggleState() && me.isUserChanging()) applyLink (me, other);
            else                                                    captureLinkRelation();   // host automation / preset recall / LINK off
        };
    };
    hook (midKnob, highKnob);
    hook (highKnob, midKnob);

    linkButton.onClick = [this] { if (linkButton.getToggleState()) captureLinkRelation(); };

    // restore last window size (a plain, non-automatable property saved with the project)
    const int savedW = (int) proc.apvts.state.getProperty ("uiWidth", kW);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    getConstrainer()->setSizeLimits (kW * 5 / 8, kH * 5 / 8, kW * 2, kH * 2);
    const int w = juce::jlimit (kW * 5 / 8, kW * 2, savedW);
    setSize (w, juce::roundToInt ((float) w * (float) kH / (float) kW));
}

BabyAirAudioProcessorEditor::~BabyAirAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void BabyAirAudioProcessorEditor::captureLinkRelation()
{
    const double m = midKnob.getValue(), h = highKnob.getValue();
    useRatio = (m >= 2.0 && h >= 2.0);
    linkRatio = useRatio ? h / m : 1.0;
    linkOffset = h - m;
}

void BabyAirAudioProcessorEditor::applyLink (LinkableSlider& moved, LinkableSlider& partner)
{
    const bool movedIsMid = (&moved == &midKnob);
    double target;
    if (useRatio) target = movedIsMid ? moved.getValue() * linkRatio : moved.getValue() / linkRatio;
    else          target = movedIsMid ? moved.getValue() + linkOffset : moved.getValue() - linkOffset;
    target = juce::jlimit (partner.getMinimum(), partner.getMaximum(), target);

    const juce::ScopedValueSetter<bool> guard (applyingLink, true);
    partner.setValue (target, juce::sendNotificationSync);   // goes through the attachment -> host automation
}

void BabyAirAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    auto place = [s] (juce::Component& c, juce::Rectangle<int> r)
    {
        c.setBounds (juce::roundToInt ((float) r.getX() * s), juce::roundToInt ((float) r.getY() * s),
                     juce::roundToInt ((float) r.getWidth() * s), juce::roundToInt ((float) r.getHeight() * s));
    };

    art.setBounds (getLocalBounds());
    place (mascot,       mascotRect());
    place (meters,       meterRect());
    place (midKnob,      midKnobRect());
    place (highKnob,     highKnobRect());
    place (trimKnob,     trimKnobRect());
    place (linkButton,   linkRect());
    place (bypassButton, bypassRect());

    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}
