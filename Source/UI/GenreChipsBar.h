#pragma once

#include <JuceHeader.h>

#include <vector>

// 26px chip row between the chord timeline and the (now narrower) waveform
// (06.1-RESEARCH.md Pattern 8): one chip per active main genre + a
// right-anchored "change genres" button that opens GenreSelectorOverlay.
//
// Pure, stateless layout functions first (same extraction rationale as
// MidiRowLayout.h -- unit-testable without a Component), Component below.

namespace GenreChipsBarLayout
{
    constexpr int kOuterMargin = 4;
    constexpr int kVerticalMargin = 2;
    constexpr int kChipGap = 4;
    constexpr int kChangeButtonWidth = 72;
    constexpr int kChangeButtonGap = 8;
}

// Right-anchored "change genres" button, sized from bounds' height (minus
// the vertical margin). Degenerate (zero/negative width or height, or too
// narrow for the fixed button width) bounds returns an empty-safe rect.
inline juce::Rectangle<int> changeButtonRect (juce::Rectangle<int> bounds)
{
    using namespace GenreChipsBarLayout;

    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return {};

    auto b = bounds.reduced (kOuterMargin, kVerticalMargin);
    if (b.getWidth() < kChangeButtonWidth || b.getHeight() <= 0)
        return {};

    return { b.getRight() - kChangeButtonWidth, b.getY(), kChangeButtonWidth, b.getHeight() };
}

// chipCount equal-width chips, left-aligned, 4px gaps, leaving a reserved
// right zone for changeButtonRect. Degenerate (zero/negative width/height,
// non-positive chipCount, or not enough room for even 1px-wide chips)
// returns an empty vector -- never asserts.
inline std::vector<juce::Rectangle<int>> computeChipRects (juce::Rectangle<int> bounds, int chipCount)
{
    using namespace GenreChipsBarLayout;

    std::vector<juce::Rectangle<int>> result;
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0 || chipCount <= 0)
        return result;

    auto b = bounds.reduced (kOuterMargin, kVerticalMargin);
    if (b.getHeight() <= 0)
        return result;

    auto changeBtn = changeButtonRect (bounds);
    int chipAreaRight = changeBtn.isEmpty() ? b.getRight() : (changeBtn.getX() - kChangeButtonGap);
    int chipAreaWidth = chipAreaRight - b.getX();

    int totalGap = kChipGap * (chipCount - 1);
    int chipWidth = (chipAreaWidth - totalGap) / chipCount;
    if (chipWidth <= 0)
        return result;

    result.reserve ((size_t) chipCount);
    int x = b.getX();
    for (int i = 0; i < chipCount; ++i)
    {
        result.push_back ({ x, b.getY(), chipWidth, b.getHeight() });
        x += chipWidth + kChipGap;
    }
    return result;
}

// The chip row Component itself. Ids/shortLabels are parallel arrays in
// mainGenreIds order (already resolved by the editor via findGenre() --
// GenreChipsBar never reaches into GenreRegistry itself, same
// "hooks/data pushed in" idiom as MidiRowView).
class GenreChipsBar : public juce::Component
{
public:
    GenreChipsBar();

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // ids.size() MUST == shortLabels.size(); mismatched sizes are truncated
    // to the shorter length defensively (never crashes).
    void setGenres (const juce::StringArray& ids, const juce::StringArray& shortLabels);
    void setActiveGenreId (const juce::String& id);

    std::function<void (const juce::String& genreId)> onGenreClicked;
    std::function<void()> onChangeGenresClicked;

private:
    juce::StringArray genreIds;
    juce::StringArray genreShortLabels;
    juce::String activeGenreId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenreChipsBar)
};
