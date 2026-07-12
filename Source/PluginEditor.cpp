#include "PluginEditor.h"

ChordAIAudioProcessorEditor::ChordAIAudioProcessorEditor (ChordAIAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (conveyor);
    addAndMakeVisible (midiSetsPlaceholder);

    conveyor.onFileDropped = [this] (juce::File f) { processor.loadAudioFile (std::move (f)); };

    setSize (800, 520);
}

void ChordAIAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d12));

    g.setColour (juce::Colour (0xff5a5a6e));
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText ("DROP A TRACK ONTO THE CONVEYOR", waveformArea, juce::Justification::centred);
}

void ChordAIAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    conveyor.setBounds (bounds.removeFromTop (120));
    midiSetsPlaceholder.setBounds (bounds.removeFromBottom (140));

    waveformArea = bounds; // remaining middle (~260 px) — Plan 02-03 fills this
}
