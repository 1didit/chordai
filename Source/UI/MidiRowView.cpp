#include "MidiRowView.h"
#include "MidiRowLayout.h"
#include "../MidiGen/MidiFileWriter.h"

namespace
{
    constexpr int kGutterWidth = 92;
    constexpr int kAccentBarWidth = 2;
    constexpr float kNoteHeight = 3.0f;
    constexpr juce::uint32 kIconIdleColour = 0xff5a5a6e;

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

    // Play glyph: a stepped pixel triangle built from 3 fillRect columns
    // (flat rects only, no gradients/paths -- pixel-art constraint).
    void drawPlayGlyph (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour colour)
    {
        g.setColour (colour);
        auto b = bounds.reduced (1);
        auto colW = juce::jmax (1, b.getWidth() / 3);
        auto h = b.getHeight();

        g.fillRect (b.getX(),               b.getY() + h / 4,     colW, h / 2);
        g.fillRect (b.getX() + colW,         b.getY() + h / 6,     colW, (h * 2) / 3);
        g.fillRect (b.getX() + colW * 2,     b.getY(),             colW, h);
    }

    // Stop glyph: a single solid square.
    void drawStopGlyph (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour colour)
    {
        g.setColour (colour);
        g.fillRect (bounds.reduced (1));
    }

    // Save glyph: a small down-arrow-into-tray, 3 fillRects (stem, arrowhead,
    // tray) -- flat rects only.
    void drawSaveGlyph (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour colour)
    {
        g.setColour (colour);
        auto b = bounds.reduced (1);
        auto w = b.getWidth();
        auto h = b.getHeight();

        g.fillRect (b.getX() + w / 2 - 1, b.getY(), 2, (h * 2) / 3);              // stem
        g.fillRect (b.getX() + 1,         b.getY() + h / 3, w - 2, 2);           // arrowhead bar
        g.fillRect (b.getX(),             b.getBottom() - 2, w, 2);              // tray
    }
}

MidiRowView::MidiRowView()
{
    // Phase 6: accepts mouse for play/stop/save icons + drag-anywhere-on-row
    // export (child components -- none here -- would still get first refusal;
    // false for that param since there are no children to route to).
    setInterceptsMouseClicks (true, false);
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
    auto gutterBounds = gutter; // full gutter, before any removeFrom* mutation below
    auto noteArea = bounds;

    auto accent = accentForStyle (row.style);

    // Label gutter.
    g.setColour (juce::Colour (0xff16161e));
    g.fillRect (gutter);

    g.setColour (accent);
    g.fillRect (gutter.removeFromLeft (kAccentBarWidth));

    auto play = playIconRect (gutterBounds);
    auto save = saveIconRect (gutterBounds);

    // Label text never underlaps the icon zones: shrink the label rect on
    // the right by however far the icons' left edge intrudes.
    auto labelRect = gutter.reduced (4, 0);
    if (! play.isEmpty())
        labelRect.setRight (juce::jmin (labelRect.getRight(), play.getX() - 2));

    g.setColour (juce::Colour (0xffd8d8e0));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (row.label, labelRect, juce::Justification::centredLeft);

    // Play/stop + save icons, flat single-colour fills only (pixel-art
    // constraint, 02-CONTEXT.md).
    const bool playing = isRowPlaying && isRowPlaying (row.id);
    if (playing)
        drawStopGlyph (g, play, accent);
    else
        drawPlayGlyph (g, play, juce::Colour (kIconIdleColour));

    drawSaveGlyph (g, save, juce::Colour (kIconIdleColour));

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

void MidiRowView::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
}

void MidiRowView::mouseDrag (const juce::MouseEvent& e)
{
    // Gesture guard only this task -- the drag-out body (temp .mid write +
    // performExternalDragDropOfFiles) lands in the next task.
    if (dragStarted || ! e.mouseWasDraggedSinceMouseDown())
        return;

    dragStarted = true;
}

void MidiRowView::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted)
    {
        auto gutterBounds = getLocalBounds().removeFromLeft (kGutterWidth);
        auto pos = e.getPosition();

        if (playIconRect (gutterBounds).contains (pos))
        {
            if (onAuditionToggle)
                onAuditionToggle (row);
            repaint();
        }
        else if (saveIconRect (gutterBounds).contains (pos))
        {
            saveRow();
        }
    }

    dragStarted = false;
}

void MidiRowView::saveRow()
{
    // Stub -- the next task fills the real async FileChooser save flow.
}
