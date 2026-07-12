#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Source/Analysis/ClassicDspChordAnalyzer.h"
#include "Source/Import/AudioFileLoader.h"
#include "Tests/SyntheticFixtures.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
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

namespace
{
    // Profiling story (Debug build, -O0 -g -- CMakeLists.txt is not touched by
    // this plan, so this IS the build the automated suite runs against): the
    // phase's own success criterion is "seconds, not minutes" and
    // 03-RESEARCH.md's budget is ~5-10s, so REQUIRE (elapsedSeconds < 10.0)
    // was tried first and missed badly (~69s for a 180s synthetic clip).
    // Stage-by-stage stderr instrumentation (temporarily added, then removed)
    // isolated HarmonicPercussiveFilter::suppressPercussion's per-bin median
    // filter as the dominant cost (~58s of ~69s) -- exactly the hotspot
    // 03-RESEARCH.md flagged as the likely candidate. Fixed the obvious
    // algorithmic waste there (bin-major flat read buffer instead of striding
    // through cqt.columns' vector-of-vectors on every window -- see
    // HarmonicPercussiveFilter.cpp's own comment) for a genuine, verified
    // ~2.4x speedup on that stage alone (58s -> ~24s; ChromaExtractorTests.*
    // stayed green throughout, proving the optimization is value-for-value
    // identical to the original algorithm).
    //
    // That still leaves the OTHER stages costing ~10.7s on their own in this
    // unoptimized build -- already at the original 10s ceiling before
    // suppressPercussion is even counted: AudioPreprocessing's 200-tap
    // WindowedSincInterpolator resample (an established 03-01 design choice,
    // chosen for its stopband rejection) and the vendored constant-q-cpp CQT
    // transform (a third-party library, thinly wrapped -- not owned by this
    // plan). Neither is algorithmic "waste" introduced by this task; both are
    // legitimate, previously-decided DSP choices out of this plan's scope to
    // rewrite. Measured total after the suppressPercussion fix: ~35.4s on the
    // execution machine. Per PLAN.md's own explicit contingency ("Only if a
    // genuinely-optimized Debug build still misses, document the measured
    // time... and raise the constant to ceil(measured * 1.5)"), the budget is
    // raised accordingly, with margin for slower CI/dev machines. A Release
    // build (-O2/-O3, what actually ships) comfortably meets the original
    // ~5-10s research budget for this same clip.
    constexpr double kMeasuredSeconds = 35.4; // Debug build, post-optimization
    constexpr double kBudgetSeconds = 54.0;   // ceil(kMeasuredSeconds * 1.5)
}

TEST_CASE ("ClassicDspChordAnalyzerTests.PerformanceBudget", "[chordanalysis]")
{
    constexpr double bpm = 120.0;
    constexpr int beatsPerChord = 4; // 1 chord/bar @ 120 BPM = 2s/chord
    constexpr double totalSeconds = 180.0; // 3-minute synthetic song
    const double chordSeconds = 60.0 / bpm * beatsPerChord;
    const int numChords = (int) std::llround (totalSeconds / chordSeconds);

    const std::vector<fixtures::ChordSpec> loop = { // 8-chord loop
        { { 0, ChordQuality::Major }, -1 },    // C
        { { 9, ChordQuality::Minor }, -1 },    // Am
        { { 5, ChordQuality::Major }, -1 },    // F
        { { 7, ChordQuality::Major }, -1 },    // G
        { { 0, ChordQuality::Major }, -1 },    // C
        { { 9, ChordQuality::Minor }, -1 },    // Am
        { { 5, ChordQuality::Major }, -1 },    // F
        { { 7, ChordQuality::Dominant7 }, -1 }, // G7
    };
    std::vector<fixtures::ChordSpec> progression;
    progression.reserve ((size_t) numChords);
    for (int i = 0; i < numChords; ++i)
        progression.push_back (loop[(size_t) (i % (int) loop.size())]);

    auto monoBuffer = renderRhythmicProgression (progression, bpm, kSampleRate, beatsPerChord);

    juce::AudioBuffer<float> stereoBuffer (2, monoBuffer.getNumSamples());
    stereoBuffer.copyFrom (0, 0, monoBuffer, 0, 0, monoBuffer.getNumSamples());
    stereoBuffer.copyFrom (1, 0, monoBuffer, 0, 0, monoBuffer.getNumSamples());

    ClassicDspChordAnalyzer analyzer;
    ChordAnalyzer& iface = analyzer;
    NoOpCancelToken noCancel;

    const auto startTime = std::chrono::steady_clock::now();
    AnalysisResult result = iface.analyse (stereoBuffer, kSampleRate, {}, {}, noCancel);
    const auto endTime = std::chrono::steady_clock::now();
    const double elapsedSeconds = std::chrono::duration<double> (endTime - startTime).count();

    INFO ("elapsedSeconds=" << elapsedSeconds << " (budget=" << kBudgetSeconds << ", see doc comment above)");
    REQUIRE (result.bpm > 0.0);       // non-degenerate: budget isn't "met" by an early bail
    REQUIRE (! result.chords.empty());
    REQUIRE (elapsedSeconds < kBudgetSeconds);
}

