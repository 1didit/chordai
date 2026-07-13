#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/PatternSeed.h"
#include "Tests/MidiGenFixtures.h"

TEST_CASE ("PatternSeedTests.SameInputsProduceSameSeed", "[patternseed]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    const auto baseA = computeBaseSeed (fixture, "trap", 2);
    const auto baseB = computeBaseSeed (fixture, "trap", 2);
    CHECK (baseA == baseB);

    const auto combinedA = combineSeedWithVariation (baseA, 5);
    const auto combinedB = combineSeedWithVariation (baseA, 5);
    CHECK (combinedA == combinedB);
}

TEST_CASE ("PatternSeedTests.DistinctIdentityProducesDistinctSeeds", "[patternseed]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    const auto baseTrap0 = computeBaseSeed (fixture, "trap", 0);
    const auto baseHouse0 = computeBaseSeed (fixture, "house", 0);
    const auto baseTrap1 = computeBaseSeed (fixture, "trap", 1);

    CHECK (baseTrap0 != baseHouse0); // different genreId
    CHECK (baseTrap0 != baseTrap1);  // different patternIndex
    CHECK (baseHouse0 != baseTrap1);

    const auto combined0 = combineSeedWithVariation (baseTrap0, 0);
    const auto combined1 = combineSeedWithVariation (baseTrap0, 1);
    CHECK (combined0 != combined1); // different variationCounter
}

TEST_CASE ("PatternSeedTests.HashIgnoresFloatFields", "[patternseed]")
{
    auto a = midigen_fixtures::makeFourChordFixture();
    auto b = a;

    // Perturb only float-valued fields -- the hash must be byte-identical.
    b.bpm = a.bpm + 37.5;
    b.analyzedRegionSeconds = juce::Range<double> (a.analyzedRegionSeconds.getStart() + 1.0,
                                                     a.analyzedRegionSeconds.getEnd() + 2.0);
    for (auto& seg : b.chords)
    {
        seg.startSeconds += 100.0;
        seg.endSeconds += 100.0;
        seg.confidence = 1.0f - seg.confidence;
    }

    CHECK (hashChordProgression (a) == hashChordProgression (b));
}

TEST_CASE ("PatternSeedTests.HashSensitiveToHarmony", "[patternseed]")
{
    auto a = midigen_fixtures::makeFourChordFixture();

    auto bPitch = a;
    bPitch.chords[0].chord.pitchClass = (bPitch.chords[0].chord.pitchClass + 1) % 12;
    CHECK (hashChordProgression (a) != hashChordProgression (bPitch));

    auto bQuality = a;
    bQuality.chords[0].chord.quality = ChordQuality::NoChord;
    CHECK (hashChordProgression (a) != hashChordProgression (bQuality));
}
