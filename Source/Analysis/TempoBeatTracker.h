#pragma once

// FROZEN CONTRACT — do not change shape without updating Phase 4 plans
// BeatGrid is consumed by 03-05/03-06.
// Owned by plan 03-03.

#include "OnsetEnvelope.h"
#include <vector>

struct BeatGrid
{
    double bpm = 0.0;
    std::vector<double> beatTimesSeconds;   // relative to analyzed-region start
    std::vector<int> barStartBeatIndices;   // v1: every 4th beat (4/4 assumption)
};

BeatGrid trackBeats (const OnsetEnvelopeResult& onset);
