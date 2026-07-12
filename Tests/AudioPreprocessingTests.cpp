#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Source/Analysis/AudioPreprocessing.h"

#include <cmath>
#include <numbers>
#include <vector>

namespace
{
    double rms (const std::vector<float>& samples)
    {
        if (samples.empty())
            return 0.0;

        double sumSq = 0.0;
        for (float s : samples)
            sumSq += (double) s * (double) s;
        return std::sqrt (sumSq / (double) samples.size());
    }
}

TEST_CASE ("AudioPreprocessingTests.DownmixAndDualRate", "[chordanalysis]")
{
    constexpr double sourceRate = 44100.0;
    constexpr double durationSeconds = 2.0;
    constexpr double freq = 440.0;
    const int numSamples = (int) std::llround (durationSeconds * sourceRate);

    juce::AudioBuffer<float> buffer (2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = 0.5f * (float) std::sin (2.0 * std::numbers::pi * freq * (double) i / sourceRate);
    }

    auto result = preprocessForAnalysis (buffer, sourceRate, {});

    CHECK ((double) result.onsetSamples.size() == Catch::Approx (16000.0).epsilon (0.01));
    CHECK ((double) result.chromaSamples.size() == Catch::Approx (22050.0).epsilon (0.01));

    const double sourceRms = 0.5 / std::sqrt (2.0);
    const double onsetRms = rms (result.onsetSamples);
    const double chromaRms = rms (result.chromaSamples);

    INFO ("onsetRms=" << onsetRms << " chromaRms=" << chromaRms << " sourceRms=" << sourceRms);
    CHECK (onsetRms == Catch::Approx (sourceRms).epsilon (0.25));
    CHECK (chromaRms == Catch::Approx (sourceRms).epsilon (0.25));
}

TEST_CASE ("AudioPreprocessingTests.RegionExtraction", "[chordanalysis]")
{
    constexpr double sourceRate = 44100.0;
    constexpr double totalDurationSeconds = 3.0;
    constexpr double freq = 440.0;
    const int totalSamples = (int) std::llround (totalDurationSeconds * sourceRate);

    juce::AudioBuffer<float> buffer (1, totalSamples);
    buffer.clear();

    const int regionStartSample = (int) std::llround (1.0 * sourceRate);
    const int regionEndSample = (int) std::llround (2.0 * sourceRate);
    auto* data = buffer.getWritePointer (0);
    for (int i = regionStartSample; i < regionEndSample; ++i)
        data[i] = 0.5f * (float) std::sin (2.0 * std::numbers::pi * freq * (double) (i - regionStartSample) / sourceRate);

    double sumSq = 0.0;
    int count = 0;
    for (int i = regionStartSample; i < regionEndSample; ++i)
    {
        sumSq += (double) data[i] * (double) data[i];
        ++count;
    }
    const double fullSineRms = count > 0 ? std::sqrt (sumSq / (double) count) : 0.0;

    SECTION ("Region {1.0, 2.0} extracts just the sine")
    {
        auto result = preprocessForAnalysis (buffer, sourceRate, { 1.0, 2.0 });
        CHECK (result.regionStartSeconds == Catch::Approx (1.0));

        const double onsetRms = rms (result.onsetSamples);
        const double chromaRms = rms (result.chromaSamples);

        INFO ("onsetRms=" << onsetRms << " chromaRms=" << chromaRms << " fullSineRms=" << fullSineRms);
        CHECK (onsetRms == Catch::Approx (fullSineRms).epsilon (0.25));
        CHECK (chromaRms == Catch::Approx (fullSineRms).epsilon (0.25));
    }

    SECTION ("Empty region processes the whole buffer")
    {
        auto result = preprocessForAnalysis (buffer, sourceRate, {});
        CHECK (result.regionStartSeconds == Catch::Approx (0.0));
        CHECK ((double) result.onsetSamples.size() == Catch::Approx (totalDurationSeconds * result.onsetRate).epsilon (0.01));
    }
}
