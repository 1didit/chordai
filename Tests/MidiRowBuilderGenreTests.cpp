#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/GenreRegistry.h"
#include "Source/MidiGen/MidiRowBuilder.h"
#include "Tests/MidiGenFixtures.h"

namespace
{
    bool notesEqual (const NoteEvent& a, const NoteEvent& b)
    {
        return a.startBeats == b.startBeats
            && a.lengthBeats == b.lengthBeats
            && a.pitch == b.pitch
            && a.velocity == b.velocity;
    }

    bool noteVectorsEqual (const std::vector<NoteEvent>& a, const std::vector<NoteEvent>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (! notesEqual (a[i], b[i]))
                return false;

        return true;
    }
}

TEST_CASE ("MidiRowBuilderGenreTests.FiveRowsForKnownGenreAndProgression", "[midirowbuildergenre]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto* trap = findGenre ("trap");
    REQUIRE (trap != nullptr);

    const auto rows = generateGenreRows (fixture, *trap);
    REQUIRE (rows.size() == 5);

    const std::vector<juce::String> expectedIds {
        "trap-sustained", "trap-rhythmic", "trap-stab-arp", "trap-bass", "trap-top-line"
    };

    for (int i = 0; i < 5; ++i)
    {
        CHECK (rows[(size_t) i].patternIndex == i);
        CHECK (rows[(size_t) i].kind == (PatternKind) i);
        CHECK (rows[(size_t) i].id == expectedIds[(size_t) i]);
        CHECK (rows[(size_t) i].label.startsWith ("Trap"));
    }

    // Every non-bass row is non-empty for the 4-chord fixture.
    for (int i = 0; i < 5; ++i)
        if ((PatternKind) i != PatternKind::BassLine)
            CHECK_FALSE (rows[(size_t) i].notes.empty());
}

TEST_CASE ("MidiRowBuilderGenreTests.SameInputProducesByteIdenticalRows", "[midirowbuildergenre]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto* trap = findGenre ("trap");
    REQUIRE (trap != nullptr);

    const auto rowsA = generateGenreRows (fixture, *trap);
    const auto rowsB = generateGenreRows (fixture, *trap);

    REQUIRE (rowsA.size() == rowsB.size());
    for (size_t i = 0; i < rowsA.size(); ++i)
    {
        CHECK (rowsA[i].id == rowsB[i].id);
        CHECK (noteVectorsEqual (rowsA[i].notes, rowsB[i].notes));
    }
}

TEST_CASE ("MidiRowBuilderGenreTests.NoChordSegmentEmitsNoNotesAcrossAllSlots", "[midirowbuildergenre]")
{
    const auto fixture = midigen_fixtures::makeNoChordFixture(); // segment beats 4-8 is NoChord
    const auto* trap = findGenre ("trap");
    REQUIRE (trap != nullptr);

    const auto rows = generateGenreRows (fixture, *trap);
    REQUIRE (rows.size() == 5);

    for (const auto& row : rows)
        for (const auto& note : row.notes)
            CHECK_FALSE ((note.startBeats >= 4.0 && note.startBeats < 8.0));
}

TEST_CASE ("MidiRowBuilderGenreTests.DistinctGenresProduceDistinctRows", "[midirowbuildergenre]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto* trap = findGenre ("trap");
    const auto* house = findGenre ("house");
    REQUIRE (trap != nullptr);
    REQUIRE (house != nullptr);

    const auto trapRows = generateGenreRows (fixture, *trap);
    const auto houseRows = generateGenreRows (fixture, *house);
    REQUIRE (trapRows.size() == houseRows.size());

    int differingSlots = 0;
    for (size_t i = 0; i < trapRows.size(); ++i)
        if (! noteVectorsEqual (trapRows[i].notes, houseRows[i].notes))
            ++differingSlots;

    CHECK (differingSlots >= 3);
}

TEST_CASE ("MidiRowBuilderGenreTests.RegenerateRowMatchesCounterSemantics", "[midirowbuildergenre]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto* trap = findGenre ("trap");
    REQUIRE (trap != nullptr);

    constexpr int kStabArpIndex = (int) PatternKind::StabArp;

    const auto baseline = generateGenreRows (fixture, *trap);
    const auto regenBaseline = regenerateRow (fixture, *trap, kStabArpIndex, 0);
    CHECK (noteVectorsEqual (regenBaseline.notes, baseline[(size_t) kStabArpIndex].notes));

    const auto regenVariant1 = regenerateRow (fixture, *trap, kStabArpIndex, 1);
    CHECK_FALSE (noteVectorsEqual (regenVariant1.notes, regenBaseline.notes));

    // Deterministic per counter value: repeated call with the same counter
    // reproduces the same notes.
    const auto regenVariant1Again = regenerateRow (fixture, *trap, kStabArpIndex, 1);
    CHECK (noteVectorsEqual (regenVariant1.notes, regenVariant1Again.notes));
}

TEST_CASE ("MidiRowBuilderGenreTests.RegenerateRowPreservesIdentity", "[midirowbuildergenre]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto* trap = findGenre ("trap");
    REQUIRE (trap != nullptr);

    constexpr int kStabArpIndex = (int) PatternKind::StabArp;

    const auto baseline = generateGenreRows (fixture, *trap);
    const auto regen = regenerateRow (fixture, *trap, kStabArpIndex, 5);

    CHECK (regen.id == baseline[(size_t) kStabArpIndex].id);
    CHECK (regen.label == baseline[(size_t) kStabArpIndex].label);
    CHECK (regen.kind == baseline[(size_t) kStabArpIndex].kind);
    CHECK (regen.patternIndex == baseline[(size_t) kStabArpIndex].patternIndex);
}
