#pragma once
// BABY AIR by PAPI SERCH - shared GUI layout (logical units) and palette.
// Everything is designed on a 768 x 512 canvas and scaled uniformly, so the plugin window can be resized.
#include <juce_graphics/juce_graphics.h>

namespace babyair::ui
{
constexpr int kW = 768;
constexpr int kH = 512;

// Component rectangles in logical units
inline juce::Rectangle<int> midKnobRect()   { return { 225 - 80, 268 - 92, 160, 200 }; }
inline juce::Rectangle<int> highKnobRect()  { return { 532 - 80, 268 - 92, 160, 200 }; }
inline juce::Rectangle<int> linkRect()      { return { 377 - 42, 268 - 22, 84, 44 }; }
inline juce::Rectangle<int> trimKnobRect()  { return { 141 - 44, 421 - 36, 88, 82 }; }
inline juce::Rectangle<int> bypassRect()    { return { 636 - 50, 421 - 24, 100, 48 }; }
inline juce::Rectangle<int> meterRect()     { return { 626, 168, 86, 204 }; }
inline juce::Rectangle<int> mascotRect()    { return { 234, 0, 300, 172 }; }

namespace colours
{
    inline const juce::Colour lime      { 0xff9dff3c };
    inline const juce::Colour limeSoft  { 0xffc4ff7a };
    inline const juce::Colour green     { 0xff52c234 };
    inline const juce::Colour greenDark { 0xff2f6b2a };
    inline const juce::Colour military  { 0xff3d5a2a };
    inline const juce::Colour black     { 0xff0b0c0b };
    inline const juce::Colour panel     { 0xff151716 };
    inline const juce::Colour panelHi   { 0xff262a27 };
    inline const juce::Colour cream     { 0xffefe6d2 };
    inline const juce::Colour creamDark { 0xffcfc2a6 };
    inline const juce::Colour creamWarm { 0xfff7f1e3 };
    inline const juce::Colour text      { 0xffe9e4d6 };
    inline const juce::Colour textDim   { 0xff8d9288 };
    inline const juce::Colour amber     { 0xffff9a3c };
    inline const juce::Colour clipRed   { 0xffff4a3a };
}

// 0..1 normalised position of a value on the rotary arc
constexpr float kArcStart = -2.356194490f; // -135 deg (from 12 o'clock, clockwise positive)
constexpr float kArcEnd   =  2.356194490f; // +135 deg
} // namespace babyair::ui
