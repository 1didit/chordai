---
phase: 03-core-chord-detection-engine
plan: 04
subsystem: audio-dsp
tags: [key-detection, krumhansl-kessler, pearson-correlation, chroma-accumulation, catch2, tdd]

# Dependency graph
requires:
  - phase: 03-core-chord-detection-engine (plan 02)
    provides: "Working tuning-corrected, percussion-robust ChromaSequence (ChromaExtractor::extractChroma) feeding accumulateChroma"
provides:
  - "KeyDetector::accumulateChroma -- energy-weighted (harmonicPreNormL2), L2-normalized single global chroma vector over the whole analyzed region"
  - "KeyDetector::detectKey -- 24-candidate (12 tonics x major/minor) Krumhansl-Kessler Pearson correlation, confidence = normalized best-vs-second margin"
  - "ANL-01 fully evidenced end-to-end: synthetic audio in -> correct KeyResult out, including relative-major/minor disambiguation via the dominant's leading tone"
affects: [03-06-classic-dsp-orchestrator, phase-4-analysis-ui]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Krumhansl-Kessler 24-profile table embedded as anonymous-namespace static constexpr arrays directly in the .cpp (not the header) -- the profile values are the algorithm itself per research; verified digit-for-digit against 03-RESEARCH.md"
    - "Candidate rotation via relative-degree lookup (rotated[p] = profile[(p - tonic + 12) % 12]) rather than array std::rotate, avoiding an extra copy/rotate step per candidate"
    - "Confidence-as-margin pattern: sort all N candidates by score descending, confidence = clamp((best - second) / (|best| + epsilon), 0, 1) -- reusable for any 'is this classification ambiguous' surface (chord decode's Viterbi could reuse the same idea later)"

key-files:
  created: []
  modified:
    - Source/Analysis/KeyDetector.h (doc comment only, contract unchanged)
    - Source/Analysis/KeyDetector.cpp
    - Tests/KeyDetectorTests.cpp

key-decisions:
  - "accumulateChroma implemented alongside detectKey in the Task 1 commit rather than deferred to Task 2 -- both were simple companion stubs in the same already-open file, and splitting them across two GREEN commits would have left a half-stubbed file in between for no benefit. Task 2 then added integration tests that validate (rather than newly drive) accumulateChroma's behavior."
  - "Krumhansl-Kessler rotation direction: candidate profile value at absolute pitch class p is profile[(p - tonic + 12) % 12] (relative scale degree lookup), not a literal array rotate -- verified via the TransposedProfileInvariance test (a C-major vector shifted +2 semitones is detected as D major)."

requirements-completed: [ANL-01]

# Metrics
duration: ~10min
completed: 2026-07-12
---

# Phase 3 Plan 04: Key Detection (Krumhansl-Kessler 24-Profile Pearson Correlation) Summary

**KeyDetector accumulates a tuning-corrected, energy-weighted global chroma vector and correlates it against all 24 rotated Krumhansl-Kessler major/minor profiles, correctly resolving the C-major/A-minor relative-key ambiguity when a dominant chord's leading tone is present.**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-07-12T22:55:05+01:00 (first RED commit)
- **Completed:** 2026-07-12T23:00:02+01:00 (last test commit)
- **Tasks:** 2 (both TDD RED -> GREEN, though Task 2 landed already-GREEN, see Decisions)
- **Files modified:** 3 (KeyDetector.h/.cpp + KeyDetectorTests.cpp)

## Accomplishments
- `KeyDetector::accumulateChroma` sums `harmonic[i] * harmonicPreNormL2` across every frame (frames with more energy count more, silent frames contribute nothing) and L2-normalizes the result, returning an all-zero vector for a fully-silent sequence
- `KeyDetector::detectKey` embeds the Krumhansl & Kessler (1982) major/minor 24-profile tables verbatim, rotates each to all 12 tonics, computes mean-subtracted (Pearson) correlation with a zero-variance guard, and picks the best of 24 candidates with `confidence` as the clamped normalized margin to the second-best
- Hand-built pure-vector tests (`PureCMajorVector`, `PureAMinorVector`, `TransposedProfileInvariance`, `ConfidenceMargins`) prove rotation correctness across all 12 tonics and confidence behavior on both a strongly-keyed and a perfectly ambiguous (uniform) vector
- Full-chain audio-integration tests (`CMajorFixture`, `RelativeMinor`, `DetunedKeyStillDetected`) exercise the real `renderChordProgression -> preprocessForAnalysis -> computeCqt -> suppressPercussion -> extractChroma -> accumulateChroma -> detectKey` pipeline, including the relative-minor case with the E-major dominant chord and a -30 cent detuned fixture with `estimateTuningCents` correction applied

