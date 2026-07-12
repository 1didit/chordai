#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Source/Analysis/OnsetEnvelope.h"
#include "Source/Analysis/TempoBeatTracker.h"
#include "Source/Analysis/AudioPreprocessing.h"
#include "Tests/SyntheticFixtures.h"

#include <cmath>
#include <numeric>
#include <vector>

namespace
{
    std::vector<float> renderClickTrackOnsetSamples (double bpm, double durationSeconds, bool syncopated = false)
    {
        constexpr double sourceRate = 44100.0;
        auto buffer = fixtures::renderClickTrack (bpm, sourceRate, durationSeconds, syncopated);
        auto preprocessed = preprocessForAnalysis (buffer, sourceRate, {});
        return preprocessed.onsetSamples;
    }

    double meanOf (const std::vector<double>& v)
    {
        if (v.empty())
            return 0.0;
        return std::accumulate (v.begin(), v.end(), 0.0) / (double) v.size();
    }

    double stdDevOf (const std::vector<double>& v)
    {
        if (v.empty())
            return 0.0;
        const double m = meanOf (v);
        double sumSq = 0.0;
        for (double x : v)
            sumSq += (x - m) * (x - m);
        return std::sqrt (sumSq / (double) v.size());
    }
}

// ---------------------------------------------------------------------
// Task 1: OnsetEnvelope
// ---------------------------------------------------------------------

TEST_CASE ("TempoBeatTrackerTests.OnsetEnvelopeRate", "[chordanalysis]")
{
    constexpr double durationSeconds = 4.0;
    auto samples8k = renderClickTrackOnsetSamples (120.0, durationSeconds);

    auto result = computeOnsetEnvelope (samples8k);

    CHECK (result.rateHz == Catch::Approx (250.0));
    const double expectedFrames = durationSeconds * 250.0;
    CHECK ((double) result.envelope.size() == Catch::Approx (expectedFrames).margin (2.0));
}

TEST_CASE ("TempoBeatTrackerTests.OnsetPeaksAtClicks", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr double durationSeconds = 8.0;
    auto samples8k = renderClickTrackOnsetSamples (bpm, durationSeconds);

    auto result = computeOnsetEnvelope (samples8k);
    REQUIRE (result.envelope.size() > 4);

    const double beatIntervalSeconds = 60.0 / bpm;
    std::vector<double> clickTimes;
    for (double t = 0.0; t < durationSeconds; t += beatIntervalSeconds)
        clickTimes.push_back (t);

    constexpr double toleranceSeconds = 0.012;
    const int toleranceFrames = juce::jmax (1, (int) std::llround (toleranceSeconds * result.rateHz));

    int hits = 0;
    for (double clickTime : clickTimes)
    {
        const int centerFrame = (int) std::llround (clickTime * result.rateHz);
        const int lo = juce::jmax (0, centerFrame - toleranceFrames);
        const int hi = juce::jmin ((int) result.envelope.size() - 1, centerFrame + toleranceFrames);
        if (lo > hi)
            continue;

        bool isLocalMax = false;
        for (int i = lo; i <= hi; ++i)
        {
            const double v = result.envelope[(size_t) i];
            const double left = i > 0 ? result.envelope[(size_t) i - 1] : v;
            const double right = i < (int) result.envelope.size() - 1 ? result.envelope[(size_t) i + 1] : v;
            if (v >= left && v >= right && v > 0.0)
            {
                isLocalMax = true;
                break;
            }
        }
        if (isLocalMax)
            ++hits;
    }

    const double hitRate = (double) hits / (double) clickTimes.size();
    INFO ("hitRate=" << hitRate << " hits=" << hits << " total=" << clickTimes.size());
    CHECK (hitRate >= 0.9);
}

TEST_CASE ("TempoBeatTrackerTests.OnsetEnvelopeZeroMeanish", "[chordanalysis]")
{
    auto samples8k = renderClickTrackOnsetSamples (120.0, 8.0);
    auto result = computeOnsetEnvelope (samples8k);
    REQUIRE (result.envelope.size() > 10);

    const double mean = meanOf (result.envelope);
    const double stdDev = stdDevOf (result.envelope);

    INFO ("mean=" << mean << " stdDev=" << stdDev);
    REQUIRE (stdDev > 1e-9);
    CHECK (std::abs (mean) < stdDev);
    CHECK (stdDev == Catch::Approx (1.0).margin (0.35));
}

