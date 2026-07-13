---
phase: 05-midi-conveyor-generation
plan: 01
subsystem: midi-generation
tags: [c++, juce, midi, music-theory, catch2, tdd]

# Dependency graph
requires:
  - phase: 03-detection-engine
    provides: "Frozen AnalysisResult.h contract (ChordSegment/ChordSymbol/ChordQuality, beat-indexed) and ChordTemplates.h's interval convention"
provides:
  - "Source/MidiGen/ pure-C++ module skeleton: NoteEvent, MidiSetRow/RowStyle, GenerationSettings data model"
  - "Frozen generator signatures for all 5 rows (generateAsIsRow, generatePopTrapRow/generateRnbNeoSoulRow/generateElectronicHouseRow, generateBassRow, generateAllRows) -- Wave 2/3 plans implement bodies only"
  - "Real, tested header-only music-math helpers: triadIntervals/rnbExtensionIntervals/intervalsToMidiNotes/rootMidiNote (ChordToneMapper.h), nearestOctaveNote (VoiceLeadingEngine.h), deterministicJitter + per-style seed/velocity constants (Humanization.h)"
  - "Tests/MidiGenFixtures.h: 4 hand-built struct-literal AnalysisResult fixtures (4-chord, NoChord, short-segment, ~150-segment real-track-scale), self-consistency tested"
  - "CHORDAI_MIDIGEN_SOURCES CMake wiring into both ChordAI and ChordAITests targets -- frozen for the rest of the phase until plan 05-05"
affects: [05-02-style-voicing, 05-03-bass-line, 05-04-orchestrator-wiring, 05-05-ui-panel]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Source/MidiGen/ pure-C++ module: zero GUI includes, only Source/Analysis/AnalysisResult.h allowed from Analysis (folder-boundary rule)"
    - "Beat-domain NoteEvent (startBeats/lengthBeats) instead of seconds-domain -- bar-aligned by construction, no float-rounding re-derivation from BPM"
    - "Deterministic hash-based humanization (Knuth multiplicative hash) instead of juce::Random -- required for GEN-04's regenerate-same-input-same-output guarantee"
    - "Header-only inline music-math helpers (ChordToneMapper.h/VoiceLeadingEngine.h/Humanization.h), matching this repo's existing ChordTemplates.h/WaveformMath.h convention"
    - "Frozen generator function signatures + CMake wiring landed in one Wave-1 plan so Wave 2/3 plans implement bodies only, zero further CMakeLists.txt edits (Phase 3's 03-01 skeleton-freeze precedent)"

key-files:
  created:
    - Source/MidiGen/NoteEvent.h
    - Source/MidiGen/MidiSetRow.h
    - Source/MidiGen/GenerationSettings.h
    - Source/MidiGen/ChordToneMapper.h
    - Source/MidiGen/VoiceLeadingEngine.h
    - Source/MidiGen/Humanization.h
    - Source/MidiGen/AsIsRowGenerator.h/.cpp
    - Source/MidiGen/StyleVoicingGenerators.h/.cpp
    - Source/MidiGen/BassLineGenerator.h/.cpp
    - Source/MidiGen/MidiRowBuilder.h/.cpp
    - Tests/MidiGenFixtures.h
    - Tests/ChordToneMapperTests.cpp
    - Tests/StyleVoicingTests.cpp (scaffold)
    - Tests/BassLineGeneratorTests.cpp (scaffold)
    - Tests/MidiRowBuilderTests.cpp
    - Tests/PluginProcessorMidiGenTests.cpp (scaffold)
  modified:
    - CMakeLists.txt

key-decisions:
  - "NoChord returns {} explicitly in both triadIntervals and rnbExtensionIntervals (not a Major fallthrough) -- Pitfall 1 guard, verified by dedicated test assertions"
  - "Register-anchor constants (kAnchorAsIs=60, kAnchorPopTrap=48, kAnchorRnbSeed=60, kAnchorHouse=72, kAnchorBass=36) and house-stab timing constants placed in ChordToneMapper.h rather than a separate constants file -- co-located with the interval tables Wave 2 will consume alongside them"
  - "MidiGenFixtures.h uses small internal helper functions (makeBeatGrid/makeBarStarts/makeSegment) rather than fully inline struct literals per fixture, to guarantee the seconds/beat-index derivation rule (index * 60/bpm) can never drift between fixtures -- still zero audio rendering, just DRY struct construction"

patterns-established:
  - "Pattern: any future MidiGen header-only helper follows ChordToneMapper.h/VoiceLeadingEngine.h/Humanization.h's inline-function style, not a .cpp"
  - "Pattern: MidiGenFixtures.h factory functions are the canonical known-answer inputs for all Wave 2/3/4 generator tests -- no new fixture files needed downstream"

requirements-completed: [GEN-01]

# Metrics
duration: 6min
completed: 2026-07-13
---

# Phase 5 Plan 01: MidiGen Module Foundation Summary

**Established the entire Source/MidiGen/ pure-C++ module surface -- beat-domain NoteEvent/MidiSetRow data model, 5 frozen generator signatures, tested header-only chord-tone/voice-leading/humanization helpers, and one-time CMake wiring for both build targets -- so Wave 2 plans (styles, bass) implement generator bodies only, never touching CMakeLists.txt.**

## Performance

