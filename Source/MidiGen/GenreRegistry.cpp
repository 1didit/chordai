#include "GenreRegistry.h"

// Genre library data lands in plan 06.1-04. Empty is safe: nothing calls
// these until Wave 3 (GenreRegistryTests / PatternEngine wiring).

const std::vector<GenreSpec>& allGenres()
{
    static const std::vector<GenreSpec> genres;
    return genres;
}

const GenreSpec* findGenre (const juce::String& id)
{
    for (const auto& genre : allGenres())
        if (genre.id == id)
            return &genre;

    return nullptr;
}
