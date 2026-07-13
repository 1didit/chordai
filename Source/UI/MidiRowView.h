#pragma once

#include <JuceHeader.h>

#include "../MidiGen/MidiSetRow.h"
#include "../Analysis/AnalysisResult.h"

#include <memory>

// One row's labeled mini piano-roll strip inside MidiSetsPanel's 140px
// bottom band. Phase 6: interactive -- play/stop + save icons in the gutter,
// drag-anywhere-on-the-row OS file drag. MidiRowView never reaches into
// PluginProcessor directly; all processor-backed behavior comes through the
// hooks below, set by MidiSetsPanel (06-RESEARCH.md User Constraints).
class MidiRowView : public juce::Component
{
public:
    MidiRowView();
    ~MidiRowView() override = default;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Copies row + repaints. totalBeats is the panel-wide shared time axis
    // (all 5 rows compare on the same scale, see MidiSetsPanel::setRows).
    void setRow (MidiSetRow newRow, double newTotalBeats);

    // Phase 6 drag-out/audition hook.
    const MidiSetRow& getRow() const { return row; }

    // --- Hooks, set by MidiSetsPanel right after construction/setRow -------
    // MidiRowView never reaches into PluginProcessor directly.
    std::function<double()> getBpmForExport;
    std::function<KeyResult()> getKeyForExport;
    std::function<void (const MidiSetRow&)> onAuditionToggle;
    std::function<bool (const juce::String& rowId)> isRowPlaying;
    // GEN-11: regenerate icon click. patternIndex identifies which of the 5
    // fixed row slots to rebuild -- MidiSetsPanel forwards this straight to
    // PluginProcessor::regenerateRow(int), same idiom as onAuditionToggle.
    std::function<void (int patternIndex)> onRegenerateRow;

private:
    void saveRow();

    MidiSetRow row;
    double totalBeats = 0.0;

    bool dragStarted = false;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRowView)
};
