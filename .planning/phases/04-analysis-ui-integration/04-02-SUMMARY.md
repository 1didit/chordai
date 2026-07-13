---
phase: 04-analysis-ui-integration
plan: 02
subsystem: ui
tags: [juce, component, chord-display, pixel-math, change-broadcaster]

# Dependency graph
requires:
  - phase: 04-analysis-ui-integration
    provides: "Plan 04-01's PluginProcessor background-analysis API: triggerAnalysis()/getAnalysisResult()/isAnalyzing()/getAnalysisProgress()/analysisBroadcaster"
provides:
  - "ChordNameFormatter.h: shared pure chordName(ChordSymbol) -> juce::String, frozen '' / 'm' / '7' / 'N.C.' convention"
  - "ChordTimelineView: read-only Component rendering named, coloured ChordSegment blocks on a dedicated 28px band above the waveform"
  - "PluginEditor wiring: analysisBroadcaster subscription pushes processor.getAnalysisResult() into the timeline on every trigger/progress/completion event, plus editor-reopen restore via handleLoadComplete"
affects: [04-analysis-ui-integration (remaining Phase 4 plans), 05-generation, 06-preview-export]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Free-function pixel/label-guard extraction for unit-testability without a Component (shouldDrawLabel), mirroring WaveformMath.h's own timeToX/xToTime extraction"
    - "Dedicated read-only display band (setInterceptsMouseClicks(false, false)) placed in its own layout slice, avoiding any z-order/mouse-ownership conflict with RegionSelectorOverlay"
    - "ChangeBroadcaster source-branching in a single changeListenerCallback (source == &processor.analysisBroadcaster vs loadBroadcaster) rather than a second listener method"

key-files:
  created:
    - Source/UI/ChordNameFormatter.h
    - Source/UI/ChordTimelineView.h
    - Source/UI/ChordTimelineView.cpp
    - Tests/ChordNameFormatterTests.cpp
    - Tests/ChordTimelineLayoutTests.cpp
  modified:
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - CMakeLists.txt

key-decisions:
  - "chordName lifted verbatim from Tests/ClassicDspChordAnalyzerTests.cpp's RealTrackHarness lambda into a header-only shared function, consumed by both the UI and (implicitly, by shared convention) the test harness"
  - "ChordTimelineView given its own dedicated 28px band carved from the top of the middle layout area (before waveformArea is assigned), rather than overlaying the waveform, to avoid any z-order/mouse-interception conflict with RegionSelectorOverlay"
  - "analysisBroadcaster and loadBroadcaster both route through the single existing changeListenerCallback, branching on the broadcaster pointer, rather than adding a second override"

requirements-completed: [ANL-05]

# Metrics
duration: ~7min
completed: 2026-07-13
---

# Phase 4 Plan 02: Chord Timeline Display Summary

**Named, coloured chord-segment blocks (C / Am / G7 / N.C. convention) render on a dedicated 28px band above the waveform, pixel-aligned via the existing WaveformMath::timeToX, restoring from the processor's published AnalysisResult on both fresh analysis and editor reopen.**

## Performance

- **Duration:** ~7 min
- **Started:** 2026-07-13T10:11:46Z
- **Completed:** 2026-07-13T10:17:05Z
- **Tasks:** 3
- **Files modified:** 8 (5 created, 3 modified)

## Accomplishments
- `ChordNameFormatter.h` provides one shared pure `chordName(ChordSymbol)` function implementing the frozen `""`/`"m"`/`"7"`/`"N.C."` convention, exhaustively tested across all 4 `ChordQuality` values x all 12 pitch classes (39 assertions)
- `ChordTimelineView` is a read-only `juce::Component` that paints `ChordSegment` blocks via `WaveformMath::timeToX` reused verbatim (no reimplementation), with a `shouldDrawLabel` free-function label-collision guard (default 24px threshold) suppressing garbled text on narrow segments
- `ChordTimelineView` never intercepts mouse input (`setInterceptsMouseClicks(false, false)`) — `RegionSelectorOverlay` keeps exclusive ownership of region-drag interaction, verified by unchanged `waveformArea`/`regionOverlay` bounds
- `PluginEditor` wires `chordTimeline` into a new 28px band above the waveform and subscribes to `processor.analysisBroadcaster`; the timeline restores from `processor.getAnalysisResult()` on editor reopen (`handleLoadComplete`) without triggering re-analysis
- Full regression suite green (72/72: 68 pre-existing + 4 new across `ChordNameFormatterTests`/`ChordTimelineLayoutTests`), pluginval strictness 5 green for both VST3 and AU with the new band wired in

