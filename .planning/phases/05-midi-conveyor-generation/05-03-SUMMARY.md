---
phase: 05-midi-conveyor-generation
plan: 03
subsystem: midi-generation
tags: [c++, juce, midi, music-theory, catch2, tdd]

# Dependency graph
requires:
  - phase: 05-midi-conveyor-generation
    provides: "Plan 05-01's frozen generateBassRow signature, ChordToneMapper.h rootMidiNote/kAnchorBass, Humanization.h deterministicJitter/kSeedBass, Tests/MidiGenFixtures.h fixtures"
provides:
  - "generateBassRow full implementation: detected chord root at C2 (kAnchorBass) with three style-appropriate rhythms (TrapSustain, RnbRootFifth, HouseFourOnFloor), pure and deterministic"
  - "Tests/BassLineGeneratorTests.cpp: 6 TEST_CASEs covering root-following across all rhythms, per-rhythm exact timing/pitch grids, NoChord silence, and byte-deterministic velocity"
affects: [05-04-orchestrator-wiring, 05-05-ui-panel]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Bass rhythm branches share one running noteIndex/jitter counter across the whole generateBassRow call, matching StyleVoicingGenerators' established humanization pattern from 05-02"
    - "RnbRootFifth walks the segment in 4-beat spans (root/fifth), falling back to a single sustained root note for any remainder shorter than 4 beats -- same 'shorter segments simplify to sustain' rule as TrapSustain's whole-segment case"

key-files:
  created: []
  modified:
    - Source/MidiGen/BassLineGenerator.cpp
    - Tests/BassLineGeneratorTests.cpp

key-decisions:
  - "HouseFourOnFloor bass note length fixed at 0.5 beats (a local kHouseBassNoteLengthBeats constant), independent of ChordToneMapper.h's kHouseStabLengthBeats (0.25, used by the off-beat stab row) -- same house rhythm family, deliberately different note length per instrument role"
  - "RnbRootFifth's fifth uses juce::jlimit(0,127, root+7), not a %12-wrapped pitch class re-anchored to the register -- keeps the fifth strictly above the root in absolute pitch, matching the research spec's 'first half root, second half fifth' walking-bass shape"

patterns-established:
  - "Pattern: any future per-segment rhythm generator in Source/MidiGen/ that needs a 'full spans then remainder' loop (4-beat spans here) uses a while(spanOffset + spanLength <= segmentLength) loop followed by a single remainder-sustain note, not a modulo/rounding computation"

requirements-completed: [GEN-03]

# Metrics
duration: ~5min
completed: 2026-07-13
---

# Phase 5 Plan 03: Bass Line Generator Summary

**generateBassRow implemented: root-following bass at C2 with three style rhythms (trap sustain, R&B root-fifth walk, house four-on-the-floor), TDD RED/GREEN across two tasks, 6 new TEST_CASEs, full suite 89/89 green.**

## Performance

- **Duration:** ~5 min
- **Tasks:** 2 (both TDD: RED + GREEN commits each)
- **Files modified:** 2 (Source/MidiGen/BassLineGenerator.cpp, Tests/BassLineGeneratorTests.cpp)

