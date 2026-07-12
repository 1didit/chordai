#include "ChromaExtractor.h"

#include <cmath>

namespace
{
    // Frequency-range masks (PITFALLS.md #2/#3, verbatim per 03-RESEARCH.md).
    constexpr double kHarmonicMinHz = 80.0;
    constexpr double kBassMinHz = 55.0;
    constexpr double kBassMaxHz = 250.0;

    // Floor below which a chroma vector's pre-normalization L2 norm is treated
    // as silence: leave the vector all-zero rather than dividing by ~0 (FMP
    // notebook's "thresholded normalization" pattern -- never NaN/Inf).
    constexpr float kSilenceEpsilon = 1.0e-6f;

    // Standard MIDI/A440 12-TET pitch-class fold, tuning-corrected. Verbatim
    // from 03-RESEARCH.md Pattern 2.
    int pitchClassForBin (double freqHz, double tuningCents)
    {
        const double correctedA4 = 440.0 * std::pow (2.0, tuningCents / 1200.0);
        const double midiNote = 69.0 + 12.0 * std::log2 (freqHz / correctedA4);
        const int pitchClass = ((int) std::lround (midiNote)) % 12;
        return pitchClass < 0 ? pitchClass + 12 : pitchClass;
    }

    void l2NormalizeWithFloor (std::array<float, 12>& v, float preNormL2)
    {
        if (preNormL2 < kSilenceEpsilon)
        {
            v.fill (0.0f);
            return;
        }

        for (float& x : v)
            x /= preNormL2;
    }

    float l2Norm (const std::array<float, 12>& v)
    {
        float sumSq = 0.0f;
        for (float x : v)
            sumSq += x * x;
        return std::sqrt (sumSq);
    }
}

ChromaSequence extractChroma (const CqtFrames& cqt, double tuningCents)
{
    ChromaSequence result;
    result.frames.reserve (cqt.columns.size());

    for (size_t frameIndex = 0; frameIndex < cqt.columns.size(); ++frameIndex)
    {
        const auto& column = cqt.columns[frameIndex];

        ChromaFrame frame;
        frame.timeSeconds = cqt.frameTimeSeconds ((int) frameIndex);

        const size_t numBins = std::min (column.size(), cqt.binFrequenciesHz.size());
        for (size_t bin = 0; bin < numBins; ++bin)
        {
            const double freqHz = cqt.binFrequenciesHz[bin];
            if (freqHz <= 0.0)
                continue;

            const float magnitude = (float) column[bin];
            const int pitchClass = pitchClassForBin (freqHz, tuningCents);

            if (freqHz >= kHarmonicMinHz)
                frame.harmonic[(size_t) pitchClass] += magnitude;

            if (freqHz >= kBassMinHz && freqHz <= kBassMaxHz)
                frame.bass[(size_t) pitchClass] += magnitude;
        }

        frame.harmonicPreNormL2 = l2Norm (frame.harmonic);
        const float bassPreNormL2 = l2Norm (frame.bass);

        l2NormalizeWithFloor (frame.harmonic, frame.harmonicPreNormL2);
        l2NormalizeWithFloor (frame.bass, bassPreNormL2);

        result.frames.push_back (frame);
    }

    return result;
}
