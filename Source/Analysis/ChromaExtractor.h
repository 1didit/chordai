#pragma once

// FROZEN CONTRACT — do not change shape without updating Phase 4 plans
// ChromaFrame/ChromaSequence are consumed by 03-04/03-05/03-06.
// Owned by plan 03-02.

#include "ConstantQAnalysis.h"
#include <array>
#include <vector>

struct ChromaFrame
{
    std::array<float, 12> harmonic {};  // full-range fold (>= ~80 Hz), tuning-corrected, L2-normalized
    std::array<float, 12> bass {};      // 55..250 Hz fold, tuning-corrected, L2-normalized
    float harmonicPreNormL2 = 0.0f;     // pre-normalization norm — silence / N-state detection
    double timeSeconds = 0.0;           // relative to analyzed-region start
};

struct ChromaSequence { std::vector<ChromaFrame> frames; };

// Folds CQT bins into per-frame harmonic (>= ~80 Hz) + bass (55-250 Hz) 12-bin
// chroma, tuning-corrected via tuningCents (see TuningEstimator) and L2-normalized
// with a silence floor. Does NOT apply percussion suppression itself -- callers
// that want it must run suppressPercussion(cqt, ...) on the CqtFrames first
// (see HarmonicPercussiveFilter.h); expected call order is:
//   computeCqt -> suppressPercussion -> extractChroma
ChromaSequence extractChroma (const CqtFrames& cqt, double tuningCents);