## Task Commits

Each task was committed atomically:

1. **Task 1: ChordNameFormatter — pure function + exhaustive tests (TDD)** - `fcff899` (feat)
2. **Task 2: ChordTimelineView component + layout tests (TDD)** - `5c35be3` (feat)
3. **Task 3: PluginEditor integration — band layout + analysisBroadcaster wiring** - `c574182` (feat)

**Plan metadata:** (this commit) `docs(04-02): complete chord-timeline-display plan`

_Note: each TDD task's RED (compile failure with the header/component missing, CMake source list wired first) was confirmed via a real build failure before the GREEN implementation was written, per the plan's own TDD flow._

## Files Created/Modified
- `Source/UI/ChordNameFormatter.h` - header-only `inline juce::String chordName(const ChordSymbol&)`, lifted verbatim from the RealTrackHarness lambda
- `Source/UI/ChordTimelineView.h` - `shouldDrawLabel` free function + `ChordTimelineView : juce::Component` declaration (`setResult`/`setTotalLength`, no mouse handling)
- `Source/UI/ChordTimelineView.cpp` - `paint()`: bails on null result/zero length; per-segment `timeToX` block fill + darker boundary line + conditional centred label; `colourForQuality` palette matching `ConveyorBeltComponent`'s muted flat scheme
- `Tests/ChordNameFormatterTests.cpp` - loops over all 12 pitch classes x 3 pitched qualities + 3 NoChord spot checks (39 mappings)
- `Tests/ChordTimelineLayoutTests.cpp` - label-guard default/custom threshold boundaries + segment pixel-span sanity check via `timeToX`
- `Source/PluginEditor.h` - `chordTimeline` member, `ChordTimelineView.h` include
- `Source/PluginEditor.cpp` - ctor subscribes to `analysisBroadcaster`; dtor unsubscribes; `resized()` carves the 28px band before `waveformArea`; `changeListenerCallback` branches on broadcaster source; `handleLoadComplete` sets total length + restores result
- `CMakeLists.txt` - `ChordTimelineView.cpp` added to both `ChordAI` and `ChordAITests` targets; both new test files added to `ChordAITests`

## Decisions Made
- `chordName` extracted as header-only (no `.cpp`) since it's a small pure function with no state — matches `WaveformMath.h`'s own header-only precedent
- `shouldDrawLabel` kept as a free function (not a private method) specifically so `ChordTimelineLayoutTests` can exercise it without constructing a `juce::Component`
- Boundary line drawn as a 1px `darker(0.4f)` variant of the block's own fill colour rather than a fixed separate colour, so it stays visually coherent across all 4 quality colours

## Deviations from Plan

None - plan executed exactly as written. All three tasks matched 04-RESEARCH.md's Pattern 4/5 examples directly; no auto-fixes, no architectural questions, no CMake surprises (Plan 04-01 had already proven the `ChordAITests` link surface needed for `PluginEditor.cpp`).

## Issues Encountered
None - both TDD cycles (RED confirmed via real build failure, then GREEN) and the final integration task passed on the first attempt; full suite and both pluginval runs (VST3 + AU, strictness 5) were green on the first run.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- ANL-05 fully evidenced by automated tests (formatter + layout-guard coverage) and the pluginval Editor Automation pass (which opens/closes the editor with analysis state present) — visual legibility itself is deferred to plan 04-04's human checkpoint per the plan's own scope note (no visual-regression tooling exists in this project)
- `ChordTimelineView`/`ChordNameFormatter` are ready for any later Phase 4/5 plan needing to display chord data elsewhere (e.g. a future MIDI-row preview referencing the same segment data)
- No blockers

---
*Phase: 04-analysis-ui-integration*
*Completed: 2026-07-13*

## Self-Check: PASSED

All created files (`ChordNameFormatter.h`, `ChordTimelineView.h`, `ChordTimelineView.cpp`, `ChordNameFormatterTests.cpp`, `ChordTimelineLayoutTests.cpp`) and all three task commits (`fcff899`, `5c35be3`, `c574182`) verified present on disk / in git history.
