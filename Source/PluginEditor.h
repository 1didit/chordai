#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/ConveyorBeltComponent.h"
#include "UI/MidiSetsPlaceholder.h"

class ChordAIAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChordAIAudioProcessorEditor (ChordAIAudioProcessor&);
    ~ChordAIAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChordAIAudioProcessor& processor;

    ConveyorBeltComponent conveyor;
    MidiSetsPlaceholder midiSetsPlaceholder;

    // Middle band, still an empty painted rect this plan — Plan 02-03 fills
    // it with a WaveformView.
    juce::Rectangle<int> waveformArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordAIAudioProcessorEditor)
};
