#pragma once

#include <JuceHeader.h>

#include "NoteEvent.h"

#include <vector>

// Position in every genre's fixed 5-slot layout. Order is stable: index ==
// (int) kind is the row's patternIndex -- the regenerateRow(index) target
// and MidiRowView's colour key. Defined HERE (not in GenreRegistry.h as
// 06.1-RESEARCH.md's Pattern 1 sketch) so MidiSetRow does not depend on
// GenreRegistry -- GenreRegistry.h includes MidiSetRow.h instead, keeping
// includes acyclic (06.1-01-PLAN.md deviation note).
enum class PatternKind { SustainedChords, RhythmicChords, StabArp, BassLine, TopLineMotif };

struct MidiSetRow
{
    juce::String id;       // "<genreId>-<slotSlug>", e.g. "trap-sustained" (MidiFileWriter uses id for filenames -- unchanged rule)
    juce::String label;    // display, e.g. "Trap — Sustained Chords"
    PatternKind kind = PatternKind::SustainedChords;
    int patternIndex = 0;  // 0-4 stable slot index
    std::vector<NoteEvent> notes;
};
