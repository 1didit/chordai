#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "cq/CQParameters.h"
#include "cq/CQSpectrogram.h"

#include "Source/Analysis/ConstantQAnalysis.h"
#include "Source/Analysis/ChromaExtractor.h"
#include "Source/Analysis/HarmonicPercussiveFilter.h"
#include "Source/Analysis/TuningEstimator.h"
#include "Source/Analysis/AudioPreprocessing.h"
#include "Tests/SyntheticFixtures.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>

namespace
{
    constexpr double kSampleRate = 11025.0;
    constexpr double kSourceRate = 44100.0;
    constexpr double kBpm = 90.0;

    // C major triad (root pitch class 0), one 8-beat chord so there's enough
    // duration for stable mean chroma / median-filter statistics.
    std::vector<fixtures::ChordSpec> cMajorTriadFixture (int bassPitchClass = -1)
    {
        return { { { 0, ChordQuality::Major }, bassPitchClass } };
    }

    ChromaSequence extractChromaFromBuffer (const juce::AudioBuffer<float>& buffer, double tuningCents)
    {
        auto preprocessed = preprocessForAnalysis (buffer, kSourceRate, {});
        auto cqt = computeCqt (preprocessed.chromaSamples, preprocessed.chromaRate);
        suppressPercussion (cqt, kHpssKernelSeconds);
        return extractChroma (cqt, tuningCents);
    }

    std::array<float, 12> meanHarmonicChroma (const ChromaSequence& chroma)
    {
        std::array<float, 12> mean {};
        if (chroma.frames.empty())
            return mean;

        for (const auto& frame : chroma.frames)
            for (int pc = 0; pc < 12; ++pc)
                mean[(size_t) pc] += frame.harmonic[(size_t) pc];
        for (auto& v : mean)
            v /= (float) chroma.frames.size();
        return mean;
    }

    std::array<float, 12> meanBassChroma (const ChromaSequence& chroma)
    {
        std::array<float, 12> mean {};
        if (chroma.frames.empty())
            return mean;

        for (const auto& frame : chroma.frames)
            for (int pc = 0; pc < 12; ++pc)
                mean[(size_t) pc] += frame.bass[(size_t) pc];
        for (auto& v : mean)
            v /= (float) chroma.frames.size();
        return mean;
    }

