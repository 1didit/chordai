#include "OnsetEnvelope.h"

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

// Ellis (2007) "Beat Tracking by Dynamic Programming", §3.1 onset strength
// envelope. Every constant below is the paper's own published parameter,
// except kGaussianSigmaFrames (paper says "~20ms Gaussian", this project
// picks the sigma-in-frames that yields that width at the 250Hz rate).
namespace
{
    constexpr int kFftOrder = 8;                    // 2^8 = 256-sample window (32 ms @ 8kHz)
    constexpr int kFftSize = 1 << kFftOrder;         // 256
    constexpr int kHopSize = 32;                     // 4 ms @ 8kHz -> 250Hz envelope rate
    constexpr double kEnvelopeRateHz = 250.0;        // 8000 / kHopSize

    constexpr int kNumMelBands = 40;
    constexpr double kMelMinHz = 0.0;
    constexpr double kMelMaxHz = 4000.0;

    constexpr double kEnvelopeHighPassHz = 0.4;      // one-pole HP, locally zero-mean
    constexpr double kGaussianSigmaFrames = 5.0;     // ~20 ms @ 250 Hz envelope rate

    double hzToMel (double hz)
    {
        return 2595.0 * std::log10 (1.0 + hz / 700.0);
    }

    double melToHz (double mel)
    {
        return 700.0 * (std::pow (10.0, mel / 2595.0) - 1.0);
    }

    // Standard HTK triangular mel filterbank, built once per call against the
    // linear-frequency FFT bins (numBins = kFftSize/2 + 1, Nyquist inclusive).
    std::vector<std::vector<double>> buildMelFilterbank (double sampleRateHz, int numBins)
    {
        std::vector<std::vector<double>> filterbank ((size_t) kNumMelBands,
                                                       std::vector<double> ((size_t) numBins, 0.0));

        const double melMin = hzToMel (kMelMinHz);
        const double melMax = hzToMel (kMelMaxHz);
        const int numPoints = kNumMelBands + 2;

        std::vector<int> binPoints ((size_t) numPoints);
        for (int i = 0; i < numPoints; ++i)
        {
            const double mel = melMin + (melMax - melMin) * (double) i / (double) (numPoints - 1);
            const double hz = melToHz (mel);
            const double bin = hz / (sampleRateHz / 2.0) * (double) (numBins - 1);
            binPoints[(size_t) i] = juce::jlimit (0, numBins - 1, (int) std::llround (bin));
        }

        for (int m = 0; m < kNumMelBands; ++m)
        {
            const int left = binPoints[(size_t) m];
            const int center = binPoints[(size_t) m + 1];
            const int right = binPoints[(size_t) m + 2];

            if (center > left)
                for (int bin = left; bin <= center; ++bin)
                    filterbank[(size_t) m][(size_t) bin] = (double) (bin - left) / (double) (center - left);

            if (right > center)
                for (int bin = center; bin <= right; ++bin)
                    filterbank[(size_t) m][(size_t) bin] = juce::jmax (filterbank[(size_t) m][(size_t) bin],
                                                                        (double) (right - bin) / (double) (right - center));
        }

        return filterbank;
    }
}

