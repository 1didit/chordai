#include <JuceHeader.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Source/PluginProcessor.h"

namespace
{
    // Renders a ~3s 220 Hz sine to a temp WAV via juce::WavAudioFormat -- same
    // shape as WaveformRegionTests.cpp's local writeWavFixture helper
    // (duplicated here, that one is private to its own translation unit). A
    // tone (not silence) makes analysis take a non-trivial, observable amount
    // of time and produce a non-degenerate analyzedRegionSeconds regardless of
    // beat/chord content -- these tests exercise the background-thread
    // plumbing (auto-trigger, cancel-and-restart, no-op guard, clear-on-reload),
    // not chord-recognition accuracy (already covered by ClassicDspChordAnalyzerTests).
    juce::File writeToneWavFixture()
    {
        auto file = juce::File::createTempFile (".wav");

        constexpr double sampleRate = 44100.0;
        constexpr int numChannels = 2;
        constexpr double lengthSeconds = 3.0;
        constexpr int numSamples = (int) (lengthSeconds * sampleRate);

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sampleRate);
        }

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), sampleRate, (unsigned int) numChannels, 32, {}, 0));
        stream.release(); // writer now owns the stream

        writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        writer.reset(); // flush + close

        return file;
    }

    // Pumps the message loop (same technique as
    // WaveformRegionTests.ThumbnailPopulates) until `condition` is true or
    // `timeoutMs` elapses. Returns whether `condition` became true.
    template <typename Predicate>
    bool pumpUntil (Predicate condition, int timeoutMs)
    {
        auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (! condition() && juce::Time::getMillisecondCounter() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        return condition();
    }
}

TEST_CASE ("AnalysisPipelineTests.AutoAnalyzeOnLoad", "[analysispipeline]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    proc.loadAudioFile (file);

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000));

    REQUIRE (pumpUntil ([&] { return proc.getAnalysisResult() != nullptr && ! proc.isAnalyzing(); }, 60000));

    auto result = proc.getAnalysisResult();
    REQUIRE (result != nullptr);
    REQUIRE_FALSE (proc.isAnalyzing());

    const double lengthSeconds = proc.getLoadedAudio()->lengthSeconds;
    CHECK (result->analyzedRegionSeconds.getStart() == Catch::Approx (0.0).margin (0.01));
    CHECK (result->analyzedRegionSeconds.getEnd() == Catch::Approx (lengthSeconds).margin (0.01));

    file.deleteFile();
}

TEST_CASE ("AnalysisPipelineTests.CancelAndRestart", "[analysispipeline]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    proc.loadAudioFile (file);

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000));
    REQUIRE (pumpUntil ([&] { return ! proc.isAnalyzing() && proc.getAnalysisResult() != nullptr; }, 60000));

    const double L = proc.getLoadedAudio()->lengthSeconds;

    // Two region changes in quick succession, no pump in between: the first
    // job's completion is superseded by the second before it can (cooperative
    // cancellation, not instant) actually stop.
    proc.setSelectedRegion ({ 0.25 * L, 0.9 * L });
    proc.setSelectedRegion ({ 0.1 * L, 0.6 * L });

    REQUIRE (pumpUntil ([&] { return ! proc.isAnalyzing(); }, 60000));

    auto result = proc.getAnalysisResult();
    REQUIRE (result != nullptr);
    CHECK (result->analyzedRegionSeconds.getStart() == Catch::Approx (0.1 * L).margin (0.01));
    CHECK (result->analyzedRegionSeconds.getEnd() == Catch::Approx (0.6 * L).margin (0.01));

    file.deleteFile();
}

TEST_CASE ("AnalysisPipelineTests.NoOpRegionDoesNotRetrigger", "[analysispipeline]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    proc.loadAudioFile (file);

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000));
    REQUIRE (pumpUntil ([&] { return ! proc.isAnalyzing() && proc.getAnalysisResult() != nullptr; }, 60000));

    auto before = proc.getAnalysisResult();
    proc.setSelectedRegion (proc.getSelectedRegion()); // same clamped region -- must be a no-op

    // No deadline-condition to wait on (nothing should happen) -- pump a fixed
    // ~300ms window and confirm nothing changed.
    pumpUntil ([] { return false; }, 300);

    CHECK_FALSE (proc.isAnalyzing());
    CHECK (proc.getAnalysisResult() == before);

    file.deleteFile();
}

TEST_CASE ("AnalysisPipelineTests.ClearedOnNewLoad", "[analysispipeline]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    proc.loadAudioFile (file);

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000));
    REQUIRE (pumpUntil ([&] { return ! proc.isAnalyzing() && proc.getAnalysisResult() != nullptr; }, 60000));

    auto firstAudio = proc.getLoadedAudio();
    proc.loadAudioFile (file); // reload the same fixture -- pointer must change on completion

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != firstAudio; }, 5000));

    // The stale result must be cleared (published nullptr) before the new
    // load's analysis can complete -- caught at the instant the new
    // loadedAudio pointer becomes visible.
    CHECK (proc.getAnalysisResult() == nullptr);

    file.deleteFile();
}
