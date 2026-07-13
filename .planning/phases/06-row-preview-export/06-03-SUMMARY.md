---
phase: 06-row-preview-export
plan: 03
subsystem: ui
tags: [juce-component, drag-and-drop, filechooser, timer, mouse-interaction]

# Dependency graph
requires:
  - phase: 06-row-preview-export
    provides: "06-01 MidiFileWriter::writeToFile/suggestedFileName; 06-02 processor startAudition/stopAudition/isAuditionPlaying/getAuditionRowId/getAnalysisResult"
provides:
  - "Interactive MidiRowView: play/stop + save icons in the 92px gutter, drag-anywhere-on-row OS file drag"
  - "MidiSetsPanel hook-forwarding + stop-audition-on-setRows + 10Hz playing-state repaint Timer"
  - "PluginEditor -> MidiSetsPanel -> MidiRowView hook wiring, end to end"
affects: [06-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Pure hit-zone geometry (MidiRowLayout.h playIconRect/saveIconRect) unit-tested without a Component, same extraction rationale as beatsToX/pitchToY"
    - "mouseDown/mouseDrag/mouseUp gesture split: dragStarted guard flips on first e.mouseWasDraggedSinceMouseDown(), click logic only runs in mouseUp when no drag happened"
    - "Deferred drag-temp-file cleanup: sweep tempDirectory/ChordAI/*.mid at the START of the next drag, never in performExternalDragDropOfFiles's completion callback (nullptr always) -- the Ableton async-read fix"
    - "FileChooser stored as a unique_ptr member (outlives launchAsync's callback); callback captures row/bpm BY VALUE, never `this` -- safe even if the owning MidiRowView is destroyed by a mid-dialog setRows() regeneration"
    - "Playing-row identity lives on PluginProcessor (getAuditionRowId), never on MidiRowView; MidiSetsPanel::setRows() calls onStopAudition unconditionally as its first statement before destroying/rebuilding every row view"

key-files:
  created: []
  modified:
    - Source/UI/MidiRowLayout.h
    - Source/UI/MidiRowView.h
    - Source/UI/MidiRowView.cpp
    - Source/UI/MidiSetsPanel.h
    - Source/UI/MidiSetsPanel.cpp
    - Source/PluginEditor.cpp
    - Tests/MidiSetsPanelLayoutTests.cpp

key-decisions:
  - "Icon layout: two 12x12 zones stacked vertically at the gutter's right edge (play/stop on top, save below, 2px gap, 3px right margin), right-aligned so the label keeps >=55px of text width -- executor discretion within the plan's tested invariants"
  - "MidiSetsPanel made a private juce::Timer (10Hz) started only inside the onAuditionToggle wrapper and stopped once isRowPlaying reports nothing playing -- avoids a permanently-running Timer, consistent with 02-CONTEXT.md's message-thread animation discipline"

patterns-established:
  - "Row-level hooks (getBpmForExport/getKeyForExport/onAuditionToggle/isRowPlaying) forwarded panel->view on every rebuild -- MidiRowView never reaches into PluginProcessor directly, the decoupling established in 06-RESEARCH.md User Constraints"

requirements-completed: [PRV-01, EXP-01, EXP-02]

# Metrics
duration: ~12min
completed: 2026-07-13
---

# Phase 6 Plan 03: MidiRowView Interaction (Audition, Drag-Out, Save) Summary

**MidiRowView gained click-to-audition play/stop + save icons and drag-anywhere-on-row OS file export, wired end-to-end through MidiSetsPanel hook-forwarding into ChordAIAudioProcessor's audition/export API; full suite 134/134 green, pluginval strictness 5 SUCCESS on VST3 and AU.**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-07-13T15:33:00+01:00 (approx, first Task 1 file read)
- **Completed:** 2026-07-13T15:45:33+01:00
- **Tasks:** 3 completed (Task 1 TDD red -> green; Tasks 2-3 direct feat)
- **Files modified:** 7

## Accomplishments
- `MidiRowLayout.h` gained `playIconRect`/`saveIconRect`: pure, unit-tested hit-zone geometry (fit-inside-gutter, non-overlapping, >=10x10px, right-aligned keeping >=55px label width, deterministic, degenerate-safe) -- 2 new test cases, `[midisetspanellayout]`
- `MidiRowView` flipped `setInterceptsMouseClicks(true, false)`; paint() draws flat-rect play/stop/save glyphs (idle grey, playing = per-row accent colour) without ever underlapping the label text
- `mouseDown`/`mouseDrag`/`mouseUp` gesture split: a real drag (anywhere on the row body, icons included) writes a fresh temp `.mid` (via 06-01's `MidiFileWriter`) and hands it to `juce::DragAndDropContainer::performExternalDragDropOfFiles` with `canMoveFiles=false` and a `nullptr` completion callback; the *previous* drag's temp file(s) are swept at the **start** of the next drag, never in a completion callback (06-RESEARCH.md Pitfall 1's Ableton "could not be opened" fix)
- `saveRow()`: async `juce::FileChooser` (member `unique_ptr`, outlives the callback), suggested filename via `MidiFileWriter::suggestedFileName`, defaults to `~/Documents/ChordAI MIDI/`, remembers the last-used directory for the session via a function-local `static`; the callback captures the row and bpm by value and never captures `this`
- `MidiSetsPanel` forwards `getBpmForExport`/`getKeyForExport`/`onAuditionToggle`/`isRowPlaying` into every rebuilt `MidiRowView`, calls `onStopAudition` unconditionally as the first line of `setRows()`, and is a private `juce::Timer` (10Hz, started on audition toggle, self-stopping once nothing is playing) so icons reflect processor-driven auto-stop within ~100ms without any cached boolean on a view
- `PluginEditor` wires all 5 hooks to `ChordAIAudioProcessor` in the ctor, before the editor-reopen restore branch that may call `setRows()` immediately
- Full suite 134/134 green (132 baseline + 2 new `[midisetspanellayout]` icon-geometry tests); pluginval strictness 5 SUCCESS on both VST3 and AU (first wave to exercise the Editor Automation pass against the new mouse/paint interaction paths)

## Task Commits

Each task was committed atomically:

1. **Task 1: Icon geometry + MidiRowView interaction skeleton**
   - `9b1fd9d` test(06-03): add failing row icon hit-zone geometry tests (RED)
   - `c472dde` feat(06-03): MidiRowView interaction skeleton -- icons, hooks, click dispatch (GREEN)
2. **Task 2: Drag-out (EXP-01) + save dialog (EXP-02) flows** - `72ae217` feat
3. **Task 3: MidiSetsPanel forwarding + stop-on-regenerate + PluginEditor wiring** - `6ecfb2e` feat

**Plan metadata:** (this commit) docs(06-03): complete row interaction plan

## Files Created/Modified
- `Source/UI/MidiRowLayout.h` - `playIconRect`/`saveIconRect` pure hit-zone geometry (`MidiRowIconLayout` constants: 12px icons, 2px gap, 3px right margin)
- `Source/UI/MidiRowView.h` - `mouseDown`/`mouseDrag`/`mouseUp` overrides; `getBpmForExport`/`getKeyForExport`/`onAuditionToggle`/`isRowPlaying` hooks; `dragStarted`/`fileChooser` members; private `saveRow()`
- `Source/UI/MidiRowView.cpp` - glyph painters (play/stop/save, flat rects only); `writeDragTempFile` file-local helper (deferred cleanup + `MidiFileWriter::writeToFile`); gesture-split mouse handlers; drag-out + async save implementations
- `Source/UI/MidiSetsPanel.h` - now `private juce::Timer`; 5 hook members (`getBpmForExport`, `getKeyForExport`, `onAuditionToggle`, `isRowPlaying`, `onStopAudition`); `timerCallback()`
- `Source/UI/MidiSetsPanel.cpp` - `setRows()` calls `onStopAudition` first, forwards hooks into every new `MidiRowView`, wraps `onAuditionToggle` to also `startTimerHz(10)`; `timerCallback()` repaints all rows and self-stops once `isRowPlaying` is false for every row
- `Source/PluginEditor.cpp` - assigns all 5 `midiSetsPanel` hooks to `processor` calls, before the editor-reopen restore branch
- `Tests/MidiSetsPanelLayoutTests.cpp` - 2 new `[midisetspanellayout]` test cases for icon geometry (fit/overlap/size/alignment, purity/degenerate-safety)

## Decisions Made
- Icon layout chosen at executor discretion within the plan's tested invariants: two stacked 12x12 zones at the gutter's right edge, matching the plan's "recommended layout" suggestion exactly.
- `MidiSetsPanel` becoming a `juce::Timer` (rather than reusing an existing animation Timer elsewhere) was the plan's own explicit instruction ("This is the only new Timer and it runs only while auditioning").

## Deviations from Plan

None - plan executed exactly as written, including the deferred-temp-cleanup fix, the `canMoveFiles=false`/`nullptr`-callback drag call, the FileChooser member/by-value-capture lifetime safety, and the `onStopAudition`-first-line `setRows()` guard.

## Issues Encountered
None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- EXP-01/EXP-02/PRV-01's interactive UI surface is fully wired end-to-end; only the OS-level drag-into-DAW, native save dialog, and audible sound are inherently manual (not unit-testable per 06-RESEARCH.md's own Validation Architecture) -- these land in 06-04's human checkpoint
- Full suite green: 134/134 (132 baseline + 2 new `[midisetspanellayout]` tests), zero regressions; pluginval strictness 5 SUCCESS on VST3 and AU (Editor Automation pass now exercises real mouse/paint code, not just a no-op editor)
- Code-review greps confirmed: `performExternalDragDropOfFiles` called with `false, this, nullptr`; `onStopAudition` is `setRows()`'s first statement; no `this` captured in the `FileChooser` callback; `setInterceptsMouseClicks (true` in the `MidiRowView` ctor; `RegionSelectorOverlay.h/.cpp` show zero diff

---
*Phase: 06-row-preview-export*
*Completed: 2026-07-13*

## Self-Check: PASSED

All 7 created/modified source/test files found on disk; all 4 task commit hashes (9b1fd9d, c472dde, 72ae217, 6ecfb2e) found in git log.
