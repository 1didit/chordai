#pragma once

#include <JuceHeader.h>

#include "../MidiGen/MidiSetRow.h"

// One row's labeled mini piano-roll strip inside MidiSetsPanel's 140px
// bottom band. Display-only in Phase 5 (setInterceptsMouseClicks(false,
// false)) -- setRow/getRow are structured so Phase 6 can add audition/drag
// without reshaping this API.
class MidiRowView : public juce::Component
{
public:
    MidiRowView();
    ~MidiRowView() override = default;

    void paint (juce::Graphics&) override;

    // Copies row + repaints. totalBeats is the panel-wide shared time axis
    // (all 5 rows compare on the same scale, see MidiSetsPanel::setRows).
    void setRow (MidiSetRow newRow, double newTotalBeats);

    // Phase 6 drag-out/audition hook.
    const MidiSetRow& getRow() const { return row; }

private:
    MidiSetRow row;
    double totalBeats = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRowView)
};
