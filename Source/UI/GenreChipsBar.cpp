#include "GenreChipsBar.h"

namespace
{
    // Flat-rect pixel chips (02-CONTEXT.md convention) -- gold accent for the
    // active chip (matches drag-hover/progress gold elsewhere), dark fill +
    // 1px border for inactive chips and the change button.
    constexpr juce::uint32 kGoldAccent = 0xffe8c547;
    constexpr juce::uint32 kInactiveFill = 0xff23232e;
    constexpr juce::uint32 kBorder = 0xff3a3a4a;
    constexpr juce::uint32 kTextLight = 0xffd8d8e0;
    constexpr juce::uint32 kTextDark = 0xff16161e;
}

GenreChipsBar::GenreChipsBar()
{
    setInterceptsMouseClicks (true, false);
}

void GenreChipsBar::setGenres (const juce::StringArray& ids, const juce::StringArray& shortLabels)
{
    auto n = juce::jmin (ids.size(), shortLabels.size());

    genreIds.clear();
    genreShortLabels.clear();
    for (int i = 0; i < n; ++i)
    {
        genreIds.add (ids[i]);
        genreShortLabels.add (shortLabels[i]);
    }

    repaint();
}

void GenreChipsBar::setActiveGenreId (const juce::String& id)
{
    activeGenreId = id;
    repaint();
}

void GenreChipsBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101018));

    auto chips = computeChipRects (getLocalBounds(), genreIds.size());
    auto changeBtn = changeButtonRect (getLocalBounds());

    g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());

    for (size_t i = 0; i < chips.size(); ++i)
    {
        auto isActive = genreIds[(int) i] == activeGenreId;

        g.setColour (juce::Colour (isActive ? kGoldAccent : kInactiveFill));
        g.fillRect (chips[i]);

        g.setColour (juce::Colour (kBorder));
        g.drawRect (chips[i], 1);

        g.setColour (juce::Colour (isActive ? kTextDark : kTextLight));
        g.drawText (genreShortLabels[(int) i], chips[i], juce::Justification::centred);
    }

    if (! changeBtn.isEmpty())
    {
        g.setColour (juce::Colour (kInactiveFill));
        g.fillRect (changeBtn);

        g.setColour (juce::Colour (kGoldAccent));
        g.drawRect (changeBtn, 1);

        g.setColour (juce::Colour (kTextLight));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("CHANGE")), changeBtn, juce::Justification::centred);
    }
}

void GenreChipsBar::mouseUp (const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    auto changeBtn = changeButtonRect (getLocalBounds());
    if (changeBtn.contains (pos))
    {
        if (onChangeGenresClicked)
            onChangeGenresClicked();
        return;
    }

    auto chips = computeChipRects (getLocalBounds(), genreIds.size());
    for (size_t i = 0; i < chips.size(); ++i)
    {
        if (chips[i].contains (pos))
        {
            if (onGenreClicked)
                onGenreClicked (genreIds[(int) i]);
            return;
        }
    }
}
