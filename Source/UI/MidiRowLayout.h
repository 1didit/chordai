#pragma once

#include <JuceHeader.h>

// Pure, stateless beat<->pixel / pitch<->pixel conversion for the MIDI-set
// rows (MidiRowView/MidiSetsPanel), same extraction rationale as
// WaveformMath.h: unit-testable without a juce::Component. No Component/GUI
// includes beyond juce_core types.

// Linear beat position -> x pixel across a row's note area. All 5 rows share
// one totalBeats (the panel's shared time axis) so patterns stay visually
// comparable across styles. Mirrors WaveformMath's zero-length guard.
inline float beatsToX (double beats, double totalBeats, int width)
{
    if (totalBeats <= 0.0)
        return 0.0f;

    return (float) ((beats / totalBeats) * (double) width);
}

// Note pixel width with a visibility floor -- a very short note (e.g. a
// 0.25-beat house stab) in a very long row (hundreds of beats) would
// otherwise round to a sub-pixel, invisible rectangle.
inline float noteWidthPx (double lengthBeats, double totalBeats, int width)
{
    if (totalBeats <= 0.0)
        return 1.0f;

    return juce::jmax (1.0f, (float) ((lengthBeats / totalBeats) * (double) width));
}

// Inverted pitch -> y pixel within a row's note area: higher pitch = higher
// on screen (smaller y), matching standard piano-roll convention.
// minPitch == maxPitch (a single-pitch row) is guarded to the vertical
// centre rather than dividing by a zero span. Result is always clamped to
// [0, height).
inline float pitchToY (int pitch, int minPitch, int maxPitch, int height)
{
    if (minPitch >= maxPitch)
        return (float) height * 0.5f;

    auto span = (float) (maxPitch - minPitch);
    auto norm = (float) (pitch - minPitch) / span; // 0 at minPitch, 1 at maxPitch
    auto y = (float) height * (1.0f - norm);         // higher pitch -> smaller y

    return juce::jlimit (0.0f, (float) height - 1.0f, y);
}

// Pure hit-zone geometry for MidiRowView's play/save icons (Phase 6, RED --
// declarations only, bodies land in the GREEN commit).
juce::Rectangle<int> playIconRect (juce::Rectangle<int> gutterBounds);
juce::Rectangle<int> saveIconRect (juce::Rectangle<int> gutterBounds);
