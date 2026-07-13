---
phase: 05-midi-conveyor-generation
plan: 05
subsystem: ui
tags: [juce, c++, midi, piano-roll, catch2, pixel-art]

# Dependency graph
requires:
  - phase: 05-midi-conveyor-generation (plan 04)
    provides: "ChordAIAudioProcessor::getMidiSetRows() atomic accessor, published in the same analysisBroadcaster message as getAnalysisResult()"
  - phase: 02-audio-import-waveform (plan 02)
    provides: "juce::CharPointer_UTF8 convention for non-ASCII string literals"
provides:
  - "MidiRowLayout.h: pure beatsToX/noteWidthPx/pitchToY pixel math (unit-testable, no Component dependency)"
  - "MidiRowView: one row's labeled mini piano-roll strip, display-only, setRow/getRow API structured for Phase 6 audition/drag"
  - "MidiSetsPanel: bottom-band panel owning 5 MidiRowViews, setRows(shared_ptr<const vector<MidiSetRow>>), shared time axis across all rows, empty-state placeholder"
  - "MidiSetsPlaceholder fully removed from repo and both CMake targets"
affects: [06-row-preview-export]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "MidiRowLayout.h mirrors WaveformMath.h's/ChordTimelineView.h's extraction rationale: pure free functions in Source/UI, header-only, unit-tested without a Component"
    - "Each row auto-zooms its own pitch register (min/max note pitch +/-2 semitones) rather than sharing one global pitch range -- keeps sparse bass rows legible next to dense chord rows"
    - "Note velocity maps to accent-colour alpha (0.55 + 0.45*velocity), not size/height -- flat rects only, matches the locked pixel-art aesthetic (no gradients/rounded corners)"

key-files:
  created:
    - Source/UI/MidiRowLayout.h
    - Source/UI/MidiRowView.h
    - Source/UI/MidiRowView.cpp
    - Source/UI/MidiSetsPanel.h
    - Source/UI/MidiSetsPanel.cpp
    - Tests/MidiSetsPanelLayoutTests.cpp
  modified:
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - CMakeLists.txt
  deleted:
    - Source/UI/MidiSetsPlaceholder.h
    - Source/UI/MidiSetsPlaceholder.cpp

key-decisions:
  - "Fixed a relative-include bug in the recovered skeleton files (MidiRowView.h/MidiSetsPanel.h used \"MidiGen/MidiSetRow.h\" instead of \"../MidiGen/MidiSetRow.h\") before proceeding -- Source/UI/ files resolve sibling-directory includes relative to their own folder, matching ChordTimelineView.h's existing \"../Analysis/...\" precedent."
  - "Empty-state string lifted verbatim from MidiSetsPlaceholder.cpp (including its CharPointer_UTF8-wrapped em dash) so the idle bottom-band look is pixel-identical before and after the swap."

patterns-established:
  - "Comments referencing a deleted component's old name were reworded (not left as historical references) so the plan's literal 'grep must be empty' dangling-reference check holds -- future deletions should do the same."

requirements-completed: [GEN-01]

