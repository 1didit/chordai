#pragma once

// Pure helpers that write/read the active genre + main-5 genre selection as
// custom non-parameter properties on an apvts.state ValueTree. Mirrors
// Source/Import/RegionState.h exactly (06.1-RESEARCH.md Pattern 9) -- rides
// the existing getStateInformation/setStateInformation XML round-trip for
// free, Phase-7-persistence-ready.

#include <JuceHeader.h>

namespace GenreState
{
    inline constexpr const char* activeGenreId = "activeGenreId";
    inline constexpr const char* mainGenreIds = "mainGenreIds"; // comma-joined, always exactly 5

    inline void write (juce::ValueTree& state, const juce::String& active, const juce::StringArray& mainFive)
    {
        state.setProperty (activeGenreId, active, nullptr);
        state.setProperty (mainGenreIds, mainFive.joinIntoString (","), nullptr);
    }

    inline juce::String readActiveGenre (const juce::ValueTree& state, const juce::String& fallback)
    {
        return state.hasProperty (activeGenreId) ? state.getProperty (activeGenreId).toString() : fallback;
    }

    inline juce::StringArray readMainGenres (const juce::ValueTree& state, const juce::StringArray& fallback)
    {
        if (! state.hasProperty (mainGenreIds))
            return fallback;
        auto arr = juce::StringArray::fromTokens (state.getProperty (mainGenreIds).toString(), ",", "");
        return arr.size() == 5 ? arr : fallback; // defensive -- corrupt/foreign state never crashes
    }
}
