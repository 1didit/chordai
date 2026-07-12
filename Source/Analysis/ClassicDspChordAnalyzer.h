#pragma once

// Real synchronous pipeline orchestration behind the ChordAnalyzer interface
// (plan 03-06): preprocessForAnalysis -> computeOnsetEnvelope/trackBeats ->
// computeCqt/estimateTuningCents/suppressPercussion/extractChroma ->
// accumulateChroma/detectKey -> decodeChords. No threads, no MessageManager,
// no GUI includes -- pure headless DSP, safe to call from a bare Catch2 test
// or (Phase 4) from a juce::ThreadPoolJob.

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
