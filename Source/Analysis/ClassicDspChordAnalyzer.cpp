#include "ClassicDspChordAnalyzer.h"

#include "AudioPreprocessing.h"
#include "OnsetEnvelope.h"
#include "TempoBeatTracker.h"
#include "ConstantQAnalysis.h"
#include "TuningEstimator.h"
#include "HarmonicPercussiveFilter.h"
#include "ChromaExtractor.h"
#include "KeyDetector.h"
#include "ChordDecoder.h"

namespace
{
    void reportProgress (const ChordAnalyzer::ProgressCallback& onProgress, double fraction, const juce::String& stage)
    {
        if (onProgress)
            onProgress (fraction, stage);
    }

    // Mirrors AudioPreprocessing.cpp's own region clamp (empty/inverted region
    // collapses to "whole buffer") exactly, so AnalysisResult::analyzedRegionSeconds
    // matches what preprocessForAnalysis actually analyzed. Duplicated here
    // (rather than widening PreprocessedAudio to carry a region end) because
    // PreprocessedAudio is a FROZEN contract (03-02).
    juce::Range<double> clampRegion (const juce::AudioBuffer<float>& audio, double sourceSampleRate,
                                      juce::Range<double> regionSeconds)
    {
        const double totalLengthSeconds = sourceSampleRate > 0.0
            ? (double) audio.getNumSamples() / sourceSampleRate
            : 0.0;

        double start = regionSeconds.getStart();
        double end = regionSeconds.getEnd();
        if (start == end || end < start)
        {
            start = 0.0;
            end = totalLengthSeconds;
        }
        start = juce::jlimit (0.0, totalLengthSeconds, start);
        end = juce::jlimit (0.0, totalLengthSeconds, end);
        if (end <= start)
        {
            start = 0.0;
            end = totalLengthSeconds;
        }

        return { start, end };
    }
}

// Pure synchronous orchestration: preprocess -> beat -> chroma -> key -> chords.
// NO threads, NO juce::MessageManager, NO GUI includes anywhere in this file --
// safe to call from a bare Catch2 test (proves ANL-06's swappable-interface
// contract) or, in Phase 4, from inside a juce::ThreadPoolJob::runJob().
//
// cancelToken.shouldCancel() is checked before any work starts and again after
// every stage (plus once more inside computeCqt's own per-chunk hook, since
// chroma extraction is the longest-running stage) so cancellation latency
// never depends on waiting out the whole pipeline. On cancel: wasCancelled is
// set, chords stay empty, and whatever AnalysisResult fields were already
// populated are left as-is (never throws, matches loadAudioFileSync's
// never-throw convention -- see Source/Import/AudioFileLoader.h).
AnalysisResult ClassicDspChordAnalyzer::analyse (const juce::AudioBuffer<float>& audio,
                                                  double sampleRate,
                                                  juce::Range<double> regionSeconds,
                                                  const ProgressCallback& onProgress,
                                                  const CancelToken& cancelToken)
{
    AnalysisResult result;
    result.sampleRate = sampleRate;
    result.analyzedRegionSeconds = clampRegion (audio, sampleRate, regionSeconds);

    if (cancelToken.shouldCancel())
    {
        result.wasCancelled = true;
        return result;
    }

    // Stage 1: decode/preprocess -- mono downmix + dual-rate resample (8kHz
    // onset path / 11025Hz chroma path) + region clamp.
    auto preprocessed = preprocessForAnalysis (audio, sampleRate, regionSeconds);
    const double regionStart = preprocessed.regionStartSeconds;
    reportProgress (onProgress, 0.10, "decoding");

    if (cancelToken.shouldCancel())
    {
        result.wasCancelled = true;
        return result;
    }

    // Stage 2: beat tracking (Ellis 2007 onset envelope -> tempo/beat DP backtrace).
    auto onset = computeOnsetEnvelope (preprocessed.onsetSamples);
    auto beatGrid = trackBeats (onset);
    reportProgress (onProgress, 0.35, "beat");

    if (cancelToken.shouldCancel())
    {
        result.wasCancelled = true;
        return result;
    }

    // Stage 3: chroma -- CQT (cancellable per feed-chunk, this is the longest
    // stage) -> tuning estimate -> percussion suppression -> chroma fold.
    auto cqt = computeCqt (preprocessed.chromaSamples, preprocessed.chromaRate,
                            [&cancelToken] { return cancelToken.shouldCancel(); });

    if (cancelToken.shouldCancel())
    {
        result.wasCancelled = true;
        return result;
    }

    const double tuningCents = estimateTuningCents (cqt);
    suppressPercussion (cqt, kHpssKernelSeconds);
    auto chroma = extractChroma (cqt, tuningCents);
    reportProgress (onProgress, 0.70, "chroma");

    if (cancelToken.shouldCancel())
    {
        result.wasCancelled = true;
        return result;
    }

    // Stage 4: key detection (global harmonic-chroma accumulation over the region).
    auto accumulatedChroma = accumulateChroma (chroma);
    result.key = detectKey (accumulatedChroma);
    reportProgress (onProgress, 0.80, "key");

    if (cancelToken.shouldCancel())
    {
        result.wasCancelled = true;
        return result;
    }

    // Stage 5: chord decoding (beat-sync chroma -> Viterbi -> segment merge).
    auto chordSegments = decodeChords (chroma, beatGrid);

    if (cancelToken.shouldCancel())
    {
        result.wasCancelled = true;
        return result;
    }

    // Always emit a final (1.0, "chords") on success (only reached once every
    // stage above has completed without a cancellation).
    reportProgress (onProgress, 1.0, "chords");

    // TIME BASE: BeatGrid/ChromaSequence/ChordSegment are region-relative
    // (relative to the analyzed region's own start); shift beat times and
    // chord segment boundaries by regionStart so AnalysisResult carries
    // ABSOLUTE source-file time -- Phase 4 overlays these directly on the
    // (full-file) waveform without needing to know the analyzed region.
    result.bpm = beatGrid.bpm;
    result.beatTimesSeconds.reserve (beatGrid.beatTimesSeconds.size());
    for (double t : beatGrid.beatTimesSeconds)
        result.beatTimesSeconds.push_back (t + regionStart);
    result.barStartBeatIndices = beatGrid.barStartBeatIndices;

    result.chords.reserve (chordSegments.size());
    for (auto segment : chordSegments)
    {
        segment.startSeconds += regionStart;
        segment.endSeconds += regionStart;
        result.chords.push_back (segment);
    }

    return result;
}
