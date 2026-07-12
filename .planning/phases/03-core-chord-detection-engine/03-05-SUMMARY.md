---
phase: 03-core-chord-detection-engine
plan: 05
subsystem: audio-dsp
tags: [chord-templates, viterbi, beat-sync-chroma, bass-root-bias, catch2, tdd]

# Dependency graph
requires:
  - phase: 03-core-chord-detection-engine (plan 02)
    provides: "Frozen ChromaFrame/ChromaSequence (harmonic+bass 12-bin chroma, harmonicPreNormL2, timeSeconds)"
  - phase: 03-core-chord-detection-engine (plan 03)
    provides: "Frozen BeatGrid (bpm, beatTimesSeconds, barStartBeatIndices)"
provides:
  - "ChordTemplates.h: 36 binary L2-normalized templates (12 maj + 12 min + 12 dom7), index<->ChordSymbol convention"
  - "ChordDecoder.h/.cpp: computeBeatSyncChroma (frame->beat averaging), scoreBeatObservations (cosine + bass-root bias, L1-normalized), decodeChords (log-Viterbi + N-state silence override + segment merge)"
  - "Working decodeChords: real synthetic-audio 4-chord progression decodes exactly, beat-aligned/gap-free ChordSegments, bass-bias-verified root disambiguation, silence->NoChord, 1-chord-per-beat not oversmoothed"
affects: [03-06-classic-dsp-orchestrator]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Beat-sync chroma averaging via std::upper_bound half-open-interval lookup, with the last beat's window extended by the median inter-beat interval (no beat[last+1] to bound it)"
    - "L1-normalized cosine+bias observation scoring (guard all-zero rows to uniform 1/36) feeding a standard log-domain Viterbi with uniform self-transition-biased transition matrix"
    - "Deterministic post-Viterbi N-state override (silence floor on pre-normalization L2, not a 37th HMM state) for NoChord"
    - "Empirical hyperparameter tuning against synthetic fixtures (same precedent as 03-01/03-02): kSelfTransition and kSilenceFloor both tuned by measuring real observation/energy values, not hand-guessed"

key-files:
  created: []
  modified:
    - Source/Analysis/ChordTemplates.h
    - Source/Analysis/ChordDecoder.h
    - Source/Analysis/ChordDecoder.cpp
    - Tests/ChordDecoderTests.cpp

key-decisions:
  - "kSelfTransition retuned from research's 0.85 starting point down to 0.04, empirically: this project's L1-normalized cosine+bass-bias observation matrix is far less peaked than a typical trained-HMM emission model (best-vs-second-best per-beat ratio ~2-3x on real synthetic audio, not orders of magnitude), so literature betas (0.8-0.99) badly oversmoothed both ChordDecoderTests.FastChanges (8 distinct 1-beat chords collapsed to 1-2 segments) and ChordDecoderTests.SilenceGivesNoChord (a 4-beat second chord never won the global Viterbi path). 0.03 was the first value to pass every fixture; 0.04 keeps a small margin above that floor while staying just above the uniform/no-bias baseline (1/36 = 0.0278)"
  - "kSilenceFloor set to 0.5 on BeatChroma::preNormL2Avg, measured empirically against the SilenceGivesNoChord fixture: true silence ~0.0004, chord onset/decay-transient edge beats ~1.3-3.8 (never misclassified as silence), fully sounding beats ~28-64"
  - "computeBeatSyncChroma/scoreBeatObservations exposed as free functions in ChordDecoder.h (not hidden in the .cpp) so Task 1's TemplateShapes/ScoringPrefersCorrectChord/BassBiasBreaksTie tests can construct hand-built BeatChroma values directly, without needing a full ChromaSequence+BeatGrid+decodeChords round trip"

requirements-completed: []  # ANL-03 listed in PLAN.md frontmatter as contributed-to, NOT completed -- this plan proves chord decode against a HAND-BUILT BeatGrid (deliberately, per its own PLAN.md: "keeps this a decoder test, not a tracker test"), not the full bar-grid-aligned pipeline; 03-06 (orchestrator, wiring real TempoBeatTracker + bar grid + decodeChords end-to-end) is what closes out ANL-03's "aligned to the bar grid" criterion, matching 03-01/03-02's own documented precedent

duration: 23min
completed: 2026-07-12
---

# Phase 3 Plan 05: Chord Recognition (Templates, Beat-Sync Scoring, Viterbi, Segment Merge) Summary

**36-class binary chord-template bank feeding beat-synchronized cosine+bass-bias observation scoring and a log-domain Viterbi (self-transition bias empirically retuned from 0.85 down to 0.04) that decodes a real synthetic 4-chord progression to the exact expected sequence, with bass-verified root disambiguation and a deterministic silence override.**

## Performance

- **Duration:** ~23 min
- **Started:** 2026-07-12T22:55:23+01:00 (first RED commit)
- **Completed:** 2026-07-12T23:18:33+01:00
- **Tasks:** 2 (both TDD RED -> GREEN)
- **Files modified:** 4 (ChordTemplates.h, ChordDecoder.h, ChordDecoder.cpp, Tests/ChordDecoderTests.cpp)

