#pragma once

// Frozen generator signature (Wave 2 implements the body only, in plan
// 05-02). Detected-as-is row: root-position triad/7th per segment, exact
// detected quality, no transformation.

#include "../Analysis/AnalysisResult.h"
#include "NoteEvent.h"

#include <vector>

std::vector<NoteEvent> generateAsIsRow (const AnalysisResult& result);