- **Duration:** ~6 min
- **Started:** 2026-07-13T12:33:26+01:00
- **Completed:** 2026-07-13T12:38:44+01:00
- **Tasks:** 3 (Task 2 was TDD: RED + GREEN commits)
- **Files modified:** 20 (18 created, CMakeLists.txt + one test file modified across tasks)

## Accomplishments
- Source/MidiGen/ pure-C++ module skeleton: 14 files, zero GUI includes, only AnalysisResult.h from Analysis
- All 5 generator function signatures frozen (AsIsRowGenerator, 3 style generators consolidated in StyleVoicingGenerators, BassLineGenerator, MidiRowBuilder's generateAllRows orchestrator) with stub `{}` bodies
- ChordToneMapper.h/VoiceLeadingEngine.h/Humanization.h fully implemented and unit-tested (6 TEST_CASEs, all green): interval tables mirror ChordTemplates.h exactly, deterministic jitter is a pure Knuth-hash function with no mutable state
- Tests/MidiGenFixtures.h: 4 struct-literal AnalysisResult fixtures covering the happy path (4 chords, all 3 qualities), the NoChord negative case, uneven segment lengths, and a ~150-segment real-track-scale fixture -- self-consistency tested (beat-grid monotonicity, bar-index alignment, seconds/beat-index derivation)
- CMakeLists.txt wired once for the whole phase: CHORDAI_MIDIGEN_SOURCES list + 5 new test files registered in both ChordAI and ChordAITests targets
- Full suite grew from 72/72 to 79/79, all green throughout

## Task Commits

Each task was committed atomically:

1. **Task 1: Data model + generator skeleton files + one-time CMake wiring** - `2205d0d` (feat)
2. **Task 2: Header-only music-math helpers (TDD)** - `1a328ad` (test, RED) + `cd3bc78` (feat, GREEN)
3. **Task 3: Hand-built fixture AnalysisResults** - `d874da6` (test)

**Plan metadata:** (this commit, docs)

_Note: Task 2 followed TDD -- failing tests committed first (confirmed compile-error RED state), then implementation committed separately once green._

## Files Created/Modified
- `Source/MidiGen/NoteEvent.h` - Pure beat-domain note struct, zero JUCE dependency
- `Source/MidiGen/MidiSetRow.h` - RowStyle enum + MidiSetRow struct
- `Source/MidiGen/GenerationSettings.h` - Empty forward-compatible v1 settings placeholder
- `Source/MidiGen/ChordToneMapper.h` - triadIntervals/rnbExtensionIntervals/intervalsToMidiNotes/rootMidiNote + register-anchor and house-stab constants (implemented + tested)
- `Source/MidiGen/VoiceLeadingEngine.h` - nearestOctaveNote register-aware voice-leading helper (implemented + tested)
- `Source/MidiGen/Humanization.h` - deterministicJitter + per-style seed/velocity/jitter constants (implemented + tested)
- `Source/MidiGen/AsIsRowGenerator.h/.cpp` - frozen signature, stub body
- `Source/MidiGen/StyleVoicingGenerators.h/.cpp` - 3 frozen style-generator signatures, stub bodies
- `Source/MidiGen/BassLineGenerator.h/.cpp` - frozen signature + BassRhythm enum, stub body
- `Source/MidiGen/MidiRowBuilder.h/.cpp` - frozen generateAllRows entry-point signature, stub body
- `Tests/MidiGenFixtures.h` - 4 fixture factory functions in namespace midigen_fixtures
- `Tests/ChordToneMapperTests.cpp` - 6 TEST_CASEs covering interval tables, clamping, voice leading, jitter determinism
- `Tests/MidiRowBuilderTests.cpp` - FixtureSelfConsistency sanity test
- `Tests/StyleVoicingTests.cpp`, `Tests/BassLineGeneratorTests.cpp`, `Tests/PluginProcessorMidiGenTests.cpp` - empty scaffolds for Wave 2/3
- `CMakeLists.txt` - CHORDAI_MIDIGEN_SOURCES added to both targets, 5 new test files registered in ChordAITests

## Decisions Made
- NoChord is an explicit `{}` case in both interval tables (not a Major fallthrough) -- directly tested, matching Pitfall 1's warning from 05-RESEARCH.md
- Register-anchor and house-stab-timing constants co-located in ChordToneMapper.h rather than a separate constants file, since Wave 2 generators will consume them alongside the interval tables
- MidiGenFixtures.h factored small internal helpers (makeBeatGrid/makeBarStarts/makeSegment) to guarantee the seconds = index*60/bpm derivation rule can never drift between the 4 fixtures, without introducing any audio rendering

## Deviations from Plan

None - plan executed exactly as written. All must-have artifacts, key-links, and truths verified directly (see Self-Check below).

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Wave 2 plans (05-02 style voicing, 05-03 bass line) can implement generator bodies in parallel against the frozen signatures with zero CMakeLists.txt edits and zero header changes
- Tests/MidiGenFixtures.h's 4 factory functions are ready to drive every downstream generator/orchestrator test without further fixture design
- No blockers identified

---
*Phase: 05-midi-conveyor-generation*
*Completed: 2026-07-13*

## Self-Check: PASSED

All 20 created/modified files under Source/MidiGen/ and Tests/ verified present on disk. All 4 task commit hashes (2205d0d, 1a328ad, cd3bc78, d874da6) verified present in git log. Full suite verified 79/79 green (`ctest --test-dir build --output-on-failure`).
