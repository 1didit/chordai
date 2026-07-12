#pragma once

// Owned by plan 03-06. Skeleton created in 03-01 for build wiring only.

#include "ChordAnalyzer.h"

class ClassicDspChordAnalyzer : public ChordAnalyzer
{
public:
    AnalysisResult analyse (const juce::AudioBuffer<float>& audio,
                             double sampleRate,
                             juce::Range<double> regionSeconds,
                             const ProgressCallback& onProgress,
                             const CancelToken& cancelToken) override;
};
