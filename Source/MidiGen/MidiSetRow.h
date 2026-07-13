#pragma once

#include <JuceHeader.h>

#include "NoteEvent.h"

#include <vector>

// RowStyle enumerates the 5 rows one generateAllRows() call always produces
// (GEN-01): the detected-as-is reference row, 3 style variants (GEN-02), and
// the bass row (GEN-03).
enum class RowStyle { DetectedAsIs, PopHipHopTrap, RnbNeoSoul, ElectronicHouse, Bass };

struct MidiSetRow
{
    juce::String id;       // stable key: "as-is", "pop-trap", "rnb-neosoul", "house", "bass"
    juce::String label;    // display label: "Detected", "Pop / Trap", "R&B / Neo-Soul", "Electronic / House", "Bass"
    RowStyle style = RowStyle::DetectedAsIs;
    std::vector<NoteEvent> notes;  // every NoteEvent for the ENTIRE analyzed region for this
                                    // style -- one row per style, not one row per chord
                                    // (see 05-RESEARCH.md Anti-Patterns).
};
