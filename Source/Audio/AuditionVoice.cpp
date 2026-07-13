#include "AuditionVoice.h"

#include <cmath>
#include <cstring>

namespace AuditionVoice
{
    juce::AudioBuffer<float> render (int pitch, float velocity, int noteOnSamples, double sampleRate)
    {
        jassert (sampleRate > 0.0);

        noteOnSamples = juce::jmax (0, noteOnSamples);

        juce::ADSR adsr;
        adsr.setSampleRate (sampleRate);
        adsr.setParameters ({ (float) kAttackSeconds, (float) kDecaySeconds, kSustainLevel, (float) kReleaseSeconds });

        // Equal-temperament frequency from a MIDI pitch (A4 = 69 = 440Hz).
        const double frequencyHz = 440.0 * std::pow (2.0, ((double) pitch - 69.0) / 12.0);
        const double phaseIncrement = frequencyHz / sampleRate;

        // juce::ADSR's release rate is time-bounded (releaseRate is derived
        // from envelopeVal / (release * sampleRate), so it always reaches
        // idle in ~kReleaseSeconds regardless of the level at noteOff) --
        // this cap is defensive belt-and-suspenders, never expected to fire.
        const int hardSafetyCapSamples = noteOnSamples + (int) std::ceil (5.0 * sampleRate);

        std::vector<float> samples;
        samples.reserve ((size_t) noteOnSamples + (size_t) std::ceil (kReleaseSeconds * sampleRate) + 16);

        const float filterCoeff =
            (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * (double) kLowpassCutoffHz / sampleRate));

        adsr.noteOn();
        double phase = 0.0;
        float filterState = 0.0f;
        bool released = false;
        int sampleIndex = 0;

        while (true)
        {
            if (! released && sampleIndex >= noteOnSamples)
            {
                adsr.noteOff();
                released = true;
            }

            if (released && ! adsr.isActive())
                break;

            if (sampleIndex >= hardSafetyCapSamples)
                break;

            // 0.6*triangle + 0.4*naive-saw, both bounded to [-1, 1]; a
            // weighted average with weights summing to 1.0 keeps the
            // combined oscillator bounded to [-1, 1] too.
            const double sawValue = 2.0 * phase - 1.0;
            const double triValue = 2.0 * std::abs (sawValue) - 1.0;
            const double osc = 0.6 * triValue + 0.4 * sawValue;

            phase += phaseIncrement;
            if (phase >= 1.0)
                phase -= 1.0;

            const float envelope = adsr.getNextSample(); // in [0, 1]
            const float raw = (float) osc * envelope;    // in [-1, 1]

            // One-pole lowpass: a convex combination of the previous state
            // and the (bounded) input, so filterState stays within [-1, 1].
            filterState += filterCoeff * (raw - filterState);

            samples.push_back (filterState * velocity);
            ++sampleIndex;
        }

        juce::AudioBuffer<float> buffer (1, (int) samples.size());
        if (! samples.empty())
            std::memcpy (buffer.getWritePointer (0), samples.data(), samples.size() * sizeof (float));

        return buffer;
    }
}
