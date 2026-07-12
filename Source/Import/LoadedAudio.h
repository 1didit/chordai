#pragma once

#include <JuceHeader.h>

// Immutable value type produced by a successful decode. Native channel count
// and sample rate — no downmix/resample happens here. Mutable selection state
// (the analysis region) intentionally does NOT live inside this shared const
// object; it lives on the processor instead (see RegionState / PluginProcessor).
struct LoadedAudio
{
    juce::File sourceFile;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
    double lengthSeconds = 0.0;
};
