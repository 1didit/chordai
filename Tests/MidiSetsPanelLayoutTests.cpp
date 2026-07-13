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

TEST_CASE ("MidiSetsPanelLayoutTests.ThreeIconRowFitsInsideGutterAndDoesNotOverlap", "[midisetspanellayout]")
{
    // Matches MidiRowView's real gutter/row shape: kGutterWidth=92, row
    // height 28 (140px band / 5 rows). 06.1-06: vertical play/save pair ->
    // horizontal regenerate/play/save triple (Pattern 7).
    juce::Rectangle<int> gutter (0, 0, 92, 28);

    auto regen = regenerateIconRect (gutter);
    auto play = playIconRect (gutter);
    auto save = saveIconRect (gutter);

    // All non-empty.
    CHECK_FALSE (regen.isEmpty());
    CHECK_FALSE (play.isEmpty());
    CHECK_FALSE (save.isEmpty());

    // Each at least a 10x10 usable hit target (actual size is 12x12).
    CHECK (regen.getWidth() >= 10);
    CHECK (regen.getHeight() >= 10);
    CHECK (play.getWidth() >= 10);
    CHECK (play.getHeight() >= 10);
    CHECK (save.getWidth() >= 10);
    CHECK (save.getHeight() >= 10);

    // All fully inside the gutter.
    CHECK (gutter.contains (regen));
    CHECK (gutter.contains (play));
    CHECK (gutter.contains (save));

    // Pairwise non-intersecting.
    CHECK_FALSE (regen.intersects (play));
    CHECK_FALSE (play.intersects (save));
    CHECK_FALSE (regen.intersects (save));

    // Ordered left-to-right: regenerate < play < save.
    CHECK (regen.getX() < play.getX());
    CHECK (play.getX() < save.getX());

    // Right-aligned group: save sits kRightMargin (3px) from the gutter's
    // right edge.
    CHECK (save.getRight() == gutter.getRight() - 3);

    // Vertically centred (all three share the same y/height).
    CHECK (regen.getY() == play.getY());
    CHECK (play.getY() == save.getY());
    CHECK (regen.getHeight() == play.getHeight());
    CHECK (play.getHeight() == save.getHeight());
}

TEST_CASE ("MidiSetsPanelLayoutTests.ThreeIconRowDegenerateSafe", "[midisetspanellayout]")
{
    juce::Rectangle<int> gutter (0, 0, 92, 28);

    // Pure/deterministic: same input -> same output across calls.
    CHECK (regenerateIconRect (gutter) == regenerateIconRect (gutter));
    CHECK (playIconRect (gutter) == playIconRect (gutter));
    CHECK (saveIconRect (gutter) == saveIconRect (gutter));

    // Degenerate zero-height gutter -> all three rects empty (no
    // asserts/negative sizes).
    juce::Rectangle<int> zeroHeight (0, 0, 92, 0);
    CHECK (regenerateIconRect (zeroHeight).isEmpty());
    CHECK (playIconRect (zeroHeight).isEmpty());
    CHECK (saveIconRect (zeroHeight).isEmpty());

    // Degenerate too-narrow gutter (width < 40 + 3 right margin) -> all
    // three rects empty.
    juce::Rectangle<int> tooNarrow (0, 0, 42, 28);
    CHECK (regenerateIconRect (tooNarrow).isEmpty());
    CHECK (playIconRect (tooNarrow).isEmpty());
    CHECK (saveIconRect (tooNarrow).isEmpty());

    // No negative sizes even in the degenerate cases.
    CHECK (regenerateIconRect (zeroHeight).getWidth() >= 0);
    CHECK (regenerateIconRect (zeroHeight).getHeight() >= 0);
}
