#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/MidiRowBuilder.h"
#include "Source/PluginProcessor.h"

// writeToneWavFixture()/pumpUntil() below are copied verbatim (file-local
// helpers, by design not extracted -- see Tests/AnalysisPipelineTests.cpp's
// own top-of-file comment) from Tests/AnalysisPipelineTests.cpp.
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

    bool rowsDeepEqual (const std::vector<MidiSetRow>& a, const std::vector<MidiSetRow>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].id != b[i].id || a[i].label != b[i].label || a[i].style != b[i].style)
                return false;

            if (a[i].notes.size() != b[i].notes.size())
                return false;

            for (size_t n = 0; n < a[i].notes.size(); ++n)
                if (! notesEqual (a[i].notes[n], b[i].notes[n]))
                    return false;
        }

        return true;
    }
}

TEST_CASE ("PluginProcessorMidiGenTests.RowsPublishedSynchronouslyWithAnalysisResult", "[processormidigen]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    struct Snapshot
    {
        bool isAnalyzing = false;
        bool resultNonNull = false;
        bool rowsNonNull = false;
        size_t rowCount = 0;
    };

    std::vector<Snapshot> snapshots;

    struct RecordingListener : juce::ChangeListener
    {
        ChordAIAudioProcessor& proc;
        std::vector<Snapshot>& snapshots;

        RecordingListener (ChordAIAudioProcessor& p, std::vector<Snapshot>& s) : proc (p), snapshots (s) {}

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            auto rows = proc.getMidiSetRows();
            snapshots.push_back ({ proc.isAnalyzing(),
                                    proc.getAnalysisResult() != nullptr,
                                    rows != nullptr,
                                    rows != nullptr ? rows->size() : 0 });
        }
    } listener (proc, snapshots);

    proc.analysisBroadcaster.addChangeListener (&listener);

    auto file = writeToneWavFixture();
    proc.loadAudioFile (file);

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000));
    REQUIRE (pumpUntil ([&] { return proc.getAnalysisResult() != nullptr && ! proc.isAnalyzing(); }, 60000));

    proc.analysisBroadcaster.removeChangeListener (&listener);

    // Find the FIRST callback where analysis was complete with a non-null
    // result -- rows must ALREADY be non-null with size 5 in that SAME
    // message (never staggered across two separate broadcasts).
    const Snapshot* firstComplete = nullptr;
    for (const auto& s : snapshots)
    {
        if (! s.isAnalyzing && s.resultNonNull)
        {
            firstComplete = &s;
            break;
        }
    }

    REQUIRE (firstComplete != nullptr);
    CHECK (firstComplete->rowsNonNull);
    CHECK (firstComplete->rowCount == 5);

    auto rows = proc.getMidiSetRows();
    auto result = proc.getAnalysisResult();
    REQUIRE (rows != nullptr);
    REQUIRE (result != nullptr);

    const auto expected = generateAllRows (*result);
    CHECK (rowsDeepEqual (*rows, expected));

    file.deleteFile();
}

TEST_CASE ("PluginProcessorMidiGenTests.RowsRegenerateOnRegionChange", "[processormidigen]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    proc.loadAudioFile (file);

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000));
    REQUIRE (pumpUntil ([&] { return proc.getAnalysisResult() != nullptr && ! proc.isAnalyzing(); }, 60000));

    auto rows0 = proc.getMidiSetRows();
    REQUIRE (rows0 != nullptr);

    const double lengthSeconds = proc.getLoadedAudio()->lengthSeconds;
    proc.setSelectedRegion ({ 0.0, lengthSeconds / 2.0 });

    REQUIRE (pumpUntil ([&] { return ! proc.isAnalyzing() && proc.getMidiSetRows() != rows0; }, 60000));

    auto rows1 = proc.getMidiSetRows();
    auto newResult = proc.getAnalysisResult();
    REQUIRE (rows1 != nullptr);
    REQUIRE (newResult != nullptr);

    CHECK (rows1 != rows0);
    CHECK (rows1->size() == 5);

    const auto expected = generateAllRows (*newResult);
    CHECK (rowsDeepEqual (*rows1, expected));

    file.deleteFile();
}

TEST_CASE ("PluginProcessorMidiGenTests.FreshLoadClearsRows", "[processormidigen]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    ChordAIAudioProcessor proc;

    auto file = writeToneWavFixture();
    proc.loadAudioFile (file);

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != nullptr; }, 5000));
    REQUIRE (pumpUntil ([&] { return proc.getAnalysisResult() != nullptr && ! proc.isAnalyzing(); }, 60000));

    REQUIRE (proc.getMidiSetRows() != nullptr);

    auto firstAudio = proc.getLoadedAudio();
    auto file2 = writeToneWavFixture();
    proc.loadAudioFile (file2); // fresh load -- rows must clear before the new analysis completes

    REQUIRE (pumpUntil ([&] { return proc.getLoadedAudio() != firstAudio; }, 5000));

    CHECK (proc.getMidiSetRows() == nullptr);

    file.deleteFile();
    file2.deleteFile();
}