## Task Commits

Each task was committed atomically (TDD RED -> GREEN):

1. **Task 1: KeyDetector core (K-K profile Pearson correlation)** - `9b5f16f` (test, RED) -> `fd68f4a` (feat, GREEN)
2. **Task 2: Audio-integration key tests through the real chroma path** - `ac282f9` (test; already GREEN against Task 1's `accumulateChroma`, see Decisions)

**Plan metadata:** (this commit)

## Files Created/Modified
- `Source/Analysis/KeyDetector.h` - Contract unchanged (skeleton frozen since 03-01); no signature edits needed
- `Source/Analysis/KeyDetector.cpp` - Real `accumulateChroma` (energy-weighted sum + L2 normalize) and `detectKey` (24-candidate K-K Pearson correlation, confidence margin)
- `Tests/KeyDetectorTests.cpp` - `PureCMajorVector`/`PureAMinorVector`/`TransposedProfileInvariance`/`ConfidenceMargins` (Task 1, pure-vector) + `CMajorFixture`/`RelativeMinor`/`DetunedKeyStillDetected` (Task 2, full audio-chain integration)

## Decisions Made
- Implemented `accumulateChroma` in the same Task 1 GREEN commit as `detectKey` (both were trivial companion stubs in `KeyDetector.cpp`) rather than strictly deferring it to Task 2 as the plan's task split implies. Task 2's three integration tests therefore passed on first build (already GREEN) rather than driving new implementation -- they still serve their purpose as end-to-end regression coverage of the full `computeCqt -> suppressPercussion -> extractChroma -> accumulateChroma -> detectKey` chain, including the relative-minor disambiguation case the plan calls out as the key correctness bar.
- Profile rotation uses a relative-degree lookup (`rotated[p] = profile[(p - tonic + 12) % 12]`), verified correct via `TransposedProfileInvariance` (a C-major-shaped vector rotated +2 semitones is detected as D major, proving the rotation direction and all-12-tonics reachability).

## Deviations from Plan

None - plan executed exactly as written (task-commit split adjusted per the Decision above, not a deviation-rule fix).

## Issues Encountered
- **Cross-agent git index race (not a plan deviation):** Plan 03-05 was executing concurrently in a separate agent against a disjoint file set (`ChordTemplates.h`, `ChordDecoder.h/.cpp`, `Tests/ChordDecoderTests.cpp`) sharing this repo's single git index. Between staging `Source/Analysis/KeyDetector.cpp` and running `git commit` for Task 1's GREEN commit, the other agent's own staged (but not yet committed) `ChordDecoder.cpp` implementation was present in the shared index and got swept into commit `fd68f4a` alongside this plan's `KeyDetector.cpp` change. No code was lost or corrupted (full suite stayed green throughout, including `ChordDecoderTests.*`), and this plan's own file set (`KeyDetector.h/.cpp`, `Tests/KeyDetectorTests.cpp`) is otherwise fully isolated from 03-05's -- but commit `fd68f4a`'s diff is not perfectly atomic to this plan alone. Mitigated for all subsequent commits in this plan by checking `git status --short` immediately before every `git add`/`git commit` to confirm only this plan's files were staged.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `KeyResult` (via `accumulateChroma` + `detectKey`) is now fully wired and ready for `ClassicDspChordAnalyzer` (03-06) to populate `AnalysisResult::key`
- ANL-01 fully evidenced end-to-end and marked complete in REQUIREMENTS.md
- Full suite green: 53/53 tests (`ctest --test-dir build`), including this plan's 7 `KeyDetectorTests.*` cases and the concurrently-landed `ChordDecoderTests.*` from Plan 03-05 (Wave 3, disjoint file sets)
- CMakeLists.txt untouched, matching the plan's constraint

---
*Phase: 03-core-chord-detection-engine*
*Completed: 2026-07-12*

## Self-Check: PASSED

All key files verified on disk (KeyDetector.h, KeyDetector.cpp, KeyDetectorTests.cpp, this SUMMARY.md). All 3 task commits (9b5f16f, fd68f4a, ac282f9) verified present in git log.
