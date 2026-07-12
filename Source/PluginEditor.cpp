#include "PluginEditor.h"

ChordAIAudioProcessorEditor::ChordAIAudioProcessorEditor (ChordAIAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (400, 300);
}

void ChordAIAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

void ChordAIAudioProcessorEditor::resized()
{
}
