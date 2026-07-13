#include <JuceHeader.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Source/UI/ChordTimelineView.h"
#include "Source/UI/WaveformMath.h"

TEST_CASE ("ChordTimelineLayoutTests.LabelCollisionGuardDefaultThreshold", "[chordtimelinelayout]")
{
    CHECK_FALSE (shouldDrawLabel (23.9f));
    CHECK (shouldDrawLabel (24.0f));
}

TEST_CASE ("ChordTimelineLayoutTests.LabelCollisionGuardCustomThreshold", "[chordtimelinelayout]")
{
    CHECK_FALSE (shouldDrawLabel (30.0f, 40.0f));
    CHECK (shouldDrawLabel (40.0f, 40.0f));
}

TEST_CASE ("ChordTimelineLayoutTests.SegmentPixelSpan", "[chordtimelinelayout]")
{
    // Complements WaveformRegionTests.PixelTimeConversion — sanity-checks that
    // ChordTimelineView's own timeToX reuse produces the expected segment span,
    // not a reimplementation.
    juce::Range<double> visibleRange (0.0, 100.0);
    constexpr int width = 800;

    auto x0 = timeToX (25.0, visibleRange, width);
    auto x1 = timeToX (50.0, visibleRange, width);

    CHECK (x0 == Catch::Approx (200.0f));
    CHECK (x1 == Catch::Approx (400.0f));
}
