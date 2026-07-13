---
phase: 05-midi-conveyor-generation
plan: 02
subsystem: midi-generation
tags: [c++, juce, midi, music-theory, voice-leading, catch2, tdd]

# Dependency graph
requires:
  - phase: 05-midi-conveyor-generation (plan 01)
    provides: "Frozen generator signatures, header-only ChordToneMapper/VoiceLeadingEngine/Humanization helpers, Tests/MidiGenFixtures.h fixtures"
provides:
  - "generateAsIsRow: literal detected-progression row (root-position, exact detected quality, flat velocity)"
  - "generatePopTrapRow: dark C3 triad-only voicing with half-bar re-strike and remainder handling"
  - "generateRnbNeoSoulRow: maj9/min11/dom9 extended voicings with nearest-octave voice leading between consecutive chords"
  - "generateElectronicHouseRow: bright C5 tight-triad off-beat 16th stabs, segment-relative with span truncation"
  - "Tests/StyleVoicingTests.cpp: 12 TEST_CASEs covering per-style known-answer behavior, cross-style content distinctness, and progression-tracking proof"
affects: [05-04-orchestrator-wiring, 05-05-ui-panel]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Octave-preserving register clamp (repeated +/-12 semitone shift, not a truncating juce::jlimit) for seeding the first voiced chord in a register band -- a plain jlimit truncation collapses distinct pitch classes onto the register boundary, corrupting chord content"
    - "Running noteIndex counter (uint32_t, starts at 0 per generator call) threaded through every emitted NoteEvent for deterministicJitter, matching the plan's per-call determinism contract"
    - "Segment-relative rhythmic subdivision (chunk/span loops keyed off segment's own startBeatIndex, never the global bar grid) reused verbatim across Pop/Trap's 2-beat re-strike and House's 4-beat stab spans"

key-files:
  created: []
  modified:
    - Source/MidiGen/AsIsRowGenerator.cpp
    - Source/MidiGen/StyleVoicingGenerators.cpp
    - Tests/StyleVoicingTests.cpp

key-decisions:
  - "R&B seed-chord register clamp uses octave-shifting (while > high: -12; while < low: +12) instead of the plan action text's literal juce::jlimit truncation -- verified by hand-calculation that a truncating clamp collapses multiple extension tones onto the register boundary note (losing distinct pitch classes), which would fail the plan's own RnbExtensionsMatchQualityTable expected pitch-class set; subsequent (non-seed) chords still use the plan's exact nearestOctaveNote + truncating-jlimit shape unchanged, matching the documented Pitfall 3 voice-crossing limitation"

patterns-established:
  - "Pattern: any generator needing to place a chord tone inside a fixed register band without losing its pitch class identity should octave-shift, not jlimit-truncate; jlimit truncation is reserved for genuine out-of-range safety nets (subsequent-chord voice leading, Pitfall 3's accepted v1 limitation), not primary placement"

requirements-completed: [GEN-02]

# Metrics
duration: ~11min
completed: 2026-07-13
---

# Phase 5 Plan 02: Style Voicing Generators Summary

**Four deterministic chord-row generators (as-is reference, Pop/Trap dark triads, R&B/Neo-soul extended voicings with nearest-octave voice leading, Electronic/House off-beat stabs) implemented against Wave 1's frozen signatures, each provably driven by the detected progression rather than a preset lookup.**

## Performance

- **Duration:** ~11 min
- **Started:** 2026-07-13T12:46:08+01:00 (first RED commit)
- **Completed:** 2026-07-13T12:57:15+01:00 (final GREEN commit)
- **Tasks:** 3 (all TDD: RED + GREEN commits each)
- **Files modified:** 3 (AsIsRowGenerator.cpp, StyleVoicingGenerators.cpp, StyleVoicingTests.cpp)

