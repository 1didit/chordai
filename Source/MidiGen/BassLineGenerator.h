#pragma once

// Frozen generator signature (Wave 2 implements the body only, in plan
// 05-03). Bass row: root pitch class always derived from chord.pitchClass,
// per-style rhythm pattern (GEN-03).

#include "../Analysis/AnalysisResult.h"
#include "NoteEvent.h"

#include <vector>

enum class BassRhythm { TrapSustain, RnbRootFifth, HouseFourOnFloor };

std::vector<NoteEvent> generateBassRow (const AnalysisResult&, BassRhythm rhythm);
