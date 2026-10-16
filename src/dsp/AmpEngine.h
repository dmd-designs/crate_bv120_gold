#pragma once

#include "Biquad.h"

#include <vector>

namespace bv
{

// Knob positions are 0..1, matching a 0–10 panel knob (0.5 is noon).
struct AmpSettings
{
    float input = 0.5f;
    float channel = 1.f; // 0 = channel 1 (rhythm), 1 = channel 2 (lead)
    float presence = 0.55f;
    float ch1Bass = 0.48f;
    float ch1Mid = 0.52f;
    float ch1Treble = 0.58f;
    float ch1Level = 0.42f;
    float ch1Reverb = 0.15f;
    float ch2Gain = 0.52f;
    float ch2Bass = 0.50f;
    float ch2Mid = 0.56f;
    float ch2Treble = 0.60f;
    float ch2Level = 0.45f;
    float ch2Reverb = 0.06f;
    float gate = 0.12f;
    float output = 0.5f;
};

// Manual specs this model follows (BV120H owner's manual):
//   Ch1 66 dB system gain class, bass 16 dB @ 100 Hz, mid 20 dB @ 300 Hz, treble 10 dB @ 10 kHz
//   Ch2 101 dB class, four triode stages, bass 15 dB @ 100 Hz, mid 13 dB @ 500 Hz, treble 10 dB @ 10 kHz
//   Presence boost only, up to 5 dB @ 10 kHz, in front of the 6L6 power stage
//   Per-channel spring reverb, solid-state rectified (tight, little sag)
class AmpEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    void setSettings (const AmpSettings& settings);
    void process (float* samples, int numSamples);

private:
    struct Triode
    {
        OnePole hp;
        OnePole lp;
        float sag = 0.f;

        void prepare (float sampleRate, float hpHz, float lpHz)
        {
            hp.setLowpassHz (hpHz, sampleRate);
            lp.setLowpassHz (lpHz, sampleRate);
            sag = 0.f;
        }

        void reset() noexcept
        {
            hp.reset();
            lp.reset();
            sag = 0.f;
        }
    };

    struct Tone
    {
        Biquad bass;
        Biquad mid;
        Biquad treble;

        void reset()
        {
            bass.reset();
            mid.reset();
            treble.reset();
        }
    };

    struct SpringLine
    {
        std::vector<float> buffer;
        int write = 0;
        int baseDelay = 1000;
        float dampState = 0.f;
    };

    float smoothToward (float current, float target) const noexcept;
    static float triodeCurve (float x, float drive, float asymmetry, float bias) noexcept;
    float runTriode (Triode& stage, float x, float driveAmount, float asymmetry, float bias, float sagAmount) noexcept;
    void updateFilters();
    float springProcess (float x) noexcept;

    float sr = 48000.f;
    float smoothCoeff = 0.01f;
    float sagAttack = 0.01f;
    float sagRelease = 0.001f;
    float gateAttack = 0.05f;
    float gateRelease = 0.002f;
    float springDamp = 0.2f;
    float lfoInc = 0.001f;
    float lfoPhase = 0.f;
    int coeffCountdown = 1;
    bool snap = true;

    AmpSettings target;
    float inKnob = 0.5f;
    float channel = 1.f;
    float presence = 0.55f;
    float b1 = 0.5f, m1 = 0.5f, t1 = 0.5f, l1 = 0.5f, r1 = 0.f;
    float g2 = 0.5f, b2 = 0.5f, m2 = 0.5f, t2 = 0.5f, l2 = 0.5f, r2 = 0.f;
    float gateKnob = 0.f;
    float outKnob = 0.5f;

    DcBlock dcIn;
    DcBlock dcOut;
    Triode inputStage;
    Triode cleanStage;
    Triode drive[4];
    Tone eq1;
    Tone eq2;
    Biquad driveEmphasis;
    Biquad presenceFilter;
    Biquad thump;
    Biquad otHighpass;
    Biquad otLowpass;
    OnePole springInHp;
    OnePole springInLp;
    OnePole gateEnvLp;

    float powerSag = 0.f;
    float gateEnv = 0.f;
    float gateGain = 1.f;
    SpringLine springs[4];
};

} // namespace bv
