#include "MidiFileWriter.h"

namespace MidiFileWriter
{
    // RED stub -- Task 1 implements buildMidiFile/writeToFile for real;
    // Task 2 implements suggestedFileName for real. Compiles, tests fail.

    juce::MidiFile buildMidiFile (const MidiSetRow&, double)
    {
        return {};
    }

    bool writeToFile (const MidiSetRow&, double, const juce::File&)
    {
        return false;
    }

    juce::String suggestedFileName (const MidiSetRow& row, const KeyResult&, double)
    {
        return row.id + ".mid";
    }
}
