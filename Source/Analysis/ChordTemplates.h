#pragma once

// Owned by plan 03-05. Header-only.
//
// 36-class binary chord template set: 12 major + 12 minor + 12 dominant-7th,
// per 03-RESEARCH.md "Chord Recognition" (Sheh & Ellis 2003 / Bello & Pickens
// 2005 baseline). Root pitch class r follows AnalysisResult.h's convention
// (0=C, 1=C#, ... 11=B).
//
// State-index convention (used by ChordDecoder's Viterbi state space):
//   index = qualityBlock * 12 + root
//   qualityBlock: 0 = Major, 1 = Minor, 2 = Dominant7
// NoChord is NOT part of the 36-state space -- it is a deterministic
// post-Viterbi override (see ChordDecoder.cpp), so indexForSymbol/symbolForIndex
// only ever operate on Major/Minor/Dominant7 symbols.

#include "AnalysisResult.h"

#include <array>
#include <cassert>
#include <cmath>

// Builds a binary, L2-normalized 12-bin chord template for root pitch class
// `root` (0..11) and `quality` (Major/Minor/Dominant7 only -- NoChord asserts).
// Active pitch classes: root always; third (major=+4, minor=+3); fifth (+7);
// seventh (+10) added only for Dominant7.
inline std::array<float, 12> buildTemplate (int root, ChordQuality quality)
{
    assert (quality != ChordQuality::NoChord && "buildTemplate: NoChord is not a template-bearing quality");

    std::array<float, 12> t {};
    auto set = [&] (int semitoneOffset) { t[(size_t) ((root + semitoneOffset) % 12)] = 1.0f; };

    set (0);                                        // root, all qualities
    set (quality == ChordQuality::Minor ? 3 : 4);    // minor third vs major third
    set (7);                                         // perfect fifth
    if (quality == ChordQuality::Dominant7)
        set (10);                                    // minor seventh

    float norm = 0.0f;
    for (float v : t)
        norm += v * v;
    norm = std::sqrt (norm);
    for (float& v : t)
        v /= norm;

    return t;
}

// Builds all 36 templates in state-index order (see convention above):
// indices 0-11 = Major (root 0..11), 12-23 = Minor, 24-35 = Dominant7.
inline std::array<std::array<float, 12>, 36> buildAllTemplates()
{
    std::array<std::array<float, 12>, 36> templates {};
    constexpr ChordQuality qualities[3] = { ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7 };

    for (int block = 0; block < 3; ++block)
        for (int root = 0; root < 12; ++root)
            templates[(size_t) (block * 12 + root)] = buildTemplate (root, qualities[(size_t) block]);

    return templates;
}

// Maps a 36-state Viterbi index back to its ChordSymbol (see convention above).
// index must be in [0, 35]; asserts otherwise.
inline ChordSymbol symbolForIndex (int index)
{
    assert (index >= 0 && index < 36 && "symbolForIndex: index out of [0,35] range");

    const int block = index / 12;
    const int root = index % 12;
    constexpr ChordQuality qualities[3] = { ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7 };

    ChordSymbol symbol;
    symbol.pitchClass = root;
    symbol.quality = qualities[(size_t) block];
    return symbol;
}

// Maps a ChordSymbol (Major/Minor/Dominant7 only -- NoChord asserts) to its
// 36-state Viterbi index (see convention above).
inline int indexForSymbol (const ChordSymbol& symbol)
{
    assert (symbol.quality != ChordQuality::NoChord && "indexForSymbol: NoChord has no state index");

    int block = 0;
    if (symbol.quality == ChordQuality::Minor)
        block = 1;
    else if (symbol.quality == ChordQuality::Dominant7)
        block = 2;

    return block * 12 + symbol.pitchClass;
}
