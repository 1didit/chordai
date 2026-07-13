#include "PatternEngine.h"

#include "Arpeggiator.h"
#include "BassLineGenerator.h"
#include "ChordToneMapper.h"
#include "GrooveEngine.h"
#include "Humanization.h"
#include "VoiceLeadingEngine.h"

#include <JuceHeader.h>

#include <cmath>

// Generic data-driven pattern engine (06.1-RESEARCH.md Pattern 3): the ONE
// per-segment loop that replaces Phase 5's four per-style generator
// functions. Every genre-specific behaviour lives in the PatternArchetype
// DATA (GenreRegistry.h/06.1-04), not in per-genre code here.
//
// Harmony preservation BY CONSTRUCTION (Pitfall A): the seed is used ONLY to
// select among pre-authored rhythm/octave/ornament variants -- it is never
// used to index result.chords, and it never reads/perturbs
// segment.chord.pitchClass/quality. toneSetIntervals() is called with the
// SAME segment.chord.quality every time regardless of seed.

namespace
{
    // Register clamp that preserves pitch class by octave-shifting, instead
    // of a truncating juce::jlimit -- same idiom as the retired
    // StyleVoicingGenerators.cpp's clampToRegisterByOctave (05-RESEARCH.md
    // Pattern 2). Any register band of >= 12 semitones always has a
    // representative of every pitch class, so this never needs to collapse
    // to the boundary and lose the chord tone's identity.
    int clampToRegisterByOctave (int pitch, int registerLow, int registerHigh)
    {
        while (pitch > registerHigh)
            pitch -= 12;
        while (pitch < registerLow)
            pitch += 12;
        return juce::jlimit (registerLow, registerHigh, pitch);
    }

    // Deterministic per-onset "should this onset get an ornament" roll.
    // Range is [0, 0.9999] (denominator 10000, NOT 9999) so that
    // ornamentProbability == 1.0f guarantees every onset qualifies and
    // ornamentProbability == 0.0f guarantees none do -- no floating-point
    // boundary flakiness at either extreme (Pitfall A-adjacent: still a pure
    // function of (seed, segmentIndex, onsetIndex), never of harmony).
    float ornamentRoll01 (uint32_t seed, int segmentIndex, int onsetIndex)
    {
        uint32_t h = seed;
        h ^= (uint32_t) segmentIndex * 2654435761u;
        h ^= (uint32_t) onsetIndex * 0x85ebca6bu;
        h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
        return (float) (h % 10000u) / 10000.0f;
    }

    // Deterministically picks ONE diatonic scale degree (0-11 pitch class)
    // for a TopLineMotif ornament -- the only place a non-chord-tone can
    // ever be emitted, and even then constrained to the detected key's
    // diatonic scale (GEN-11's "still key-diatonic" contract).
    int diatonicOrnamentPitchClass (uint32_t seed, int segmentIndex, int onsetIndex, bool isMajor, int tonicPitchClass)
    {
        const auto scale = diatonicScaleIntervals (isMajor);

        uint32_t h = seed;
        h ^= (uint32_t) segmentIndex * 0x9e3779b1u;
        h ^= (uint32_t) onsetIndex * 0x2545f491u;
        h ^= h >> 15; h *= 0x27d4eb2fu; h ^= h >> 15;

        const size_t idx = (size_t) h % scale.size();
        return ((tonicPitchClass + scale[idx]) % 12 + 12) % 12;
    }
}

