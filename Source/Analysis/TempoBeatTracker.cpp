#include "TempoBeatTracker.h"

#include <JuceHeader.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

// Ellis (2007) "Beat Tracking by Dynamic Programming", §2/§3.2. Every
// constant is the paper's own published value except kBeatTightness, which
// the paper leaves as an implementation knob — librosa's reimplementation of
// this exact algorithm defaults its analogous "tightness" parameter to 100,
// used here as the v1 starting point (03-RESEARCH.md "Beat & Tempo Detection").
namespace
{
    constexpr double kMinPeriodSeconds = 0.2;   // 300 BPM
    constexpr double kMaxPeriodSeconds = 4.0;   // 15 BPM
    constexpr double kTau0Seconds = 0.5;        // 120 BPM, perceptual-weighting center
    constexpr double kSigmaTauOctaves = 1.4;    // perceptual-weighting width, in octaves
    constexpr double kBeatTightness = 100.0;    // alpha: DP transition-cost weight

    // Direct C++ port of Ellis (2007) Fig. 1's published MATLAB backtrace
    // (03-RESEARCH.md "Code Examples"). periodFrames is the target inter-beat
    // interval in envelope frames (not raw audio samples).
    std::vector<int> dpBeatTrack (const std::vector<double>& localScore, double periodFrames, double alpha)
    {
        const int n = (int) localScore.size();
        std::vector<double> cumScore = localScore;
        std::vector<int> backlink (n, -1);

        const int lo = (int) std::llround (-2.0 * periodFrames);
        const int hi = (int) std::llround (-0.5 * periodFrames);

        for (int i = 0; i < n; ++i)
        {
            double best = -1e300;
            int bestTau = -1;
            for (int d = lo; d <= hi; ++d)
            {
                const int tau = i + d;
                if (tau < 0)
                    continue;

                const double f = -std::pow (std::log ((double) -d / periodFrames), 2.0); // F(delta, period)
                const double score = alpha * f + cumScore[(size_t) tau];
                if (score > best)
                {
                    best = score;
                    bestTau = tau;
                }
            }
            cumScore[(size_t) i] = (bestTau >= 0 ? best : 0.0) + localScore[(size_t) i];
            backlink[(size_t) i] = bestTau;
        }

        const int endIdx = (int) (std::max_element (cumScore.begin(), cumScore.end()) - cumScore.begin());
        std::vector<int> beats;
        for (int t = endIdx; t >= 0; t = backlink[(size_t) t])
        {
            beats.push_back (t);
            if (backlink[(size_t) t] < 0)
                break;
        }
        std::reverse (beats.begin(), beats.end());
        return beats;
    }
}

