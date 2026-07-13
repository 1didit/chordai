#pragma once

#include <JuceHeader.h>

#include "../Analysis/AnalysisResult.h"

// Pure chord-name formatting, shared by the UI (ChordTimelineView) and tests.
// Naming convention lifted verbatim from Tests/ClassicDspChordAnalyzerTests.cpp's
// RealTrackHarness chordName lambda (already shipping, already exercised on a
// real 75s track). Frozen-contract convention: "" / "m" / "7" / "N.C." — the
// ChordQuality enum has exactly 4 values and ChordSymbol has no inversion
// field, so this never produces Cmaj7/F/A-style names.
inline juce::String chordName (const ChordSymbol& chord)
{
    static constexpr const char* kNoteNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    if (chord.quality == ChordQuality::NoChord)
        return "N.C.";

    const juce::String suffix = chord.quality == ChordQuality::Major     ? ""
                               : chord.quality == ChordQuality::Minor    ? "m"
                                                                          : "7"; // Dominant7
    return juce::String (kNoteNames[chord.pitchClass]) + suffix;
}
