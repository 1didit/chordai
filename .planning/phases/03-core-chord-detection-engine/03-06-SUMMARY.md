---
phase: 03-core-chord-detection-engine
plan: 06
subsystem: audio-dsp
tags: [orchestration, chord-analyzer, catch2, tdd, performance-budget, real-track-verification]

# Dependency graph
requires:
  - phase: 03-core-chord-detection-engine (plan 02)
    provides: "computeCqt / estimateTuningCents / suppressPercussion / extractChroma chroma path"
  - phase: 03-core-chord-detection-engine (plan 03)
    provides: "computeOnsetEnvelope / trackBeats tempo-beat-bar path"
  - phase: 03-core-chord-detection-engine (plan 04)
    provides: "accumulateChroma / detectKey key-detection path"
  - phase: 03-core-chord-detection-engine (plan 05)
    provides: "decodeChords chord-recognition path (beat-sync chroma, Viterbi, segment merge)"
provides:
  - "ClassicDspChordAnalyzer: synchronous, headless, cancellable, progress-reporting orchestration of the full preprocess -> beat -> chroma -> key -> chords pipeline behind the ChordAnalyzer interface"
  - "computeCqt optional shouldCancel hook for sub-second cancellation latency inside the longest pipeline stage"
  - "Performance budget enforced in-suite (3-minute synthetic song analyzed well under 10s Debug)"
  - "Hidden [.realtrack] harness (CHORDAI_REAL_TRACK env var) with CHORDAI_DIAG stage-by-stage diagnostics, for human listening verification on any real audio file"
  - "Onset envelope half-wave rectification fix — real-mix silence no longer produces beat-tracker-breaking negative dips"
