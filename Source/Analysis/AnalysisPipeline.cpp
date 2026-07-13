#include "AnalysisPipeline.h"

#include "ClassicDspChordAnalyzer.h"

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
    // Adapts this job's cooperative-cancellation flag to ChordAnalyzer's
    // abstract CancelToken -- the one-line adapter named verbatim in
    // ChordAnalyzer.h's own doc comment.
    struct Adapter : ChordAnalyzer::CancelToken
    {
        ThreadPoolJob& job;
        explicit Adapter (ThreadPoolJob& j) : job (j) {}
        bool shouldCancel() const override { return job.shouldExit(); }
    } cancelToken (*this);

    ClassicDspChordAnalyzer analyzer; // stateless, local -- no need to hold on the processor

    // Snapshot generation/onProgress by value into the lambda so the
    // background thread never touches processor state directly; the
    // MessageManager::callAsync closure runs later, on the message thread.
    ChordAnalyzer::ProgressCallback progress = [this] (double fraction, const juce::String& stage)
    {
        auto gen = generation;
        auto cb = onProgress;
        juce::MessageManager::callAsync ([cb, gen, fraction, stage]
        {
            if (cb)
                cb (gen, fraction, stage);
        });
    };

    auto result = std::make_shared<const AnalysisResult> (
        analyzer.analyse (audio->buffer, audio->sampleRate, region, progress, cancelToken));

    auto gen = generation;
    auto cb = onDone;
    juce::MessageManager::callAsync ([cb, gen, result]
    {
        if (cb)
            cb (gen, result);
    });

    return JobStatus::jobHasFinished;
}
