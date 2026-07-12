#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ChordAIAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChordAIAudioProcessorEditor (ChordAIAudioProcessor&);
    ~ChordAIAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChordAIAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordAIAudioProcessorEditor)
};
