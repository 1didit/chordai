---
phase: 03-core-chord-detection-engine
plan: 01
subsystem: audio-dsp
tags: [juce, constant-q-cpp, cqt, catch2, cmake, tdd, audio-preprocessing]

# Dependency graph
requires:
  - phase: 02-audio-import-waveform
    provides: "LoadedAudio/AudioFileLoader decode pipeline + Catch2/CTest test infrastructure conventions this plan extends"
provides:
  - "Frozen ChordAnalyzer/AnalysisResult headless interface contracts (ChordQuality, ChordSymbol, ChordSegment, KeyResult, AnalysisResult, CancelToken, ProgressCallback)"
  - "23-file Source/Analysis/ skeleton (all Phase 3 module headers + stub .cpp) registered in both CMake targets — Wave 2/3 plans need zero CMakeLists.txt edits"
  - "constant-q-cpp static lib wired via FetchContent (pinned SHA), linked into ChordAI + ChordAITests, license captured in THIRD_PARTY_LICENSES.md"
  - "Tests/SyntheticFixtures.h: renderChordProgression/renderClickTrack/addPercussiveBursts/toneEnergy in-memory audio generators, reusable by 03-02..03-06"
  - "Real AudioPreprocessing.cpp: mono downmix + dual-rate (8000/11025 Hz) WindowedSincInterpolator resample + region clamp"
  - "Observed CQT runtime parameters resolving 03-RESEARCH.md Open Question 1: totalBins=288, minFrequency=16.6695 Hz, columnHop=34 samples (~3.08 ms)"
affects: [03-02-constant-q-chroma-pipeline, 03-03-tempo-beat-tracking, 03-04-key-detection, 03-05-chord-recognition, 03-06-classic-dsp-orchestrator]

# Tech tracking
tech-stack:
  added: ["constant-q-cpp (pinned commit 7ac8404, MIT-style + bundled KissFFT BSD-3-Clause)", "juce::juce_dsp module (linked into both CMake targets)"]
  patterns: ["Frozen-contract headers marked with '// FROZEN CONTRACT' comment", "Stub .cpp per module so both plugin + test targets link before the owning plan implements real behavior", "In-memory-only Catch2 fixtures (no file I/O) reusable across Phase 3 plans", "Runtime-queried CQT parameters asserted via self-consistency, never hardcoded literal ranges"]

key-files:
  created:
    - Source/Analysis/ChordAnalyzer.h
    - Source/Analysis/AnalysisResult.h
    - Source/Analysis/AudioPreprocessing.h
    - Source/Analysis/AudioPreprocessing.cpp
    - Source/Analysis/ConstantQAnalysis.h/.cpp
    - Source/Analysis/TuningEstimator.h/.cpp
    - Source/Analysis/HarmonicPercussiveFilter.h/.cpp
    - Source/Analysis/ChromaExtractor.h/.cpp
    - Source/Analysis/OnsetEnvelope.h/.cpp
    - Source/Analysis/TempoBeatTracker.h/.cpp
    - Source/Analysis/KeyDetector.h/.cpp
    - Source/Analysis/ChordTemplates.h
    - Source/Analysis/ChordDecoder.h/.cpp
    - Source/Analysis/ClassicDspChordAnalyzer.h/.cpp
    - Tests/SyntheticFixtures.h
    - Tests/SyntheticFixturesTests.cpp
    - Tests/AudioPreprocessingTests.cpp
    - Tests/ChromaExtractorTests.cpp (real CqtEngineSanity test)
    - Tests/TuningEstimatorTests.cpp / TempoBeatTrackerTests.cpp / KeyDetectorTests.cpp / ChordDecoderTests.cpp / ClassicDspChordAnalyzerTests.cpp (empty stubs, owned by later plans)
    - THIRD_PARTY_LICENSES.md
  modified:
    - CMakeLists.txt

