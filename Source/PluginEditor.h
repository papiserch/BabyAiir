#pragma once
#include "PluginProcessor.h"
#include "GUI/BabyLookAndFeel.h"
#include "GUI/DeviceArt.h"
#include "GUI/MascotView.h"
#include "GUI/MeterPanel.h"

// BABY AIR editor: a physical-looking toy. Owns only GUI objects; it talks to the audio side through
// the parameter tree (host-safe) and the lock-free meter/activity atomics (read only).
class BabyAirAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit BabyAirAudioProcessorEditor (BabyAirAudioProcessor&);
    ~BabyAirAudioProcessorEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

    MascotView& getMascot() { return mascot; }
    MeterPanel& getMeters() { return meters; }
    LinkableSlider& getMidKnob() { return midKnob; }
    LinkableSlider& getHighKnob() { return highKnob; }
    juce::ToggleButton& getLinkButton() { return linkButton; }

private:
    // LINK: moves the partner knob so the *relationship* between MID AIR and HIGH AIR is kept
    // (ratio, or offset when one of them is near 0). The two knobs are never forced to be equal.
    void captureLinkRelation();
    void applyLink (LinkableSlider& moved, LinkableSlider& partner);

    BabyAirAudioProcessor& proc;
    BabyLookAndFeel laf;

    DeviceArt art;
    MascotView mascot;
    MeterPanel meters;

    LinkableSlider midKnob, highKnob, trimKnob;
    juce::ToggleButton linkButton, bypassButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> midAtt, highAtt, trimAtt;
    std::unique_ptr<ButtonAttachment> linkAtt, bypassAtt;

    bool useRatio = true, applyingLink = false;
    double linkRatio = 1.0, linkOffset = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyAirAudioProcessorEditor)
};
