#pragma once

#include <JuceHeader.h>

// Full-editor-bounds modal "all genres" checkbox-list overlay (06.1-RESEARCH.md
// Pattern 8), same category as RegionSelectorOverlay: a Component covering
// the whole editor, hidden by default (addChildComponent), shown via
// setVisible(true) + toFront(false).
//
// Deliberately dumb: the overlay never calls toggleMainGenre() itself or
// touches the processor -- it only reports which id was clicked via
// onGenreToggled, and the EDITOR applies GenreSelectionLogic::toggleMainGenre
// + processor.setMainGenres, then calls setSelection() back so the checkbox
// glyphs reflect the new state. The overlay stays open across multiple
// toggles (no per-click dismiss) -- only DONE (or a backdrop click outside
// the panel) closes it.
class GenreSelectorOverlay : public juce::Component
{
public:
    GenreSelectorOverlay();

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Reflects the current main-5 selection in the checkbox glyphs.
    void setSelection (juce::StringArray mainFive);

    std::function<void (const juce::String& clickedId)> onGenreToggled;
    std::function<void()> onDismiss;

private:
    juce::Rectangle<int> panelBounds() const;
    juce::Rectangle<int> rowRect (int index) const;
    juce::Rectangle<int> doneButtonRect() const;

    juce::StringArray selectedFive;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenreSelectorOverlay)
};