key-decisions:
  - "constant-q-cpp pinned at commit 7ac8404 (verified current master HEAD via git ls-remote at execution time, matching the plan's pre-specified SHA exactly — no substitution needed)"
  - "constant-q-cpp static lib include paths + kiss_fft_scalar=double compile definition derived directly from upstream Makefile.inc's GENERAL_FLAGS (-I. -Icq -Isrc -Isrc/ext/kissfft -Isrc/ext/kissfft/tools), not guessed"
  - "CqtEngineSanity's minFrequency assertion changed from the plan's hand-guessed [25,70] Hz literal range to a self-consistency check (minFreq == maxFreq/2^(totalBins/binsPerOctave)) after empirically observing the library returns ~16.67 Hz (8 octaves) rather than ~32.7 Hz (7 octaves) for a requested C1 minimum — resolves 03-RESEARCH.md Open Question 1 with real measured values instead of a hardcoded assumption"

requirements-completed: []  # ANL-06/ANL-03 listed in PLAN.md frontmatter as contributed-to, NOT completed — see "Next Phase Readiness"; REQUIREMENTS.md left unchecked until fully evidenced (Phase 2 precedent: IMP-01 stayed unchecked across 02-01/02-02 until drag-and-drop UI landed)

duration: 20min
completed: 2026-07-12
---

# Phase 3 Plan 01: Build/Test Foundation for Core Chord-Detection Engine Summary

**Frozen headless ChordAnalyzer/AnalysisResult contracts, a 23-file Source/Analysis/ DSP module skeleton wired into both CMake targets, constant-q-cpp linked from a pinned commit with license capture, and working synthetic-fixture + dual-rate audio-preprocessing implementations — all downstream Phase 3 plans (03-02..03-06) can now build without ever touching CMakeLists.txt.**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-07-12T21:06:52Z
- **Completed:** 2026-07-12T21:26:13Z
- **Tasks:** 3 (Task 3 executed as TDD RED→GREEN)
- **Files modified:** 32 created/changed across 4 commits

## Accomplishments
- `ChordAnalyzer`/`AnalysisResult` frozen headless interface (zero GUI/ThreadPool deps) transcribed exactly from 03-RESEARCH.md
- 21 stub module files (10 `.h`/`.cpp` pairs + `ChordTemplates.h`) registered in both `ChordAI` and `ChordAITests` CMake targets, so Wave 2/3 plans implement real bodies without any build-file edits
- `constant-q-cpp` wired from a pinned commit as a hand-enumerated static lib (no upstream CMakeLists.txt), linked into both targets; `THIRD_PARTY_LICENSES.md` captures verbatim COPYING text for constant-q-cpp (MIT-style) and bundled KissFFT (BSD-3-Clause)
- `CqtEngineSanity` test exercises the raw `CQSpectrogram`/`CQParameters` API end-to-end (440 Hz sine, streaming `process()` + `getRemainingOutput()` flush, peak-bin detection within 3% of 440 Hz) and resolves the research's Open Question 1 with real measured runtime parameters
- `Tests/SyntheticFixtures.h` provides deterministic in-memory chord-progression/click-track/percussion-overlay/Goertzel-energy generators, TDD'd RED→GREEN against their own self-tests
- `Source/Analysis/AudioPreprocessing.cpp` implements real mono downmix + dual-rate (8000 Hz / 11025 Hz) `WindowedSincInterpolator` resampling with region clamping, TDD'd RED→GREEN

## Task Commits

1. **Task 1: Frozen analysis contracts + module skeletons + CMake registration** - `722703d` (feat)
2. **Task 2: constant-q-cpp dependency + license capture + CQT engine sanity test** - `0af1d18` (feat)
3. **Task 3: Synthetic audio fixtures + dual-rate audio preprocessing** - `4cfe20b` (test, RED) → `5ae930b` (feat, GREEN)

**Plan metadata:** (this commit)

