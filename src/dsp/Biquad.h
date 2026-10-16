#pragma once

#include <algorithm>
#include <cmath>

namespace bv
{

constexpr float kPi = 3.14159265358979323846f;

inline float dbToGain (float db) noexcept
{
    return std::pow (10.f, db * 0.05f);
}

inline float clampFreq (float freq, float sampleRate) noexcept
{
    const float nyquist = 0.45f * sampleRate;
    return std::clamp (freq, 12.f, std::max (13.f, nyquist));
}

// Transposed direct form II. Coefficients are RBJ-cookbook values
// after dividing through by a0.
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    float z1 = 0.f, z2 = 0.f;

    void reset() noexcept { z1 = z2 = 0.f; }

    void setNormalized (float nb0, float nb1, float nb2, float na1, float na2) noexcept
    {
        b0 = nb0;
        b1 = nb1;
        b2 = nb2;
        a1 = na1;
        a2 = na2;
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (float freq, float q, float sampleRate) noexcept
    {
        const float w0 = 2.f * kPi * clampFreq (freq, sampleRate) / sampleRate;
        const float cosw = std::cos (w0);
        const float sinw = std::sin (w0);
        const float alpha = sinw / (2.f * std::max (0.15f, q));
        const float a0 = 1.f + alpha;
        const float inv = 1.f / a0;
        const float oneMinus = 1.f - cosw;
        setNormalized ((oneMinus * 0.5f) * inv,
                       oneMinus * inv,
                       (oneMinus * 0.5f) * inv,
                       (-2.f * cosw) * inv,
                       (1.f - alpha) * inv);
    }

    void setHighpass (float freq, float q, float sampleRate) noexcept
    {
        const float w0 = 2.f * kPi * clampFreq (freq, sampleRate) / sampleRate;
        const float cosw = std::cos (w0);
        const float sinw = std::sin (w0);
        const float alpha = sinw / (2.f * std::max (0.15f, q));
        const float a0 = 1.f + alpha;
        const float inv = 1.f / a0;
        const float onePlus = 1.f + cosw;
        setNormalized ((onePlus * 0.5f) * inv,
                       (-onePlus) * inv,
                       (onePlus * 0.5f) * inv,
                       (-2.f * cosw) * inv,
                       (1.f - alpha) * inv);
    }

    void setPeak (float freq, float q, float db, float sampleRate) noexcept
    {
        const float A = std::pow (10.f, db / 40.f);
        const float w0 = 2.f * kPi * clampFreq (freq, sampleRate) / sampleRate;
        const float cosw = std::cos (w0);
        const float sinw = std::sin (w0);
        const float alpha = sinw / (2.f * std::max (0.15f, q));
        const float a0 = 1.f + alpha / A;
        const float inv = 1.f / a0;
        setNormalized ((1.f + alpha * A) * inv,
                       (-2.f * cosw) * inv,
                       (1.f - alpha * A) * inv,
                       (-2.f * cosw) * inv,
                       (1.f - alpha / A) * inv);
    }

    void setLowShelf (float freq, float db, float sampleRate) noexcept
    {
        const float A = std::pow (10.f, db / 40.f);
        const float w0 = 2.f * kPi * clampFreq (freq, sampleRate) / sampleRate;
        const float cosw = std::cos (w0);
        const float sinw = std::sin (w0);
        const float alpha = (sinw * 0.5f) * std::sqrt (2.f);
        const float two = 2.f * std::sqrt (A) * alpha;
        const float a0 = (A + 1.f) + (A - 1.f) * cosw + two;
        const float inv = 1.f / a0;
        setNormalized ((A * ((A + 1.f) - (A - 1.f) * cosw + two)) * inv,
                       (2.f * A * ((A - 1.f) - (A + 1.f) * cosw)) * inv,
                       (A * ((A + 1.f) - (A - 1.f) * cosw - two)) * inv,
                       (-2.f * ((A - 1.f) + (A + 1.f) * cosw)) * inv,
                       ((A + 1.f) + (A - 1.f) * cosw - two) * inv);
    }

    void setHighShelf (float freq, float db, float sampleRate) noexcept
    {
        const float A = std::pow (10.f, db / 40.f);
        const float w0 = 2.f * kPi * clampFreq (freq, sampleRate) / sampleRate;
        const float cosw = std::cos (w0);
        const float sinw = std::sin (w0);
        const float alpha = (sinw * 0.5f) * std::sqrt (2.f);
        const float two = 2.f * std::sqrt (A) * alpha;
        const float a0 = (A + 1.f) - (A - 1.f) * cosw + two;
        const float inv = 1.f / a0;
        setNormalized ((A * ((A + 1.f) + (A - 1.f) * cosw + two)) * inv,
                       (-2.f * A * ((A - 1.f) + (A + 1.f) * cosw)) * inv,
                       (A * ((A + 1.f) + (A - 1.f) * cosw - two)) * inv,
                       (2.f * ((A - 1.f) - (A + 1.f) * cosw)) * inv,
                       ((A + 1.f) - (A - 1.f) * cosw - two) * inv);
    }
};

struct OnePole
{
    float coeff = 0.f;
    float z = 0.f;

    void reset() noexcept { z = 0.f; }

    void setLowpassHz (float freq, float sampleRate) noexcept
    {
        const float hz = clampFreq (freq, sampleRate);
        coeff = 1.f - std::exp (-2.f * kPi * hz / sampleRate);
    }

    float lowpass (float x) noexcept
    {
        z += (x - z) * coeff;
        return z;
    }

    float highpass (float x) noexcept
    {
        return x - lowpass (x);
    }
};

struct DcBlock
{
    float r = 0.995f;
    float x1 = 0.f;
    float y1 = 0.f;

    void reset() noexcept { x1 = y1 = 0.f; }

    void prepare (float sampleRate, float hz = 12.f) noexcept
    {
        r = std::exp (-2.f * kPi * hz / sampleRate);
    }

    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

} // namespace bv
