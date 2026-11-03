#pragma once
// BABY AIR by PAPI SERCH - parameter definitions.
// Parameter IDs are part of the saved-project format: DO NOT rename them after release.
#include <juce_audio_processors/juce_audio_processors.h>

namespace babyair::ids
{
inline constexpr const char* midAir  = "midAir";   // 0..100 %
inline constexpr const char* highAir = "highAir";  // 0..100 %
inline constexpr const char* link    = "link";     // bool
inline constexpr const char* trim    = "trim";     // -12..+12 dB
inline constexpr const char* bypass  = "bypass";   // bool
} // namespace babyair::ids

namespace babyair
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using juce::AudioParameterFloat;
    using juce::AudioParameterBool;
    using juce::ParameterID;
    using juce::NormalisableRange;

    constexpr int version = 1; // bump only when adding parameters in future releases

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = juce::AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })
                       .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });

    auto decibels = juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })
                        .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::midAir, version }, "Mid Air",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::highAir, version }, "High Air",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, percent));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { ids::link, version }, "Link", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::trim, version }, "Trim",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, decibels));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { ids::bypass, version }, "Bypass", false));
    return layout;
}
} // namespace babyair
