#include "ChordDecoder.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

namespace
{
    constexpr int kNumStates = 36;

    // Self-transition weight (beta in 03-RESEARCH.md's Viterbi transition
    // matrix). Literature-reported chord-HMM values cluster in 0.8-0.99, with
    // 0.85 flagged as only a v1 STARTING point, explicitly tunable (Open
    // Question 3) and required to be validated against both a slow
    // (1-chord-per-bar) and a fast (1-chord-per-beat) synthetic progression.
    // Empirically, 0.85 badly oversmoothed ChordDecoderTests.FastChanges (8
    // distinct 1-beat chords collapsed to 1-2 segments) and even
    // ChordDecoderTests.SilenceGivesNoChord's 4-beat second chord never won
    // the global Viterbi path away from the first chord's state. Root cause:
    // this project's L1-normalized cosine+bass-bias observation matrix (Task
    // 1) is far less peaked than a typical trained-HMM emission model -- the
    // best-vs-second-best per-beat likelihood ratio for real (non-idealized)
    // synthetic audio is usually only ~2-3x, not orders of magnitude -- so
    // literature betas tuned for sharper emission models are far too sticky
    // here. 0.85 down to 0.05 all still failed FastChanges; 0.03 was the
    // first value to pass every fixture, 0.04 (this value) keeps a small
    // margin above that empirical floor while remaining just above the
    // uniform/no-bias baseline (1/36 = 0.0278) -- i.e. v1's self-transition
    // bias is real but deliberately weak given the current scoring sharpness.
    constexpr float kSelfTransition = 0.04f;

    // N-state silence-override floor on the beat-averaged pre-normalization
    // harmonic L2 norm (BeatChroma::preNormL2Avg). Beats below this are forced
    // to ChordQuality::NoChord regardless of the Viterbi state -- NoChord is
    // NOT a 37th HMM state (03-RESEARCH.md "v1 chord vocabulary"). Tuned
    // against ChordDecoderTests.SilenceGivesNoChord (synthetic sine-stack
    // fixtures): observed true-silence beats measure ~0.0004, onset/decay
    // transient beats at a chord's fade-in/fade-out edge measure ~1.3-3.8,
    // and fully sounding beats measure ~28-64. 0.5 sits below every observed
    // transient-edge value (so real chord onsets are never misclassified as
    // silence) while sitting far above the true-silence noise floor.
    constexpr float kSilenceFloor = 0.5f;

    // Log-domain Viterbi observation floor (03-RESEARCH.md Code Examples:
    // "Use log(B + 1e-12) for observations").
    constexpr float kLogObservationFloor = 1e-12f;
}

