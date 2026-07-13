#include <JuceHeader.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Source/MidiGen/MidiFileWriter.h"

#include <cmath>
#include <memory>
#include <vector>

namespace
{
    // 3+ notes at non-trivial beat positions, distinct pitches -- one note
    // (startBeats 4.0) lands exactly on bar 2 in 4/4 at TPQN 960.
    MidiSetRow makeTestRow()
    {
        MidiSetRow row;
        row.id = "as-is";
        row.label = "Detected";
        row.kind = PatternKind::SustainedChords;
        row.patternIndex = 0;
        row.notes = {
            { 0.0, 1.0, 60, 0.8f },
            { 1.5, 0.25, 64, 0.8f },
            { 4.0, 2.0, 67, 0.8f },
        };
        return row;
    }

    juce::MidiFile roundTripThroughMemory (const juce::MidiFile& file)
    {
        juce::MemoryOutputStream mos;
        REQUIRE (file.writeTo (mos, 1));

        juce::MidiFile readBack;
        juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
        REQUIRE (readBack.readFrom (mis));
        return readBack;
    }

    juce::File makeTempDestination (const juce::String& fileName)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("ChordAITests_MidiFileWriter");
        dir.createDirectory();
        auto destination = dir.getChildFile (fileName);
        destination.deleteFile();
        return destination;
    }
}

TEST_CASE ("MidiFileWriterTests.RoundTripTicksTempoTimeSigAndBarAlignment", "[midifilewriter]")
{
    const auto row = makeTestRow();
    const double bpm = 128.2; // deliberately non-integer

    const auto readBack = roundTripThroughMemory (MidiFileWriter::buildMidiFile (row, bpm));

    CHECK (readBack.getTimeFormat() == MidiFileWriter::kTicksPerQuarterNote);
    REQUIRE (readBack.getNumTracks() == 2);

    const auto* metaTrack = readBack.getTrack (0);
    REQUIRE (metaTrack != nullptr);

    bool foundTempo = false;
    bool foundTimeSig = false;
    for (int i = 0; i < metaTrack->getNumEvents(); ++i)
    {
        const auto& msg = metaTrack->getEventPointer (i)->message;
        if (msg.isTempoMetaEvent())
        {
            foundTempo = true;
            CHECK (msg.getTempoSecondsPerQuarterNote() == Catch::Approx (60.0 / bpm).margin (1e-4));
        }
        if (msg.isTimeSignatureMetaEvent())
        {
            foundTimeSig = true;
            int numerator = 0, denominator = 0;
            msg.getTimeSignatureInfo (numerator, denominator);
            CHECK (numerator == 4);
            CHECK (denominator == 4);
        }
    }
    CHECK (foundTempo);
    CHECK (foundTimeSig);

    const auto* noteTrack = readBack.getTrack (1);
    REQUIRE (noteTrack != nullptr);

    const int tpqn = MidiFileWriter::kTicksPerQuarterNote;
    for (const auto& note : row.notes)
    {
        const double expectedOnTick = note.startBeats * (double) tpqn;
        const double expectedOffTick = (note.startBeats + note.lengthBeats) * (double) tpqn;

        bool foundOn = false, foundOff = false;
        for (int i = 0; i < noteTrack->getNumEvents(); ++i)
        {
            const auto& msg = noteTrack->getEventPointer (i)->message;
            if (msg.isNoteOn() && msg.getNoteNumber() == note.pitch
                && std::abs (msg.getTimeStamp() - expectedOnTick) <= 1.0)
            {
                foundOn = true;
                CHECK (msg.getFloatVelocity() == Catch::Approx (note.velocity).margin (1.0f / 127.0f));
            }
            if (msg.isNoteOff() && msg.getNoteNumber() == note.pitch
                && std::abs (msg.getTimeStamp() - expectedOffTick) <= 1.0)
            {
                foundOff = true;
            }
        }
        CHECK (foundOn);
        CHECK (foundOff);
    }

    // Bar alignment falls out of the beat domain: startBeats 4.0 lands at
    // tick 3840 = exactly bar 2 in 4/4 at TPQN 960 (already asserted above
    // via the foundOn/foundOff loop for that note; restated explicitly here).
    CHECK (4.0 * (double) tpqn == 3840.0);
}

