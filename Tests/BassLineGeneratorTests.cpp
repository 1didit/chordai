#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/BassLineGenerator.h"
#include "Source/MidiGen/ChordToneMapper.h"

#include "Tests/MidiGenFixtures.h"

#include <algorithm>

// RootPitchClassMatchesDetectedChord: for ALL THREE rhythms on
// makeFourChordFixture(), the note whose startBeats == segment.startBeatIndex
// has pitch % 12 == chord.pitchClass, for every non-NoChord segment.
// NOTE (Task 1): RnbRootFifth/HouseFourOnFloor branches are still empty
// stubs, so this SECTION is scoped to TrapSustain only for now. Task 2
// extends the remaining two SECTIONs once those branches are implemented.
TEST_CASE ("BassLineGeneratorTests.RootPitchClassMatchesDetectedChord", "[basslinegenerator]")
{
    auto fixture = midigen_fixtures::makeFourChordFixture();

    auto checkRootFollowing = [&] (BassRhythm rhythm, bool everyNoteIsRootClass)
    {
        auto notes = generateBassRow (fixture, rhythm);

        for (const auto& segment : fixture.chords)
        {
            if (segment.chord.quality == ChordQuality::NoChord)
                continue;

            auto it = std::find_if (notes.begin(), notes.end(), [&] (const NoteEvent& n)
            {
                return n.startBeats == (double) segment.startBeatIndex;
            });
            REQUIRE (it != notes.end());
            CHECK (((it->pitch % 12) + 12) % 12 == segment.chord.pitchClass);
        }

        if (everyNoteIsRootClass)
        {
            for (const auto& note : notes)
            {
                bool matchesSomeSegmentRoot = std::any_of (fixture.chords.begin(), fixture.chords.end(),
                    [&] (const ChordSegment& segment)
                {
                    if (segment.chord.quality == ChordQuality::NoChord)
                        return false;
                    return ((note.pitch % 12) + 12) % 12 == segment.chord.pitchClass;
                });
                CHECK (matchesSomeSegmentRoot);
            }
        }
    };

    SECTION ("TrapSustain")
    {
        checkRootFollowing (BassRhythm::TrapSustain, true);
    }
}

TEST_CASE ("BassLineGeneratorTests.TrapBassSustainsFullSegment", "[basslinegenerator]")
{
    auto fixture = midigen_fixtures::makeFourChordFixture();
    auto notes = generateBassRow (fixture, BassRhythm::TrapSustain);

    // Exactly one note per non-NoChord segment (all 4 segments are pitched here).
    REQUIRE (notes.size() == fixture.chords.size());

    for (size_t i = 0; i < fixture.chords.size(); ++i)
    {
        const auto& segment = fixture.chords[i];
        const auto& note = notes[i];

        CHECK (note.startBeats == (double) segment.startBeatIndex);
        CHECK (note.lengthBeats == (double) (segment.endBeatIndex - segment.startBeatIndex));
        CHECK (note.pitch >= 36);
        CHECK (note.pitch <= 47);
    }

    // Am -> root C2 anchor(36) + pitchClass 9 == 45.
    CHECK (notes[0].pitch == rootMidiNote (9, kAnchorBass));
    CHECK (notes[0].pitch == 45);
}

TEST_CASE ("BassLineGeneratorTests.NoChordEmitsNoBassNotes", "[basslinegenerator]")
{
    auto fixture = midigen_fixtures::makeNoChordFixture();

    for (BassRhythm rhythm : { BassRhythm::TrapSustain, BassRhythm::RnbRootFifth, BassRhythm::HouseFourOnFloor })
    {
        auto notes = generateBassRow (fixture, rhythm);
        for (const auto& note : notes)
            CHECK_FALSE ((note.startBeats >= 4.0 && note.startBeats < 8.0));
    }
}

TEST_CASE ("BassLineGeneratorTests.BassVelocityDeterministicAndBounded", "[basslinegenerator]")
{
    auto fixture = midigen_fixtures::makeFourChordFixture();

    auto notesA = generateBassRow (fixture, BassRhythm::TrapSustain);
    auto notesB = generateBassRow (fixture, BassRhythm::TrapSustain);

    REQUIRE (notesA.size() == notesB.size());
    REQUIRE_FALSE (notesA.empty());

    for (size_t i = 0; i < notesA.size(); ++i)
    {
        CHECK (notesA[i].startBeats == notesB[i].startBeats);
        CHECK (notesA[i].lengthBeats == notesB[i].lengthBeats);
        CHECK (notesA[i].pitch == notesB[i].pitch);
        CHECK (notesA[i].velocity == notesB[i].velocity);

        CHECK (notesA[i].velocity >= 0.82f);
        CHECK (notesA[i].velocity <= 0.88f);
    }
}
