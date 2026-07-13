// Genre library data + shape tests (06.1-04). Task 1 covers the 5 main
// genres (trap, uk-drill, boom-bap, rnb-neosoul, house); Task 2 appends the
// remaining 5 and the phase-critical tick-exactness sweep.

#include "../Source/MidiGen/GenreRegistry.h"
#include "../Source/MidiGen/GrooveEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

TEST_CASE ("GenreRegistryTests.RegistryShapeIsWellFormed", "[genreregistry]")
{
    const auto& genres = allGenres();
    REQUIRE (genres.size() == 10);

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

// Phase-critical sweep (Pitfall C): every onset/span in every RhythmVariant
// of every genre must land on an exact integer tick against TPQN 960 -- no
// decimal swing anywhere in the data. Written FIRST as the guardrail while
// authoring the remaining 5 genres; any authored constant that fails this
// gets the CONSTANT fixed, never the test loosened.
TEST_CASE ("GenreRegistryTests.AllRhythmPoolOnsetsDivide960Exactly", "[genreregistry]")
{
    constexpr int kTpqn = 960;

    for (const auto& genre : allGenres())
    {
        for (const auto& p : genre.patterns)
        {
            for (const auto& rv : p.rhythmPool)
            {
                INFO ("genre: " << genre.id << " kind: " << (int) p.kind);
                CHECK (rv.spanBeats > 0.0);
                CHECK (isTickExact (rv.spanBeats, kTpqn));

                double previousOnset = -1.0;
                for (double onset : rv.onsetsBeats)
                {
                    CHECK (isTickExact (onset, kTpqn));
                    CHECK (onset >= 0.0);
                    CHECK (onset < rv.spanBeats);
                    CHECK (onset > previousOnset);
                    previousOnset = onset;
                }
            }
        }
    }
}

TEST_CASE ("GenreRegistryTests.UniqueIdsAndUniqueShortLabels", "[genreregistry]")
{
    std::set<std::string> ids, shortLabels;

    for (const auto& genre : allGenres())
    {
        INFO ("genre id: " << genre.id);
        CHECK (ids.insert (genre.id.toStdString()).second);
        CHECK (shortLabels.insert (genre.shortLabel.toStdString()).second);
        CHECK (genre.shortLabel.length() <= 9);
    }
}

TEST_CASE ("GenreRegistryTests.RegeneratePoolsOfferVariety", "[genreregistry]")
{
    for (const auto& id : kDefaultMainGenreIds)
    {
        const auto* g = findGenre (id);
        REQUIRE (g != nullptr);

        int variedSlots = 0;
        for (const auto& p : g->patterns)
            if (p.rhythmPool.size() >= 2 || p.octaveOffsetPool.size() >= 2)
                ++variedSlots;

        INFO ("genre: " << id);
        CHECK (variedSlots >= 3);
    }
}
