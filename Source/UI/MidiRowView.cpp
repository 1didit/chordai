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

    // Drag-out temp .mid (EXP-01). 06-RESEARCH.md Pitfall 1 (Ableton "could
    // not be opened"): deleting the temp file in a performExternalDragDropOfFiles
    // completion callback races the host's own async read of the file --
    // Ableton loses that race and reports a read error, with no crash. The
    // fix is to NEVER delete the file in that callback; instead sweep the
    // PREVIOUS drag's file(s) at the START of the next drag, before writing
    // the new one. Do not "optimize" this into a cleanup callback.
    juce::File writeDragTempFile (const MidiSetRow& row, const KeyResult& key, double bpm)
    {
        auto subfolder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ChordAI");

        // Deferred cleanup of the PREVIOUS drag's temp file(s) -- see the
        // Pitfall 1 comment above. This is the only place *.mid files in this
        // subfolder are ever deleted.
        for (auto& f : subfolder.findChildFiles (juce::File::findFiles, false, "*.mid"))
            f.deleteFile();

        subfolder.createDirectory();

        auto target = subfolder.getChildFile (MidiFileWriter::suggestedFileName (row, key, bpm));
        if (! MidiFileWriter::writeToFile (row, bpm, target))
            return {}; // invalid File on write failure

        return target;
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
    // Gesture guard: performExternalDragDropOfFiles must fire exactly once
    // per gesture -- mouseDrag fires repeatedly while the mouse moves. A real
    // drag overrides a click; the click only fires from mouseUp when no drag
    // happened (satisfies "drag anywhere on the row body", icons included).
    if (dragStarted || ! e.mouseWasDraggedSinceMouseDown())
        return;

    dragStarted = true;

    const double bpm = getBpmForExport ? getBpmForExport() : 120.0;
    const auto key = getKeyForExport ? getKeyForExport() : KeyResult {};

    auto tempFile = writeDragTempFile (row, key, bpm);
    if (tempFile.existsAsFile())
    {
        // canMoveFiles MUST be false (the DAW copies bytes, never takes
        // ownership of our temp file) and the callback MUST be nullptr (see
        // the writeDragTempFile comment / 06-RESEARCH.md Pitfall 1) -- ANY
        // cleanup callback here races Ableton's async read. Leave the file on
        // disk; the next drag's cleanup (or OS temp hygiene) collects it.
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            { tempFile.getFullPathName() }, /*canMoveFiles*/ false, this, /*callback*/ nullptr);
    }
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
    // Session-lifetime last-used directory -- v1 pragmatic choice (06-
    // RESEARCH.md Code Examples): promote to a persisted apvts-backed setting
    // only if a later phase needs it to survive across sessions.
    static juce::File lastUsedDirectory =
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ChordAI MIDI");

    const double bpm = getBpmForExport ? getBpmForExport() : 120.0;
    const auto key = getKeyForExport ? getKeyForExport() : KeyResult {};
    const auto suggestedName = MidiFileWriter::suggestedFileName (row, key, bpm);

    lastUsedDirectory.createDirectory();
    auto initial = lastUsedDirectory.getChildFile (suggestedName);

    // fileChooser is a member (MUST outlive launchAsync's callback per the
    // FileChooser header's own doc comment).
    fileChooser = std::make_unique<juce::FileChooser> ("Save MIDI row", initial, "*.mid");

    // rowCopy/bpm captured BY VALUE, no `this` capture: MidiSetsPanel::setRows
    // destroys every MidiRowView on every regeneration, and a save dialog may
    // still be open when that happens. If this view dies, fileChooser dies
    // with it and the callback simply never fires -- nothing dangles.
    auto rowCopy = row;
    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [rowCopy, bpm] (const juce::FileChooser& fc)
        {
            auto chosen = fc.getResult();
            if (chosen == juce::File {})
                return; // user cancelled

            lastUsedDirectory = chosen.getParentDirectory();
            MidiFileWriter::writeToFile (rowCopy, bpm, chosen);
        });
}
