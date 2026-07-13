#include "StyleVoicingGenerators.h"

#include "ChordToneMapper.h"
#include "Humanization.h"

// Pop/Hip-hop/Trap: plain triads only, always dropped to the triad even if
// the source chord was Dominant7 (GEN-02: "triads, dark minor" -- extensions
// are the R&B row's job, not this one's). Dark/close register (C3 anchor).
// Re-struck every 2 beats (half a 4/4 bar); a segment not evenly divisible
// by 2 beats holds its final chunk until segment end (05-RESEARCH.md
// Pattern 3's chunkSegment shape).
std::vector<NoteEvent> generatePopTrapRow (const AnalysisResult& result, const GenerationSettings&)
{
    std::vector<NoteEvent> notes;
    uint32_t noteIndex = 0;

    for (const auto& segment : result.chords)
    {
        // Pitfall 1: NoChord segments emit zero notes.
        if (segment.chord.quality == ChordQuality::NoChord)
            continue;

        auto intervals = triadIntervals (segment.chord.quality);
        if (intervals.size() > 3)
            intervals.resize (3);

        const int root = rootMidiNote (segment.chord.pitchClass, kAnchorPopTrap);
        const auto pitches = intervalsToMidiNotes (root, intervals);

        const double segStartBeats = (double) segment.startBeatIndex;
        const double segLengthBeats = (double) (segment.endBeatIndex - segment.startBeatIndex);
        constexpr double chunkBeats = 2.0;

        for (double offset = 0.0; offset < segLengthBeats; offset += chunkBeats)
        {
            const double len = juce::jmin (chunkBeats, segLengthBeats - offset);

            for (int pitch : pitches)
            {
                const float velocity = juce::jlimit (0.0f, 1.0f,
                    kVelPopTrap + deterministicJitter (noteIndex++, kSeedPopTrap, kJitterPopTrap));
                notes.push_back ({ segStartBeats + offset, len, pitch, velocity });
            }
        }
    }

    return notes;
}

// body: plan 05-02
std::vector<NoteEvent> generateRnbNeoSoulRow (const AnalysisResult&, const GenerationSettings&)
{
    return {};
}

// body: plan 05-02
std::vector<NoteEvent> generateElectronicHouseRow (const AnalysisResult&, const GenerationSettings&)
{
    return {};
}
