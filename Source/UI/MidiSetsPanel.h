#pragma once

#include <JuceHeader.h>

#include "MidiRowView.h"
#include "../MidiGen/MidiSetRow.h"

#include <memory>
#include <vector>

// Bottom-band panel (140px) owning one MidiRowView per generated row.
// Replaces the bottom-band placeholder once wired into PluginEditor (05-05 Task 3).
class MidiSetsPanel : public juce::Component
{
public:
    MidiSetsPanel() = default;
    ~MidiSetsPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    // nullptr/empty -> empty state (keeps the placeholder's idle look).
    void setRows (std::shared_ptr<const std::vector<MidiSetRow>> newRows);

private:
    juce::OwnedArray<MidiRowView> rowViews;
    std::shared_ptr<const std::vector<MidiSetRow>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSetsPanel)
};
