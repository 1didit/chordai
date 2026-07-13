#pragma once

// Header-only: chord-tone interval tables and pitch-class -> MIDI note
// helpers, shared by every voicing generator. The ONLY Analysis include
// allowed anywhere under Source/MidiGen/ is AnalysisResult.h (for
// ChordQuality) -- see 05-RESEARCH.md Anti-Patterns.
//
// Bodies implemented in plan 05-01 Task 2.

#include "../Analysis/AnalysisResult.h"

#include <vector>

// implemented in plan 05-01 Task 2
