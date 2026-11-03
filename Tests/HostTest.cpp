// BABY AIR - integration tests.
//  1) Loads the REAL compiled .vst3 bundle through JUCE's VST3 hosting and exercises it.
//  2) Exercises the processor class directly (meters, mascot activity value).
//  3) (optional) --snapshots <dir>: renders the editor in several mascot states to PNG files.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf ("  [FAIL] %s\n", msg); ++failures; } else std::printf ("  [ ok ] %s\n", msg); } while (0)

static void fillNoise (juce::AudioBuffer<float>& b, float amp, unsigned seed)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> d (-amp, amp);
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, d (rng));
}

static float maxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float m = 0; for (int c = 0; c < a.getNumChannels(); ++c) for (int i = 0; i < a.getNumSamples(); ++i) m = std::max (m, std::abs (a.getSample (c, i) - b.getSample (c, i)));
    return m;
}

static bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

// Hosted VST3 parameters expose numeric ids, so look them up by their visible name.
static juce::AudioProcessorParameter* paramById (juce::AudioProcessor& p, const juce::String& id)
{
    juce::String name = id == babyair::ids::midAir ? "Mid Air" : id == babyair::ids::highAir ? "High Air"
                      : id == babyair::ids::link ? "Link" : id == babyair::ids::trim ? "Trim" : "Bypass";
    for (auto* prm : p.getParameters())
        if (prm->getName (64) == name) return prm;
    std::printf ("  [FAIL] parameter '%s' not found\n", name.toRawUTF8());
    ++failures;
    return nullptr;
}

