#include "AudioPreprocessing.h"

// Stub implementation — real dual-rate downmix/resample lands in Task 3 of this plan.
PreprocessedAudio preprocessForAnalysis (const juce::AudioBuffer<float>& /*audio*/,
                                          double /*sourceSampleRate*/,
                                          juce::Range<double> /*regionSeconds*/)
{
    return {};
}
