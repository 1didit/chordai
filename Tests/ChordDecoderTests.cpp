#include <catch2/catch_test_macros.hpp>

#include "Source/Analysis/AudioPreprocessing.h"
#include "Source/Analysis/ChordDecoder.h"
#include "Source/Analysis/ChordTemplates.h"
#include "Source/Analysis/ConstantQAnalysis.h"
#include "Source/Analysis/ChromaExtractor.h"
#include "Source/Analysis/HarmonicPercussiveFilter.h"
#include "Tests/SyntheticFixtures.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
    constexpr float kEpsilon = 1e-4f;
    constexpr double kSampleRate = 44100.0;

    float l2Norm (const std::array<float, 12>& v)
    {
        float sum = 0.0f;
        for (float x : v)
            sum += x * x;
        return std::sqrt (sum);
    }

    int argmax36 (const std::array<float, 36>& scores)
    {
        return (int) std::distance (scores.begin(), std::max_element (scores.begin(), scores.end()));
    }

    bool sameChord (const ChordSymbol& a, const ChordSymbol& b)
    {
        return a.pitchClass == b.pitchClass && a.quality == b.quality;
    }

    // computeCqt -> suppressPercussion -> extractChroma, per ChromaExtractor.h's
    // documented call order (03-02). tuningCents=0.0 -- these fixtures are
    // never detuned, matching ChromaExtractorTests.cpp's own precedent.
    ChromaSequence extractChromaFromBuffer (const juce::AudioBuffer<float>& buffer)
    {
        auto preprocessed = preprocessForAnalysis (buffer, kSampleRate, {});
        auto cqt = computeCqt (preprocessed.chromaSamples, preprocessed.chromaRate);
        suppressPercussion (cqt, kHpssKernelSeconds);
        return extractChroma (cqt, 0.0);
    }

    // Hand-built exact BeatGrid at `bpm`, covering [0, totalSeconds) -- this is
    // a decoder test, not a tracker test (03-05 PLAN.md), so beats are placed
    // at exact multiples of the beat period rather than run through
    // TempoBeatTracker (full-chain integration is 03-06's job).
    BeatGrid buildBeatGrid (double bpm, double totalSeconds)
    {
        BeatGrid grid;
        grid.bpm = bpm;
        const double beatSeconds = 60.0 / bpm;
        const int numBeats = (int) std::llround (totalSeconds / beatSeconds);
        grid.beatTimesSeconds.reserve ((size_t) numBeats);
        for (int i = 0; i < numBeats; ++i)
            grid.beatTimesSeconds.push_back ((double) i * beatSeconds);
        for (int i = 0; i < numBeats; i += 4)
            grid.barStartBeatIndices.push_back (i);
        return grid;
    }

    // Renders `chord`'s tones (root/third/fifth/[seventh], per
    // fixtures::detail::activePitchClasses) plus a single bass note at
    // `bassPitchClass`, additively into `buffer` starting at `startSample` for
    // `lengthSamples`, with a short fade in/out. Mirrors
    // fixtures::renderChordProgression's own per-chord rendering so multiple
    // calls can be spliced/overlaid into one buffer (e.g. chord ... silence
    // ... chord, or two overlapping tone sets for an ambiguous chroma).
    void renderStaticChordInto (juce::AudioBuffer<float>& buffer, int startSample, int lengthSamples,
                                 double sampleRate, const ChordSymbol& chord, int bassPitchClass)
    {
        auto pitchClasses = fixtures::detail::activePitchClasses (chord);
        std::vector<double> toneFrequencies;
        for (int pc : pitchClasses)
            toneFrequencies.push_back (fixtures::detail::midiToFrequency (60 + pc));
        const double bassFrequency = fixtures::detail::midiToFrequency (36 + bassPitchClass);

        constexpr double fadeSeconds = 0.01;
        const double totalSeconds = (double) lengthSamples / sampleRate;
        auto* data = buffer.getWritePointer (0);

        for (int i = 0; i < lengthSamples; ++i)
        {
            const int idx = startSample + i;
            if (idx < 0 || idx >= buffer.getNumSamples())
                continue;

            const double t = (double) i / sampleRate;
            float envelope = 1.0f;
            if (t < fadeSeconds)
                envelope = (float) (t / fadeSeconds);
            else if (t > totalSeconds - fadeSeconds)
                envelope = (float) juce::jmax (0.0, (totalSeconds - t) / fadeSeconds);

            float sample = 0.0f;
            for (double freq : toneFrequencies)
                sample += 0.15f * (float) std::sin (2.0 * std::numbers::pi * freq * t);
            sample += 0.2f * (float) std::sin (2.0 * std::numbers::pi * bassFrequency * t);

            data[idx] += sample * envelope;
        }
    }

    // A static (non-progressing), harmonically ambiguous chord: equal energy
    // at pitch classes {9,0,4,7} (A,C,E,G) -- same set as
    // ChordDecoderTests.BassBiasBreaksTie's hand-built unit test, now at real
    // audio/CQT level -- plus a single controlling bass note at
    // `bassPitchClass`, held for the whole buffer duration.
    juce::AudioBuffer<float> renderAmbiguousChordWithBass (int bassPitchClass, double sampleRate, double durationSeconds)
    {
        const int numSamples = (int) std::llround (durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer (1, numSamples);
        buffer.clear();

        constexpr int harmonicPitchClasses[4] = { 9, 0, 4, 7 }; // A, C, E, G
        std::vector<double> toneFrequencies;
        for (int pc : harmonicPitchClasses)
            toneFrequencies.push_back (fixtures::detail::midiToFrequency (60 + pc));
        const double bassFrequency = fixtures::detail::midiToFrequency (36 + bassPitchClass);

        constexpr double fadeSeconds = 0.01;
        auto* data = buffer.getWritePointer (0);

        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            float envelope = 1.0f;
            if (t < fadeSeconds)
                envelope = (float) (t / fadeSeconds);
            else if (t > durationSeconds - fadeSeconds)
                envelope = (float) juce::jmax (0.0, (durationSeconds - t) / fadeSeconds);

            float sample = 0.0f;
            for (double freq : toneFrequencies)
                sample += 0.15f * (float) std::sin (2.0 * std::numbers::pi * freq * t);
            sample += 0.2f * (float) std::sin (2.0 * std::numbers::pi * bassFrequency * t);

            data[i] += sample * envelope;
        }

        return buffer;
    }
}

