#pragma once

// ThreadPoolJob wrapping the frozen ChordAnalyzer interface (Phase 3,
// ClassicDspChordAnalyzer::analyse). Owns an immutable snapshot captured at
// submit time -- shared_ptr<const LoadedAudio>, a region, and a generation id
// -- no live references into PluginProcessor state, mirroring
// Source/Import/AudioFileLoader.h's AudioFileLoadJob discipline.
//
// runJob() IS the background thread: it adapts ThreadPoolJob::shouldExit()
// to ChordAnalyzer::CancelToken (per ChordAnalyzer.h's own doc comment),
// calls analyse() synchronously, then marshals both progress and the final
// result back to the message thread via juce::MessageManager::callAsync,
// tagging both with the captured generation so the caller
// (PluginProcessor::triggerAnalysis) can discard superseded callbacks.

#include <JuceHeader.h>

#include "AnalysisResult.h"
#include "../Import/LoadedAudio.h"

class AnalysisPipeline : public juce::ThreadPoolJob
{
public:
    using ProgressCallback = std::function<void (uint64_t generation, double fractionComplete, const juce::String& stage)>;
    using CompletionCallback = std::function<void (uint64_t generation, std::shared_ptr<const AnalysisResult>)>;

    AnalysisPipeline (std::shared_ptr<const LoadedAudio> audioIn,
                       juce::Range<double> regionIn,
                       uint64_t generationIn,
                       ProgressCallback onProgressIn,
                       CompletionCallback onDoneIn);

    JobStatus runJob() override;

private:
    std::shared_ptr<const LoadedAudio> audio;
    juce::Range<double> region;
    uint64_t generation;
    ProgressCallback onProgress;
    CompletionCallback onDone;
};
