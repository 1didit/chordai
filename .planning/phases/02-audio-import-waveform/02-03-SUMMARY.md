---
phase: 02-audio-import-waveform
plan: 03
subsystem: ui
tags: [juce, audiothumbnail, mouse-interaction, tdd, catch2]

# Dependency graph
requires:
  - phase: 02-audio-import-waveform (Plan 02-01)
    provides: "processor.loadAudioFile/getLoadedAudio/get+setSelectedRegion/getLastLoadError/loadBroadcaster public API"
  - phase: 02-audio-import-waveform (Plan 02-02)
    provides: "ConveyorBeltComponent.triggerChunkFallStub(), 800x520 three-band PluginEditor layout with waveformArea bounds"
provides:
  - "WaveformMath.h: pure timeToX/xToTime pixel<->time conversion (free functions, zero-length-range guarded)"
  - "RegionSelectionModel.h: pure drag-selection state machine (whole-file default, clamping, normalization, click/micro-drag reset)"
  - "WaveformView: AudioThumbnail + AudioThumbnailCache wrapper with progressive-fill repaint, error text, empty-state hint"
  - "RegionSelectorOverlay: transparent mouse-handling overlay painting the selection, onRegionChanged callback"
  - "Full editor wiring: load complete -> waveform + region restore -> processor.setSelectedRegion on drag; editor-reopen restores from processor state"
affects: [02-04-regression-gate, 03-chord-detection-engine, 04-analysis-ui-wiring]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Testable-core pure-logic headers (WaveformMath.h, RegionSelectionModel.h) with zero Component/GUI dependency beyond juce_core types, unit-tested directly; thin Component wrappers (WaveformView, RegionSelectorOverlay) delegate all math/state to them"
    - "AudioThumbnail owns an independent AudioFormatManager instance from the processor's own decode path — thumbnail scanning and processor decode never share mutable state"
    - "Shared private helper (handleLoadComplete) reused by both the ChangeListener callback (fresh load) and the ctor's editor-reopen restore path, avoiding duplicated wiring logic"

key-files:
  created:
    - Source/UI/WaveformMath.h
    - Source/UI/RegionSelectionModel.h
    - Source/UI/WaveformView.h
    - Source/UI/WaveformView.cpp
    - Source/UI/RegionSelectorOverlay.h
    - Source/UI/RegionSelectorOverlay.cpp
    - Tests/WaveformRegionTests.cpp
  modified:
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - CMakeLists.txt

key-decisions:
  - "RegionSelectionModel exposes getTotalLength() (not in the original Task 1 spec) so RegionSelectorOverlay can build the {0, totalLength} visibleRange needed for WaveformMath conversion, without duplicating the length in the overlay."
  - "handleLoadComplete(const LoadedAudio&) factored as a single private editor method, called from both changeListenerCallback (fresh load) and the ctor's editor-reopen branch, per the plan's 'run the same load-complete path' instruction — avoids duplicating the waveform/region/conveyor-stub wiring."

patterns-established:
  - "Pattern: pure-logic header + thin Component wrapper split — WaveformMath/RegionSelectionModel are message-thread-agnostic and unit-tested directly; WaveformView/RegionSelectorOverlay are untested-by-design Components that only wire mouse/paint events to the tested logic."

requirements-completed: [IMP-02, IMP-03]

# Metrics
duration: 5min
completed: 2026-07-12
---

# Phase 2 Plan 03: Waveform Display + Region Selection Summary

**AudioThumbnail-based waveform view plus a unit-tested drag-selection overlay (RegionSelectionModel + WaveformMath), fully wired so load completion renders the waveform and dragging updates `processor.setSelectedRegion`.**

## Performance

- **Duration:** ~5 min (commit-to-commit span)
- **Started:** 2026-07-12T19:45:21+01:00
- **Completed:** 2026-07-12T19:49:58+01:00
- **Tasks:** 3 (Task 1 followed TDD red→green)
- **Files modified:** 10 (7 created, 3 modified)

## Accomplishments
- `WaveformMath.h`: pure `timeToX`/`xToTime` pixel<->time conversion, zero-length-visible-range guarded against divide-by-zero — 5/5 pure-logic + thumbnail test cases green
- `RegionSelectionModel.h`: pure drag-selection state machine — whole-file default on `setTotalLength`, drag clamping/normalization (right-to-left drags always produce `start <= end`), click/micro-drag (<0.05s) resets back to whole file
- `WaveformView`: `AudioThumbnail` + `AudioThumbnailCache` wrapper with its own `AudioFormatManager`; progressive-fill repaint via `ChangeListener`, error text on failed load, "DROP A TRACK ONTO THE CONVEYOR" empty-state hint
- `RegionSelectorOverlay`: transparent mouse-handling `Component` on top of `WaveformView`, driving `RegionSelectionModel` and painting the selection chrome (dimmed outside areas, bright edges, subtle tint) — paints nothing when whole-file is selected
- Full wiring loop in `PluginEditor`: `ChangeListener` on `processor.loadBroadcaster` pulls `getLoadedAudio()`/`getLastLoadError()`, sets the waveform source and region total length, and fires `conveyor.triggerChunkFallStub()`; `regionOverlay.onRegionChanged` calls `processor.setSelectedRegion`; editor-reopen (DAW closes/reopens editor) restores the waveform+region from processor state without a re-drop; destructor removes the change listener
- Full regression gate green: 15/15 `ChordAITests`, pluginval strictness 5 SUCCESS on both VST3 and AU (`auval exited with code: 0`), standalone smoke test PASS