TEST_CASE ("ChordDecoderTests.TemplateShapes", "[chordanalysis]")
{
    auto cMaj = buildTemplate (0, ChordQuality::Major);
    for (int pc : { 0, 4, 7 })
        REQUIRE (cMaj[(size_t) pc] > 0.0f);
    for (int pc = 0; pc < 12; ++pc)
        if (pc != 0 && pc != 4 && pc != 7)
            REQUIRE (cMaj[(size_t) pc] == 0.0f);
    REQUIRE (cMaj[0] == cMaj[4]);
    REQUIRE (cMaj[4] == cMaj[7]);
    REQUIRE (std::abs (l2Norm (cMaj) - 1.0f) < kEpsilon);

    auto aMin = buildTemplate (9, ChordQuality::Minor);
    for (int pc : { 9, 0, 4 })
        REQUIRE (aMin[(size_t) pc] > 0.0f);
    for (int pc = 0; pc < 12; ++pc)
        if (pc != 9 && pc != 0 && pc != 4)
            REQUIRE (aMin[(size_t) pc] == 0.0f);
    REQUIRE (std::abs (l2Norm (aMin) - 1.0f) < kEpsilon);

    auto g7 = buildTemplate (7, ChordQuality::Dominant7);
    for (int pc : { 7, 11, 2, 5 })
        REQUIRE (g7[(size_t) pc] > 0.0f);
    for (int pc = 0; pc < 12; ++pc)
        if (pc != 7 && pc != 11 && pc != 2 && pc != 5)
            REQUIRE (g7[(size_t) pc] == 0.0f);
    REQUIRE (std::abs (l2Norm (g7) - 1.0f) < kEpsilon);
}

TEST_CASE ("ChordDecoderTests.IndexSymbolRoundTrip", "[chordanalysis]")
{
    for (int index = 0; index < 36; ++index)
    {
        ChordSymbol s = symbolForIndex (index);
        REQUIRE (s.quality != ChordQuality::NoChord);
        int roundTripIndex = indexForSymbol (s);
        REQUIRE (roundTripIndex == index);

        ChordSymbol s2 = symbolForIndex (roundTripIndex);
        REQUIRE (s2.pitchClass == s.pitchClass);
        REQUIRE (s2.quality == s.quality);
    }
}

TEST_CASE ("ChordDecoderTests.ScoringPrefersCorrectChord", "[chordanalysis]")
{
    auto templates = buildAllTemplates();

    BeatChroma beat;
    beat.harmonicAvg[0] = 1.0f;
    beat.harmonicAvg[4] = 1.0f;
    beat.harmonicAvg[7] = 1.0f;
    // bassAvg left all-zero -- isolate cosine-only preference for this test.

    auto scores = scoreBeatObservations (beat, templates);
    int best = argmax36 (scores);
    ChordSymbol bestSymbol = symbolForIndex (best);

    REQUIRE (bestSymbol.pitchClass == 0);
    REQUIRE (bestSymbol.quality == ChordQuality::Major);
}

