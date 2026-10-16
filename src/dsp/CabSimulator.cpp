#include "CabSimulator.h"

#include <algorithm>
#include <cmath>

namespace bv
{

void CabSimulator::prepare (double sampleRate)
{
    sr = static_cast<float> (std::max (8000.0, sampleRate));
    length = std::clamp (static_cast<int> (std::lround (sampleRate * 0.055)), 256, 16384);
    ir.assign (static_cast<size_t> (length), 0.f);
    irPrev.assign (static_cast<size_t> (length), 0.f);
    scratch.assign (static_cast<size_t> (length), 0.f);
    history.assign (static_cast<size_t> (length), 0.f);
    widthDelay.assign (static_cast<size_t> (std::max (64, static_cast<int> (sampleRate * 0.004))), 0.f);
    sideHp.setLowpassHz (200.f, sr);
    mixCoeff = 1.f - std::exp (-1.f / (0.012f * sr));
    historyPos = 0;
    widthWrite = 0;
    hasIr = false;
    xfade = 0;
    irDirty = true;
    cabMix = enabled ? 1.f : 0.f;
}

void CabSimulator::reset()
{
    std::fill (history.begin(), history.end(), 0.f);
    std::fill (widthDelay.begin(), widthDelay.end(), 0.f);
    sideHp.reset();
    historyPos = 0;
    widthWrite = 0;
    xfade = 0;
}

void CabSimulator::setControls (float micPosition, bool slantCabinet, bool cabEnabled, float stereoWidth)
{
    const float nextMic = std::clamp (micPosition, 0.f, 1.f);
    if (std::abs (nextMic - mic) > 0.02f || slantCabinet != slant)
    {
        mic = nextMic;
        slant = slantCabinet;
        irDirty = true;
    }

    enabled = cabEnabled;
    width = std::clamp (stereoWidth, 0.f, 1.f);
}

float CabSimulator::convolve (const std::vector<float>& coeff) const noexcept
{
    const int n = length;
    if (n <= 0)
        return 0.f;

    float y = 0.f;
    int j = historyPos;
    const float* h = history.data();
    const float* c = coeff.data();

    for (int i = 0; i < n; ++i)
    {
        y += c[i] * h[j];
        if (--j < 0)
            j = n - 1;
    }

    return y;
}

void CabSimulator::rebuild()
{
    const int n = length;
    std::fill (ir.begin(), ir.end(), 0.f);
    ir[0] = 1.f;

    const float micT = std::clamp (mic, 0.f, 1.f);
    const float barkDb = 7.4f - micT * 4.2f;
    const float biteDb = 4.4f - micT * 3.4f + (slant ? 1.15f : 0.f);
    const float lowDb = slant ? 3.15f : 5.7f;
    const float lowHz = slant ? 118.f : 104.f;
    const float proximity = 2.6f * (1.f - micT);
    const float lowpassHz = 7800.f - micT * 4200.f;

    auto run = [&] (auto&& setup) {
        setup (work);
        work.reset();
        for (int i = 0; i < n; ++i)
            ir[static_cast<size_t> (i)] = work.process (ir[static_cast<size_t> (i)]);
    };

    // Sealed oversized box: the big internal volume parks the resonance just
    // above 100 Hz, then the Vintage 30 cone takes over above 2 kHz and is
    // gone by the time a guitar speaker should be gone.
    run ([&] (Biquad& b) { b.setHighpass (74.f, 0.74f, sr); });
    run ([&] (Biquad& b) { b.setPeak (lowHz, 1.28f, lowDb, sr); });
    run ([&] (Biquad& b) { b.setPeak (145.f, 0.65f, proximity, sr); });
    run ([&] (Biquad& b) { b.setPeak (228.f, 2.5f, 1.25f, sr); });
    run ([&] (Biquad& b) { b.setPeak (390.f, 0.62f, -1.7f, sr); });
    run ([&] (Biquad& b) { b.setPeak (545.f, 2.8f, slant ? 0.35f : 0.85f, sr); });
    run ([&] (Biquad& b) { b.setPeak (2350.f, 1.5f, barkDb, sr); });
    run ([&] (Biquad& b) { b.setPeak (3250.f, 2.1f, -2.5f, sr); });
    run ([&] (Biquad& b) { b.setPeak (4550.f, 2.3f, biteDb, sr); });
    run ([&] (Biquad& b) { b.setLowpass (lowpassHz, 0.64f, sr); });
    run ([&] (Biquad& b) { b.setLowpass (lowpassHz * 0.9f, 0.72f, sr); });

    std::copy (ir.begin(), ir.end(), scratch.begin());

    auto addSpeaker = [&] (float delayMs, float gain, float darkenHz) {
        const int delay = std::max (1, static_cast<int> (std::lround (static_cast<double> (sr) * delayMs * 0.001)));
        OnePole dark;
        dark.setLowpassHz (darkenHz, sr);
        for (int i = 0; i < n; ++i)
        {
            const float raw = (i >= delay) ? scratch[static_cast<size_t> (i - delay)] : 0.f;
            ir[static_cast<size_t> (i)] += dark.lowpass (raw) * gain;
        }
    };

    // The close mic mostly hears one cone. The other three arrive late and darker.
    addSpeaker (0.40f, 0.40f, 4600.f);
    addSpeaker (0.86f, 0.22f, 3400.f);
    addSpeaker (1.32f, 0.11f, 2600.f);

    const float tau = 0.024f;
    const int fadeN = std::min (n / 6, static_cast<int> (sr * 0.005f));
    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float> (i) / sr;
        float env = std::exp (-t / tau);
        if (i > n - fadeN)
        {
            const float u = static_cast<float> (n - i) / static_cast<float> (std::max (1, fadeN));
            env *= u * u;
        }
        ir[static_cast<size_t> (i)] *= env;
    }

