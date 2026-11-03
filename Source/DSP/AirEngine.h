#pragma once
// BABY AIR by PAPI SERCH - dynamic high-frequency "air" engine.
//
// Original design (not a copy of any commercial algorithm). Architecture:
//
//   input x ----------------------------------------------> (dry, untouched) ---------+
//     |                                                                               |
//     +--> BP2 centred 4.5 kHz  (MID band)  --> level-dependent lift --> * midAmt  ---+--> sum
//     |                                                                               |
//     +--> BP2 centred 13 kHz   (HIGH band) --> level-dependent lift --> * highAmt ---+
//
// The enhancement is ADDED to the untouched dry signal (parallel / encoder-only topology, the
// general principle behind Dolby-A style "band enhancers"). The dry path is never filtered, so the
// body of the signal is preserved. The wet bands are 2nd-order band-pass filters because their phase
// never leaves +-90 degrees: added to the dry they can only raise the level, never cancel it
// (steeper filters - e.g. Linkwitz-Riley splits - were measured to cause a deep dip at the crossover).
// The amount of lift depends on the band's own level:
//   - very low level  : lift fades out (noise-floor guard, avoids raising hiss)
//   - low/mid level   : full lift (detail and air are exposed)
//   - high level      : lift is gently reduced with a soft knee (avoids harsh/sibilant peaks)
// The detector is stereo-linked (one gain for all channels) to keep the stereo image stable.
#include "Biquad.h"
#include <array>

namespace babyair
{
struct BandSettings
{
    double centerHz;      // band-pass centre frequency
    double q;             // band-pass Q
    double attackMs;      // envelope attack
    double releaseMs;     // envelope release
    double maxWet;        // linear wet gain at amount = 1 and full lift
    double lowGuardDb0;   // below this level the lift is 0 (noise-floor guard)
    double lowGuardDb1;   // above this level the lift is complete
    double kneeStartDb;   // lift starts to be reduced
    double kneeEndDb;     // lift reaches its minimum
    double minLift;       // lift factor at high level (0..1)
};

class AirEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kControlInterval = 16; // samples between gain-curve evaluations

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        nch = std::min (std::max (numChannels, 1), kMaxChannels);

        // Experimental starting values (documented in Docs/DSP.md)
        //          centre  Q    att  rel   maxWet  guard0  guard1  kneeA  kneeB  minLift
        mid  = {  4500.0, 0.90, 3.0, 90.0, 1.00,  -78.0,  -58.0,  -40.0, -12.0, 0.40 };
        high = { 13000.0, 1.30, 2.0, 70.0, 1.20,  -82.0,  -62.0,  -42.0, -14.0, 0.35 };

        const auto midC  = makeBandpass (fs, mid.centerHz, mid.q);
        const auto highC = makeBandpass (fs, std::min (high.centerHz, 0.40 * fs), high.q);

        for (int c = 0; c < kMaxChannels; ++c)
        {
            midBp[c].setCoeffs (midC);
            highBp[c].setCoeffs (highC);
        }

        midEnv.prepare (fs, mid.attackMs, mid.releaseMs);
        highEnv.prepare (fs, high.attackMs, high.releaseMs);
        midAmt.prepare (fs, 25.0);
        highAmt.prepare (fs, 25.0);
        midGainSm = std::exp (-1.0 / (fs * 0.0004));
        highGainSm = midGainSm;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            midBp[c].reset(); highBp[c].reset();
        }
        midEnv.reset(); highEnv.reset();
        midGain = highGain = 0.0;
        midGainTarget = highGainTarget = 0.0;
        controlCounter = 0;
        midAmt.setImmediate (midAmt.getTarget());
        highAmt.setImmediate (highAmt.getTarget());
    }

    // amount values are normalised 0..1
    void setAmounts (double midAmount01, double highAmount01) noexcept
    {
        midAmt.setTarget (std::min (1.0, std::max (0.0, midAmount01)));
        highAmt.setTarget (std::min (1.0, std::max (0.0, highAmount01)));
    }

    // Processes one frame in place. data[0..n-1] = input sample of each channel -> output.
    inline void processFrame (float* data, int n) noexcept
    {
        std::array<double, kMaxChannels> x {}, midBand {}, highBand {};
        double midPow = 0.0, highPow = 0.0;
        const int chans = std::min (n, nch);

        for (int c = 0; c < chans; ++c)
        {
            x[(size_t) c] = (double) data[c];
            const double m = midBp[c].process (x[(size_t) c]);
            const double h = highBp[c].process (x[(size_t) c]);
            midBand[(size_t) c] = m;
            highBand[(size_t) c] = h;
            midPow  += m * m;
            highPow += h * h;
        }

        // Stereo-linked detection (mean power across channels)
        const double invN = 1.0 / (double) std::max (chans, 1);
        const double mEnv = midEnv.process (midPow * invN);
        const double hEnv = highEnv.process (highPow * invN);

        if (--controlCounter <= 0)
        {
            controlCounter = kControlInterval;
            midGainTarget  = liftCurve (mid, mEnv);
            highGainTarget = liftCurve (high, hEnv);
        }

        // Fast gain smoothing prevents zipper noise between control updates
        midGain  = midGainTarget  + midGainSm  * (midGain  - midGainTarget);
        highGain = highGainTarget + highGainSm * (highGain - highGainTarget);

        const double wm = midAmt.next()  * mid.maxWet  * midGain;
        const double wh = highAmt.next() * high.maxWet * highGain;

        for (int c = 0; c < chans; ++c)
            data[c] = (float) (x[(size_t) c] + wm * midBand[(size_t) c] + wh * highBand[(size_t) c]);
    }

    void flushDenormals() noexcept
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            midBp[c].flushDenormals(); highBp[c].flushDenormals();
        }
        midEnv.flushDenormals(); highEnv.flushDenormals();
        if (midGain < 1.0e-20) midGain = 0.0;
        if (highGain < 1.0e-20) highGain = 0.0;
    }

    // Exposed for tests / documentation: lift factor 0..1 for a band level in dBFS (mean-square domain).
    static double liftForLevelDb (const BandSettings& s, double levelDb) noexcept
    {
        const double guard = smoothstep01 ((levelDb - s.lowGuardDb0) / (s.lowGuardDb1 - s.lowGuardDb0));
        const double knee  = smoothstep01 ((levelDb - s.kneeStartDb) / (s.kneeEndDb - s.kneeStartDb));
        return guard * (1.0 - (1.0 - s.minLift) * knee);
    }

    const BandSettings& midSettings() const noexcept { return mid; }
    const BandSettings& highSettings() const noexcept { return high; }

private:
    static inline double liftCurve (const BandSettings& s, double meanSquare) noexcept
    {
        const double db = 10.0 * std::log10 (meanSquare + 1.0e-20);
        return liftForLevelDb (s, db);
    }

    double fs = 44100.0;
    int nch = 2;
    BandSettings mid {}, high {};

    std::array<Biquad, kMaxChannels> midBp, highBp;
    PowerEnvelope midEnv, highEnv;
    Smoother midAmt, highAmt;
    double midGain = 0.0, highGain = 0.0, midGainTarget = 0.0, highGainTarget = 0.0;
    double midGainSm = 0.99, highGainSm = 0.99;
    int controlCounter = 0;
};

} // namespace babyair
