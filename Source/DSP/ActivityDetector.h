#pragma once
// BABY AIR by PAPI SERCH - input activity detector for the mascot.
// Runs on the audio thread, only READS the audio (never modifies it) and publishes two atomics.
// Values are internal (not user controls): threshold -40 dBFS, hysteresis 4 dB,
// attack 30 ms, release 350 ms.
#include "Biquad.h"
#include <atomic>

namespace babyair
{
class ActivityDetector
{
public:
    static constexpr double kThresholdOnDb  = -40.0;
    static constexpr double kHysteresisDb   = 4.0;
    static constexpr double kAttackMs       = 30.0;
    static constexpr double kReleaseMs      = 350.0;
    static constexpr double kFloorDb        = -60.0; // maps to 0.0
    static constexpr double kCeilDb         = -6.0;  // maps to 1.0

    void prepare (double sampleRate)
    {
        // PowerEnvelope works in the power (squared) domain: half the time constant = same amplitude-domain time
        env.prepare (sampleRate, kAttackMs * 0.5, kReleaseMs * 0.5);
        env.reset();
        isActive = false;
        level.store (0.0f, std::memory_order_relaxed);
        active.store (false, std::memory_order_relaxed);
    }

    // Call per sample with the squared (max across channels) input value.
    inline void pushSquared (double x2) noexcept { env.process (x2); }

    // Call once per block (after pushing all samples) to publish the results.
    void publish() noexcept
    {
        env.flushDenormals();
        const double db = 10.0 * std::log10 (env.get() + 1.0e-20);

        if (! isActive && db > kThresholdOnDb)                       isActive = true;
        else if (isActive && db < kThresholdOnDb - kHysteresisDb)    isActive = false;

        const double norm = std::min (1.0, std::max (0.0, (db - kFloorDb) / (kCeilDb - kFloorDb)));
        level.store ((float) norm, std::memory_order_relaxed);
        active.store (isActive, std::memory_order_relaxed);
    }

    // GUI thread
    float getLevel() const noexcept { return level.load (std::memory_order_relaxed); }
    bool  getActive() const noexcept { return active.load (std::memory_order_relaxed); }

private:
    PowerEnvelope env;
    bool isActive = false;
    std::atomic<float> level { 0.0f };
    std::atomic<bool> active { false };
};

} // namespace babyair
