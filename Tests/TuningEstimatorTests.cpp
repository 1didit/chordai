#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Source/Analysis/TuningEstimator.h"
#include "Source/Analysis/ConstantQAnalysis.h"
#include "Source/Analysis/AudioPreprocessing.h"
#include "Tests/SyntheticFixtures.h"

#include <cmath>

namespace
{
    constexpr double kSourceRate = 44100.0;
    constexpr double kBpm = 90.0;

    std::vector<fixtures::ChordSpec> cFGcProgression()
    {
        return {
            { { 0, ChordQuality::Major } },
            { { 5, ChordQuality::Major } },
            { { 7, ChordQuality::Major } },
            { { 0, ChordQuality::Major } },
        };
    }

    double estimateForDetune (double detuneCents)
    {
        auto buffer = fixtures::renderChordProgression (cFGcProgression(), kBpm, kSourceRate, 4, detuneCents);
        auto preprocessed = preprocessForAnalysis (buffer, kSourceRate, {});
        auto cqt = computeCqt (preprocessed.chromaSamples, preprocessed.chromaRate);
        return estimateTuningCents (cqt);
    }
}

TEST_CASE ("TuningEstimatorTests.NearZeroOnTunedFixture", "[chordanalysis]")
{
    const double thetaCents = estimateForDetune (0.0);
    INFO ("thetaCents=" << thetaCents);
    CHECK (std::abs (thetaCents) < 8.0);
}

TEST_CASE ("TuningEstimatorTests.DetectsMinus30Cents", "[chordanalysis]")
{
    const double thetaCents = estimateForDetune (-30.0);
    INFO ("thetaCents=" << thetaCents);
    CHECK (thetaCents >= -40.0);
    CHECK (thetaCents <= -20.0);
}

TEST_CASE ("TuningEstimatorTests.DetectsPlus25Cents", "[chordanalysis]")
{
    const double thetaCents = estimateForDetune (25.0);
    INFO ("thetaCents=" << thetaCents);
    CHECK (thetaCents >= 15.0);
    CHECK (thetaCents <= 35.0);
}
