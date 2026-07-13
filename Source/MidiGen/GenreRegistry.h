#pragma once

// Genre library data model (06.1-RESEARCH.md Pattern 1/3): every genre is
// exactly 5 PatternArchetypes, one per fixed PatternKind slot. Types +
// declarations only here -- allGenres()/findGenre() implementations land in
// plan 06.1-04.

#include "MidiSetRow.h"
#include "ChordToneMapper.h"
#include "BassLineGenerator.h"
#include "../Analysis/AnalysisResult.h"

#include <JuceHeader.h>

#include <array>
#include <vector>

enum class ToneSetKind { Triad, SeventhExtension, PowerChord, RootOnly, SingleTopTone };

struct RhythmVariant
{
    std::vector<double> onsetsBeats;  // beat offsets within one repeating span, SEGMENT-relative
                                       // (Phase 5 precedent). MUST be rational fractions whose
                                       // denominator divides 960 -- enforced by GenreRegistryTests sweep.
    double spanBeats = 4.0;
    double noteLengthRatio = 0.9;     // fraction of the inter-onset gap (staccato < 1.0)
};

struct PatternArchetype
{
    PatternKind kind = PatternKind::SustainedChords;
    ToneSetKind toneSet = ToneSetKind::Triad;
    int registerAnchor = 60;
    int registerLow = 48, registerHigh = 72;
    bool dropRoot = false;             // trap: leave the root to the 808
    bool useVoiceLeading = false;      // opt-in to VoiceLeadingEngine cross-chord continuity (R&B)
    std::vector<RhythmVariant> rhythmPool;     // >=1; seed % pool.size() selects (GEN-11 variation dim 1)
    std::vector<int> octaveOffsetPool { 0 };   // multiples of 12 (GEN-11 variation dim 2)
    std::vector<BassRhythm> bassRhythmPool;    // BassLine slot only: non-empty => delegate to generateBassRow
    float baseVelocity = 0.75f;
    std::vector<float> accentPattern { 1.0f }; // cyclic per-onset multiplier
    float jitterRange = 0.08f;                 // fed to Humanization::deterministicJitter
    float ornamentProbability = 0.0f;          // TopLineMotif only, seed-thresholded diatonic passing tone
};

struct GenreSpec
{
    juce::String id, label, shortLabel;        // shortLabel: uppercase chip text, e.g. "BOOM BAP"
    std::array<PatternArchetype, 5> patterns;  // index == (int) PatternKind
};

const std::vector<GenreSpec>& allGenres();                 // 10 genres (06.1-04 fills)
const GenreSpec* findGenre (const juce::String& id);       // nullptr if unknown -- callers MUST null-check (Pitfall F)
inline const juce::StringArray kDefaultMainGenreIds { "trap", "uk-drill", "boom-bap", "rnb-neosoul", "house" };

// Dispatches a chord's tone set per ToneSetKind, applied to the SAME
// segment.chord.quality every call -- seed/regenerate never reaches this
// function's inputs (harmony preservation by construction, Pitfall A).
// CRITICAL: every ToneSetKind returns EMPTY for NoChord (Phase 5 Pitfall 1 --
// a silence segment must never emit a bogus chord).
inline std::vector<int> toneSetIntervals (ToneSetKind kind, ChordQuality quality, bool dropRoot)
{
    std::vector<int> intervals;
    switch (kind)
    {
        case ToneSetKind::Triad:            intervals = triadIntervals (quality); break;
        case ToneSetKind::SeventhExtension: intervals = rnbExtensionIntervals (quality); break;
        case ToneSetKind::PowerChord:       intervals = (quality == ChordQuality::NoChord) ? std::vector<int>{} : powerChordIntervals(); break;
        case ToneSetKind::RootOnly:         intervals = (quality == ChordQuality::NoChord) ? std::vector<int>{} : std::vector<int>{ 0 }; break;
        case ToneSetKind::SingleTopTone:  { auto t = triadIntervals (quality); intervals = t.empty() ? std::vector<int>{} : std::vector<int>{ t.back() }; break; }
    }
    if (dropRoot && intervals.size() > 1)
        intervals.erase (intervals.begin());
    return intervals;
}
