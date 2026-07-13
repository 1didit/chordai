#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/AsIsRowGenerator.h"
#include "Source/MidiGen/ChordToneMapper.h"
#include "Source/MidiGen/Humanization.h"
#include "Source/MidiGen/StyleVoicingGenerators.h"
#include "Tests/MidiGenFixtures.h"

#include <algorithm>
#include <set>

namespace
{
    bool hasNoteStartingIn (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        return std::any_of (notes.begin(), notes.end(), [lo, hi] (const NoteEvent& n)
        {
            return n.startBeats >= lo && n.startBeats < hi;
        });
    }
}

// ---------------------------------------------------------------------------
// Task 1: As-is row + Pop/Trap voicing
// ---------------------------------------------------------------------------

TEST_CASE ("StyleVoicingTests.AsIsRowIsLiteralDetectedProgression", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notes = generateAsIsRow (fixture);

    SECTION ("segment 1 (Am) is a literal root-position triad")
    {
        std::vector<NoteEvent> seg1;
        for (const auto& n : notes)
            if (n.startBeats == 0.0)
                seg1.push_back (n);

        REQUIRE (seg1.size() == 3);

        std::vector<int> pitches;
        for (const auto& n : seg1)
            pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        CHECK (pitches == std::vector<int> { 69, 72, 76 });

        for (const auto& n : seg1)
        {
            CHECK (n.startBeats == 0.0);
            CHECK (n.lengthBeats == 4.0);
            CHECK (n.velocity == 0.75f);
        }
    }

    SECTION ("segment 4 (G7) keeps its 7th -- literal detected quality")
    {
        std::vector<NoteEvent> seg4;
        for (const auto& n : notes)
            if (n.startBeats == 12.0)
                seg4.push_back (n);

        REQUIRE (seg4.size() == 4);

        std::vector<int> pitches;
        for (const auto& n : seg4)
            pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        CHECK (pitches == std::vector<int> { 67, 71, 74, 77 });
    }

    SECTION ("NoChord segment emits zero notes")
    {
        const auto noChordFixture = midigen_fixtures::makeNoChordFixture();
        const auto noChordNotes = generateAsIsRow (noChordFixture);
        CHECK_FALSE (hasNoteStartingIn (noChordNotes, 4.0, 8.0));
    }
}

TEST_CASE ("StyleVoicingTests.PopTrapTriadOnlyCloseRegister", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notes = generatePopTrapRow (fixture, GenerationSettings {});

    SECTION ("G7 segment drops the 7th -- triad only")
    {
        std::set<int> pitchesInSeg4;
        for (const auto& n : notes)
            if (n.startBeats >= 12.0 && n.startBeats < 16.0)
                pitchesInSeg4.insert (n.pitch);

        const int root = rootMidiNote (7, kAnchorPopTrap);
        std::set<int> expected { root, root + 4, root + 7 };
        CHECK (pitchesInSeg4 == expected);
    }

    SECTION ("every root pitch matches the dark C3 anchor; chord tones stay <= 66")
    {
        for (const auto& n : notes)
            CHECK (n.pitch <= 66);
    }

    SECTION ("Am strike pitches are {57, 60, 64}")
    {
        std::set<int> pitchesInSeg1;
        for (const auto& n : notes)
            if (n.startBeats >= 0.0 && n.startBeats < 4.0)
                pitchesInSeg1.insert (n.pitch);

        CHECK (pitchesInSeg1 == std::set<int> { 57, 60, 64 });
    }
}

