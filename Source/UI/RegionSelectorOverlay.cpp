#include "RegionSelectorOverlay.h"
#include "WaveformMath.h"

namespace
{
    const juce::Colour dimColour  { 0x99000000 };
    const juce::Colour edgeColour { 0xffe8c547 };
    const juce::Colour tintColour { 0x22e8c547 };
}

RegionSelectorOverlay::RegionSelectorOverlay()
{
    // The waveform view below never needs mouse events — only this overlay
    // handles drag-selection.
    setInterceptsMouseClicks (true, false);
}

void RegionSelectorOverlay::setTotalLength (double seconds)
{
    model.setTotalLength (seconds);
    repaint();

    if (onRegionChanged)
        onRegionChanged (model.getRegion());
}

void RegionSelectorOverlay::mouseDown (const juce::MouseEvent& e)
{
    juce::Range<double> visibleRange (0.0, model.getTotalLength());
    model.beginDrag (xToTime ((float) e.x, visibleRange, getWidth()));
    repaint();
}

void RegionSelectorOverlay::mouseDrag (const juce::MouseEvent& e)
{
    juce::Range<double> visibleRange (0.0, model.getTotalLength());
    model.dragTo (xToTime ((float) e.x, visibleRange, getWidth()));
    repaint();
}

void RegionSelectorOverlay::mouseUp (const juce::MouseEvent&)
{
    model.endDrag();
    repaint();

    if (onRegionChanged)
        onRegionChanged (model.getRegion());
}

void RegionSelectorOverlay::paint (juce::Graphics& g)
{
    // Whole file selected = default state = no visual clutter.
    if (model.isWholeFile())
        return;

    juce::Range<double> visibleRange (0.0, model.getTotalLength());
    auto region = model.getRegion();

    auto startX = timeToX (region.getStart(), visibleRange, getWidth());
    auto endX = timeToX (region.getEnd(), visibleRange, getWidth());

    auto bounds = getLocalBounds().toFloat();

    // Dim the areas outside the selected region.
    g.setColour (dimColour);
    if (startX > bounds.getX())
        g.fillRect (bounds.withRight (startX));
    if (endX < bounds.getRight())
        g.fillRect (bounds.withLeft (endX));

    // Subtle tint inside the selected region.
    g.setColour (tintColour);
    g.fillRect (juce::Rectangle<float> (startX, bounds.getY(), endX - startX, bounds.getHeight()));

    // Bright 1px edges at the region start/end.
    g.setColour (edgeColour);
    g.drawVerticalLine ((int) startX, bounds.getY(), bounds.getBottom());
    g.drawVerticalLine ((int) endX, bounds.getY(), bounds.getBottom());
}
