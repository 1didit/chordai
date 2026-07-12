#include <catch2/catch_test_macros.hpp>

#include "Source/Analysis/KeyDetector.h"
#include "Source/Analysis/AudioPreprocessing.h"
#include "Source/Analysis/ChromaExtractor.h"
#include "Source/Analysis/ConstantQAnalysis.h"
#include "Source/Analysis/HarmonicPercussiveFilter.h"
#include "Source/Analysis/TuningEstimator.h"
#include "Tests/SyntheticFixtures.h"

#include <array>

namespace
{
    // Rotates a 12-bin pitch-class vector up by `shift` semitones: the energy
    // that was at absolute pitch class p moves to (p + shift) % 12.
    std::array<float, 12> rotatePitchClasses (const std::array<float, 12>& v, int shift)
    {
        std::array<float, 12> rotated {};
        for (int p = 0; p < 12; ++p)
            rotated[(size_t) ((p + shift + 12) % 12)] = v[(size_t) p];
        return rotated;
    }

    constexpr double kSourceRate = 44100.0;
    constexpr double kBpm = 120.0;

    // Full real chain: computeCqt -> suppressPercussion -> extractChroma.
    // tuningCents is either a fixed 0.0 (in-tune fixtures) or the real
    // estimateTuningCents(cqt) result (detuned fixture).
    ChromaSequence extractChromaThroughFullChain (const juce::AudioBuffer<float>& buffer, bool applyTuningEstimate)
    {
        auto preprocessed = preprocessForAnalysis (buffer, kSourceRate, {});
        auto cqt = computeCqt (preprocessed.chromaSamples, preprocessed.chromaRate);
        suppressPercussion (cqt, kHpssKernelSeconds);
        const double tuningCents = applyTuningEstimate ? estimateTuningCents (cqt) : 0.0;
        return extractChroma (cqt, tuningCents);
    }
}

// Task 1: KeyDetector core -- K-K profile Pearson correlation over a hand-built
// 12-bin vector (no ChromaSequence/audio chain involved yet, see Task 2 below).

TEST_CASE ("KeyDetectorTests.PureCMajorVector", "[chordanalysis]")
{
    // Weighted like a I-IV-V progression's chord tones: strong C, E, G; medium
    // F, A; light D, B; everything else silent.
    std::array<float, 12> chroma {
        1.0f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.2f
    };

    auto key = detectKey (chroma);

    CHECK (key.tonicPitchClass == 0);
    CHECK (key.isMajor == true);
}

TEST_CASE ("KeyDetectorTests.PureAMinorVector", "[chordanalysis]")
{
    // Strong A, C, E plus G# (the leading tone supplied by the E-major
    // dominant) -- this is exactly what disambiguates A minor from its
    // relative major (C major) sharing the same natural-minor pitch classes.
    // pitch classes: C=0, D=2, E=4, F=5, G=7, A=9, B=11, G#=8.
    std::array<float, 12> chroma {};
    chroma[0] = 1.0f;   // C
    chroma[2] = 0.3f;   // D
    chroma[4] = 1.0f;   // E
    chroma[5] = 0.3f;   // F
    chroma[7] = 0.2f;   // G
    chroma[8] = 1.0f;   // G# (leading tone)
    chroma[9] = 1.0f;   // A
    chroma[11] = 0.3f;  // B

    auto key = detectKey (chroma);

    CHECK (key.tonicPitchClass == 9);
    CHECK (key.isMajor == false);
}

TEST_CASE ("KeyDetectorTests.TransposedProfileInvariance", "[chordanalysis]")
{
    std::array<float, 12> cMajorChroma {
        1.0f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.2f
    };

    auto dMajorChroma = rotatePitchClasses (cMajorChroma, 2);
    auto key = detectKey (dMajorChroma);

    CHECK (key.tonicPitchClass == 2);
    CHECK (key.isMajor == true);
}

TEST_CASE ("KeyDetectorTests.ConfidenceMargins", "[chordanalysis]")
{
    std::array<float, 12> stronglyKeyed {
        1.0f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.2f
    };
    std::array<float, 12> ambiguous {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };

    auto strongKey = detectKey (stronglyKeyed);
    auto ambiguousKey = detectKey (ambiguous);

    CHECK (strongKey.confidence > ambiguousKey.confidence);
    CHECK (ambiguousKey.confidence < 0.1f);
    CHECK (ambiguousKey.confidence >= 0.0f);
}

// Task 2: audio-integration key tests through the real chroma path
// (renderChordProgression -> preprocessForAnalysis -> computeCqt ->
// suppressPercussion -> extractChroma -> accumulateChroma -> detectKey).
// 2 bars/chord @ 120 BPM (beatsPerChord=8) so each fixture is well over the
// >= 8s duration needed for stable accumulated chroma.

TEST_CASE ("KeyDetectorTests.CMajorFixture", "[chordanalysis]")
{
    // C-F-G-C, roots 0/5/7/0, all major.
    std::vector<fixtures::ChordSpec> chords {
        { { 0, ChordQuality::Major } },
        { { 5, ChordQuality::Major } },
        { { 7, ChordQuality::Major } },
        { { 0, ChordQuality::Major } }
    };
    auto buffer = fixtures::renderChordProgression (chords, kBpm, kSourceRate, 8);

    auto chroma = extractChromaThroughFullChain (buffer, false);
    REQUIRE (! chroma.frames.empty());

    auto key = detectKey (accumulateChroma (chroma));

    CHECK (key.tonicPitchClass == 0);
    CHECK (key.isMajor == true);
}

TEST_CASE ("KeyDetectorTests.RelativeMinor", "[chordanalysis]")
{
    // Am-Dm-E-Am, roots 9/2/4/9, qualities minor/minor/MAJOR/minor -- the E
    // major dominant supplies the G# leading tone that breaks the C-major
    // relative-key tie; a natural-minor-only progression would be genuinely
    // ambiguous with C major, so this fixture must include the E major chord.
    std::vector<fixtures::ChordSpec> chords {
        { { 9, ChordQuality::Minor } },
        { { 2, ChordQuality::Minor } },
        { { 4, ChordQuality::Major } },
        { { 9, ChordQuality::Minor } }
    };
    auto buffer = fixtures::renderChordProgression (chords, kBpm, kSourceRate, 8);

    auto chroma = extractChromaThroughFullChain (buffer, false);
    REQUIRE (! chroma.frames.empty());

    auto key = detectKey (accumulateChroma (chroma));

    CHECK (key.tonicPitchClass == 9);
    CHECK (key.isMajor == false);
}

TEST_CASE ("KeyDetectorTests.DetunedKeyStillDetected", "[chordanalysis]")
{
    // C-F-G-C at detuneCents=-30 -- still C major once the tuning estimate is
    // applied through the real chain.
    std::vector<fixtures::ChordSpec> chords {
        { { 0, ChordQuality::Major } },
        { { 5, ChordQuality::Major } },
        { { 7, ChordQuality::Major } },
        { { 0, ChordQuality::Major } }
    };
    auto buffer = fixtures::renderChordProgression (chords, kBpm, kSourceRate, 8, -30.0);

    auto chroma = extractChromaThroughFullChain (buffer, true);
    REQUIRE (! chroma.frames.empty());

    auto key = detectKey (accumulateChroma (chroma));

    CHECK (key.tonicPitchClass == 0);
    CHECK (key.isMajor == true);
}
