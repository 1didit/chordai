---
phase: 02-audio-import-waveform
plan: 02
subsystem: ui
tags: [juce, timer-animation, drag-and-drop, pixel-art]

# Dependency graph
requires:
  - phase: 02-audio-import-waveform (Plan 02-01)
    provides: "processor.loadAudioFile/getLoadedAudio/get+setSelectedRegion/getLastLoadError/loadBroadcaster public API"
provides:
  - "ConveyorBeltComponent: Timer-driven (30Hz) pixel-art belt, FileDragAndDropTarget, onFileDropped callback, triggerChunkFallStub() stub"
  - "MidiSetsPlaceholder: reserved bottom-band empty state for Phase 5's MIDI-set list"
  - "800x520 three-band PluginEditor layout (conveyor / waveform hint area / MidiSetsPlaceholder) wired end-to-end to processor.loadAudioFile"
affects: [02-03-waveform-display, 02-04-regression-gate, 05-generation-ui, 06-preview-export]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Pixel-art rendering: paint into a small logical-resolution juce::Image (1/4 scale, allocated once in resized()), then g.drawImageWithin with lowResamplingQuality for the nearest-neighbour upscale look"
    - "Message-thread-only animation: juce::Timer drives state + repaint(); repaint() dirties only the owning component's bounds, never the parent"
    - "Non-ASCII string literals must go through juce::String(juce::CharPointer_UTF8(...)) — the implicit const char* -> String constructor uses CharPointer_ASCII and jasserts on bytes > 127"

key-files:
  created:
    - Source/UI/ConveyorBeltComponent.h
    - Source/UI/ConveyorBeltComponent.cpp
    - Source/UI/MidiSetsPlaceholder.h
    - Source/UI/MidiSetsPlaceholder.cpp
  modified:
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - CMakeLists.txt

key-decisions:
  - "Non-ASCII em-dash literal in MidiSetsPlaceholder's label routed through juce::CharPointer_UTF8 explicitly, since the implicit const char* String constructor asserts on any byte > 127 (caught by pluginval strictness 5's Editor Automation pass, not by the build)."
  - "Deprecated juce::Font(float) constructor calls replaced with juce::Font(juce::FontOptions(float)) per JUCE 8's own deprecation guidance, to keep the build warning-free."

patterns-established:
  - "Pattern: UI-only Timer/painting components (ConveyorBeltComponent, MidiSetsPlaceholder) are added to the ChordAI target only, not ChordAITests — untested-by-design per 02-VALIDATION.md's manual-only table."

requirements-completed: [IMP-01]

# Metrics
duration: 3min
completed: 2026-07-12
---

# Phase 2 Plan 02: Conveyor Belt UI + Three-Band Editor Layout Summary

**Timer-driven pixel-art conveyor belt (30Hz, nearest-neighbour upscaled) as the OS file-drop target, plus an 800x520 three-band editor (conveyor / waveform hint / reserved MIDI-sets placeholder) with drop wired end-to-end into processor.loadAudioFile.**

## Performance

- **Duration:** ~3 min (commit-to-commit span)
- **Started:** 2026-07-12T19:38:07+01:00
- **Completed:** 2026-07-12T19:40:45+01:00
- **Tasks:** 2
- **Files modified:** 7 (4 created, 3 modified)

## Accomplishments
- `ConveyorBeltComponent`: procedural pixel-art belt (dark background, mid-band belt surface, travelling tread slats, roller ends, edge highlights) rendered at 1/4 logical resolution and upscaled nearest-neighbour; `juce::Timer` at 30Hz advances `beltOffset` and calls `repaint()` on itself only
- `FileDragAndDropTarget` on the belt: accepts exactly one file with extension in `{.wav, .mp3, .aiff, .aif, .flac}`; hover state brightens the belt and draws a gold outline; `onFileDropped` std::function fires the dropped file up to the editor
- `triggerChunkFallStub()` — falling piano-roll-note visual stub (gravity-integrated y position, removed once off-screen); method exists but nothing calls it yet (Plan 02-03 fires it on load-complete)
- `MidiSetsPlaceholder`: reserved bottom-band empty state with a dimmed centered label; no rows/interaction (Phase 5 scope)
- `PluginEditor` resized to 800x520 with three Rectangle-sliced bands (conveyor top 120px, MidiSetsPlaceholder bottom 140px, remaining ~260px middle stored as `waveformArea` for Plan 02-03); `conveyor.onFileDropped` lambda calls `processor.loadAudioFile(std::move(f))`
- Full regression gate green: 10/10 ChordAITests, pluginval strictness 5 SUCCESS on VST3 and AU (including `auval exited with code: 0`), standalone smoke test PASS (5s liveness, no crash report)
- IMP-01 now fully evidenced end-to-end: OS drag-and-drop onto the conveyor reaches the Plan 02-01 background decode path

