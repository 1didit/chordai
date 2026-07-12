#include <JuceHeader.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Source/Import/AudioFileLoader.h"

namespace
{
    // Writes a 1 s, 440 Hz sine, 44.1 kHz stereo test file using the given format's writer.
    // bitsPerSample lets FLAC use 16-bit (its writer rejects 32-bit float).
    juce::File writeSineFixture (juce::AudioFormat& format, const juce::String& extension, int bitsPerSample)
    {
        auto file = juce::File::createTempFile (extension);

        constexpr double sampleRate = 44100.0;
        constexpr int numChannels = 2;
        constexpr int numSamples = 44100; // exactly 1 s

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) i / sampleRate);
        }

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        REQUIRE (stream != nullptr);

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate, (unsigned int) numChannels, bitsPerSample, {}, 0));
        REQUIRE (writer != nullptr);
        stream.release(); // writer now owns the stream

        writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        writer.reset(); // flush + close

        return file;
    }
}

TEST_CASE ("AudioFileLoaderTests.WavDecode", "[audiofileloader]")
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::WavAudioFormat wav;
    auto file = writeSineFixture (wav, ".wav", 32);

    auto loaded = loadAudioFileSync (file, formatManager);
    REQUIRE (loaded != nullptr);
    CHECK (loaded->sampleRate == 44100.0);
    CHECK (loaded->buffer.getNumChannels() == 2);
    CHECK (loaded->buffer.getNumSamples() == 44100);
    CHECK (loaded->lengthSeconds == Catch::Approx (1.0).margin (0.01));

    file.deleteFile();
}

TEST_CASE ("AudioFileLoaderTests.AiffDecode", "[audiofileloader]")
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::AiffAudioFormat aiff;
    auto file = writeSineFixture (aiff, ".aiff", 32);

    auto loaded = loadAudioFileSync (file, formatManager);
    REQUIRE (loaded != nullptr);
    CHECK (loaded->sampleRate == 44100.0);
    CHECK (loaded->buffer.getNumChannels() == 2);
    CHECK (loaded->buffer.getNumSamples() == 44100);
    CHECK (loaded->lengthSeconds == Catch::Approx (1.0).margin (0.01));

    file.deleteFile();
}

TEST_CASE ("AudioFileLoaderTests.FlacDecode", "[audiofileloader]")
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::FlacAudioFormat flac;
    auto file = writeSineFixture (flac, ".flac", 16); // FLAC writer does not accept 32-bit float

    auto loaded = loadAudioFileSync (file, formatManager);
    REQUIRE (loaded != nullptr);
    CHECK (loaded->sampleRate == 44100.0);
    CHECK (loaded->buffer.getNumChannels() == 2);
    CHECK (loaded->buffer.getNumSamples() == 44100);
    CHECK (loaded->lengthSeconds == Catch::Approx (1.0).margin (0.01));

    file.deleteFile();
}

TEST_CASE ("AudioFileLoaderTests.Mp3Decode", "[audiofileloader]")
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::File file (juce::String (CHORDAI_FIXTURES_DIR) + "/silence_1s.mp3");
    REQUIRE (file.existsAsFile());

    auto loaded = loadAudioFileSync (file, formatManager);
    REQUIRE (loaded != nullptr);
    CHECK (loaded->sampleRate == 44100.0);
    CHECK (loaded->lengthSeconds >= 0.9);
    CHECK (loaded->lengthSeconds <= 1.2);
}

TEST_CASE ("AudioFileLoaderTests.UnsupportedFileFails", "[audiofileloader]")
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    auto file = juce::File::createTempFile (".txt");
    file.replaceWithText ("this is not an audio file, just garbage bytes 12345");

    auto loaded = loadAudioFileSync (file, formatManager);
    CHECK (loaded == nullptr);

    file.deleteFile();
}
