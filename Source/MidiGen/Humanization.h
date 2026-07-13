#pragma once

// Header-only: deterministicJitter() velocity variation + per-style seed
// constants. Pure hash function -- NO juce::Random anywhere under
// Source/MidiGen/, no mutable state (GEN-04's regeneration-determinism
// requirement, see 05-RESEARCH.md Pattern 5 / Pitfall 2).

#include <cstdint>

// Deterministic per-note velocity jitter. Pure function of noteIndex (a
// stable, caller-assigned running count within one row-generation call --
// generateGenreRows()/regenerateRow() as of 06.1-05) and a per-style seed
// constant -- NOT juce::Random with a time-based seed.
// GEN-04 requires that regenerating the SAME AnalysisResult always produces
// the SAME rows (the user must be able to trust that toggling the region
// selector back to an identical range doesn't change how a row sounds) --
// any run-to-run randomness here would silently break that guarantee and
// would also make exact-expected-NoteEvent tests impossible to write.
inline float deterministicJitter (uint32_t noteIndex, uint32_t styleSeed, float range)
{
    uint32_t h = noteIndex * 2654435761u + styleSeed; // Knuth multiplicative hash
    h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16;
    float unit = (float) (h % 10000u) / 9999.0f;      // [0, 1]
    return (unit - 0.5f) * range;                      // [-range/2, range/2]
}

// Per-style seed constants, used by Wave 2/3 generators.
inline constexpr uint32_t kSeedPopTrap = 0x50505050u, kSeedRnb = 0x52424242u,
                          kSeedHouse = 0x48534531u, kSeedBass = 0x42415353u;

// Base velocity / jitter range per row (product-design defaults, MEDIUM
// confidence -- confirm by ear once Phase 6 audition exists; range param is
// full width, jitter returns [-range/2, +range/2]).
inline constexpr float kVelAsIs = 0.75f,               // no jitter
                       kVelPopTrap = 0.78f, kJitterPopTrap = 0.12f,   // +/-0.06
                       kVelRnb = 0.62f,     kJitterRnb = 0.08f,       // +/-0.04
                       kVelHouse = 0.88f,   kJitterHouse = 0.10f,     // +/-0.05
                       kVelBass = 0.85f,    kJitterBass = 0.06f;      // +/-0.03
