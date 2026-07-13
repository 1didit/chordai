#pragma once

// Frozen generator signature (Wave 2 implements the body only, in plan
// 05-04). The one public entry point PluginProcessor calls.

#include "../Analysis/AnalysisResult.h"
#include "GenerationSettings.h"
#include "MidiSetRow.h"

#include <vector>

// Pure function: same AnalysisResult (+ settings) always produces
// byte-identical output (GEN-04 determinism requirement, see 05-RESEARCH.md
// Pitfall 2). Empty AnalysisResult.chords (or all-NoChord) produces 5 rows
// with empty notes vectors, never a crash.
std::vector<MidiSetRow> generateAllRows (const AnalysisResult& result, const GenerationSettings& settings = {});
