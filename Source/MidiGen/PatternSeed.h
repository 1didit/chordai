#pragma once

// Deterministic base-seed hashing + seed/variation combination (06.1-
// RESEARCH.md Pattern 4). Extends Humanization.h's existing Knuth-
// multiplicative-hash idiom to a new purpose (pattern/variant selection,
// not velocity jitter) -- same proven non-juce::Random approach. NO
// juce::Random/std::mt19937/std::random_device anywhere in this file
// (Pitfall B).

#include "../Analysis/AnalysisResult.h"

#include <JuceHeader.h>

#include <cstdint>

// Hashes only INTEGER-valued, already-stable fields (pitchClass, quality
// cast to int, startBeatIndex, endBeatIndex) -- never bpm/startSeconds/
// endSeconds/confidence (doubles/floats), sidestepping any floating-point
// hash-stability question entirely (GEN-11's harmony-preservation contract
// starts here: the hash that seeds variation selection can never itself
// depend on presentation-only float fields).
inline uint64_t hashChordProgression (const AnalysisResult& result)
{
    uint64_t h = 0x9e3779b97f4a7c15ull; // fixed constant, NOT wall-clock/address-derived
    for (const auto& seg : result.chords)
    {
        uint64_t v = ((uint64_t) seg.chord.pitchClass << 40)
                    ^ ((uint64_t) seg.chord.quality << 32)
                    ^ ((uint64_t) (uint32_t) seg.startBeatIndex << 16)
                    ^ (uint64_t) (uint32_t) seg.endBeatIndex;
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); // boost-style combine
    }
    return h;
}

inline uint64_t computeBaseSeed (const AnalysisResult& result, const juce::String& genreId, int patternIndex)
{
    uint64_t h = hashChordProgression (result);
    h ^= (uint64_t) genreId.hashCode() * 2654435761ull;
    h ^= (uint64_t) patternIndex * 0x100000001b3ull;
    return h;
}

// Combines the base seed with a per-row, click-incremented variation
// counter into the 32-bit seed generatePattern()/deterministicJitter()
// consume.
inline uint32_t combineSeedWithVariation (uint64_t baseSeed, uint32_t variationCounter)
{
    uint64_t h = baseSeed ^ ((uint64_t) variationCounter * 0xff51afd7ed558ccdull);
    h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ull; h ^= h >> 33; // MurmurHash3 finalizer shape
    return (uint32_t) h;
}
