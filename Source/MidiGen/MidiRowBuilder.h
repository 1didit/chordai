#pragma once

// Frozen generator signature (Wave 2 implements the body only, in plan
// 05-04). The one public entry point PluginProcessor calls.

#include "../Analysis/AnalysisResult.h"
#include "GenerationSettings.h"
#include "GenreRegistry.h"
#include "MidiSetRow.h"

#include <cstdint>
#include <vector>

// Pure function: same AnalysisResult (+ settings) always produces
// byte-identical output (GEN-04 determinism requirement, see 05-RESEARCH.md
// Pitfall 2). Empty AnalysisResult.chords (or all-NoChord) produces 5 rows
// with empty notes vectors, never a crash.
std::vector<MidiSetRow> generateAllRows (const AnalysisResult& result, const GenerationSettings& settings = {});

// Genre-aware row API (06.1-05, GEN-09/GEN-11): builds all 5 slots of one
// genre over the PatternEngine, at the baseline (variationCounter == 0)
// variant for every slot. Pure function -- same (result, genre) always
// produces byte-identical output.
std::vector<MidiSetRow> generateGenreRows (const AnalysisResult& result, const GenreSpec& genre, const GenerationSettings& settings = {});

// Builds the row for patternIndex ONLY, using the caller-supplied
// variationCounter (the caller owns + pre-bumps this counter -- see
// PluginProcessor::regenerateRow, Pitfall B). patternIndex is defensively
// clamped to 0..4 (the processor also guards, but this stays safe standalone).
MidiSetRow regenerateRow (const AnalysisResult& result, const GenreSpec& genre, int patternIndex, uint32_t variationCounter, const GenerationSettings& settings = {});
