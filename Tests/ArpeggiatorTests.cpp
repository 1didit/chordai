#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/Arpeggiator.h"

TEST_CASE ("ArpeggiatorTests.UpStepsAndWraps", "[arpeggiator]")
{
    const std::vector<int> tones { 60, 64, 67 };
    const auto result = arpeggiate (tones, ArpDirection::Up, 7);
    const std::vector<int> expected { 60, 64, 67, 60, 64, 67, 60 };
    CHECK (result == expected);
}

TEST_CASE ("ArpeggiatorTests.DownStepsAndWraps", "[arpeggiator]")
{
    const std::vector<int> tones { 60, 64, 67 };
    const auto result = arpeggiate (tones, ArpDirection::Down, 5);
    const std::vector<int> expected { 67, 64, 60, 67, 64 };
    CHECK (result == expected);
}

TEST_CASE ("ArpeggiatorTests.UpDownPingPongsWithoutRepeatingEndpoints", "[arpeggiator]")
{
    const std::vector<int> tones { 60, 64, 67 };
    const auto result = arpeggiate (tones, ArpDirection::UpDown, 8);
    const std::vector<int> expected { 60, 64, 67, 64, 60, 64, 67, 64 };
    CHECK (result == expected);
}

TEST_CASE ("ArpeggiatorTests.EmptyAndSingleToneSafe", "[arpeggiator]")
{
    CHECK (arpeggiate ({}, ArpDirection::Up, 7).empty());

    const std::vector<int> single { 60 };
    const auto result = arpeggiate (single, ArpDirection::Up, 4);
    const std::vector<int> expected { 60, 60, 60, 60 };
    CHECK (result == expected);

    CHECK (arpeggiate ({ 60, 64, 67 }, ArpDirection::Up, 0).empty());
    CHECK (arpeggiate ({ 60, 64, 67 }, ArpDirection::Up, -3).empty());
}