TEST_CASE ("StyleVoicingTests.PopTrapHalfBarRestrike", "[stylevoicing]")
{
    SECTION ("4-beat segment restrikes at 0 and 2")
    {
        const auto fixture = midigen_fixtures::makeFourChordFixture();
        const auto notes = generatePopTrapRow (fixture, GenerationSettings {});

        std::set<double> onsetsInSeg1;
        for (const auto& n : notes)
            if (n.startBeats >= 0.0 && n.startBeats < 4.0)
            {
                onsetsInSeg1.insert (n.startBeats);
                CHECK (n.lengthBeats == 2.0);
            }

        CHECK (onsetsInSeg1 == std::set<double> { 0.0, 2.0 });
    }

    SECTION ("3-beat segment: strike at 0 (length 2.0) + remainder strike at 2 (length 1.0)")
    {
        const auto fixture = midigen_fixtures::makeShortSegmentFixture();
        const auto notes = generatePopTrapRow (fixture, GenerationSettings {});

        // F segment: startBeatIndex 2, endBeatIndex 5 (3 beats)
        for (const auto& n : notes)
        {
            if (n.startBeats == 2.0)
                CHECK (n.lengthBeats == 2.0);
            else if (n.startBeats == 4.0)
                CHECK (n.lengthBeats == 1.0);
        }

        CHECK (hasNoteStartingIn (notes, 2.0, 2.001));
        CHECK (hasNoteStartingIn (notes, 4.0, 4.001));
    }
}

TEST_CASE ("StyleVoicingTests.PopTrapVelocityDeterministicAndBounded", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notesA = generatePopTrapRow (fixture, GenerationSettings {});
    const auto notesB = generatePopTrapRow (fixture, GenerationSettings {});

    REQUIRE (notesA.size() == notesB.size());

    for (size_t i = 0; i < notesA.size(); ++i)
    {
        CHECK (notesA[i].velocity >= 0.72f);
        CHECK (notesA[i].velocity <= 0.84f);

        CHECK (notesA[i].startBeats == notesB[i].startBeats);
        CHECK (notesA[i].lengthBeats == notesB[i].lengthBeats);
        CHECK (notesA[i].pitch == notesB[i].pitch);
        CHECK (notesA[i].velocity == notesB[i].velocity);
    }
}

// ---------------------------------------------------------------------------
// Task 2: R&B/Neo-soul voicing with voice leading
// ---------------------------------------------------------------------------

namespace
{
    // Dedicated high-root fixture (Pitfall 3 stress case): B minor, pc 11,
    // one 4-beat segment. Not part of Tests/MidiGenFixtures.h since it's a
    // single-purpose extreme-register probe, not a general-purpose fixture.
    AnalysisResult makeBMinorFixture()
    {
        AnalysisResult result;
        result.sampleRate = 44100.0;
        result.bpm = 120.0;
        result.beatTimesSeconds = { 0.0, 0.5, 1.0, 1.5, 2.0 };
        result.barStartBeatIndices = { 0 };
        result.analyzedRegionSeconds = juce::Range<double> (0.0, 2.0);
        result.key.tonicPitchClass = 11;
        result.key.isMajor = false;
        result.key.confidence = 0.8f;
        result.wasCancelled = false;

        ChordSegment segment;
        segment.chord.pitchClass = 11;
        segment.chord.quality = ChordQuality::Minor;
        segment.startBeatIndex = 0;
        segment.endBeatIndex = 4;
        segment.startSeconds = 0.0;
        segment.endSeconds = 2.0;
        segment.confidence = 0.9f;
        result.chords = { segment };

        return result;
    }

    std::vector<int> pitchesStartingAt (const std::vector<NoteEvent>& notes, double startBeats)
    {
        std::vector<int> pitches;
        for (const auto& n : notes)
            if (n.startBeats == startBeats)
                pitches.push_back (n.pitch);
        return pitches;
    }

    double meanPitch (const std::vector<int>& pitches)
    {
        if (pitches.empty())
            return 0.0;
        double sum = 0.0;
        for (int p : pitches)
            sum += (double) p;
        return sum / (double) pitches.size();
    }
}