# Metrics
duration: ~15min (recovery session -- resumed from a prior agent's untracked skeleton files, no prior commits existed)
completed: 2026-07-13
---

# Phase 5 Plan 05: MidiSetsPanel Bottom-Band UI Summary

**Five labeled mini piano-roll strips (MidiRowView inside MidiSetsPanel) now render the generated MIDI rows in the 140px bottom band, sharing one beat-axis time scale, wired atomically to the same broadcast as the chord timeline, replacing MidiSetsPlaceholder end-to-end.**

## Performance

- **Duration:** ~15 min (this session) -- recovery from a prior agent's cutoff; Task 1's files (MidiRowLayout.h, MidiSetsPanelLayoutTests.cpp, and MidiRowView/MidiSetsPanel skeletons) already existed untracked and matched the plan exactly once a relative-include bug was fixed
- **Started:** 2026-07-13 (session start, recovery)
- **Completed:** 2026-07-13T14:08:36+01:00 (final GREEN commit)
- **Tasks:** 3 (all `type="auto"`, no TDD flag on this plan)
- **Files modified:** 11 (6 created, 3 modified, 2 deleted)

## Accomplishments
- `MidiRowLayout.h`: `beatsToX`/`noteWidthPx`/`pitchToY` pure pixel-math free functions, proven by 3 known-answer/edge-case TEST_CASEs (linear beat mapping incl. zero-length guard, inverted pitch mapping incl. single-pitch guard and full-range clamp, sub-pixel note-width visibility floor)
- `MidiRowView`: renders a fixed ~92px label gutter (accent bar + row label) and a note area where every note rect is positioned purely via `MidiRowLayout` math; each row auto-computes its own min/max pitch (padded +/-2 semitones) so bass and chord rows both read clearly; note alpha scales with velocity (0.55-1.0); `setInterceptsMouseClicks(false, false)` keeps it display-only this phase while `getRow()` stays ready for Phase 6
- `MidiSetsPanel`: owns a `juce::OwnedArray<MidiRowView>` rebuilt to `rows->size()` on every `setRows()` call (never hardcoded to 5); computes one shared `totalBeats` across every row/note so all styles are visually comparable on the same time axis; empty state (`nullptr`/empty vector) renders MidiSetsPlaceholder's exact idle text and frame, pixel-identical
- `PluginEditor` wiring: `midiSetsPanel.setRows(processor.getMidiSetRows())` added directly after `chordTimeline.setResult(...)` in both `changeListenerCallback`'s analysisBroadcaster branch and `handleLoadComplete` -- rows and the chord timeline can never be observed out of sync, blank on fresh load, restore on editor reopen
- `MidiSetsPlaceholder.h/.cpp` deleted (`git rm`) and removed from both `ChordAI` and `ChordAITests` CMake source lists; `grep -rn "MidiSetsPlaceholder" Source/ Tests/ CMakeLists.txt` returns no matches (stray code-comment references to the old name were reworded, not left in place)
- Full suite grew from 106 to 109 (3 new `MidiSetsPanelLayoutTests`), all green; pluginval strictness 5 green for both VST3 and AU (Editor Automation exercised editor open/close with real rows now published)

## Task Commits

1. **Task 1: MidiRowLayout pure layout math + CMake registration** - `ca99ec6` (feat)
2. **Task 2: MidiRowView + MidiSetsPanel components** - `ac34a85` (feat)
3. **Task 3: Editor wiring + placeholder deletion + wave gate** - `7df5466` (feat)

**Plan metadata:** (this commit, docs)

## Files Created/Modified
- `Source/UI/MidiRowLayout.h` - Pure `beatsToX`/`noteWidthPx`/`pitchToY` conversion functions (recovered from prior session, already matched plan spec exactly)
- `Source/UI/MidiRowView.h`/`.cpp` - One row's mini piano-roll strip; `paint()` fleshed out this session (gutter + accent + note rects); relative include fixed to `../MidiGen/MidiSetRow.h`
- `Source/UI/MidiSetsPanel.h`/`.cpp` - Bottom-band panel; `setRows`/`paint`/`resized` fleshed out this session (empty state, row rebuild, shared totalBeats, even height distribution)
- `Tests/MidiSetsPanelLayoutTests.cpp` - 3 TEST_CASEs for `MidiRowLayout.h` (recovered, matched plan spec exactly)
- `Source/PluginEditor.h`/`.cpp` - `MidiSetsPlaceholder` swapped for `MidiSetsPanel`; `setRows` wired into both publication points
- `CMakeLists.txt` - `MidiRowView.cpp`/`MidiSetsPanel.cpp`/`MidiSetsPanelLayoutTests.cpp` added to both targets; `MidiSetsPlaceholder.cpp` removed from both targets
- `Source/UI/MidiSetsPlaceholder.h`/`.cpp` - deleted

## Decisions Made
- Fixed the recovered skeleton's relative-include bug (`"MidiGen/MidiSetRow.h"` -> `"../MidiGen/MidiSetRow.h"`) rather than adding a new include directory, matching `ChordTimelineView.h`'s existing `"../Analysis/..."` convention (Rule 3 - blocking build error)
- Reworded two code comments that referenced the deleted `MidiSetsPlaceholder` class name by name, so the plan's literal `grep -rn "MidiSetsPlaceholder"` dangling-reference check returns empty as specified

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed relative-include path in recovered skeleton headers**
- **Found during:** Task 1 (first build attempt after CMake registration)
- **Issue:** `Source/UI/MidiRowView.h` and `Source/UI/MidiSetsPanel.h` (left over from the prior cut-off session) included `"MidiGen/MidiSetRow.h"`, which resolves relative to `Source/UI/` and does not exist -- build failed with `fatal error: 'MidiGen/MidiSetRow.h' file not found`
- **Fix:** Changed both includes to `"../MidiGen/MidiSetRow.h"`, matching the existing `ChordTimelineView.h` pattern for reaching sibling `Source/` subdirectories from `Source/UI/`
- **Files modified:** Source/UI/MidiRowView.h, Source/UI/MidiSetsPanel.h
- **Verification:** `cmake --build build` succeeded after the fix; full suite green
- **Committed in:** ca99ec6 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Required to get the recovered skeleton files compiling at all; no scope change, no architectural impact.

## Issues Encountered
- Recovery session: 6 untracked files from a prior cut-off agent were assessed first per the recovery instructions. `MidiRowLayout.h` and `Tests/MidiSetsPanelLayoutTests.cpp` (Task 1 deliverables) matched the plan's exact spec and needed no changes. `MidiRowView.h/.cpp` and `MidiSetsPanel.h/.cpp` were genuine minimal skeletons (empty `paint()` bodies) exactly as Task 1's instructions describe -- fleshed out per Task 2 as planned, after fixing the include-path bug above. CMakeLists.txt and PluginEditor were confirmed unmodified as stated, so Tasks 1 and 3's CMake/editor edits were applied fresh in this session.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- GEN-01's visible half now fully evidenced: 5 labeled mini piano-roll strips render real generated rows in the bottom band, one shared time axis, pixel-art palette, blank on fresh load, restore on editor reopen, always in sync with the chord timeline
- `MidiRowView::getRow()` and the display-only `setInterceptsMouseClicks(false, false)` are the exact hooks 05-06/Phase 6 need to flip on audition/drag without reshaping this API
- Full suite (109/109) and pluginval strictness 5 (VST3 + AU) green
- No blockers identified; visual legibility itself is deferred to plan 05-06's human checkpoint per this plan's own verification note

---
*Phase: 05-midi-conveyor-generation*
*Completed: 2026-07-13*

## Self-Check: PASSED

All 9 created/modified source and test files verified present on disk; both deleted `MidiSetsPlaceholder.h`/`.cpp` files confirmed absent. All 3 task commit hashes (ca99ec6, ac34a85, 7df5466) verified present in `git log`. Full suite verified 109/109 green (`ctest --test-dir build --output-on-failure`); pluginval strictness 5 verified SUCCESS for both VST3 and AU.