_Note: Task 3 was TDD — RED commit (`4cfe20b`) confirmed 5 new tests fail against stub fixtures/preprocessing, GREEN commit (`5ae930b`) confirmed all pass. No REFACTOR commit needed — implementation was clean on first pass._

## Files Created/Modified
- `Source/Analysis/ChordAnalyzer.h` - Frozen pure-virtual interface: `CancelToken`, `ProgressCallback`, `analyse()`
- `Source/Analysis/AnalysisResult.h` - Frozen value types: `ChordQuality`, `ChordSymbol`, `ChordSegment`, `KeyResult`, `AnalysisResult`
- `Source/Analysis/AudioPreprocessing.h/.cpp` - Frozen `PreprocessedAudio` struct + real dual-rate downmix/resample implementation
- `Source/Analysis/{ConstantQAnalysis,TuningEstimator,HarmonicPercussiveFilter,ChromaExtractor,OnsetEnvelope,TempoBeatTracker,KeyDetector,ChordTemplates,ChordDecoder,ClassicDspChordAnalyzer}.h(/.cpp)` - Module skeletons owned by plans 03-02..03-06
- `Tests/SyntheticFixtures.h` - In-memory chord/click/percussion/Goertzel fixture generators (221 lines)
- `Tests/SyntheticFixturesTests.cpp`, `Tests/AudioPreprocessingTests.cpp` - Real behavior tests (TDD)
- `Tests/ChromaExtractorTests.cpp` - Real `CqtEngineSanity` test against raw constant-q-cpp API
- `Tests/{TuningEstimator,TempoBeatTracker,KeyDetector,ChordDecoder,ClassicDspChordAnalyzer}Tests.cpp` - Empty Catch2 TUs, owned by later plans
- `CMakeLists.txt` - `constant_q_cpp` FetchContent + static lib target, `CHORDAI_ANALYSIS_SOURCES` registered in both targets, 8 new test files registered, `juce::juce_dsp` linked into both targets
- `THIRD_PARTY_LICENSES.md` - New file: constant-q-cpp + bundled KissFFT license capture

## Decisions Made
- Pinned `constant-q-cpp` SHA `7ac8404...` verified as current master HEAD via `git ls-remote` — matched the plan's pre-specified commit exactly, no substitution needed.
- Static-lib include paths (`cq/`, `src/`, `src/ext/kissfft/`, `src/ext/kissfft/tools/`) and `-Dkiss_fft_scalar=double` derived directly from reading upstream `Makefile.inc`'s `GENERAL_FLAGS`, rather than guessed, after the public `SYSTEM PUBLIC` include (repo root, for `#include "cq/CQSpectrogram.h"`) alone proved insufficient for the library's own internal unqualified includes.
- Adjusted `CqtEngineSanity`'s min-frequency assertion from a hand-guessed literal range to a runtime self-consistency check after observing constant-q-cpp actually returns ~16.67 Hz / 288 bins (8 octaves) rather than the hand-calculated ~32.7 Hz / 7 octaves for the requested C1-C8 span — this is the real resolution of 03-RESEARCH.md's Open Question 1.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] CqtEngineSanity's hardcoded minFrequency range didn't match actual library behavior**
- **Found during:** Task 2 (constant-q-cpp sanity test)
- **Issue:** Plan's literal assertion `getMinFrequency() in [25, 70]` failed — the library returned 16.6695 Hz (288 bins / 8 octaves) instead of the hand-calculated ~32.7 Hz (7 octaves), because the actual octave-rounding behavior for `CQParameters(11025.0, 32.70, 4186.01, 36)` isn't exactly what a naive `log2(maxFreq/minFreq)` calculation predicts.
- **Fix:** Replaced the literal range with a self-consistency check (`minFreq == maxFreq / 2^(totalBins/binsPerOctave)`, runtime-queried on both sides) plus a generous sanity floor/ceiling (5–40 Hz), matching research's own pitfall guidance to query rather than hardcode.
- **Files modified:** `Tests/ChromaExtractorTests.cpp`
- **Verification:** `CqtEngineSanity` passes (11/11 assertions); observed values logged via `WARN`/`INFO` and recorded above, resolving Open Question 1
- **Committed in:** `0af1d18` (Task 2 commit)

