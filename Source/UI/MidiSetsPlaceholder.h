#pragma once

#include <JuceHeader.h>

// Bottom-band empty state reserved for the future MIDI-set list (the
// "conveyor rows" from 02-CONTEXT.md). Phase 5 populates this with real
// rows; Phase 6 makes them auditionable/draggable. Phase 2 reserves the
// layout region only — do NOT add rows or interaction here.
class MidiSetsPlaceholder : public juce::Component
{
public:
    MidiSetsPlaceholder() = default;
    ~MidiSetsPlaceholder() override = default;

    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSetsPlaceholder)
};
