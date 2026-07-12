#include "ConstantQAnalysis.h"

#include "cq/CQParameters.h"
#include "cq/CQSpectrogram.h"

#include <algorithm>

namespace
{
    // C1 (32.70 Hz) .. C8 (4186.01 Hz), 36 bins/octave (3 bins/semitone) — matches
    // 03-RESEARCH.md Pipeline Stage 2 / 03-01's CqtEngineSanity. Library defaults
    // kept for q/atomHopFactor/threshold/window/decimator (not set explicitly).
    constexpr double kMinFrequencyHz = 32.70;
    constexpr double kMaxFrequencyHz = 4186.01;
    constexpr int kBinsPerOctave = 36;

    // Chunk size for feeding process(): large enough to keep call overhead low,
    // small enough to leave a natural per-chunk insertion point for an optional
    // cancellation check (plan 03-06 may add a shouldCancel callback parameter
    // here without reshaping this loop).
    constexpr size_t kProcessChunkSize = 16384;
}

CqtFrames computeCqt (const std::vector<float>& samples, double sampleRate)
{
    CqtFrames result;

    if (samples.empty() || sampleRate <= 0.0)
        return result;

    CQParameters params (sampleRate, kMinFrequencyHz, kMaxFrequencyHz, kBinsPerOctave);
    // InterpolateLinear: every cell filled (InterpolateZeros leaves lower-octave
    // cells empty), matching the raw-API convention validated by 03-01's
    // CqtEngineSanity — downstream chroma folding wants fully-populated columns.
    CQSpectrogram cq (params, CQSpectrogram::InterpolateLinear);

    if (! cq.isValid())
        return result;

    std::vector<double> input (samples.size());
    for (size_t i = 0; i < samples.size(); ++i)
        input[i] = (double) samples[i];

    CQSpectrogram::RealBlock columns;

    for (size_t offset = 0; offset < input.size(); offset += kProcessChunkSize)
    {
        const size_t chunkLen = std::min (kProcessChunkSize, input.size() - offset);
        CQSpectrogram::RealSequence chunk (input.begin() + (std::ptrdiff_t) offset,
                                            input.begin() + (std::ptrdiff_t) (offset + chunkLen));
        auto produced = cq.process (chunk);
        columns.insert (columns.end(), produced.begin(), produced.end());
    }

    // Mandatory flush — the tail is silently dropped otherwise (research pitfall:
    // final partial constant-Q block never emitted without getRemainingOutput()).
    auto remaining = cq.getRemainingOutput();
    columns.insert (columns.end(), remaining.begin(), remaining.end());

    const int totalBins = cq.getTotalBins();
    result.binFrequenciesHz.resize ((size_t) totalBins);
    for (int b = 0; b < totalBins; ++b)
        result.binFrequenciesHz[(size_t) b] = cq.getBinFrequency ((double) b);

    result.columnHopSeconds = (double) cq.getColumnHop() / sampleRate;
    result.latencySeconds = (double) cq.getLatency() / sampleRate;

    // Keep every produced column, including any leading latency-only ones, rather
    // than trying to identify/drop a "latency prefix" — frameTimeSeconds' own
    // clamping (hop*i - latency, clamped >= 0) already gives every column a
    // consistent, monotonic timeline position, so no columns need discarding here.
    result.columns = std::move (columns);

    return result;
}
