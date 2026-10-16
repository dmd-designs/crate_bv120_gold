#pragma once

#include "Biquad.h"

#include <vector>

namespace bv
{

// Sealed oversized 4x12, about 32.9 x 30.1 x 14.3 inches, rear-mounted
// Celestion Vintage 30s. Straight is the fully coupled baffle; slant aims
// the top pair and gives up a little of the low resonance for more bite.
class CabSimulator
{
public:
    void prepare (double sampleRate);
    void reset();
    void setControls (float micPosition, bool slant, bool enabled, float stereoWidth);
    void process (const float* input, float* left, float* right, int numSamples);

    const std::vector<float>& impulse() const noexcept { return ir; }

private:
    void rebuild();
    float convolve (const std::vector<float>& coeff) const noexcept;

    float sr = 48000.f;
    int length = 0;
    bool irDirty = true;
    bool hasIr = false;
    bool enabled = true;
    bool slant = false;
    float mic = 0.32f;
    float width = 0.3f;
    float widthState = 0.3f;
    float cabMix = 1.f;
    float mixCoeff = 0.01f;
    int xfade = 0;
    int historyPos = 0;

    Biquad work;
    OnePole sideHp;
    std::vector<float> ir;
    std::vector<float> irPrev;
    std::vector<float> scratch;
    std::vector<float> history;
    std::vector<float> widthDelay;
    int widthWrite = 0;
};

} // namespace bv
