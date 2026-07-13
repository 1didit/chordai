#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/MidiRowBuilder.h"
#include "Tests/MidiGenFixtures.h"

#include <cmath>

// Remaining TEST_CASEs (orchestration, determinism, NoChord handling,
// performance) land in plan 05-04.

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
}

TEST_CASE ("MidiRowBuilderTests.FixtureSelfConsistency", "[midirowbuilder]")
{
    checkSelfConsistent (midigen_fixtures::makeFourChordFixture());
    checkSelfConsistent (midigen_fixtures::makeNoChordFixture());
    checkSelfConsistent (midigen_fixtures::makeShortSegmentFixture());
    checkSelfConsistent (midigen_fixtures::makeRealTrackScaleFixture());
}
