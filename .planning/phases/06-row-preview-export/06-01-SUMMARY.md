---
phase: 06-row-preview-export
plan: 01
subsystem: midigen
tags: [juce-midifile, standard-midi-file, export, tdd]

# Dependency graph
requires:
  - phase: 05-midi-conveyor-generation
    provides: "MidiSetRow/NoteEvent beat-domain data model, generateAllRows orchestrator"
provides:
  - "MidiFileWriter::buildMidiFile/writeToFile/suggestedFileName -- the single shared MIDI-file core both EXP-01 (drag-out) and EXP-02 (save dialog) will call in 06-03"
affects: [06-02, 06-03, 06-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Format-1 Standard MIDI File: tempo+4/4 meta-events isolated on track 0, notes on track 1, beat-domain-to-tick conversion is a pure multiply (ticks = beats * 960), never re-derived via seconds/BPM"
    - "juce::TemporaryFile + overwriteTargetFileWithTemporary() for atomic file replace (never a direct FileOutputStream on the destination)"
    - "Duplicate ChordNameFormatter.h's 12-entry kNoteNames array verbatim as a file-local constant rather than extracting a shared header (research's explicit v1 guidance)"

key-files:
  created:
    - Source/MidiGen/MidiFileWriter.h
    - Source/MidiGen/MidiFileWriter.cpp
    - Tests/MidiFileWriterTests.cpp
  modified:
    - CMakeLists.txt

key-decisions:
  - "MidiFileWriter kept under Source/MidiGen/ (not a new Source/Audio/ folder) since it only needs juce_audio_basics' MIDI types, not audio-buffer/DSP types -- per 06-RESEARCH.md Open Question 2's own noted option"

patterns-established:
  - "Single shared MIDI-writer code path for both export entry points -- divergence here is exactly how EXP-03's tempo/bar-alignment guarantee could silently break in one path and not the other"

requirements-completed: [EXP-03]

# Metrics
duration: ~14min
completed: 2026-07-13
---

# Phase 6 Plan 01: MidiFileWriter Core Summary

**Beat-domain MidiSetRow + detected bpm converts to a format-1 Standard MIDI File (TPQN 960, tempo/4/4 meta track + note track), written atomically to disk, with MIDI-pack-style suggested file naming -- proven by 11 round-trip unit tests, not manual DAW import.**

## Performance

- **Duration:** ~14 min
- **Started:** 2026-07-13T15:04:00+01:00 (approx, first Task 1 file write)
- **Completed:** 2026-07-13T15:11:40+01:00
- **Tasks:** 2 completed (both TDD red -> green)
- **Files modified:** 4 (3 created, 1 modified)

## Accomplishments
- `MidiFileWriter::buildMidiFile` converts any `MidiSetRow` + bpm into a format-1 `juce::MidiFile`: track 0 carries tempo (from `AnalysisResult.bpm`, `bpm<=0` falls back to 120) + a 4/4 time-signature meta-event; track 1 carries every well-formed note as a matched note-on/note-off pair via pure beat-domain tick multiplication (`ticks = beats * 960`)
- Zero/negative-length notes (`lengthBeats <= 0.0`) are silently dropped rather than written as degenerate zero-tick-length events
- `MidiFileWriter::writeToFile` streams to disk atomically via `juce::TemporaryFile` + `overwriteTargetFileWithTemporary()` -- a second write to the same path replaces rather than appends
- `MidiFileWriter::suggestedFileName` produces `rowId_Key_NNNbpm.mid` (e.g. `pop-trap_G#m_128bpm.mid`), filesystem-safe across all 5 row ids x 12 keys x major/minor, and unique per row id
- 11 new `[midifilewriter]` tests covering round-trip ticks/tempo/time-sig/bar-alignment, bpm fallback, zero-length guard, write-to-file, overwrite-not-append, empty-row safety, and all 5 file-naming behaviors

## Task Commits

Each task was committed atomically (TDD red -> green):

1. **Task 1: MidiFileWriter core (buildMidiFile + writeToFile)**
   - `4df3639` test(06-01): add failing MidiFileWriter round-trip tests (RED)
   - `96faefd` feat(06-01): implement MidiFileWriter (format 1, TPQN 960, tempo + 4/4 meta) (GREEN)
2. **Task 2: suggestedFileName**
   - `f308172` test(06-01): add failing suggestedFileName tests (RED)
   - `824b4ee` feat(06-01): implement MIDI-pack-style suggestedFileName (GREEN)

**Plan metadata:** (this commit) docs(06-01): complete MidiFileWriter core plan

## Files Created/Modified
- `Source/MidiGen/MidiFileWriter.h` - Public contract: `kTicksPerQuarterNote`, `buildMidiFile`, `writeToFile`, `suggestedFileName`
- `Source/MidiGen/MidiFileWriter.cpp` - Implementation: beat->tick note-sequence conversion, meta track, zero-length guard, atomic write, MIDI-pack-style naming
- `Tests/MidiFileWriterTests.cpp` - 11 `[midifilewriter]` tests (round-trip, tempo/fallback, time-sig, zero-length-guard, write-to-file, overwrite, empty-row, 5 naming tests)
- `CMakeLists.txt` - `MidiFileWriter.cpp` added to `CHORDAI_MIDIGEN_SOURCES` (both `ChordAI`/`ChordAITests` targets); `MidiFileWriterTests.cpp` added to `ChordAITests` sources

## Decisions Made
- `MidiFileWriter.h/.cpp` placed in `Source/MidiGen/` (not a new `Source/Audio/` folder) since it only depends on `juce_audio_basics` MIDI types (`juce::MidiFile`/`MidiMessageSequence`/`MidiMessage`), not audio-buffer/DSP types -- consistent with 06-RESEARCH.md Open Question 2's own noted fallback option. `Source/Audio/AuditionRenderer` (PRV-01, a later plan) is the module that actually needs the new folder.
- `kNoteNames` duplicated verbatim as a file-local constant in `MidiFileWriter.cpp` rather than extracting `ChordNameFormatter.h`'s array into a shared header, per the plan's explicit instruction (research's stated v1 guidance: duplicate, don't refactor this phase).

## Deviations from Plan

None - plan executed exactly as written. Both tasks followed the plan's specified TDD red->green sequence and implementation shape verbatim (Pattern 1 from 06-RESEARCH.md).

## Issues Encountered
None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- `MidiFileWriter::buildMidiFile`/`writeToFile`/`suggestedFileName` are ready for 06-03 to wire into both EXP-01 (drag-out temp file) and EXP-02 (save dialog) -- a single shared conversion path exists, eliminating the risk of EXP-03's tempo/bar-alignment guarantee diverging between the two entry points
- EXP-03 core is evidenced by unit tests (bar-2 tick assertion at startBeats 4.0, tempo round-trip including non-integer bpm and the bpm<=0 fallback); full DAW-import verification is still 06-03/06-04's manual checkpoint job
- No plugin-target (`processBlock`) behavior changed this plan -- pluginval gate is deferred to the first `processBlock`-touching wave (06-02, the audition renderer)
- Full suite green: 120/120 (109 baseline + 11 new `[midifilewriter]` tests), zero regressions

---
*Phase: 06-row-preview-export*
*Completed: 2026-07-13*

## Self-Check: PASSED

All created files found on disk; all 4 task commit hashes (4df3639, 96faefd, f308172, 824b4ee) found in git log.
