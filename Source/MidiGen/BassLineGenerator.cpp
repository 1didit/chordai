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
                // Walk each full 4-beat span within the segment: root for
                // the first half, fifth (root+7 semitones -- stays in
                // register, no %12 wrap) for the second half. A trailing
                // remainder shorter than 4 beats (or a whole segment
                // shorter than 4 beats) sustains root only.
                const int fifth = juce::jlimit (0, 127, root + 7);
                double spanOffset = 0.0;

                while (spanOffset + 4.0 <= segmentLength)
                {
                    pushNote (segmentStart + spanOffset, 2.0, root);
                    pushNote (segmentStart + spanOffset + 2.0, 2.0, fifth);
                    spanOffset += 4.0;
                }

                const double remainder = segmentLength - spanOffset;
                if (remainder > 0.0)
                    pushNote (segmentStart + spanOffset, remainder, root);

                break;
            }

            case BassRhythm::HouseFourOnFloor:
            {
                // One short, punchy note per whole beat -- sits under the
                // off-beat stabs (kHouseStabOffsetsBeats).
                constexpr double kHouseBassNoteLengthBeats = 0.5;
                for (double beatOffset = 0.0; beatOffset < segmentLength; beatOffset += 1.0)
                    pushNote (segmentStart + beatOffset, kHouseBassNoteLengthBeats, root);

                break;
            }
        }
    }

    return notes;
}
