#pragma once

// Header-only: chord-tone interval tables and pitch-class -> MIDI note
// helpers, shared by every voicing generator. The ONLY Analysis include
// allowed anywhere under Source/MidiGen/ is AnalysisResult.h (for
// ChordQuality) -- see 05-RESEARCH.md Anti-Patterns.

#include "../Analysis/AnalysisResult.h"

#include <JuceHeader.h>

#include <vector>

// Register anchors (MIDI note) used to seed each row's root position -- the
// contracts Wave 2 generators code against (05-RESEARCH.md "Register/root
// helper" table).
inline constexpr int kAnchorAsIs = 60, kAnchorPopTrap = 48, kAnchorRnbSeed = 60,
                     kAnchorHouse = 72, kAnchorBass = 36;
inline constexpr int kRnbRegisterLow = 48, kRnbRegisterHigh = 72;

// Beat offsets relative to the start of each 4-beat span within a chord
// segment -- the "and" of every beat (Electronic/House stab rhythm).
constexpr double kHouseStabOffsetsBeats[4] = { 0.5, 1.5, 2.5, 3.5 };
constexpr double kHouseStabLengthBeats = 0.25;

// Plain triad intervals, identical semitone offsets to ChordTemplates.h's
// buildTemplate() (root/minor-or-major-third/fifth/dominant-seventh) -- reuse
// the same interpretation of "quality" the detector itself used, don't invent
// a second one.
//
// NoChord is an explicit empty case, NOT a fallthrough to Major (Pitfall 1:
// a generator that forgets this would silently emit a bogus C-major chord
// during a detected-silence segment).
inline std::vector<int> triadIntervals (ChordQuality quality)
{
    switch (quality)
    {
        case ChordQuality::Minor:     return { 0, 3, 7 };
        case ChordQuality::Dominant7: return { 0, 4, 7, 10 };
        case ChordQuality::NoChord:   return {};
        case ChordQuality::Major:
        default:                     return { 0, 4, 7 };
    }
}

// R&B/Neo-soul extension sets. Concrete GEN-02 "7th/9th/11th" mapping:
//   Major      -> maj9  (root, 3, 5, maj7, 9)               -- 5 tones
//   Minor      -> min11 (root, b3, 5, b7, 9, 11)             -- 6 tones
//   Dominant7  -> dom9  (root, 3, 5, b7, 9)                  -- 5 tones
// Minor gets the 11th (not Major/Dominant7): an 11th against a major 3rd is a
// dissonant semitone clash (the "avoid note"), but is consonant against a
// minor 3rd -- standard jazz/neo-soul voicing practice, and keeps this table
// a fixed, testable, per-quality lookup rather than a runtime dissonance check.
//
// NoChord is an explicit empty case (Pitfall 1), same reasoning as
// triadIntervals above.
inline std::vector<int> rnbExtensionIntervals (ChordQuality quality)
{
    switch (quality)
    {
        case ChordQuality::Minor:     return { 0, 3, 7, 10, 14, 17 };
        case ChordQuality::Dominant7: return { 0, 4, 7, 10, 14 };
        case ChordQuality::NoChord:   return {};
        case ChordQuality::Major:
        default:                     return { 0, 4, 7, 11, 14 };
    }
}

// Anchors a set of semitone intervals at a concrete MIDI root note, clamped
// into the valid 0-127 range.
inline std::vector<int> intervalsToMidiNotes (int rootMidiNote, const std::vector<int>& intervals)
{
    std::vector<int> notes;
    notes.reserve (intervals.size());
    for (int iv : intervals)
        notes.push_back (juce::jlimit (0, 127, rootMidiNote + iv));
    return notes;
}

// pitchClass: 0-11, matches AnalysisResult.h's 0=C..11=B convention exactly.
// anchorOctaveBase: the style's chosen register anchor (see kAnchor* above).
inline int rootMidiNote (int pitchClass, int anchorOctaveBase)
{
    return juce::jlimit (0, 127, anchorOctaveBase + pitchClass);
}

// Root + fifth only, quality-independent (Phase 6.1 GenreRegistry's
// ToneSetKind::PowerChord -- trap/rock-style "5" voicing, no third).
inline std::vector<int> powerChordIntervals() { return { 0, 7 }; }