TEST_CASE ("StyleVoicingTests.RnbExtensionsMatchQualityTable", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notes = generateRnbNeoSoulRow (fixture, GenerationSettings {});

    auto pitchClassesStartingAt = [&] (double startBeats)
    {
        std::vector<int> classes;
        for (int p : pitchesStartingAt (notes, startBeats))
            classes.push_back (((p % 12) + 12) % 12);
        std::sort (classes.begin(), classes.end());
        return classes;
    };

    SECTION ("Am (min11) -- 6 notes, pitch classes {9,0,4,7,11,2}")
    {
        const auto pitches = pitchesStartingAt (notes, 0.0);
        REQUIRE (pitches.size() == 6);
        CHECK (pitchClassesStartingAt (0.0) == std::vector<int> { 0, 2, 4, 7, 9, 11 });
    }

    SECTION ("first-chord anchor: root == rootMidiNote(9, 60) == 69")
    {
        const auto pitches = pitchesStartingAt (notes, 0.0);
        CHECK (std::find (pitches.begin(), pitches.end(), 69) != pitches.end());
    }

    SECTION ("G7 (dom9) -- 5 notes, pitch classes {7,11,2,5,9}")
    {
        const auto pitches = pitchesStartingAt (notes, 12.0);
        REQUIRE (pitches.size() == 5);
        CHECK (pitchClassesStartingAt (12.0) == std::vector<int> { 2, 5, 7, 9, 11 });
    }
}

TEST_CASE ("StyleVoicingTests.RnbVoiceLeadingMinimizesMovement", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notes = generateRnbNeoSoulRow (fixture, GenerationSettings {});

    const std::vector<double> segmentStarts = { 0.0, 4.0, 8.0, 12.0 };
    bool foundStrictlyLess = false;

    for (size_t i = 1; i < segmentStarts.size(); ++i)
    {
        const auto previousPitches = pitchesStartingAt (notes, segmentStarts[i - 1]);
        const auto actualPitches = pitchesStartingAt (notes, segmentStarts[i]);
        const double previousChordMeanPitch = meanPitch (previousPitches);

        double voiceLedMovement = 0.0;
        for (int p : actualPitches)
            voiceLedMovement += std::abs ((double) p - previousChordMeanPitch);

        const auto& segment = fixture.chords[i];
        const int naiveRoot = rootMidiNote (segment.chord.pitchClass, 60);
        const auto naivePitches = intervalsToMidiNotes (naiveRoot, rnbExtensionIntervals (segment.chord.quality));

        double naiveMovement = 0.0;
        for (int p : naivePitches)
            naiveMovement += std::abs ((double) p - previousChordMeanPitch);

        CHECK (voiceLedMovement <= naiveMovement);
        if (voiceLedMovement < naiveMovement)
            foundStrictlyLess = true;
    }

    CHECK (foundStrictlyLess);
}

TEST_CASE ("StyleVoicingTests.RnbRegisterClampHolds", "[stylevoicing]")
{
    const auto fixture = makeBMinorFixture();
    const auto notes = generateRnbNeoSoulRow (fixture, GenerationSettings {});

    REQUIRE_FALSE (notes.empty());
    for (const auto& n : notes)
    {
        CHECK (n.pitch >= kRnbRegisterLow);
        CHECK (n.pitch <= kRnbRegisterHigh);
    }
}

TEST_CASE ("StyleVoicingTests.RnbVelocitySoftAndDeterministic", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notesA = generateRnbNeoSoulRow (fixture, GenerationSettings {});
    const auto notesB = generateRnbNeoSoulRow (fixture, GenerationSettings {});

    REQUIRE (notesA.size() == notesB.size());

    for (size_t i = 0; i < notesA.size(); ++i)
    {
        CHECK (notesA[i].velocity >= 0.58f);
        CHECK (notesA[i].velocity <= 0.66f);

        CHECK (notesA[i].startBeats == notesB[i].startBeats);
        CHECK (notesA[i].lengthBeats == notesB[i].lengthBeats);
        CHECK (notesA[i].pitch == notesB[i].pitch);
        CHECK (notesA[i].velocity == notesB[i].velocity);
    }
}
