#pragma once

// Header-only: nearestOctaveNote() register-aware voice-leading helper, used
// by the R&B/Neo-soul generator to place each successive chord's extension
// tones close to the previous chord's voicing (05-RESEARCH.md Pattern 2/
// "R&B voice-leading between consecutive chords").

#include <JuceHeader.h>

#include <cmath>

// Places `noteClass` (a 0-11 pitch class, already reduced mod 12) in the
// octave nearest to `previousNote`, clamped into [registerLow, registerHigh].
// Deterministic: depends only on its three inputs, no history/state beyond
// the single previous note the caller passes in (see Pitfall 3 for this
// hard-clamp approximation's known voice-crossing limitation).
inline int nearestOctaveNote (int noteClass, int previousNote, int registerLow = 48, int registerHigh = 72)
{
    int best = noteClass;
    int bestDist = std::abs (best - previousNote);
    for (int candidate = ((noteClass % 12) + 12) % 12; candidate <= 127; candidate += 12)
    {
        int dist = std::abs (candidate - previousNote);
        if (dist < bestDist) { bestDist = dist; best = candidate; }
    }
    return juce::jlimit (registerLow, registerHigh, best);
}
