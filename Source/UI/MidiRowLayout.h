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

// Pure hit-zone geometry for MidiRowView's regenerate/play/save icons
// (Phase 6.1: horizontal triple, replacing the Phase 6 vertical play/save
// pair -- the third stacked icon does not fit the 28px row height, see
// 06.1-RESEARCH.md Pattern 7). Three 12x12 zones in a single horizontal row
// at the gutter's right edge (regenerate, play, save, left-to-right, 2px
// gaps, 3px right margin, vertically centred) -- unit-testable without a
// Component, same rationale as the functions above. Degenerate (zero/
// negative height, or too-narrow width) gutterBounds returns empty-safe
// (zero-size, non-negative) rects rather than asserting.
namespace MidiRowIconLayout
{
    constexpr int kIconSize = 12;
    constexpr int kIconGap = 2;
    constexpr int kRightMargin = 3;
    constexpr int kRowWidth = kIconSize * 3 + kIconGap * 2; // 40
}

inline juce::Rectangle<int> saveIconRect (juce::Rectangle<int> gutterBounds)
{
    using namespace MidiRowIconLayout;

    if (gutterBounds.getHeight() < kIconSize || gutterBounds.getWidth() < kRowWidth + kRightMargin)
        return {};

    const int right = gutterBounds.getRight() - kRightMargin;
    const int top = gutterBounds.getY() + (gutterBounds.getHeight() - kIconSize) / 2;

    return { right - kIconSize, top, kIconSize, kIconSize };
}

inline juce::Rectangle<int> playIconRect (juce::Rectangle<int> gutterBounds)
{
    using namespace MidiRowIconLayout;

    auto save = saveIconRect (gutterBounds);
    if (save.isEmpty())
        return {};

    return { save.getX() - kIconGap - kIconSize, save.getY(), kIconSize, kIconSize };
}

inline juce::Rectangle<int> regenerateIconRect (juce::Rectangle<int> gutterBounds)
{
    using namespace MidiRowIconLayout;

    auto play = playIconRect (gutterBounds);
    if (play.isEmpty())
        return {};

    return { play.getX() - kIconGap - kIconSize, play.getY(), kIconSize, kIconSize };
}
