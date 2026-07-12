#pragma once

// FROZEN CONTRACT — do not change shape without updating Phase 4 plans

#include <JuceHeader.h>
#include <vector>

enum class ChordQuality { Major, Minor, Dominant7, NoChord };

// pitchClass: 0=C, 1=C#, 2=D, ... 11=B (standard 12-TET / A440 convention)
struct ChordSymbol
{
    int pitchClass = 0;
    ChordQuality quality = ChordQuality::NoChord;
};

struct ChordSegment
{
    ChordSymbol chord;
    double startSeconds = 0.0, endSeconds = 0.0;
    int startBeatIndex = 0, endBeatIndex = 0;   // indices into AnalysisResult::beatTimesSeconds
    float confidence = 0.0f;                     // Viterbi/template match score, 0..1
};

struct KeyResult
{
    int tonicPitchClass = 0;   // 0=C..11=B
    bool isMajor = true;
    float confidence = 0.0f;   // normalized Krumhansl-Kessler correlation margin
};

struct AnalysisResult
{
    double sampleRate = 0.0;
    juce::Range<double> analyzedRegionSeconds;

    double bpm = 0.0;
    std::vector<double> beatTimesSeconds;      // full beat grid
    std::vector<int> barStartBeatIndices;       // v1: every 4th beat index (4/4 assumption)

    KeyResult key;
    std::vector<ChordSegment> chords;           // beat-boundary-aligned, adjacent-equal merged

    bool wasCancelled = false;
};
