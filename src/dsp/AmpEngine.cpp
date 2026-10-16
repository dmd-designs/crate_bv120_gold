#include "AmpEngine.h"

#include <algorithm>
#include <cmath>

namespace bv
{
namespace
{
constexpr float kCh1BassRange = 16.f;
constexpr float kCh1MidRange = 20.f;
constexpr float kCh1TrebleRange = 10.f;
constexpr float kCh2BassRange = 15.f;
constexpr float kCh2MidRange = 13.f;
constexpr float kCh2TrebleRange = 10.f;

// Shelf midpoints sit above/below the manual's measurement frequencies so the
// quoted band (100 Hz, 10 kHz) is near the shelf asymptote rather than the knee.
constexpr float kBassShelfHz = 340.f;
constexpr float kTrebleShelfHz = 2200.f;
constexpr float kPresenceHz = 2800.f;
constexpr float kCh1MidHz = 300.f;
constexpr float kCh2MidHz = 500.f;
constexpr float kMidQ = 0.85f;

float knobToToneDb (float knob, float range) noexcept
{
    return (std::clamp (knob, 0.f, 1.f) - 0.5f) * range;
}

float audioTaper (float knob) noexcept
{
    const float k = std::clamp (knob, 0.f, 1.f);
    return k * k;
}
} // namespace

void AmpEngine::prepare (double sampleRate)
{
    sr = static_cast<float> (std::max (8000.0, sampleRate));
    smoothCoeff = 1.f - std::exp (-1.f / (0.03f * sr));
    sagAttack = 1.f - std::exp (-1.f / (0.008f * sr));
    sagRelease = 1.f - std::exp (-1.f / (0.140f * sr));
    gateAttack = 1.f - std::exp (-1.f / (0.0015f * sr));
    gateRelease = 1.f - std::exp (-1.f / (0.160f * sr));
    springDamp = 1.f - std::exp (-2.f * kPi * 3400.f / sr);
    lfoInc = 2.f * kPi * 0.62f / sr;

    dcIn.prepare (sr, 12.f);
    dcOut.prepare (sr, 8.f);
    inputStage.prepare (sr, 75.f, 12000.f);
    cleanStage.prepare (sr, 85.f, 9000.f);
    drive[0].prepare (sr, 90.f, 9800.f);
    drive[1].prepare (sr, 140.f, 7000.f);
    drive[2].prepare (sr, 170.f, 5400.f);
    drive[3].prepare (sr, 80.f, 6800.f);

    springInHp.setLowpassHz (280.f, sr);
    springInLp.setLowpassHz (5200.f, sr);
    gateEnvLp.setLowpassHz (40.f, sr);

    const float timesMs[4] = { 29.5f, 41.0f, 53.5f, 67.8f };
    for (int i = 0; i < 4; ++i)
    {
        springs[i].baseDelay = std::max (8, static_cast<int> (sr * timesMs[i] * 0.001f));
        springs[i].buffer.assign (static_cast<size_t> (springs[i].baseDelay + 96), 0.f);
        springs[i].write = 0;
        springs[i].dampState = 0.f;
    }

    snap = true;
    coeffCountdown = 1;
    reset();
    updateFilters();
}

void AmpEngine::reset()
{
    dcIn.reset();
    dcOut.reset();
    inputStage.reset();
    cleanStage.reset();
    for (auto& stage : drive)
        stage.reset();
    eq1.reset();
    eq2.reset();
    driveEmphasis.reset();
    presenceFilter.reset();
    thump.reset();
    otHighpass.reset();
    otLowpass.reset();
    springInHp.reset();
    springInLp.reset();
    gateEnvLp.reset();
    powerSag = 0.f;
    gateEnv = 0.f;
    gateGain = 1.f;
    lfoPhase = 0.f;

    for (auto& spring : springs)
    {
        std::fill (spring.buffer.begin(), spring.buffer.end(), 0.f);
        spring.write = 0;
        spring.dampState = 0.f;
    }
}

void AmpEngine::setSettings (const AmpSettings& settings)
{
    target = settings;
    if (! snap)
        return;

    inKnob = target.input;
    channel = target.channel;
    presence = target.presence;
    b1 = target.ch1Bass;
    m1 = target.ch1Mid;
    t1 = target.ch1Treble;
    l1 = target.ch1Level;
    r1 = target.ch1Reverb;
    g2 = target.ch2Gain;
    b2 = target.ch2Bass;
    m2 = target.ch2Mid;
    t2 = target.ch2Treble;
    l2 = target.ch2Level;
    r2 = target.ch2Reverb;
    gateKnob = target.gate;
    outKnob = target.output;
    snap = false;
    coeffCountdown = 1;
    updateFilters();
}

float AmpEngine::smoothToward (float current, float targetValue) const noexcept
{
    return current + (targetValue - current) * smoothCoeff;
}

float AmpEngine::triodeCurve (float x, float drive, float asymmetry, float bias) noexcept
{
    const float u = x * drive + bias;
    const float positiveKnee = 1.05f + 0.85f * asymmetry;
    const float negativeKnee = 0.48f + 0.32f * (1.f - asymmetry);
    const float positive = std::tanh (positiveKnee * u) / positiveKnee;
    const float negative = std::tanh (negativeKnee * u) / negativeKnee;
    const float blend = 0.5f + 0.5f * std::tanh (u * 4.0f);
    return negative + (positive - negative) * blend;
}

float AmpEngine::runTriode (Triode& stage, float x, float driveAmount, float asymmetry, float bias, float sagAmount) noexcept
{
    const float coupled = stage.hp.highpass (x);
    const float grid = std::max (0.f, coupled * driveAmount + bias - 0.65f);
    stage.sag += (grid - stage.sag) * (grid > stage.sag ? sagAttack : sagRelease);
    const float y = triodeCurve (coupled - stage.sag * sagAmount * 0.15f, driveAmount, asymmetry, bias);
    return stage.lp.lowpass (y);
}

void AmpEngine::updateFilters()
{
    eq1.bass.setLowShelf (kBassShelfHz, knobToToneDb (b1, kCh1BassRange), sr);
    eq1.mid.setPeak (kCh1MidHz, kMidQ, knobToToneDb (m1, kCh1MidRange), sr);
    eq1.treble.setHighShelf (kTrebleShelfHz, knobToToneDb (t1, kCh1TrebleRange), sr);

    eq2.bass.setLowShelf (kBassShelfHz, knobToToneDb (b2, kCh2BassRange), sr);
    eq2.mid.setPeak (kCh2MidHz, kMidQ, knobToToneDb (m2, kCh2MidRange), sr);
    eq2.treble.setHighShelf (kTrebleShelfHz, knobToToneDb (t2, kCh2TrebleRange), sr);

    // Lead channel picks up upper-mid energy before the clippers, which the
    // speaker later turns into attack instead of fizz.
    driveEmphasis.setHighShelf (1650.f, 3.6f, sr);
    presenceFilter.setHighShelf (kPresenceHz, std::clamp (presence, 0.f, 1.f) * 5.f, sr);
    thump.setLowShelf (150.f, 2.4f, sr);
    otHighpass.setHighpass (38.f, 0.70f, sr);
    otLowpass.setLowpass (11500.f, 0.70f, sr);

    const float g = 0.08f + 0.92f * std::pow (std::clamp (g2, 0.f, 1.f), 1.05f);
    drive[0].hp.setLowpassHz (80.f + 20.f * g, sr);
    drive[1].hp.setLowpassHz (110.f + 80.f * g, sr);
    drive[2].hp.setLowpassHz (130.f + 90.f * g, sr);
    drive[0].lp.setLowpassHz (10500.f - 1800.f * g, sr);
    drive[1].lp.setLowpassHz (8200.f - 2200.f * g, sr);
    drive[2].lp.setLowpassHz (6400.f - 1800.f * g, sr);
}

float AmpEngine::springProcess (float x) noexcept
{
    lfoPhase += lfoInc;
    if (lfoPhase > 2.f * kPi)
        lfoPhase -= 2.f * kPi;

    float sum = 0.f;
    for (int i = 0; i < 4; ++i)
    {
        auto& line = springs[i];
        const float wobble = std::sin (lfoPhase * (1.f + 0.17f * static_cast<float> (i)) + static_cast<float> (i) * 1.3f);
        const float delay = static_cast<float> (line.baseDelay) + wobble * 7.f;
        float read = static_cast<float> (line.write) - delay;
        const float size = static_cast<float> (line.buffer.size());
        while (read < 0.f)
            read += size;

        const int i0 = static_cast<int> (read) % static_cast<int> (line.buffer.size());
        const int i1 = (i0 + 1) % static_cast<int> (line.buffer.size());
        const float frac = read - std::floor (read);
        const float tap = line.buffer[static_cast<size_t> (i0)] * (1.f - frac)
                        + line.buffer[static_cast<size_t> (i1)] * frac;

        line.dampState += (tap - line.dampState) * springDamp;
        const float recirculating = x * 0.45f + line.dampState * 0.58f;
        line.buffer[static_cast<size_t> (line.write)] = recirculating;
        if (++line.write >= static_cast<int> (line.buffer.size()))
            line.write = 0;

        sum += line.dampState;
    }

    return sum * 0.28f;
}

void AmpEngine::process (float* samples, int numSamples)
{
    for (int n = 0; n < numSamples; ++n)
    {
        inKnob = smoothToward (inKnob, target.input);
        channel = smoothToward (channel, target.channel);
        presence = smoothToward (presence, target.presence);
        b1 = smoothToward (b1, target.ch1Bass);
        m1 = smoothToward (m1, target.ch1Mid);
        t1 = smoothToward (t1, target.ch1Treble);
        l1 = smoothToward (l1, target.ch1Level);
        r1 = smoothToward (r1, target.ch1Reverb);
        g2 = smoothToward (g2, target.ch2Gain);
        b2 = smoothToward (b2, target.ch2Bass);
        m2 = smoothToward (m2, target.ch2Mid);
        t2 = smoothToward (t2, target.ch2Treble);
        l2 = smoothToward (l2, target.ch2Level);
        r2 = smoothToward (r2, target.ch2Reverb);
        gateKnob = smoothToward (gateKnob, target.gate);
        outKnob = smoothToward (outKnob, target.output);

        if (--coeffCountdown <= 0)
        {
            coeffCountdown = 64;
            updateFilters();
        }

        const float inputGain = dbToGain ((inKnob - 0.5f) * 24.f);
        const float keyed = dcIn.process (samples[n] * inputGain);

        float shared = inputStage.hp.highpass (keyed);
        shared = triodeCurve (shared, 1.65f, 0.22f, 0.0f);
        shared = inputStage.lp.lowpass (shared);

        float clean = cleanStage.hp.highpass (shared);
        clean = triodeCurve (clean, 2.15f, 0.28f, 0.02f);
        clean = cleanStage.lp.lowpass (clean);
        clean = eq1.bass.process (clean);
        clean = eq1.mid.process (clean);
        clean = eq1.treble.process (clean);
        clean *= 0.04f + 2.15f * audioTaper (l1);

        const float gain = 0.08f + 0.92f * std::pow (std::clamp (g2, 0.f, 1.f), 1.05f);
        float driveSignal = driveEmphasis.process (shared);
        driveSignal = runTriode (drive[0], driveSignal, 1.05f + 5.6f * gain, 0.30f, 0.0f, 0.20f);
        driveSignal = runTriode (drive[1], driveSignal, 0.85f + 6.8f * gain, 0.82f, -0.14f, 0.55f);
        driveSignal = runTriode (drive[2], driveSignal, 0.7f + 4.2f * gain, 0.42f, 0.06f, 0.28f);
        driveSignal = runTriode (drive[3], driveSignal, 0.6f + 1.45f * gain, 0.16f, 0.0f, 0.08f);
        driveSignal = eq2.bass.process (driveSignal);
        driveSignal = eq2.mid.process (driveSignal);
        driveSignal = eq2.treble.process (driveSignal);
        driveSignal *= 0.03f + 1.55f * audioTaper (l2);

        const float mix = std::clamp (channel, 0.f, 1.f);
        const float cleanGain = std::cos (mix * 0.5f * kPi);
        const float driveGain = std::sin (mix * 0.5f * kPi);
        float mixed = clean * cleanGain + driveSignal * driveGain;

        const float keyLevel = std::abs (keyed);
        gateEnv += (keyLevel - gateEnv) * (keyLevel > gateEnv ? gateAttack : gateRelease);
        const float thresholdDb = -92.f + std::clamp (gateKnob, 0.f, 1.f) * 54.f;
        const float threshold = dbToGain (thresholdDb);
        const float gateTarget = (gateKnob < 0.004f || gateEnv > threshold) ? 1.f : 0.f;
        gateGain += (gateTarget - gateGain) * (gateTarget > gateGain ? 0.08f : gateRelease);
        mixed *= gateGain;

        const float reverbKnob = r1 * cleanGain + r2 * driveGain;
        const float send = springInLp.lowpass (springInHp.highpass (mixed));
        mixed += springProcess (send) * audioTaper (reverbKnob) * 1.15f;

        float power = presenceFilter.process (mixed);
        power = thump.process (power);
        const float env = std::abs (power);
        powerSag += (env - powerSag) * (env > powerSag ? sagAttack : sagRelease);
        // Solid-state rectifier: a little give, not a spongy tube-rectifier sag.
        power *= 1.f / (1.f + powerSag * 0.22f);

        const float positive = std::tanh (1.22f * power) / 1.22f;
        const float negative = std::tanh (1.02f * power) / 1.02f;
        const float blend = 0.5f + 0.5f * std::tanh (power * 5.f);
        power = negative + (positive - negative) * blend;
        power = otHighpass.process (power);
        power = otLowpass.process (power);
        power = dcOut.process (power);

        const float outputGain = dbToGain ((outKnob - 0.5f) * 24.f);
        float out = power * 0.85f * outputGain;
        // Safety ceiling so a resonant cab hit cannot hard-clip the host.
        out = std::tanh (out * 1.15f) / 1.15f;
        samples[n] = out;
    }
}

} // namespace bv
