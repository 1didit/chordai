#include "MidiSetsPanel.h"

#include <algorithm>

void MidiSetsPanel::paint (juce::Graphics& g)
{
    if (rows != nullptr && ! rows->empty())
        return; // Rows paint themselves; nothing left for the panel to draw.

    // Empty state -- keeps the original bottom-band placeholder's exact idle
    // look (string lifted verbatim so it doesn't change).
    g.fillAll (juce::Colour (0xff101018));

    g.setColour (juce::Colour (0xff2a2a38));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    g.setColour (juce::Colour (0xff5a5a6e));
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.drawText (juce::String (juce::CharPointer_UTF8 ("MIDI SETS \xe2\x80\x94 ANALYZE A TRACK TO FILL THE CONVEYOR")),
                getLocalBounds(),
                juce::Justification::centred);
}

void MidiSetsPanel::resized()
{
    if (rowViews.isEmpty())
        return;

    auto bounds = getLocalBounds();
    auto rowHeight = bounds.getHeight() / rowViews.size();

    for (int i = 0; i < rowViews.size(); ++i)
    {
        auto isLast = (i == rowViews.size() - 1);
        rowViews.getUnchecked (i)->setBounds (isLast ? bounds : bounds.removeFromTop (rowHeight));
    }
}

void MidiSetsPanel::setRows (std::shared_ptr<const std::vector<MidiSetRow>> newRows)
{
    // FIRST LINE, unconditionally: a regenerate always invalidates whatever
    // was playing -- the old MidiSetRow object no longer exists, and every
    // MidiRowView is about to be destroyed below (06-RESEARCH.md Pitfall 3).
    if (onStopAudition)
        onStopAudition();

    rows = std::move (newRows);
    rowViews.clear();

    if (rows == nullptr || rows->empty())
    {
        repaint();
        return;
    }

    // All rows share one time axis -- cross-style rhythm comparison is the
    // point (05-05-PLAN.md). Computed fresh each call, never hardcoded.
    double totalBeats = 0.0;
    for (auto& row : *rows)
        for (auto& note : row.notes)
            totalBeats = std::max (totalBeats, note.startBeats + note.lengthBeats);

    for (auto& row : *rows)
    {
        auto* view = rowViews.add (new MidiRowView());
        addAndMakeVisible (view);
        view->setRow (row, totalBeats);

        // Forward the row-level hooks -- MidiRowView never reaches into
        // PluginProcessor directly.
        view->getBpmForExport = getBpmForExport;
        view->getKeyForExport = getKeyForExport;
        view->isRowPlaying = isRowPlaying;
        view->onAuditionToggle = [this] (const MidiSetRow& r)
        {
            if (onAuditionToggle)
                onAuditionToggle (r);

            // Playing-state repaint: icon glyphs read play state live from
            // isRowPlaying (no cached booleans on views) -- the timer just
            // has to keep repainting while something is (or might still be)
            // playing, including auto-stop at buffer end.
            startTimerHz (10);
        };
    }

    resized();
    repaint();
}

void MidiSetsPanel::timerCallback()
{
    for (auto* view : rowViews)
        view->repaint();

    bool anyPlaying = false;
    if (isRowPlaying && rows != nullptr)
        for (auto& row : *rows)
            anyPlaying = anyPlaying || isRowPlaying (row.id);

    if (! anyPlaying)
        stopTimer(); // one final repaint above already happened this call
}
