#pragma once

#include <JuceHeader.h>

#include "MidiSetRow.h"
#include "../Analysis/AnalysisResult.h"

// Shared MIDI-file core for Phase 6 (EXP-01 drag-out, EXP-02 save dialog).
// Both export paths call this single code path -- divergence here is exactly
// how EXP-03's "carries the detected tempo" guarantee could silently break
// in one path and not the other (see 06-RESEARCH.md Pattern 1).
//
// Beat-domain multiply only -- ticks = beats * kTicksPerQuarterNote. Never
// re-derive tick positions via seconds/BPM anywhere (06-RESEARCH.md
// Anti-Patterns); this is what makes EXP-03's bar alignment structural, not
// incidental.
namespace MidiFileWriter
{
    constexpr int kTicksPerQuarterNote = 960;

    // Pure: same row + bpm always produces byte-identical MIDI file bytes.
    // bpm <= 0.0 is defensively treated as 120.0 -- a tempo meta-event cannot
    // represent a zero/negative tempo, guard here rather than upstream.
    juce::MidiFile buildMidiFile (const MidiSetRow& row, double bpm);

    // Writes atomically via a TemporaryFile + overwriteTargetFileWithTemporary
    // (never a direct FileOutputStream on destination -- append-by-default
    // corruption). Returns true on success.
    bool writeToFile (const MidiSetRow& row, double bpm, const juce::File& destination);

    // MIDI-pack-style, filesystem-safe suggested file name:
    // rowId_Key_NNNbpm.mid (e.g. "pop-trap_G#m_128bpm.mid").
    juce::String suggestedFileName (const MidiSetRow& row, const KeyResult& key, double bpm);
}
