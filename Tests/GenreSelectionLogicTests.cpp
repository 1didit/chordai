// GenreSelectionLogic.h: pure toggleMainGenre() FIFO-swap invariant tests
// (06.1-RESEARCH.md Pattern 8). Exhaustively proves the "exactly 5, always"
// invariant holds at every instant across a scripted toggle sequence, not
// just for a single call.

#include "../Source/UI/GenreSelectionLogic.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

namespace
{
    // The 10-genre universe (mirrors GenreRegistry.cpp's real ids -- kept
    // hand-written here so this test stays independent of GenreRegistry.h,
    // matching PatternEngineTests.cpp's own fixture-independence precedent).
    const juce::StringArray kUniverse {
        "trap", "uk-drill", "boom-bap", "rnb-neosoul", "house",
        "pop", "lofi", "afrobeats", "reggaeton", "techno"
    };
}

TEST_CASE ("GenreSelectionLogicTests.ExactlyFiveInvariantHoldsAcrossToggleSequences", "[genreselectionlogic]")
{
    juce::StringArray five { "trap", "uk-drill", "boom-bap", "rnb-neosoul", "house" };

    // Scripted mix of already-selected and new ids, cycling through the
    // universe deterministically (no RNG -- this is a pure-logic test).
    for (int i = 0; i < 50; ++i)
    {
        // Alternate between clicking something already in `five` and
        // something from the wider universe not currently in `five`.
        juce::String clicked;
        if (i % 3 == 0)
        {
            clicked = five[i % 5]; // already-selected
        }
        else
        {
            clicked = kUniverse[i % kUniverse.size()];
        }

        five = toggleMainGenre (five, clicked);

        INFO ("step " << i << " clicked " << clicked);
        CHECK (five.size() == 5);

        std::set<juce::String> asSet;
        for (auto& id : five)
            asSet.insert (id);
        CHECK (asSet.size() == 5); // no duplicates

        for (auto& id : five)
            CHECK (kUniverse.contains (id));
    }
}

TEST_CASE ("GenreSelectionLogicTests.AlreadySelectedClickIsNoOp", "[genreselectionlogic]")
{
    juce::StringArray five { "trap", "uk-drill", "boom-bap", "rnb-neosoul", "house" };

    auto result = toggleMainGenre (five, five[2]);

    CHECK (result == five); // unchanged, same order
}

TEST_CASE ("GenreSelectionLogicTests.NewClickEvictsOldestAppendsNewest", "[genreselectionlogic]")
{
    juce::StringArray five { "a", "b", "c", "d", "e" };

    auto result = toggleMainGenre (five, "f");

    juce::StringArray expected { "b", "c", "d", "e", "f" };
    CHECK (result == expected);
}