std::vector<NoteEvent> generatePattern (const AnalysisResult& result, const PatternArchetype& archetype, uint32_t seed)
{
    std::vector<NoteEvent> notes;

    // Two independent sub-seeds derived from the one uint32_t (documented
    // per 06.1-03-PLAN.md's interfaces block):
    //   seedRhythm -- selects rhythmPool/bassRhythmPool variant (raw seed).
    //   seedOctave -- selects octaveOffsetPool entry (Knuth-multiplicative
    //                 mix so it doesn't just track seedRhythm in lockstep).
    // Per-onset ornament rolls use their own hash (ornamentRoll01 above),
    // seeded by (seed, segmentIndex, onsetIndex) -- never by chord content.
    const uint32_t seedRhythm = seed;
    const uint32_t seedOctave = seed * 0x9e3779b9u ^ 0x85ebca6bu;

    // BassLine slot with a configured bass rhythm pool delegates entirely to
    // the dedicated bass generator (Phase 5, already proven deterministic
    // and NoChord-safe), then reshapes octave + velocity per the archetype.
    if (archetype.kind == PatternKind::BassLine && ! archetype.bassRhythmPool.empty())
    {
        const auto bassRhythm = archetype.bassRhythmPool[seedRhythm % archetype.bassRhythmPool.size()];
        auto bassNotes = generateBassRow (result, bassRhythm);

        const int octaveOffset = archetype.octaveOffsetPool.empty() ? 0
            : archetype.octaveOffsetPool[seedOctave % archetype.octaveOffsetPool.size()];

        uint32_t noteIndex = 0;
        for (auto& n : bassNotes)
        {
            n.pitch = juce::jlimit (0, 127, n.pitch + octaveOffset);
            n.velocity = juce::jlimit (0.05f, 1.0f,
                archetype.baseVelocity + deterministicJitter (noteIndex++, seed, archetype.jitterRange));
        }

        return bassNotes;
    }

    uint32_t runningNoteIndex = 0;
    bool hasVoicedPrevious = false;
    int previousVoicedPitch = archetype.registerAnchor;

    for (size_t segIdx = 0; segIdx < result.chords.size(); ++segIdx)
    {
        const auto& segment = result.chords[segIdx];

        // Behavior 1 / Pitfall A: NoChord segments emit zero notes, every
        // slot. toneSetIntervals() also returns {} for NoChord (belt and
        // braces -- both guards are independently correct).
        if (segment.chord.quality == ChordQuality::NoChord)
            continue;

        const auto toneSet = toneSetIntervals (archetype.toneSet, segment.chord.quality, archetype.dropRoot);
        if (toneSet.empty())
            continue;

        const int octaveOffset = archetype.octaveOffsetPool.empty() ? 0
            : archetype.octaveOffsetPool[seedOctave % archetype.octaveOffsetPool.size()];
        const int root = rootMidiNote (segment.chord.pitchClass, archetype.registerAnchor) + octaveOffset;

        // Register: place each chord tone, optionally voice-led against the
        // previous voiced segment (Phase 5 R&B mechanism -- VoiceLeadingEngine's
        // nearestOctaveNote against the running mean pitch), always clamped
        // into [registerLow, registerHigh] pitch-class-preservingly.
        std::vector<int> pitches;
        pitches.reserve (toneSet.size());

        if (archetype.useVoiceLeading && hasVoicedPrevious)
        {
            for (int interval : toneSet)
            {
                const int targetClass = ((segment.chord.pitchClass + interval) % 12 + 12) % 12;
                pitches.push_back (nearestOctaveNote (targetClass, previousVoicedPitch, archetype.registerLow, archetype.registerHigh));
            }
        }
        else
        {
            for (int interval : toneSet)
                pitches.push_back (clampToRegisterByOctave (root + interval, archetype.registerLow, archetype.registerHigh));
        }

        if (! pitches.empty())
        {
            double sum = 0.0;
            for (int p : pitches)
                sum += (double) p;
            previousVoicedPitch = (int) std::lround (sum / (double) pitches.size());
            hasVoicedPrevious = true;
        }

        // Rhythm: seed-selected variant, tiled across the segment.
        const double segStartBeats = (double) segment.startBeatIndex;
        const double segLengthBeats = (double) (segment.endBeatIndex - segment.startBeatIndex);

        if (archetype.rhythmPool.empty())
            continue;

        const auto& variant = archetype.rhythmPool[seedRhythm % archetype.rhythmPool.size()];
        const auto onsets = tileOnsets (variant.onsetsBeats, variant.spanBeats, segLengthBeats);

        // StabArp: one tone per onset, stepping Up through the chord tones
        // (Arpeggiator.h) -- only meaningful when there's more than one
        // tone to step through.
        std::vector<int> arpSequence;
        if (archetype.kind == PatternKind::StabArp && pitches.size() > 1 && ! onsets.empty())
            arpSequence = arpeggiate (pitches, ArpDirection::Up, (int) onsets.size());

        for (size_t onsetIdx = 0; onsetIdx < onsets.size(); ++onsetIdx)
        {
            const double onsetBeat = onsets[onsetIdx];

            // Note length: noteLengthRatio * gap to the next onset, or to
            // the segment end for the last onset in this segment.
            const double gap = (onsetIdx + 1 < onsets.size())
                ? onsets[onsetIdx + 1] - onsetBeat
                : segLengthBeats - onsetBeat;
            const double noteLength = juce::jmax (0.0, gap * variant.noteLengthRatio);

            const float accent = archetype.accentPattern.empty() ? 1.0f
                : archetype.accentPattern[onsetIdx % archetype.accentPattern.size()];

            const bool isArpOnset = archetype.kind == PatternKind::StabArp && pitches.size() > 1;
            const std::vector<int> onsetPitches = isArpOnset
                ? std::vector<int> { arpSequence[onsetIdx] }
                : pitches;

            for (int pitch : onsetPitches)
            {
                const float velocity = juce::jlimit (0.05f, 1.0f,
                    archetype.baseVelocity * accent + deterministicJitter (runningNoteIndex++, seed, archetype.jitterRange));
                notes.push_back ({ segStartBeats + onsetBeat, noteLength, pitch, velocity });
            }

            // TopLineMotif ONLY: per-onset hash-thresholded diatonic passing
            // tone -- the ONLY non-chord-tone path, still key-diatonic.
            if (archetype.kind == PatternKind::TopLineMotif && archetype.ornamentProbability > 0.0f)
            {
                const float roll = ornamentRoll01 (seed, (int) segIdx, (int) onsetIdx);
                if (roll < archetype.ornamentProbability)
                {
                    const int ornamentClass = diatonicOrnamentPitchClass (seed, (int) segIdx, (int) onsetIdx,
                                                                            result.key.isMajor, result.key.tonicPitchClass);
                    const int anchorPitch = pitches.empty() ? root : pitches.front();
                    const int ornamentPitch = nearestOctaveNote (ornamentClass, anchorPitch, archetype.registerLow, archetype.registerHigh);

                    const float ornamentVelocity = juce::jlimit (0.05f, 1.0f,
                        archetype.baseVelocity * accent + deterministicJitter (runningNoteIndex++, seed, archetype.jitterRange));
                    notes.push_back ({ segStartBeats + onsetBeat, noteLength, ornamentPitch, ornamentVelocity });
                }
            }
        }
    }

    return notes;
}
