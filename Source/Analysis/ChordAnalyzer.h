#pragma once

// FROZEN CONTRACT — do not change shape without updating Phase 4 plans

#include <JuceHeader.h>
#include "AnalysisResult.h"

class ChordAnalyzer
{
public:
    virtual ~ChordAnalyzer() = default;

    // Abstract cancellation contract: decouples the interface from any specific
    // threading primitive. Phase 3 test harness passes a trivial always-false
    // token; Phase 4's AnalysisPipeline (juce::ThreadPoolJob) implements a
    // one-line adapter: `bool shouldCancel() const override { return job.shouldExit(); }`
    struct CancelToken
    {
        virtual ~CancelToken() = default;
        virtual bool shouldCancel() const = 0;
    };

    // fractionComplete in [0,1]; stage is a short label ("decoding","chroma","beat","key","chords")
    // for Phase 4's progress UI. May be called from whatever thread analyse() runs on —
    // caller (Phase 4) is responsible for marshalling to the message thread.
    using ProgressCallback = std::function<void (double fractionComplete, const juce::String& stage)>;

    // Synchronous. audio is already decoded (LoadedAudio::buffer); analyse() does its own
    // mono downmix + resample internally. Returns a fully-populated AnalysisResult, or a
    // result with chords.empty() if cancelled mid-run (never throws, never asserts on bad input
    // — matches existing loadAudioFileSync() convention in Source/Import/AudioFileLoader.h).
    virtual AnalysisResult analyse (const juce::AudioBuffer<float>& audio,
                                     double sampleRate,
                                     juce::Range<double> regionSeconds,
                                     const ProgressCallback& onProgress,
                                     const CancelToken& cancelToken) = 0;
};
