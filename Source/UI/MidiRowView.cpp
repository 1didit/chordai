#include "MidiRowView.h"
#include "MidiRowLayout.h"

namespace
{
    constexpr int kGutterWidth = 92;
    constexpr int kAccentBarWidth = 2;
    constexpr float kNoteHeight = 3.0f;

    // Per-row accent within the locked pixel-art palette (05-05-PLAN.md
    // <interfaces>). Used for the gutter's left edge bar and note fills.
    juce::Colour accentForStyle (RowStyle style)
    {
        switch (style)
        {
            case RowStyle::DetectedAsIs:    return juce::Colour (0xffd8d8e0);
            case RowStyle::PopHipHopTrap:   return juce::Colour (0xff3a4a6e);
            case RowStyle::RnbNeoSoul:      return juce::Colour (0xff6e5a2f);
            case RowStyle::ElectronicHouse: return juce::Colour (0xffe8c547);
            case RowStyle::Bass:            return juce::Colour (0xff7ec850);
        }

        return juce::Colour (0xffd8d8e0);
    }
}

MidiRowView::MidiRowView()
{
    // Display-only this phase -- Phase 6 flips this for drag-out/audition.
    setInterceptsMouseClicks (false, false);
}

void MidiRowView::setRow (MidiSetRow newRow, double newTotalBeats)
{
    row = std::move (newRow);
    totalBeats = newTotalBeats;
    repaint();
}

void MidiRowView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto gutter = bounds.removeFromLeft (kGutterWidth);
    auto noteArea = bounds;

    auto accent = accentForStyle (row.style);

    // Label gutter.
    g.setColour (juce::Colour (0xff16161e));
    g.fillRect (gutter);

    g.setColour (accent);
    g.fillRect (gutter.removeFromLeft (kAccentBarWidth));

    g.setColour (juce::Colour (0xffd8d8e0));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (row.label, gutter.reduced (4, 0), juce::Justification::centredLeft);

    // Note area.
    g.setColour (juce::Colour (0xff101018));
    g.fillRect (noteArea);

    g.setColour (juce::Colour (0xff2a2a38));
    g.drawHorizontalLine (noteArea.getY(), (float) noteArea.getX(), (float) noteArea.getRight());

    if (row.notes.empty())
        return; // Empty row: paint nothing in the note area beyond the frame above.

    int rowMinPitch = row.notes.front().pitch;
    int rowMaxPitch = row.notes.front().pitch;

    for (auto& note : row.notes)
    {
        rowMinPitch = juce::jmin (rowMinPitch, note.pitch);
        rowMaxPitch = juce::jmax (rowMaxPitch, note.pitch);
    }

    // Each row auto-zooms its own register -- bass reads low-and-sparse
    // without crushing the chord rows into a shared global pitch range.
    rowMinPitch -= 2;
    rowMaxPitch += 2;

    auto noteAreaWidth = noteArea.getWidth();
    auto noteAreaHeight = noteArea.getHeight();

    for (auto& note : row.notes)
    {
        auto x = (float) noteArea.getX() + beatsToX (note.startBeats, totalBeats, noteAreaWidth);
        auto w = noteWidthPx (note.lengthBeats, totalBeats, noteAreaWidth);
        auto y = (float) noteArea.getY() + pitchToY (note.pitch, rowMinPitch, rowMaxPitch, noteAreaHeight);

        // Velocity reads as brightness, not height/size -- flat rects, no
        // gradients, matches the pixel aesthetic.
        auto alpha = 0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, note.velocity);
        g.setColour (accent.withAlpha (alpha));
        g.fillRect (juce::Rectangle<float> (x, y, w, kNoteHeight));
    }
}
