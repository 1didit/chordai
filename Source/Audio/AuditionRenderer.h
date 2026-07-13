#pragma once

#include <JuceHeader.h>

#include "../MidiGen/MidiSetRow.h"

// Message-thread-only pre-renderer: converts a whole MidiSetRow into a fixed
// mono PCM buffer that Source/PluginProcessor.h's double-buffer handoff can
// hand to processBlock. This is the second of the two sanctioned
// beat->seconds conversion points in the codebase (the first is
// Source/MidiGen/MidiFileWriter.cpp's beat->tick conversion) -- see
// 06-RESEARCH.md Anti-Patterns: do not re-derive beat/seconds conversions
// anywhere else.
namespace AuditionRenderer
{
    // Padding (seconds) appended after the last note's own end so its
    // release tail has room to fully decay inside the returned buffer.
    // Public: tests compute expected buffer length with it.
    constexpr double kReleaseTailSeconds = 0.2;

    // Message-thread only; allocates freely (AudioFileLoadJob-decode
    // allocation category, not the processBlock "prepareToPlay only" rule,
    // which is about the AUDIO thread specifically).
    //
    // Deterministic: same row + bpm + sampleRate always produces
    // byte-identical output. bpm <= 0 is defensively treated as 120.0 (same
    // guard as MidiFileWriter::buildMidiFile). Mono (1 channel).
    //
    // numSamples == (int) std::ceil(((lastNoteEndBeats * 60.0 / safeBpm) +
    // kReleaseTailSeconds) * sampleRate), where lastNoteEndBeats is the max
    // over all notes of (startBeats + lengthBeats); 0 samples for an empty
    // row (row.notes.empty()).
    juce::AudioBuffer<float> render (const MidiSetRow& row, double bpm, double sampleRate);
}
