#pragma once

// Owned by plan 03-02. Skeleton created in 03-01 for build wiring only.

#include "ConstantQAnalysis.h"

// Default median-filter kernel length in seconds (Fitzgerald 2010-style harmonic
// enhancement). Research flags 200-400ms as the tunable range; callers may pass
// a different value to suppressPercussion, this is simply the recommended default.
constexpr double kHpssKernelSeconds = 0.3;

// Per-frequency-bin (row) median filter along the TIME axis of cqt.columns, in
// place. Suppresses percussive (broadband, transient) energy while preserving
// harmonic (narrowband, sustained) energy. Call BEFORE extractChroma() if
// percussion suppression is desired -- extractChroma() itself never calls this.
void suppressPercussion (CqtFrames& cqt, double kernelSeconds);
