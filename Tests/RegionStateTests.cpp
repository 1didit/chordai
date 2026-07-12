#include <JuceHeader.h>
#include <catch2/catch_test_macros.hpp>

#include "Source/Import/RegionState.h"

TEST_CASE ("RegionStateTests.WriteReadRoundtrip", "[regionstate]")
{
    juce::ValueTree state ("PARAMETERS");
    juce::File file ("/tmp/x.wav");

    RegionState::write (state, file, { 1.5, 42.0 });

    CHECK (RegionState::readSourceFile (state) == file);

    auto region = RegionState::readRegion (state);
    CHECK (region.getStart() == 1.5);
    CHECK (region.getEnd() == 42.0);
}

TEST_CASE ("RegionStateTests.XmlSurvival", "[regionstate]")
{
    juce::ValueTree state ("PARAMETERS");
    juce::File file ("/tmp/x.wav");

    RegionState::write (state, file, { 1.5, 42.0 });

    auto xml = state.createXml();
    REQUIRE (xml != nullptr);

    auto roundTripped = juce::ValueTree::fromXml (*xml);
    REQUIRE (roundTripped.isValid());

    CHECK (RegionState::readSourceFile (roundTripped) == file);

    auto region = RegionState::readRegion (roundTripped);
    CHECK (region.getStart() == 1.5);
    CHECK (region.getEnd() == 42.0);
}

TEST_CASE ("RegionStateTests.ClampRegion", "[regionstate]")
{
    // Out-of-range clamps to [0, totalLength].
    auto clamped = RegionState::clampRegion ({ -5.0, 999.0 }, 10.0);
    CHECK (clamped.getStart() == 0.0);
    CHECK (clamped.getEnd() == 10.0);

    // Inverted input gets normalized.
    auto normalized = RegionState::clampRegion ({ 8.0, 2.0 }, 10.0);
    CHECK (normalized.getStart() == 2.0);
    CHECK (normalized.getEnd() == 8.0);

    // Empty range means "whole file".
    auto whole = RegionState::clampRegion ({ 0.0, 0.0 }, 10.0);
    CHECK (whole.getStart() == 0.0);
    CHECK (whole.getEnd() == 10.0);
}

TEST_CASE ("RegionStateTests.ReadDefaults", "[regionstate]")
{
    juce::ValueTree state ("PARAMETERS");

    CHECK (RegionState::readSourceFile (state) == juce::File());

    auto region = RegionState::readRegion (state);
    CHECK (region.isEmpty());
}
