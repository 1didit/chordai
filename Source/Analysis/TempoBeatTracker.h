#pragma once

// FROZEN CONTRACT — do not change shape without updating Phase 4 plans
// BeatGrid is consumed by 03-05/03-06.
// Owned by plan 03-03.

#include "OnsetEnvelope.h"
#include <vector>

// v1 DELIBERATE LIMITATION (03-RESEARCH.md Open Question 2): no true
// downbeat/meter detection. barStartBeatIndices simply marks every 4th
// detected beat, treating the first detected beat as downbeat 1 of bar 1 -
// this assumes 4/4 time and does not verify it. Accuracy on 3/4, 6/8, or
// pickup-beat material is unvalidated. Downstream consumers (chord
// segmentation in 03-05, bar-aligned MIDI export in Phases 4-6) must not
// assume this field reflects genuine musical downbeats.
struct BeatGrid
{
    double bpm = 0.0;
    std::vector<double> beatTimesSeconds;   // relative to analyzed-region start
    std::vector<int> barStartBeatIndices;   // v1: every 4th beat (4/4 assumption)
};

BeatGrid trackBeats (const OnsetEnvelopeResult& onset);
