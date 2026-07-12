#include "KeyDetector.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Krumhansl-Kessler (1982) probe-tone key profiles, index 0 = tonic scale
    // degree, embedded verbatim from 03-RESEARCH.md's "Key Detection" section.
    constexpr std::array<float, 12> kMajorProfile {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };
    constexpr std::array<float, 12> kMinorProfile {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };

    // Builds the candidate correlation vector for absolute tonic pitch class
    // `tonic`: absolute pitch class p's weight is profile[relative degree],
    // where relative degree = (p - tonic) mod 12.
    std::array<float, 12> rotateProfileForTonic (const std::array<float, 12>& profile, int tonic)
    {
        std::array<float, 12> rotated {};
        for (int p = 0; p < 12; ++p)
            rotated[(size_t) p] = profile[(size_t) ((p - tonic + 12) % 12)];
        return rotated;
    }

    // Pearson correlation between two 12-bin vectors. Zero-variance input in
    // either vector (e.g. an all-silent or perfectly uniform chroma) is
    // guarded to a defined 0 rather than a NaN divide-by-zero.
    float pearsonCorrelation (const std::array<float, 12>& a, const std::array<float, 12>& b)
    {
        double meanA = 0.0, meanB = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            meanA += (double) a[(size_t) i];
            meanB += (double) b[(size_t) i];
        }
        meanA /= 12.0;
        meanB /= 12.0;

        double numerator = 0.0, varianceA = 0.0, varianceB = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const double da = (double) a[(size_t) i] - meanA;
            const double db = (double) b[(size_t) i] - meanB;
            numerator += da * db;
            varianceA += da * da;
            varianceB += db * db;
        }

        const double denominator = std::sqrt (varianceA * varianceB);
        if (denominator < 1e-9)
            return 0.0f;

        return (float) (numerator / denominator);
    }

    struct KeyCandidate
    {
        int tonic = 0;
        bool isMajor = true;
        float correlation = 0.0f;
    };
}

std::array<float, 12> accumulateChroma (const ChromaSequence& chroma)
{
    // Energy-weighted sum: frames with more harmonic energy count more
    // towards the accumulated key-detection vector; silent frames contribute
    // nothing. Key detection wants maximum data over the whole analyzed
    // region, not beat-synchronized granularity.
    std::array<double, 12> sum {};
    for (const auto& frame : chroma.frames)
        for (int pc = 0; pc < 12; ++pc)
            sum[(size_t) pc] += (double) frame.harmonic[(size_t) pc] * (double) frame.harmonicPreNormL2;

    double normSquared = 0.0;
    for (double v : sum)
        normSquared += v * v;
    const double norm = std::sqrt (normSquared);

    std::array<float, 12> result {};
    if (norm < 1e-9)
        return result; // all-silent sequence -> all-zero vector

    for (int pc = 0; pc < 12; ++pc)
        result[(size_t) pc] = (float) (sum[(size_t) pc] / norm);

    return result;
}

KeyResult detectKey (const std::array<float, 12>& accumulatedChroma)
{
    std::array<KeyCandidate, 24> candidates;
    int index = 0;
    for (int tonic = 0; tonic < 12; ++tonic)
    {
        candidates[(size_t) index++] = {
            tonic, true, pearsonCorrelation (accumulatedChroma, rotateProfileForTonic (kMajorProfile, tonic))
        };
        candidates[(size_t) index++] = {
            tonic, false, pearsonCorrelation (accumulatedChroma, rotateProfileForTonic (kMinorProfile, tonic))
        };
    }

    std::stable_sort (candidates.begin(), candidates.end(),
                       [] (const KeyCandidate& a, const KeyCandidate& b) { return a.correlation > b.correlation; });

    const auto& best = candidates[0];
    const auto& second = candidates[1];

    KeyResult result;
    result.tonicPitchClass = best.tonic;
    result.isMajor = best.isMajor;

    const float margin = (best.correlation - second.correlation) / (std::abs (best.correlation) + 1e-9f);
    result.confidence = std::clamp (margin, 0.0f, 1.0f);

    return result;
}
