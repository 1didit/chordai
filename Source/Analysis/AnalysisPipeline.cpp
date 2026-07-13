#include "AnalysisPipeline.h"

AnalysisPipeline::AnalysisPipeline (std::shared_ptr<const LoadedAudio> audioIn,
                                     juce::Range<double> regionIn,
                                     uint64_t generationIn,
                                     ProgressCallback onProgressIn,
                                     CompletionCallback onDoneIn)
    : juce::ThreadPoolJob ("ChordAnalysis"),
      audio (std::move (audioIn)),
      region (regionIn),
      generation (generationIn),
      onProgress (std::move (onProgressIn)),
      onDone (std::move (onDoneIn))
{
}

AnalysisPipeline::JobStatus AnalysisPipeline::runJob()
{
    // Wave 0 skeleton (Task 1, RED): no real analysis work yet. Task 2 wires
    // in the CancelToken adapter + ClassicDspChordAnalyzer::analyse() call +
    // callAsync progress/completion marshalling per 04-RESEARCH.md Pattern 1.
    return JobStatus::jobHasFinished;
}
