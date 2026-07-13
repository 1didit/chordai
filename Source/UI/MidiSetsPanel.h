#pragma once

#include <JuceHeader.h>

#include "MidiRowView.h"
#include "../MidiGen/MidiSetRow.h"
#include "../Analysis/AnalysisResult.h"

#include <memory>
#include <vector>

// Bottom-band panel (140px) owning one MidiRowView per generated row.
// Replaces the bottom-band placeholder once wired into PluginEditor (05-05 Task 3).
//
// Phase 6: forwards audition/export hooks (assigned once by the editor) into
// every rebuilt MidiRowView, and stops any playing audition before every
// rebuild -- MidiSetsPanel::setRows() destroys and recreates every
// MidiRowView on every call, including region-change regeneration
// (06-RESEARCH.md Pitfall 3), so playing-row state must never live on a view.
class MidiSetsPanel : public juce::Component,
                       private juce::Timer
{
public:
    MidiSetsPanel() = default;
    ~MidiSetsPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    // nullptr/empty -> empty state (keeps the placeholder's idle look).
    void setRows (std::shared_ptr<const std::vector<MidiSetRow>> newRows);

    // --- Hooks, assigned once by the editor -------------------------------
    std::function<double()> getBpmForExport;
    std::function<KeyResult()> getKeyForExport;
    std::function<void (const MidiSetRow&)> onAuditionToggle;
    std::function<bool (const juce::String& rowId)> isRowPlaying;
    std::function<void()> onStopAudition;
    // GEN-11: forwarded verbatim into every rebuilt MidiRowView, same idiom
    // as onAuditionToggle -- MidiRowView never reaches into PluginProcessor
    // directly.
    std::function<void (int patternIndex)> onRegenerateRow;

private:
    void timerCallback() override;

    juce::OwnedArray<MidiRowView> rowViews;
    std::shared_ptr<const std::vector<MidiSetRow>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSetsPanel)
};
