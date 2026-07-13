#include "MidiSetsPanel.h"

void MidiSetsPanel::paint (juce::Graphics&)
{
    // Fleshed out in Task 2.
}

void MidiSetsPanel::resized()
{
    // Fleshed out in Task 2.
}

void MidiSetsPanel::setRows (std::shared_ptr<const std::vector<MidiSetRow>> newRows)
{
    rows = std::move (newRows);
    repaint();
}