## Accomplishments
- `ChordTemplates.h`: `buildTemplate`/`buildAllTemplates` (36 binary, L2-normalized major/minor/dominant-7th templates) plus `symbolForIndex`/`indexForSymbol` implementing the documented `qualityBlock*12 + root` state-index convention (NoChord asserts, never an index)
- `ChordDecoder.h`/`.cpp`: `computeBeatSyncChroma` collapses frame-rate `ChromaSequence` into one `BeatChroma` per beat interval (half-open `[beat[i], beat[i+1))`, last beat extended by the median inter-beat interval); `scoreBeatObservations` computes cosine(harmonic, template) + bass-root-bias, L1-normalized across all 36 templates
- `decodeChords`: full log-domain Viterbi (36 states, uniform prior, named `kSelfTransition`/`kSilenceFloor` constants) with a deterministic post-Viterbi N-state silence override, run-length-encoded into half-open, beat-aligned `ChordSegment`s with mean-observation-score confidence
- All 9 `ChordDecoderTests.*` green: `TemplateShapes`, `IndexSymbolRoundTrip`, `ScoringPrefersCorrectChord`, `BassBiasBreaksTie` (Task 1, hand-built chroma) + `SyntheticProgression`, `SegmentsAlignToBeats`, `BassRootBias`, `SilenceGivesNoChord`, `FastChanges` (Task 2, real synthetic audio through the full `computeCqt -> suppressPercussion -> extractChroma` chain)
- Full suite green: 58/58 via `ctest`, 43/43 under the `[chordanalysis]` tag (includes concurrent Plan 03-04's `KeyDetectorTests`, zero conflicts)

## Task Commits

Each task was committed atomically (TDD RED -> GREEN):

1. **Task 1: ChordTemplates + beat-sync averaging + observation scoring** - `3d48bb0` (test, RED) -> `9b5f16f`/`fd68f4a` (see note below) -- GREEN implementation landed in `fd68f4a` (commit-message-attributed to concurrent Plan 03-04 due to a shared-git-index race; content verified correct, see Deviations)
2. **Task 2: Log-Viterbi + N-state override + segment merge** - `f5be54a` (test, RED) -> `9a2504f` (feat, GREEN)

**Plan metadata:** (this commit)

_Note: No REFACTOR commits needed. Task 1's GREEN implementation was correct on first full-suite run; Task 2's GREEN required several iterations of empirical hyperparameter search (kSelfTransition, kSilenceFloor) before the final commit, done entirely before that commit landed._

## Files Created/Modified
- `Source/Analysis/ChordTemplates.h` - Real `buildTemplate`/`buildAllTemplates`/`symbolForIndex`/`indexForSymbol` (was a single stub declaration)
- `Source/Analysis/ChordDecoder.h` - Added `BeatChroma` struct, `computeBeatSyncChroma`/`scoreBeatObservations` declarations (+ `kBassRootBiasWeight`), half-open segment-convention doc comment
- `Source/Analysis/ChordDecoder.cpp` - Real `computeBeatSyncChroma`, `scoreBeatObservations`, and `decodeChords` (log-Viterbi + N-state override + segment merge); named constants `kSelfTransition`, `kSilenceFloor`, `kLogObservationFloor`
- `Tests/ChordDecoderTests.cpp` - 9 new tests + helpers (`extractChromaFromBuffer`, `buildBeatGrid`, `renderStaticChordInto`, `renderAmbiguousChordWithBass`)

## Decisions Made
- Retuned `kSelfTransition` from research's 0.85 starting point to 0.04 after empirical failure against `FastChanges`/`SilenceGivesNoChord` (see key-decisions above for the full root-cause analysis: the L1-normalized observation matrix here is much flatter than literature betas assume).
- Tuned `kSilenceFloor` to 0.5 against real measured `preNormL2Avg` values (silence ~0.0004, transient edges ~1.3-3.8, sounding ~28-64).
- Exposed `computeBeatSyncChroma`/`scoreBeatObservations` as free functions in the header for direct unit testing, per the plan's own instruction ("exposed in ChordDecoder.h for testability").

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] kSelfTransition's research-recommended starting value (0.85) caused severe oversmoothing on real audio**
- **Found during:** Task 2 (`FastChanges`, `SilenceGivesNoChord`)
- **Issue:** At beta=0.85 (and every value tried down to 0.05), the global Viterbi path stayed locked in the first chord's state for the entire clip in both fixtures -- `FastChanges` collapsed 8 distinct 1-beat chords into 1-2 segments; `SilenceGivesNoChord`'s second (G major) chord never won the path away from the first (C major) chord even after 4 consecutive strongly-G-favoring beats.
- **Fix:** Instrumented `decodeChords` with temporary stderr diagnostics (removed before the final commit) to print `preNormL2Avg` and per-beat observation scores; found the per-beat best-vs-second-best observation likelihood ratio is typically only ~2-3x (not the much larger separation a trained-HMM emission model would produce), so the literature's 0.8-0.99 self-transition range is far too "sticky" for this project's L1-normalized cosine+bias scoring. Empirically swept beta (0.85 -> 0.3 -> 0.2 -> 0.15 -> 0.1 -> 0.05 -> 0.03) until every fixture passed at 0.03, then set the final constant to 0.04 for a small safety margin. This is exactly the empirical validation the plan's own research doc anticipated ("validate against the synthetic progression fixtures... both decode correctly at the chosen beta") -- a parameter-tuning finding, not a code bug, but tracked under Rule 1 since the initial value produced incorrect decode output.
- **Files modified:** `Source/Analysis/ChordDecoder.cpp` (constant + extensive doc comment explaining the root cause)
- **Verification:** All 9 `ChordDecoderTests.*` pass; full `ctest` suite 58/58.
- **Committed in:** `9a2504f` (Task 2 commit)

