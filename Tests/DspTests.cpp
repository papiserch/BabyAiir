// BABY AIR - objective DSP tests (offline, no JUCE). Build target: BabyAirDspTests
#include "../Source/DSP/AirEngine.h"
#include "../Source/DSP/ActivityDetector.h"
#include <cstdio>
#include <vector>
#include <random>
#include <complex>

using namespace babyair;
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  [FAIL] %s\n", msg); ++failures; } else std::printf("  [ ok ] %s\n", msg); } while (0)

static double rmsOf (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (to - from));
}

// Goertzel magnitude (amplitude) of frequency f in v[from,to)
static double toneAmp (const std::vector<float>& v, size_t from, size_t to, double f, double fs)
{
    std::complex<double> acc = 0; const size_t n = to - from;
    for (size_t i = 0; i < n; ++i)
        acc += (double) v[from + i] * std::exp (std::complex<double> (0, -2.0 * kPi * f * (double) i / fs));
    return 2.0 * std::abs (acc) / (double) n;
}

static std::vector<float> runMono (AirEngine& e, const std::vector<float>& in)
{
    std::vector<float> out (in.size());
    for (size_t i = 0; i < in.size(); ++i) { float s = in[i]; e.processFrame (&s, 1); out[i] = s; }
    return out;
}

// steady sine -> gain in dB at the output, measured at the tail
static double sineGainDb (double fs, double freq, double levelDb, double mid, double high)
{
    AirEngine e; e.prepare (fs, 1); e.setAmounts (mid, high); e.reset();
    const size_t N = (size_t) (fs * 1.2);
    std::vector<float> in (N);
    const double a = dbToGain (levelDb);
    for (size_t i = 0; i < N; ++i) in[i] = (float) (a * std::sin (2.0 * kPi * freq * (double) i / fs));
    auto out = runMono (e, in);
    const size_t from = N - (size_t) (fs * 0.2);
    return gainToDb (rmsOf (out, from, N) / rmsOf (in, from, N));
}

