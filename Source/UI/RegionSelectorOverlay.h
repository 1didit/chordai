#pragma once

#include <JuceHeader.h>

#include "RegionSelectionModel.h"

// Transparent overlay that sits exactly on top of a WaveformView, handling
// all mouse interaction for drag-selection. Owns the pure
// RegionSelectionModel; the view underneath never needs mouse events.
class RegionSelectorOverlay : public juce::Component
{
public:
    RegionSelectorOverlay();

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Sets the file length; resets the selection to whole-file and fires
    // onRegionChanged so the whole-file default is registered downstream.
    void setTotalLength (double seconds);

    // Fired on every selection change, including the initial whole-file
    // default fired by setTotalLength().
    std::function<void (juce::Range<double>)> onRegionChanged;

private:
    RegionSelectionModel model;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionSelectorOverlay)
};
