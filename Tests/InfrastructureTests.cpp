#include <JuceHeader.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Infrastructure.CatchAndJuceLink", "[infrastructure]")
{
    auto version = juce::SystemStats::getJUCEVersion();
    REQUIRE (version.isNotEmpty());
    REQUIRE (version.startsWith ("JUCE v8"));
}
