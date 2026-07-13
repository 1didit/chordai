#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/BassLineGenerator.h"
#include "Source/MidiGen/ChordToneMapper.h"

#include "Tests/MidiGenFixtures.h"

#include <algorithm>

// RootPitchClassMatchesDetectedChord: for ALL THREE rhythms on
// makeFourChordFixture(), the note whose startBeats == segment.startBeatIndex
// has pitch % 12 == chord.pitchClass, for every non-NoChord segment.
// TrapSustain and HouseFourOnFloor additionally require EVERY note to be
// root-class; RnbRootFifth does not (its second note per span is the fifth,
// a different pitch class), so only the first-note-at-segment-start check
// applies there.
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

    SECTION ("RnbRootFifth")
    {
        checkRootFollowing (BassRhythm::RnbRootFifth, false);
    }

    SECTION ("HouseFourOnFloor")
    {
        checkRootFollowing (BassRhythm::HouseFourOnFloor, true);
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

TEST_CASE ("BassLineGeneratorTests.RnbBassRootFifthWalk", "[basslinegenerator]")
{
    SECTION ("4-beat segment walks root then fifth")
    {
        auto fixture = midigen_fixtures::makeFourChordFixture();
        auto notes = generateBassRow (fixture, BassRhythm::RnbRootFifth);

        // Am segment (beats 0-4) is first in fixture order.
        REQUIRE (notes.size() >= 2);
        CHECK (notes[0].startBeats == 0.0);
        CHECK (notes[0].lengthBeats == 2.0);
        CHECK (notes[0].pitch == 45); // root

        CHECK (notes[1].startBeats == 2.0);
        CHECK (notes[1].lengthBeats == 2.0);
        CHECK (notes[1].pitch == 52); // root + 7 (fifth)
    }

    SECTION ("segments shorter than 4 beats sustain root only")
    {
        auto fixture = midigen_fixtures::makeShortSegmentFixture();
        auto notes = generateBassRow (fixture, BassRhythm::RnbRootFifth);

        // C, 2 beats (startBeatIndex 0-2): single root note.
        auto cNote = std::find_if (notes.begin(), notes.end(), [] (const NoteEvent& n)
        {
            return n.startBeats == 0.0;
        });
        REQUIRE (cNote != notes.end());
        CHECK (cNote->lengthBeats == 2.0);
        CHECK (cNote->pitch == rootMidiNote (0, kAnchorBass));

        // F, 3 beats (startBeatIndex 2-5): single root note.
        auto fNote = std::find_if (notes.begin(), notes.end(), [] (const NoteEvent& n)
        {
            return n.startBeats == 2.0;
        });
        REQUIRE (fNote != notes.end());
        CHECK (fNote->lengthBeats == 3.0);
        CHECK (fNote->pitch == rootMidiNote (5, kAnchorBass));
    }

    SECTION ("8-beat segment repeats root/fifth pattern per 4-beat span")
    {
        constexpr double bpm = 120.0;
        AnalysisResult fixture;
        fixture.sampleRate = 44100.0;
        fixture.bpm = bpm;
        fixture.beatTimesSeconds = midigen_fixtures::detail::makeBeatGrid (8, bpm);
        fixture.barStartBeatIndices = midigen_fixtures::detail::makeBarStarts (8);
        fixture.analyzedRegionSeconds = juce::Range<double> (0.0, fixture.beatTimesSeconds.back());
        fixture.chords = { midigen_fixtures::detail::makeSegment (9, ChordQuality::Minor, 0, 8, bpm) }; // Am, 8 beats

        auto notes = generateBassRow (fixture, BassRhythm::RnbRootFifth);
        REQUIRE (notes.size() == 4);

        CHECK (notes[0].startBeats == 0.0); CHECK (notes[0].lengthBeats == 2.0); CHECK (notes[0].pitch == 45);
        CHECK (notes[1].startBeats == 2.0); CHECK (notes[1].lengthBeats == 2.0); CHECK (notes[1].pitch == 52);
        CHECK (notes[2].startBeats == 4.0); CHECK (notes[2].lengthBeats == 2.0); CHECK (notes[2].pitch == 45);
        CHECK (notes[3].startBeats == 6.0); CHECK (notes[3].lengthBeats == 2.0); CHECK (notes[3].pitch == 52);
    }
}

TEST_CASE ("BassLineGeneratorTests.HouseBassFourOnTheFloor", "[basslinegenerator]")
{
    SECTION ("4-beat segment: one note per beat, root class")
    {
        auto fixture = midigen_fixtures::makeFourChordFixture();
        auto notes = generateBassRow (fixture, BassRhythm::HouseFourOnFloor);

        std::vector<NoteEvent> amNotes;
        for (const auto& n : notes)
            if (n.startBeats < 4.0)
                amNotes.push_back (n);

        REQUIRE (amNotes.size() == 4);
        for (size_t i = 0; i < 4; ++i)
        {
            CHECK (amNotes[i].startBeats == (double) i);
            CHECK (amNotes[i].lengthBeats == 0.5);
            CHECK (amNotes[i].pitch == 45);
        }
    }

    SECTION ("3-beat segment: 3 notes")
    {
        auto fixture = midigen_fixtures::makeShortSegmentFixture();
        auto notes = generateBassRow (fixture, BassRhythm::HouseFourOnFloor);

        // F segment: startBeatIndex 2, endBeatIndex 5 (3 beats).
        std::vector<NoteEvent> fNotes;
        for (const auto& n : notes)
            if (n.startBeats >= 2.0 && n.startBeats < 5.0)
                fNotes.push_back (n);

        REQUIRE (fNotes.size() == 3);
        CHECK (fNotes[0].startBeats == 2.0);
        CHECK (fNotes[1].startBeats == 3.0);
        CHECK (fNotes[2].startBeats == 4.0);
        for (const auto& n : fNotes)
        {
            CHECK (n.lengthBeats == 0.5);
            CHECK (n.pitch == rootMidiNote (5, kAnchorBass));
        }
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
