// PatternEngineTests.cpp -- RED suite for generatePattern (06.1-03 Task 1).
//
// Independent of 06.1-04's genre data (hand-authored PatternArchetype
// fixtures only via the file-local makeArchetype() helper) -- this is what
// makes Wave 2 (this plan) and Wave 2's 06.1-04 (GenreRegistry data) safe to
// run in parallel.

#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/BassLineGenerator.h"
#include "Source/MidiGen/ChordToneMapper.h"
#include "Source/MidiGen/GenreRegistry.h"
#include "Source/MidiGen/PatternEngine.h"
#include "Source/MidiGen/PatternSeed.h"
#include "Tests/MidiGenFixtures.h"

#include <algorithm>
#include <chrono>
#include <set>
#include <vector>

namespace
{
    // File-local archetype builder (per plan instruction): sane, wide-open
    // defaults every test then narrows for its own scenario -- keeps each
    // TEST_CASE focused on the ONE dimension it's proving.
    PatternArchetype makeArchetype (PatternKind kind, ToneSetKind toneSet)
    {
        PatternArchetype a;
        a.kind = kind;
        a.toneSet = toneSet;
        a.registerAnchor = 60;
        a.registerLow = 0;
        a.registerHigh = 127;
        a.dropRoot = false;
        a.useVoiceLeading = false;
        a.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 1.0 } };
        a.octaveOffsetPool = { 0 };
        a.bassRhythmPool = {};
        a.baseVelocity = 0.75f;
        a.accentPattern = { 1.0f };
        a.jitterRange = 0.08f;
        a.ornamentProbability = 0.0f;
        return a;
    }

    std::vector<NoteEvent> notesInRange (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        std::vector<NoteEvent> out;
        for (const auto& n : notes)
            if (n.startBeats >= lo && n.startBeats < hi)
                out.push_back (n);
        return out;
    }

    bool notesEqual (const std::vector<NoteEvent>& a, const std::vector<NoteEvent>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].startBeats != b[i].startBeats || a[i].lengthBeats != b[i].lengthBeats
                || a[i].pitch != b[i].pitch || a[i].velocity != b[i].velocity)
                return false;
        return true;
    }

    std::set<int> chordTonePitchClasses (ToneSetKind toneSet, const ChordSymbol& chord, bool dropRoot = false)
    {
        std::set<int> pcs;
        for (int iv : toneSetIntervals (toneSet, chord.quality, dropRoot))
            pcs.insert (((chord.pitchClass + iv) % 12 + 12) % 12);
        return pcs;
    }

    std::set<int> diatonicPitchClasses (const KeyResult& key)
    {
        std::set<int> pcs;
        for (int iv : diatonicScaleIntervals (key.isMajor))
            pcs.insert (((key.tonicPitchClass + iv) % 12 + 12) % 12);
        return pcs;
    }

    std::set<double> onsetsInRange (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        std::set<double> onsets;
        for (const auto& n : notes)
            if (n.startBeats >= lo && n.startBeats < hi)
                onsets.insert (n.startBeats);
        return onsets;
    }
}

// ---------------------------------------------------------------------------
// 1. Sustained slot: one block chord per segment
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.SustainedSlotEmitsOneChordPerSegment", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto archetype = makeArchetype (PatternKind::SustainedChords, ToneSetKind::Triad);

    const auto notes = generatePattern (fixture, archetype, 42u);

    SECTION ("segment 1 (Am): one onset at beat 0, full-segment length, root-position triad")
    {
        const auto seg1 = notesInRange (notes, 0.0, 4.0);
        REQUIRE (seg1.size() == 3);

        std::vector<int> pitches;
        for (const auto& n : seg1)
        {
            pitches.push_back (n.pitch);
            CHECK (n.startBeats == 0.0);
            CHECK (n.lengthBeats == 4.0);
        }
        std::sort (pitches.begin(), pitches.end());
        CHECK (pitches == std::vector<int> { 69, 72, 76 });
    }

    SECTION ("segment 4 (G7): one onset at beat 12, full-segment length, keeps the 7th")
    {
        const auto seg4 = notesInRange (notes, 12.0, 16.0);
        REQUIRE (seg4.size() == 4);

        std::vector<int> pitches;
        for (const auto& n : seg4)
        {
            pitches.push_back (n.pitch);
            CHECK (n.startBeats == 12.0);
            CHECK (n.lengthBeats == 4.0);
        }
        std::sort (pitches.begin(), pitches.end());
        CHECK (pitches == std::vector<int> { 67, 71, 74, 77 });
    }

    SECTION ("every segment produces exactly one onset")
    {
        const auto onsets = onsetsInRange (notes, 0.0, 16.0);
        CHECK (onsets == std::set<double> { 0.0, 4.0, 8.0, 12.0 });
    }
}

