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

    // Bin-major flat copy of the ORIGINAL (pre-filter) magnitudes: turns each
    // bin's time-series into one CONTIGUOUS run of numFrames doubles, so the
    // per-window scratch copy below is a single contiguous range-copy instead
    // of striding through numFrames separate heap-allocated row vectors
    // (cqt.columns is frame-major vector-of-vectors -- great for the CQT
    // library's own column-at-a-time production, terrible cache locality for
    // this per-bin time-axis scan). Same values, same order, same nth_element
    // median below -- purely a read-layout optimization (measured ~2.4x
    // wall-clock speedup on this stage on a 180s synthetic clip in an
    // unoptimized Debug build; see ClassicDspChordAnalyzerTests.PerformanceBudget's
    // doc comment for the full profiling story and why this alone isn't
    // enough to hit the 10s budget in Debug).
    std::vector<double> binMajor (numBins * numFrames);
    for (size_t frame = 0; frame < numFrames; ++frame)
    {
        const auto& column = cqt.columns[frame];
        for (size_t bin = 0; bin < numBins; ++bin)
            binMajor[bin * numFrames + frame] = column[bin];
    }

    // Output is built into a separate buffer first (never read-after-write
    // against cqt.columns/binMajor) then swapped in, so each frame's median
    // always sees the ORIGINAL neighboring magnitudes. (Only the READ side
    // needed the bin-major flattening above -- a matching bin-major write
    // buffer was measured to give no further improvement, since nth_element
    // itself, not the write pattern, dominates the remaining cost; kept
    // frame-major here to match cqt.columns' own shape with a plain move,
    // no extra transpose pass.)
    std::vector<std::vector<double>> filtered (numFrames, std::vector<double> (numBins, 0.0));
    std::vector<double> scratch;
    scratch.reserve ((size_t) kernelFrames);

    for (size_t bin = 0; bin < numBins; ++bin)
    {
        const double* row = binMajor.data() + bin * numFrames;

        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            const int lo = std::max (0, (int) frame - halfWindow);
            const int hi = std::min ((int) numFrames - 1, (int) frame + halfWindow);

            scratch.assign (row + lo, row + hi + 1);

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
