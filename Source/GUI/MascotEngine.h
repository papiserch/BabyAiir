#pragma once
// BABY AIR by PAPI SERCH - Baby Demon animation brain.
//
// Pure logic: no drawing, no JUCE, no audio. It is driven ONLY by two numbers that the audio thread
// publishes through atomics (activity flag + normalised level) and by elapsed time. It can never
// influence the sound. The GUI (MascotView) reads `pose` every frame and draws the layers.
//
//   SLEEPING -> WAKING_UP -> HAPPY <-> EXCITED -> RETURNING_TO_SLEEP -> SLEEPING
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace babyair
{
enum class MascotState { Sleeping, WakingUp, Happy, Excited, ReturningToSleep };

inline const char* toString (MascotState s)
{
    switch (s) { case MascotState::Sleeping: return "SLEEPING"; case MascotState::WakingUp: return "WAKING_UP";
                 case MascotState::Happy: return "HAPPY"; case MascotState::Excited: return "EXCITED";
                 case MascotState::ReturningToSleep: return "RETURNING_TO_SLEEP"; }
    return "?";
}

// Every field is one independently animated channel of the layered character.
struct MascotPose
{
    float eyeOpen = 0.0f;      // 0 closed .. 1 wide open (eyelids)
    float sparkle = 0.0f;      // excited glints in the eyes
    float browRaise = -0.3f;   // -1..1
    float browTilt = 0.0f;     // -1 sleepy/sad .. +1 mischievous
    float smile = -0.15f;      // -1 frown .. 1 big smile (mouth)
    float mouthOpen = 0.0f;    // 0..1
    float blush = 0.15f;       // cheeks
    float headLift = -3.0f;    // px (negative = head resting low)
    float headTilt = 0.05f;    // radians
    float wingBase = 56.0f;    // degrees, wings drooping (rest) .. raised
    float wingAmp = 0.0f;      // flap amplitude (degrees)
    float wingPhase = 0.0f;    // radians
    float tailAmp = 3.0f;      // degrees
    float tailPhase = 0.0f;
    float bounce = 0.0f;       // px, body hop
    float breath = 0.0f;       // -1..1
    float pawPat = 0.0f;       // px
    float zAlpha = 1.0f;       // sleeping "z z z"
    float haloGlow = 0.5f;
    float lookX = 0.0f;        // pupils
};

struct Heart { float x, y, vx, vy, rot, vrot, size, age, life, phase; };

class HeartSystem
{
public:
    static constexpr int kMaxAlive = 10;
    static constexpr float kMinInterval = 0.28f; // limits the spawn frequency (seconds)

    void burst (int n) { pending += n; }
    void trickle()      { pending += 1; }
    void clear() { hearts.clear(); pending = 0; }

    void update (float dt)
    {
        sinceSpawn += dt;
        if (pending > 0 && sinceSpawn >= kMinInterval * (pending > 2 ? 0.35f : 1.0f) && (int) hearts.size() < kMaxAlive)
        {
            spawnOne(); --pending; sinceSpawn = 0.0f;
        }
        if (pending > 8) pending = 8;

        for (auto& h : hearts)
        {
            h.age += dt;
            h.x += (h.vx + std::sin (h.age * 2.3f + h.phase) * 9.0f) * dt;
            h.y += h.vy * dt;
            h.rot += h.vrot * dt;
        }
        hearts.erase (std::remove_if (hearts.begin(), hearts.end(), [] (const Heart& h) { return h.age >= h.life; }), hearts.end());
    }

    // 0..1 opacity (quick fade-in, long gradual fade-out)
    static float alphaOf (const Heart& h)
    {
        const float t = h.age / h.life;
        return std::min (1.0f, h.age / 0.18f) * (t < 0.55f ? 1.0f : std::max (0.0f, 1.0f - (t - 0.55f) / 0.45f));
    }

    const std::vector<Heart>& get() const { return hearts; }
    int totalSpawned = 0;

private:
    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; }
    void spawnOne()
    {
        const float side = rnd() < 0.5f ? -1.0f : 1.0f;
        Heart h;
        h.x = 150.0f + side * (52.0f + rnd() * 62.0f);       // beside the head, never over the controls
        h.y = 96.0f - rnd() * 34.0f;
        h.vx = side * (2.0f + rnd() * 6.0f);
        h.vy = -(22.0f + rnd() * 20.0f);
        h.rot = (rnd() - 0.5f) * 0.7f;
        h.vrot = (rnd() - 0.5f) * 1.3f;
        h.size = 8.0f + rnd() * 11.0f;                         // different sizes
        h.age = 0.0f; h.life = 1.9f + rnd() * 0.9f; h.phase = rnd() * 6.28f;
        hearts.push_back (h); ++totalSpawned;
    }

    std::vector<Heart> hearts;
    int pending = 0;
    float sinceSpawn = 1.0f;
    uint32_t seed = 0x1234abcdu;
};

