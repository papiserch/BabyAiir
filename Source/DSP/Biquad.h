#pragma once
// BABY AIR by PAPI SERCH - DSP primitives (no JUCE dependency, header-only).
#include <cmath>
#include <algorithm>

namespace babyair
{
constexpr double kPi = 3.14159265358979323846;

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// RBJ cookbook, bilinear transform with frequency pre-warp. Q = 1/sqrt(2) -> Butterworth.
inline BiquadCoeffs makeLowpass (double fs, double fc, double q = 0.70710678118654752)
{
    fc = std::min (fc, 0.49 * fs);
    const double w0 = 2.0 * kPi * fc / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    BiquadCoeffs c;
    c.b0 = ((1.0 - cw) * 0.5) / a0;
    c.b1 = (1.0 - cw) / a0;
    c.b2 = c.b0;
    c.a1 = (-2.0 * cw) / a0;
    c.a2 = (1.0 - alpha) / a0;
    return c;
}

inline BiquadCoeffs makeHighpass (double fs, double fc, double q = 0.70710678118654752)
{
    fc = std::min (fc, 0.49 * fs);
    const double w0 = 2.0 * kPi * fc / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    BiquadCoeffs c;
    c.b0 = ((1.0 + cw) * 0.5) / a0;
    c.b1 = -(1.0 + cw) / a0;
    c.b2 = c.b0;
    c.a1 = (-2.0 * cw) / a0;
    c.a2 = (1.0 - alpha) / a0;
    return c;
}

// RBJ band-pass, constant 0 dB peak gain. Phase never exceeds +-90 degrees, which is what makes it
// safe to ADD to the dry signal: the sum can only add energy, it can never cancel the dry path.
inline BiquadCoeffs makeBandpass (double fs, double fc, double q)
{
    fc = std::min (fc, 0.49 * fs);
    const double w0 = 2.0 * kPi * fc / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    BiquadCoeffs c;
    c.b0 = alpha / a0;
    c.b1 = 0.0;
    c.b2 = -alpha / a0;
    c.a1 = (-2.0 * cw) / a0;
    c.a2 = (1.0 - alpha) / a0;
    return c;
}

// Transposed direct form II, double precision state (good numerical behaviour for low fc / high fs).
class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
    void reset() noexcept { z1 = z2 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        return y;
    }

    // Denormal protection: call once per block.
    void flushDenormals() noexcept
    {
        if (std::abs (z1) < 1.0e-24) z1 = 0.0;
        if (std::abs (z2) < 1.0e-24) z2 = 0.0;
    }

private:
    BiquadCoeffs k;
    double z1 = 0.0, z2 = 0.0;
};

// One-pole parameter smoother (exponential), per-sample.
class Smoother
{
public:
    void prepare (double sampleRate, double timeMs) noexcept
    {
        coef = std::exp (-1.0 / (sampleRate * timeMs * 0.001));
    }
    void setImmediate (double v) noexcept { current = target = v; }
    void setTarget (double v) noexcept { target = v; }
    inline double next() noexcept
    {
        current = target + coef * (current - target);
        return current;
    }
    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }
    bool isSettled() const noexcept { return std::abs (current - target) < 1.0e-7; }

private:
    double coef = 0.999, current = 0.0, target = 0.0;
};

// Power-domain envelope follower with separate attack/release (RMS-like detector).
class PowerEnvelope
{
public:
    void prepare (double sampleRate, double attackMs, double releaseMs) noexcept
    {
        att = std::exp (-1.0 / (sampleRate * attackMs * 0.001));
        rel = std::exp (-1.0 / (sampleRate * releaseMs * 0.001));
    }
    void reset() noexcept { env = 0.0; }
    inline double process (double x2) noexcept
    {
        const double c = x2 > env ? att : rel;
        env = x2 + c * (env - x2);
        return env;
    }
    double get() const noexcept { return env; }
    void flushDenormals() noexcept { if (env < 1.0e-30) env = 0.0; }

private:
    double att = 0.99, rel = 0.999, env = 0.0;
};

inline double smoothstep01 (double x) noexcept
{
    x = std::min (1.0, std::max (0.0, x));
    return x * x * (3.0 - 2.0 * x);
}

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

} // namespace babyair
