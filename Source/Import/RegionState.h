#pragma once

#include <JuceHeader.h>

// Pure helpers that write/read the loaded file path and selected analysis
// region as custom non-parameter properties on an apvts.state ValueTree.
// These properties ride along getStateInformation/setStateInformation's
// XML round-trip automatically since they simply live on the same tree
// as the plugin's real parameters (Phase 7 persistence path).
namespace RegionState
{
    inline constexpr const char* sourceFilePath = "sourceFilePath";
    inline constexpr const char* regionStartSeconds = "regionStartSeconds";
    inline constexpr const char* regionEndSeconds = "regionEndSeconds";

    inline void write (juce::ValueTree& state, const juce::File& file, juce::Range<double> regionSeconds)
    {
        state.setProperty (sourceFilePath, file.getFullPathName(), nullptr);
        state.setProperty (regionStartSeconds, regionSeconds.getStart(), nullptr);
        state.setProperty (regionEndSeconds, regionSeconds.getEnd(), nullptr);
    }

    inline juce::File readSourceFile (const juce::ValueTree& state)
    {
        return juce::File (state.getProperty (sourceFilePath, {}).toString());
    }

    inline juce::Range<double> readRegion (const juce::ValueTree& state)
    {
        if (! state.hasProperty (regionStartSeconds) || ! state.hasProperty (regionEndSeconds))
            return {};

        auto start = (double) state.getProperty (regionStartSeconds, 0.0);
        auto end = (double) state.getProperty (regionEndSeconds, 0.0);
        return { start, end };
    }

    // Normalizes inverted input, clamps to [0, totalLengthSeconds]; an empty
    // (zero-length) input means "whole file" per IMP-03.
    //
    // Takes raw start/end doubles rather than a juce::Range<double>: Range's own
    // constructor already forces end = jmax(start, end) at construction time, so
    // an inverted pair like {8.0, 2.0} silently collapses to {8.0, 8.0} before it
    // could ever reach a Range-typed parameter here — the original "2.0" would
    // already be lost. Raw doubles let this function observe and normalize a
    // genuinely inverted selection (e.g. a right-to-left drag) itself.
    inline juce::Range<double> clampRegion (double a, double b, double totalLengthSeconds)
    {
        if (a == b)
            return { 0.0, totalLengthSeconds };

        auto start = juce::jlimit (0.0, totalLengthSeconds, juce::jmin (a, b));
        auto end = juce::jlimit (0.0, totalLengthSeconds, juce::jmax (a, b));

        return { start, end };
    }

    inline juce::Range<double> clampRegion (juce::Range<double> regionSeconds, double totalLengthSeconds)
    {
        return clampRegion (regionSeconds.getStart(), regionSeconds.getEnd(), totalLengthSeconds);
    }
}
