#include "PluginProcessor.h"
#include "PluginEditor.h"

BabyAirAudioProcessor::BabyAirAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BABYAIR_STATE", babyair::createParameterLayout())
{
    midParam    = apvts.getRawParameterValue (babyair::ids::midAir);
    highParam   = apvts.getRawParameterValue (babyair::ids::highAir);
    trimParam   = apvts.getRawParameterValue (babyair::ids::trim);
    bypassParam = apvts.getRawParameterValue (babyair::ids::bypass);
}

bool BabyAirAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void BabyAirAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    enginesChannels = juce::jlimit (1, 2, getTotalNumInputChannels());
    engine.prepare (sampleRate, enginesChannels);
    engine.setAmounts ((double) midParam->load() * 0.01, (double) highParam->load() * 0.01);
    engine.reset();

    trimSmoother.prepare (sampleRate, 20.0);
    trimSmoother.setImmediate (babyair::dbToGain ((double) trimParam->load()));
    bypassSmoother.prepare (sampleRate, 15.0);
    bypassSmoother.setImmediate (bypassParam->load() > 0.5f ? 1.0 : 0.0);

    activity.prepare (sampleRate);
    rmsCoef = std::exp (-1.0 / (sampleRate * 0.150)); // power domain: 150 ms ~ 300 ms amplitude
    inMeanSquare = outMeanSquare = 0.0;

    setLatencySamples (0); // IIR bands, zero latency
}

template <typename SampleType>
void BabyAirAudioProcessor::process (juce::AudioBuffer<SampleType>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = std::min (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    if (numCh == 0 || numSamples == 0)
        return;

    engine.setAmounts ((double) midParam->load() * 0.01, (double) highParam->load() * 0.01);
    trimSmoother.setTarget (babyair::dbToGain ((double) trimParam->load()));
    bypassSmoother.setTarget (bypassParam->load() > 0.5f ? 1.0 : 0.0);

    SampleType* ch[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    float inPk[2] = { 0, 0 }, outPk[2] = { 0, 0 };
    bool clipped = false;
    double inMs = inMeanSquare, outMs = outMeanSquare;
    const double oneMinusCoef = 1.0 - rmsCoef;

    for (int i = 0; i < numSamples; ++i)
    {
        float frame[2] = { 0.0f, 0.0f }, dry[2] = { 0.0f, 0.0f };
        double inPow = 0.0, maxSq = 0.0;

        for (int c = 0; c < numCh; ++c)
        {
            const float x = (float) ch[c][i];
            dry[c] = frame[c] = x;
            const float ax = std::abs (x);
            if (ax > inPk[c]) inPk[c] = ax;
            const double sq = (double) x * (double) x;
            inPow += sq;
            maxSq = std::max (maxSq, sq);
        }

        // Analysis for the mascot: READ ONLY, does not touch the audio path
        activity.pushSquared (maxSq);
        inMs += oneMinusCoef * (inPow / (double) numCh - inMs);

        engine.processFrame (frame, numCh);

        const float trimGain = (float) trimSmoother.next();
        const float byp = (float) bypassSmoother.next();   // 0 = processed, 1 = bypassed (smooth crossfade)

        double outPow = 0.0;
        for (int c = 0; c < numCh; ++c)
        {
            const float processed = frame[c] * trimGain;
            const float y = byp <= 0.0f ? processed : (byp >= 1.0f ? dry[c] : processed + byp * (dry[c] - processed));
            ch[c][i] = (SampleType) y;

            const float ay = std::abs (y);
            if (ay > outPk[c]) outPk[c] = ay;
            if (ay >= 1.0f) clipped = true;
            outPow += (double) y * (double) y;
        }
        outMs += oneMinusCoef * (outPow / (double) numCh - outMs);
    }

    engine.flushDenormals();
    if (inMs < 1.0e-30) inMs = 0.0;
    if (outMs < 1.0e-30) outMs = 0.0;
    inMeanSquare = inMs;
    outMeanSquare = outMs;

    activity.publish();

    for (int c = 0; c < numCh; ++c)
    {
        SharedMeters::raiseTo (meters.inPeak[c], inPk[c]);
        SharedMeters::raiseTo (meters.outPeak[c], outPk[c]);
    }
    if (numCh == 1) { SharedMeters::raiseTo (meters.inPeak[1], inPk[0]); SharedMeters::raiseTo (meters.outPeak[1], outPk[0]); }
    meters.inRms.store ((float) std::sqrt (inMs), std::memory_order_relaxed);
    meters.outRms.store ((float) std::sqrt (outMs), std::memory_order_relaxed);
    if (clipped) meters.outClipEvent.store (true, std::memory_order_relaxed);
}

void BabyAirAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)  { process (buffer); }
void BabyAirAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { process (buffer); }

juce::AudioProcessorEditor* BabyAirAudioProcessor::createEditor() { return new BabyAirAudioProcessorEditor (*this); }

void BabyAirAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BabyAirAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BabyAirAudioProcessor(); }
