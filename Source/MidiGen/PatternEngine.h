#pragma once

// Frozen generator signature (06.1-RESEARCH.md Pattern 3) -- body lands in
// plan 06.1-03.

#include "../Analysis/AnalysisResult.h"
#include "GenreRegistry.h"
#include "NoteEvent.h"

#include <cstdint>
#include <vector>

// Pure function: same (result, archetype, seed) => byte-identical output. Seed only ever
// selects among pre-authored rhythm/octave/ornament variants -- it NEVER reads or perturbs
// segment.chord.pitchClass/quality (harmony preservation BY CONSTRUCTION, Pitfall A).
std::vector<NoteEvent> generatePattern (const AnalysisResult& result, const PatternArchetype& archetype, uint32_t seed);