int main()
{
    std::printf ("BABY AIR DSP objective tests\n============================\n");

    // 1) Amount 0 => transparent
    std::printf ("\n[1] Transparency at 0%% / 0%% (44.1 kHz)\n");
    {
        double worst = 0;
        for (double f : { 100.0, 1000.0, 5000.0, 10000.0, 16000.0 })
            worst = std::max (worst, std::abs (sineGainDb (44100, f, -20, 0, 0)));
        std::printf ("  worst deviation: %.5f dB\n", worst);
        CHECK (worst < 0.01, "transparent when both amounts are 0");
    }

    // 2) Frequency response tables
    std::printf ("\n[2] Frequency response @ 48 kHz (sine at -30 dBFS; gain in dB)\n");
    std::printf ("  freq(Hz) | MID100 | HIGH100 | BOTH100 | BOTH50\n");
    {
        double maxLow = 0, maxMid = 0, maxHigh = 0, maxBoth = 0;
        for (double f : { 100.0, 500.0, 1000.0, 2000.0, 2500.0, 3500.0, 5000.0, 6500.0, 8000.0, 10000.0, 12000.0, 14000.0, 16000.0, 18000.0, 20000.0 })
        {
            const double a = sineGainDb (48000, f, -30, 1, 0), b = sineGainDb (48000, f, -30, 0, 1);
            const double c = sineGainDb (48000, f, -30, 1, 1), d = sineGainDb (48000, f, -30, 0.5, 0.5);
            std::printf ("  %8.0f | %+6.2f | %+7.2f | %+7.2f | %+6.2f\n", f, a, b, c, d);
            if (f <= 500)  maxLow = std::max ({ maxLow, std::abs (a), std::abs (b) });
            if (f >= 3500 && f <= 6500) maxMid = std::max (maxMid, a);
            if (f >= 10000 && f <= 16000) maxHigh = std::max (maxHigh, b);
            maxBoth = std::max (maxBoth, c);
        }
        CHECK (maxLow < 0.3,  "low frequencies (<=500 Hz) untouched (<0.3 dB)");
        CHECK (maxMid > 3.0,  "MID AIR lifts the 3.5-6.5 kHz region (>3 dB at 100%)");
        CHECK (maxHigh > 3.0, "HIGH AIR lifts the 10-16 kHz region (>3 dB at 100%)");
        CHECK (maxBoth < 15.0, "combined maximum lift stays below 15 dB (no runaway)");
    }

    // 3) Dynamic behaviour: lift vs input level
    std::printf ("\n[3] Level-dependent lift (HIGH 100%%, 12 kHz, 48 kHz) and MID (100%%, 5 kHz)\n");
    std::printf ("  level(dBFS) | HIGH lift dB | MID lift dB\n");
    double h70 = 0, h40 = 0, h6 = 0;
    for (double lv : { -90.0, -70.0, -50.0, -40.0, -30.0, -20.0, -10.0, -3.0 })
    {
        const double h = sineGainDb (48000, 12000, lv, 0, 1), m = sineGainDb (48000, 5000, lv, 1, 0);
        std::printf ("  %10.0f | %+11.2f | %+10.2f\n", lv, h, m);
        if (lv == -70) { h70 = h; }
        if (lv == -40) { h40 = h; }
        if (lv == -3)  { h6 = h; }
    }
    CHECK (h40 > h6 + 1.0, "lift is larger at low level than at high level (dynamic, not a static shelf)");
    CHECK (h70 < h40,      "noise-floor guard: very quiet signals are not boosted as much");

    // 4) Digital silence
    std::printf ("\n[4] Silence stays silent\n");
    {
        AirEngine e; e.prepare (44100, 2); e.setAmounts (1, 1);
        double peak = 0; for (int i = 0; i < 44100; ++i) { float f[2] = { 0, 0 }; e.processFrame (f, 2); peak = std::max ({ peak, (double) std::abs (f[0]), (double) std::abs (f[1]) }); }
        CHECK (peak == 0.0, "zero in -> exactly zero out");
    }

    // 5) Stability at all sample rates, with noise / impulse / full-scale
    std::printf ("\n[5] Stability across sample rates (white noise, pink noise, impulse, full-scale square)\n");
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        AirEngine e; e.prepare (fs, 2); e.setAmounts (1, 1);
        std::mt19937 rng (1234); std::uniform_real_distribution<float> d (-1.f, 1.f);
        bool finite = true; double peak = 0, b0=0,b1=0,b2=0;
        const size_t N = (size_t) (fs * 2.0);
        for (size_t i = 0; i < N; ++i)
        {
            float w = d (rng) * 0.5f;
            // Paul Kellet pink filter (economy)
            b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
            const float pink = (float) ((b0 + b1 + b2 + w * 0.1848) * 0.2);
            float s[2];
            if (i < N / 4) { s[0] = w; s[1] = -w; }                    // white noise (anti-phase L/R)
            else if (i < N / 2) { s[0] = pink; s[1] = pink; }          // pink noise
            else if (i < N / 2 + 10) { s[0] = s[1] = (i == N / 2) ? 1.0f : 0.0f; } // impulse
            else if (i < 3 * N / 4) { s[0] = s[1] = ((i / 40) % 2) ? 1.0f : -1.0f; } // full-scale square
            else { s[0] = s[1] = 0.0f; }
            e.processFrame (s, 2);
            if (! std::isfinite (s[0]) || ! std::isfinite (s[1])) finite = false;
            peak = std::max ({ peak, (double) std::abs (s[0]), (double) std::abs (s[1]) });
        }
        char msg[128]; std::snprintf (msg, sizeof msg, "%.1f kHz: finite output, peak %.2f", fs / 1000.0, peak);
        CHECK (finite && peak < 8.0, msg);
    }

    // 6) Impulse response: no long ringing
    std::printf ("\n[6] Impulse response decay (96 kHz, 100%%/100%%)\n");
    {
        AirEngine e; e.prepare (96000, 1); e.setAmounts (1, 1); e.reset();
        std::vector<float> in (96000, 0.0f); in[100] = 0.1f;
        auto out = runMono (e, in);
        size_t last = 0; for (size_t i = 0; i < out.size(); ++i) if (std::abs (out[i]) > 0.1f * 1.0e-4f) last = i; // -80 dB
        const double ms = (double) (last - 100) / 96.0;
        std::printf ("  time to fall below -80 dB re impulse: %.2f ms\n", ms);
        CHECK (ms < 20.0, "impulse response decays below -80 dB within 20 ms (no resonance)");
    }

    // 7) Harmonic distortion of a steady tone (gain modulation must not create harmonics)
    std::printf ("\n[7] Harmonic distortion (48 kHz)\n");
    {
        const double fs = 48000, f0 = 3000; AirEngine e; e.prepare (fs, 1); e.setAmounts (1, 1);
        const size_t N = (size_t) (fs * 1.5); std::vector<float> in (N);
        for (size_t i = 0; i < N; ++i) in[i] = (float) (0.1 * std::sin (2 * kPi * f0 * (double) i / fs));
        auto out = runMono (e, in);
        const size_t from = N - (size_t) fs / 2, to = N;
        const double fund = toneAmp (out, from, to, f0, fs);
        double h = 0; for (int k = 2; k <= 5; ++k) { const double a = toneAmp (out, from, to, f0 * k, fs); h += a * a; }
        const double thd = 100.0 * std::sqrt (h) / fund;
        std::printf ("  THD (2nd-5th) of a 3 kHz tone at -20 dBFS: %.5f %%\n", thd);
        CHECK (thd < 0.1, "THD < 0.1 %");
    }

    // 8) Stereo coherence + mono
    std::printf ("\n[8] Stereo stability\n");
    {
        AirEngine e; e.prepare (48000, 2); e.setAmounts (0.8, 0.8);
        std::mt19937 rng (5); std::uniform_real_distribution<float> d (-0.3f, 0.3f);
        double maxDiff = 0;
        for (int i = 0; i < 48000; ++i) { float v = d (rng), s[2] = { v, v }; e.processFrame (s, 2); maxDiff = std::max (maxDiff, (double) std::abs (s[0] - s[1])); }
        CHECK (maxDiff == 0.0, "identical L/R in -> identical L/R out (centre stays centred)");

        // hard-panned: silent channel must stay silent
        AirEngine e2; e2.prepare (48000, 2); e2.setAmounts (1, 1);
        double r = 0; for (int i = 0; i < 48000; ++i) { float s[2] = { d (rng), 0.0f }; e2.processFrame (s, 2); r = std::max (r, (double) std::abs (s[1])); }
        CHECK (r == 0.0, "hard-panned signal does not leak into the silent channel");
    }

    // 9) Activity detector
    std::printf ("\n[9] Activity detector (internal thresholds)\n");
    {
        ActivityDetector ad; const double fs = 48000; ad.prepare (fs);
        auto feed = [&] (double amp, double seconds) {
            const int n = (int) (fs * seconds);
            for (int i = 0; i < n; ++i) { const double x = amp * std::sin (2 * kPi * 200.0 * i / fs); ad.pushSquared (x * x); if (i % 256 == 255) ad.publish(); }
            ad.publish(); };
        feed (0.0, 0.5);   CHECK (! ad.getActive(), "silence -> inactive");
        feed (0.0032, 1.0); CHECK (! ad.getActive(), "-50 dBFS sine (below -40 dB threshold) -> inactive");
        feed (0.1, 0.5);   CHECK (ad.getActive(), "-20 dBFS sine -> active");
        const float lvl = ad.getLevel();
        std::printf ("  normalised level at -20 dBFS: %.2f\n", lvl);
        CHECK (lvl > 0.5f && lvl < 1.0f, "normalised level in expected range");
        feed (0.0, 1.5);   CHECK (! ad.getActive(), "silence after music -> inactive again");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL DSP TESTS PASSED" : "SOME TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
