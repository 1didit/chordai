#include "AsIsRowGenerator.h"

#include "ChordToneMapper.h"
#include "Humanization.h"

// Literal detected progression: root-position, exact detected quality (a
// Dominant7 source chord keeps its 4th tone here -- style transformation is
// the other generators' job), full-segment sustain, flat velocity (no
// jitter -- this row reads as raw data, 05-RESEARCH.md Pattern 5 table).
std::vector<NoteEvent> generateAsIsRow (const AnalysisResult& result)
{
    std::vector<NoteEvent> notes;

    for (const auto& segment : result.chords)
    {
        // Pitfall 1: NoChord segments emit zero notes, in every row.
        if (segment.chord.quality == ChordQuality::NoChord)
            continue;

        const auto intervals = triadIntervals (segment.chord.quality);
        const int root = rootMidiNote (segment.chord.pitchClass, kAnchorAsIs);
        const auto pitches = intervalsToMidiNotes (root, intervals);

        const double startBeats = (double) segment.startBeatIndex;
        const double lengthBeats = (double) (segment.endBeatIndex - segment.startBeatIndex);

        for (int pitch : pitches)
            notes.push_back ({ startBeats, lengthBeats, pitch, kVelAsIs });
    }

    return notes;
}
