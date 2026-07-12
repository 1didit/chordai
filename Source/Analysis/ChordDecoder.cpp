#include "ChordDecoder.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Median inter-beat interval, used to extend the last beat's averaging
    // window past the end of the beat grid (there is no beat[last+1] to bound it).
    double medianInterBeatInterval (const std::vector<double>& beatTimesSeconds)
    {
        if (beatTimesSeconds.size() < 2)
            return 0.5; // arbitrary fallback -- unused when < 2 beats (decodeChords guards this case)

        std::vector<double> intervals;
        intervals.reserve (beatTimesSeconds.size() - 1);
        for (size_t i = 1; i < beatTimesSeconds.size(); ++i)
            intervals.push_back (beatTimesSeconds[i] - beatTimesSeconds[i - 1]);

        std::sort (intervals.begin(), intervals.end());
        const size_t mid = intervals.size() / 2;
        return (intervals.size() % 2 == 0)
            ? 0.5 * (intervals[mid - 1] + intervals[mid])
            : intervals[mid];
    }

    float cosineSimilarity (const std::array<float, 12>& a, const std::array<float, 12>& b)
    {
        float dot = 0.0f, normA = 0.0f, normB = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            dot += a[(size_t) i] * b[(size_t) i];
            normA += a[(size_t) i] * a[(size_t) i];
            normB += b[(size_t) i] * b[(size_t) i];
        }
        if (normA <= 0.0f || normB <= 0.0f)
            return 0.0f; // guard zero vectors -- silent/degenerate beats never NaN

        return dot / (std::sqrt (normA) * std::sqrt (normB));
    }
}

std::vector<BeatChroma> computeBeatSyncChroma (const ChromaSequence& chroma, const BeatGrid& beats)
{
    std::vector<BeatChroma> result;
    const auto& beatTimes = beats.beatTimesSeconds;
    if (beatTimes.empty())
        return result;

    const double medianIbi = medianInterBeatInterval (beatTimes);
    result.resize (beatTimes.size());
    std::vector<int> frameCounts (beatTimes.size(), 0);

    for (const auto& frame : chroma.frames)
    {
        // Beat interval containing frame.timeSeconds: interior beats are
        // [beatTimes[i], beatTimes[i+1)); the last beat extends to
        // [beatTimes[last], beatTimes[last] + medianIbi).
        auto it = std::upper_bound (beatTimes.begin(), beatTimes.end(), frame.timeSeconds);
        const long idxLong = std::distance (beatTimes.begin(), it) - 1;
        if (idxLong < 0)
            continue; // frame before the first beat -- not covered by any interval

        const size_t idx = (size_t) idxLong;
        if (idx == beatTimes.size() - 1 && frame.timeSeconds >= beatTimes[idx] + medianIbi)
            continue; // past the extended last-beat window

        auto& acc = result[idx];
        for (int pc = 0; pc < 12; ++pc)
        {
            acc.harmonicAvg[(size_t) pc] += frame.harmonic[(size_t) pc];
            acc.bassAvg[(size_t) pc] += frame.bass[(size_t) pc];
        }
        acc.preNormL2Avg += frame.harmonicPreNormL2;
        ++frameCounts[idx];
    }

    for (size_t i = 0; i < result.size(); ++i)
    {
        if (frameCounts[i] <= 0)
            continue; // zero frames in this interval -- stays all-zero (silence)

        const float count = (float) frameCounts[i];
        for (int pc = 0; pc < 12; ++pc)
        {
            result[i].harmonicAvg[(size_t) pc] /= count;
            result[i].bassAvg[(size_t) pc] /= count;
        }
        result[i].preNormL2Avg /= count;
    }

    return result;
}

std::array<float, 36> scoreBeatObservations (const BeatChroma& beat, const std::array<std::array<float, 12>, 36>& templates)
{
    constexpr float kBassBiasEpsilon = 1e-9f;

    float bassSum = 0.0f;
    for (float v : beat.bassAvg)
        bassSum += v;

    std::array<float, 36> rawScores {};
    for (int i = 0; i < 36; ++i)
    {
        const float cosine = cosineSimilarity (beat.harmonicAvg, templates[(size_t) i]);
        const ChordSymbol symbol = symbolForIndex (i);
        const float bassBias = kBassRootBiasWeight * beat.bassAvg[(size_t) symbol.pitchClass] / (bassSum + kBassBiasEpsilon);
        rawScores[(size_t) i] = cosine + bassBias;
    }

    float l1Sum = 0.0f;
    for (float v : rawScores)
        l1Sum += v; // scores are guaranteed >= 0 (nonneg chroma, nonneg templates, nonneg bias)

    std::array<float, 36> normalized {};
    if (l1Sum <= 0.0f)
        normalized.fill (1.0f / 36.0f); // guard all-zero row -- silent/degenerate beat
    else
        for (int i = 0; i < 36; ++i)
            normalized[(size_t) i] = rawScores[(size_t) i] / l1Sum;

    return normalized;
}

// Stub -- full Viterbi decode + N-state override + segment merge owned by Task 2.
std::vector<ChordSegment> decodeChords (const ChromaSequence& /*chroma*/, const BeatGrid& /*beats*/)
{
    return {};
}
