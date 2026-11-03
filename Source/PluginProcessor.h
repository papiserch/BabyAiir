#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Parameters.h"
#include "DSP/AirEngine.h"
#include "DSP/ActivityDetector.h"

// Lock-free values shared from the audio thread to the GUI thread.
// The audio thread only WRITES, the GUI only READS (peaks use "max since last read").
struct SharedMeters
{
    std::atomic<float> inPeak[2]  { { 0.0f }, { 0.0f } };   // max |x| since last GUI read
    std::atomic<float> outPeak[2] { { 0.0f }, { 0.0f } };
    std::atomic<float> inRms  { 0.0f };                      // linear, ~300 ms window, channels averaged
    std::atomic<float> outRms { 0.0f };
    std::atomic<bool>  outClipEvent { false };               // set on |out| >= 0 dBFS, cleared by GUI

    static void raiseTo (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};

class BabyAirAudioProcessor : public juce::AudioProcessor
{
public:
    BabyAirAudioProcessor();
    ~BabyAirAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    // Tell the host that OUR Bypass parameter is the plugin's official bypass. Without this the VST3
    // wrapper adds a second, separate "Bypass" parameter, and the DAW's own bypass button would not
    // use the click-free crossfade.
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (babyair::ids::bypass); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BABY AIR"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    SharedMeters meters;
    babyair::ActivityDetector activity;

private:
    template <typename SampleType>
    void process (juce::AudioBuffer<SampleType>& buffer);

    babyair::AirEngine engine;
    babyair::Smoother trimSmoother, bypassSmoother;

    std::atomic<float>* midParam = nullptr;
    std::atomic<float>* highParam = nullptr;
    std::atomic<float>* trimParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    double sampleRateHz = 44100.0;
    double inMeanSquare = 0.0, outMeanSquare = 0.0;
    double rmsCoef = 0.999;
    int enginesChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyAirAudioProcessor)
};
