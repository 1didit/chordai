#include <JuceHeader.h>
#include <catch2/catch_test_macros.hpp>

#include "Source/Audio/AuditionRenderer.h"
#include "Source/MidiGen/MidiSetRow.h"

#include <cmath>
#include <cstring>

namespace
{
    MidiSetRow makeSingleNoteRow (double startBeats, double lengthBeats, int pitch, float velocity)
    {
        MidiSetRow row;
        row.id = "as-is";
        row.notes = { { startBeats, lengthBeats, pitch, velocity } };
        return row;
    }

    // 5 simultaneous notes, full velocity -- worst case for additive-mix
    // clipping (all voices peak at the same instant, at attack).
    MidiSetRow makeDenseChordRow()
    {
        MidiSetRow row;
        row.id = "as-is";
        row.notes = {
            { 0.0, 2.0, 60, 1.0f },
            { 0.0, 2.0, 64, 1.0f },
            { 0.0, 2.0, 67, 1.0f },
            { 0.0, 2.0, 71, 1.0f },
            { 0.0, 2.0, 74, 1.0f },
        };
        return row;
    }
}

TEST_CASE ("AuditionRendererTests.DeterministicByteIdenticalOutput", "[auditionrenderer]")
{
    const auto row = makeDenseChordRow();

    const auto first = AuditionRenderer::render (row, 120.0, 44100.0);
    const auto second = AuditionRenderer::render (row, 120.0, 44100.0);

    REQUIRE (first.getNumSamples() == second.getNumSamples());
    REQUIRE (first.getNumChannels() == second.getNumChannels());

    if (first.getNumSamples() > 0)
    {
        const auto result = std::memcmp (first.getReadPointer (0),
                                          second.getReadPointer (0),
                                          (size_t) first.getNumSamples() * sizeof (float));
        CHECK (result == 0);
    }
}

TEST_CASE ("AuditionRendererTests.SampleCountMatchesFormula", "[auditionrenderer]")
{
    const auto row = makeSingleNoteRow (0.0, 2.0, 60, 0.8f);

    const auto buffer = AuditionRenderer::render (row, 120.0, 44100.0);

    const int expected = (int) std::ceil ((2.0 * 60.0 / 120.0 + AuditionRenderer::kReleaseTailSeconds) * 44100.0);
    CHECK (buffer.getNumSamples() == expected);
    CHECK (buffer.getNumChannels() == 1);
}

TEST_CASE ("AuditionRendererTests.EveryDenseChordSampleIsFinite", "[auditionrenderer]")
{
    const auto row = makeDenseChordRow();
    const auto buffer = AuditionRenderer::render (row, 120.0, 44100.0);

    REQUIRE (buffer.getNumSamples() > 0);

    const auto* data = buffer.getReadPointer (0);
    bool allFinite = true;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        if (! std::isfinite (data[i]))
            allFinite = false;

    CHECK (allFinite);
}

TEST_CASE ("AuditionRendererTests.DenseChordRowDoesNotClip", "[auditionrenderer]")
{
    const auto row = makeDenseChordRow();
    const auto buffer = AuditionRenderer::render (row, 120.0, 44100.0);

    REQUIRE (buffer.getNumSamples() > 0);

    const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
    CHECK (peak <= 1.0f);
    // Deliberate headroom, not luck: normalize-down engages at peak > 0.9.
    CHECK (peak <= 0.95f);
}

TEST_CASE ("AuditionRendererTests.EmptyRowIsValidZeroLengthBuffer", "[auditionrenderer]")
{
    MidiSetRow row;
    row.id = "as-is"; // row.notes left empty

    const auto buffer = AuditionRenderer::render (row, 120.0, 44100.0);

    CHECK (buffer.getNumSamples() == 0);
}

TEST_CASE ("AuditionRendererTests.BpmFallbackMatchesDefaultBpmLength", "[auditionrenderer]")
{
    const auto row = makeSingleNoteRow (0.0, 2.0, 60, 0.8f);

    const auto fallback = AuditionRenderer::render (row, 0.0, 44100.0);
    const auto explicit120 = AuditionRenderer::render (row, 120.0, 44100.0);

    CHECK (fallback.getNumSamples() == explicit120.getNumSamples());
}

TEST_CASE ("AuditionRendererTests.NormalVelocitySingleNoteIsAudible", "[auditionrenderer]")
{
    const auto row = makeSingleNoteRow (0.0, 2.0, 60, 0.8f);
    const auto buffer = AuditionRenderer::render (row, 120.0, 44100.0);

    REQUIRE (buffer.getNumSamples() > 0);

    const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
    CHECK (peak >= 0.05f);
}
