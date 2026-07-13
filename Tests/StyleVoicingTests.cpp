#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/AsIsRowGenerator.h"
#include "Source/MidiGen/ChordToneMapper.h"
#include "Source/MidiGen/Humanization.h"
#include "Source/MidiGen/StyleVoicingGenerators.h"
#include "Tests/MidiGenFixtures.h"

#include <algorithm>
#include <set>

namespace
{
    bool hasNoteStartingIn (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        return std::any_of (notes.begin(), notes.end(), [lo, hi] (const NoteEvent& n)
        {
            return n.startBeats >= lo && n.startBeats < hi;
        });
    }
}

// ---------------------------------------------------------------------------
// Task 1: As-is row + Pop/Trap voicing
// ---------------------------------------------------------------------------

TEST_CASE ("StyleVoicingTests.AsIsRowIsLiteralDetectedProgression", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notes = generateAsIsRow (fixture);

    SECTION ("segment 1 (Am) is a literal root-position triad")
    {
        std::vector<NoteEvent> seg1;
        for (const auto& n : notes)
            if (n.startBeats == 0.0)
                seg1.push_back (n);

        REQUIRE (seg1.size() == 3);

        std::vector<int> pitches;
        for (const auto& n : seg1)
            pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        CHECK (pitches == std::vector<int> { 69, 72, 76 });

        for (const auto& n : seg1)
        {
            CHECK (n.startBeats == 0.0);
            CHECK (n.lengthBeats == 4.0);
            CHECK (n.velocity == 0.75f);
        }
    }

    SECTION ("segment 4 (G7) keeps its 7th -- literal detected quality")
    {
        std::vector<NoteEvent> seg4;
        for (const auto& n : notes)
            if (n.startBeats == 12.0)
                seg4.push_back (n);

        REQUIRE (seg4.size() == 4);

        std::vector<int> pitches;
        for (const auto& n : seg4)
            pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        CHECK (pitches == std::vector<int> { 67, 71, 74, 77 });
    }

    SECTION ("NoChord segment emits zero notes")
    {
        const auto noChordFixture = midigen_fixtures::makeNoChordFixture();
        const auto noChordNotes = generateAsIsRow (noChordFixture);
        CHECK_FALSE (hasNoteStartingIn (noChordNotes, 4.0, 8.0));
    }
}

TEST_CASE ("StyleVoicingTests.PopTrapTriadOnlyCloseRegister", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notes = generatePopTrapRow (fixture, GenerationSettings {});

    SECTION ("G7 segment drops the 7th -- triad only")
    {
        std::set<int> pitchesInSeg4;
        for (const auto& n : notes)
            if (n.startBeats >= 12.0 && n.startBeats < 16.0)
                pitchesInSeg4.insert (n.pitch);

        const int root = rootMidiNote (7, kAnchorPopTrap);
        std::set<int> expected { root, root + 4, root + 7 };
        CHECK (pitchesInSeg4 == expected);
    }

    SECTION ("every root pitch matches the dark C3 anchor; chord tones stay <= 66")
    {
        for (const auto& n : notes)
            CHECK (n.pitch <= 66);
    }

    SECTION ("Am strike pitches are {57, 60, 64}")
    {
        std::set<int> pitchesInSeg1;
        for (const auto& n : notes)
            if (n.startBeats >= 0.0 && n.startBeats < 4.0)
                pitchesInSeg1.insert (n.pitch);

        CHECK (pitchesInSeg1 == std::set<int> { 57, 60, 64 });
    }
}

TEST_CASE ("StyleVoicingTests.PopTrapHalfBarRestrike", "[stylevoicing]")
{
    SECTION ("4-beat segment restrikes at 0 and 2")
    {
        const auto fixture = midigen_fixtures::makeFourChordFixture();
        const auto notes = generatePopTrapRow (fixture, GenerationSettings {});

        std::set<double> onsetsInSeg1;
        for (const auto& n : notes)
            if (n.startBeats >= 0.0 && n.startBeats < 4.0)
            {
                onsetsInSeg1.insert (n.startBeats);
                CHECK (n.lengthBeats == 2.0);
            }

        CHECK (onsetsInSeg1 == std::set<double> { 0.0, 2.0 });
    }

    SECTION ("3-beat segment: strike at 0 (length 2.0) + remainder strike at 2 (length 1.0)")
    {
        const auto fixture = midigen_fixtures::makeShortSegmentFixture();
        const auto notes = generatePopTrapRow (fixture, GenerationSettings {});

        // F segment: startBeatIndex 2, endBeatIndex 5 (3 beats)
        for (const auto& n : notes)
        {
            if (n.startBeats == 2.0)
                CHECK (n.lengthBeats == 2.0);
            else if (n.startBeats == 4.0)
                CHECK (n.lengthBeats == 1.0);
        }

        CHECK (hasNoteStartingIn (notes, 2.0, 2.001));
        CHECK (hasNoteStartingIn (notes, 4.0, 4.001));
    }
}

TEST_CASE ("StyleVoicingTests.PopTrapVelocityDeterministicAndBounded", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notesA = generatePopTrapRow (fixture, GenerationSettings {});
    const auto notesB = generatePopTrapRow (fixture, GenerationSettings {});

    REQUIRE (notesA.size() == notesB.size());

    for (size_t i = 0; i < notesA.size(); ++i)
    {
        CHECK (notesA[i].velocity >= 0.72f);
        CHECK (notesA[i].velocity <= 0.84f);

        CHECK (notesA[i].startBeats == notesB[i].startBeats);
        CHECK (notesA[i].lengthBeats == notesB[i].lengthBeats);
        CHECK (notesA[i].pitch == notesB[i].pitch);
        CHECK (notesA[i].velocity == notesB[i].velocity);
    }
}
