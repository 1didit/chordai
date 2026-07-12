#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/ConveyorBeltComponent.h"
#include "UI/MidiSetsPlaceholder.h"
#include "UI/WaveformView.h"
#include "UI/RegionSelectorOverlay.h"

class ChordAIAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::ChangeListener
{
public:
    explicit ChordAIAudioProcessorEditor (ChordAIAudioProcessor&);
    ~ChordAIAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Shared by changeListenerCallback (fresh load) and the ctor's
    // editor-reopen restore path (DAW closes/reopens editor while the
    // processor keeps its state) — both need the same waveform/region/
    // conveyor-stub wiring.
    void handleLoadComplete (const LoadedAudio& loaded);

    ChordAIAudioProcessor& processor;

    ConveyorBeltComponent conveyor;
    WaveformView waveformView;
    RegionSelectorOverlay regionOverlay;
    MidiSetsPlaceholder midiSetsPlaceholder;

    juce::Rectangle<int> waveformArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordAIAudioProcessorEditor)
};