TEST_CASE ("MidiFileWriterTests.BpmFallbackWhenZeroOrNegative", "[midifilewriter]")
{
    const auto row = makeTestRow();

    for (double bpm : { 0.0, -5.0 })
    {
        const auto readBack = roundTripThroughMemory (MidiFileWriter::buildMidiFile (row, bpm));

        const auto* metaTrack = readBack.getTrack (0);
        REQUIRE (metaTrack != nullptr);

        bool foundTempo = false;
        for (int i = 0; i < metaTrack->getNumEvents(); ++i)
        {
            const auto& msg = metaTrack->getEventPointer (i)->message;
            if (msg.isTempoMetaEvent())
            {
                foundTempo = true;
                CHECK (msg.getTempoSecondsPerQuarterNote() == Catch::Approx (60.0 / 120.0).margin (1e-4));
            }
        }
        CHECK (foundTempo);
    }
}

TEST_CASE ("MidiFileWriterTests.ZeroOrNegativeLengthNotesAreDropped", "[midifilewriter]")
{
    MidiSetRow row;
    row.id = "as-is";
    row.notes = {
        { 0.0, 1.0, 60, 0.8f },  // well-formed -- must round-trip
        { 2.0, 0.0, 62, 0.8f },  // lengthBeats == 0.0 -- must be dropped
        { 3.0, -1.0, 64, 0.8f }, // lengthBeats < 0.0 -- must be dropped
    };

    const auto readBack = roundTripThroughMemory (MidiFileWriter::buildMidiFile (row, 120.0));

    const auto* noteTrack = readBack.getTrack (1);
    REQUIRE (noteTrack != nullptr);

    int noteOnCount = 0;
    bool foundPitch60 = false, foundPitch62 = false, foundPitch64 = false;
    for (int i = 0; i < noteTrack->getNumEvents(); ++i)
    {
        const auto& msg = noteTrack->getEventPointer (i)->message;
        if (msg.isNoteOn())
        {
            ++noteOnCount;
            if (msg.getNoteNumber() == 60) foundPitch60 = true;
            if (msg.getNoteNumber() == 62) foundPitch62 = true;
            if (msg.getNoteNumber() == 64) foundPitch64 = true;
        }
    }

    CHECK (noteOnCount == 1);
    CHECK (foundPitch60);
    CHECK_FALSE (foundPitch62);
    CHECK_FALSE (foundPitch64);
}

TEST_CASE ("MidiFileWriterTests.WriteToFileCreatesValidFile", "[midifilewriter]")
{
    const auto row = makeTestRow();
    auto destination = makeTempDestination ("test-write.mid");

    REQUIRE (MidiFileWriter::writeToFile (row, 120.0, destination));
    REQUIRE (destination.existsAsFile());
    CHECK (destination.getSize() > 0);

    juce::MidiFile readBack;
    std::unique_ptr<juce::FileInputStream> stream (destination.createInputStream());
    REQUIRE (stream != nullptr);
    REQUIRE (readBack.readFrom (*stream));
    CHECK (readBack.getNumTracks() == 2);

    destination.getParentDirectory().deleteRecursively();
}

TEST_CASE ("MidiFileWriterTests.WriteToFileTwiceOverwritesNotAppends", "[midifilewriter]")
{
    const auto row = makeTestRow();
    auto destination = makeTempDestination ("test-overwrite.mid");

    REQUIRE (MidiFileWriter::writeToFile (row, 120.0, destination));

    juce::MidiFile firstRead;
    {
        std::unique_ptr<juce::FileInputStream> stream (destination.createInputStream());
        REQUIRE (stream != nullptr);
        REQUIRE (firstRead.readFrom (*stream));
    }
    REQUIRE (firstRead.getTrack (1) != nullptr);
    const int firstEventCount = firstRead.getTrack (1)->getNumEvents();

    REQUIRE (MidiFileWriter::writeToFile (row, 120.0, destination));

    juce::MidiFile secondRead;
    {
        std::unique_ptr<juce::FileInputStream> stream (destination.createInputStream());
        REQUIRE (stream != nullptr);
        REQUIRE (secondRead.readFrom (*stream));
    }
    REQUIRE (secondRead.getNumTracks() == 2);
    REQUIRE (secondRead.getTrack (1) != nullptr);
    CHECK (secondRead.getTrack (1)->getNumEvents() == firstEventCount);

    destination.getParentDirectory().deleteRecursively();
}

