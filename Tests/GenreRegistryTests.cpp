// Genre library data + shape tests (06.1-04). Task 1 covers the 5 main
// genres (trap, uk-drill, boom-bap, rnb-neosoul, house); Task 2 appends the
// remaining 5 and the phase-critical tick-exactness sweep.

#include "../Source/MidiGen/GenreRegistry.h"
#include "../Source/MidiGen/GrooveEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

namespace
{
    // Task 1: only the 5 main genres exist yet -- bumped to == 10 in Task 2.
    constexpr size_t kExpectedGenreCountSoFar = 5; // TODO(Task 2): becomes 10
}

TEST_CASE ("GenreRegistryTests.RegistryShapeIsWellFormed", "[genreregistry]")
{
    const auto& genres = allGenres();
    REQUIRE (genres.size() >= kExpectedGenreCountSoFar);

    for (const auto& genre : genres)
    {
        INFO ("genre id: " << genre.id);
        CHECK (genre.id.isNotEmpty());
        CHECK (genre.label.isNotEmpty());
        CHECK (genre.shortLabel.isNotEmpty());

        for (size_t i = 0; i < genre.patterns.size(); ++i)
        {
            const auto& p = genre.patterns[i];
            INFO ("slot index: " << (int) i);
            CHECK (p.kind == (PatternKind) i);
            CHECK (p.registerLow < p.registerHigh);
            CHECK (p.baseVelocity > 0.0f);
            CHECK (p.baseVelocity <= 1.0f);
            CHECK_FALSE (p.accentPattern.empty());

            // BassLine slot may delegate to generateBassRow (bassRhythmPool
            // non-empty) OR use the generic toneSet+rhythmPool path
            // (bassRhythmPool empty) -- at least one must carry material.
            if (p.kind == PatternKind::BassLine)
                CHECK_FALSE ((p.rhythmPool.empty() && p.bassRhythmPool.empty()));
            else
                CHECK_FALSE (p.rhythmPool.empty());
        }
    }
}

TEST_CASE ("GenreRegistryTests.DefaultFiveAreRegistered", "[genreregistry]")
{
    CHECK (kDefaultMainGenreIds.size() == 5);
    for (const auto& id : kDefaultMainGenreIds)
        CHECK (findGenre (id) != nullptr);
}

TEST_CASE ("GenreRegistryTests.FindGenreUnknownReturnsNullptr", "[genreregistry]")
{
    CHECK (findGenre ("no-such-genre") == nullptr);
}

TEST_CASE ("GenreRegistryTests.MainGenresAreDataDistinct", "[genreregistry]")
{
    // Simple fingerprint: concatenate per-slot toneSet/registerAnchor/dropRoot
    // and rhythmPool shape (variant count + first onset + span) -- enough to
    // catch "genre B is a copy-paste of genre A" without full deep equality.
    auto fingerprint = [] (const GenreSpec& g)
    {
        juce::String fp;
        for (const auto& p : g.patterns)
        {
            fp << (int) p.toneSet << "," << p.registerAnchor << "," << (p.dropRoot ? 1 : 0)
               << "," << (int) p.rhythmPool.size();
            for (const auto& rv : p.rhythmPool)
                fp << "|" << (rv.onsetsBeats.empty() ? -1.0 : rv.onsetsBeats.front()) << ":" << rv.spanBeats;
            fp << ";";
        }
        return fp;
    };

    const std::vector<juce::String> ids { "trap", "uk-drill", "boom-bap", "rnb-neosoul", "house" };
    std::vector<juce::String> prints;
    for (const auto& id : ids)
    {
        const auto* g = findGenre (id);
        REQUIRE (g != nullptr);
        prints.push_back (fingerprint (*g));
    }

    for (size_t i = 0; i < prints.size(); ++i)
        for (size_t j = i + 1; j < prints.size(); ++j)
        {
            INFO (ids[i] << " vs " << ids[j]);
            CHECK (prints[i] != prints[j]);
        }
}