TEST_CASE ("ChordDecoderTests.BassBiasBreaksTie", "[chordanalysis]")
{
    auto templates = buildAllTemplates();

    BeatChroma ambiguous;
    for (int pc : { 9, 0, 4, 7 })
        ambiguous.harmonicAvg[(size_t) pc] = 1.0f;

    SECTION ("bass peaked at A (pc 9) -> Am wins")
    {
        BeatChroma beat = ambiguous;
        beat.bassAvg[9] = 1.0f;

        auto scores = scoreBeatObservations (beat, templates);
        ChordSymbol best = symbolForIndex (argmax36 (scores));

        REQUIRE (best.pitchClass == 9);
        REQUIRE (best.quality == ChordQuality::Minor);
    }

    SECTION ("bass peaked at C (pc 0) -> C major wins")
    {
        BeatChroma beat = ambiguous;
        beat.bassAvg[0] = 1.0f;

        auto scores = scoreBeatObservations (beat, templates);
        ChordSymbol best = symbolForIndex (argmax36 (scores));

        REQUIRE (best.pitchClass == 0);
        REQUIRE (best.quality == ChordQuality::Major);
    }
}

TEST_CASE ("ChordDecoderTests.SyntheticProgression", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr int beatsPerChord = 8; // 2 bars @ 4/4

    const std::vector<fixtures::ChordSpec> progression = {
        { { 0, ChordQuality::Major }, -1 }, // C
        { { 5, ChordQuality::Major }, -1 }, // F
        { { 7, ChordQuality::Major }, -1 }, // G
        { { 9, ChordQuality::Minor }, -1 }, // Am
    };

    auto buffer = fixtures::renderChordProgression (progression, bpm, kSampleRate, beatsPerChord);
    auto chroma = extractChromaFromBuffer (buffer);
    auto beats = buildBeatGrid (bpm, (double) buffer.getNumSamples() / kSampleRate);

    auto segments = decodeChords (chroma, beats);

    REQUIRE (segments.size() == 4);

    const ChordSymbol expected[4] = {
        { 0, ChordQuality::Major },
        { 5, ChordQuality::Major },
        { 7, ChordQuality::Major },
        { 9, ChordQuality::Minor },
    };
    for (size_t i = 0; i < 4; ++i)
    {
        INFO ("segment " << i << " pitchClass=" << segments[i].chord.pitchClass
              << " quality=" << (int) segments[i].chord.quality);
        CHECK (sameChord (segments[i].chord, expected[i]));
    }
}

TEST_CASE ("ChordDecoderTests.SegmentsAlignToBeats", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr int beatsPerChord = 8;

    const std::vector<fixtures::ChordSpec> progression = {
        { { 0, ChordQuality::Major }, -1 },
        { { 5, ChordQuality::Major }, -1 },
        { { 7, ChordQuality::Major }, -1 },
        { { 9, ChordQuality::Minor }, -1 },
    };

    auto buffer = fixtures::renderChordProgression (progression, bpm, kSampleRate, beatsPerChord);
    auto chroma = extractChromaFromBuffer (buffer);
    auto beats = buildBeatGrid (bpm, (double) buffer.getNumSamples() / kSampleRate);

    auto segments = decodeChords (chroma, beats);
    REQUIRE (! segments.empty());

    const auto& beatTimes = beats.beatTimesSeconds;
    const double lastBeatTime = beatTimes.back();
    const double secondToLastBeatTime = beatTimes[beatTimes.size() - 2];
    const double medianIbi = lastBeatTime - secondToLastBeatTime; // uniform grid -- exact
    constexpr double kTolerance = 1e-9;

    for (size_t i = 0; i < segments.size(); ++i)
    {
        const auto& seg = segments[i];

        REQUIRE (seg.startBeatIndex >= 0);
        REQUIRE (seg.startBeatIndex < (int) beatTimes.size());
        REQUIRE (std::abs (seg.startSeconds - beatTimes[(size_t) seg.startBeatIndex]) < kTolerance);

        if (seg.endBeatIndex < (int) beatTimes.size())
            REQUIRE (std::abs (seg.endSeconds - beatTimes[(size_t) seg.endBeatIndex]) < kTolerance);
        else
            REQUIRE (std::abs (seg.endSeconds - (lastBeatTime + medianIbi)) < kTolerance);

        // Tiling: no gaps/overlaps between consecutive segments.
        if (i > 0)
            REQUIRE (seg.startBeatIndex == segments[i - 1].endBeatIndex);
    }

    REQUIRE (segments.front().startBeatIndex == 0);
    REQUIRE (segments.back().endBeatIndex == (int) beatTimes.size());
}

