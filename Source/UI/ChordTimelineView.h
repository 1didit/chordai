#pragma once

#include <JuceHeader.h>

#include "../Analysis/AnalysisResult.h"

// Free function (unit-testable without a Component), same extraction
// rationale as WaveformMath.h: decides whether a segment's pixel width is
// wide enough to draw its chord-name label without garbled overlap.
inline bool shouldDrawLabel (float segmentWidthPx, float minLabelWidthPx = 24.0f)
{
    return segmentWidthPx >= minLabelWidthPx;
}

// Read-only chord timeline band: renders the detected ChordSegment progression
// as named, coloured blocks directly above the waveform, using the same
// timeToX pixel math RegionSelectorOverlay already uses. Never intercepts
// mouse input — RegionSelectorOverlay keeps all region-drag interaction.
class ChordTimelineView : public juce::Component
{
public:
    ChordTimelineView();

    void paint (juce::Graphics&) override;

    // Publishes a new (or cleared, via nullptr) analysis result; stores +
    // repaints. Safe to call with the same processor.getAnalysisResult()
    // pointer used by the editor's changeListenerCallback/handleLoadComplete.
    void setResult (std::shared_ptr<const AnalysisResult> newResult);

    // Sets the full-file length used to build the {0, totalLength} visible
    // range for pixel math — matches RegionSelectorOverlay's own convention.
    void setTotalLength (double seconds);

private:
    juce::Colour colourForQuality (ChordQuality quality) const;

    std::shared_ptr<const AnalysisResult> result;
    double totalLength = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordTimelineView)
};
