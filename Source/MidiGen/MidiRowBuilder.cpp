#include "MidiRowBuilder.h"

#include "PatternEngine.h"
#include "PatternSeed.h"

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
