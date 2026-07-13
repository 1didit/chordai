#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/GenreRegistry.h"
#include "Source/MidiGen/GenreState.h"
#include "Source/MidiGen/MidiRowBuilder.h"
#include "Source/PluginProcessor.h"

// writeToneWavFixture()/pumpUntil() copied verbatim (file-local helpers, by
// design not extracted -- see Tests/AnalysisPipelineTests.cpp's own
// top-of-file comment) from Tests/PluginProcessorMidiGenTests.cpp.
namespace
{
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

    template <typename Predicate>
    bool pumpUntil (Predicate condition, int timeoutMs)
    {
        auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (! condition() && juce::Time::getMillisecondCounter() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        return condition();
    }

    bool notesEqual (const NoteEvent& a, const NoteEvent& b)
    {
        return a.startBeats == b.startBeats
            && a.lengthBeats == b.lengthBeats
            && a.pitch == b.pitch
            && a.velocity == b.velocity;
    }

    bool noteVectorsEqual (const std::vector<NoteEvent>& a, const std::vector<NoteEvent>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (! notesEqual (a[i], b[i]))
                return false;

        return true;
    }

    bool rowsDeepEqual (const std::vector<MidiSetRow>& a, const std::vector<MidiSetRow>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].id != b[i].id || a[i].label != b[i].label || a[i].kind != b[i].kind)
                return false;

            if (! noteVectorsEqual (a[i].notes, b[i].notes))
                return false;
        }

        return true;
    }

    // Minimal broadcast-observed flag, reused by several cases below.
    struct FlagListener : juce::ChangeListener
    {
        bool fired = false;
        void changeListenerCallback (juce::ChangeBroadcaster*) override { fired = true; }
    };

    // Shared load+analyze-to-completion helper -- every case below needs this
    // exact sequence before exercising the genre API.
    bool loadAndWaitForAnalysis (ChordAIAudioProcessor& proc, const juce::File& file)
    {
        proc.loadAudioFile (file);

        if (! pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000))
            return false;

        return pumpUntil ([&] { return proc.getAnalysisResult() != nullptr && ! proc.isAnalyzing(); }, 60000);
    }
}

TEST_CASE ("PluginProcessorGenreTests.GenreSwitchRegeneratesDistinctRows", "[processorgenre]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    REQUIRE (loadAndWaitForAnalysis (proc, file));

    CHECK (proc.getActiveGenreId() == "trap");
    auto trapRows = proc.getMidiSetRows();
    auto resultBeforeSwitch = proc.getAnalysisResult();
    REQUIRE (trapRows != nullptr);
    REQUIRE (trapRows->size() == 5);

    FlagListener listener;
    proc.analysisBroadcaster.addChangeListener (&listener);

    proc.setActiveGenre ("house");

    // sendChangeMessage() is delivered asynchronously on the message thread
    // (same discipline as every other pumpUntil-gated assertion in this
    // file) -- a bare boolean check immediately after the call would race it.
    REQUIRE (pumpUntil ([&] { return listener.fired; }, 5000));
    proc.analysisBroadcaster.removeChangeListener (&listener);

    CHECK (proc.getActiveGenreId() == "house");
    auto houseRows = proc.getMidiSetRows();
    REQUIRE (houseRows != nullptr);
    REQUIRE (houseRows->size() == 5);

    for (const auto& row : *houseRows)
        CHECK (row.id.startsWith ("house-"));

    CHECK_FALSE (rowsDeepEqual (*trapRows, *houseRows));

    // No re-analysis: the AnalysisResult pointer is byte-identical.
    CHECK (proc.getAnalysisResult() == resultBeforeSwitch);

    file.deleteFile();
}

TEST_CASE ("PluginProcessorGenreTests.RegenerateChangesOnlyTargetRow", "[processorgenre]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    REQUIRE (loadAndWaitForAnalysis (proc, file));

    auto before = proc.getMidiSetRows();
    REQUIRE (before != nullptr);
    REQUIRE (before->size() == 5);

    proc.regenerateRow (2);

    auto after = proc.getMidiSetRows();
    REQUIRE (after != nullptr);
    REQUIRE (after->size() == 5);

    for (size_t i = 0; i < 5; ++i)
    {
        if (i == 2)
            CHECK_FALSE (noteVectorsEqual ((*before)[i].notes, (*after)[i].notes));
        else
            CHECK (noteVectorsEqual ((*before)[i].notes, (*after)[i].notes));
    }

    file.deleteFile();
}

