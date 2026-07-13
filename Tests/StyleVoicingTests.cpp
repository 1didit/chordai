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

// ---------------------------------------------------------------------------
// Task 3: House stabs + cross-style distinctness + progression-tracking
// ---------------------------------------------------------------------------

namespace
{
    // Single 6-beat segment -- exercises the House stab pattern's second
    // 4-beat span truncation (offsets 4.5/5.5 kept, 6.5/7.5 dropped).
    AnalysisResult makeSixBeatSegmentFixture()
    {
        AnalysisResult result;
        result.sampleRate = 44100.0;
        result.bpm = 120.0;
        result.beatTimesSeconds = { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0 };
        result.barStartBeatIndices = { 0, 4 };
        result.analyzedRegionSeconds = juce::Range<double> (0.0, 3.0);
        result.key.tonicPitchClass = 9;
        result.key.isMajor = false;
        result.key.confidence = 0.8f;
        result.wasCancelled = false;

        ChordSegment segment;
        segment.chord.pitchClass = 9;
        segment.chord.quality = ChordQuality::Minor;
        segment.startBeatIndex = 0;
        segment.endBeatIndex = 6;
        segment.startSeconds = 0.0;
        segment.endSeconds = 3.0;
        segment.confidence = 0.9f;
        result.chords = { segment };

        return result;
    }

    std::set<double> onsetsInRange (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        std::set<double> onsets;
        for (const auto& n : notes)
            if (n.startBeats >= lo && n.startBeats < hi)
                onsets.insert (n.startBeats);
        return onsets;
    }

    bool notesEqual (const std::vector<NoteEvent>& a, const std::vector<NoteEvent>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].startBeats != b[i].startBeats || a[i].lengthBeats != b[i].lengthBeats
                || a[i].pitch != b[i].pitch || a[i].velocity != b[i].velocity)
                return false;
        return true;
    }
}

TEST_CASE ("StyleVoicingTests.HouseStabPatternExactTiming", "[stylevoicing]")
{
    SECTION ("4-beat Am segment: stabs at 0.5/1.5/2.5/3.5, each a 3-note triad, length 0.25")
    {
        const auto fixture = midigen_fixtures::makeFourChordFixture();
        const auto notes = generateElectronicHouseRow (fixture, GenerationSettings {});

        const auto onsets = onsetsInRange (notes, 0.0, 4.0);
        CHECK (onsets == std::set<double> { 0.5, 1.5, 2.5, 3.5 });

        for (double onset : onsets)
        {
            int count = 0;
            for (const auto& n : notes)
                if (n.startBeats == onset)
                {
                    CHECK (n.lengthBeats == 0.25);
                    ++count;
                }
            CHECK (count == 3);
        }
    }

    SECTION ("3-beat segment (start beat 2): stabs at 2.5/3.5/4.5 only")
    {
        const auto fixture = midigen_fixtures::makeShortSegmentFixture();
        const auto notes = generateElectronicHouseRow (fixture, GenerationSettings {});

        const auto onsets = onsetsInRange (notes, 2.0, 5.0);
        CHECK (onsets == std::set<double> { 2.5, 3.5, 4.5 });
    }

    SECTION ("6-beat segment: first span full, second span truncated to 4.5/5.5")
    {
        const auto fixture = makeSixBeatSegmentFixture();
        const auto notes = generateElectronicHouseRow (fixture, GenerationSettings {});

        const auto onsets = onsetsInRange (notes, 0.0, 6.0);
        CHECK (onsets == std::set<double> { 0.5, 1.5, 2.5, 3.5, 4.5, 5.5 });
    }
}

TEST_CASE ("StyleVoicingTests.HouseBrightRegister", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();
    const auto notes = generateElectronicHouseRow (fixture, GenerationSettings {});

    std::set<int> pitchesInSeg1;
    for (const auto& n : notes)
        if (n.startBeats >= 0.0 && n.startBeats < 4.0)
            pitchesInSeg1.insert (n.pitch);

    CHECK (pitchesInSeg1 == std::set<int> { 81, 84, 88 });

    const int expectedRoot = rootMidiNote (9, kAnchorHouse);
    CHECK (expectedRoot == 81);
}

