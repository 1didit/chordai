#include <catch2/catch_test_macros.hpp>

#include "Source/Analysis/AnalysisResult.h"
#include "Source/MidiGen/AsIsRowGenerator.h"
#include "Source/MidiGen/MidiRowBuilder.h"
#include "Tests/MidiGenFixtures.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
    void checkSelfConsistent (const AnalysisResult& result)
    {
        REQUIRE (result.beatTimesSeconds.size() > 1);

        for (size_t i = 1; i < result.beatTimesSeconds.size(); ++i)
            CHECK (result.beatTimesSeconds[i] > result.beatTimesSeconds[i - 1]);

        for (int barStart : result.barStartBeatIndices)
            CHECK (barStart % 4 == 0);

        const int lastBeatIndex = (int) result.beatTimesSeconds.size() - 1;

        for (const auto& segment : result.chords)
        {
            CHECK (segment.endBeatIndex > segment.startBeatIndex);
            CHECK (segment.endBeatIndex <= lastBeatIndex);

            const double expectedStart = (double) segment.startBeatIndex * 60.0 / result.bpm;
            const double expectedEnd = (double) segment.endBeatIndex * 60.0 / result.bpm;
            CHECK (std::abs (segment.startSeconds - expectedStart) < 1e-9);
            CHECK (std::abs (segment.endSeconds - expectedEnd) < 1e-9);
        }
    }

    bool notesEqual (const NoteEvent& a, const NoteEvent& b)
    {
        return a.startBeats == b.startBeats
            && a.lengthBeats == b.lengthBeats
            && a.pitch == b.pitch
            && a.velocity == b.velocity;
    }

    bool rowNotesEqual (const std::vector<NoteEvent>& a, const std::vector<NoteEvent>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (! notesEqual (a[i], b[i]))
                return false;

        return true;
    }
}

TEST_CASE ("MidiRowBuilderTests.FixtureSelfConsistency", "[midirowbuilder]")
{
    checkSelfConsistent (midigen_fixtures::makeFourChordFixture());
    checkSelfConsistent (midigen_fixtures::makeNoChordFixture());
    checkSelfConsistent (midigen_fixtures::makeShortSegmentFixture());
    checkSelfConsistent (midigen_fixtures::makeRealTrackScaleFixture());
}

TEST_CASE ("MidiRowBuilderTests.FiveRowsForKnownProgression", "[midirowbuilder]")
{
    const auto rows = generateAllRows (midigen_fixtures::makeFourChordFixture());

    REQUIRE (rows.size() == 5);

    CHECK (rows[0].id == "as-is");
    CHECK (rows[0].label == "Detected");
    CHECK (rows[0].kind == PatternKind::SustainedChords);
    CHECK (rows[0].patternIndex == 0);

    CHECK (rows[1].id == "pop-trap");
    CHECK (rows[1].label == "Pop / Trap");
    CHECK (rows[1].kind == PatternKind::RhythmicChords);
    CHECK (rows[1].patternIndex == 1);

    CHECK (rows[2].id == "rnb-neosoul");
    CHECK (rows[2].label == "R&B / Neo-Soul");
    CHECK (rows[2].kind == PatternKind::StabArp);
    CHECK (rows[2].patternIndex == 2);

    CHECK (rows[3].id == "house");
    CHECK (rows[3].label == "Electronic / House");
    CHECK (rows[3].kind == PatternKind::TopLineMotif);
    CHECK (rows[3].patternIndex == 4);

    CHECK (rows[4].id == "bass");
    CHECK (rows[4].label == "Bass");
    CHECK (rows[4].kind == PatternKind::BassLine);
    CHECK (rows[4].patternIndex == 3);

    for (const auto& row : rows)
        CHECK_FALSE (row.notes.empty());
}

TEST_CASE ("MidiRowBuilderTests.AsIsRowMatchesDetectedProgression", "[midirowbuilder]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto rows = generateAllRows (fixture);
    const auto expected = generateAsIsRow (fixture);

    REQUIRE (rows.size() == 5);
    CHECK (rowNotesEqual (rows[0].notes, expected));
}

TEST_CASE ("MidiRowBuilderTests.NoChordSegmentEmitsNoNotes", "[midirowbuilder]")
{
    const auto rows = generateAllRows (midigen_fixtures::makeNoChordFixture());

    REQUIRE (rows.size() == 5);

    for (const auto& row : rows)
        for (const auto& note : row.notes)
            CHECK_FALSE ((note.startBeats >= 4.0 && note.startBeats < 8.0));
}

TEST_CASE ("MidiRowBuilderTests.SameInputProducesByteIdenticalRows", "[midirowbuilder]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto rowsA = generateAllRows (fixture);
    const auto rowsB = generateAllRows (fixture);

    REQUIRE (rowsA.size() == rowsB.size());

    for (size_t i = 0; i < rowsA.size(); ++i)
    {
        CHECK (rowsA[i].id == rowsB[i].id);
        CHECK (rowNotesEqual (rowsA[i].notes, rowsB[i].notes));
    }
}

TEST_CASE ("MidiRowBuilderTests.EmptyResultProducesFiveEmptyRows", "[midirowbuilder]")
{
    AnalysisResult empty;
    const auto rows = generateAllRows (empty);

    REQUIRE (rows.size() == 5);

    for (const auto& row : rows)
        CHECK (row.notes.empty());
}

TEST_CASE ("MidiRowBuilderTests.GenerationPerformanceBudget", "[midirowbuilder]")
{
    const auto fixture = midigen_fixtures::makeRealTrackScaleFixture();

    double minElapsedMs = 1.0e9;
    for (int run = 0; run < 5; ++run)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto rows = generateAllRows (fixture);
        const auto end = std::chrono::steady_clock::now();
        REQUIRE (rows.size() == 5);

        const double elapsedMs = std::chrono::duration<double, std::milli> (end - start).count();
        minElapsedMs = std::min (minElapsedMs, elapsedMs);
    }

    CHECK (minElapsedMs < 1.0);
}
