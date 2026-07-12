#include <catch2/catch_test_macros.hpp>

#include "Source/Analysis/ChordDecoder.h"
#include "Source/Analysis/ChordTemplates.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kEpsilon = 1e-4f;

    float l2Norm (const std::array<float, 12>& v)
    {
        float sum = 0.0f;
        for (float x : v)
            sum += x * x;
        return std::sqrt (sum);
    }

    int argmax36 (const std::array<float, 36>& scores)
    {
        return (int) std::distance (scores.begin(), std::max_element (scores.begin(), scores.end()));
    }
}

TEST_CASE ("ChordDecoderTests.TemplateShapes", "[chordanalysis]")
{
    auto cMaj = buildTemplate (0, ChordQuality::Major);
    for (int pc : { 0, 4, 7 })
        REQUIRE (cMaj[(size_t) pc] > 0.0f);
    for (int pc = 0; pc < 12; ++pc)
        if (pc != 0 && pc != 4 && pc != 7)
            REQUIRE (cMaj[(size_t) pc] == 0.0f);
    REQUIRE (cMaj[0] == cMaj[4]);
    REQUIRE (cMaj[4] == cMaj[7]);
    REQUIRE (std::abs (l2Norm (cMaj) - 1.0f) < kEpsilon);

    auto aMin = buildTemplate (9, ChordQuality::Minor);
    for (int pc : { 9, 0, 4 })
        REQUIRE (aMin[(size_t) pc] > 0.0f);
    for (int pc = 0; pc < 12; ++pc)
        if (pc != 9 && pc != 0 && pc != 4)
            REQUIRE (aMin[(size_t) pc] == 0.0f);
    REQUIRE (std::abs (l2Norm (aMin) - 1.0f) < kEpsilon);

    auto g7 = buildTemplate (7, ChordQuality::Dominant7);
    for (int pc : { 7, 11, 2, 5 })
        REQUIRE (g7[(size_t) pc] > 0.0f);
    for (int pc = 0; pc < 12; ++pc)
        if (pc != 7 && pc != 11 && pc != 2 && pc != 5)
            REQUIRE (g7[(size_t) pc] == 0.0f);
    REQUIRE (std::abs (l2Norm (g7) - 1.0f) < kEpsilon);
}

TEST_CASE ("ChordDecoderTests.IndexSymbolRoundTrip", "[chordanalysis]")
{
    for (int index = 0; index < 36; ++index)
    {
        ChordSymbol s = symbolForIndex (index);
        REQUIRE (s.quality != ChordQuality::NoChord);
        int roundTripIndex = indexForSymbol (s);
        REQUIRE (roundTripIndex == index);

        ChordSymbol s2 = symbolForIndex (roundTripIndex);
        REQUIRE (s2.pitchClass == s.pitchClass);
        REQUIRE (s2.quality == s.quality);
    }
}

TEST_CASE ("ChordDecoderTests.ScoringPrefersCorrectChord", "[chordanalysis]")
{
    auto templates = buildAllTemplates();

    BeatChroma beat;
    beat.harmonicAvg[0] = 1.0f;
    beat.harmonicAvg[4] = 1.0f;
    beat.harmonicAvg[7] = 1.0f;
    // bassAvg left all-zero -- isolate cosine-only preference for this test.

    auto scores = scoreBeatObservations (beat, templates);
    int best = argmax36 (scores);
    ChordSymbol bestSymbol = symbolForIndex (best);

    REQUIRE (bestSymbol.pitchClass == 0);
    REQUIRE (bestSymbol.quality == ChordQuality::Major);
}

TEST_CASE ("ChordDecoderTests.BassBiasBreaksTie", "[chordanalysis]")
{
    auto templates = buildAllTemplates();

    BeatChroma ambiguous;
    for (int pc : { 9, 0, 4, 7 })
        ambiguous.harmonicAvg[(size_t) pc] = 1.0f;

    SECTION ("bass peaked at A (pc 9) -> Am wins")
    {
        BeatChroma beat = ambiguous;
        beat.bassAvg[9] = 1.0f;

        auto scores = scoreBeatObservations (beat, templates);
        ChordSymbol best = symbolForIndex (argmax36 (scores));

        REQUIRE (best.pitchClass == 9);
        REQUIRE (best.quality == ChordQuality::Minor);
    }

    SECTION ("bass peaked at C (pc 0) -> C major wins")
    {
        BeatChroma beat = ambiguous;
        beat.bassAvg[0] = 1.0f;

        auto scores = scoreBeatObservations (beat, templates);
        ChordSymbol best = symbolForIndex (argmax36 (scores));

        REQUIRE (best.pitchClass == 0);
        REQUIRE (best.quality == ChordQuality::Major);
    }
}