TEST_CASE ("MidiFileWriterTests.EmptyRowProducesValidMinimalFile", "[midifilewriter]")
{
    MidiSetRow row;
    row.id = "as-is"; // row.notes left empty

    const auto readBack = roundTripThroughMemory (MidiFileWriter::buildMidiFile (row, 120.0));

    REQUIRE (readBack.getNumTracks() == 2);
    const auto* metaTrack = readBack.getTrack (0);
    const auto* noteTrack = readBack.getTrack (1);
    REQUIRE (metaTrack != nullptr);
    REQUIRE (noteTrack != nullptr);
    CHECK (metaTrack->getNumEvents() > 0);

    bool hasNoteOn = false;
    for (int i = 0; i < noteTrack->getNumEvents(); ++i)
        if (noteTrack->getEventPointer (i)->message.isNoteOn())
            hasNoteOn = true;
    CHECK_FALSE (hasNoteOn);

    auto destination = makeTempDestination ("test-empty.mid");
    REQUIRE (MidiFileWriter::writeToFile (row, 120.0, destination));
    REQUIRE (destination.existsAsFile());
    destination.getParentDirectory().deleteRecursively();
}

TEST_CASE ("MidiFileWriterTests.SuggestedFileNameMinorKey", "[midifilewriter]")
{
    MidiSetRow row;
    row.id = "pop-trap";

    KeyResult key;
    key.tonicPitchClass = 8; // G#
    key.isMajor = false;

    CHECK (MidiFileWriter::suggestedFileName (row, key, 128.2) == "pop-trap_G#m_128bpm.mid");
}

TEST_CASE ("MidiFileWriterTests.SuggestedFileNameMajorKey", "[midifilewriter]")
{
    MidiSetRow row;
    row.id = "as-is";

    KeyResult key;
    key.tonicPitchClass = 0; // C
    key.isMajor = true;

    CHECK (MidiFileWriter::suggestedFileName (row, key, 90.0) == "as-is_C_90bpm.mid");
}

TEST_CASE ("MidiFileWriterTests.SuggestedFileNameBpmFallback", "[midifilewriter]")
{
    MidiSetRow row;
    row.id = "bass";

    KeyResult key;
    key.tonicPitchClass = 0;
    key.isMajor = true;

    for (double bpm : { 0.0, -5.0 })
        CHECK (MidiFileWriter::suggestedFileName (row, key, bpm) == "bass_C_120bpm.mid");
}

TEST_CASE ("MidiFileWriterTests.SuggestedFileNameIsFilesystemSafe", "[midifilewriter]")
{
    const juce::String forbiddenChars = "/\\:*?\"<>|";
    const std::vector<juce::String> rowIds = { "as-is", "pop-trap", "rnb-neosoul", "house", "bass" };

    for (const auto& rowId : rowIds)
    {
        MidiSetRow row;
        row.id = rowId;

        for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        {
            for (bool isMajor : { true, false })
            {
                KeyResult key;
                key.tonicPitchClass = pitchClass;
                key.isMajor = isMajor;

                const auto name = MidiFileWriter::suggestedFileName (row, key, 128.2);

                for (auto forbidden : forbiddenChars)
                    CHECK_FALSE (name.containsChar (forbidden));

                CHECK_FALSE (name.containsAnyOf (" \t\n"));
            }
        }
    }
}

TEST_CASE ("MidiFileWriterTests.SuggestedFileNameUniquePerRow", "[midifilewriter]")
{
    const std::vector<juce::String> rowIds = { "as-is", "pop-trap", "rnb-neosoul", "house", "bass" };

    KeyResult key;
    key.tonicPitchClass = 8;
    key.isMajor = false;

    std::vector<juce::String> names;
    for (const auto& rowId : rowIds)
    {
        MidiSetRow row;
        row.id = rowId;
        names.push_back (MidiFileWriter::suggestedFileName (row, key, 128.2));
    }

    for (size_t i = 0; i < names.size(); ++i)
        for (size_t j = i + 1; j < names.size(); ++j)
            CHECK (names[i] != names[j]);
}
