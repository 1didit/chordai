#include "StyleVoicingGenerators.h"

#include "ChordToneMapper.h"
#include "Humanization.h"
#include "VoiceLeadingEngine.h"

#include <cmath>

namespace
{
    // Register clamp that preserves pitch class by octave-shifting, instead
    // of a truncating juce::jlimit -- the R&B register band (kRnbRegisterLow
    // to kRnbRegisterHigh) spans a full 24 semitones (2 octaves), so every
    // pitch class always has a representative inside it without ever needing
    // to collapse to the boundary and lose its identity. Used only to seed
    // the FIRST voiced chord (Pattern 2); every subsequent chord instead
    // voice-leads via nearestOctaveNote (VoiceLeadingEngine.h), whose own
    // final clamp is the documented Pitfall 3 approximation.
    int clampToRegisterByOctave (int pitch, int registerLow, int registerHigh)
    {
        while (pitch > registerHigh)
            pitch -= 12;
        while (pitch < registerLow)
            pitch += 12;
        return juce::jlimit (registerLow, registerHigh, pitch);
    }
}

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

// R&B/Neo-soul: extended chords (maj9/min11/dom9 per rnbExtensionIntervals)
// with nearest-octave voice leading between consecutive chords. The first
// voiced chord seeds the progression at kAnchorRnbSeed (root-anchored,
// register-clamped preserving pitch class); every subsequent voiced chord
// places each extension tone's target pitch class in the octave nearest the
// previous voiced chord's mean pitch (05-RESEARCH.md Pattern 2/Code
// Examples). One sustained voicing per segment (smooth neo-soul pads, no
// re-strike). NoChord segments are skipped WITHOUT updating the running
// previous-mean-pitch state -- voice-leads across the gap from the last
// voiced chord (05-RESEARCH.md Pattern 2's own NoChord clause).
std::vector<NoteEvent> generateRnbNeoSoulRow (const AnalysisResult& result, const GenerationSettings&)
{
    std::vector<NoteEvent> notes;
    uint32_t noteIndex = 0;
    bool hasVoicedFirstChord = false;
    double previousChordMeanPitch = 0.0;

    for (const auto& segment : result.chords)
    {
        // Pitfall 1: NoChord segments emit zero notes; also skip WITHOUT
        // touching previousChordMeanPitch (voice-lead across the gap).
        if (segment.chord.quality == ChordQuality::NoChord)
            continue;

        const auto intervals = rnbExtensionIntervals (segment.chord.quality);
        std::vector<int> pitches;
        pitches.reserve (intervals.size());

        if (! hasVoicedFirstChord)
        {
            const int root = rootMidiNote (segment.chord.pitchClass, kAnchorRnbSeed);
            for (int rawPitch : intervalsToMidiNotes (root, intervals))
                pitches.push_back (clampToRegisterByOctave (rawPitch, kRnbRegisterLow, kRnbRegisterHigh));
        }
        else
        {
            const int previousNote = (int) std::lround (previousChordMeanPitch);
            for (int interval : intervals)
            {
                const int targetClass = ((segment.chord.pitchClass + interval) % 12 + 12) % 12;
                pitches.push_back (nearestOctaveNote (targetClass, previousNote, kRnbRegisterLow, kRnbRegisterHigh));
            }
        }

        const double startBeats = (double) segment.startBeatIndex;
        const double lengthBeats = (double) (segment.endBeatIndex - segment.startBeatIndex);

        for (int pitch : pitches)
        {
            const float velocity = juce::jlimit (0.0f, 1.0f,
                kVelRnb + deterministicJitter (noteIndex++, kSeedRnb, kJitterRnb));
            notes.push_back ({ startBeats, lengthBeats, pitch, velocity });
        }

        if (! pitches.empty())
        {
            double sum = 0.0;
            for (int p : pitches)
                sum += (double) p;
            previousChordMeanPitch = sum / (double) pitches.size();
        }
        hasVoicedFirstChord = true;
    }

    return notes;
}

// body: plan 05-02
std::vector<NoteEvent> generateElectronicHouseRow (const AnalysisResult&, const GenerationSettings&)
{
    return {};
}
