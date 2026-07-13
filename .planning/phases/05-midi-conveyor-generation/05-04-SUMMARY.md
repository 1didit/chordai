---
phase: 05-midi-conveyor-generation
plan: 04
subsystem: midi-generation
tags: [c++, juce, midi, atomics, threading, catch2, tdd]

# Dependency graph
requires:
  - phase: 05-midi-conveyor-generation (plan 01)
    provides: "Frozen generateAllRows/MidiSetRow/RowStyle signatures, generation-guarded triggerAnalysis wiring pattern from Phase 4"
  - phase: 05-midi-conveyor-generation (plan 02)
    provides: "generateAsIsRow, generatePopTrapRow, generateRnbNeoSoulRow, generateElectronicHouseRow"
  - phase: 05-midi-conveyor-generation (plan 03)
    provides: "generateBassRow (TrapSustain/RnbRootFifth/HouseFourOnFloor)"
provides:
  - "generateAllRows orchestrator: fans one AnalysisResult out into the fixed 5-row set (as-is, pop-trap, rnb-neosoul, house, bass), byte-deterministic, <1ms measured"
  - "ChordAIAudioProcessor::getMidiSetRows() atomic-load accessor, published in the SAME analysisBroadcaster message as getAnalysisResult() (never staggered)"
  - "Region-change regeneration for free, reusing 04-01's generation-guarded triggerAnalysis path"
  - "Tests/MidiRowBuilderTests.cpp: 7 TEST_CASEs (5-row contract, as-is fidelity, NoChord silence, determinism, empty-result safety, performance budget)"
  - "Tests/PluginProcessorMidiGenTests.cpp: 3 integration TEST_CASEs (synchronous same-broadcast publication, region-change regeneration, fresh-load clear)"
affects: [05-05-ui-panel, 06-preview-export]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "generateAllRows(result, settings) is the single MidiGen public entry point PluginProcessor calls -- wraps each Wave 2/3 generator's std::vector<NoteEvent> in a MidiSetRow with fixed id/label/style, in fixed order"
    - "Row publication reuses analysisResult's exact atomic_load/atomic_store shared_ptr idiom (Apple libc++ has no complete std::atomic<shared_ptr>) -- midiSetRows generated and stored BEFORE analysisBroadcaster.sendChangeMessage(), making 'same broadcast' an ordering guarantee rather than a race"
    - "GEN-04 region-change regeneration required zero new wiring -- it is 04-01's generation-guarded cancel-and-restart trigger path plus this plan's synchronous row generation inside the existing onDone callback"

key-files:
  created: []
  modified:
    - Source/MidiGen/MidiRowBuilder.cpp
    - Tests/MidiRowBuilderTests.cpp
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - Tests/PluginProcessorMidiGenTests.cpp

key-decisions:
  - "Shipped bass row uses BassRhythm::TrapSustain as the v1 default (Claude's-discretion call documented in the plan): sparse sustained root is the most universally usable bass for piano-roll drag-out and matches SyntheticFixtures' 'bass = root' convention. RnbRootFifth/HouseFourOnFloor stay fully implemented/tested from 05-03 for a future per-row style control (05-RESEARCH.md Open Question 1)."
  - "GenerationSettings threaded through generateAllRows unused (v1 no-op) so a future settings-changed callback can call the same entry point without a shape change."

patterns-established:
  - "Pattern: any future PluginProcessor-published value that must never be observed out of sync with analysisResult should mirror this plan's ordering -- atomic_store the new value BEFORE analysisBroadcaster.sendChangeMessage(), never after."

requirements-completed: [GEN-01, GEN-04]

# Metrics
duration: ~6min
completed: 2026-07-13
---

# Phase 5 Plan 04: Orchestrator + Processor Wiring Summary

**generateAllRows fans one AnalysisResult into the fixed 5-row set and publishes it atomically with the chord timeline via PluginProcessor's existing analysisBroadcaster, with region-change regeneration reused for free from Phase 4's generation-guarded trigger path.**

## Performance

- **Duration:** ~6 min (~4.5 min task work + wave-gate verification)
- **Started:** 2026-07-13T13:06:53+01:00 (first RED commit)
- **Completed:** 2026-07-13T13:11:13+01:00 (final GREEN commit)
- **Tasks:** 3 (Tasks 1-2 TDD: RED + GREEN commits each; Task 3 verification-only, no commit)
- **Files modified:** 5 (MidiRowBuilder.cpp, MidiRowBuilderTests.cpp, PluginProcessor.h, PluginProcessor.cpp, PluginProcessorMidiGenTests.cpp)

