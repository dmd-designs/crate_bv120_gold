#pragma once

#include "AmpEngine.h"

namespace bv
{

struct Patch
{
    const char* name;
    AmpSettings amp;
    float mic;
    float width;
    bool slant;
    bool cab;
};

// Knob positions are 0..1. Channel 1 is the rhythm channel, channel 2 is the lead.
// Names follow the suggested settings in the BV120H manual.
inline const Patch kPatches[] = {
    {
        "Crunchy Edge",
        { 0.50f, 1.f, 0.55f, 0.48f, 0.52f, 0.58f, 0.42f, 0.14f, 0.52f, 0.50f, 0.56f, 0.60f, 0.45f, 0.06f, 0.12f, 0.50f },
        0.32f, 0.30f, false, true,
    },
    {
        "Rock Rhythm",
        { 0.50f, 1.f, 0.42f, 0.55f, 0.60f, 0.50f, 0.46f, 0.12f, 0.36f, 0.58f, 0.70f, 0.48f, 0.48f, 0.10f, 0.10f, 0.50f },
        0.40f, 0.28f, false, true,
    },
    {
        "Massive Metal",
        { 0.52f, 1.f, 0.40f, 0.46f, 0.40f, 0.55f, 0.40f, 0.05f, 0.80f, 0.70f, 0.22f, 0.64f, 0.38f, 0.00f, 0.38f, 0.48f },
        0.58f, 0.42f, false, true,
    },
    {
        "Sparkling Clean",
        { 0.48f, 0.f, 0.68f, 0.40f, 0.46f, 0.74f, 0.40f, 0.32f, 0.28f, 0.48f, 0.52f, 0.55f, 0.36f, 0.08f, 0.04f, 0.52f },
        0.22f, 0.28f, false, true,
    },
    {
        "Fat Clean",
        { 0.50f, 0.f, 0.30f, 0.74f, 0.64f, 0.38f, 0.50f, 0.16f, 0.30f, 0.62f, 0.48f, 0.42f, 0.40f, 0.06f, 0.06f, 0.50f },
        0.48f, 0.22f, false, true,
    },
};

inline constexpr int kNumPatches = static_cast<int> (sizeof (kPatches) / sizeof (kPatches[0]));

} // namespace bv
