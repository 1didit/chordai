#pragma once

// Owned by plan 03-05. Skeleton created in 03-01 for build wiring only.

#include "AnalysisResult.h"
#include "ChromaExtractor.h"
#include "TempoBeatTracker.h"
#include <vector>

std::vector<ChordSegment> decodeChords (const ChromaSequence& chroma, const BeatGrid& beats);
