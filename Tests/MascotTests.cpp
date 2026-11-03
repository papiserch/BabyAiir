// BABY AIR - deterministic tests of the mascot state machine (no JUCE).
#include "../Source/GUI/MascotEngine.h"
#include <cstdio>
using namespace babyair;
static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf ("  [FAIL] %s\n", m); ++failures; } else std::printf ("  [ ok ] %s\n", m); } while (0)

static void run (MascotEngine& e, float seconds, bool active, float level, float fps = 60.0f)
{ const int n = (int) (seconds * fps); for (int i = 0; i < n; ++i) e.update (1.0f / fps, active, level); }

int main()
{
    std::printf ("Baby Demon state-machine tests\n==============================\n");
    MascotEngine e;

    run (e, 3.0f, false, 0.0f);
    CHECK (e.state == MascotState::Sleeping, "no audio -> SLEEPING");
    CHECK (e.pose.eyeOpen < 0.02f, "SLEEPING: eyes closed");
    const float breathMax = [&] { float m = 0; for (int i = 0; i < 300; ++i) { e.update (1/60.f, false, 0); m = std::max (m, std::abs (e.pose.breath)); } return m; }();
    CHECK (breathMax > 0.5f, "SLEEPING: breathing animation visible");
    CHECK (e.hearts.get().empty() && e.hearts.totalSpawned == 0, "SLEEPING: no hearts");

    // audio starts at moderate level
    e.update (1/60.f, true, 0.6f);
    CHECK (e.state == MascotState::WakingUp, "audio detected -> WAKING_UP");
    float prevEye = e.pose.eyeOpen; bool monotonic = true; int frames = 0;
    while (e.state == MascotState::WakingUp && frames < 600) { e.update (1/60.f, true, 0.6f); monotonic = monotonic && e.pose.eyeOpen >= prevEye - 1e-4f; prevEye = e.pose.eyeOpen; ++frames; }
    std::printf ("  eyes opened gradually over %.2f s\n", frames / 60.0f);
    CHECK (monotonic && frames > 60, "WAKING_UP: eyes open slowly and smoothly (>1 s)");
    CHECK (e.state == MascotState::Happy, "music continues -> HAPPY");
    run (e, 2.0f, true, 0.6f);
    CHECK (e.pose.eyeOpen > 0.8f && e.pose.smile > 0.6f, "HAPPY: eyes open and smiling");
    CHECK (e.pose.wingAmp > 8.0f && e.pose.tailAmp > 10.0f, "HAPPY: wings and tail moving");
    CHECK (e.hearts.totalSpawned >= 2, "hearts appeared while waking/happy");

    // heart rate limiting over 20 s of happy
    const int before = e.hearts.totalSpawned; int maxAlive = 0;
    for (int i = 0; i < 60 * 20; ++i) { e.update (1/60.f, true, 0.6f); maxAlive = std::max (maxAlive, (int) e.hearts.get().size()); }
    const int spawned = e.hearts.totalSpawned - before;
    std::printf ("  20 s of HAPPY: %d hearts spawned, max alive at once: %d\n", spawned, maxAlive);
    CHECK (spawned >= 6 && spawned <= 20, "hearts are occasional (6-20 per 20 s), not constant");
    CHECK (maxAlive <= HeartSystem::kMaxAlive, "never more than the particle cap alive");

    // intense level -> EXCITED
    int hb = e.hearts.totalSpawned;
    run (e, 1.0f, true, 0.95f);
    CHECK (e.state == MascotState::Excited, "intense level -> EXCITED");
    CHECK (e.hearts.totalSpawned - hb >= 5, "EXCITED: small explosion of hearts");
    run (e, 1.5f, true, 0.95f);
    CHECK (e.pose.wingAmp > 25.0f && e.pose.mouthOpen > 0.5f, "EXCITED: faster/larger wings and open-mouth grin");
    // hysteresis: level between on/off thresholds keeps EXCITED
    run (e, 2.0f, true, 0.75f);
    CHECK (e.state == MascotState::Excited, "hysteresis: 0.75 (between thresholds) stays EXCITED");
    run (e, 2.0f, true, 0.55f);
    CHECK (e.state == MascotState::Happy, "level drops -> back to HAPPY");

    // brief gap does not put it to sleep
    run (e, 0.7f, false, 0.0f);
    CHECK (e.state == MascotState::Happy, "short pause (0.7 s) keeps it awake");
    run (e, 1.0f, true, 0.6f);

    // silence -> returning -> sleeping
    run (e, 1.6f, false, 0.0f);
    CHECK (e.state == MascotState::ReturningToSleep, "silence -> RETURNING_TO_SLEEP");
    float eye0 = e.pose.eyeOpen; run (e, 0.5f, false, 0.0f);
    CHECK (e.pose.eyeOpen < eye0, "RETURNING: eyes closing gradually");
    run (e, 4.0f, false, 0.0f);
    CHECK (e.state == MascotState::Sleeping && e.pose.eyeOpen < 0.03f, "finally SLEEPING again, eyes closed");
    CHECK (e.pose.wingAmp < 1.0f && e.pose.tailAmp < 4.5f, "wings at rest and tail relaxed");
    CHECK (e.hearts.get().empty(), "hearts cleared on sleep");

    // audio returns mid-way through falling asleep
    MascotEngine e2; run (e2, 0.1f, true, 0.6f); run (e2, 3.0f, true, 0.6f); run (e2, 2.0f, false, 0.0f);
    CHECK (e2.state == MascotState::ReturningToSleep, "(setup) falling asleep");
    e2.update (1/60.f, true, 0.6f);
    CHECK (e2.state == MascotState::WakingUp, "audio returns while falling asleep -> WAKING_UP again");

    // no NaN anywhere
    bool finite = true; MascotEngine e3; for (int i = 0; i < 60 * 120; ++i) { e3.update (1/60.f, (i / 600) % 2 == 0, (float) ((i * 37) % 100) / 100.f); finite = finite && std::isfinite (e3.pose.eyeOpen + e3.pose.wingPhase + e3.pose.bounce + e3.pose.headLift); }
    CHECK (finite, "2 minutes of random input: all pose channels stay finite");

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL MASCOT TESTS PASSED" : "SOME TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