// ---------------------------------------------------------------------------
// 2. Every note is chord-or-scale-tone, across every ToneSetKind
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.EveryNoteIsChordOrScaleTone", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    const ToneSetKind toneSets[] = { ToneSetKind::Triad, ToneSetKind::SeventhExtension,
                                      ToneSetKind::PowerChord, ToneSetKind::RootOnly,
                                      ToneSetKind::SingleTopTone };

    for (auto toneSet : toneSets)
    {
        const auto kind = (toneSet == ToneSetKind::SingleTopTone) ? PatternKind::TopLineMotif
                                                                    : PatternKind::SustainedChords;
        const auto archetype = makeArchetype (kind, toneSet);
        const auto notes = generatePattern (fixture, archetype, 7u);
        REQUIRE_FALSE (notes.empty()); // guards against a vacuous pass on an empty-skeleton return

        for (const auto& segment : fixture.chords)
        {
            const auto expected = chordTonePitchClasses (toneSet, segment.chord, archetype.dropRoot);
            const auto segNotes = notesInRange (notes, (double) segment.startBeatIndex, (double) segment.endBeatIndex);
            REQUIRE_FALSE (segNotes.empty());

            for (const auto& n : segNotes)
            {
                const int pc = ((n.pitch % 12) + 12) % 12;
                CHECK (expected.count (pc) == 1);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 3. Harmony preservation across 20+ seed variations (Pitfall A)
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.HarmonyPreservationAcrossVariations", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    auto archetype = makeArchetype (PatternKind::TopLineMotif, ToneSetKind::SingleTopTone);
    archetype.rhythmPool = {
        RhythmVariant { { 0.0 }, 4.0, 0.9 },
        RhythmVariant { { 0.0, 2.0 }, 4.0, 0.9 },
        RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 },
    };
    archetype.octaveOffsetPool = { 0, 12 };
    archetype.ornamentProbability = 0.3f;

    const auto diatonic = diatonicPitchClasses (fixture.key);
    const auto baseSeed = computeBaseSeed (fixture, "harmony-test", (int) PatternKind::TopLineMotif);

    for (uint32_t counter = 0; counter <= 20; ++counter)
    {
        const auto seed = combineSeedWithVariation (baseSeed, counter);
        const auto notes = generatePattern (fixture, archetype, seed);
        REQUIRE_FALSE (notes.empty()); // guards against a vacuous pass on an empty-skeleton return

        for (const auto& segment : fixture.chords)
        {
            const auto chordTones = chordTonePitchClasses (ToneSetKind::SingleTopTone, segment.chord, false);
            const auto segNotes = notesInRange (notes, (double) segment.startBeatIndex, (double) segment.endBeatIndex);

            for (const auto& n : segNotes)
            {
                const int pc = ((n.pitch % 12) + 12) % 12;
                const bool isAllowed = chordTones.count (pc) == 1 || diatonic.count (pc) == 1;
                CHECK (isAllowed);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 4. Determinism: byte-identical output for identical inputs
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.DeterminismByteIdentical", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    auto archetype = makeArchetype (PatternKind::RhythmicChords, ToneSetKind::SeventhExtension);
    archetype.rhythmPool = {
        RhythmVariant { { 0.0 }, 4.0, 0.9 },
        RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 },
    };
    archetype.octaveOffsetPool = { 0, 12 };

    const auto notesA = generatePattern (fixture, archetype, 777u);
    const auto notesB = generatePattern (fixture, archetype, 777u);

    REQUIRE (notesA.size() == notesB.size());
    REQUIRE_FALSE (notesA.empty());
    CHECK (notesEqual (notesA, notesB));
}

// ---------------------------------------------------------------------------
// 5. Seed selects among the rhythm pool
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.SeedSelectsAmongRhythmPool", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    auto archetype = makeArchetype (PatternKind::SustainedChords, ToneSetKind::Triad);
    archetype.rhythmPool = {
        RhythmVariant { { 0.0 }, 4.0, 0.9 },                     // 1 onset
        RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 },      // 4 onsets
    };

    const auto notesEven = generatePattern (fixture, archetype, 0u);  // 0 % 2 == 0 -> variant 0
    const auto notesOdd  = generatePattern (fixture, archetype, 1u);  // 1 % 2 == 1 -> variant 1

    const auto onsetsEven = onsetsInRange (notesEven, 0.0, 4.0);
    const auto onsetsOdd  = onsetsInRange (notesOdd, 0.0, 4.0);

    CHECK (onsetsEven.size() == 1);
    CHECK (onsetsOdd.size() == 4);
    CHECK (onsetsEven != onsetsOdd);

    // Same seed -> same variant (repeat call is stable).
    const auto notesEvenAgain = generatePattern (fixture, archetype, 0u);
    CHECK (onsetsInRange (notesEvenAgain, 0.0, 4.0) == onsetsEven);
}

// ---------------------------------------------------------------------------
// 6. NoChord segments emit zero notes, every slot (incl. bass delegation)
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.NoChordSegmentsEmitNoNotes", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeNoChordFixture(); // segment 2 (beats 4-8) is NoChord

    const ToneSetKind toneSets[] = { ToneSetKind::Triad, ToneSetKind::SeventhExtension,
                                      ToneSetKind::PowerChord, ToneSetKind::RootOnly,
                                      ToneSetKind::SingleTopTone };

    for (auto toneSet : toneSets)
    {
        const auto kind = (toneSet == ToneSetKind::SingleTopTone) ? PatternKind::TopLineMotif
                                                                    : PatternKind::SustainedChords;
        const auto archetype = makeArchetype (kind, toneSet);
        const auto notes = generatePattern (fixture, archetype, 3u);
        // Guards against a vacuous pass: prove the OTHER (chorded) segments
        // actually produced notes before trusting the NoChord range is empty.
        REQUIRE_FALSE (notesInRange (notes, 0.0, 4.0).empty());
        REQUIRE_FALSE (notesInRange (notes, 8.0, 12.0).empty());
        CHECK (notesInRange (notes, 4.0, 8.0).empty());
    }

    auto bassArchetype = makeArchetype (PatternKind::BassLine, ToneSetKind::RootOnly);
    bassArchetype.bassRhythmPool = { BassRhythm::TrapSustain };
    const auto bassNotes = generatePattern (fixture, bassArchetype, 3u);
    REQUIRE_FALSE (notesInRange (bassNotes, 0.0, 4.0).empty());
    REQUIRE_FALSE (notesInRange (bassNotes, 8.0, 12.0).empty());
    CHECK (notesInRange (bassNotes, 4.0, 8.0).empty());
}