affects: [04-analysis-ui-integration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Pure synchronous orchestration with no threads/MessageManager/GUI includes — Phase 4 wraps this in a ThreadPoolJob, this class itself stays headless and unit-testable"
    - "Region-relative BeatGrid/ChromaSequence times shifted by regionStartSeconds at the orchestrator boundary so AnalysisResult always carries ABSOLUTE source-file time"
    - "Cancel checked before/after every stage plus one in-CQT per-feed-chunk hook (the only stage long enough to need sub-stage cancellation)"
    - "Degenerate input at any stage (bpm 0 / no beats / empty chroma) returns a valid, never-thrown AnalysisResult with empty chords — matches loadAudioFileSync's error convention"
    - "Onset envelope is half-wave rectified AFTER HP-filter smoothing, BEFORE std-dev normalization — onset strength is inherently non-negative (matches librosa's onset_strength); synthetic click fixtures never exposed this because their envelopes are already positive-spiked"

key-files:
  created:
    - Tests/ClassicDspChordAnalyzerTests.cpp
  modified:
    - Source/Analysis/ClassicDspChordAnalyzer.h
    - Source/Analysis/ClassicDspChordAnalyzer.cpp
    - Source/Analysis/ConstantQAnalysis.h
    - Source/Analysis/ConstantQAnalysis.cpp
    - Source/Analysis/OnsetEnvelope.cpp

key-decisions:
  - "computeCqt gained an optional const std::function<bool()>& shouldCancel = {} parameter (backward-compatible default) checked per feed chunk, per the plan's explicit instruction, rather than adding cancellation only at the outer orchestrator level — needed because CQT is the longest single stage and outer-only checks would give multi-second cancel latency"
  - "Checkpoint-discovered real defect fixed live during Task 3 verification rather than deferred: half-wave rectify the onset envelope after smoothing, before normalization (commit d4eced3) — see Deviations"
  - "CHORDAI_DIAG stage-by-stage diagnostics (onset envelope min/max/meanAbs, beat count/BPM, first beat times) added to the hidden real-track harness as permanent debugging infrastructure, not removed after the fix, since synthetic fixtures cannot reproduce real-mix failure modes and this tool will be needed again"

patterns-established:
  - "Real-track listening checkpoints are load-bearing verification, not a formality — this plan's checkpoint caught a defect (BPM 0 / empty chords on real audio) that 58/58 synthetic-fixture tests across 5 prior plans never exposed"

requirements-completed: [ANL-01, ANL-02, ANL-03, ANL-06]

duration: ~23min (Tasks 1-2) + checkpoint verification/fix session
completed: 2026-07-13
---

# Phase 3 Plan 06: ClassicDspChordAnalyzer Orchestrator Summary

**Synchronous, headless, cancellable ClassicDspChordAnalyzer wires preprocessing -> beat tracking -> chroma -> key -> chord decoding behind the frozen ChordAnalyzer interface; performance budget enforced in-suite; human-verified on a real 75s trap beat after fixing a real onset-envelope defect the synthetic fixtures never caught.**

## Performance

- **Duration:** ~23 min for Tasks 1-2 (2026-07-12 23:28-23:51 UTC+1); Task 3 checkpoint verification + live defect fix on 2026-07-13 (session start 09:39 UTC+1)
- **Started:** 2026-07-12T23:28:51+01:00 (first RED commit)
- **Completed:** 2026-07-13T09:39:18+01:00 (fix commit, checkpoint approved)
- **Tasks:** 3 (2 auto/TDD + 1 checkpoint:human-verify)
- **Files modified:** 5 (ClassicDspChordAnalyzer.h/.cpp, ConstantQAnalysis.h/.cpp, OnsetEnvelope.cpp) + 1 created (Tests/ClassicDspChordAnalyzerTests.cpp)

## Accomplishments
- `ClassicDspChordAnalyzer::analyse` implemented as a pure synchronous sequence (no threads, no MessageManager, no GUI includes): clamp region -> preprocess (0-10%) -> onset+beat tracking (10-35%) -> CQT+tuning+percussion-suppression+chroma (35-70%) -> key detection (70-80%) -> chord decoding (80-100%), with cancellation checked before/after every stage plus a per-feed-chunk hook inside `computeCqt`
- Region-relative `BeatGrid`/`ChromaSequence` output shifted by `regionStartSeconds` so `AnalysisResult` carries absolute source-file time throughout (beat times, chord segment start/end)
- Degenerate inputs (empty buffer, sub-second buffer) return valid empty-but-safe results, never throw
- All 5 `ClassicDspChordAnalyzerTests.*` orchestration tests green: `HeadlessInvocation` (through `ChordAnalyzer&`, proving the swappable-interface contract), `Cancellation` (immediate and mid-run), `ProgressMonotonic` (0.0-1.0, non-decreasing, all 4 named stages present), `RegionRestriction` (absolute-time chord segments), `DegenerateInput`
- `PerformanceBudget` test: 180s synthetic 8-chord progression analyzed well under the 10s budget (Debug build), non-degenerate result required
- Hidden `[.realtrack]` harness (`CHORDAI_REAL_TRACK` env var) loads any real audio file via the production `loadAudioFileSync` path and prints BPM/key/timed chord list; `CHORDAI_DIAG` env var adds stage-by-stage diagnostics (onset envelope statistics, beat count/BPM)
- **Checkpoint-discovered defect fixed live:** first real-track run (user's own 75s trap beat, TOCK.mp3) returned BPM 0 and an empty chord list despite correct key detection (G# minor). Root-caused via the new diagnostics to the onset envelope's HP filter producing large negative dips at real-mix silence (-17.6 sigma measured vs +0.44 sigma onsets), which dominated std-dev normalization and tempo autocorrelation and drove the Ellis DP beat-tracker's cumulative score argmax to ~t=0 -> zero beats -> no beat grid -> no chord segments. Fixed by half-wave rectifying the onset envelope after smoothing, before normalization (commit `d4eced3`) -- matches librosa's `onset_strength` convention. Full suite stayed green (64/64 test cases, 11288 assertions) after the fix.
- Re-run on TOCK.mp3 post-fix: BPM 170.455 (trap double-time, plausible), key G# minor, 212 beats / 53 bars, a coherent repeating diatonic progression (G#m -> D#m -> E -> A#7 -> G#7 -> E -> B, all diatonic to G# minor), analysis time 15.5s (Debug build, ~75s source). User (the track's author) confirmed the output is approximately correct: "так десь такі" (roughly right).
- Full regression gate green: `ctest --test-dir build` 64/64 (100%), matching direct Catch2 binary invocation (11288 assertions in 64 test cases) -- no Phase 1/2/3 regressions.

## Task Commits

1. **Task 1: ClassicDspChordAnalyzer orchestration (TDD)** - `5d60b9d` (test, RED) -> `a05a115` (feat, GREEN)
2. **Task 2: Performance budget + real-track harness + full regression** - `8cca1a9` (test)
3. **Task 3: Real-track listening checkpoint** - no planned code changes; checkpoint-discovered defect fixed in `d4eced3` (fix)

**Plan metadata:** (this commit)

_Note: Task 1 was TDD RED->GREEN as planned. Task 2 added the performance test and hidden harness in a single commit (no separate RED needed, not TDD-tagged in the plan). Task 3's own scope was human verification only, but the verification surfaced a genuine pipeline defect invisible to every synthetic fixture across Plans 03-01 through 03-06 -- fixed under Deviation Rule 1 (bug) before recording checkpoint approval._

## Files Created/Modified
- `Source/Analysis/ClassicDspChordAnalyzer.h` - `analyse()` override declaration (already frozen shape from 03-01; no interface changes)
- `Source/Analysis/ClassicDspChordAnalyzer.cpp` - Real synchronous orchestration implementation (was a stub)
- `Source/Analysis/ConstantQAnalysis.h` - `computeCqt` gained optional `const std::function<bool()>& shouldCancel = {}` parameter (backward-compatible default)
- `Source/Analysis/ConstantQAnalysis.cpp` - Per-feed-chunk cancel check inside the CQT feed loop
- `Source/Analysis/OnsetEnvelope.cpp` - Half-wave rectification of the smoothed envelope before std-dev normalization (the defect fix)
- `Tests/ClassicDspChordAnalyzerTests.cpp` - `HeadlessInvocation`, `Cancellation`, `ProgressMonotonic`, `RegionRestriction`, `DegenerateInput`, `PerformanceBudget`, hidden `RealTrackHarness` (`[.realtrack]`) with `CHORDAI_DIAG` diagnostics

## Decisions Made
- `computeCqt`'s cancel hook checked per feed chunk (not just at the orchestrator level) so cancellation latency inside the longest pipeline stage stays sub-second, per the plan's explicit performance requirement.
- The onset-envelope fix was applied live during the checkpoint rather than deferred to a follow-up plan: it is a correctness bug (Rule 1) in code this exact plan owns end-to-end (the orchestrator is what first exercises the full pipeline on non-synthetic, real-silence-containing audio), and the phase's own success criteria require human-verified real-track plausibility -- shipping the checkpoint as "approved" against a broken BPM/chord result would misrepresent the phase's actual state.
- `CHORDAI_DIAG` diagnostics were kept as permanent (env-var-gated) harness infrastructure rather than stripped after the fix, since this is exactly the kind of real-mix failure mode synthetic fixtures cannot reproduce and future real-track regressions will need the same diagnostic path.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Onset envelope's negative HP-filter dips at real-mix silence broke beat tracking end-to-end**
- **Found during:** Task 3 (real-track listening checkpoint, first run on user's own trap beat TOCK.mp3)
- **Issue:** `analyse()` returned BPM 0 and an empty chord list on a real 75s track, while key detection (G# minor) was correct. Diagnosed via new `CHORDAI_DIAG` stage diagnostics: the harmonic-percussive-separation-derived onset envelope had huge negative dips during silence in a real mix (-17.6 sigma measured, vs +0.44 sigma for genuine onsets) that dominated the envelope's std-dev normalization and the tempo autocorrelation, driving the Ellis-2007 DP beat-tracker's cumulative score argmax to approximately t=0 -- producing zero beats, no beat grid, and consequently zero chord segments downstream. Every synthetic click fixture across Plans 03-01 through 03-06 has an always-positive onset envelope by construction, so this failure mode was structurally invisible to the full 58-test synthetic suite.
- **Fix:** Half-wave rectify the smoothed onset envelope (clamp negatives to 0) immediately after HP-filter smoothing and before std-dev normalization -- matches librosa's `onset_strength`, which clamps identically, since onset strength is inherently non-negative by definition.
- **Files modified:** `Source/Analysis/OnsetEnvelope.cpp` (fix), `Tests/ClassicDspChordAnalyzerTests.cpp` (added `CHORDAI_DIAG` diagnostics to the hidden harness)
- **Verification:** Full suite green after the fix -- `ctest --test-dir build` 64/64 (100%), 11288 assertions in 64 test cases via direct Catch2 invocation, zero regressions. Re-run of the hidden `[.realtrack]` harness on TOCK.mp3 post-fix produced BPM 170.455, key G# minor, 212 beats/53 bars, a coherent diatonic chord progression, 15.5s analysis time -- user (track author) confirmed approximate correctness.
- **Commit:** `d4eced3`

---

**Total deviations:** 1 auto-fixed (Rule 1 - real-mix beat-tracking bug, checkpoint-discovered)
**Impact on plan:** The fix is a small, surgical, one-line-of-logic change (clamp negatives) with a well-documented root cause and zero architectural impact -- no new files, no interface changes, no regressions. It was essential: without it, ANL-02/ANL-03's real-track plausibility criterion would have been false for any track containing genuine silence, which is nearly all real music. This is precisely the class of defect the phase's human-verification checkpoint exists to catch (03-VALIDATION.md's manual-only row).

## Issues Encountered
None beyond the documented deviation above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `ClassicDspChordAnalyzer` is a complete, tested, headless implementation of `ChordAnalyzer` -- Phase 4 needs only a `ThreadPoolJob`-backed `CancelToken` adapter and progress marshalling onto the message thread; no further orchestration logic required.
- `ChordAnalyzer.h` / `AnalysisResult.h` remain shape-identical to their 03-01 frozen definitions (`ConstantQAnalysis.h`'s `computeCqt` cancel parameter is backward-compatible via default argument; no other interface surface changed).
- Phase 3 complete: all four requirements (ANL-01, ANL-02, ANL-03, ANL-06) now fully evidenced end-to-end through one `analyse()` call, including human-verified real-track musical plausibility -- this was the final Phase 3 plan (Wave 4).
- The onset-envelope half-wave-rectification fix and its `CHORDAI_DIAG` diagnostic tooling are a durable asset for any future real-track debugging in Phase 4+ (e.g. if a different real track exposes a new degenerate case).
- Known v1 limitations carried forward per plan (not defects): root-position-only chord labels (no slash chords), maj/min/dom7 vocabulary only, 4/4-only bar grid, ~75-80% domain accuracy ceiling.

---
*Phase: 03-core-chord-detection-engine*
*Completed: 2026-07-13*

## Self-Check: PASSED

SUMMARY.md verified present on disk. All 4 referenced commits (5d60b9d, a05a115, 8cca1a9, d4eced3) verified present in git log. Full suite verified green: 64/64 (100%) via `ctest --test-dir build`, 11288 assertions in 64 test cases via direct Catch2 binary invocation, matching counts. REQUIREMENTS.md, ROADMAP.md, STATE.md updated accordingly.
