#include "MidiSetsPanel.h"

#include <algorithm>

void MidiSetsPanel::paint (juce::Graphics& g)
{
    if (rows != nullptr && ! rows->empty())
        return; // Rows paint themselves; nothing left for the panel to draw.

    // Empty state -- keeps MidiSetsPlaceholder's exact idle look (string
    // lifted verbatim so it doesn't change).
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
    }

    resized();
    repaint();
}
