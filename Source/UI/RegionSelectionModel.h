#pragma once

#include <JuceHeader.h>

// Pure drag-selection state machine — no pixels, no JUCE Component. All units
// are seconds; the overlay component converts pixel<->time via WaveformMath
// and drives this model with the results. Message-thread agnostic (no
// dependency on any Component/GUI type beyond juce_core's juce::Range).
struct RegionSelectionModel
{
    // Stores the file length and resets the selection to the whole file
    // (IMP-03 default: zero interaction == whole file selected).
    void setTotalLength (double lengthSeconds)
    {
        totalLength = juce::jmax (0.0, lengthSeconds);
        region = { 0.0, totalLength };
    }

    void beginDrag (double timeSeconds)
    {
        dragAnchor = juce::jlimit (0.0, totalLength, timeSeconds);
        region = { dragAnchor, dragAnchor };
    }

    // Live region = normalized min/max of the drag anchor and the current
    // position, clamped to [0, totalLength]. Right-to-left drags normalize
    // so start <= end always holds.
    void dragTo (double timeSeconds)
    {
        auto clamped = juce::jlimit (0.0, totalLength, timeSeconds);
        auto lo = juce::jmin (dragAnchor, clamped);
        auto hi = juce::jmax (dragAnchor, clamped);
        region = { lo, hi };
    }

    // A click or micro-drag (resulting selection under 0.05s) is not a
    // deliberate region choice — reset back to the whole-file default.
    void endDrag()
    {
        if (region.getLength() < 0.05)
            region = { 0.0, totalLength };
    }

    juce::Range<double> getRegion() const { return region; }

    double getTotalLength() const { return totalLength; }

    bool isWholeFile() const
    {
        return std::abs (region.getStart() - 0.0) < 1e-9
            && std::abs (region.getEnd() - totalLength) < 1e-9;
    }

private:
    double totalLength = 0.0;
    double dragAnchor = 0.0;
    juce::Range<double> region;
};