    // Scale so the loudest note through the cab sits near unity, not the
    // time-domain spike. A resonant 4x12 is much louder at its peaks than
    // an RMS reading of the impulse suggests.
    float peakResponse = 0.f;
    for (int step = 0; step < 40; ++step)
    {
        const float freq = 50.f * std::pow (2.f, static_cast<float> (step) / 6.f);
        if (freq >= sr * 0.42f)
            break;

        const double omega = 2.0 * 3.14159265358979323846 * static_cast<double> (freq) / static_cast<double> (sr);
        const double coefficient = 2.0 * std::cos (omega);
        double s1 = 0.0;
        double s2 = 0.0;
        for (float sample : ir)
        {
            const double s0 = static_cast<double> (sample) + coefficient * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
        peakResponse = std::max (peakResponse, static_cast<float> (std::sqrt (std::max (0.0, power))));
    }

    if (peakResponse > 1.0e-6f)
    {
        const float gain = 0.85f / peakResponse;
        for (float& sample : ir)
            sample *= gain;
    }
}

void CabSimulator::process (const float* input, float* left, float* right, int numSamples)
{
    if (left == nullptr || numSamples <= 0 || length <= 0)
        return;

    if (irDirty)
    {
        if (hasIr)
            std::copy (ir.begin(), ir.end(), irPrev.begin());

        rebuild();
        xfade = hasIr ? std::min (length, static_cast<int> (sr * 0.008f)) : 0;
        hasIr = true;
        irDirty = false;
    }

    const int xfadeStart = std::max (1, xfade);
    const float targetMix = enabled ? 1.f : 0.f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dry = input[i];
        history[static_cast<size_t> (historyPos)] = dry;

        float wet = convolve (ir);
        if (xfade > 0)
        {
            const float old = convolve (irPrev);
            const float remain = static_cast<float> (xfade) / static_cast<float> (xfadeStart);
            wet += (old - wet) * remain;
            --xfade;
        }

        if (++historyPos >= length)
            historyPos = 0;

        cabMix += (targetMix - cabMix) * mixCoeff;
        const float voiced = dry + (wet - dry) * cabMix;

        widthState += (width - widthState) * mixCoeff;
        const float delaySamples = 8.f + widthState * static_cast<float> (sr) * 0.0007f;
        widthDelay[static_cast<size_t> (widthWrite)] = voiced;

        float read = static_cast<float> (widthWrite) - delaySamples;
        const float size = static_cast<float> (widthDelay.size());
        while (read < 0.f)
            read += size;

        const int i0 = static_cast<int> (read) % static_cast<int> (widthDelay.size());
        const int i1 = (i0 + 1) % static_cast<int> (widthDelay.size());
        const float frac = read - std::floor (read);
        const float delayed = widthDelay[static_cast<size_t> (i0)] * (1.f - frac)
                            + widthDelay[static_cast<size_t> (i1)] * frac;
        if (++widthWrite >= static_cast<int> (widthDelay.size()))
            widthWrite = 0;

        // Side energy is the short mic offset. Summing L+R returns the mono cab.
        const float side = sideHp.highpass (delayed - voiced) * widthState * 0.85f;
        left[i] = voiced + side;
        if (right != nullptr)
            right[i] = voiced - side;
    }
}

} // namespace bv
