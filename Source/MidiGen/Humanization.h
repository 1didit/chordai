#pragma once

// Header-only: deterministicJitter() velocity variation + per-style seed
// constants. Pure hash function -- NO juce::Random anywhere under
// Source/MidiGen/, no mutable state (GEN-04's regeneration-determinism
// requirement, see 05-RESEARCH.md Pattern 5 / Pitfall 2).
//
// Bodies implemented in plan 05-01 Task 2.

#include <cstdint>

// implemented in plan 05-01 Task 2
