#include "ClassicDspChordAnalyzer.h"

// Stub implementation — real pipeline orchestration owned by plan 03-06.
AnalysisResult ClassicDspChordAnalyzer::analyse (const juce::AudioBuffer<float>& /*audio*/,
                                                  double /*sampleRate*/,
                                                  juce::Range<double> /*regionSeconds*/,
                                                  const ProgressCallback& /*onProgress*/,
                                                  const CancelToken& /*cancelToken*/)
{
    return {};
}
