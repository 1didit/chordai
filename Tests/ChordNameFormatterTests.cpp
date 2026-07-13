#include <JuceHeader.h>
#include <catch2/catch_test_macros.hpp>

#include "Source/UI/ChordNameFormatter.h"

TEST_CASE ("ChordNameFormatterTests.AllQualitiesAllPitchClasses", "[chordnameformatter]")
{
    constexpr const char* kNoteNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    for (int pc = 0; pc < 12; ++pc)
    {
        ChordSymbol major { pc, ChordQuality::Major };
        CHECK (chordName (major) == juce::String (kNoteNames[pc]));

        ChordSymbol minor { pc, ChordQuality::Minor };
        CHECK (chordName (minor) == juce::String (kNoteNames[pc]) + "m");

        ChordSymbol dominant7 { pc, ChordQuality::Dominant7 };
        CHECK (chordName (dominant7) == juce::String (kNoteNames[pc]) + "7");
    }

    // NoChord is "N.C." regardless of pitchClass.
    for (int pc : { 0, 5, 11 })
    {
        ChordSymbol noChord { pc, ChordQuality::NoChord };
        CHECK (chordName (noChord) == "N.C.");
    }
}
