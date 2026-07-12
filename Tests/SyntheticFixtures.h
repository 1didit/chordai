#pragma once

// In-memory synthetic audio fixture generators for Phase 3 DSP tests.
// NO file I/O — everything renders directly into a juce::AudioBuffer<float>,
// deterministic (no RNG, or a fixed-seed juce::Random where noise is needed).
//
// STUB — RED phase of Task 3's TDD cycle. Real implementations land in the
// GREEN commit.

#include <JuceHeader.h>

#include "Source/Analysis/AnalysisResult.h"

#include <vector>

namespace fixtures
{
    // bassPitchClass == -1 means "bass = chord root".
    struct ChordSpec
    {
        ChordSymbol chord;
        int bassPitchClass = -1;
    };

    inline juce::AudioBuffer<float> renderChordProgression (const std::vector<ChordSpec>& /*chords*/,
                                                              double /*bpm*/,
                                                              double /*sampleRate*/,
                                                              int /*beatsPerChord*/ = 4,
                                                              double /*detuneCents*/ = 0.0)
    {
        return {};
    }

    inline juce::AudioBuffer<float> renderClickTrack (double /*bpm*/, double /*sampleRate*/, double /*durationSeconds*/, bool /*syncopated*/ = false)
    {
        return {};
    }

    inline void addPercussiveBursts (juce::AudioBuffer<float>& /*buffer*/, double /*bpm*/, double /*sampleRate*/)
    {
    }

    inline double toneEnergy (const juce::AudioBuffer<float>& /*buffer*/, double /*freqHz*/, double /*sampleRate*/)
    {
        return 0.0;
    }
}
