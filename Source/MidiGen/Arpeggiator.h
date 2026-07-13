#pragma once

// Directional tone-sequence stepping (06.1-RESEARCH.md Pattern 3). Header-
// only, pure, no JUCE dependency.

#include <vector>

enum class ArpDirection { Up, Down, UpDown };

// Deterministic directional stepping through an already-computed tone set (chord/scale
// tones from ChordToneMapper) -- never invents pitches. count is the number of emitted
// steps; UpDown ping-pongs without repeating the endpoints (classic arp convention).
inline std::vector<int> arpeggiate (const std::vector<int>& tones, ArpDirection direction, int count)
{
    std::vector<int> result;
    if (tones.empty() || count <= 0)
        return result;

    result.reserve ((size_t) count);

    if (direction == ArpDirection::Up)
    {
        for (int i = 0; i < count; ++i)
            result.push_back (tones[(size_t) i % tones.size()]);
        return result;
    }

    if (direction == ArpDirection::Down)
    {
        const size_t n = tones.size();
        for (int i = 0; i < count; ++i)
            result.push_back (tones[n - 1 - ((size_t) i % n)]);
        return result;
    }

    // UpDown: build the ping-pong cycle {t0..tn-1, tn-2..t1} and index modulo
    // its length. Guard n==1 (cycle is just {t0}) and n==2 (cycle is {t0,t1},
    // there is no interior tone to ping-pong through).
    std::vector<int> cycle = tones;
    if (tones.size() > 2)
        for (size_t i = tones.size() - 1; i-- > 1; )
            cycle.push_back (tones[i]);

    for (int i = 0; i < count; ++i)
        result.push_back (cycle[(size_t) i % cycle.size()]);

    return result;
}