## Accomplishments
- `generateAllRows`: returns exactly 5 `MidiSetRow`s in fixed order (as-is, pop-trap, rnb-neosoul, house, bass) with correct id/label/style, delegating to the Wave 2/3 generators; proven byte-identical across two calls on the same fixture, proven empty-but-non-crashing on an empty/all-NoChord `AnalysisResult`, and proven to complete in under 1ms (min-of-5 runs) on a ~150-segment real-track-scale fixture
- `PluginProcessor::getMidiSetRows()`: atomic-load accessor (`midiSetRows` member, same `atomic_load`/`atomic_store` discipline as `analysisResult`) published inside `triggerAnalysis()`'s `onDone` callback, generated and stored immediately after `analysisResult` and strictly before `analysisBroadcaster.sendChangeMessage()` -- proven via a recording `ChangeListener` that the FIRST callback observing a non-null result already has non-null rows of size 5, never staggered across two messages
- Region change (`setSelectedRegion`) regenerates rows deterministically: `RowsRegenerateOnRegionChange` proves the new rows object differs from the old one, has size 5, and deep-equals `generateAllRows(*newResult)` for the new region's own progression
- Fresh file load clears `midiSetRows` to nullptr alongside the pre-existing `analysisResult` clear, before broadcasting -- `FreshLoadClearsRows` proves the UI window between a new `loadedAudio` pointer and analysis completion never observes stale rows
- Full suite grew from 97 (post-05-02/05-03) to 106, all green; pluginval strictness 5 green for both VST3 and AU (Editor Automation pass exercised editor open/close with rows now published, no lifecycle/threading regressions)

## Task Commits

Tasks 1-2 followed TDD (RED test commit, then GREEN implementation commit); Task 3 was verification-only.

1. **Task 1: generateAllRows orchestrator** - `d1d575b` (test, RED) + `ec773fc` (feat, GREEN)
2. **Task 2: Processor wiring** - `6de3934` (test, RED) + `25ffaef` (feat, GREEN)
3. **Task 3: Wave gate (full suite + pluginval strictness 5, VST3+AU)** - verification only, no commit

**Plan metadata:** (this commit, docs)

## Files Created/Modified
- `Source/MidiGen/MidiRowBuilder.cpp` - `generateAllRows` body: wires the 5 generators into the fixed row order, with the bass-rhythm-default rationale documented as a comment at the call site
- `Tests/MidiRowBuilderTests.cpp` - 7 TEST_CASEs: 5-row contract, as-is fidelity, NoChord silence, byte-identical determinism, empty-result safety, sub-1ms performance budget (plus the pre-existing fixture self-consistency test)
- `Source/PluginProcessor.h` - `#include "MidiGen/MidiSetRow.h"`, `getMidiSetRows()` public accessor, `midiSetRows` private member with the same atomic-publication doc comment as `analysisResult`
- `Source/PluginProcessor.cpp` - `#include "MidiGen/MidiRowBuilder.h"`; row generation + atomic_store inserted in `triggerAnalysis()`'s `onDone`, before `sendChangeMessage()`; `midiSetRows` cleared alongside `analysisResult` in `loadAudioFile()`'s fresh-load callback
- `Tests/PluginProcessorMidiGenTests.cpp` - 3 integration TEST_CASEs: synchronous same-broadcast publication (via a recording `ChangeListener`), region-change regeneration, fresh-load clearing; `writeToneWavFixture`/`pumpUntil` copied verbatim from `Tests/AnalysisPipelineTests.cpp` per that file's own file-local-by-design convention

## Decisions Made
- Shipped bass row uses `BassRhythm::TrapSustain` (documented Claude's-discretion default from the plan) -- sparse sustained root suits piano-roll drag-out best; the other two rhythms stay implemented/tested for a future style control
- `GenerationSettings` threaded through unused (v1 no-op forward-compat plumbing, per 05-RESEARCH.md Open Question 1)

## Deviations from Plan

None - plan executed exactly as written. The one Catch2 chained-comparison syntax fix (`CHECK_FALSE ((a >= x && a < y))` needing extra parentheses) is the same known static_assert quirk already documented in 05-03-SUMMARY.md's "Issues Encountered" -- not a deviation from plan scope, just required Catch2 syntax to get the intended RED state compiling.

## Issues Encountered
- Same Catch2 `static_assert` on unparenthesized chained comparisons inside `CHECK_FALSE` hit in 05-03 recurred in `MidiRowBuilderTests.NoChordSegmentEmitsNoNotes` (`note.startBeats >= 4.0 && note.startBeats < 8.0`); fixed by wrapping the expression in parentheses before the RED build. No functional impact.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- GEN-01 fully evidenced: one analysis pass produces all 5 rows, published atomically (same broadcast, never staggered) with the chord timeline's own data source
- GEN-04 fully evidenced: region change through the processor's public API regenerates rows deterministically via the existing generation-guarded path; same input always produces byte-identical rows
- Full suite (106/106) and pluginval strictness 5 (VST3 + AU) green -- ready for 05-05's UI panel to consume `getMidiSetRows()` and render the 5 rows
- No blockers identified

---
*Phase: 05-midi-conveyor-generation*
*Completed: 2026-07-13*

## Self-Check: PASSED

All 5 modified source/test files verified present on disk. All 4 task commit hashes (d1d575b, ec773fc, 6de3934, 25ffaef) verified present in `git log`. Full suite verified 106/106 green (`ctest --test-dir build --output-on-failure`); pluginval strictness 5 verified SUCCESS for both VST3 and AU.
