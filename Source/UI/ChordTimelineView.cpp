#include "ChordTimelineView.h"
#include "ChordNameFormatter.h"
#include "WaveformMath.h"

ChordTimelineView::ChordTimelineView()
{
    // Read-only display in Phase 4 — never steals mouse input from
    // RegionSelectorOverlay, which keeps all region-drag interaction (own
    // dedicated band means no z-order fight either, per 04-RESEARCH.md
    // Pitfall 6).
    setInterceptsMouseClicks (false, false);
}

void ChordTimelineView::setResult (std::shared_ptr<const AnalysisResult> newResult)
{
    result = std::move (newResult);
    repaint();
}

void ChordTimelineView::setTotalLength (double seconds)
{
    totalLength = seconds;
    repaint();
}

juce::Colour ChordTimelineView::colourForQuality (ChordQuality quality) const
{
    switch (quality)
    {
        case ChordQuality::Major:     return juce::Colour (0xff3f6f4a);
        case ChordQuality::Minor:     return juce::Colour (0xff3a4a6e);
        case ChordQuality::Dominant7: return juce::Colour (0xff6e5a2f);
        case ChordQuality::NoChord:
        default:                     return juce::Colour (0xff23232e);
    }
}

void ChordTimelineView::paint (juce::Graphics& g)
{
    if (result == nullptr || totalLength <= 0.0)
        return;

    juce::Range<double> visibleRange (0.0, totalLength);
    auto height = (float) getHeight();

    for (const auto& seg : result->chords)
    {
        auto x0 = timeToX (seg.startSeconds, visibleRange, getWidth());
        auto x1 = timeToX (seg.endSeconds, visibleRange, getWidth());
        juce::Rectangle<float> blockBounds (x0, 0.0f, x1 - x0, height);

        g.setColour (colourForQuality (seg.chord.quality));
        g.fillRect (blockBounds);

        // 1px darker boundary line at the segment start.
        g.setColour (colourForQuality (seg.chord.quality).darker (0.4f));
        g.drawVerticalLine ((int) x0, 0.0f, height);

        if (shouldDrawLabel (blockBounds.getWidth()))
        {
            g.setColour (juce::Colour (0xffd8d8e0));
            g.setFont (11.0f);
            g.drawText (chordName (seg.chord), blockBounds.reduced (1.0f), juce::Justification::centred, false);
        }
    }
}
