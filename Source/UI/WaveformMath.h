#pragma once

#include <JuceHeader.h>

// Pure, stateless pixel<->time conversion mirroring the vendored
// AudioPlaybackDemo pattern (examples/Audio/AudioPlaybackDemo.h), extracted
// as free functions so they are unit-testable without a juce::Component.
// No Component/GUI includes beyond juce_core types (juce::Range).

inline float timeToX (double time, juce::Range<double> visibleRange, int width)
{
    auto length = visibleRange.getLength();
    if (length <= 0.0)
        return 0.0f;

    return (float) (((time - visibleRange.getStart()) / length) * (double) width);
}

inline double xToTime (float x, juce::Range<double> visibleRange, int width)
{
    auto length = visibleRange.getLength();
    if (length <= 0.0 || width <= 0)
        return visibleRange.getStart();

    return visibleRange.getStart() + ((double) x / (double) width) * length;
}
