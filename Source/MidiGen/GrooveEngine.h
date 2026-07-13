#pragma once

// Tick-exact swing/groove primitives (06.1-RESEARCH.md Pattern 5). Header-
// only, pure, no JUCE dependency (kept as free of JUCE as NoteEvent.h --
// only <cmath>/<vector> needed).
//
// MidiFileWriter::kTicksPerQuarterNote = 960 = 2^6*3*5 is the frozen export
// resolution. Every constant/offset below is a beat-fraction whose
// denominator divides 960, so it lands on an exact integer tick with zero
// rounding -- NO decimal swing literals (Pitfall C).

#include <cmath>
#include <vector>

inline constexpr double kSwingStraight = 0.5;
inline constexpr double kSwingLight    = 7.0 / 12.0;  // ~58%, boom bap/lofi
inline constexpr double kSwingTriplet  = 2.0 / 3.0;   // true triplet, UK drill/trap

// Delays the SECOND sixteenth of each eighth-note pair by swingRatio of the
// eighth's span. sixteenthIndexInBeat: 0-3 (four 16ths per beat). Straight
// (kSwingStraight) reproduces the unswung grid exactly (0, 0.25, 0.5, 0.75).
inline double swingSixteenthOffsetBeats (int sixteenthIndexInBeat, double swingRatio)
{
    const int pairIndex = sixteenthIndexInBeat / 2;         // which 8th-note pair (0 or 1)
    const bool isSecondOfPair = (sixteenthIndexInBeat % 2) == 1;
    const double pairStartBeats = pairIndex * 0.5;
    return pairStartBeats + (isSecondOfPair ? swingRatio * 0.5 : 0.0);
}

// Tiles a span-relative onset list across [0, segmentLengthBeats), emitting
// k*spanBeats + onset for k = 0,1,2... while < segmentLengthBeats. Takes
// primitive args (NOT RhythmVariant) so GrooveEngine stays independent of
// GenreRegistry. Guards: spanBeats <= 0, segmentLengthBeats <= 0, or empty
// input all return {} (no infinite loop).
inline std::vector<double> tileOnsets (const std::vector<double>& onsetsBeats, double spanBeats, double segmentLengthBeats)
{
    std::vector<double> tiled;
    if (onsetsBeats.empty() || spanBeats <= 0.0 || segmentLengthBeats <= 0.0)
        return tiled;

    for (double spanStart = 0.0; spanStart < segmentLengthBeats; spanStart += spanBeats)
    {
        for (double onset : onsetsBeats)
        {
            const double beat = spanStart + onset;
            if (beat < segmentLengthBeats)
                tiled.push_back (beat);
        }
    }

    return tiled;
}

// std::fabs(std::fmod(beats * ticksPerQuarterNote, 1.0)) < 1e-9, also
// accepting the near-1.0 fmod-wraparound case.
inline bool isTickExact (double beats, int ticksPerQuarterNote)
{
    const double ticks = beats * (double) ticksPerQuarterNote;
    const double frac = std::fabs (std::fmod (ticks, 1.0));
    return frac < 1e-9 || frac > 1.0 - 1e-9;
}