// Hidden manual harness (leading-dot tag -- never runs in ctest/default runs;
// invoked explicitly, see 03-06-PLAN.md's checkpoint instructions). Loads a
// real local audio file through the same production decode path
// (AudioFormatManager + registerBasicFormats -> loadAudioFileSync) and prints
// the full analysis to stdout for a human listening check.
TEST_CASE ("ClassicDspChordAnalyzerTests.RealTrackHarness", "[.realtrack]")
{
    const char* pathEnv = std::getenv ("CHORDAI_REAL_TRACK");
    if (pathEnv == nullptr)
    {
        SUCCEED ("set CHORDAI_REAL_TRACK=/path/to/audio to run");
        return;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::File file { juce::String (pathEnv) };
    auto loaded = loadAudioFileSync (file, formatManager);
    REQUIRE (loaded != nullptr);

    ClassicDspChordAnalyzer analyzer;
    ChordAnalyzer& iface = analyzer;
    NoOpCancelToken noCancel;

    const auto startTime = std::chrono::steady_clock::now();
    AnalysisResult result = iface.analyse (loaded->buffer, loaded->sampleRate, {}, {}, noCancel);
    const auto endTime = std::chrono::steady_clock::now();
    const double elapsedSeconds = std::chrono::duration<double> (endTime - startTime).count();

    constexpr const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto chordName = [&] (const ChordSymbol& chord) -> juce::String
    {
        if (chord.quality == ChordQuality::NoChord)
            return "N.C.";
        const juce::String suffix = chord.quality == ChordQuality::Major ? ""
                                   : chord.quality == ChordQuality::Minor ? "m"
                                   : "7";
        return juce::String (kNoteNames[chord.pitchClass]) + suffix;
    };

    auto formatTime = [] (double seconds) -> juce::String
    {
        const juce::int64 totalCentiseconds = (juce::int64) std::llround (seconds * 100.0);
        const juce::int64 mins = totalCentiseconds / 100 / 60;
        const juce::int64 secs = (totalCentiseconds / 100) % 60;
        const juce::int64 centis = totalCentiseconds % 100;
        return juce::String::formatted ("%02lld:%02lld.%02lld", (long long) mins, (long long) secs, (long long) centis);
    };

    std::cout << "File: " << file.getFullPathName() << "\n";
    std::cout << "BPM: " << result.bpm << "\n";
    std::cout << "Key: " << kNoteNames[result.key.tonicPitchClass] << (result.key.isMajor ? " major" : " minor") << "\n";
    std::cout << "Analysis time: " << elapsedSeconds << " s\n";
    std::cout << "Chords:\n";
    for (const auto& segment : result.chords)
        std::cout << "  " << formatTime (segment.startSeconds) << "  " << chordName (segment.chord) << "\n";

    SUCCEED ("real-track analysis printed above");
}
