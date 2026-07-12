#include "AudioFileLoader.h"

std::shared_ptr<const LoadedAudio> loadAudioFileSync (const juce::File& file,
                                                       juce::AudioFormatManager& formatManager)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
        return nullptr;

    auto result = std::make_shared<LoadedAudio>();
    result->sourceFile = file;
    result->sampleRate = reader->sampleRate;

    auto numChannels = (int) reader->numChannels;
    auto numSamples = (int) reader->lengthInSamples;

    if (numChannels <= 0 || numSamples <= 0 || reader->sampleRate <= 0.0)
        return nullptr;

    result->buffer.setSize (numChannels, numSamples);
    reader->read (&result->buffer, 0, numSamples, 0, true, true);
    result->lengthSeconds = (double) numSamples / reader->sampleRate;

    return result;
}
