#include "MidiRowBuilder.h"

#include "AsIsRowGenerator.h"
#include "BassLineGenerator.h"
#include "PatternEngine.h"
#include "PatternSeed.h"
#include "StyleVoicingGenerators.h"

// The one public MidiGen entry point (GEN-01): fans one AnalysisResult out
// into the 5 fixed rows, in fixed order, in one synchronous call. Pure
// function -- same input always produces byte-identical output (GEN-04
// determinism contract, 05-RESEARCH.md Pitfall 2), proven by
// MidiRowBuilderTests.SameInputProducesByteIdenticalRows.
std::vector<MidiSetRow> generateAllRows (const AnalysisResult& result, const GenerationSettings& settings)
{
    // Bass rhythm: BassRhythm::TrapSustain is the shipped v1 default (Claude's
    // discretion, documented in 05-04-PLAN.md) -- a sparse sustained root is
    // the most universally usable bass for piano-roll drag-out, and matches
    // Tests/SyntheticFixtures.h's existing "bass = root" convention.
    // RnbRootFifth/HouseFourOnFloor remain implemented/tested (05-03) for a
    // future per-row style/rhythm control (05-RESEARCH.md Open Question 1);
    // `settings` is threaded through unused (v1 no-op) so that future control
    // can call this same entry point without a shape change.
    //
    // Transitional mapping (RowStyle -> PatternKind + patternIndex) until
    // 06.1-05 replaces this function with generateGenreRows -- preserves row
    // order/ids/labels/notes exactly (06.1-01-PLAN.md Task 1 step 7).
    return {
        { "as-is",       "Detected",           PatternKind::SustainedChords, 0, generateAsIsRow (result) },
        { "pop-trap",    "Pop / Trap",         PatternKind::RhythmicChords,  1, generatePopTrapRow (result, settings) },
        { "rnb-neosoul", "R&B / Neo-Soul",     PatternKind::StabArp,         2, generateRnbNeoSoulRow (result, settings) },
        { "house",       "Electronic / House", PatternKind::TopLineMotif,    4, generateElectronicHouseRow (result, settings) },
        { "bass",        "Bass",               PatternKind::BassLine,        3, generateBassRow (result, BassRhythm::TrapSustain) },
    };
}

namespace
{
    // Slot slug/label tables, index == (int) PatternKind, per this plan's
    // frozen interfaces block. Kept file-local (only MidiRowBuilder.cpp
    // assembles row identity).
    constexpr const char* kSlotSlugs[5]  = { "sustained", "rhythmic", "stab-arp", "bass", "top-line" };
    constexpr const char* kSlotLabels[5] = { "Sustained Chords", "Rhythmic Chords", "Stab / Arp", "Bass Line", "Top Line" };

    // "<GenreLabel> \xe2\x80\x94 <Slot Label>" -- the em dash MUST go through
    // CharPointer_UTF8 (never a plain ASCII-assuming string literal), per
    // 02-02's non-ASCII-literal rule (MidiSetsPanel.cpp's own precedent).
    juce::String slotLabelFor (const GenreSpec& genre, int patternIndex)
    {
        return genre.label + juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 ")) + kSlotLabels[(size_t) patternIndex];
    }

    // One row assembly path shared by generateGenreRows (variationCounter ==
    // 0 for every slot) and regenerateRow (one slot, caller-supplied
    // counter) -- identity (id/label/kind/patternIndex) and seed derivation
    // are identical either way, only variationCounter differs.
    MidiSetRow assembleGenreRow (const AnalysisResult& result, const GenreSpec& genre, int patternIndex,
                                  uint32_t variationCounter, const GenerationSettings& settings)
    {
        juce::ignoreUnused (settings); // GenerationSettings is still the v1 no-op placeholder (05-04 precedent)

        const auto baseSeed = computeBaseSeed (result, genre.id, patternIndex);
        const auto seed = combineSeedWithVariation (baseSeed, variationCounter);

        MidiSetRow row;
        row.id = genre.id + "-" + kSlotSlugs[(size_t) patternIndex];
        row.label = slotLabelFor (genre, patternIndex);
        row.kind = (PatternKind) patternIndex;
        row.patternIndex = patternIndex;
        row.notes = generatePattern (result, genre.patterns[(size_t) patternIndex], seed);
        return row;
    }
}

std::vector<MidiSetRow> generateGenreRows (const AnalysisResult& result, const GenreSpec& genre, const GenerationSettings& settings)
{
    std::vector<MidiSetRow> rows;
    rows.reserve (5);
    for (int i = 0; i < 5; ++i)
        rows.push_back (assembleGenreRow (result, genre, i, 0, settings));
    return rows;
}

MidiSetRow regenerateRow (const AnalysisResult& result, const GenreSpec& genre, int patternIndex, uint32_t variationCounter, const GenerationSettings& settings)
{
    jassert (patternIndex >= 0 && patternIndex < 5); // caller (PluginProcessor) already guards; this stays safe standalone
    const int clamped = juce::jlimit (0, 4, patternIndex);
    return assembleGenreRow (result, genre, clamped, variationCounter, settings);
}
