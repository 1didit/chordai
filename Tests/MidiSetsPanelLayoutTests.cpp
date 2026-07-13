#include <JuceHeader.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Source/UI/MidiRowLayout.h"

TEST_CASE ("MidiSetsPanelLayoutTests.BeatsToXMapsLinearly", "[midisetspanellayout]")
{
    CHECK (beatsToX (0.0, 16.0, 320) == Catch::Approx (0.0f));
    CHECK (beatsToX (8.0, 16.0, 320) == Catch::Approx (160.0f));
    CHECK (beatsToX (16.0, 16.0, 320) == Catch::Approx (320.0f));

    // Zero-length guard, mirrors WaveformMath.h's own totalLength <= 0 guard.
    CHECK (beatsToX (8.0, 0.0, 320) == Catch::Approx (0.0f));
    CHECK (beatsToX (8.0, -4.0, 320) == Catch::Approx (0.0f));
}

TEST_CASE ("MidiSetsPanelLayoutTests.PitchToYInvertsAndSpans", "[midisetspanellayout]")
{
    constexpr int height = 28;
    constexpr int minPitch = 40;
    constexpr int maxPitch = 64;

    // Higher pitch = higher on screen (smaller y): maxPitch -> top edge,
    // minPitch -> bottom edge.
    auto topY = pitchToY (maxPitch, minPitch, maxPitch, height);
    auto bottomY = pitchToY (minPitch, minPitch, maxPitch, height);

    CHECK (topY == Catch::Approx (0.0f));
    CHECK (bottomY == Catch::Approx ((float) height - 1.0f));
    CHECK (topY < bottomY);

    // Single-pitch row guard -> vertical centre.
    CHECK (pitchToY (60, 60, 60, height) == Catch::Approx ((float) height * 0.5f));

    // Results always within [0, height) across the whole pitch span.
    for (int p = minPitch; p <= maxPitch; ++p)
    {
        auto y = pitchToY (p, minPitch, maxPitch, height);
        CHECK (y >= 0.0f);
        CHECK (y < (float) height);
    }
}

TEST_CASE ("MidiSetsPanelLayoutTests.NoteRectNonDegenerate", "[midisetspanellayout]")
{
    // A 0.25-beat house stab in a 600-beat row at width 320 would compute to
    // a sub-pixel width without the floor -- stabs must stay visible on long
    // tracks.
    auto w = noteWidthPx (0.25, 600.0, 320);
    CHECK (w >= 1.0f);
}

TEST_CASE ("MidiSetsPanelLayoutTests.IconRectsFitInsideGutterAndDoNotOverlap", "[midisetspanellayout]")
{
    // Matches MidiRowView's real gutter/row shape: kGutterWidth=92, row
    // height 28 (140px band / 5 rows).
    juce::Rectangle<int> gutter (0, 0, 92, 28);

    auto play = playIconRect (gutter);
    auto save = saveIconRect (gutter);

    // Both fully inside the gutter.
    CHECK (gutter.contains (play));
    CHECK (gutter.contains (save));

    // Each at least a 10x10 usable hit target.
    CHECK (play.getWidth() >= 10);
    CHECK (play.getHeight() >= 10);
    CHECK (save.getWidth() >= 10);
    CHECK (save.getHeight() >= 10);

    // Non-overlapping.
    CHECK_FALSE (play.intersects (save));

    // Right-aligned: label keeps >= 55px of text width to the left of the
    // icon zones' left edge.
    auto iconsLeft = juce::jmin (play.getX(), save.getX());
    CHECK (iconsLeft >= 55);
}

TEST_CASE ("MidiSetsPanelLayoutTests.IconRectsAreStableAndDegenerateSafe", "[midisetspanellayout]")
{
    juce::Rectangle<int> gutter (0, 0, 92, 28);

    // Pure/deterministic: same input -> same output across calls.
    CHECK (playIconRect (gutter) == playIconRect (gutter));
    CHECK (saveIconRect (gutter) == saveIconRect (gutter));

    // Degenerate zero-height gutter must return empty-safe (non-negative
    // size) rects, never negative width/height.
    juce::Rectangle<int> degenerate (0, 0, 92, 0);
    auto playDeg = playIconRect (degenerate);
    auto saveDeg = saveIconRect (degenerate);

    CHECK (playDeg.getWidth() >= 0);
    CHECK (playDeg.getHeight() >= 0);
    CHECK (saveDeg.getWidth() >= 0);
    CHECK (saveDeg.getHeight() >= 0);
}
