#pragma once

#include <JuceHeader.h>

// Pure, stateless "exactly 5 main genres" toggle logic (06.1-RESEARCH.md
// Pattern 8). No Component/GUI includes beyond juce_core types -- unit
// testable without any UI, same extraction rationale as MidiRowLayout.h.
//
// FIFO auto-swap is the locked interaction model (06.1-RESEARCH.md Open
// Question 2, resolved): the invariant (exactly 5, no duplicates) holds true
// at every instant, never a transient 4-or-6 state, no separate Done-gating
// step.
//
// currentFive: exactly 5 ids, oldest-selected at index 0 (insertion order).
// clicked: the id the user just clicked in the "all genres" list.
// Already-selected click -> no-op (returns currentFive unchanged, same
// order). New-genre click -> evicts index 0 (oldest), appends clicked at the
// end (newest).
inline juce::StringArray toggleMainGenre (juce::StringArray currentFive, const juce::String& clicked)
{
    jassert (currentFive.size() == 5);

    if (currentFive.contains (clicked))
        return currentFive;

    currentFive.remove (0);
    currentFive.add (clicked);
    return currentFive;
}
