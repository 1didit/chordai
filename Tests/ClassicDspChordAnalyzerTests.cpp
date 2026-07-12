#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Source/Analysis/ClassicDspChordAnalyzer.h"
#include "Tests/SyntheticFixtures.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace
{
    constexpr double kSampleRate = 44100.0;

    struct NoOpCancelToken : ChordAnalyzer::CancelToken
    {
        bool shouldCancel() const override { return false; }
    };

    struct AlwaysCancelToken : ChordAnalyzer::CancelToken
    {
        bool shouldCancel() const override { return true; }
    };

    // Flips to "cancel" once externally told to (see Cancellation test's
    // "flips after the first progress callback" section) -- flag lives behind
    // a shared_ptr so the ProgressCallback lambda and this token can share it
    // without shouldCancel() (const) needing a mutable member.
    struct FlagCancelToken : ChordAnalyzer::CancelToken
    {
        std::shared_ptr<bool> flag = std::make_shared<bool> (false);
        bool shouldCancel() const override { return *flag; }
    };

    bool sameChord (const ChordSymbol& a, const ChordSymbol& b)
    {
        return a.pitchClass == b.pitchClass && a.quality == b.quality;
    }

    // renderChordProgression's sustained chord tones carry no per-beat attacks
    // of their own -- only fades at chord-change boundaries -- so TempoBeatTracker
    // has nothing periodic at the beat rate to lock onto. Overlaying
    // addPercussiveBursts (broadband, every beat) gives it real rhythmic onset
    // content while suppressPercussion (already wired into the orchestrator)
    // recovers the underlying harmonic chroma regardless, exactly as
    // ChromaExtractorTests.PercussionRobustness (03-02) already validates. This
    // is THE fixture for exercising the full real pipeline end-to-end, as
    // opposed to 03-05's decoder-only tests which deliberately used a
    // hand-built BeatGrid to avoid needing real beat tracking.
    juce::AudioBuffer<float> renderRhythmicProgression (const std::vector<fixtures::ChordSpec>& chords,
                                                          double bpm, double sampleRate, int beatsPerChord)
    {
        auto buffer = fixtures::renderChordProgression (chords, bpm, sampleRate, beatsPerChord);
        fixtures::addPercussiveBursts (buffer, bpm, sampleRate);
        return buffer;
    }

    // Builds a `totalSeconds`-long silent buffer with `content` copied in
    // starting at `contentStartSeconds` (truncated if it would run past the
    // end). Used by RegionRestriction to place a progression inside a longer,
    // otherwise-silent source file.
    juce::AudioBuffer<float> embedInSilence (const juce::AudioBuffer<float>& content, double contentStartSeconds,
                                              double totalSeconds, double sampleRate)
    {
        const int totalSamples = (int) std::llround (totalSeconds * sampleRate);
        const int startSample = (int) std::llround (contentStartSeconds * sampleRate);

        juce::AudioBuffer<float> buffer (content.getNumChannels(), totalSamples);
        buffer.clear();

        for (int ch = 0; ch < content.getNumChannels(); ++ch)
        {
            const int copyLength = juce::jmin (content.getNumSamples(), totalSamples - startSample);
            if (copyLength > 0)
                buffer.copyFrom (ch, startSample, content, ch, 0, copyLength);
        }

        return buffer;
    }

    // True if `needle` appears as an ORDERED (not necessarily contiguous)
    // subsequence of `haystack`'s chords -- proves the progression appears in
    // the right order without requiring an exact segment-for-segment match
    // (real beat tracking may introduce extra/boundary segments a hand-built
    // grid wouldn't).
    bool containsOrderedSubsequence (const std::vector<ChordSegment>& haystack, const std::vector<ChordSymbol>& needle)
    {
        size_t needleIndex = 0;
        for (const auto& segment : haystack)
        {
            if (needleIndex >= needle.size())
                break;
            if (sameChord (segment.chord, needle[needleIndex]))
                ++needleIndex;
        }
        return needleIndex == needle.size();
    }
}

TEST_CASE ("ClassicDspChordAnalyzerTests.HeadlessInvocation", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    const std::vector<fixtures::ChordSpec> progression = {
        { { 0, ChordQuality::Major }, -1 }, // C
        { { 5, ChordQuality::Major }, -1 }, // F
        { { 7, ChordQuality::Major }, -1 }, // G
        { { 9, ChordQuality::Minor }, -1 }, // Am
    };
    auto buffer = renderRhythmicProgression (progression, bpm, kSampleRate, 8); // 16s

    ClassicDspChordAnalyzer analyzer;
    ChordAnalyzer& iface = analyzer; // proves ANL-06's swappable-interface contract
    NoOpCancelToken noCancel;

    AnalysisResult result = iface.analyse (buffer, kSampleRate, {}, {}, noCancel);

    CHECK (result.wasCancelled == false);
    CHECK (result.key.tonicPitchClass == 0);
    CHECK (result.key.isMajor == true);
    CHECK (result.bpm == Catch::Approx (bpm).margin (3.0));
    REQUIRE (! result.beatTimesSeconds.empty());

    for (int idx : result.barStartBeatIndices)
        CHECK (idx % 4 == 0);

    const std::vector<ChordSymbol> expectedOrder = {
        { 0, ChordQuality::Major },
        { 5, ChordQuality::Major },
        { 7, ChordQuality::Major },
        { 9, ChordQuality::Minor },
    };
    CHECK (containsOrderedSubsequence (result.chords, expectedOrder));
}