TEST_CASE ("ChordDecoderTests.BassRootBias", "[chordanalysis]")
{
    constexpr double durationSeconds = 2.0;
    constexpr double bpm = 120.0; // 4 beats @ 0.5s each over 2.0s

    SECTION ("bass at A (pc 9) -> decoded root is A minor")
    {
        auto buffer = renderAmbiguousChordWithBass (9, kSampleRate, durationSeconds);
        auto chroma = extractChromaFromBuffer (buffer);
        auto beats = buildBeatGrid (bpm, durationSeconds);

        auto segments = decodeChords (chroma, beats);
        REQUIRE (! segments.empty());
        CHECK (segments.front().chord.pitchClass == 9);
        CHECK (segments.front().chord.quality == ChordQuality::Minor);
    }

    SECTION ("bass at C (pc 0) -> decoded root is C major")
    {
        auto buffer = renderAmbiguousChordWithBass (0, kSampleRate, durationSeconds);
        auto chroma = extractChromaFromBuffer (buffer);
        auto beats = buildBeatGrid (bpm, durationSeconds);

        auto segments = decodeChords (chroma, beats);
        REQUIRE (! segments.empty());
        CHECK (segments.front().chord.pitchClass == 0);
        CHECK (segments.front().chord.quality == ChordQuality::Major);
    }
}

TEST_CASE ("ChordDecoderTests.SilenceGivesNoChord", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr double chordSeconds = 2.0;   // 4 beats each
    constexpr double silenceSeconds = 2.0; // 4 beats of true silence

    const int chordLengthSamples = (int) std::llround (chordSeconds * kSampleRate);
    const int silenceLengthSamples = (int) std::llround (silenceSeconds * kSampleRate);
    const int totalSamples = chordLengthSamples * 2 + silenceLengthSamples;

    juce::AudioBuffer<float> buffer (1, totalSamples);
    buffer.clear();

    renderStaticChordInto (buffer, 0, chordLengthSamples, kSampleRate, { 0, ChordQuality::Major }, 0); // C major
    renderStaticChordInto (buffer, chordLengthSamples + silenceLengthSamples, chordLengthSamples,
                            kSampleRate, { 7, ChordQuality::Major }, 7); // G major

    auto chroma = extractChromaFromBuffer (buffer);
    auto beats = buildBeatGrid (bpm, (double) totalSamples / kSampleRate);

    auto segments = decodeChords (chroma, beats);
    REQUIRE (segments.size() >= 3);

    CHECK (segments.front().chord.pitchClass == 0);
    CHECK (segments.front().chord.quality == ChordQuality::Major);

    CHECK (segments.back().chord.pitchClass == 7);
    CHECK (segments.back().chord.quality == ChordQuality::Major);

    const bool anyNoChord = std::any_of (segments.begin(), segments.end(), [] (const ChordSegment& s) {
        return s.chord.quality == ChordQuality::NoChord;
    });
    CHECK (anyNoChord);
}

TEST_CASE ("ChordDecoderTests.FastChanges", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr int beatsPerChord = 1; // 1 chord per beat -- tests self-transition bias doesn't oversmooth

    const std::vector<fixtures::ChordSpec> progression = {
        { { 0, ChordQuality::Major }, -1 },  // C
        { { 5, ChordQuality::Major }, -1 },  // F
        { { 7, ChordQuality::Major }, -1 },  // G
        { { 9, ChordQuality::Minor }, -1 },  // Am
        { { 0, ChordQuality::Major }, -1 },  // C
        { { 5, ChordQuality::Major }, -1 },  // F
        { { 0, ChordQuality::Major }, -1 },  // C
        { { 7, ChordQuality::Major }, -1 },  // G
    };

    auto buffer = fixtures::renderChordProgression (progression, bpm, kSampleRate, beatsPerChord);
    auto chroma = extractChromaFromBuffer (buffer);
    auto beats = buildBeatGrid (bpm, (double) buffer.getNumSamples() / kSampleRate);

    auto segments = decodeChords (chroma, beats);

    REQUIRE (segments.size() == progression.size());
    for (size_t i = 0; i < progression.size(); ++i)
    {
        INFO ("segment " << i << " pitchClass=" << segments[i].chord.pitchClass
              << " quality=" << (int) segments[i].chord.quality);
        CHECK (sameChord (segments[i].chord, progression[i].chord));
    }
}