// ---------------------------------------------------------------------------
// 7. Register clamp respected (voice-leading path)
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.RegisterClampRespected", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    auto archetype = makeArchetype (PatternKind::RhythmicChords, ToneSetKind::SeventhExtension);
    archetype.useVoiceLeading = true;
    archetype.registerLow = 60;
    archetype.registerHigh = 72;

    const auto notes = generatePattern (fixture, archetype, 11u);
    REQUIRE_FALSE (notes.empty());

    for (const auto& n : notes)
    {
        CHECK (n.pitch >= 60);
        CHECK (n.pitch <= 72);
    }
}

// ---------------------------------------------------------------------------
// 8. dropRoot omits the root pitch class
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.DropRootOmitsRootPitchClass", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    auto archetype = makeArchetype (PatternKind::SustainedChords, ToneSetKind::Triad);
    archetype.dropRoot = true;

    const auto notes = generatePattern (fixture, archetype, 5u);
    REQUIRE_FALSE (notes.empty());

    for (const auto& segment : fixture.chords)
    {
        const auto segNotes = notesInRange (notes, (double) segment.startBeatIndex, (double) segment.endBeatIndex);
        for (const auto& n : segNotes)
        {
            const int pc = ((n.pitch % 12) + 12) % 12;
            CHECK (pc != segment.chord.pitchClass);
        }
    }
}

// ---------------------------------------------------------------------------
// 9. BassLine slot delegates to generateBassRow
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.BassLineDelegatesToBassRhythmPool", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    auto archetype = makeArchetype (PatternKind::BassLine, ToneSetKind::RootOnly);
    archetype.bassRhythmPool = { BassRhythm::TrapSustain };
    archetype.octaveOffsetPool = { 0 };

    const auto notes = generatePattern (fixture, archetype, 21u);
    const auto directNotes = generateBassRow (fixture, BassRhythm::TrapSustain);

    REQUIRE (notes.size() == directNotes.size());
    for (size_t i = 0; i < notes.size(); ++i)
    {
        CHECK (notes[i].startBeats == directNotes[i].startBeats);
        CHECK (notes[i].lengthBeats == directNotes[i].lengthBeats);
        CHECK (notes[i].pitch == directNotes[i].pitch); // octaveOffset == 0 -> exact match, not just pitch class
    }
}

