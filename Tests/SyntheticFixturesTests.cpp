#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Tests/SyntheticFixtures.h"

#include <cmath>
#include <vector>

TEST_CASE ("SyntheticFixturesTests.ClickTrackHasExpectedBursts", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr double sampleRate = 44100.0;
    constexpr double durationSeconds = 4.0;

    auto buffer = fixtures::renderClickTrack (bpm, sampleRate, durationSeconds);
    REQUIRE (buffer.getNumSamples() > 0);

    const double beatIntervalSeconds = 60.0 / bpm;
    std::vector<double> expectedBeatTimes;
    for (double t = 0.0; t < durationSeconds; t += beatIntervalSeconds)
        expectedBeatTimes.push_back (t);

    CHECK (expectedBeatTimes.size() >= 7);
    CHECK (expectedBeatTimes.size() <= 9);

    const float* data = buffer.getReadPointer (0);
    const int numSamples = buffer.getNumSamples();

    for (double expectedTime : expectedBeatTimes)
    {
        int windowStart = juce::jmax (0, (int) std::llround ((expectedTime - 0.01) * sampleRate));
        int windowEnd = juce::jmin (numSamples - 1, (int) std::llround ((expectedTime + 0.01) * sampleRate));

        int peakIndex = windowStart;
        float peakValue = 0.0f;
        for (int i = windowStart; i <= windowEnd; ++i)
        {
            float magnitude = std::abs (data[i]);
            if (magnitude > peakValue)
            {
                peakValue = magnitude;
                peakIndex = i;
            }
        }

        REQUIRE (peakValue > 0.01f);

        double peakTime = (double) peakIndex / sampleRate;
        CHECK (std::abs (peakTime - expectedTime) <= 0.005);
    }
}

TEST_CASE ("SyntheticFixturesTests.ChordRenderHasChordToneEnergy", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr double sampleRate = 44100.0;

    std::vector<fixtures::ChordSpec> chords { fixtures::ChordSpec { ChordSymbol { 0, ChordQuality::Major } } };
    auto buffer = fixtures::renderChordProgression (chords, bpm, sampleRate);
    REQUIRE (buffer.getNumSamples() > 0);

    constexpr double c4 = 261.6256;
    constexpr double e4 = 329.6276;
    constexpr double g4 = 391.9954;
    constexpr double cSharp4 = 277.1826;

    const double nonChordEnergy = juce::jmax (fixtures::toneEnergy (buffer, cSharp4, sampleRate), 1e-12);

    for (double freq : { c4, e4, g4 })
    {
        double energy = fixtures::toneEnergy (buffer, freq, sampleRate);
        INFO ("freq=" << freq << " energy=" << energy << " nonChordEnergy=" << nonChordEnergy);
        CHECK (energy >= nonChordEnergy * 10.0);
    }
}

TEST_CASE ("SyntheticFixturesTests.DetuneShiftsFrequencies", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr double sampleRate = 44100.0;

    std::vector<fixtures::ChordSpec> chords { fixtures::ChordSpec { ChordSymbol { 0, ChordQuality::Major } } };
    auto buffer = fixtures::renderChordProgression (chords, bpm, sampleRate, 4, -30.0);
    REQUIRE (buffer.getNumSamples() > 0);

    constexpr double inTuneFreq = 261.63;
    constexpr double detunedFreq = 257.1;

    double inTuneEnergy = fixtures::toneEnergy (buffer, inTuneFreq, sampleRate);
    double detunedEnergy = fixtures::toneEnergy (buffer, detunedFreq, sampleRate);

    INFO ("inTuneEnergy=" << inTuneEnergy << " detunedEnergy=" << detunedEnergy);
    CHECK (detunedEnergy > inTuneEnergy);
}
