#include "AudioPreprocessing.h"

namespace
{
    // Resamples a mono float stream with a fresh WindowedSincInterpolator
    // (200-tap Hann-windowed sinc — recommended over Lagrange for the ~4x
    // downsample this stage performs, per 03-RESEARCH.md's stopband-rejection
    // rationale). Handles downsampling and upsampling (speedRatio <= 1) alike.
    std::vector<float> resampleTo (const std::vector<float>& monoInput, double sourceRate, double targetRate)
    {
        if (monoInput.empty() || sourceRate <= 0.0 || targetRate <= 0.0)
            return {};

        const double speedRatio = sourceRate / targetRate;
        const int numOut = (int) std::floor ((double) monoInput.size() * targetRate / sourceRate);
        if (numOut <= 0)
            return {};

        // GenericInterpolator::process() requires the input buffer to contain
        // at least (speedRatio * numOutputSamplesToProduce) samples; pad a few
        // trailing zeros as a defensive margin against floating-point rounding
        // landing exactly on that boundary.
        std::vector<float> paddedInput = monoInput;
        paddedInput.resize (paddedInput.size() + 16, 0.0f);

        std::vector<float> output ((size_t) numOut, 0.0f);

        juce::WindowedSincInterpolator interpolator;
        interpolator.reset();
        interpolator.process (speedRatio, paddedInput.data(), output.data(), numOut);

        return output;
    }
}

PreprocessedAudio preprocessForAnalysis (const juce::AudioBuffer<float>& audio,
                                          double sourceSampleRate,
                                          juce::Range<double> regionSeconds)
{
    PreprocessedAudio result;

    const int numChannels = audio.getNumChannels();
    const int totalSamples = audio.getNumSamples();
    const double totalLengthSeconds = sourceSampleRate > 0.0 ? (double) totalSamples / sourceSampleRate : 0.0;

    // Clamp/normalize region: empty (zero-length) or inverted/out-of-range
    // collapses to "whole buffer", matching Phase 2's RegionState::clampRegion
    // default-whole-file convention.
    double start = regionSeconds.getStart();
    double end = regionSeconds.getEnd();
    if (start == end || end < start)
    {
        start = 0.0;
        end = totalLengthSeconds;
    }
    start = juce::jlimit (0.0, totalLengthSeconds, start);
    end = juce::jlimit (0.0, totalLengthSeconds, end);
    if (end <= start)
    {
        start = 0.0;
        end = totalLengthSeconds;
    }

    result.regionStartSeconds = start;

    int startSample = juce::jlimit (0, totalSamples, (int) std::floor (start * sourceSampleRate));
    int endSample = juce::jlimit (0, totalSamples, (int) std::floor (end * sourceSampleRate));
    const int regionLength = juce::jmax (0, endSample - startSample);

    // Downmix: average all channels into one float vector over the region samples.
    std::vector<float> mono ((size_t) regionLength, 0.0f);
    if (numChannels > 0 && regionLength > 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = audio.getReadPointer (ch, startSample);
            for (int i = 0; i < regionLength; ++i)
                mono[(size_t) i] += channelData[i];
        }
        const float scale = 1.0f / (float) numChannels;
        for (auto& s : mono)
            s *= scale;
    }

    result.onsetSamples = resampleTo (mono, sourceSampleRate, result.onsetRate);
    result.chromaSamples = resampleTo (mono, sourceSampleRate, result.chromaRate);

    return result;
}