**2. [Rule 3 - Blocking] constant-q-cpp static lib needed private include paths + a compile definition beyond the plan's stated SYSTEM PUBLIC root include**
- **Found during:** Task 2 (static lib target setup)
- **Issue:** Upstream `.cpp` files use unqualified includes (`#include "CQKernel.h"`, `#include "kiss_fft.h"`) resolved relative to `cq/`, `src/ext/kissfft/`, and `src/ext/kissfft/tools/` — not reachable via the single root `SYSTEM PUBLIC` include dir alone. KissFFT also defaults its scalar type to `float`, mismatching constant-q-cpp's `double`-based `RealSequence`/`RealColumn` types.
- **Fix:** Added `PRIVATE` include dirs for `cq/`, `src/`, `src/ext/kissfft/`, `src/ext/kissfft/tools/` and `target_compile_definitions(constant_q_cpp PRIVATE kiss_fft_scalar=double)`, both read directly from upstream's own `Makefile.inc` `GENERAL_FLAGS`/`-Dkiss_fft_scalar` rather than guessed.
- **Files modified:** `CMakeLists.txt`
- **Verification:** `constant_q_cpp` static lib compiles and links cleanly into both `ChordAI` and `ChordAITests`
- **Committed in:** `0af1d18` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)
**Impact on plan:** Both fixes were necessary for the constant-q-cpp integration to build and produce a correct, non-fragile sanity test. No scope creep — both stayed within Task 2's stated boundaries.

## Issues Encountered
None beyond the two deviations documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- **ANL-06 and ANL-03 remain unchecked in REQUIREMENTS.md** — this plan only establishes the frozen interface shape and skeleton wiring, not the working behavior. ANL-06 ("invocable and verifiable through a `ChordAnalyzer` interface") needs `ClassicDspChordAnalyzer::analyse()` to actually orchestrate the pipeline (lands in 03-06). ANL-03 ("chord progression detected... aligned to the bar grid") needs the real CQT/chroma/beat/decode implementations (lands across 03-02, 03-03, 03-05). Both will be marked complete once fully evidenced by their owning plans, matching the Phase 2 precedent for IMP-01.
- All 23 `Source/Analysis/` skeleton files + both frozen contract headers are in place; plans 03-02 through 03-06 can implement their owned modules' real bodies without touching `CMakeLists.txt`.
- `Tests/SyntheticFixtures.h` fixture generators (chord progressions with bass override + detune, click tracks with syncopation, percussive-burst overlay, Goertzel energy) are ready for reuse by 03-02's `PercussionRobustness`/`TuningEstimatorTests`, 03-03's `OctaveErrorResistance`, and 03-05's `ChordDecoderTests`.
- Real `AudioPreprocessing.cpp` gives every downstream module correctly-resampled 8000 Hz (onset) and 11025 Hz (chroma) mono streams from any region selection.
- Observed CQT runtime parameters (bins=288, minFreq≈16.67 Hz, hop≈3.08 ms/34 samples) are now known ground truth for 03-02's chroma/tuning/percussion-filter implementation — no further guessing needed.
- Full suite green: 21/21 tests (15 pre-existing + 6 new `[chordanalysis]`-tagged).

---
*Phase: 03-core-chord-detection-engine*
*Completed: 2026-07-12*

## Self-Check: PASSED

All key files verified on disk (ChordAnalyzer.h, AnalysisResult.h, AudioPreprocessing.cpp, SyntheticFixtures.h, THIRD_PARTY_LICENSES.md, this SUMMARY.md). All 4 task commits (722703d, 0af1d18, 4cfe20b, 5ae930b) verified present in git log.
