#pragma once

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace bv
{

// Cascaded 2x windowed-sinc halfbands. factor is fixed at 4x so the
// triode and 6L6 nonlinearities are not folding back into the guitar band.
class Oversampler4x
{
public:
    void prepare (int maxBlockIn)
    {
        maxBlock = std::max (256, maxBlockIn);
        up2.assign (static_cast<size_t> (maxBlock * 2), 0.f);
        up4.assign (static_cast<size_t> (maxBlock * 4), 0.f);
        upA.design();
        upB.design();
        downA.design();
        downB.design();
        reset();
        measureLatency();
        reset();
    }

    void reset() noexcept
    {
        upA.reset();
        upB.reset();
        downA.reset();
        downB.reset();
    }

    int latencySamples() const noexcept { return latency; }

    template <typename Fn>
    void process (const float* input, float* output, int numSamples, Fn&& nonlinear)
    {
        if (numSamples <= 0)
            return;

        const int n = std::min (numSamples, maxBlock);
        const int n2 = n * 2;
        const int n4 = n * 4;

        for (int i = 0; i < n; ++i)
        {
            up2[static_cast<size_t> (i * 2)] = upA.push (input[i]) * 2.f;
            up2[static_cast<size_t> (i * 2 + 1)] = upA.push (0.f) * 2.f;
        }

        for (int i = 0; i < n2; ++i)
        {
            up4[static_cast<size_t> (i * 2)] = upB.push (up2[static_cast<size_t> (i)]) * 2.f;
            up4[static_cast<size_t> (i * 2 + 1)] = upB.push (0.f) * 2.f;
        }

        nonlinear (up4.data(), n4);

        for (int i = 0; i < n4; ++i)
        {
            const float y = downB.push (up4[static_cast<size_t> (i)]);
            if ((i & 1) == 1)
                up2[static_cast<size_t> (i >> 1)] = y;
        }

        for (int i = 0; i < n2; ++i)
        {
            const float y = downA.push (up2[static_cast<size_t> (i)]);
            if ((i & 1) == 1)
                output[i >> 1] = y;
        }
    }

private:
    struct Halfband
    {
        static constexpr int N = 63;
        float h[N] {};
        float hist[N] {};
        int pos = 0;

        void design() noexcept
        {
            constexpr int M = (N - 1) / 2;
            constexpr float fc = 0.25f;
            float sum = 0.f;

            for (int i = 0; i < N; ++i)
            {
                const float x = static_cast<float> (i - M);
                const float sinc = (std::abs (x) < 1.0e-6f)
                                       ? 2.f * fc
                                       : std::sin (2.f * kPi * fc * x) / (kPi * x);
                const float w = 0.42f
                              - 0.5f * std::cos (2.f * kPi * static_cast<float> (i) / static_cast<float> (N - 1))
                              + 0.08f * std::cos (4.f * kPi * static_cast<float> (i) / static_cast<float> (N - 1));
                h[i] = sinc * w;
                sum += h[i];
            }

            const float inv = (sum == 0.f) ? 1.f : 1.f / sum;
            for (int i = 0; i < N; ++i)
                h[i] *= inv;

            reset();
        }

        void reset() noexcept
        {
            std::fill (std::begin (hist), std::end (hist), 0.f);
            pos = 0;
        }

        float push (float x) noexcept
        {
            hist[pos] = x;
            float y = 0.f;
            int j = pos;

            for (int i = 0; i < N; ++i)
            {
                y += h[i] * hist[j];
                if (--j < 0)
                    j = N - 1;
            }

            if (++pos >= N)
                pos = 0;

            return y;
        }
    };

    void measureLatency()
    {
        std::vector<float> impulse (256, 0.f);
        std::vector<float> observed (256, 0.f);
        impulse[0] = 1.f;
        process (impulse.data(), observed.data(), 256, [] (float*, int) {});

        int peak = 0;
        for (int i = 1; i < 256; ++i)
            if (std::abs (observed[static_cast<size_t> (i)]) > std::abs (observed[static_cast<size_t> (peak)]))
                peak = i;

        latency = peak;
    }

    Halfband upA, upB, downA, downB;
    std::vector<float> up2;
    std::vector<float> up4;
    int maxBlock = 256;
    int latency = 0;
};

} // namespace bv