TEST_CASE ("PluginProcessorGenreTests.RegenerateIsDeterministicGivenSameCounter", "[processorgenre]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    ChordAIAudioProcessor procA;
    ChordAIAudioProcessor procB;

    auto fileA = writeToneWavFixture();
    auto fileB = writeToneWavFixture(); // same deterministic sine-wave content

    REQUIRE (loadAndWaitForAnalysis (procA, fileA));
    REQUIRE (loadAndWaitForAnalysis (procB, fileB));

    // Same call sequence on both processors (extends Phase 5's two-processor
    // determinism shape to regenerateRow).
    procA.regenerateRow (0); procA.regenerateRow (0); procA.regenerateRow (0); procA.regenerateRow (2);
    procB.regenerateRow (0); procB.regenerateRow (0); procB.regenerateRow (0); procB.regenerateRow (2);

    auto rowsA = procA.getMidiSetRows();
    auto rowsB = procB.getMidiSetRows();
    REQUIRE (rowsA != nullptr);
    REQUIRE (rowsB != nullptr);

    CHECK (rowsDeepEqual (*rowsA, *rowsB));

    fileA.deleteFile();
    fileB.deleteFile();
}

TEST_CASE ("PluginProcessorGenreTests.RegenerateOutOfRangeIndexIsNoOp", "[processorgenre]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    REQUIRE (loadAndWaitForAnalysis (proc, file));

    auto before = proc.getMidiSetRows();
    REQUIRE (before != nullptr);

    FlagListener listener;
    proc.analysisBroadcaster.addChangeListener (&listener);

    proc.regenerateRow (-1);
    proc.regenerateRow (5);

    proc.analysisBroadcaster.removeChangeListener (&listener);
    CHECK_FALSE (listener.fired);

    auto after = proc.getMidiSetRows();
    CHECK (after == before); // same shared_ptr instance -- no publish happened

    file.deleteFile();
}

TEST_CASE ("PluginProcessorGenreTests.RegenerateBeforeAnalysisIsNoOp", "[processorgenre]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc; // fresh, nothing loaded/analyzed

    proc.regenerateRow (0); // must not crash

    CHECK (proc.getMidiSetRows() == nullptr);
}

TEST_CASE ("PluginProcessorGenreTests.SetMainGenresValidatesAndPersists", "[processorgenre]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    const auto originalIds = proc.getMainGenreIds();

    juce::StringArray fourIds { "trap", "house", "pop", "lofi" };
    proc.setMainGenres (fourIds);
    CHECK (proc.getMainGenreIds() == originalIds); // ignored -- wrong size

    juce::StringArray fiveIds { "pop", "lofi", "afrobeats", "reggaeton", "techno" };
    proc.setMainGenres (fiveIds);
    CHECK (proc.getMainGenreIds() == fiveIds);
    CHECK (proc.apvts.state.getProperty (GenreState::mainGenreIds).toString() == fiveIds.joinIntoString (","));

    // getStateInformation/setStateInformation round-trip onto a second
    // processor restores both genre props (RegionState precedent).
    juce::MemoryBlock block;
    proc.getStateInformation (block);

    ChordAIAudioProcessor proc2;
    proc2.setStateInformation (block.getData(), (int) block.getSize());

    CHECK (proc2.getMainGenreIds() == fiveIds);
    CHECK (proc2.getActiveGenreId() == proc.getActiveGenreId());
}

TEST_CASE ("PluginProcessorGenreTests.UnknownActiveGenreIdFallsBackSafely", "[processorgenre]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    // Build a corrupt state blob on a throwaway processor, then load it onto
    // a fresh one BEFORE any audio load -- setStateInformation's own
    // GenreState re-read (06.1-05) is what actually applies the corruption
    // to the message-thread cache; hand-mutating apvts.state directly on the
    // processor under test would never reach that cache.
    juce::MemoryBlock corruptBlock;
    {
        ChordAIAudioProcessor seed;
        seed.apvts.state.setProperty (GenreState::activeGenreId, "future-genre", nullptr);
        seed.getStateInformation (corruptBlock);
    }

    ChordAIAudioProcessor proc;
    proc.setStateInformation (corruptBlock.getData(), (int) corruptBlock.getSize());
    REQUIRE (proc.getActiveGenreId() == "future-genre"); // corruption landed in the cache

    auto file = writeToneWavFixture();
    REQUIRE (loadAndWaitForAnalysis (proc, file));

    auto rows = proc.getMidiSetRows();
    REQUIRE (rows != nullptr);
    REQUIRE (rows->size() == 5);

    for (const auto& row : *rows)
        CHECK (row.id.startsWith ("trap-")); // findGenre fallback landed on trap, no crash

    file.deleteFile();
}
