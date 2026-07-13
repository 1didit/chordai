#include <JuceHeader.h>
#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/GenreState.h"

TEST_CASE ("GenreStateTests.RoundTripsThroughValueTreeXml", "[genrestate]")
{
    juce::ValueTree state ("PARAMETERS");
    juce::StringArray five { "uk-drill", "trap", "house", "boom-bap", "rnb-neosoul" };

    GenreState::write (state, "uk-drill", five);

    auto xml = state.createXml();
    REQUIRE (xml != nullptr);

    auto roundTripped = juce::ValueTree::fromXml (*xml);
    REQUIRE (roundTripped.isValid());

    CHECK (GenreState::readActiveGenre (roundTripped, "trap") == "uk-drill");
    CHECK (GenreState::readMainGenres (roundTripped, juce::StringArray {}) == five);
}

TEST_CASE ("GenreStateTests.MissingPropsReturnFallbacks", "[genrestate]")
{
    juce::ValueTree state ("PARAMETERS");

    CHECK (GenreState::readActiveGenre (state, "trap") == "trap");

    juce::StringArray fallback { "trap", "uk-drill", "boom-bap", "rnb-neosoul", "house" };
    CHECK (GenreState::readMainGenres (state, fallback) == fallback);
}

TEST_CASE ("GenreStateTests.CorruptMainGenresFallsBack", "[genrestate]")
{
    juce::ValueTree state ("PARAMETERS");
    state.setProperty (GenreState::mainGenreIds, "a,b,c", nullptr); // only 3 tokens, never 5

    juce::StringArray fallback { "trap", "uk-drill", "boom-bap", "rnb-neosoul", "house" };
    CHECK (GenreState::readMainGenres (state, fallback) == fallback);
}