## Task Commits

Each task was committed atomically:

1. **Task 1: ConveyorBeltComponent — procedural pixel-art belt, Timer animation, file-drop target** - `64ce458` (feat)
2. **Task 2: MidiSetsPlaceholder + three-band editor layout wired to loadAudioFile** - `00fd405` (feat)

**Plan metadata:** (this commit)

_Note: Task 2's commit also includes the em-dash/CharPointer_UTF8 fix discovered via pluginval, not a separate commit — see Deviations below._

## Files Created/Modified
- `Source/UI/ConveyorBeltComponent.h` / `.cpp` - Timer-driven pixel-art belt, FileDragAndDropTarget, onFileDropped, triggerChunkFallStub()
- `Source/UI/MidiSetsPlaceholder.h` / `.cpp` - Reserved bottom-band empty state
- `Source/PluginEditor.h` - Added conveyor/midiSetsPlaceholder members, waveformArea rect
- `Source/PluginEditor.cpp` - 800x520 setSize, three-band resized(), drop wired to processor.loadAudioFile
- `CMakeLists.txt` - Added both new .cpp files to ChordAI target_sources only (not ChordAITests)

## Decisions Made
- Non-ASCII em-dash literal routed through `juce::String(juce::CharPointer_UTF8(...))` instead of a plain `"..."` literal — JUCE's `String(const char*)` constructor assumes ASCII and `jassert`s on any byte > 127; the plain literal's UTF-8-encoded em-dash bytes tripped this every time the editor painted, caught by pluginval's Editor Automation pass (not the build).
- Replaced deprecated `juce::Font(float)` calls with `juce::Font(juce::FontOptions(float))` (JUCE 8's own recommended replacement) to keep the build warning-free.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Non-ASCII em-dash literal asserted via JUCE's ASCII String constructor**
- **Found during:** Task 2 (pluginval strictness 5 regression gate, VST3 "Editor Automation" pass)
- **Issue:** `MidiSetsPlaceholder`'s label used a plain `"MIDI SETS — ..."` string literal; the implicit `const char* -> juce::String` conversion uses `CharPointer_ASCII` and `jassert`s on the em-dash's UTF-8 bytes (>127), firing twice during editor open/close under pluginval
- **Fix:** Wrapped the literal in `juce::String (juce::CharPointer_UTF8 ("..."))`
- **Files modified:** Source/UI/MidiSetsPlaceholder.cpp
- **Commit:** 00fd405

**2. [Rule 1 - Bug] Deprecated juce::Font(float) constructor produced build warnings**
- **Found during:** Task 2 (first `cmake --build` after adding text rendering)
- **Issue:** `juce::Font(float)` is deprecated in JUCE 8 in favor of the `FontOptions`-based constructor
- **Fix:** Switched both new call sites to `juce::Font (juce::FontOptions (size))`
- **Files modified:** Source/UI/MidiSetsPlaceholder.cpp, Source/PluginEditor.cpp
- **Commit:** 00fd405

---

**Total deviations:** 2 auto-fixed (both Rule 1 - bugs in the current task's own new code, one caught by the pluginval gate, one by the build)
**Impact on plan:** Both fixes necessary for correctness (assertion) and build hygiene (deprecation warning). No scope creep, no architectural changes.

## Issues Encountered
None beyond the two auto-fixed issues above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `waveformArea` rectangle is stored on `PluginEditor` and ready for Plan 02-03 to place a `WaveformView` into
- `ConveyorBeltComponent::triggerChunkFallStub()` exists and is ready for Plan 02-03 to call once on load-complete (via `processor.loadBroadcaster`)
- IMP-01 requirement is now fully evidenced (backend decode from Plan 02-01 + drag-and-drop UI from this plan) and marked complete
- No blockers for Plan 02-03

---
*Phase: 02-audio-import-waveform*
*Completed: 2026-07-12*

## Self-Check: PASSED

All 8 claimed files verified present on disk; both claimed commit hashes (64ce458, 00fd405) verified present in git history.