class MascotEngine
{
public:
    // Internal tuning (not user controls)
    static constexpr float kExcitedOn  = 0.80f;  // normalised level (~ -17 dBFS)
    static constexpr float kExcitedOff = 0.70f;  // hysteresis
    static constexpr float kSleepHold  = 1.2f;   // seconds of silence before going back to sleep

    MascotState state = MascotState::Sleeping;
    MascotPose pose;
    HeartSystem hearts;
    float time = 0.0f;
    float stateTime = 0.0f;

    // Cosmetic click interaction (never touches audio)
    void poke() { pokeVel = 95.0f; hearts.burst (3); }

    void update (float dt, bool active, float level)
    {
        dt = std::min (dt, 0.1f);
        time += dt; stateTime += dt;
        silence = active ? 0.0f : silence + dt;
        smoothedLevel += (level - smoothedLevel) * (1.0f - std::exp (-6.0f * dt));

        switch (state)
        {
            case MascotState::Sleeping:
                if (active) enter (MascotState::WakingUp);
                break;
            case MascotState::WakingUp:
                if (! active && silence > 0.8f) enter (MascotState::ReturningToSleep);
                else if (pose.eyeOpen > 0.88f && stateTime > 1.1f) enter (MascotState::Happy);
                break;
            case MascotState::Happy:
                if (silence > kSleepHold) enter (MascotState::ReturningToSleep);
                else if (level > kExcitedOn && stateTime > 0.6f) enter (MascotState::Excited);
                break;
            case MascotState::Excited:
                if (silence > kSleepHold) enter (MascotState::ReturningToSleep);
                else if (level < kExcitedOff && stateTime > 0.9f) enter (MascotState::Happy);
                break;
            case MascotState::ReturningToSleep:
                if (active) enter (MascotState::WakingUp);
                else if (pose.eyeOpen < 0.03f && stateTime > 0.8f) enter (MascotState::Sleeping);
                break;
        }

        // ---- targets per state
        struct T { float eye, brow, tilt, smile, mouth, blush, lift, headTilt, wBase, wAmp, wFreq, tAmp, tFreq, bAmp, bFreq, paw, z, sparkle, eyeRate, wingRate; };
        T t {};
        switch (state)
        {   //          eye   brow  tilt  smile mouth blush lift  hTilt wBase wAmp wFreq tAmp tFreq bAmp bFreq paw  z    spk  eyeR wingR
            case MascotState::Sleeping:          t = { 0.0f, -0.3f, -0.2f, -0.15f, 0.00f, 0.15f, -3.0f, 0.05f, 56.0f,  0.0f, 0.0f,  3.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 3.0f, 2.0f }; break;
            case MascotState::WakingUp:          t = { 0.95f,  0.6f, -0.1f,  0.10f, 0.28f, 0.30f,  4.0f, -0.04f, 22.0f,  9.0f, 2.0f,  6.0f, 1.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.4f, 2.5f }; break;
            case MascotState::Happy:             t = { 0.93f,  0.35f, 0.55f, 0.85f, 0.00f, 0.55f,  2.0f,  0.00f, 12.0f, 14.0f + 8.0f * smoothedLevel, 2.2f, 16.0f, 2.6f, 2.2f, 2.2f, 0.0f, 0.0f, 0.0f, 4.0f, 3.0f }; break;
            case MascotState::Excited:           t = { 1.00f,  0.85f, 0.35f, 1.00f, 0.65f, 0.85f,  6.0f,  0.00f,  4.0f, 30.0f, 4.6f, 26.0f, 5.0f, 5.0f, 3.2f, 3.0f, 0.0f, 1.0f, 8.0f, 6.0f }; break;
            case MascotState::ReturningToSleep:  t = { 0.00f, -0.2f, -0.2f, -0.05f, 0.00f, 0.25f, -2.0f,  0.04f, 52.0f,  0.0f, 0.0f,  4.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.2f, 1.6f }; break;
        }

        const bool awake = state == MascotState::Happy || state == MascotState::Excited;
        approach (pose.eyeOpen, t.eye * blinkFactor (dt, awake), dt, t.eyeRate);
        approach (pose.sparkle, t.sparkle, dt, 5.0f);
        approach (pose.browRaise, t.brow, dt, 4.0f);
        approach (pose.browTilt, t.tilt, dt, 4.0f);
        approach (pose.smile, t.smile, dt, 4.0f);
        approach (pose.mouthOpen, t.mouth, dt, 6.0f);
        approach (pose.blush, t.blush, dt, 2.5f);
        approach (pose.headLift, t.lift, dt, 3.5f);
        approach (pose.headTilt, t.headTilt, dt, 3.5f);
        approach (pose.wingBase, t.wBase, dt, t.wingRate);
        approach (pose.wingAmp, t.wAmp, dt, 3.0f);
        approach (pose.tailAmp, t.tAmp, dt, 3.0f);
        approach (pose.zAlpha, t.z, dt, 2.0f);
        approach (pose.haloGlow, awake ? 1.0f : 0.45f, dt, 2.0f);
        approach (pose.pawPat, t.paw, dt, 4.0f);

        // oscillators run at state-dependent speeds (frequency eases, so no phase jumps)
        approach (wingFreq, t.wFreq, dt, 3.0f);
        approach (tailFreq, t.tFreq, dt, 3.0f);
        approach (bounceFreq, t.bFreq, dt, 3.0f);
        approach (bounceAmp, t.bAmp * (0.55f + 0.45f * smoothedLevel), dt, 4.0f);
        pose.wingPhase += dt * 6.2831853f * wingFreq;
        pose.tailPhase += dt * 6.2831853f * tailFreq;
        bouncePhase += dt * 6.2831853f * bounceFreq * 0.5f; // |sin| gives one hop per half period
        breathPhase += dt * 6.2831853f * 0.32f;

        // hop = |sin| arc + damped spring from clicks
        pokeVel -= (pokeY * 260.0f + pokeVel * 11.0f) * dt; pokeY += pokeVel * dt * 0.05f;
        pose.bounce = bounceAmp * std::abs (std::sin (bouncePhase)) + std::max (0.0f, pokeY);
        pose.breath = std::sin (breathPhase) * (awake ? 0.35f : 1.0f);
        pose.lookX = awake ? std::sin (time * 0.7f) * 0.5f : 0.0f;

        // ---- hearts: limited, never constant
        if (awake)
        {
            heartTimer -= dt;
            if (heartTimer <= 0.0f) { hearts.trickle(); heartTimer = (state == MascotState::Excited ? 0.9f : 1.7f) + 0.8f * nextRand(); }
        }
        hearts.update (dt);
    }

