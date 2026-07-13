#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/ChordToneMapper.h"
#include "Source/MidiGen/Humanization.h"
#include "Source/MidiGen/VoiceLeadingEngine.h"

#include <algorithm>

TEST_CASE ("ChordToneMapperTests.TriadIntervalsMatchDetectorConvention", "[chordtonemapper]")
{
    // Identical semitone offsets to ChordTemplates.h's buildTemplate() -- the
    // detector's own interpretation of quality, not a second one.
    CHECK (triadIntervals (ChordQuality::Major) == std::vector<int> { 0, 4, 7 });
    CHECK (triadIntervals (ChordQuality::Minor) == std::vector<int> { 0, 3, 7 });
    CHECK (triadIntervals (ChordQuality::Dominant7) == std::vector<int> { 0, 4, 7, 10 });

    // Pitfall 1: NoChord must be an explicit empty case, never a Major fallthrough.
    CHECK (triadIntervals (ChordQuality::NoChord).empty());
}

TEST_CASE ("ChordToneMapperTests.RnbExtensionsMatchQualityTable", "[chordtonemapper]")
{
    CHECK (rnbExtensionIntervals (ChordQuality::Major) == std::vector<int> { 0, 4, 7, 11, 14 });     // maj9
    CHECK (rnbExtensionIntervals (ChordQuality::Minor) == std::vector<int> { 0, 3, 7, 10, 14, 17 }); // min11
    CHECK (rnbExtensionIntervals (ChordQuality::Dominant7) == std::vector<int> { 0, 4, 7, 10, 14 }); // dom9

    // Pitfall 1: NoChord must be an explicit empty case.
    CHECK (rnbExtensionIntervals (ChordQuality::NoChord).empty());
}

TEST_CASE ("ChordToneMapperTests.IntervalsToMidiNotesClampsToValidRange", "[chordtonemapper]")
{
    auto highNotes = intervalsToMidiNotes (125, { 0, 4, 7 });
    for (int n : highNotes)
        CHECK (n <= 127);

    auto lowNotes = intervalsToMidiNotes (-5, { 0, 4, 7 });
    for (int n : lowNotes)
        CHECK (n >= 0);
}

TEST_CASE ("ChordToneMapperTests.RootMidiNoteAnchors", "[chordtonemapper]")
{
    CHECK (rootMidiNote (9, 48) == 57);   // A2..A in the C3 anchor octave band
    CHECK (rootMidiNote (0, 60) == 60);
    CHECK (rootMidiNote (11, 72) == 83);
}

TEST_CASE ("ChordToneMapperTests.NearestOctaveNotePicksClosestOctave", "[chordtonemapper]")
{
    CHECK (nearestOctaveNote (0 /* C */, 62 /* prev D4 */) == 60);
    CHECK (nearestOctaveNote (11 /* B */, 60) == 59); // down, not 71

    for (int pc = 0; pc < 12; ++pc)
    {
        for (int prev : { -50, 0, 30, 60, 90, 200 })
        {
            int result = nearestOctaveNote (pc, prev);
            CHECK (result >= 48);
            CHECK (result <= 72);
        }
    }
}

TEST_CASE ("ChordToneMapperTests.DeterministicJitterIsPureAndBounded", "[chordtonemapper]")
{
    constexpr float range = 0.1f;

    SECTION ("same inputs produce exactly the same output")
    {
        for (uint32_t i = 0; i < 32; ++i)
            CHECK (deterministicJitter (i, kSeedPopTrap, range) == deterministicJitter (i, kSeedPopTrap, range));
    }

    SECTION ("bounded to [-range/2, range/2]")
    {
        for (uint32_t i = 0; i < 1000; ++i)
        {
            float jitter = deterministicJitter (i, kSeedRnb, range);
            CHECK (jitter >= -range / 2.0f);
            CHECK (jitter <= range / 2.0f);
        }
    }

    SECTION ("different seeds produce different sequences")
    {
        bool foundDifference = false;
        for (uint32_t i = 0; i < 16; ++i)
        {
            if (deterministicJitter (i, kSeedPopTrap, range) != deterministicJitter (i, kSeedHouse, range))
            {
                foundDifference = true;
                break;
            }
        }
        CHECK (foundDifference);
    }
}
