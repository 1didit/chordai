#pragma once

// Pure value type -- no JUCE dependency at all, and Source/MidiGen/ never
// includes anything under Source/Analysis/ beyond AnalysisResult.h (folder-
// boundary rule, see 05-RESEARCH.md Anti-Patterns).
//
// Timing is in BEATS, not seconds: 1.0 == one detected beat == one quarter
// note at AnalysisResult.bpm. This makes every generated note bar-aligned by
// construction (it inherits ChordSegment's own startBeatIndex/endBeatIndex
// alignment) -- no re-deriving bar position from seconds/BPM anywhere in
// MidiGen (see 05-RESEARCH.md Anti-Patterns, Pitfall 4's own root cause).
//
// The eventual Phase 6 juce::MidiFile export is a one-line multiply:
// ticks = startBeats * ticksPerQuarterNote. Phase 5 does not build that
// conversion -- see 05-RESEARCH.md "Code Examples" for the exact shape.
struct NoteEvent
{
    double startBeats = 0.0;
    double lengthBeats = 0.0;
    int pitch = 60;          // MIDI note number, 0-127 (60 = middle C, matches
                              // Tests/SyntheticFixtures.h's existing "60 + pitchClass"
                              // chord-tone-octave convention -- reuse it, don't invent a new one)
    float velocity = 0.8f;   // 0.0-1.0, matches juce::MidiMessage::noteOn's float overload
};
