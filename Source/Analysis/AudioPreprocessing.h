#pragma once

// FROZEN CONTRACT — do not change shape without updating Phase 4 plans

#include <JuceHeader.h>
#include <vector>

struct PreprocessedAudio
{
    std::vector<float> onsetSamples;    // mono @ 8000 Hz (Ellis 2007 path)
    std::vector<float> chromaSamples;   // mono @ 11025 Hz (CQT path)
    double onsetRate  = 8000.0;
    double chromaRate = 11025.0;
    double regionStartSeconds = 0.0;    // absolute source-file time of sample 0
};

PreprocessedAudio preprocessForAnalysis (const juce::AudioBuffer<float>& audio,
                                          double sourceSampleRate,
                                          juce::Range<double> regionSeconds);