BeatGrid trackBeats (const OnsetEnvelopeResult& onset)
{
    BeatGrid grid;

    const auto& envelope = onset.envelope;
    const double rateHz = onset.rateHz;
    const int n = (int) envelope.size();

    const int minLag = (int) std::llround (kMinPeriodSeconds * rateHz);
    const int maxLag = juce::jmin ((int) std::llround (kMaxPeriodSeconds * rateHz), n - 1);

    if (n < 2 || minLag < 1 || maxLag <= minLag)
        return grid; // degenerate: envelope too short for any valid period estimate

    // Autocorrelation over the candidate period range.
    std::vector<double> autocorr ((size_t) (maxLag + 1), 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (int t = lag; t < n; ++t)
            sum += envelope[(size_t) t] * envelope[(size_t) (t - lag)];
        autocorr[(size_t) lag] = sum;
    }

    // Perceptual weighting W(tau) = exp(-0.5 * (log2(tau/tau0)/sigmaTau)^2) -> TPS(tau).
    std::vector<double> tps ((size_t) (maxLag + 1), 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        const double tauSeconds = (double) lag / rateHz;
        const double logRatio = std::log2 (tauSeconds / kTau0Seconds);
        const double weight = std::exp (-0.5 * (logRatio * logRatio) / (kSigmaTauOctaves * kSigmaTauOctaves));
        tps[(size_t) lag] = weight * autocorr[(size_t) lag];
    }

    auto tpsAt = [&] (int lag) -> double
    {
        return (lag >= minLag && lag <= maxLag) ? tps[(size_t) lag] : 0.0;
    };

    // Octave-error mitigation: TPS2(tau) = TPS(tau) + TPS(2*tau), TPS3(tau) = TPS(tau) + TPS(3*tau)
    // (re-checking the 0.33/0.5/2/3x metrical levels); the winning tau is whichever position -
    // in the original TPS or either composite - has the single strongest value.
    int bestLag = minLag;
    double bestValue = -1.0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        const double original = tps[(size_t) lag];
        const double composite2 = tps[(size_t) lag] + tpsAt (lag * 2);
        const double composite3 = tps[(size_t) lag] + tpsAt (lag * 3);
        const double strongest = juce::jmax (original, juce::jmax (composite2, composite3));
        if (strongest > bestValue)
        {
            bestValue = strongest;
            bestLag = lag;
        }
    }

    // Metrical-level disambiguation: the TPS winner can lock onto a related
    // metrical level instead of the perceptual quarter-note — measured on a
    // real trap beat where triplet/dotted-eighth hi-hats made the 4/3x level
    // (170 BPM) beat the true 128 BPM. Ellis's own 2x/3x composites can't see
    // 4/3 or 3/4 relatives. So: build candidate periods at the common metrical
    // ratios of the TPS winner, snap each to the nearest local TPS peak, run
    // the (cheap) DP tracker on each, and keep the grid whose beats actually
    // land on the strongest onsets — weighted by the same perceptual W(tau)
    // so ~120 BPM interpretations win ties.
    auto snapToLocalPeak = [&] (int lag) -> int
    {
        int best = lag;
        double bestTps = tpsAt (lag);
        for (int d = -5; d <= 5; ++d)
        {
            const int cand = lag + d;
            if (tpsAt (cand) > bestTps)
            {
                bestTps = tpsAt (cand);
                best = cand;
            }
        }
        return best;
    };

    constexpr double kMetricalRatios[] = { 1.0, 0.75, 4.0 / 3.0, 0.5, 2.0, 1.0 / 3.0, 3.0 };

    std::vector<int> candidateLags;
    for (double ratio : kMetricalRatios)
    {
        const int lag = snapToLocalPeak ((int) std::llround ((double) bestLag * ratio));
        if (lag < minLag || lag > maxLag)
            continue;
        if (std::find (candidateLags.begin(), candidateLags.end(), lag) == candidateLags.end())
            candidateLags.push_back (lag);
    }

    std::vector<int> beatFrames;
    double bestGridScore = -1e300;
    for (int lag : candidateLags)
    {
        auto candidateBeats = dpBeatTrack (envelope, (double) lag, kBeatTightness);
        if (candidateBeats.size() < 2)
            continue;

        // Mean onset strength at the chosen beats (how well the grid "hits").
        double onsetSum = 0.0;
        for (int frame : candidateBeats)
            onsetSum += envelope[(size_t) juce::jlimit (0, n - 1, frame)];
        const double meanOnset = onsetSum / (double) candidateBeats.size();

        const double tauSeconds = (double) lag / rateHz;
        const double logRatio = std::log2 (tauSeconds / kTau0Seconds);
        const double weight = std::exp (-0.5 * (logRatio * logRatio) / (kSigmaTauOctaves * kSigmaTauOctaves));

        // Density compensation: a sparser grid "cherry-picks" only the
        // loudest onsets, so raw meanOnset grows ~sqrt(period) regardless of
        // metrical correctness (measured on a real 128 BPM trap beat:
        // meanOnset 2.07@170BPM, 2.42@127, 2.76@86, 3.40@57 — monotone in
        // period). Dividing by sqrt(periodFrames) removes that bias; the
        // perceptual W(tau) then breaks ties toward ~120 BPM readings.
        const double score = weight * meanOnset / std::sqrt ((double) lag);

        if (std::getenv ("CHORDAI_DIAG") != nullptr)
            std::fprintf (stderr, "[diag/tempo] lag %d (%.2f BPM) tps %.1f meanOnset %.3f W %.3f score %.3f beats %d\n",
                          lag, 60.0 * rateHz / (double) lag, tpsAt (lag), meanOnset, weight, score,
                          (int) candidateBeats.size());

        if (score > bestGridScore)
        {
            bestGridScore = score;
            beatFrames = std::move (candidateBeats);
        }
    }

    if (beatFrames.size() < 2)
        return grid; // not enough beats recovered for a usable grid

    grid.beatTimesSeconds.reserve (beatFrames.size());
    for (int frame : beatFrames)
        grid.beatTimesSeconds.push_back ((double) frame / rateHz);

    // bpm from the median inter-beat interval of the DP output (more robust
    // than the raw autocorrelation period estimate).
    std::vector<double> intervals;
    intervals.reserve (grid.beatTimesSeconds.size() - 1);
    for (size_t i = 1; i < grid.beatTimesSeconds.size(); ++i)
        intervals.push_back (grid.beatTimesSeconds[i] - grid.beatTimesSeconds[i - 1]);

    std::sort (intervals.begin(), intervals.end());
    const double medianInterval = intervals[intervals.size() / 2];
    grid.bpm = medianInterval > 1e-9 ? 60.0 / medianInterval : 0.0;

    // v1 explicit 4/4 assumption (documented limitation, see TempoBeatTracker.h):
    // no true downbeat detection - bar boundaries are simply every 4th beat.
    for (int i = 0; i < (int) grid.beatTimesSeconds.size(); i += 4)
        grid.barStartBeatIndices.push_back (i);

    return grid;
}
