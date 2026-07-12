#pragma once

// Owned by plan 03-05.

#include "AnalysisResult.h"
#include "ChordTemplates.h"
#include "ChromaExtractor.h"
#include "TempoBeatTracker.h"

#include <array>
#include <vector>

// Beat-synchronized chroma: harmonic/bass chroma averaged over all ChromaFrames
// falling in the half-open interval [beatTimesSeconds[i], beatTimesSeconds[i+1)),
// with the last beat's window extended to
// [beatTimesSeconds[last], beatTimesSeconds[last] + medianInterBeatInterval).
// preNormL2Avg is the mean of ChromaFrame::harmonicPreNormL2 over the same
// window -- feeds the N-state silence override in decodeChords (ChordDecoder.cpp).
// A beat interval containing zero frames yields an all-zero BeatChroma
// (treated as silence). This is the decoder's anti-flicker boundary: it never
// scores frame-rate chroma directly (03-RESEARCH.md Pattern 4).
struct BeatChroma
{
    std::array<float, 12> harmonicAvg {};
    std::array<float, 12> bassAvg {};
    float preNormL2Avg = 0.0f;
};

// Collapses frame-rate ChromaSequence into one BeatChroma per beat interval
// (see BeatChroma doc above). Exposed for unit testing.
std::vector<BeatChroma> computeBeatSyncChroma (const ChromaSequence& chroma, const BeatGrid& beats);

// Bass-root-bias weight (gamma in 03-RESEARCH.md's observation-scoring
// formula) -- research starting point in the documented [0.2, 0.3] range
// (Open Question 3), named and tunable.
constexpr float kBassRootBiasWeight = 0.25f;

// Raw score per 36-state template: cosine(harmonicAvg, template) +
// kBassRootBiasWeight * bassAvg[root(template)] / (sum(bassAvg) + eps).
// The 36 raw scores are then L1-normalized (guarding an all-zero row to a
// uniform 1/36 distribution) to form one column of the Viterbi observation
// matrix. Exposed for unit testing.
std::array<float, 36> scoreBeatObservations (const BeatChroma& beat, const std::array<std::array<float, 12>, 36>& templates);

std::vector<ChordSegment> decodeChords (const ChromaSequence& chroma, const BeatGrid& beats);
