#pragma once

// In-memory synthetic audio fixture generators for Phase 3 DSP tests.
// NO file I/O — everything renders directly into a juce::AudioBuffer<float>,
// deterministic (no RNG, or a fixed-seed juce::Random where noise is needed).

#include <JuceHeader.h>

#include "Source/Analysis/AnalysisResult.h"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace fixtures
{
    // bassPitchClass == -1 means "bass = chord root".
    struct ChordSpec
    {
        ChordSymbol chord;
        int bassPitchClass = -1;
    };

    namespace detail
    {
        inline double midiToFrequency (int midiNote)
        {
            return 440.0 * std::pow (2.0, ((double) midiNote - 69.0) / 12.0);
        }

        // Pitch classes active in a chord's triad/seventh, matching
        // Source/Analysis/ChordTemplates.h's root/third/fifth/seventh convention.
        inline std::vector<int> activePitchClasses (const ChordSymbol& chord)
        {
            std::vector<int> pitchClasses;
            pitchClasses.push_back (chord.pitchClass);
            pitchClasses.push_back ((chord.pitchClass + (chord.quality == ChordQuality::Minor ? 3 : 4)) % 12);
            pitchClasses.push_back ((chord.pitchClass + 7) % 12);
            if (chord.quality == ChordQuality::Dominant7)
                pitchClasses.push_back ((chord.pitchClass + 10) % 12);
            return pitchClasses;
        }
    }

    // Renders a chord progression as sine-tone stacks (chord tones around
    // octave 4, a bass note ~2 octaves below at slightly higher amplitude,
    // 2-3 integer harmonics per tone at decaying amplitude), with a short
    // fade in/out at each chord boundary. Mono. Deterministic.
    inline juce::AudioBuffer<float> renderChordProgression (const std::vector<ChordSpec>& chords,
                                                              double bpm,
                                                              double sampleRate,
                                                              int beatsPerChord = 4,
                                                              double detuneCents = 0.0)
    {
        const double beatSeconds = 60.0 / bpm;
        const double chordSeconds = beatSeconds * (double) beatsPerChord;
        const double totalSeconds = chordSeconds * (double) chords.size();
        const int totalSamples = juce::jmax (1, (int) std::llround (totalSeconds * sampleRate));

        juce::AudioBuffer<float> buffer (1, totalSamples);
        buffer.clear();

        const double detuneRatio = std::pow (2.0, detuneCents / 1200.0);
        constexpr double fadeSeconds = 0.01;
        constexpr std::array<float, 3> harmonicAmps { 1.0f, 0.4f, 0.2f };

        auto* data = buffer.getWritePointer (0);
        const int chordLengthSamples = juce::jmax (1, (int) std::llround (chordSeconds * sampleRate));

        for (size_t chordIndex = 0; chordIndex < chords.size(); ++chordIndex)
        {
            const auto& spec = chords[chordIndex];
            const int chordStartSample = (int) std::llround ((double) chordIndex * chordSeconds * sampleRate);

            auto pitchClasses = detail::activePitchClasses (spec.chord);
            std::vector<double> toneFrequencies;
            toneFrequencies.reserve (pitchClasses.size());
            for (int pc : pitchClasses)
                toneFrequencies.push_back (detail::midiToFrequency (60 + pc) * detuneRatio);

            const int bassPitchClass = spec.bassPitchClass < 0 ? spec.chord.pitchClass : spec.bassPitchClass;
            const double bassFrequency = detail::midiToFrequency (36 + bassPitchClass) * detuneRatio;

            for (int i = 0; i < chordLengthSamples; ++i)
            {
                const int sampleIndex = chordStartSample + i;
                if (sampleIndex < 0 || sampleIndex >= buffer.getNumSamples())
                    continue;

                const double t = (double) i / sampleRate;

                float envelope = 1.0f;
                if (t < fadeSeconds)
                    envelope = (float) (t / fadeSeconds);
                else if (t > chordSeconds - fadeSeconds)
                    envelope = (float) juce::jmax (0.0, (chordSeconds - t) / fadeSeconds);

                float sample = 0.0f;

                for (double freq : toneFrequencies)
                    for (int h = 0; h < (int) harmonicAmps.size(); ++h)
                        sample += 0.15f * harmonicAmps[(size_t) h]
                                * (float) std::sin (2.0 * std::numbers::pi * freq * (double) (h + 1) * t);

                for (int h = 0; h < (int) harmonicAmps.size(); ++h)
                    sample += 0.2f * harmonicAmps[(size_t) h]
                            * (float) std::sin (2.0 * std::numbers::pi * bassFrequency * (double) (h + 1) * t);

                data[sampleIndex] += sample * envelope;
            }
        }

        return buffer;
    }

    // 5 ms exponentially-decaying 1 kHz burst on every beat. When syncopated,
    // adds extra louder bursts on the "and" of beats 2 and 4 of each 4-beat
    // group (tempts half/double-tempo errors). Mono. Deterministic.
    inline juce::AudioBuffer<float> renderClickTrack (double bpm, double sampleRate, double durationSeconds, bool syncopated = false)
    {
        const int numSamples = juce::jmax (1, (int) std::llround (durationSeconds * sampleRate));
        juce::AudioBuffer<float> buffer (1, numSamples);
        buffer.clear();

        const double beatIntervalSeconds = 60.0 / bpm;
        constexpr double burstDurationSeconds = 0.005;
        const double decayRate = 6.0 / burstDurationSeconds;
        const int burstSamples = juce::jmax (1, (int) std::llround (burstDurationSeconds * sampleRate));

        auto* data = buffer.getWritePointer (0);

        auto renderBurst = [&] (double startSeconds, float amplitude)
        {
            const int startSample = (int) std::llround (startSeconds * sampleRate);
            for (int i = 0; i < burstSamples; ++i)
            {
                const int sampleIndex = startSample + i;
                if (sampleIndex < 0 || sampleIndex >= buffer.getNumSamples())
                    continue;

                const double t = (double) i / sampleRate;
                const float env = (float) std::exp (-decayRate * t);
                data[sampleIndex] += amplitude * env * (float) std::sin (2.0 * std::numbers::pi * 1000.0 * t);
            }
        };

        for (double beatTime = 0.0; beatTime < durationSeconds; beatTime += beatIntervalSeconds)
            renderBurst (beatTime, 0.8f);

        if (syncopated)
        {
            const double groupSeconds = beatIntervalSeconds * 4.0;
            for (double groupStart = 0.0; groupStart < durationSeconds; groupStart += groupSeconds)
            {
                renderBurst (groupStart + beatIntervalSeconds * 1.5, 1.2f);
                renderBurst (groupStart + beatIntervalSeconds * 3.5, 1.2f);
            }
        }

        return buffer;
    }

    // Overlays broadband (fixed-seed white-noise) 10 ms bursts on every beat,
    // at amplitude comparable to the harmonic content already in the buffer.
    inline void addPercussiveBursts (juce::AudioBuffer<float>& buffer, double bpm, double sampleRate)
    {
        const double beatIntervalSeconds = 60.0 / bpm;
        constexpr double burstDurationSeconds = 0.010;
        const int burstSamples = juce::jmax (1, (int) std::llround (burstDurationSeconds * sampleRate));
        const double totalSeconds = (double) buffer.getNumSamples() / sampleRate;

        juce::Random random (42);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (double beatTime = 0.0; beatTime < totalSeconds; beatTime += beatIntervalSeconds)
            {
                const int startSample = (int) std::llround (beatTime * sampleRate);
                for (int i = 0; i < burstSamples; ++i)
                {
                    const int sampleIndex = startSample + i;
                    if (sampleIndex < 0 || sampleIndex >= buffer.getNumSamples())
                        continue;

                    const float envelope = 1.0f - (float) i / (float) burstSamples;
                    const float noiseSample = random.nextFloat() * 2.0f - 1.0f;
                    data[sampleIndex] += noiseSample * envelope * 0.3f;
                }
            }
        }
    }

    // Goertzel single-bin energy estimate at freqHz, channel 0 only (fixtures
    // in this file are mono). Reusable by later plans' tests.
    inline double toneEnergy (const juce::AudioBuffer<float>& buffer, double freqHz, double sampleRate)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
            return 0.0;

        const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
        const double coeff = 2.0 * std::cos (omega);

        double s1 = 0.0, s2 = 0.0;
        const float* data = buffer.getReadPointer (0);

        for (int i = 0; i < numSamples; ++i)
        {
            const double s0 = (double) data[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double real = s1 - s2 * std::cos (omega);
        const double imag = s2 * std::sin (omega);
        return (real * real + imag * imag) / ((double) numSamples * (double) numSamples);
    }
}