## Accomplishments
- `generateBassRow` shared skeleton: iterates `result.chords`, skips `NoChord` segments (Pitfall 1), derives `root = rootMidiNote(chord.pitchClass, kAnchorBass)`, branches on `BassRhythm`
- TrapSustain: one full-segment-length sustained root note in the C2 register (36-47 for this fixture set)
- RnbRootFifth: walks each full 4-beat span as root (2 beats) then fifth (root+7 semitones, 2 beats); any remainder shorter than 4 beats (or a whole segment shorter than 4 beats) sustains root only; verified against a 4-beat segment, 2-beat/3-beat short segments, and a locally-built 8-beat segment (2 repeats of the pattern)
- HouseFourOnFloor: one 0.5-beat root note per whole beat, truncating cleanly on short segments (verified 4-beat -> 4 notes, 3-beat -> 3 notes)
- Deterministic per-note velocity jitter shared across all three rhythms via one running `noteIndex` counter and `kSeedBass`/`kJitterBass` -- two identical `generateBassRow` calls produce byte-identical `NoteEvent` sequences
- `RootPitchClassMatchesDetectedChord` proven for all three rhythms: the note at each segment's start always matches the detected chord's pitch class; TrapSustain/HouseFourOnFloor additionally require every emitted note to be root-class (RnbRootFifth's fifth note is intentionally a different pitch class)
- Full suite grew from 79 (post-05-01) to 89, all green throughout, including the concurrently-executing 05-02 plan's StyleVoicingTests

## Task Commits

Each task followed TDD (RED then GREEN):

1. **Task 1: Root derivation + TrapSustain rhythm** - `07205a8` (test, RED) + `abc7c08` (feat, GREEN)
2. **Task 2: RnbRootFifth walk + HouseFourOnFloor rhythms** - `a5bcd14` (test, RED) + `801814a` (feat, GREEN)

**Plan metadata:** (this commit, docs)

## Files Created/Modified
- `Source/MidiGen/BassLineGenerator.cpp` - Full `generateBassRow` body: shared skeleton + three rhythm branches (TrapSustain, RnbRootFifth, HouseFourOnFloor)
- `Tests/BassLineGeneratorTests.cpp` - 6 TEST_CASEs: `RootPitchClassMatchesDetectedChord` (3 SECTIONs, one per rhythm), `TrapBassSustainsFullSegment`, `NoChordEmitsNoBassNotes`, `RnbBassRootFifthWalk` (3 SECTIONs), `HouseBassFourOnTheFloor` (2 SECTIONs), `BassVelocityDeterministicAndBounded`

## Decisions Made
- HouseFourOnFloor's bass note length is a local `kHouseBassNoteLengthBeats = 0.5` constant, kept distinct from `ChordToneMapper.h`'s `kHouseStabLengthBeats` (0.25, the off-beat stab row's note length) -- same style family, different instrument role, no accidental coupling between the two rows' timing
- RnbRootFifth's fifth is computed as `juce::jlimit(0, 127, root + 7)`, not a pitch-class-wrapped-then-reanchored value, to guarantee the fifth is always audibly above the root (matches the research spec's walking-bass shape, avoids the "fifth below root" bug called out in the plan)

## Deviations from Plan

None - plan executed exactly as written. `RootPitchClassMatchesDetectedChord` was written with all three SECTIONs from the start (TrapSustain enabled in Task 1, RnbRootFifth/HouseFourOnFloor added in Task 2) per the plan's own instruction to "extend to all three rhythms in Task 2."

## Issues Encountered
- Initial RED-state build failed on a Catch2 static_assert (`CHECK_FALSE (a >= x && a < y)` -- chained comparisons inside an unparenthesized assertion aren't supported by Catch2's expression decomposer). Fixed by wrapping the expression in parentheses (`CHECK_FALSE ((a >= x && a < y))`) before the RED run; not a deviation from plan scope, just a Catch2 syntax fix required to get the intended RED state compiling.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- GEN-03 fully evidenced at unit level: bass follows detected roots (C2 register) with three style-appropriate rhythms, exact timing per rhythm, silent on NoChord, byte-deterministic
- `generateBassRow` ready for 05-04's orchestrator (`generateAllRows`) to wire in alongside 05-02's style-voicing generators
- No blockers identified; full suite (89/89) green including the concurrently-executing 05-02 plan's tests, confirming disjoint-file parallel execution left no conflicts

---
*Phase: 05-midi-conveyor-generation*
*Completed: 2026-07-13*

## Self-Check: PASSED

Verified `Source/MidiGen/BassLineGenerator.cpp` and `Tests/BassLineGeneratorTests.cpp` present on disk with expected content. All 4 task commit hashes (07205a8, abc7c08, a5bcd14, 801814a) verified present in `git log`. Targeted suite (`ChordAITests.(BassLineGeneratorTests|ChordToneMapperTests)`) 12/12 green; full suite `ctest --test-dir build` 89/89 green.
