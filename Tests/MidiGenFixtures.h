#pragma once

// Hand-built struct-literal fixture AnalysisResults for MidiGen tests -- NO
// audio rendering (unlike Tests/SyntheticFixtures.h, these feed pure
// generator functions directly). Every fixture is internally consistent:
// beatTimesSeconds spaced 60/bpm apart starting at 0, barStartBeatIndices
// every 4th index, and every ChordSegment's startSeconds/endSeconds derived
// from its own beat indices (index * 60/bpm) -- never a second, independent
// seconds computation that could drift from the beat grid.

#include "Source/Analysis/AnalysisResult.h"

#include <vector>

namespace midigen_fixtures
{
    namespace detail
    {
        inline std::vector<double> makeBeatGrid (int numBeats, double bpm)
        {
            std::vector<double> beats;
            beats.reserve ((size_t) numBeats + 1);
            const double beatSeconds = 60.0 / bpm;
            for (int i = 0; i <= numBeats; ++i)
                beats.push_back ((double) i * beatSeconds);
            return beats;
        }

        // Every 4th beat index (4/4 assumption), stopping before an
        // incomplete trailing bar -- matches AnalysisResult.h's own
        // "barStartBeatIndices" contract comment.
        inline std::vector<int> makeBarStarts (int numBeats)
        {
            std::vector<int> bars;
            for (int i = 0; i < numBeats; i += 4)
                bars.push_back (i);
            return bars;
        }

        inline ChordSegment makeSegment (int pitchClass, ChordQuality quality,
                                          int startBeatIndex, int endBeatIndex, double bpm,
                                          float confidence = 0.9f)
        {
            ChordSegment segment;
            segment.chord.pitchClass = pitchClass;
            segment.chord.quality = quality;
            segment.startBeatIndex = startBeatIndex;
            segment.endBeatIndex = endBeatIndex;
            segment.startSeconds = (double) startBeatIndex * 60.0 / bpm;
            segment.endSeconds = (double) endBeatIndex * 60.0 / bpm;
            segment.confidence = confidence;
            return segment;
        }
    }

    // 120 BPM, 16 beats (17 beatTimes entries 0.0..8.0), bars {0,4,8,12}, key
    // A minor. Am -> F -> C -> G7, 4 beats each -- covers all 3 pitched
    // qualities (GEN-01/02's happy path).
    inline AnalysisResult makeFourChordFixture()
    {
        constexpr double bpm = 120.0;
        constexpr int numBeats = 16;

        AnalysisResult result;
        result.sampleRate = 44100.0;
        result.bpm = bpm;
        result.beatTimesSeconds = detail::makeBeatGrid (numBeats, bpm);
        result.barStartBeatIndices = detail::makeBarStarts (numBeats);
        result.analyzedRegionSeconds = juce::Range<double> (0.0, result.beatTimesSeconds.back());
        result.key.tonicPitchClass = 9; // A
        result.key.isMajor = false;
        result.key.confidence = 0.85f;
        result.wasCancelled = false;

        result.chords = {
            detail::makeSegment (9, ChordQuality::Minor,     0,  4, bpm),  // Am
            detail::makeSegment (5, ChordQuality::Major,     4,  8, bpm),  // F
            detail::makeSegment (0, ChordQuality::Major,     8, 12, bpm),  // C
            detail::makeSegment (7, ChordQuality::Dominant7, 12, 16, bpm), // G7
        };

        return result;
    }

    // Same 120 BPM grid convention; Pitfall-1 negative case -- middle
    // segment is NoChord.
    inline AnalysisResult makeNoChordFixture()
    {
        constexpr double bpm = 120.0;
        constexpr int numBeats = 12;

        AnalysisResult result;
        result.sampleRate = 44100.0;
        result.bpm = bpm;
        result.beatTimesSeconds = detail::makeBeatGrid (numBeats, bpm);
        result.barStartBeatIndices = detail::makeBarStarts (numBeats);
        result.analyzedRegionSeconds = juce::Range<double> (0.0, result.beatTimesSeconds.back());
        result.key.tonicPitchClass = 0; // C
        result.key.isMajor = true;
        result.key.confidence = 0.7f;
        result.wasCancelled = false;

        result.chords = {
            detail::makeSegment (0, ChordQuality::Major,   0, 4, bpm),  // C
            detail::makeSegment (0, ChordQuality::NoChord, 4, 8, bpm),  // silence
            detail::makeSegment (9, ChordQuality::Minor,   8, 12, bpm), // Am
        };

        return result;
    }

    // 120 BPM; uneven segment lengths -- exercises Pop/Trap re-strike
    // remainder, R&B bass short-segment rule, House span truncation.
    inline AnalysisResult makeShortSegmentFixture()
    {
        constexpr double bpm = 120.0;
        constexpr int numBeats = 9;

        AnalysisResult result;
        result.sampleRate = 44100.0;
        result.bpm = bpm;
        result.beatTimesSeconds = detail::makeBeatGrid (numBeats, bpm);
        result.barStartBeatIndices = detail::makeBarStarts (numBeats);
        result.analyzedRegionSeconds = juce::Range<double> (0.0, result.beatTimesSeconds.back());
        result.key.tonicPitchClass = 0;
        result.key.isMajor = true;
        result.key.confidence = 0.75f;
        result.wasCancelled = false;

        result.chords = {
            detail::makeSegment (0, ChordQuality::Major, 0, 2, bpm), // C, 2 beats
            detail::makeSegment (5, ChordQuality::Major, 2, 5, bpm), // F, 3 beats
            detail::makeSegment (7, ChordQuality::Major, 5, 9, bpm), // G, 4 beats
        };

        return result;
    }

    // ~150 segments, 4 beats each (600 beats, ~170 BPM to mirror TOCK-scale),
    // cycling pitch classes and qualities, every 10th segment NoChord --
    // exercises the <1ms performance budget test against a real-track-sized
    // progression.
    inline AnalysisResult makeRealTrackScaleFixture()
    {
        constexpr double bpm = 170.0;
        constexpr int numSegments = 150;
        constexpr int beatsPerSegment = 4;
        constexpr int numBeats = numSegments * beatsPerSegment;

        AnalysisResult result;
        result.sampleRate = 44100.0;
        result.bpm = bpm;
        result.beatTimesSeconds = detail::makeBeatGrid (numBeats, bpm);
        result.barStartBeatIndices = detail::makeBarStarts (numBeats);
        result.analyzedRegionSeconds = juce::Range<double> (0.0, result.beatTimesSeconds.back());
        result.key.tonicPitchClass = 0;
        result.key.isMajor = true;
        result.key.confidence = 0.8f;
        result.wasCancelled = false;

        result.chords.reserve ((size_t) numSegments);
        constexpr ChordQuality cyclingQualities[3] = { ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7 };

        for (int i = 0; i < numSegments; ++i)
        {
            const int startBeatIndex = i * beatsPerSegment;
            const int endBeatIndex = startBeatIndex + beatsPerSegment;
            const int pitchClass = i % 12;

            if (i % 10 == 0)
                result.chords.push_back (detail::makeSegment (pitchClass, ChordQuality::NoChord, startBeatIndex, endBeatIndex, bpm));
            else
                result.chords.push_back (detail::makeSegment (pitchClass, cyclingQualities[i % 3], startBeatIndex, endBeatIndex, bpm));
        }

        return result;
    }
}