## Accomplishments
- `generateAsIsRow`: literal root-position triad/7th per segment, full-segment sustain, flat 0.75 velocity (no jitter), NoChord skipped
- `generatePopTrapRow`: triad-only (7th dropped even from Dominant7), dark C3-anchored register, 2-beat re-strike with correct remainder handling on uneven segment lengths
- `generateRnbNeoSoulRow`: exact maj9/min11/dom9 extension tables per quality; first voiced chord seeded at C4 with an octave-preserving register clamp; every subsequent chord voice-leads via `nearestOctaveNote` against the previous chord's mean pitch; NoChord segments skip without disturbing the running voice-leading state
- `generateElectronicHouseRow`: bright C5-anchored tight triads, 4 off-beat 16th-note stabs per 4-beat span (0.5/1.5/2.5/3.5, length 0.25 beats), span offsets past segment end correctly dropped (verified on 3-beat and 6-beat edge-case fixtures)
- Cross-cutting proofs: three style rows are pairwise content-distinct on identical input (pitch-class multiset and onset-pattern assertions, never `.size()` comparisons); changing one segment's detected chord changes every style row's output for that segment while leaving unrelated segments byte-identical
- Full suite: 97/97 green throughout (includes plan 05-03's concurrently-landed BassLineGeneratorTests)

## Task Commits

Each task followed TDD (RED test commit, then GREEN implementation commit):

1. **Task 1: As-is row + Pop/Trap voicing** - `d381902` (test, RED) + `0af611a` (feat, GREEN)
2. **Task 2: R&B/Neo-soul voicing with voice leading** - `9f039be` (test, RED) + `11eb597` (feat, GREEN)
3. **Task 3: House stabs + cross-style distinctness + progression-tracking** - `9c4dac7` (test, RED) + `9c736b3` (feat, GREEN)

**Plan metadata:** (this commit, docs)

## Files Created/Modified
- `Source/MidiGen/AsIsRowGenerator.cpp` - `generateAsIsRow` body: literal detected progression per segment
- `Source/MidiGen/StyleVoicingGenerators.cpp` - `generatePopTrapRow`/`generateRnbNeoSoulRow`/`generateElectronicHouseRow` bodies, plus a local `clampToRegisterByOctave` helper for the R&B seed chord
- `Tests/StyleVoicingTests.cpp` - 12 TEST_CASEs: 4 As-is/Pop-Trap, 4 R&B, 4 House/cross-style/progression-tracking

## Decisions Made
- R&B seed-chord register placement uses an octave-shifting clamp rather than the plan action text's literal `juce::jlimit` truncation (see Deviations below) -- verified necessary by hand-computing the plan's own expected pitch-class test values
- Dedicated single-purpose test fixtures (B-minor high-root stress case, 6-beat span-truncation case) built inline in `StyleVoicingTests.cpp` rather than added to `Tests/MidiGenFixtures.h`, since they're single-test extreme-case probes, not general-purpose fixtures other Wave 2/3/4 tests would reuse

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] R&B seed-chord register clamp corrected from truncating jlimit to octave-preserving clamp**
- **Found during:** Task 2 (R&B/Neo-soul voicing implementation)
- **Issue:** The plan's action text specifies clamping the seed chord's extension tones "into [kRnbRegisterLow, kRnbRegisterHigh] via juce::jlimit." Hand-calculating this literally against the plan's own `RnbExtensionsMatchQualityTable` expected values (Am min11 seeded at root 69, intervals `{0,3,7,10,14,17}` producing raw pitches `{69,72,76,79,83,86}`) shows a truncating `jlimit(48,72,...)` collapses four of the six tones onto the boundary value 72 (pitch classes `{9,0,0,0,0,0}`), which does not match the plan's own specified expected pitch-class set `{9,0,4,7,11,2}`.
- **Fix:** Implemented a local `clampToRegisterByOctave` helper that repeatedly shifts by +/-12 semitones until the pitch lands inside the register band (with a final `jlimit` as a no-op safety net), preserving pitch class identity. The register band is exactly 24 semitones (2 octaves) wide, so this always terminates without ever needing to fall back to truncation for the seed chord's tone spread. Verified by hand-calculation to reproduce exactly `{69,72,64,67,71,62}` -> pitch classes `{9,0,4,7,11,2}`, matching the plan's expected values precisely.
- **Files modified:** Source/MidiGen/StyleVoicingGenerators.cpp
- **Verification:** `StyleVoicingTests.RnbExtensionsMatchQualityTable` and `StyleVoicingTests.RnbRegisterClampHolds` both pass; subsequent (non-seed) chords are unaffected and still use the plan's exact `nearestOctaveNote` shape (including its own truncating-jlimit safety net, matching the documented Pitfall 3 voice-crossing limitation for that path only)
- **Committed in:** `11eb597` (Task 2 GREEN commit)

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Necessary correction to make the implementation match the plan's own specified test expectations; no scope change, no new files, no architectural change.

## Issues Encountered
- Cross-agent git index race (parallel plan 05-03 sharing this repo): after Task 2's implementation, a `git commit` without path restriction (relying only on prior `git add`) swept in plan 05-03's already-staged `05-03-SUMMARY.md` alongside this plan's `StyleVoicingGenerators.cpp` change (commit `11eb597`). No content was lost or corrupted -- the swept-in file's content was already finalized by the 05-03 agent's own prior commit; this only affected commit attribution (the file ended up committed under this plan's commit message instead of 05-03's own "docs" commit). Mitigated for all subsequent commits in this plan by using `git commit -m "..." -- <specific-path>` (path-scoped commit) instead of a bare `git commit` after `git add`, which restricts the commit to exactly the intended files regardless of what else is staged in the shared index. Same root cause as the documented 03-04 lesson (shared git index across concurrently-executing agents); no functional impact, full suite stayed green throughout.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All four voiced-row generators (`generateAsIsRow`, `generatePopTrapRow`, `generateRnbNeoSoulRow`, `generateElectronicHouseRow`) are implemented, tested, and deterministic -- ready for `MidiRowBuilder::generateAllRows` (05-04) to wire them together with the bass row (05-03) into the 5-row orchestrator
- GEN-02 fully evidenced: per-style register/rhythm/extension behavior matches 05-RESEARCH.md exactly, cross-style content distinctness proven via pitch-class/onset/register assertions (not size comparisons), and progression-tracking proven by mutating a fixture chord and asserting every style row's output changes accordingly
- No blockers identified

---
*Phase: 05-midi-conveyor-generation*
*Completed: 2026-07-13*

## Self-Check: PASSED

All 3 modified source/test files verified present on disk. All 6 task commit hashes (d381902, 0af611a, 9f039be, 11eb597, 9c4dac7, 9c736b3) verified present in git log. Full suite verified 97/97 green (`ctest --test-dir build --output-on-failure`), including plan 05-03's concurrently-landed tests.
