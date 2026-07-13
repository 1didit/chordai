#pragma once

// The one public MidiGen entry point PluginProcessor calls (GEN-01/GEN-09/
// GEN-11). Phase 5's fixed generateAllRows (AsIsRowGenerator/
// StyleVoicingGenerators) is retired as of 06.1-05 -- generateGenreRows/
// regenerateRow over the genre-data-driven PatternEngine replace it.

#include "../Analysis/AnalysisResult.h"
#include "GenerationSettings.h"
#include "GenreRegistry.h"
#include "MidiSetRow.h"

#include <cstdint>
#include <vector>

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
