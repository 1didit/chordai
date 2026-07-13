#include <catch2/catch_test_macros.hpp>

#include "Source/Audio/AuditionRenderer.h"
#include "Source/PluginProcessor.h"

#include <cmath>

// Test-construction boilerplate mirrors Tests/PluginProcessorMidiGenTests.cpp
// (juce::ScopedJuceInitialiser_GUI + direct ChordAIAudioProcessor construction).
namespace
{
    MidiSetRow makeRow (const juce::String& id, double startBeats, double lengthBeats, int pitch, float velocity)
    {
        MidiSetRow row;
        row.id = id;
        row.notes = { { startBeats, lengthBeats, pitch, velocity } };
        return row;
    }

    // At default-bpm fallback (120, no analysisResult set), 1.0 beat == 0.5s.
    bool driveBlockHasNonZeroSamples (ChordAIAudioProcessor& proc, int numSamples)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();
        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                if (data[i] != 0.0f)
                    return true;
        }
        return false;
    }

    bool isBufferAllZero (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (data[i] != 0.0f)
                    return false;
        }
        return true;
    }
}

TEST_CASE ("ProcessorAuditionTests.PlaybackMixesAuditionAudioIntoProcessBlock", "[processoraudition]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;
    // Real hosts (VST3/AU/Standalone wrapper code) always call
    // setRateAndBufferSizeDetails() before prepareToPlay() -- getSampleRate()
    // is only valid inside/after prepareToPlay because of that host-side
    // contract, so tests must replicate it rather than calling prepareToPlay
    // bare.
    proc.setRateAndBufferSizeDetails (44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    const auto row = makeRow ("as-is", 0.0, 1.0, 60, 0.8f);
    proc.startAudition (row);

    CHECK (proc.isAuditionPlaying());
    CHECK (proc.getAuditionRowId() == row.id);

    bool foundNonZeroBlock = false;
    for (int block = 0; block < 5 && ! foundNonZeroBlock; ++block)
        if (driveBlockHasNonZeroSamples (proc, 512))
            foundNonZeroBlock = true;

    CHECK (foundNonZeroBlock);
}

TEST_CASE ("ProcessorAuditionTests.AutoStopsAtBufferEnd", "[processoraudition]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;
    // Real hosts (VST3/AU/Standalone wrapper code) always call
    // setRateAndBufferSizeDetails() before prepareToPlay() -- getSampleRate()
    // is only valid inside/after prepareToPlay because of that host-side
    // contract, so tests must replicate it rather than calling prepareToPlay
    // bare.
    proc.setRateAndBufferSizeDetails (44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    // 1 beat @ default-bpm fallback 120 = 0.5s; no analysisResult set.
    const auto row = makeRow ("as-is", 0.0, 1.0, 60, 0.8f);
    proc.startAudition (row);
    REQUIRE (proc.isAuditionPlaying());

    const int expectedTotalSamples =
        (int) std::ceil ((0.5 + AuditionRenderer::kReleaseTailSeconds) * 44100.0);
    const int blocksToDrive = expectedTotalSamples / 512 + 2;

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocksToDrive; ++block)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    CHECK_FALSE (proc.isAuditionPlaying());
}

TEST_CASE ("ProcessorAuditionTests.StopAuditionSilencesImmediately", "[processoraudition]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;
    // Real hosts (VST3/AU/Standalone wrapper code) always call
    // setRateAndBufferSizeDetails() before prepareToPlay() -- getSampleRate()
    // is only valid inside/after prepareToPlay because of that host-side
    // contract, so tests must replicate it rather than calling prepareToPlay
    // bare.
    proc.setRateAndBufferSizeDetails (44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    const auto row = makeRow ("as-is", 0.0, 2.0, 60, 0.8f);
    proc.startAudition (row);
    REQUIRE (proc.isAuditionPlaying());

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 3; ++block)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    proc.stopAudition();
    CHECK_FALSE (proc.isAuditionPlaying());

    buffer.clear();
    proc.processBlock (buffer, midi);
    CHECK (isBufferAllZero (buffer));
}

TEST_CASE ("ProcessorAuditionTests.RestartWhilePlayingSwapsToNewRow", "[processoraudition]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;
    // Real hosts (VST3/AU/Standalone wrapper code) always call
    // setRateAndBufferSizeDetails() before prepareToPlay() -- getSampleRate()
    // is only valid inside/after prepareToPlay because of that host-side
    // contract, so tests must replicate it rather than calling prepareToPlay
    // bare.
    proc.setRateAndBufferSizeDetails (44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    const auto rowA = makeRow ("as-is", 0.0, 2.0, 60, 0.8f);
    const auto rowB = makeRow ("pop-trap", 0.0, 1.0, 67, 0.8f);

    proc.startAudition (rowA);
    REQUIRE (proc.isAuditionPlaying());

    proc.startAudition (rowB);

    CHECK (proc.isAuditionPlaying());
    CHECK (proc.getAuditionRowId() == rowB.id);

    // Playback completes without crashing (double-buffer swap correctness).
    const int expectedTotalSamples =
        (int) std::ceil ((0.5 + AuditionRenderer::kReleaseTailSeconds) * 44100.0);
    const int blocksToDrive = expectedTotalSamples / 512 + 2;

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocksToDrive; ++block)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    CHECK_FALSE (proc.isAuditionPlaying());
}

TEST_CASE ("ProcessorAuditionTests.PrepareToPlayStopsStaleAudition", "[processoraudition]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;
    // Real hosts (VST3/AU/Standalone wrapper code) always call
    // setRateAndBufferSizeDetails() before prepareToPlay() -- getSampleRate()
    // is only valid inside/after prepareToPlay because of that host-side
    // contract, so tests must replicate it rather than calling prepareToPlay
    // bare.
    proc.setRateAndBufferSizeDetails (44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    const auto row = makeRow ("as-is", 0.0, 2.0, 60, 0.8f);
    proc.startAudition (row);
    REQUIRE (proc.isAuditionPlaying());

    proc.setRateAndBufferSizeDetails (48000.0, 256);
    proc.prepareToPlay (48000.0, 256);

    CHECK_FALSE (proc.isAuditionPlaying());
}