    // Indices of the n largest entries in a 12-bin chroma vector.
    std::set<int> topNPitchClasses (const std::array<float, 12>& chroma, int n)
    {
        std::array<int, 12> indices { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        std::sort (indices.begin(), indices.end(), [&] (int a, int b) { return chroma[(size_t) a] > chroma[(size_t) b]; });
        std::set<int> result;
        for (int i = 0; i < n; ++i)
            result.insert (indices[(size_t) i]);
        return result;
    }

    std::set<int> top3PitchClasses (const std::array<float, 12>& chroma) { return topNPitchClasses (chroma, 3); }
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

// computeCqt: the ConstantQAnalysis.h/.cpp wrapper around the raw API above.

TEST_CASE ("ChromaExtractorTests.CqtWrapperCoversFullDuration", "[chordanalysis]")
{
    constexpr double durationSeconds = 3.0;
    const int numSamples = (int) std::lround (durationSeconds * kSampleRate);
    std::vector<float> sine ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        sine[(size_t) i] = (float) std::sin (2.0 * std::numbers::pi * 220.0 * (double) i / kSampleRate);

    auto cqt = computeCqt (sine, kSampleRate);

    REQUIRE (! cqt.columns.empty());
    const double covered = (double) cqt.columns.size() * cqt.columnHopSeconds;
    CHECK (covered >= 2.9);

    const size_t expectedBins = cqt.binFrequenciesHz.size();
    REQUIRE (expectedBins > 0);
    for (const auto& column : cqt.columns)
        CHECK (column.size() == expectedBins);
}

TEST_CASE ("ChromaExtractorTests.CqtWrapperSinePeak", "[chordanalysis]")
{
    constexpr double durationSeconds = 2.0;
    const int numSamples = (int) std::lround (durationSeconds * kSampleRate);
    std::vector<float> sine ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        sine[(size_t) i] = (float) std::sin (2.0 * std::numbers::pi * 440.0 * (double) i / kSampleRate);

    auto cqt = computeCqt (sine, kSampleRate);

    REQUIRE (! cqt.columns.empty());
    const size_t numBins = cqt.binFrequenciesHz.size();
    REQUIRE (numBins > 0);

    std::vector<double> meanMagnitude (numBins, 0.0);
    for (const auto& column : cqt.columns)
        for (size_t bin = 0; bin < numBins; ++bin)
            meanMagnitude[bin] += column[bin];
    for (auto& m : meanMagnitude)
        m /= (double) cqt.columns.size();

    size_t bestBin = 0;
    for (size_t bin = 1; bin < numBins; ++bin)
        if (meanMagnitude[bin] > meanMagnitude[bestBin])
            bestBin = bin;

    const double detectedFreq = cqt.binFrequenciesHz[bestBin];
    INFO ("Detected peak bin " << bestBin << " -> " << detectedFreq << " Hz (expected ~440 Hz)");
    CHECK (detectedFreq == Catch::Approx (440.0).epsilon (0.03));
}

TEST_CASE ("ChromaExtractorTests.CqtWrapperEmptyInput", "[chordanalysis]")
{
    std::vector<float> empty;
    auto cqt = computeCqt (empty, kSampleRate);
    CHECK (cqt.columns.empty());
}

// HarmonicPercussiveFilter + ChromaExtractor dual fold.

TEST_CASE ("ChromaExtractorTests.TriadFoldTopPitchClasses", "[chordanalysis]")
{
    auto buffer = fixtures::renderChordProgression (cMajorTriadFixture(), kBpm, kSourceRate, 8);
    auto chroma = extractChromaFromBuffer (buffer, 0.0);

    REQUIRE (! chroma.frames.empty());
    auto mean = meanHarmonicChroma (chroma);
    auto top3 = top3PitchClasses (mean);

    INFO ("top3 = {" << *top3.begin() << ", ...}");
    CHECK (top3 == std::set<int> { 0, 4, 7 });
}

TEST_CASE ("ChromaExtractorTests.PercussionRobustness", "[chordanalysis]")
{
    auto buffer = fixtures::renderChordProgression (cMajorTriadFixture(), kBpm, kSourceRate, 8);
    fixtures::addPercussiveBursts (buffer, kBpm, kSourceRate);

    auto chroma = extractChromaFromBuffer (buffer, 0.0);

    REQUIRE (! chroma.frames.empty());
    auto mean = meanHarmonicChroma (chroma);
    auto top3 = top3PitchClasses (mean);

    CHECK (top3 == std::set<int> { 0, 4, 7 });
}

TEST_CASE ("ChromaExtractorTests.DetunedFixtureFoldsCorrectly", "[chordanalysis]")
{
    auto buffer = fixtures::renderChordProgression (cMajorTriadFixture(), kBpm, kSourceRate, 8, -30.0);

    auto preprocessed = preprocessForAnalysis (buffer, kSourceRate, {});
    auto cqt = computeCqt (preprocessed.chromaSamples, preprocessed.chromaRate);
    suppressPercussion (cqt, kHpssKernelSeconds);
    const double tuningCents = estimateTuningCents (cqt);
    auto chroma = extractChroma (cqt, tuningCents);

    REQUIRE (! chroma.frames.empty());
    auto mean = meanHarmonicChroma (chroma);
    auto top3 = top3PitchClasses (mean);

    CHECK (top3 == std::set<int> { 0, 4, 7 });
}

TEST_CASE ("ChromaExtractorTests.BassChromaIsolatesBassNote", "[chordanalysis]")
{
    auto buffer = fixtures::renderChordProgression (cMajorTriadFixture (9), kBpm, kSourceRate, 8);
    auto chroma = extractChromaFromBuffer (buffer, 0.0);

    REQUIRE (! chroma.frames.empty());
    auto meanBass = meanBassChroma (chroma);
    auto meanHarmonic = meanHarmonicChroma (chroma);

    int bassArgmax = 0;
    for (int pc = 1; pc < 12; ++pc)
        if (meanBass[(size_t) pc] > meanBass[(size_t) bassArgmax])
            bassArgmax = pc;

    CHECK (bassArgmax == 9);

    // The harmonic fold intentionally includes the 55-250 Hz band too (research
    // Pattern 3's overlapping ranges -- kHarmonicMinHz=80 only excludes sub-bass
    // rumble, not a genuine bass note); a loud, distinct bass note is therefore
    // expected to compete for a place in harmonic chroma alongside the true
    // chord tones, not be invisible to it. Root-bias disambiguation from this
    // exact ambiguity is what the bass chroma + chord-decoder scoring (03-05)
    // is for. So the chord tones must all still rank in the top 4 (not a strict
    // top 3, which would require the harmonic fold to exclude the bass range
    // entirely, defeating Pattern 3's rationale).
    auto harmonicTop4 = topNPitchClasses (meanHarmonic, 4);
    CHECK (harmonicTop4.count (0) == 1);
    CHECK (harmonicTop4.count (4) == 1);
    CHECK (harmonicTop4.count (7) == 1);
}

TEST_CASE ("ChromaExtractorTests.SilenceFramesFlagged", "[chordanalysis]")
{
    auto chordBuffer = fixtures::renderChordProgression (cMajorTriadFixture(), kBpm, kSourceRate, 8);

    const int silenceSamples = (int) std::llround (1.0 * kSourceRate);
    juce::AudioBuffer<float> buffer (1, chordBuffer.getNumSamples() + silenceSamples);
    buffer.clear();
    buffer.copyFrom (0, 0, chordBuffer, 0, 0, chordBuffer.getNumSamples());

    auto chroma = extractChromaFromBuffer (buffer, 0.0);
    REQUIRE (! chroma.frames.empty());

    // Split frames into "audible" (within the chord's own duration) and
    // "trailing" (within the appended silence) by timeSeconds.
    const double chordDurationSeconds = (double) chordBuffer.getNumSamples() / kSourceRate;

    float maxAudibleL2 = 0.0f;
    float maxTrailingL2 = 0.0f;
    bool anyTrailing = false;

    // The CQT's own low-frequency bins have wide time-domain windows, so
    // magnitude decays gradually (measured ~0.5-0.6s tail, not an instant drop)
    // after the audio itself stops -- classify "trailing" only well past that
    // settling time, not immediately at the nominal chord boundary.
    constexpr double kDecaySettlingMargin = 0.7;

    for (const auto& frame : chroma.frames)
    {
        if (frame.timeSeconds < chordDurationSeconds - 0.05)
            maxAudibleL2 = std::max (maxAudibleL2, frame.harmonicPreNormL2);
        else if (frame.timeSeconds > chordDurationSeconds + kDecaySettlingMargin)
        {
            anyTrailing = true;
            maxTrailingL2 = std::max (maxTrailingL2, frame.harmonicPreNormL2);

            for (float v : frame.harmonic)
            {
                CHECK (! std::isnan (v));
                CHECK (! std::isinf (v));
                CHECK (v == 0.0f);
            }
        }
    }

    REQUIRE (anyTrailing);
    CHECK (maxTrailingL2 < maxAudibleL2);
}