// ---------------------------------------------------------------------------
// 10. TopLineMotif ornaments are diatonic-only
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.TopLineOrnamentsAreDiatonicOnly", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto diatonic = diatonicPitchClasses (fixture.key);

    auto archetype = makeArchetype (PatternKind::TopLineMotif, ToneSetKind::SingleTopTone);
    archetype.rhythmPool = { RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 } };

    SECTION ("ornamentProbability 1.0 -> every note is chord-tone or diatonic")
    {
        archetype.ornamentProbability = 1.0f;
        const auto notes = generatePattern (fixture, archetype, 9u);

        // 4 onsets/segment * 4 segments, chord tone + ornament each onset.
        CHECK (notes.size() == 32);

        for (const auto& segment : fixture.chords)
        {
            const auto chordTones = chordTonePitchClasses (ToneSetKind::SingleTopTone, segment.chord, false);
            const auto segNotes = notesInRange (notes, (double) segment.startBeatIndex, (double) segment.endBeatIndex);

            for (const auto& n : segNotes)
            {
                const int pc = ((n.pitch % 12) + 12) % 12;
                CHECK ((chordTones.count (pc) == 1 || diatonic.count (pc) == 1));
            }
        }
    }

    SECTION ("ornamentProbability 0.0 -> pure chord tones only")
    {
        archetype.ornamentProbability = 0.0f;
        const auto notes = generatePattern (fixture, archetype, 9u);

        CHECK (notes.size() == 16); // 4 onsets/segment * 4 segments, chord tone only

        for (const auto& segment : fixture.chords)
        {
            const auto chordTones = chordTonePitchClasses (ToneSetKind::SingleTopTone, segment.chord, false);
            const auto segNotes = notesInRange (notes, (double) segment.startBeatIndex, (double) segment.endBeatIndex);

            for (const auto& n : segNotes)
            {
                const int pc = ((n.pitch % 12) + 12) % 12;
                CHECK (chordTones.count (pc) == 1);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 11. StabArp distributes one tone per onset (Arpeggiator::Up)
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.ArpSlotDistributesOneTonePerOnset", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    auto archetype = makeArchetype (PatternKind::StabArp, ToneSetKind::Triad);
    archetype.rhythmPool = { RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 } };

    const auto notes = generatePattern (fixture, archetype, 13u);

    // Segment 1 (Am): triad pitches {69, 72, 76} in root/third/fifth order;
    // arpeggiate(Up, 4) cycles 69,72,76,69.
    const std::vector<double> onsetBeats { 0.0, 1.0, 2.0, 3.0 };
    const std::vector<int> expectedPitches { 69, 72, 76, 69 };

    for (size_t i = 0; i < onsetBeats.size(); ++i)
    {
        const auto atOnset = notesInRange (notes, onsetBeats[i], onsetBeats[i] + 0.5);
        REQUIRE (atOnset.size() == 1); // one tone per onset, not the full triad
        CHECK (atOnset[0].pitch == expectedPitches[i]);
    }
}

// ---------------------------------------------------------------------------
// 12. Performance budget: 5 slot calls on a real-track-scale fixture < 1ms
// ---------------------------------------------------------------------------

TEST_CASE ("PatternEngineTests.GenerationPerformanceBudget", "[patternengine]")
{
    const auto fixture = midigen_fixtures::makeRealTrackScaleFixture();

    auto sustained = makeArchetype (PatternKind::SustainedChords, ToneSetKind::Triad);

    auto rhythmic = makeArchetype (PatternKind::RhythmicChords, ToneSetKind::SeventhExtension);
    rhythmic.rhythmPool = { RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 } };

    auto stabArp = makeArchetype (PatternKind::StabArp, ToneSetKind::Triad);
    stabArp.rhythmPool = { RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 } };

    auto bass = makeArchetype (PatternKind::BassLine, ToneSetKind::RootOnly);
    bass.bassRhythmPool = { BassRhythm::RnbRootFifth };

    auto topLine = makeArchetype (PatternKind::TopLineMotif, ToneSetKind::SingleTopTone);
    topLine.rhythmPool = { RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.9 } };
    topLine.ornamentProbability = 0.5f;

    double minElapsedMs = 1.0e9;
    size_t totalNotes = 0;

    for (int run = 0; run < 5; ++run)
    {
        const auto start = std::chrono::steady_clock::now();

        totalNotes += generatePattern (fixture, sustained, 1u).size();
        totalNotes += generatePattern (fixture, rhythmic, 2u).size();
        totalNotes += generatePattern (fixture, stabArp, 3u).size();
        totalNotes += generatePattern (fixture, bass, 4u).size();
        totalNotes += generatePattern (fixture, topLine, 5u).size();

        const auto end = std::chrono::steady_clock::now();
        const double elapsedMs = std::chrono::duration<double, std::milli> (end - start).count();
        minElapsedMs = std::min (minElapsedMs, elapsedMs);
    }

    CHECK (totalNotes > 0);
    CHECK (minElapsedMs < 1.0);
}
