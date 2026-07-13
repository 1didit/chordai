#include "AuditionRenderer.h"

namespace AuditionRenderer
{
    juce::AudioBuffer<float> render (const MidiSetRow& row, double bpm, double sampleRate)
    {
        juce::ignoreUnused (row, bpm, sampleRate);

        // RED stub -- GREEN phase (Task 1) implements the real per-note mix
        // + gain-normalization described in AuditionRenderer.h.
        return juce::AudioBuffer<float> (1, 0);
    }
}
