#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "cq/CQParameters.h"
#include "cq/CQSpectrogram.h"

#include <cmath>
#include <numbers>

namespace
{
    constexpr double kSampleRate = 11025.0;
}

// Exercises the RAW constant-q-cpp library API (not the ChromaExtractor
// wrapper, still a stub owned by plan 03-02). Resolves 03-RESEARCH.md Open
// Question 1: logs the actual runtime-queried hop/bin-count/min-freq values.
TEST_CASE ("ChromaExtractorTests.CqtEngineSanity", "[chordanalysis]")
{
    CQParameters params (kSampleRate, 32.70, 4186.01, 36);
    // InterpolateLinear: per CQSpectrogram.h doc comment, "in the other
    // interpolation modes every cell will be filled" (InterpolateZeros
    // leaves lower-octave cells empty) — this test wants fully-populated
    // rectangular columns.
    CQSpectrogram cq (params, CQSpectrogram::InterpolateLinear);
    REQUIRE (cq.isValid());

    // 2.0 s, 440 Hz sine at 11025 Hz.
    constexpr double durationSeconds = 2.0;
    const int numSamples = (int) std::lround (durationSeconds * kSampleRate);
    std::vector<double> sine (numSamples);
    for (int i = 0; i < numSamples; ++i)
        sine[(size_t) i] = std::sin (2.0 * std::numbers::pi * 440.0 * (double) i / kSampleRate);

    auto columns = cq.process (sine);

    // Pitfall: skipping the flush silently drops the tail — always call
    // getRemainingOutput() and append.
    auto remaining = cq.getRemainingOutput();
    columns.insert (columns.end(), remaining.begin(), remaining.end());

    // Query, never hardcode (research pitfall #2).
    const int totalBins = cq.getTotalBins();
    const double minFreq = cq.getMinFrequency();
    const int columnHopSamples = cq.getColumnHop();
    const double columnHopSeconds = (double) columnHopSamples / kSampleRate;

    INFO ("getTotalBins() = " << totalBins);
    INFO ("getMinFrequency() = " << minFreq);
    INFO ("getColumnHop() = " << columnHopSamples << " samples (" << columnHopSeconds << " s)");
    WARN ("CqtEngineSanity observed: totalBins=" << totalBins
          << " minFrequency=" << minFreq
          << " columnHopSamples=" << columnHopSamples
          << " columnHopSeconds=" << columnHopSeconds);

    CHECK (totalBins >= 36 * 6);

    // Research Open Question 1 resolved empirically: the library does NOT
    // return ~32.70 Hz (C1) for a requested minFrequency of 32.70 — it rounds
    // the octave count so the total span stays a clean number of octaves
    // below maxFrequency, and observed behavior here lands on 8 octaves
    // (~16.67 Hz, close to C0) rather than the hand-calculated 7. Assert
    // self-consistency against the runtime-queried octave/bin count (not a
    // hand-guessed literal range) plus a generous sanity floor/ceiling.
    const int octaves = totalBins / cq.getBinsPerOctave();
    const double expectedMinFreq = 4186.01 / std::pow (2.0, (double) octaves);
    CHECK (minFreq == Catch::Approx (expectedMinFreq).epsilon (0.05));
    CHECK (minFreq > 5.0);
    CHECK (minFreq < 40.0);

    CHECK (columnHopSeconds > 0.001);
    CHECK (columnHopSeconds < 0.1);

    // Coverage: flush worked, full input duration accounted for.
    const double coveredSeconds = (double) columns.size() * columnHopSeconds;
    CHECK (coveredSeconds >= 1.9);

    // Find the bin with maximum mean magnitude across frames.
    REQUIRE (! columns.empty());
    const size_t numBins = columns.front().size();
    REQUIRE (numBins == (size_t) totalBins);

    std::vector<double> meanMagnitude (numBins, 0.0);
    for (const auto& column : columns)
        for (size_t bin = 0; bin < numBins; ++bin)
            meanMagnitude[bin] += column[bin];
    for (auto& m : meanMagnitude)
        m /= (double) columns.size();

    size_t bestBin = 0;
    double bestMagnitude = meanMagnitude[0];
    for (size_t bin = 1; bin < numBins; ++bin)
    {
        if (meanMagnitude[bin] > bestMagnitude)
        {
            bestMagnitude = meanMagnitude[bin];
            bestBin = bin;
        }
    }

    const double detectedFreq = cq.getBinFrequency ((double) bestBin);
    INFO ("Detected peak bin " << bestBin << " -> " << detectedFreq << " Hz (expected ~440 Hz)");
    CHECK (detectedFreq == Catch::Approx (440.0).epsilon (0.03));
}