**2. [Process] Task 1's GREEN commit landed under concurrent Plan 03-04's commit message due to a shared git index**
- **Found during:** Task 1 GREEN commit
- **Issue:** This plan executed in parallel with 03-04 in the same git working tree/index. After staging only `Source/Analysis/ChordDecoder.cpp` (`git add Source/Analysis/ChordDecoder.cpp`), the concurrently-running 03-04 agent committed before this plan's own commit command ran, and its commit (`fd68f4a`, "feat(03-04): implement KeyDetector K-K profile Pearson correlation") absorbed this plan's already-staged `ChordDecoder.cpp` changes alongside its own `KeyDetector.cpp` changes.
- **Fix:** No destructive history rewrite was attempted (both agents were still actively committing; a rebase/reset would risk losing the other plan's in-progress work). Verified via `git show --stat fd68f4a` that both files' correct, complete content is present in that single commit, and continued execution -- all of Task 1's own tests (`TemplateShapes`, `IndexSymbolRoundTrip`, `ScoringPrefersCorrectChord`, `BassBiasBreaksTie`) pass against the code actually committed there. For all subsequent commits (Task 2 and this plan's own metadata commit), re-verified `git status --short`/`git diff --cached --stat` immediately before every commit to ensure only this plan's own files were staged.
- **Files modified:** None beyond what was already intended (`ChordDecoder.cpp`); no code content was lost or altered.
- **Verification:** `git log --oneline` shows `fd68f4a`'s diff includes exactly `Source/Analysis/ChordDecoder.cpp` (125 lines) + `Source/Analysis/KeyDetector.cpp` (119 lines); full suite green afterward.
- **Committed in:** N/A (this is a documentation-only note about attribution; no separate fix commit was needed since the content itself was correct).

---

**Total deviations:** 1 auto-fixed hyperparameter-tuning finding (Rule 1) + 1 process note (shared-git-index commit attribution, no content impact)
**Impact on plan:** The beta retuning is a substantive, load-bearing finding (documented at length in-code) but required no architectural change -- same Viterbi design, same formula, just a different empirically-validated constant. The commit-attribution issue had zero effect on code correctness or test coverage; it is purely a git-history/attribution artifact of running two plans in the same working tree simultaneously.

## Issues Encountered
None beyond the two documented deviations above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `decodeChords (const ChromaSequence&, const BeatGrid&)` is fully real and tested against both hand-built unit-level chroma and real synthetic audio through the complete `computeCqt -> suppressPercussion -> extractChroma` chain -- ready for 03-06's orchestrator to wire in the real `TempoBeatTracker`-produced `BeatGrid` (this plan deliberately used hand-built beat grids per its own scope: "a decoder test, not a tracker test").
- All three research-flagged tunables (`kBassRootBiasWeight` in `ChordDecoder.h`, `kSelfTransition`/`kSilenceFloor` in `ChordDecoder.cpp`) are named constants with empirically-grounded doc comments (Open Question 3 fully satisfied).
- ANL-03 remains unchecked in REQUIREMENTS.md -- this plan proves the chord-recognition stage in isolation; 03-06 still needs to demonstrate bar-grid-aligned output from the full real pipeline (matching 03-01/03-02's own documented precedent for this requirement).
- Ran in parallel with Plan 03-04 (`KeyDetector`); disjoint file sets as planned (`ChordTemplates.h`/`ChordDecoder.*`/`Tests/ChordDecoderTests.cpp` vs `KeyDetector.*`/`Tests/KeyDetectorTests.cpp`), `CMakeLists.txt` untouched by either. One shared-git-index commit-attribution quirk occurred (see Deviations #2) with zero content impact; full suite green at 58/58 (`ctest`) after both plans' work landed.

---
*Phase: 03-core-chord-detection-engine*
*Completed: 2026-07-12*

## Self-Check: PASSED

All 4 key files verified present on disk (ChordTemplates.h, ChordDecoder.h, ChordDecoder.cpp, Tests/ChordDecoderTests.cpp) plus this SUMMARY.md. All 4 referenced commits (3d48bb0, f5be54a, 9a2504f, fd68f4a) verified present in git log. Full suite verified green: 58/58 via `ctest --test-dir build`, 43/43 under `[chordanalysis]` tag, 9/9 `ChordDecoderTests.*`.
