#include "TuningEstimator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
    // Bins within a frame whose magnitude exceeds this fraction of the frame's
    // own max magnitude are "strong" and contribute to the tuning histogram
    // (Harte & Sandler 2005-style: only confident, harmonically-active bins
    // should vote on the tuning offset, not the noise floor).
    constexpr double kStrongBinThreshold = 0.3;
}

double estimateTuningCents (const CqtFrames& cqt)
{
    double sumCos = 0.0;
    double sumSin = 0.0;
    double totalWeight = 0.0;

    for (const auto& column : cqt.columns)
    {
        if (column.empty())
            continue;

        double frameMax = 0.0;
        for (double magnitude : column)
            frameMax = std::max (frameMax, magnitude);

        if (frameMax <= 0.0)
            continue;

        const double strongFloor = kStrongBinThreshold * frameMax;

        for (size_t bin = 0; bin < column.size(); ++bin)
        {
            const double magnitude = column[bin];
            if (magnitude <= strongFloor)
                continue;

            const double freqHz = cqt.binFrequenciesHz[bin];
            if (freqHz <= 0.0)
                continue;

            const double midiNote = 69.0 + 12.0 * std::log2 (freqHz / 440.0);
            const double roundedNote = std::round (midiNote);
            const double centsDeviation = (midiNote - roundedNote) * 100.0; // in [-50, 50)

            // Circular weighted mean: deviation space wraps at +-50 cents, so
            // accumulate as a magnitude-weighted vector on the unit circle
            // rather than a naive arithmetic mean (which would break near the
            // +-50 cent wraparound boundary).
            const double theta = centsDeviation / 50.0 * std::numbers::pi;
            sumCos += magnitude * std::cos (theta);
            sumSin += magnitude * std::sin (theta);
            totalWeight += magnitude;
        }
    }

    // Silence / no confident bins at all -- no tuning information available.
    if (totalWeight <= 0.0)
        return 0.0;

    return std::atan2 (sumSin, sumCos) / std::numbers::pi * 50.0;
}