TEST_CASE ("StyleVoicingTests.ThreeStylesProduceDistinctContent", "[stylevoicing]")
{
    const auto fixture = midigen_fixtures::makeFourChordFixture();

    const auto asIsNotes = generateAsIsRow (fixture);
    const auto popTrapNotes = generatePopTrapRow (fixture, GenerationSettings {});
    const auto rnbNotes = generateRnbNeoSoulRow (fixture, GenerationSettings {});
    const auto houseNotes = generateElectronicHouseRow (fixture, GenerationSettings {});

    auto pitchClassMultiset = [] (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        std::vector<int> classes;
        for (const auto& n : notes)
            if (n.startBeats >= lo && n.startBeats < hi)
                classes.push_back (((n.pitch % 12) + 12) % 12);
        std::sort (classes.begin(), classes.end());
        return classes;
    };

    auto meanPitchInRange = [] (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        double sum = 0.0;
        int count = 0;
        for (const auto& n : notes)
            if (n.startBeats >= lo && n.startBeats < hi)
            {
                sum += n.pitch;
                ++count;
            }
        return count > 0 ? sum / count : 0.0;
    };

    SECTION ("R&B pitch-class multiset differs from Pop/Trap's and House's")
    {
        const auto rnbClasses = pitchClassMultiset (rnbNotes, 0.0, 4.0);
        const auto popClasses = pitchClassMultiset (popTrapNotes, 0.0, 4.0);
        const auto houseClasses = pitchClassMultiset (houseNotes, 0.0, 4.0);

        CHECK (rnbClasses != popClasses);
        CHECK (rnbClasses != houseClasses);
    }

    SECTION ("House onset-beat multiset differs from Pop/Trap's")
    {
        const auto houseOnsets = onsetsInRange (houseNotes, 0.0, 4.0);
        const auto popOnsets = onsetsInRange (popTrapNotes, 0.0, 4.0);

        CHECK (houseOnsets == std::set<double> { 0.5, 1.5, 2.5, 3.5 });
        CHECK (popOnsets == std::set<double> { 0.0, 2.0 });
        CHECK (houseOnsets != popOnsets);
    }

    SECTION ("mean pitch register separation: House > As-is > Pop/Trap")
    {
        const double houseMean = meanPitchInRange (houseNotes, 0.0, 4.0);
        const double asIsMean = meanPitchInRange (asIsNotes, 0.0, 4.0);
        const double popMean = meanPitchInRange (popTrapNotes, 0.0, 4.0);

        CHECK (houseMean > asIsMean);
        CHECK (asIsMean > popMean);
    }
}

TEST_CASE ("StyleVoicingTests.OutputTracksInputProgression", "[stylevoicing]")
{
    auto original = midigen_fixtures::makeFourChordFixture();
    auto modified = original;
    modified.chords[1].chord.pitchClass = 2;               // was F Major (pc 5)
    modified.chords[1].chord.quality = ChordQuality::Minor; // now D Minor (pc 2)

    auto notesInRange = [] (const std::vector<NoteEvent>& notes, double lo, double hi)
    {
        std::vector<NoteEvent> result;
        for (const auto& n : notes)
            if (n.startBeats >= lo && n.startBeats < hi)
                result.push_back (n);
        return result;
    };

    auto checkGenerator = [&] (auto generatorFn)
    {
        const auto originalNotes = generatorFn (original);
        const auto modifiedNotes = generatorFn (modified);

        const auto originalFirst = notesInRange (originalNotes, 0.0, 4.0);
        const auto modifiedFirst = notesInRange (modifiedNotes, 0.0, 4.0);
        CHECK (notesEqual (originalFirst, modifiedFirst));

        const auto originalSecond = notesInRange (originalNotes, 4.0, 8.0);
        const auto modifiedSecond = notesInRange (modifiedNotes, 4.0, 8.0);
        CHECK_FALSE (notesEqual (originalSecond, modifiedSecond));
    };

    checkGenerator ([] (const AnalysisResult& r) { return generateAsIsRow (r); });
    checkGenerator ([] (const AnalysisResult& r) { return generatePopTrapRow (r, GenerationSettings {}); });
    checkGenerator ([] (const AnalysisResult& r) { return generateRnbNeoSoulRow (r, GenerationSettings {}); });
    checkGenerator ([] (const AnalysisResult& r) { return generateElectronicHouseRow (r, GenerationSettings {}); });
}
