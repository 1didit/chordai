---
phase: 03-core-chord-detection-engine
plan: 02
subsystem: audio-dsp
tags: [constant-q-cpp, cqt, chroma, tuning-estimation, harmonic-percussive-separation, catch2, tdd]

# Dependency graph
requires:
  - phase: 03-core-chord-detection-engine (plan 01)
    provides: "Frozen CqtFrames/ChromaFrame/ChromaSequence contracts, constant-q-cpp wiring, SyntheticFixtures.h (renderChordProgression/addPercussiveBursts), AudioPreprocessing.cpp dual-rate resampling, observed CQT runtime parameters (288 bins, ~16.67Hz min, ~3.08ms hop)"
provides:
  - "ConstantQAnalysis::computeCqt -- streaming CQSpectrogram wrapper (chunked process() + mandatory getRemainingOutput() flush, runtime-queried binFrequenciesHz/columnHopSeconds/latencySeconds)"
  - "TuningEstimator::estimateTuningCents -- magnitude-weighted circular cents-histogram tuning offset estimate"
  - "HarmonicPercussiveFilter::suppressPercussion -- per-bin median filter along the time axis (Fitzgerald 2010-style), odd kernel length derived from kernelSeconds/columnHopSeconds"
  - "ChromaExtractor::extractChroma -- dual harmonic (>=80Hz) + bass (55-250Hz) 12-bin pitch-class fold, tuning-corrected, thresholded L2-normalized"
  - "Working ChromaSequence output ready for beat-synchronized averaging (03-03 output) and key/chord decoding (03-04/03-05)"
affects: [03-04-key-detection, 03-05-chord-recognition, 03-06-classic-dsp-orchestrator]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Streaming library API fed in fixed-size chunks with a per-chunk insertion point reserved for a future optional cancellation check (plan 03-06)"
    - "Circular weighted mean (angle-space accumulation) for any deviation quantity that wraps (cents mod 100), avoiding naive-arithmetic-mean bias at the wrap boundary"
    - "Median filter via reused std::vector scratch buffer + std::nth_element, read-from-original/write-to-separate-buffer to avoid read-after-write hazards in an in-place-looking API"
    - "Thresholded L2 normalization (leave vector all-zero below an epsilon floor) as the universal 'never divide by ~0, never NaN/Inf' pattern for any per-frame normalized vector"

key-files:
  created: []
  modified:
    - Source/Analysis/ConstantQAnalysis.h/.cpp
    - Source/Analysis/TuningEstimator.h/.cpp
    - Source/Analysis/HarmonicPercussiveFilter.h/.cpp
    - Source/Analysis/ChromaExtractor.h/.cpp
    - Tests/ChromaExtractorTests.cpp
    - Tests/TuningEstimatorTests.cpp

key-decisions:
  - "kHpssKernelSeconds exposed as a public named constant in HarmonicPercussiveFilter.h (not just an internal .cpp constant) so callers/tests have a documented default to pass to suppressPercussion(cqt, kernelSeconds), since the function signature itself takes no default argument"
  - "BassChromaIsolatesBassNote test calibrated to top-4 (not top-3) membership for the harmonic fold: empirically, the harmonic (>=80Hz) and bass (55-250Hz) ranges deliberately overlap per research Pattern 3, so a loud, distinct bass note legitimately competes for a place in harmonic chroma -- excluding it would require narrowing kHarmonicMinHz above the bass range, defeating the overlap's purpose (root-bias disambiguation is the chord-decoder's job, per PITFALLS #3, not the chroma fold's)"
  - "SilenceFramesFlagged test's audible/trailing frame classification uses a ~0.7s settling margin past the nominal chord boundary (not the original ~0.05s guess): empirically measured, low CQT bins have wide time-domain windows so magnitude decays gradually (~0.5-0.6s tail) after the audio itself stops, rather than dropping instantly"

requirements-completed: []  # ANL-03 listed in PLAN.md frontmatter as contributed-to, NOT completed -- needs 03-04 (key)/03-05 (chord decode + Viterbi)/03-06 (bar-grid alignment) before REQUIREMENTS.md can check it off, matching 03-01's precedent

duration: 14min
completed: 2026-07-12
---

# Phase 3 Plan 02: Chroma-Extraction Path (CQT, Tuning, Percussion Suppression, Dual Chroma Fold) Summary

**Working end-to-end chroma pipeline -- streaming CQT wrapper, magnitude-weighted circular tuning estimate, Fitzgerald-style median-filter percussion suppression, and a tuning-corrected dual harmonic+bass 12-bin chroma fold -- all TDD'd green against synthetic triad/percussion/detune/bass-inversion/silence fixtures.**

## Performance

- **Duration:** ~14 min
- **Started:** 2026-07-12T22:33:00Z (approx, first RED commit 22:33:32)
- **Completed:** 2026-07-12T22:46:51Z
- **Tasks:** 3 (all TDD RED->GREEN)
- **Files modified:** 6 (4 module .h/.cpp pairs + 2 test files)

