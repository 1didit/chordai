#pragma once

// Owned by plan 03-04. Skeleton created in 03-01 for build wiring only.

#include "AnalysisResult.h"
#include "ChromaExtractor.h"
#include <array>

std::array<float, 12> accumulateChroma (const ChromaSequence& chroma);
KeyResult detectKey (const std::array<float, 12>& accumulatedChroma);
