#include "AuditionRenderer.h"

#include "AuditionVoice.h"

#include <cmath>

namespace AuditionRenderer
{
    namespace
    {
        constexpr float kOverallPreviewGain = 0.5f;
        constexpr float kNormalizeDownThreshold = 0.9f;
        constexpr float kNormalizeDownTarget = 0.9f;
    }

    juce::AudioBuffer<float> render (const MidiSetRow& row, double bpm, double sampleRate)
    {
        jassert (sampleRate > 0.0);

        if (row.notes.empty())
            return juce::AudioBuffer<float> (1, 0);

        const double safeBpm = bpm > 0.0 ? bpm : 120.0;
        const double secondsPerBeat = 60.0 / safeBpm; // sanctioned beat->seconds conversion point

        double lastNoteEndBeats = 0.0;
        for (const auto& note : row.notes)
            lastNoteEndBeats = juce::jmax (lastNoteEndBeats, note.startBeats + note.lengthBeats);

        const int numSamples =
            (int) std::ceil ((lastNoteEndBeats * secondsPerBeat + kReleaseTailSeconds) * sampleRate);

        if (numSamples <= 0)
            return juce::AudioBuffer<float> (1, 0);

        juce::AudioBuffer<float> dest (1, numSamples);
        dest.clear();

        for (const auto& note : row.notes)
        {
            if (note.lengthBeats <= 0.0)
                continue; // degenerate note, same defensive guard as MidiFileWriter

            const int startSample = juce::roundToInt (note.startBeats * secondsPerBeat * sampleRate);
            const int noteOnSamples = juce::roundToInt (note.lengthBeats * secondsPerBeat * sampleRate);

            if (startSample >= numSamples || noteOnSamples <= 0)
                continue;

            auto voiceBuffer = AuditionVoice::render (note.pitch, note.velocity, noteOnSamples, sampleRate);

            const int copyLength = juce::jmin (voiceBuffer.getNumSamples(), numSamples - startSample);
            if (copyLength > 0)
                dest.addFrom (0, startSample, voiceBuffer, 0, 0, copyLength);
        }

        dest.applyGain (kOverallPreviewGain);

        const float peak = dest.getMagnitude (0, numSamples);
        if (peak > kNormalizeDownThreshold)
            dest.applyGain (kNormalizeDownTarget / peak); // normalize DOWN only -- never scale up

        return dest;
    }
}
