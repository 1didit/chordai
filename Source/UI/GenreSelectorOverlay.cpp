#include "GenreSelectorOverlay.h"
#include "../MidiGen/GenreRegistry.h"

namespace
{
    constexpr juce::uint32 kBackdrop = 0xe0101018;
    constexpr juce::uint32 kPanelFill = 0xff16161e;
    constexpr juce::uint32 kPanelBorder = 0xff3a3a4a;
    constexpr juce::uint32 kGoldAccent = 0xffe8c547;
    constexpr juce::uint32 kTextLight = 0xffd8d8e0;
    constexpr juce::uint32 kTextDark = 0xff16161e;
    constexpr juce::uint32 kCheckboxEmpty = 0xff23232e;

    constexpr int kPanelWidth = 260;
    constexpr int kTitleHeight = 24;
    constexpr int kRowHeight = 24;
    constexpr int kDoneHeight = 28;
    constexpr int kPadding = 12;
    constexpr int kCheckboxSize = 12;
}

GenreSelectorOverlay::GenreSelectorOverlay()
{
    // Intercepts ALL mouse input while visible (modal, same as
    // RegionSelectorOverlay's whole-waveform interception, but here for the
    // whole editor).
    setInterceptsMouseClicks (true, false);
}

void GenreSelectorOverlay::setSelection (juce::StringArray mainFive)
{
    selectedFive = std::move (mainFive);
    repaint();
}

juce::Rectangle<int> GenreSelectorOverlay::panelBounds() const
{
    auto numRows = (int) allGenres().size();
    auto panelHeight = kPadding * 2 + kTitleHeight + numRows * kRowHeight + kPadding + kDoneHeight;

    return juce::Rectangle<int> (kPanelWidth, panelHeight).withCentre (getLocalBounds().getCentre());
}

juce::Rectangle<int> GenreSelectorOverlay::rowRect (int index) const
{
    auto panel = panelBounds();
    auto y = panel.getY() + kPadding + kTitleHeight + index * kRowHeight;
    return { panel.getX() + kPadding, y, panel.getWidth() - kPadding * 2, kRowHeight };
}

juce::Rectangle<int> GenreSelectorOverlay::doneButtonRect() const
{
    auto panel = panelBounds();
    auto numRows = (int) allGenres().size();
    auto y = panel.getY() + kPadding + kTitleHeight + numRows * kRowHeight + kPadding;
    return { panel.getX() + kPadding, y, panel.getWidth() - kPadding * 2, kDoneHeight };
}

void GenreSelectorOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBackdrop));

    auto panel = panelBounds();
    g.setColour (juce::Colour (kPanelFill));
    g.fillRect (panel);
    g.setColour (juce::Colour (kGoldAccent));
    g.drawRect (panel, 1);

    g.setColour (juce::Colour (kTextLight));
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.drawText (juce::String (juce::CharPointer_UTF8 ("SELECT 5 GENRES")),
                panel.getX(), panel.getY() + kPadding / 2, panel.getWidth(), kTitleHeight,
                juce::Justification::centred);

    auto& genres = allGenres();
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (int i = 0; i < (int) genres.size(); ++i)
    {
        auto row = rowRect (i);
        auto& genre = genres[(size_t) i];
        bool selected = selectedFive.contains (genre.id);

        juce::Rectangle<int> checkbox (row.getX(), row.getCentreY() - kCheckboxSize / 2, kCheckboxSize, kCheckboxSize);
        g.setColour (juce::Colour (selected ? kGoldAccent : kCheckboxEmpty));
        g.fillRect (checkbox);
        g.setColour (juce::Colour (kPanelBorder));
        g.drawRect (checkbox, 1);

        g.setColour (juce::Colour (kTextLight));
        auto labelRect = row.withTrimmedLeft (kCheckboxSize + 8);
        g.drawText (genre.label, labelRect, juce::Justification::centredLeft);
    }

    auto done = doneButtonRect();
    g.setColour (juce::Colour (kGoldAccent));
    g.fillRect (done);
    g.setColour (juce::Colour (kTextDark));
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.drawText (juce::String (juce::CharPointer_UTF8 ("DONE")), done, juce::Justification::centred);
}

void GenreSelectorOverlay::mouseUp (const juce::MouseEvent& e)
{
    auto pos = e.getPosition();
    auto panel = panelBounds();

    if (! panel.contains (pos))
    {
        if (onDismiss)
            onDismiss();
        return;
    }

    if (doneButtonRect().contains (pos))
    {
        if (onDismiss)
            onDismiss();
        return;
    }

    auto& genres = allGenres();
    for (int i = 0; i < (int) genres.size(); ++i)
    {
        if (rowRect (i).contains (pos))
        {
            if (onGenreToggled)
                onGenreToggled (genres[(size_t) i].id);
            return;
        }
    }
}
