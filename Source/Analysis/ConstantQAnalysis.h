#pragma once

// Owned by plan 03-02. Skeleton created in 03-01 for build wiring only.

#include <functional>
#include <vector>

struct CqtFrames
{
    std::vector<std::vector<double>> columns;  // [frame][bin] magnitudes, rectangular
    std::vector<double> binFrequenciesHz;      // per-bin center Hz, queried via getBinFrequency()
    double columnHopSeconds = 0.0;
    double latencySeconds = 0.0;

    // hop*i - latency, clamped >= 0
    double frameTimeSeconds (int frameIndex) const
    {
        double t = columnHopSeconds * (double) frameIndex - latencySeconds;
        return t < 0.0 ? 0.0 : t;
    }
};

// shouldCancel (optional, default empty -- existing callers unaffected) is
// polled once per feed chunk (plan 03-06): the streaming process() loop below
// bails out early without flushing getRemainingOutput(), giving sub-second
// cancellation latency even for the longest analysis stage.
CqtFrames computeCqt (const std::vector<float>& samples, double sampleRate,
                       const std::function<bool()>& shouldCancel = {});