    float smoothedLevel = 0.0f;

private:
    void enter (MascotState s)
    {
        const auto prev = state;
        state = s; stateTime = 0.0f;
        if (s == MascotState::WakingUp && prev == MascotState::Sleeping) hearts.burst (2);
        if (s == MascotState::Excited && time - lastBurst > 2.5f) { hearts.burst (6); pokeVel = std::max (pokeVel, 70.0f); lastBurst = time; }  // small explosion + hop
        if (s == MascotState::Happy && prev == MascotState::WakingUp) { hearts.burst (2); heartTimer = 1.2f; }
        if (s == MascotState::Sleeping) hearts.clear();
    }

    static void approach (float& v, float target, float dt, float rate) { v += (target - v) * (1.0f - std::exp (-rate * dt)); }

    float nextRand() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; }

    // returns a multiplier (1 = no blink); awake states blink every 2.5..5.5 s for ~0.16 s
    float blinkFactor (float dt, bool awake)
    {
        if (! awake) { blinkT = -1.0f; nextBlink = 2.0f; return 1.0f; }
        if (blinkT >= 0.0f)
        {
            blinkT += dt;
            if (blinkT > 0.16f) { blinkT = -1.0f; nextBlink = 2.5f + 3.0f * nextRand(); return 1.0f; }
            const float x = blinkT / 0.16f;
            return 1.0f - std::sin (x * 3.14159265f);
        }
        nextBlink -= dt;
        if (nextBlink <= 0.0f) blinkT = 0.0f;
        return 1.0f;
    }

    float silence = 0.0f, heartTimer = 1.0f, lastBurst = -10.0f;
    float wingFreq = 0, tailFreq = 0.5f, bounceFreq = 0, bounceAmp = 0, bouncePhase = 0, breathPhase = 0;
    float pokeY = 0.0f, pokeVel = 0.0f, blinkT = -1.0f, nextBlink = 2.0f;
    uint32_t seed = 0x9e3779b9u;
};

} // namespace babyair