// ---------------------------------------------------------------------
// Task 2: TempoBeatTracker
// ---------------------------------------------------------------------

namespace
{
    BeatGrid trackBeatsForClickTrack (double bpm, double durationSeconds, bool syncopated = false)
    {
        auto samples8k = renderClickTrackOnsetSamples (bpm, durationSeconds, syncopated);
        auto onset = computeOnsetEnvelope (samples8k);
        return trackBeats (onset);
    }
}

TEST_CASE ("TempoBeatTrackerTests.Detects90Bpm", "[chordanalysis]")
{
    auto grid = trackBeatsForClickTrack (90.0, 12.0);
    INFO ("bpm=" << grid.bpm);
    CHECK (grid.bpm == Catch::Approx (90.0).margin (3.0));
}

TEST_CASE ("TempoBeatTrackerTests.Detects120Bpm", "[chordanalysis]")
{
    auto grid = trackBeatsForClickTrack (120.0, 12.0);
    INFO ("bpm=" << grid.bpm);
    CHECK (grid.bpm == Catch::Approx (120.0).margin (3.0));
}

TEST_CASE ("TempoBeatTrackerTests.Detects160Bpm", "[chordanalysis]")
{
    auto grid = trackBeatsForClickTrack (160.0, 12.0);
    INFO ("bpm=" << grid.bpm);
    CHECK (grid.bpm == Catch::Approx (160.0).margin (3.0));
}

TEST_CASE ("TempoBeatTrackerTests.OctaveErrorResistance", "[chordanalysis]")
{
    auto grid = trackBeatsForClickTrack (100.0, 12.0, true);
    INFO ("bpm=" << grid.bpm);
    CHECK (grid.bpm == Catch::Approx (100.0).margin (3.0));
    CHECK (std::abs (grid.bpm - 50.0) > 5.0);
    CHECK (std::abs (grid.bpm - 200.0) > 5.0);
}

TEST_CASE ("TempoBeatTrackerTests.BeatTimesAlignToClicks", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr double durationSeconds = 12.0;
    auto grid = trackBeatsForClickTrack (bpm, durationSeconds);
    REQUIRE (! grid.beatTimesSeconds.empty());

    const double beatIntervalSeconds = 60.0 / bpm;

    int aligned = 0;
    for (double beatTime : grid.beatTimesSeconds)
    {
        const double nearestClickIndex = std::round (beatTime / beatIntervalSeconds);
        const double nearestClickTime = nearestClickIndex * beatIntervalSeconds;
        if (std::abs (beatTime - nearestClickTime) <= 0.070)
            ++aligned;
    }

    const double alignedFraction = (double) aligned / (double) grid.beatTimesSeconds.size();
    INFO ("alignedFraction=" << alignedFraction << " aligned=" << aligned << " total=" << grid.beatTimesSeconds.size());
    CHECK (alignedFraction >= 0.8);
}

TEST_CASE ("TempoBeatTrackerTests.BarGrid", "[chordanalysis]")
{
    auto grid = trackBeatsForClickTrack (120.0, 12.0);
    REQUIRE (! grid.beatTimesSeconds.empty());
    REQUIRE (! grid.barStartBeatIndices.empty());

    for (size_t i = 0; i < grid.barStartBeatIndices.size(); ++i)
        CHECK (grid.barStartBeatIndices[i] == (int) (i * 4));

    const int lastBarStart = grid.barStartBeatIndices.back();
    CHECK (lastBarStart < (int) grid.beatTimesSeconds.size());
    CHECK ((size_t) grid.barStartBeatIndices.size() == (grid.beatTimesSeconds.size() + 3) / 4);
}

TEST_CASE ("TempoBeatTrackerTests.DegenerateInput", "[chordanalysis]")
{
    OnsetEnvelopeResult emptyOnset;
    auto grid = trackBeats (emptyOnset);
    CHECK (grid.bpm == Catch::Approx (0.0));
    CHECK (grid.beatTimesSeconds.empty());
    CHECK (grid.barStartBeatIndices.empty());

    OnsetEnvelopeResult shortOnset;
    shortOnset.envelope = { 0.1, 0.2, 0.1 };
    shortOnset.rateHz = 250.0;
    auto grid2 = trackBeats (shortOnset);
    CHECK (grid2.bpm == Catch::Approx (0.0));
    CHECK (grid2.beatTimesSeconds.empty());
    CHECK (grid2.barStartBeatIndices.empty());
}
