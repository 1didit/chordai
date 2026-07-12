#pragma once

#include <JuceHeader.h>

#include "LoadedAudio.h"

// Testable core: synchronous decode of a single audio file. Returns nullptr on
// an unsupported/corrupt file — never throws, never asserts on bad input.
std::shared_ptr<const LoadedAudio> loadAudioFileSync (const juce::File& file,
                                                       juce::AudioFormatManager& formatManager);

// Thin async wrapper around loadAudioFileSync for use on a juce::ThreadPool
// worker thread. The completion callback is always delivered on the message
// thread via juce::MessageManager::callAsync — either with a non-null result,
// or with (nullptr, errorMessage) so load failure is observable rather than a
// silent no-op.
class AudioFileLoadJob : public juce::ThreadPoolJob
{
public:
    using Callback = std::function<void (std::shared_ptr<const LoadedAudio>, juce::String errorMessage)>;

    AudioFileLoadJob (juce::File fileToLoad, juce::AudioFormatManager& formatManagerToUse, Callback callbackToUse)
        : juce::ThreadPoolJob ("AudioFileLoad"),
          file (std::move (fileToLoad)),
          formatManager (formatManagerToUse),
          callback (std::move (callbackToUse))
    {
    }

    JobStatus runJob() override
    {
        if (shouldExit())
            return JobStatus::jobHasFinished;

        auto result = loadAudioFileSync (file, formatManager);
        auto capturedFile = file;
        auto cb = callback;

        if (result != nullptr)
        {
            juce::MessageManager::callAsync ([cb, result]
            {
                cb (result, {});
            });
        }
        else
        {
            juce::String errorMessage = "Couldn't load " + capturedFile.getFileName();
            juce::MessageManager::callAsync ([cb, errorMessage]
            {
                cb (nullptr, errorMessage);
            });
        }

        return JobStatus::jobHasFinished;
    }

private:
    juce::File file;
    juce::AudioFormatManager& formatManager;
    Callback callback;
};
