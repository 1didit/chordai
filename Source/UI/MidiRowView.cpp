#include "MidiRowView.h"

MidiRowView::MidiRowView()
{
    // Display-only this phase -- Phase 6 flips this for drag-out/audition.
    setInterceptsMouseClicks (false, false);
}

void MidiRowView::setRow (MidiSetRow newRow, double newTotalBeats)
{
    row = std::move (newRow);
    totalBeats = newTotalBeats;
    repaint();
}

void MidiRowView::paint (juce::Graphics&)
{
    // Fleshed out in Task 2.
}
