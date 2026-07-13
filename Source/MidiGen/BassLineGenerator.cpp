#include "BassLineGenerator.h"

#include "ChordToneMapper.h"
#include "Humanization.h"

// Bass row: detected chord root at C2 (kAnchorBass) with a per-style
// rhythm. Pure and deterministic -- see 05-RESEARCH.md Pattern 3 bass table.
std::vector<NoteEvent> generateBassRow (const AnalysisResult& result, BassRhythm rhythm)
{
    std::vector<NoteEvent> notes;
    uint32_t noteIndex = 0;

    auto pushNote = [&] (double startBeats, double lengthBeats, int pitch)
    {
        NoteEvent note;
        note.startBeats = startBeats;
        note.lengthBeats = lengthBeats;
        note.pitch = pitch;
        note.velocity = juce::jlimit (0.0f, 1.0f,
            kVelBass + deterministicJitter (noteIndex++, kSeedBass, kJitterBass));
        notes.push_back (note);
    };

    for (const auto& segment : result.chords)
    {
        if (segment.chord.quality == ChordQuality::NoChord)
            continue; // Pitfall 1: skip NoChord segments entirely

        const int root = rootMidiNote (segment.chord.pitchClass, kAnchorBass);
        const double segmentStart = (double) segment.startBeatIndex;
        const double segmentLength = (double) (segment.endBeatIndex - segment.startBeatIndex);

        switch (rhythm)
        {
            case BassRhythm::TrapSustain:
            {
                // Sparse, one sustained note spanning the whole segment --
                // leaves room for the 808.
                pushNote (segmentStart, segmentLength, root);
                break;
            }

            case BassRhythm::RnbRootFifth:
            {
                // Implemented in plan 05-03 Task 2.
                break;
            }

            case BassRhythm::HouseFourOnFloor:
            {
                // Implemented in plan 05-03 Task 2.
                break;
            }
        }
    }

    return notes;
}