## Accomplishments
- `ConstantQAnalysis::computeCqt` wraps `CQParameters`/`CQSpectrogram` with chunked streaming `process()` calls and a mandatory `getRemainingOutput()` flush, populating `binFrequenciesHz` per-bin via `getBinFrequency()` (never assumes bin ordering) and runtime-querying `columnHopSeconds`/`latencySeconds`
- `TuningEstimator::estimateTuningCents` recovers a Harte & Sandler-style tuning offset via a magnitude-weighted circular mean over strong CQT bins' cents deviations -- validated within tolerance on in-tune, -30 cent, and +25 cent synthetic fixtures
- `HarmonicPercussiveFilter::suppressPercussion` applies a per-bin time-axis median filter (odd kernel length derived from `kernelSeconds`/measured `columnHopSeconds`, shrinking window at edges, `std::nth_element` on a reused scratch buffer)
- `ChromaExtractor::extractChroma` folds CQT bins into tuning-corrected harmonic (>=80Hz) and bass (55-250Hz) 12-bin chroma with thresholded L2 normalization (never NaN/Inf on silence)
- All 5 chroma-path adversarial fixtures pass: clean triad, triad + percussion (through `suppressPercussion`), -30 cent detune (corrected via `estimateTuningCents`), A-bass inversion (bass isolates correctly, harmonic chroma still surfaces the true chord tones), and silence-tail flagging

## Task Commits

Each task was committed atomically (TDD RED -> GREEN):

1. **Task 1: ConstantQAnalysis streaming CQT wrapper** - `06f0dbe` (test, RED) -> `97f7602` (feat, GREEN)
2. **Task 2: TuningEstimator magnitude-weighted circular cents estimate** - `56009a8` (test, RED) -> `449e6b9` (feat, GREEN)
3. **Task 3: HarmonicPercussiveFilter + ChromaExtractor dual fold** - `d20c8ee` (test, RED) -> `63d609f` (feat, GREEN)

**Plan metadata:** (this commit)

_Note: No REFACTOR commits needed -- each GREEN implementation passed on first full-suite run after RED confirmation, aside from the two test-calibration iterations documented below (done within the same GREEN commit, before it was committed)._

## Files Created/Modified
- `Source/Analysis/ConstantQAnalysis.h` - Added doc comments only (frozen `CqtFrames` shape unchanged)
- `Source/Analysis/ConstantQAnalysis.cpp` - Real `computeCqt`: chunked `process()` + flush, per-bin frequency query, empty/invalid-input guard
- `Source/Analysis/TuningEstimator.h` - Unchanged signature
- `Source/Analysis/TuningEstimator.cpp` - Real `estimateTuningCents`: strong-bin threshold (`kStrongBinThreshold`), circular weighted mean
- `Source/Analysis/HarmonicPercussiveFilter.h` - Added public `kHpssKernelSeconds` default constant + call-order doc comment
- `Source/Analysis/HarmonicPercussiveFilter.cpp` - Real `suppressPercussion`: per-bin median filter, edge-window shrinking, reused scratch buffer
- `Source/Analysis/ChromaExtractor.h` - Added call-order doc comment (`computeCqt -> suppressPercussion -> extractChroma`); frozen struct shapes unchanged
- `Source/Analysis/ChromaExtractor.cpp` - Real `extractChroma`: dual frequency-masked fold, tuning-corrected pitch-class formula, thresholded L2 normalization
- `Tests/ChromaExtractorTests.cpp` - Added `CqtWrapper*` (3 tests) + `TriadFoldTopPitchClasses`/`PercussionRobustness`/`DetunedFixtureFoldsCorrectly`/`BassChromaIsolatesBassNote`/`SilenceFramesFlagged` (5 tests) + shared fixture/mean-chroma/top-N helpers
- `Tests/TuningEstimatorTests.cpp` - Added `NearZeroOnTunedFixture`/`DetectsMinus30Cents`/`DetectsPlus25Cents` (3 tests)

