// GenreChipsBar.h: pure computeChipRects/changeButtonRect layout functions.
// Mirrors MidiSetsPanelLayoutTests.cpp's icon-rect test pattern -- geometry
// proven against a real fixture, unit-testable without a Component.

#include "../Source/UI/GenreChipsBar.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("GenreChipsBarLayoutTests.ChipRectsFitAndDoNotOverlap", "[genrechipsbarlayout]")
{
    // Matches the real 26px chip-row band (06.1-RESEARCH.md Pattern 8) at a
    // representative window width.
    juce::Rectangle<int> bounds (0, 0, 800, 26);

    auto chips = computeChipRects (bounds, 5);
    auto changeBtn = changeButtonRect (bounds);

    REQUIRE (chips.size() == 5);
    CHECK_FALSE (changeBtn.isEmpty());

    for (auto& chip : chips)
    {
        CHECK_FALSE (chip.isEmpty());
        CHECK (bounds.contains (chip));
        CHECK (chip.getHeight() >= 18); // >= 10px hit target with margin
    }
    CHECK (bounds.contains (changeBtn));
    CHECK (changeBtn.getHeight() >= 18);

    // Pairwise non-overlapping (chips vs chips, chips vs change button).
    for (size_t i = 0; i < chips.size(); ++i)
    {
        for (size_t j = i + 1; j < chips.size(); ++j)
            CHECK_FALSE (chips[i].intersects (chips[j]));

        CHECK_FALSE (chips[i].intersects (changeBtn));
    }

    // Left-to-right chip order.
    for (size_t i = 1; i < chips.size(); ++i)
        CHECK (chips[i - 1].getX() < chips[i].getX());

    // Change button is rightmost.
    for (auto& chip : chips)
        CHECK (chip.getX() < changeBtn.getX());
}

TEST_CASE ("GenreChipsBarLayoutTests.ChipRectsDegenerateSafe", "[genrechipsbarlayout]")
{
    juce::Rectangle<int> zeroWidth (0, 0, 0, 26);
    juce::Rectangle<int> zeroHeight (0, 0, 800, 0);
    juce::Rectangle<int> negativeWidth (0, 0, -10, 26);

    CHECK (computeChipRects (zeroWidth, 5).empty());
    CHECK (computeChipRects (zeroHeight, 5).empty());
    CHECK (computeChipRects (negativeWidth, 5).empty());

    CHECK (changeButtonRect (zeroWidth).isEmpty());
    CHECK (changeButtonRect (zeroHeight).isEmpty());
    CHECK (changeButtonRect (negativeWidth).isEmpty());

    // Zero chip count -> no crash, empty result.
    CHECK (computeChipRects (juce::Rectangle<int> (0, 0, 800, 26), 0).empty());
}
