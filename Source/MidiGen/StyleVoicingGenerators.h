#pragma once

// Frozen generator signatures (Wave 2 implements bodies only, in plan
// 05-02): the three style-variant voicing rows (GEN-02). Consolidated into
// one header/cpp pair per 05-RESEARCH.md's own file-count note -- functional
// separation (one function per style, independently testable) matters more
// than file count.

#include "../Analysis/AnalysisResult.h"
#include "GenerationSettings.h"
#include "NoteEvent.h"

#include <vector>

// Pop/Hip-hop/Trap: plain triads only (7th dropped even from a Dominant7
// source chord), dark/close register, half-bar re-strike rhythm.
std::vector<NoteEvent> generatePopTrapRow (const AnalysisResult&, const GenerationSettings&);

// R&B/Neo-soul: 7th/9th/11th extension chords with voice leading between
// consecutive chords.
std::vector<NoteEvent> generateRnbNeoSoulRow (const AnalysisResult&, const GenerationSettings&);

// Electronic/House: plain triads, bright/tight register, off-beat stab
// rhythm.
std::vector<NoteEvent> generateElectronicHouseRow (const AnalysisResult&, const GenerationSettings&);
