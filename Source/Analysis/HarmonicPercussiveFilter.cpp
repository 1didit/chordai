#include "HarmonicPercussiveFilter.h"

#include <algorithm>
#include <cmath>

void suppressPercussion (CqtFrames& cqt, double kernelSeconds)
{
    const size_t numFrames = cqt.columns.size();
    if (numFrames == 0)
        return;

    const size_t numBins = cqt.columns.front().size();
    if (numBins == 0)
        return;

    // Kernel length in frames: odd number of frames >= 3, derived from the
    // requested seconds via the measured columnHopSeconds (never hardcoded).
    int kernelFrames = cqt.columnHopSeconds > 0.0
        ? (int) std::lround (kernelSeconds / cqt.columnHopSeconds)
        : 3;
    kernelFrames = std::max (3, kernelFrames);
    if (kernelFrames % 2 == 0)
        ++kernelFrames;

    const int halfWindow = kernelFrames / 2;

    // Per-frequency-bin (row) median filter along the TIME axis (Fitzgerald
    // 2010-style harmonic enhancement). Output is built into a separate buffer
    // first (never read-after-write against cqt.columns) then swapped in, so
    // each frame's median always sees the ORIGINAL neighboring magnitudes.
    std::vector<std::vector<double>> filtered (numFrames, std::vector<double> (numBins, 0.0));
    std::vector<double> scratch;
    scratch.reserve ((size_t) kernelFrames);

    for (size_t bin = 0; bin < numBins; ++bin)
    {
        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            const int lo = std::max (0, (int) frame - halfWindow);
            const int hi = std::min ((int) numFrames - 1, (int) frame + halfWindow);

            scratch.clear();
            for (int f = lo; f <= hi; ++f)
                scratch.push_back (cqt.columns[(size_t) f][bin]);

            const size_t count = scratch.size();
            const size_t mid = count / 2;
            std::nth_element (scratch.begin(), scratch.begin() + (std::ptrdiff_t) mid, scratch.end());
            double median = scratch[mid];

            if (count % 2 == 0)
            {
                // Even-sized edge window (shrunk asymmetrically near boundaries):
                // average the two middle elements for a proper median.
                std::nth_element (scratch.begin(), scratch.begin() + (std::ptrdiff_t) mid - 1, scratch.begin() + (std::ptrdiff_t) mid);
                median = 0.5 * (median + scratch[mid - 1]);
            }

            filtered[frame][bin] = median;
        }
    }

    for (size_t frame = 0; frame < numFrames; ++frame)
        cqt.columns[frame] = std::move (filtered[frame]);
}