static void runSnapshots (const juce::File& outDir);

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc >= 3 && juce::String (argv[1]) == "--snapshots")
    {
        runSnapshots (juce::File (argv[2]));
        return failures == 0 ? 0 : 1;
    }

    // ---------------------------------------------------------------- 1b) LINK behaviour in the real editor
    std::printf ("\n[LINK] MID AIR / HIGH AIR relationship\n");
    {
        BabyAirAudioProcessor lp; lp.prepareToPlay (48000.0, 512);
        auto setP = [&] (const char* id, float v) { auto* q = lp.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        setP (babyair::ids::midAir, 40.0f); setP (babyair::ids::highAir, 50.0f); setP (babyair::ids::link, 1.0f);
        BabyAirAudioProcessorEditor le (lp);
        auto& mid = le.getMidKnob(); auto& high = le.getHighKnob();
        CHECK (le.getLinkButton().getToggleState(), "LINK is on (default)");
        mid.setUserGestureForTest (true); mid.setValue (20.0, juce::sendNotificationSync); mid.setUserGestureForTest (false);
        CHECK (std::abs (high.getValue() - 25.0) < 0.2, "LINK on: halving MID AIR halves HIGH AIR (ratio kept, values NOT forced equal)");
        CHECK (std::abs (mid.getValue() - high.getValue()) > 1.0, "knobs are not forced to the same value");
        mid.setUserGestureForTest (true); mid.setValue (100.0, juce::sendNotificationSync); mid.setUserGestureForTest (false);
        CHECK (high.getValue() <= 100.0 + 1.0e-6, "partner is clamped at its maximum");
        mid.setUserGestureForTest (true); mid.setValue (20.0, juce::sendNotificationSync); mid.setUserGestureForTest (false);
        CHECK (std::abs (high.getValue() - 25.0) < 0.2, "the original relationship survives hitting the end stop");
        high.setUserGestureForTest (true); high.setValue (50.0, juce::sendNotificationSync); high.setUserGestureForTest (false);
        CHECK (std::abs (mid.getValue() - 40.0) < 0.3, "works in both directions (HIGH AIR drives MID AIR)");
        // parameters (what the host/DSP see) follow too
        CHECK (std::abs (lp.apvts.getRawParameterValue (babyair::ids::midAir)->load() - 40.0f) < 0.3f, "linked change reaches the host parameter");
        le.getLinkButton().setToggleState (false, juce::sendNotificationSync);
        const double hBefore = high.getValue();
        mid.setUserGestureForTest (true); mid.setValue (10.0, juce::sendNotificationSync); mid.setUserGestureForTest (false);
        CHECK (std::abs (high.getValue() - hBefore) < 1.0e-6, "LINK off: knobs are fully independent");
    }

    const juce::String bundle = argc >= 2 ? argv[1] : "";

    // ---------------------------------------------------------------- 2) direct processor tests
    std::printf ("\n[A] Direct processor: meters, activity, trim, bypass\n");
    {
        BabyAirAudioProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);

        // silence: mascot should stay asleep
        for (int k = 0; k < 40; ++k) { buf.clear(); p.processBlock (buf, midi); }
        CHECK (! p.activity.getActive(), "silence -> activity inactive");

        // -20 dBFS sine on L, nothing on R: meters must show real levels
        const double f = 1000.0, a = 0.1; long n = 0;
        auto feedSine = [&] (int blocks, double amp)
        {
            for (int k = 0; k < blocks; ++k)
            {
                for (int i = 0; i < 512; ++i, ++n) { const float s = (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * f * (double) n / 48000.0)); buf.setSample (0, i, s); buf.setSample (1, i, 0.0f); }
                p.processBlock (buf, midi);
            }
        };
        p.apvts.getParameter (babyair::ids::midAir)->setValueNotifyingHost (0.0f);
        p.apvts.getParameter (babyair::ids::highAir)->setValueNotifyingHost (0.0f);
        feedSine (30, a); // let the 25 ms parameter smoothers settle at 0 %
        p.meters.inPeak[0].store (0); p.meters.inPeak[1].store (0); p.meters.outPeak[0].store (0); p.meters.outPeak[1].store (0);
        feedSine (60, a);
        const float inPkL = p.meters.inPeak[0].exchange (0.0f), inPkR = p.meters.inPeak[1].exchange (0.0f);
        const float outPkL = p.meters.outPeak[0].exchange (0.0f);
        const float inRms = p.meters.inRms.load();
        std::printf ("  in peak L=%.4f R=%.4f  out peak L=%.4f  in RMS=%.4f (expected peak 0.1000, RMS of both channels avg = 0.0500)\n", inPkL, inPkR, outPkL, inRms);
        CHECK (std::abs (inPkL - 0.1f) < 0.002f, "input peak L reads the real level (-20 dBFS)");
        CHECK (inPkR == 0.0f, "input peak R reads silence (channels independent)");
        CHECK (std::abs (outPkL - 0.1f) < 0.002f, "output peak equals input with amounts at 0 and trim 0");
        CHECK (std::abs (inRms - 0.05f) < 0.003f, "RMS meter reads the real level");
        CHECK (p.activity.getActive(), "music-level signal -> activity active");
        CHECK (p.activity.getLevel() > 0.4f, "normalised activity value reflects level");

        // trim +6 dB
        p.apvts.getParameter (babyair::ids::trim)->setValueNotifyingHost (p.apvts.getParameter (babyair::ids::trim)->convertTo0to1 (6.0f));
        feedSine (30, a); p.meters.outPeak[0].store (0);
        feedSine (10, a);
        const float outTrim = p.meters.outPeak[0].exchange (0.0f);
        std::printf ("  out peak with TRIM +6 dB: %.4f (expected %.4f)\n", outTrim, 0.1f * 1.9953f);
        CHECK (std::abs (outTrim - 0.19953f) < 0.004f, "TRIM +6 dB applies +6 dB");

        // clip detection
        p.apvts.getParameter (babyair::ids::trim)->setValueNotifyingHost (p.apvts.getParameter (babyair::ids::trim)->convertTo0to1 (12.0f));
        p.meters.outClipEvent.store (false);
        feedSine (30, 0.5);
        CHECK (p.meters.outClipEvent.load(), "output clipping (+12 dB trim on -6 dBFS) raises the clip event");
        p.meters.outClipEvent.store (false);

        // bypass: reaches exact dry and transition is click-free
        p.apvts.getParameter (babyair::ids::trim)->setValueNotifyingHost (p.apvts.getParameter (babyair::ids::trim)->convertTo0to1 (6.0f));
        feedSine (30, 0.1);
        p.apvts.getParameter (babyair::ids::bypass)->setValueNotifyingHost (1.0f);
        // record the transition sample-by-sample
        float prev = 0, maxStep = 0; bool firstSample = true;
        for (int k = 0; k < 20; ++k)
        {
            for (int i = 0; i < 512; ++i, ++n) { buf.setSample (0, i, (float) (0.1 * std::sin (2 * juce::MathConstants<double>::pi * f * (double) n / 48000.0))); buf.setSample (1, i, 0.0f); }
            p.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i) { const float s = buf.getSample (0, i); if (! firstSample) maxStep = std::max (maxStep, std::abs (s - prev)); prev = s; firstSample = false; }
        }
        // unprocessed sine at 1 kHz / 48 kHz / amp 0.1 has max natural step = 0.1 * 2*sin(pi*1000/48000) ~ 0.0131
        std::printf ("  largest sample-to-sample step during BYPASS crossfade (+6 dB trim -> dry): %.4f (natural max for a 1 kHz sine at 0.2 = 0.026)\n", maxStep);
        CHECK (maxStep < 0.04f, "BYPASS transition has no clicks");
        float lastErr = 0; for (int i = 0; i < 512; ++i) { const float expect = (float) (0.1 * std::sin (2 * juce::MathConstants<double>::pi * f * (double) (n - 512 + i) / 48000.0)); lastErr = std::max (lastErr, std::abs (buf.getSample (0, i) - expect)); }
        CHECK (lastErr < 1.0e-6f, "fully bypassed output is bit-transparent (trim also bypassed)");
    }

    // ---------------------------------------------------------------- 1) real VST3 bundle
    std::printf ("\n[B] Real VST3 bundle loaded through the VST3 host\n");
    if (bundle.isEmpty()) { std::printf ("  (no bundle path given: skipped)\n"); }
    else
    {
        juce::AudioPluginFormatManager fm; fm.addFormat (new juce::VST3PluginFormat());
        juce::OwnedArray<juce::PluginDescription> descs;
        for (auto* fmt : fm.getFormats()) fmt->findAllTypesForFile (descs, bundle);
        CHECK (descs.size() == 1, "VST3 bundle scans and exposes one plugin");
        if (descs.size() == 1)
        {
            std::printf ("  name='%s' manufacturer='%s' category='%s' inputs=%d outputs=%d\n", descs[0]->name.toRawUTF8(), descs[0]->manufacturerName.toRawUTF8(), descs[0]->category.toRawUTF8(), descs[0]->numInputChannels, descs[0]->numOutputChannels);
            juce::String err;
            auto plug = fm.createPluginInstance (*descs[0], 48000.0, 512, err);
            CHECK (plug != nullptr, ("instantiates (" + err + ")").toRawUTF8());
            if (plug)
            {
                std::printf ("  parameters exposed to the host:\n");
                for (auto* prm : plug->getParameters()) std::printf ("    - '%s'  default=%.3f  automatable=%d\n", prm->getName (64).toRawUTF8(), prm->getDefaultValue(), (int) prm->isAutomatable());
                CHECK (plug->getParameters().size() == 5, "exactly 5 host-visible parameters (Mid Air, High Air, Link, Trim, Bypass)");
                bool allAuto = true; for (auto* prm : plug->getParameters()) allAuto = allAuto && prm->isAutomatable();
                CHECK (allAuto, "all parameters are automatable");

                const int blockSizes[] = { 1, 17, 64, 480, 512, 1000, 4096 };
                for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
                {
                    plug->setPlayConfigDetails (2, 2, sr, 512);
                    plug->prepareToPlay (sr, 4096);
                    for (auto* prm : plug->getParameters()) prm->setValue (prm->getName (32) == "Link" || prm->getName (32) == "Bypass" ? 0.0f : 1.0f);
                    bool ok = true; juce::MidiBuffer midi;
                    for (int bs : blockSizes) { juce::AudioBuffer<float> b (2, bs); for (int rep = 0; rep < 4; ++rep) { fillNoise (b, 0.5f, (unsigned) (bs + rep)); plug->processBlock (b, midi); ok = ok && allFinite (b); } }
                    char msg[96]; std::snprintf (msg, sizeof msg, "%.1f kHz: variable block sizes 1..4096 process cleanly", sr / 1000.0);
                    CHECK (ok, msg);
                    CHECK (plug->getLatencySamples() == 0, "reports 0 samples of latency");
                    plug->releaseResources();
                }

                // transparency with amounts 0 (host path)
                plug->setPlayConfigDetails (2, 2, 48000.0, 4096); plug->prepareToPlay (48000.0, 4096);
                if (auto* m = paramById (*plug, babyair::ids::midAir)) m->setValue (0.0f);
                if (auto* h = paramById (*plug, babyair::ids::highAir)) h->setValue (0.0f);
                if (auto* t = paramById (*plug, babyair::ids::trim)) t->setValue (0.5f);
                if (auto* b = paramById (*plug, babyair::ids::bypass)) b->setValue (0.0f);
                { juce::AudioBuffer<float> in (2, 4096), out; juce::MidiBuffer midi;
                  for (int k = 0; k < 30; ++k) { fillNoise (in, 0.5f, (unsigned) k); out.makeCopyOf (in); plug->processBlock (out, midi);
                      if (k < 4 || k == 29) std::printf ("    warm-up block %d: output/input RMS = %.4f\n", k, (double) (out.getRMSLevel (0, 0, 4096) / in.getRMSLevel (0, 0, 4096))); }
                  fillNoise (in, 0.5f, 99); out.makeCopyOf (in); plug->processBlock (out, midi);
                  const float d = maxAbsDiff (in, out); std::printf ("  max deviation from input after settling: %.3g\n", (double) d);
                  CHECK (d < 1.0e-7f, "MID/HIGH at 0 %, TRIM 0 dB -> transparent passthrough (< -140 dB)"); }

                // MID/HIGH at 100% raise high-frequency energy; low band is untouched
                if (auto* m = paramById (*plug, babyair::ids::midAir)) m->setValue (1.0f);
                if (auto* h = paramById (*plug, babyair::ids::highAir)) h->setValue (1.0f);
                { juce::MidiBuffer midi; juce::AudioBuffer<float> b (2, 2048); double e0 = 0, e1 = 0;
                  for (int rep = 0; rep < 40; ++rep) { fillNoise (b, 0.05f, (unsigned) rep); juce::AudioBuffer<float> orig; orig.makeCopyOf (b); plug->processBlock (b, midi);
                      if (rep > 10) for (int i = 1; i < 2048; ++i) { const float d0 = orig.getSample (0, i) - orig.getSample (0, i - 1), d1 = b.getSample (0, i) - b.getSample (0, i - 1); e0 += (double) d0 * d0; e1 += (double) d1 * d1; } }
                  const double gain = 10.0 * std::log10 (e1 / e0);
                  std::printf ("  high-frequency energy (first difference) of -26 dBFS white noise: %+.2f dB\n", gain);
                  CHECK (gain > 2.0, "MID AIR + HIGH AIR at 100 % really process audio through the host"); }

                // mono
                plug->releaseResources();
                {
                    juce::AudioProcessor::BusesLayout mono; mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
                    const bool supported = plug->checkBusesLayoutSupported (mono);
                    CHECK (supported, "host can negotiate a mono (1 in / 1 out) layout");
                    if (supported && plug->setBusesLayout (mono))
                    {
                        plug->prepareToPlay (48000.0, 4096);
                        juce::MidiBuffer midi; juce::AudioBuffer<float> b (1, 512); bool ok = true;
                        for (int k = 0; k < 10; ++k) { fillNoise (b, 0.3f, (unsigned) k); plug->processBlock (b, midi); ok = ok && allFinite (b); }
                        CHECK (ok, "mono instance processes audio");
                        plug->releaseResources();
                    }
                    juce::AudioProcessor::BusesLayout stereo; stereo.inputBuses.add (juce::AudioChannelSet::stereo()); stereo.outputBuses.add (juce::AudioChannelSet::stereo());
                    plug->setBusesLayout (stereo);
                    plug->prepareToPlay (48000.0, 4096);
                }

                // state save / restore into a NEW instance
                if (auto* m = paramById (*plug, babyair::ids::midAir)) m->setValue (0.37f);
                if (auto* h = paramById (*plug, babyair::ids::highAir)) h->setValue (0.81f);
                if (auto* l = paramById (*plug, babyair::ids::link)) l->setValue (0.0f);
                if (auto* t = paramById (*plug, babyair::ids::trim)) t->setValue (0.25f);
                if (auto* b = paramById (*plug, babyair::ids::bypass)) b->setValue (1.0f);
                juce::MemoryBlock state; plug->getStateInformation (state);
                std::printf ("  saved state: %d bytes\n", (int) state.getSize());
                juce::String err2; auto plug2 = fm.createPluginInstance (*descs[0], 48000.0, 512, err2);
                CHECK (plug2 != nullptr, "second instance created");
                if (plug2)
                {
                    plug2->setStateInformation (state.getData(), (int) state.getSize());
                    bool same = true;
                    for (const char* id : { babyair::ids::midAir, babyair::ids::highAir, babyair::ids::link, babyair::ids::trim, babyair::ids::bypass })
                    { auto* a = paramById (*plug, id); auto* b = paramById (*plug2, id); if (! a || ! b) { same = false; continue; } const float va = a->getValue(), vb = b->getValue(); std::printf ("    %-8s saved=%.4f restored=%.4f\n", id, va, vb); same = same && std::abs (va - vb) < 1.0e-4f; }
                    CHECK (same, "project recall: all 5 parameters restored exactly");
                }
            }
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL INTEGRATION TESTS PASSED" : "SOME TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

static void runSnapshots (const juce::File& outDir)
{
    // Renders the real editor (same code path as the DAW) at the size of the reference image (1536 x 1024).
    outDir.createDirectory();
    BabyAirAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    p.apvts.getParameter (babyair::ids::midAir)->setValueNotifyingHost (p.apvts.getParameter (babyair::ids::midAir)->convertTo0to1 (42.0f));
    p.apvts.getParameter (babyair::ids::highAir)->setValueNotifyingHost (p.apvts.getParameter (babyair::ids::highAir)->convertTo0to1 (58.0f));

    auto ed = std::make_unique<BabyAirAudioProcessorEditor> (p);
    ed->setSize (1536, 1024);
    auto& m = ed->getMascot();
    m.setManualMode (true);
    auto& eng = m.getEngine();

    auto save = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (juce::String (name) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        CHECK (ok, (juce::String ("snapshot written: ") + name).toRawUTF8());
    };
    auto run = [&] (float seconds, bool active, float level) { for (int i = 0; i < (int) (seconds * 60.0f); ++i) eng.update (1.0f / 60.0f, active, level); };

    ed->getMeters().setManualValues (-120.0f, -120.0f, -120.0f, -120.0f, false);
    run (2.0f, false, 0.0f);                 save ("1_sleeping");
    ed->getMeters().setManualValues (-9.0f, -11.0f, -6.0f, -22.0f, false);
    run (0.55f, true, 0.45f);                save ("2_waking_up");
    run (4.0f, true, 0.5f);                  save ("3_happy");
    ed->getMeters().setManualValues (-0.4f, -0.1f, -0.1f, -18.7f, true);
    run (3.0f, true, 0.95f);                 save ("4_excited");
    ed->getMeters().setManualValues (-30.0f, -31.0f, -0.1f, -40.0f, false);
    run (3.2f, false, 0.0f);                 save ("5_returning_to_sleep");
}