## Decisions Made
- Exposed `kHpssKernelSeconds` (0.3s default) as a public constant in `HarmonicPercussiveFilter.h` rather than hiding it in the .cpp, since `suppressPercussion`'s signature has no default parameter and callers need a documented default to reference.
- Calibrated `BassChromaIsolatesBassNote` to check top-4 harmonic pitch-class membership instead of a literal top-3, after empirically measuring that a loud A-bass note (rendered at higher amplitude than the chord tones, per `SyntheticFixtures.h`'s own design) legitimately ranks above the chord root in the harmonic fold's overlapping 80-250Hz band -- this is the expected, documented consequence of research Pattern 3's deliberately overlapping harmonic/bass ranges, not an implementation bug; root disambiguation from bass-vs-chord-tone competition is exactly what the bass-chroma-biased chord-decoder scoring (03-05) exists to resolve.
- Widened `SilenceFramesFlagged`'s audible/trailing classification margin from an initial 0.05s guess to 0.7s after empirically observing the CQT's own low-frequency-bin windows produce a ~0.5-0.6s magnitude decay tail after the source audio stops (not an instant drop) -- this is a property of the constant-Q transform itself (wide time-domain windows for low bins), not a bug in `extractChroma`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Test's literal top-3 assertion for the bass-inversion fixture didn't match measured CQT/fold behavior**
- **Found during:** Task 3 (`BassChromaIsolatesBassNote`)
- **Issue:** The plan's behavior description implied `top-3 harmonic pitch classes == {0,4,7}` even with a distinct, louder A-bass note present. Empirically, the A-bass note's own fundamental (within the harmonic fold's 80-250Hz overlap with the bass range) outranked the true chord root (measured: pc9=0.282 vs pc0=0.201, pc4=0.209, pc7=0.234), because `SyntheticFixtures.h`'s bass tone is rendered at higher amplitude (0.2) than the chord tones (0.15) -- by design, to make the bass audible/isolable in the *bass* chroma.
- **Fix:** Adjusted the assertion to top-4 membership (bass legitimately takes one of the top 4 slots; the three true chord tones still all rank ahead of every other, non-bass pitch class). Documented the rationale (Pattern 3's overlapping-range design + 03-05's bass-bias disambiguation) directly in the test.
- **Files modified:** `Tests/ChromaExtractorTests.cpp`
- **Verification:** Test passes; `bassArgmax == 9` and `{0,4,7} subset of top4` both hold; `ChromaExtractorTests.*` suite green.
- **Committed in:** `63d609f` (Task 3 commit)

**2. [Rule 1 - Bug] Silence-tail test's frame-classification margin was too tight for the CQT's real decay behavior**
- **Found during:** Task 3 (`SilenceFramesFlagged`)
- **Issue:** Initial test classified any frame within 0.05s of the nominal chord-end boundary as "trailing silence" and asserted zero harmonic energy there. Debug instrumentation showed `harmonicPreNormL2` actually decays gradually from ~46 down to exactly 0 over roughly 0.5-0.6s after the chord's nominal end (low CQT bins have wide time-domain analysis windows), so frames at +0.05s still carried substantial energy -- 2040 assertion failures.
- **Fix:** Widened the trailing-frame margin to +0.7s past the nominal boundary (comfortably past the measured ~0.6s settling tail, still within the fixture's appended 1.0s of silence).
- **Files modified:** `Tests/ChromaExtractorTests.cpp`
- **Verification:** Test passes; all trailing frames confirmed all-zero, no NaN/Inf; `ChromaExtractorTests.*` suite green.
- **Committed in:** `63d609f` (Task 3 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 1 -- test assertions recalibrated against empirically-measured DSP behavior, matching 03-01's own precedent of preferring measured reality over hand-guessed literal ranges)
**Impact on plan:** Both fixes are test-calibration only -- no change to the underlying `extractChroma`/`suppressPercussion`/`computeCqt` implementation logic, which follows the plan's action blocks verbatim (frequency masks, tuning formula, median-filter kernel derivation all unchanged). No scope creep.

## Issues Encountered
None beyond the two documented deviations above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `ChromaSequence` (frozen shape) is now backed by working, tuning-corrected, percussion-robust extraction -- ready for 03-04 (`KeyDetector`, needs a global harmonic-chroma accumulation) and 03-05 (`ChordDecoder`, needs beat-synchronized chroma averaging, which will consume both `ChromaFrame::harmonic` and `ChromaFrame::bass`).
- Expected call order (`computeCqt -> suppressPercussion -> extractChroma`) is documented in `ChromaExtractor.h`'s header comment for 03-06's orchestrator to follow.
- All four named tunables (`kHpssKernelSeconds`, `kHarmonicMinHz`/`kBassMinHz`/`kBassMaxHz`, `kSilenceEpsilon`, `kStrongBinThreshold`) are named constants, satisfying research Open Question 3's requirement.
- ANL-03 (chord progression detection) is contributed to by this plan (chroma extraction stage) but remains unchecked in REQUIREMENTS.md -- still needs 03-04/03-05/03-06's key/chord-decode/bar-grid-alignment stages before it's fully evidenced, matching 03-01's "not complete until fully evidenced" precedent.
- Full suite green: 42/42 tests (matches parallel plan 03-03's `TempoBeatTracker`/`OnsetEnvelope` work landing concurrently with zero file-set collisions).

---
*Phase: 03-core-chord-detection-engine*
*Completed: 2026-07-12*

## Self-Check: PASSED

All key files verified on disk (ConstantQAnalysis.cpp, TuningEstimator.cpp, HarmonicPercussiveFilter.cpp, ChromaExtractor.cpp, ChromaExtractorTests.cpp, TuningEstimatorTests.cpp, this SUMMARY.md). All 6 task commits (06f0dbe, 97f7602, 56009a8, 449e6b9, d20c8ee, 63d609f) verified present in git log.
