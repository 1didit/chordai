#include <JuceHeader.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Source/UI/WaveformMath.h"
#include "Source/UI/RegionSelectionModel.h"

namespace
{
    // Writes a 1 s, 440 Hz sine, 44.1 kHz stereo WAV fixture — same shape as
    // AudioFileLoaderTests' helper, duplicated locally (that helper lives in
    // an anonymous namespace private to its own translation unit).
    juce::File writeWavFixture()
    {
        auto file = juce::File::createTempFile (".wav");

        constexpr double sampleRate = 44100.0;
        constexpr int numChannels = 2;
        constexpr int numSamples = 44100; // 1 s

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) i / sampleRate);
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
}

TEST_CASE ("WaveformRegionTests.PixelTimeConversion", "[waveformregion]")
{
    juce::Range<double> visibleRange (0.0, 10.0);
    constexpr int width = 800;

    CHECK (timeToX (5.0, visibleRange, width) == Catch::Approx (400.0f));
    CHECK (xToTime (400.0f, visibleRange, width) == Catch::Approx (5.0));

    for (double t : { 0.0, 3.3, 10.0 })
    {
        auto x = timeToX (t, visibleRange, width);
        auto roundTripped = xToTime (x, visibleRange, width);
        CHECK (roundTripped == Catch::Approx (t).margin (1e-9));
    }

    // Degenerate (zero-length) visible range must not divide by zero.
    juce::Range<double> degenerate (5.0, 5.0);
    CHECK (timeToX (5.0, degenerate, width) == 0.0f);
}

TEST_CASE ("WaveformRegionTests.DefaultWholeFile", "[waveformregion]")
{
    RegionSelectionModel model;
    model.setTotalLength (30.0);

    auto region = model.getRegion();
    CHECK (region.getStart() == 0.0);
    CHECK (region.getEnd() == 30.0);
    CHECK (model.isWholeFile());
}

TEST_CASE ("WaveformRegionTests.DragSelectionClamped", "[waveformregion]")
{
    RegionSelectionModel model;
    model.setTotalLength (10.0);

    model.beginDrag (2.0);
    model.dragTo (8.0);
    model.endDrag();
    {
        auto region = model.getRegion();
        CHECK (region.getStart() == 2.0);
        CHECK (region.getEnd() == 8.0);
    }

    model.beginDrag (5.0);
    model.dragTo (-3.0);
    {
        // Right-to-left drag normalizes and clamps to [0, totalLength].
        auto region = model.getRegion();
        CHECK (region.getStart() == 0.0);
        CHECK (region.getEnd() == 5.0);
    }

    model.beginDrag (9.0);
    model.dragTo (25.0);
    {
        auto region = model.getRegion();
        CHECK (region.getStart() == 9.0);
        CHECK (region.getEnd() == 10.0);
    }
}

TEST_CASE ("WaveformRegionTests.ClickResetsToWholeFile", "[waveformregion]")
{
    RegionSelectionModel model;
    model.setTotalLength (10.0);

    model.beginDrag (2.0);
    model.dragTo (8.0);
    model.endDrag();
    REQUIRE (model.getRegion().getStart() == 2.0);
    REQUIRE (model.getRegion().getEnd() == 8.0);

    // A click / micro-drag (selection length < 0.05s) resets to whole file.
    model.beginDrag (4.0);
    model.dragTo (4.01);
    model.endDrag();

    auto region = model.getRegion();
    CHECK (region.getStart() == 0.0);
    CHECK (region.getEnd() == 10.0);
    CHECK (model.isWholeFile());
}

TEST_CASE ("WaveformRegionTests.ThumbnailPopulates", "[waveformregion]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::AudioThumbnailCache cache (5);
    juce::AudioThumbnail thumbnail (512, formatManager, cache);

    auto file = writeWavFixture();
    thumbnail.setSource (new juce::FileInputSource (file));

    auto deadline = juce::Time::getMillisecondCounter() + 5000;
    while (! thumbnail.isFullyLoaded() && juce::Time::getMillisecondCounter() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    REQUIRE (thumbnail.getTotalLength() > 0.9);

    file.deleteFile();
}