TEST_CASE ("ClassicDspChordAnalyzerTests.Cancellation", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    const std::vector<fixtures::ChordSpec> progression = {
        { { 0, ChordQuality::Major }, -1 },
        { { 5, ChordQuality::Major }, -1 },
        { { 7, ChordQuality::Major }, -1 },
        { { 9, ChordQuality::Minor }, -1 },
    };
    auto buffer = renderRhythmicProgression (progression, bpm, kSampleRate, 8);

    ClassicDspChordAnalyzer analyzer;
    ChordAnalyzer& iface = analyzer;

    SECTION ("cancelled from the very start")
    {
        AlwaysCancelToken cancelToken;
        AnalysisResult result = iface.analyse (buffer, kSampleRate, {}, {}, cancelToken);

        CHECK (result.wasCancelled == true);
        CHECK (result.chords.empty());
    }

    SECTION ("cancelled right after the first progress callback")
    {
        FlagCancelToken cancelToken;
        int progressCallCount = 0;
        ChordAnalyzer::ProgressCallback onProgress = [&] (double /*fraction*/, const juce::String& /*stage*/)
        {
            ++progressCallCount;
            if (progressCallCount == 1)
                *cancelToken.flag = true;
        };

        AnalysisResult result = iface.analyse (buffer, kSampleRate, {}, onProgress, cancelToken);

        CHECK (result.wasCancelled == true);
        CHECK (result.chords.empty());
        CHECK (progressCallCount >= 1);
    }
}

TEST_CASE ("ClassicDspChordAnalyzerTests.ProgressMonotonic", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    const std::vector<fixtures::ChordSpec> progression = {
        { { 0, ChordQuality::Major }, -1 },
        { { 5, ChordQuality::Major }, -1 },
        { { 7, ChordQuality::Major }, -1 },
        { { 9, ChordQuality::Minor }, -1 },
    };
    auto buffer = renderRhythmicProgression (progression, bpm, kSampleRate, 8);

    ClassicDspChordAnalyzer analyzer;
    ChordAnalyzer& iface = analyzer;
    NoOpCancelToken noCancel;

    std::vector<std::pair<double, juce::String>> recorded;
    ChordAnalyzer::ProgressCallback onProgress = [&] (double fraction, const juce::String& stage)
    {
        recorded.emplace_back (fraction, stage);
    };

    iface.analyse (buffer, kSampleRate, {}, onProgress, noCancel);

    REQUIRE (! recorded.empty());
    CHECK (recorded.front().first <= 0.1);
    CHECK (recorded.back().first == Catch::Approx (1.0));

    for (size_t i = 1; i < recorded.size(); ++i)
        CHECK (recorded[i].first >= recorded[i - 1].first);

    auto hasStage = [&] (const juce::String& label)
    {
        return std::any_of (recorded.begin(), recorded.end(),
                             [&] (const auto& entry) { return entry.second == label; });
    };
    CHECK (hasStage ("beat"));
    CHECK (hasStage ("chroma"));
    CHECK (hasStage ("key"));
    CHECK (hasStage ("chords"));
}

TEST_CASE ("ClassicDspChordAnalyzerTests.RegionRestriction", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    const std::vector<fixtures::ChordSpec> progression = {
        { { 0, ChordQuality::Major }, -1 },
        { { 7, ChordQuality::Major }, -1 },
    };
    auto progressionBuffer = renderRhythmicProgression (progression, bpm, kSampleRate, 8); // 8s
    auto buffer = embedInSilence (progressionBuffer, 5.0, 20.0, kSampleRate);

    ClassicDspChordAnalyzer analyzer;
    ChordAnalyzer& iface = analyzer;
    NoOpCancelToken noCancel;

    AnalysisResult result = iface.analyse (buffer, kSampleRate, { 5.0, 15.0 }, {}, noCancel);

    CHECK (result.wasCancelled == false);
    REQUIRE (! result.chords.empty());

    constexpr double kTolerance = 1.0; // last segment's window may extend past the region by ~medianIbi
    for (const auto& segment : result.chords)
    {
        CHECK (segment.startSeconds >= 5.0 - kTolerance);
        CHECK (segment.startSeconds <= 15.0 + kTolerance);
        CHECK (segment.endSeconds >= 5.0 - kTolerance);
        CHECK (segment.endSeconds <= 15.0 + kTolerance);
    }
}

TEST_CASE ("ClassicDspChordAnalyzerTests.DegenerateInput", "[chordanalysis]")
{
    ClassicDspChordAnalyzer analyzer;
    ChordAnalyzer& iface = analyzer;
    NoOpCancelToken noCancel;

    SECTION ("empty buffer")
    {
        juce::AudioBuffer<float> emptyBuffer;
        AnalysisResult result = iface.analyse (emptyBuffer, kSampleRate, {}, {}, noCancel);

        CHECK (result.wasCancelled == false);
        CHECK (result.bpm == Catch::Approx (0.0));
        CHECK (result.chords.empty());
    }

    SECTION ("0.2s buffer")
    {
        const int numSamples = (int) std::llround (0.2 * kSampleRate);
        juce::AudioBuffer<float> shortBuffer (1, numSamples);
        shortBuffer.clear();

        AnalysisResult result = iface.analyse (shortBuffer, kSampleRate, {}, {}, noCancel);

        CHECK (result.wasCancelled == false);
        CHECK (result.bpm == Catch::Approx (0.0));
        CHECK (result.chords.empty());
    }
}
