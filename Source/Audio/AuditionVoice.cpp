#include "AuditionVoice.h"

namespace AuditionVoice
{
    juce::AudioBuffer<float> render (int pitch, float velocity, int noteOnSamples, double sampleRate)
    {
        juce::ignoreUnused (pitch, velocity, noteOnSamples, sampleRate);

        // RED stub -- GREEN phase (Task 1) implements the real oscillator +
        // filter + ADSR voice described in AuditionVoice.h.
        return juce::AudioBuffer<float> (1, 0);
    }
}
