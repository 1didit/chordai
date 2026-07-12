#pragma once

#include <JuceHeader.h>

// AudioThumbnail + AudioThumbnailCache wrapper — renders the loaded file's
// waveform in the editor's middle band. Owns its own AudioFormatManager so
// thumbnail scanning is entirely independent of the processor's own decode
// path (they never share mutable state).
//
// RT-safety: lives entirely on the message thread. Must never be referenced
// from AudioProcessor::processBlock.
class WaveformView : public juce::Component,
                      public juce::ChangeListener
{
public:
    WaveformView();
    ~WaveformView() override;

    void paint (juce::Graphics&) override;

    // Kicks off an async thumbnail scan of the given file. Clears any prior
    // error message. The waveform fills in progressively as the thumbnail's
    // background scan proceeds (it broadcasts changes as it goes).
    void setSource (const juce::File& file);

    // Displays an error message in place of the waveform (e.g. failed load).
    void setErrorMessage (juce::String message);

    double getTotalLength() const;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Declaration order matters: formatManager must exist before thumbnail
    // (ctor arg), and thumbnailCache must exist before thumbnail too.
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    juce::String errorMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