std::vector<ChordSegment> decodeChords (const ChromaSequence& chroma, const BeatGrid& beats)
{
    const auto& beatTimes = beats.beatTimesSeconds;
    if (chroma.frames.empty() || beatTimes.size() < 2)
        return {};

    const auto beatChromas = computeBeatSyncChroma (chroma, beats);
    const auto templates = buildAllTemplates();
    const int numBeats = (int) beatChromas.size();

    // Observation matrix B[beat][state] -- L1-normalized per-beat scores from Task 1.
    std::vector<std::array<float, kNumStates>> observations ((size_t) numBeats);
    for (int t = 0; t < numBeats; ++t)
        observations[(size_t) t] = scoreBeatObservations (beatChromas[(size_t) t], templates);

    // Log-domain Viterbi: uniform initial prior 1/36; self-transition beta on
    // the diagonal, (1-beta)/35 elsewhere.
    const float logInitialPrior = std::log (1.0f / (float) kNumStates);
    const float logSelfTransition = std::log (kSelfTransition);
    const float logOtherTransition = std::log ((1.0f - kSelfTransition) / (float) (kNumStates - 1));

    std::vector<std::array<float, kNumStates>> logDelta ((size_t) numBeats);
    std::vector<std::array<int, kNumStates>> backpointer ((size_t) numBeats);

    for (int s = 0; s < kNumStates; ++s)
        logDelta[0][(size_t) s] = logInitialPrior + std::log (observations[0][(size_t) s] + kLogObservationFloor);

    for (int t = 1; t < numBeats; ++t)
    {
        for (int s = 0; s < kNumStates; ++s)
        {
            float bestScore = -std::numeric_limits<float>::infinity();
            int bestPrev = 0;
            for (int sp = 0; sp < kNumStates; ++sp)
            {
                const float transitionLogProb = (sp == s) ? logSelfTransition : logOtherTransition;
                const float score = logDelta[(size_t) (t - 1)][(size_t) sp] + transitionLogProb;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestPrev = sp;
                }
            }
            logDelta[(size_t) t][(size_t) s] = bestScore + std::log (observations[(size_t) t][(size_t) s] + kLogObservationFloor);
            backpointer[(size_t) t][(size_t) s] = bestPrev;
        }
    }

    // Backtrace from the highest-scoring final state.
    std::vector<int> stateSequence ((size_t) numBeats);
    {
        int bestFinalState = 0;
        float bestFinalScore = -std::numeric_limits<float>::infinity();
        for (int s = 0; s < kNumStates; ++s)
        {
            if (logDelta[(size_t) (numBeats - 1)][(size_t) s] > bestFinalScore)
            {
                bestFinalScore = logDelta[(size_t) (numBeats - 1)][(size_t) s];
                bestFinalState = s;
            }
        }
        stateSequence[(size_t) (numBeats - 1)] = bestFinalState;
        for (int t = numBeats - 2; t >= 0; --t)
            stateSequence[(size_t) t] = backpointer[(size_t) (t + 1)][(size_t) stateSequence[(size_t) (t + 1)]];
    }

    // N-state override (deterministic, post-Viterbi): force NoChord on beats
    // whose averaged pre-norm harmonic L2 is below kSilenceFloor.
    std::vector<ChordSymbol> perBeatChord ((size_t) numBeats);
    std::vector<float> perBeatConfidence ((size_t) numBeats);
    for (int t = 0; t < numBeats; ++t)
    {
        if (beatChromas[(size_t) t].preNormL2Avg < kSilenceFloor)
        {
            perBeatChord[(size_t) t] = ChordSymbol { 0, ChordQuality::NoChord };
            perBeatConfidence[(size_t) t] = 0.0f; // no template match applies to a forced NoChord beat
        }
        else
        {
            perBeatChord[(size_t) t] = symbolForIndex (stateSequence[(size_t) t]);
            perBeatConfidence[(size_t) t] = observations[(size_t) t][(size_t) stateSequence[(size_t) t]];
        }
    }

    // Segment merge: run-length-encode consecutive equal (pitchClass, quality)
    // beats into ChordSegments. startBeatIndex/endBeatIndex is a half-open
    // [start, end) range into beatTimesSeconds (endBeatIndex is one-past-last,
    // so it may equal beatTimesSeconds.size() for the final segment).
    // startSeconds/endSeconds mirror computeBeatSyncChroma's own last-interval
    // rule: the final segment's endSeconds is the last beat time plus the
    // median inter-beat interval (there is no beat[last+1] to bound it).
    std::vector<ChordSegment> segments;
    const double medianIbi = medianInterBeatInterval (beatTimes);
    int segmentStart = 0;

    for (int t = 1; t <= numBeats; ++t)
    {
        const bool segmentBoundary = (t == numBeats)
            || ! (perBeatChord[(size_t) t].pitchClass == perBeatChord[(size_t) (t - 1)].pitchClass
                  && perBeatChord[(size_t) t].quality == perBeatChord[(size_t) (t - 1)].quality);
        if (! segmentBoundary)
            continue;

        ChordSegment segment;
        segment.chord = perBeatChord[(size_t) segmentStart];
        segment.startBeatIndex = segmentStart;
        segment.endBeatIndex = t;
        segment.startSeconds = beatTimes[(size_t) segmentStart];
        segment.endSeconds = (t < numBeats) ? beatTimes[(size_t) t] : (beatTimes.back() + medianIbi);

        float confidenceSum = 0.0f;
        for (int k = segmentStart; k < t; ++k)
            confidenceSum += perBeatConfidence[(size_t) k];
        segment.confidence = confidenceSum / (float) (t - segmentStart);

        segments.push_back (segment);
        segmentStart = t;
    }

    return segments;
}
