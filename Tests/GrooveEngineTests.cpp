#include <JuceHeader.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/GrooveEngine.h"
#include "Source/MidiGen/MidiFileWriter.h"

#include <cmath>

namespace
{
    // isTickExact against the real export resolution -- every named swing
    // constant/offset must land on an exact integer tick (960 = 2^6*3*5).
    bool divides960Exactly (double beats)
    {
        return isTickExact (beats, MidiFileWriter::kTicksPerQuarterNote);
    }
}

TEST_CASE ("GrooveEngineTests.AllNamedSwingConstantsDivide960Exactly", "[grooveengine]")
{
    CHECK (divides960Exactly (kSwingStraight));
    CHECK (divides960Exactly (kSwingLight));
    CHECK (divides960Exactly (kSwingTriplet));

    for (int i = 0; i < 4; ++i)
    {
        CHECK (divides960Exactly (swingSixteenthOffsetBeats (i, kSwingStraight)));
        CHECK (divides960Exactly (swingSixteenthOffsetBeats (i, kSwingLight)));
        CHECK (divides960Exactly (swingSixteenthOffsetBeats (i, kSwingTriplet)));
    }
}

TEST_CASE ("GrooveEngineTests.SwingStraightReproducesUnswungGrid", "[grooveengine]")
{
    CHECK (swingSixteenthOffsetBeats (0, kSwingStraight) == Catch::Approx (0.0));
    CHECK (swingSixteenthOffsetBeats (1, kSwingStraight) == Catch::Approx (0.25));
    CHECK (swingSixteenthOffsetBeats (2, kSwingStraight) == Catch::Approx (0.5));
    CHECK (swingSixteenthOffsetBeats (3, kSwingStraight) == Catch::Approx (0.75));
}

TEST_CASE ("GrooveEngineTests.SwingLightDelaysSecondSixteenth", "[grooveengine]")
{
    CHECK (swingSixteenthOffsetBeats (1, kSwingLight) == Catch::Approx (7.0 / 24.0));
    CHECK (swingSixteenthOffsetBeats (3, kSwingLight) == Catch::Approx (0.5 + 7.0 / 24.0));
}

TEST_CASE ("GrooveEngineTests.TileOnsetsTilesAndTruncates", "[grooveengine]")
{
    const std::vector<double> onsets { 0.0, 0.75, 1.5 };
    const auto tiled = tileOnsets (onsets, 2.0, 5.0);

    const std::vector<double> expected { 0.0, 0.75, 1.5, 2.0, 2.75, 3.5, 4.0, 4.75 };
    REQUIRE (tiled.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        CHECK (tiled[i] == Catch::Approx (expected[i]));

    for (double onset : tiled)
        CHECK (onset < 5.0);
}

TEST_CASE ("GrooveEngineTests.TileOnsetsEmptyAndDegenerateSafe", "[grooveengine]")
{
    CHECK (tileOnsets ({}, 2.0, 5.0).empty());
    CHECK (tileOnsets ({ 0.0, 0.5 }, 0.0, 5.0).empty());
    CHECK (tileOnsets ({ 0.0, 0.5 }, -1.0, 5.0).empty());
    CHECK (tileOnsets ({ 0.0, 0.5 }, 2.0, 0.0).empty());
}

TEST_CASE ("GrooveEngineTests.SwingOffsetsRoundTripExactTicksThroughMidiFileWriter", "[grooveengine]")
{
    // Research Open Question 1, closed empirically: a swung note's beat
    // offset must land on an exact integer tick through the REAL
    // MidiFileWriter, not just via the arithmetic alone.
    MidiSetRow row;
    row.id = "as-is";
    row.notes = { { swingSixteenthOffsetBeats (1, kSwingLight), 0.25, 60, 0.8f } }; // 7/24 beats

    auto midiFile = MidiFileWriter::buildMidiFile (row, 120.0);

    juce::MemoryOutputStream mos;
    REQUIRE (midiFile.writeTo (mos, 1));
    juce::MidiFile readBack;
    juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
    REQUIRE (readBack.readFrom (mis));

    bool foundNoteOn = false;
    for (int t = 0; t < readBack.getNumTracks(); ++t)
    {
        const auto* track = readBack.getTrack (t);
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer (i)->message;
            if (msg.isNoteOn() && msg.getNoteNumber() == 60)
            {
                foundNoteOn = true;
                CHECK (msg.getTimeStamp() == Catch::Approx (280.0)); // 7/24 * 960
            }
        }
    }
    CHECK (foundNoteOn);
}