OnsetEnvelopeResult computeOnsetEnvelope (const std::vector<float>& samples8k)
{
    OnsetEnvelopeResult result;
    result.rateHz = kEnvelopeRateHz;

    if ((int) samples8k.size() < kFftSize)
        return result;

    constexpr double sampleRateHz = 8000.0;
    const int numBins = kFftSize / 2 + 1; // 129 non-negative bins

    // Centered-frame convention (as in librosa's stft(center=True)): frame i's
    // window is centered on original sample i*kHopSize, so envelope frame i
    // maps to time i/kEnvelopeRateHz seconds. Zero-pad kFftSize/2 samples on
    // each side so numFrames ~= duration * kEnvelopeRateHz (the rate the
    // struct promises), rather than undercounting by one window's worth.
    const int pad = kFftSize / 2;
    const int numOriginalSamples = (int) samples8k.size();
    const int numFrames = 1 + numOriginalSamples / kHopSize;
    if (numFrames <= 1)
        return result;

    std::vector<float> padded ((size_t) (numOriginalSamples + 2 * pad), 0.0f);
    for (int i = 0; i < numOriginalSamples; ++i)
        padded[(size_t) (pad + i)] = samples8k[(size_t) i];

    auto melFilterbank = buildMelFilterbank (sampleRateHz, numBins);

    juce::dsp::FFT fft (kFftOrder);
    juce::dsp::WindowingFunction<float> window (kFftSize, juce::dsp::WindowingFunction<float>::hann, true);

    // melDb[frame][band]
    std::vector<std::vector<double>> melDb ((size_t) numFrames, std::vector<double> ((size_t) kNumMelBands, 0.0));
    std::vector<float> fftData ((size_t) kFftSize * 2, 0.0f);

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const int start = frame * kHopSize; // index into padded array
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < kFftSize; ++i)
        {
            const int idx = start + i;
            fftData[(size_t) i] = (idx >= 0 && idx < (int) padded.size()) ? padded[(size_t) idx] : 0.0f;
        }

        window.multiplyWithWindowingTable (fftData.data(), (size_t) kFftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        for (int m = 0; m < kNumMelBands; ++m)
        {
            double bandEnergy = 0.0;
            for (int bin = 0; bin < numBins; ++bin)
                bandEnergy += melFilterbank[(size_t) m][(size_t) bin] * (double) fftData[(size_t) bin];

            melDb[(size_t) frame][(size_t) m] = 20.0 * std::log10 (juce::jmax (bandEnergy, 1e-10));
        }
    }

    // First-order difference along time per band, half-wave rectify, sum across bands.
    std::vector<double> rawEnvelope ((size_t) numFrames, 0.0);
    for (int frame = 1; frame < numFrames; ++frame)
    {
        double sum = 0.0;
        for (int m = 0; m < kNumMelBands; ++m)
        {
            const double diff = melDb[(size_t) frame][(size_t) m] - melDb[(size_t) frame - 1][(size_t) m];
            if (diff > 0.0)
                sum += diff;
        }
        rawEnvelope[(size_t) frame] = sum;
    }
    rawEnvelope[0] = rawEnvelope[1];

    // High-pass one-pole filter at kEnvelopeHighPassHz to make the envelope locally zero-mean.
    std::vector<double> hpEnvelope (rawEnvelope.size(), 0.0);
    {
        const double rc = 1.0 / (2.0 * juce::MathConstants<double>::pi * kEnvelopeHighPassHz);
        const double dt = 1.0 / kEnvelopeRateHz;
        const double alpha = rc / (rc + dt);

        double prevIn = rawEnvelope[0];
        double prevOut = 0.0;
        hpEnvelope[0] = 0.0;
        for (size_t i = 1; i < rawEnvelope.size(); ++i)
        {
            const double out = alpha * (prevOut + rawEnvelope[i] - prevIn);
            hpEnvelope[i] = out;
            prevOut = out;
            prevIn = rawEnvelope[i];
        }
    }

    // Smooth with a ~20ms-wide Gaussian kernel (sigma = kGaussianSigmaFrames frames).
    std::vector<double> smoothed (hpEnvelope.size(), 0.0);
    {
        const int radius = juce::jmax (1, (int) std::ceil (kGaussianSigmaFrames * 3.0));
        std::vector<double> kernel ((size_t) (radius * 2 + 1));
        double kernelSum = 0.0;
        for (int k = -radius; k <= radius; ++k)
        {
            const double v = std::exp (-0.5 * (double) (k * k) / (kGaussianSigmaFrames * kGaussianSigmaFrames));
            kernel[(size_t) (k + radius)] = v;
            kernelSum += v;
        }
        for (auto& v : kernel)
            v /= kernelSum;

        const int n = (int) hpEnvelope.size();
        for (int i = 0; i < n; ++i)
        {
            double acc = 0.0;
            for (int k = -radius; k <= radius; ++k)
            {
                const int idx = juce::jlimit (0, n - 1, i + k);
                acc += kernel[(size_t) (k + radius)] * hpEnvelope[(size_t) idx];
            }
            smoothed[(size_t) i] = acc;
        }
    }

    // Normalize by dividing by the envelope's own standard deviation.
    double mean = 0.0;
    for (double v : smoothed)
        mean += v;
    mean /= (double) smoothed.size();

    double variance = 0.0;
    for (double v : smoothed)
        variance += (v - mean) * (v - mean);
    variance /= (double) smoothed.size();
    const double stdDev = std::sqrt (variance);

    result.envelope.assign (smoothed.size(), 0.0);
    if (stdDev > 1e-9)
        for (size_t i = 0; i < smoothed.size(); ++i)
            result.envelope[i] = smoothed[i] / stdDev;

    return result;
}