## Task Commits

Each task was committed atomically (Task 1's TDD portion produced separate RED/GREEN commits):

1. **Task 1a: Failing tests for WaveformMath + RegionSelectionModel (RED)** - `b9bfd9c` (test)
1. **Task 1b: Implement WaveformMath + RegionSelectionModel (GREEN)** - `6e94434` (feat)
2. **Task 2: WaveformView + RegionSelectorOverlay components (+ ThumbnailPopulates test)** - `805fe85` (feat)
3. **Task 3: Editor wiring — load completion to waveform, selection to processor state** - `aa07d98` (feat)

**Plan metadata:** (this commit)

## Files Created/Modified
- `Source/UI/WaveformMath.h` - Pure `timeToX`/`xToTime` free functions, zero-length-range guarded
- `Source/UI/RegionSelectionModel.h` - Pure drag-selection state machine (whole-file default, clamp/normalize, click-reset)
- `Source/UI/WaveformView.h` / `.cpp` - `AudioThumbnail` wrapper: setSource/setErrorMessage/getTotalLength, progressive-fill repaint, error/hint/waveform paint states
- `Source/UI/RegionSelectorOverlay.h` / `.cpp` - Transparent mouse-handling overlay, `onRegionChanged` callback, selection chrome painting
- `Tests/WaveformRegionTests.cpp` - PixelTimeConversion, DefaultWholeFile, DragSelectionClamped, ClickResetsToWholeFile, ThumbnailPopulates (5 cases)
- `Source/PluginEditor.h` / `.cpp` - `ChangeListener` on `loadBroadcaster`; `waveformView`/`regionOverlay` members; `handleLoadComplete` shared helper; editor-reopen restore; destructor removes listener; static hint text removed (WaveformView owns it now)
- `CMakeLists.txt` - `Tests/WaveformRegionTests.cpp` added to `ChordAITests`; `WaveformView.cpp`/`RegionSelectorOverlay.cpp` added to `ChordAI` only

## Decisions Made
- Added `RegionSelectionModel::getTotalLength()` (beyond the Task 1 spec's four methods) so the overlay can construct the `{0, totalLength}` visible range it needs for `WaveformMath` conversion, without the overlay caching its own copy of the length.
- Factored `handleLoadComplete(const LoadedAudio&)` as one private editor method shared by `changeListenerCallback` (fresh load) and the ctor's editor-reopen branch, matching the plan's instruction to "run the same load-complete path" on reopen rather than duplicating the three wiring lines.

## Deviations from Plan

None - plan executed exactly as written. The one addition (`RegionSelectionModel::getTotalLength()`) is a same-file, same-task extension needed to satisfy the plan's own `RegionSelectorOverlay` interface description ("convert e.x via xToTime(..., {0.0, model total length}, ...)"), not a scope change.

## Issues Encountered
None. One pre-existing, out-of-scope build warning was noted and left alone (see below).

**Out-of-scope note:** `Tests/WaveformRegionTests.cpp`'s new WAV-fixture helper triggers the same `AudioFormatWriter::createWriterFor` deprecation warning already present in `Tests/AudioFileLoaderTests.cpp` (Plan 02-01, unfixed there). Left as-is for consistency with the existing test-fixture convention; does not affect correctness or any test result.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- IMP-02 and IMP-03 are now fully evidenced: waveform renders on load, region selection drags/clamps/resets and reaches `apvts.state` via `processor.setSelectedRegion`
- Decoded buffer (`LoadedAudio::buffer`) and the selected region (`processor.getSelectedRegion()`) are both user-drivable and available for Phase 3's detection engine to consume directly
- Plan 02-04 (regression gate / phase close-out) has a clean full-suite baseline: 15/15 `ChordAITests`, pluginval strictness 5 SUCCESS (VST3+AU), standalone smoke PASS
- No blockers for Plan 02-04

---
*Phase: 02-audio-import-waveform*
*Completed: 2026-07-12*

## Self-Check: PASSED

All 10 claimed files verified present on disk; all 4 claimed commit hashes (b9bfd9c, 6e94434, 805fe85, aa07d98) verified present in git history.
