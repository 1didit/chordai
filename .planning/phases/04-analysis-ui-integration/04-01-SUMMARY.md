---
phase: 04-analysis-ui-integration
plan: 01
subsystem: audio-analysis
tags: [juce, threadpool, threadpooljob, cancellation, atomic-shared_ptr, background-thread, cross-thread-messaging]

# Dependency graph
requires:
  - phase: 03-core-chord-detection-engine
    provides: "Frozen ChordAnalyzer/AnalysisResult contracts + synchronous, headless, cancellable ClassicDspChordAnalyzer::analyse()"
provides:
  - "AnalysisPipeline (juce::ThreadPoolJob) wrapping ClassicDspChordAnalyzer::analyse with a CancelToken adapter and generation-tagged callAsync callbacks"
  - "PluginProcessor background-analysis API: triggerAnalysis(), getAnalysisResult(), isAnalyzing(), getAnalysisProgress(), analysisBroadcaster"
  - "Generation-guarded cancel-and-restart pattern (analysisPool.removeAllJobs + atomic<uint64_t> generation) -- reusable verbatim by Phase 5's GEN-04"
affects: [05-generation, 06-preview-export]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Second juce::ThreadPool (analysisPool, size 1) kept separate from loaderPool -- avoids decode/analysis queue contention and keeps removeAllJobs's cancellation scope isolated"
    - "Generation counter (std::atomic<uint64_t>) tags every triggerAnalysis() call; progress/completion callbacks compare captured generation against the live counter and silently discard on mismatch -- the mechanism for supersedable background jobs"
    - "Busy flag set synchronously inside triggerAnalysis() (before the new job even starts running) so there's no visible gap between supersede and the new job's first callback, since ThreadPool cancellation is cooperative, not instant"

key-files:
  created:
    - Source/Analysis/AnalysisPipeline.h
    - Source/Analysis/AnalysisPipeline.cpp
    - Tests/AnalysisPipelineTests.cpp
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - CMakeLists.txt

key-decisions:
  - "analysisPool declared as a second size-1 juce::ThreadPool member (not reusing loaderPool) so a new-file-drop mid-re-analysis never queues behind a still-cancelling analysis job, and removeAllJobs's cancellation scope never touches an unrelated decode job"
  - "setSelectedRegion gained a no-op guard (clamped == selectedRegion) before triggering analysis, since RegionSelectorOverlay::setTotalLength refires the whole-file default on every editor reopen"
  - "loadAudioFile's success branch clears analysisResult (atomic_store nullptr) before broadcasting so a freshly-loaded song never briefly shows the previous song's chords"
  - "ChordAITests target gained juce_audio_processors + juce_gui_extra links, PluginProcessor/PluginEditor/UI component sources, and a JucePlugin_Name define -- needed for the test file to construct a real ChordAIAudioProcessor via its public API (previously only DSP-layer headless code was tested)"

patterns-established:
  - "Generation-guarded cancel-and-restart: analysisPool.removeAllJobs(true, 0) (non-blocking cooperative-cancel signal, never track a raw ThreadPoolJob*) + ++analysisGeneration + synchronous busy-flag set + generation comparison inside both progress and completion callbacks"

requirements-completed: [ANL-04]

# Metrics
duration: ~20min
completed: 2026-07-13
---

# Phase 4 Plan 01: Background Analysis Pipeline Summary

**Cancellable, restartable background chord-analysis pipeline (juce::ThreadPoolJob) wired into PluginProcessor with a generation-guarded cancel-and-restart mechanism, auto-triggered on file load and real region changes.**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-07-13T09:48Z (approx, prior commit timestamp)
- **Completed:** 2026-07-13T10:07Z
- **Tasks:** 3 (2 code tasks + 1 verification-only regression gate)
- **Files modified:** 6 (3 created, 3 modified)

## Accomplishments
- `AnalysisPipeline` (`juce::ThreadPoolJob`) wraps the frozen, already-verified `ClassicDspChordAnalyzer::analyse()` with a one-line `CancelToken` adapter (`shouldExit()`) and `MessageManager::callAsync` marshalling for both progress and completion, tagged with a generation id
- `ChordAIAudioProcessor` gained a full background-analysis public API: `triggerAnalysis()`, `getAnalysisResult()`, `isAnalyzing()`, `getAnalysisProgress()`, `analysisBroadcaster` -- all message-thread-only, mirroring the existing `loadAudioFile`/`getLoadedAudio` idiom
- Analysis auto-triggers on successful file load (after clearing the stale result) and on real (non-no-op) region changes; a rapid double region-change publishes only the last region's result, proven by `AnalysisPipelineTests.CancelAndRestart`
- Full regression suite green (68/68: 64 pre-existing + 4 new `AnalysisPipelineTests`), pluginval strictness 5 green for both VST3 and AU with the new pipeline wired in
- Frozen contracts (`ChordAnalyzer.h`, `AnalysisResult.h`) verified byte-identical (`git diff --stat` empty against both)

## Task Commits

Each task was committed atomically:

1. **Task 1: Wave 0 -- test scaffold, API surface stubs, CMake wiring (RED)** - `6a6d123` (test)
2. **Task 2: Implement AnalysisPipeline + generation-guarded triggerAnalysis (GREEN)** - `500feb8` (feat)
3. **Task 3: Wave regression gate -- pluginval strictness 5 (VST3 + AU)** - verification only, no files modified, no commit

**Plan metadata:** (this commit) `docs(04-01): complete background-analysis-pipeline plan`

_Note: TDD Task 1 (RED) and Task 2 (GREEN) form the standard two-commit TDD cycle; no REFACTOR commit was needed since Task 2's implementation matched 04-RESEARCH.md's pre-verified patterns with no cleanup pass required._

## Files Created/Modified
- `Source/Analysis/AnalysisPipeline.h` - `ThreadPoolJob` subclass declaration: owns an immutable `shared_ptr<const LoadedAudio>`/region/generation snapshot + progress/completion callback types
- `Source/Analysis/AnalysisPipeline.cpp` - `runJob()`: `CancelToken` adapter, calls `ClassicDspChordAnalyzer::analyse()` on the background thread, marshals both progress and result back via `callAsync`
- `Source/PluginProcessor.h` - new `analysisPool` (size-1, separate from `loaderPool`), `analysisResult` (atomic shared_ptr), `analysisGeneration` (atomic uint64_t), `analyzingFlag`/`analysisProgress` (message-thread-only), public API declarations
- `Source/PluginProcessor.cpp` - `triggerAnalysis()` implementation (cancel + generation bump + synchronous busy flag + job submission), `getAnalysisResult()`/`isAnalyzing()`/`getAnalysisProgress()`, trigger wiring in `loadAudioFile` (clear-then-trigger) and `setSelectedRegion` (no-op guard then trigger)
- `Tests/AnalysisPipelineTests.cpp` - `AutoAnalyzeOnLoad`, `CancelAndRestart`, `NoOpRegionDoesNotRetrigger`, `ClearedOnNewLoad`, all driving `ChordAIAudioProcessor`'s public API with the pumped-message-loop technique from `WaveformRegionTests.ThumbnailPopulates`
- `CMakeLists.txt` - `AnalysisPipeline.cpp` added to both targets; `ChordAITests` gained `PluginProcessor.cpp`/`PluginEditor.cpp`/UI component sources, `juce_audio_processors`/`juce_gui_extra` links, and a `JucePlugin_Name="ChordAI"` define

## Decisions Made
- `analysisPool` kept as its own size-1 `juce::ThreadPool`, not sharing `loaderPool` -- prevents a new-file-drop from queueing behind a still-cancelling analysis job and keeps `removeAllJobs`'s cancellation scope isolated from decode jobs (per 04-RESEARCH.md's own "Alternatives Considered" analysis)
- `setSelectedRegion`'s no-op guard compares the *clamped* region against the current `selectedRegion` before triggering, correctly absorbing `RegionSelectorOverlay::setTotalLength`'s unconditional refire on every editor reopen
- `analysisResult` is cleared (published `nullptr`) inside `loadAudioFile`'s success branch, before the `loadBroadcaster` broadcast -- guarantees the Editor never observes a stale chord result alongside a freshly-loaded waveform

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `ChordAITests` target was missing `juce::juce_audio_processors`**
- **Found during:** Task 1 (CMake wiring step, before first build attempt)
- **Issue:** The plan's CMake instructions listed `PluginProcessor.cpp`/`PluginEditor.cpp`/UI sources to add to `ChordAITests`, and flagged `juce_gui_extra` as a conditional add "if link errors surface," but `ChordAIAudioProcessor` (via `juce::AudioProcessor`/`juce::AudioProcessorValueTreeState`) requires the `juce_audio_processors` module, which was linked only into the `ChordAI` target, not `ChordAITests`. Without it, `PluginProcessor.h` would fail to compile in the test target (undefined `juce::AudioProcessor` base class).
- **Fix:** Added `juce::juce_audio_processors` to `target_link_libraries(ChordAITests ...)` alongside the plan's own conditional `juce::juce_gui_extra` addition (both added proactively; build then succeeded cleanly with zero link errors on the first attempt).
- **Files modified:** `CMakeLists.txt`
- **Verification:** `cmake --build build` succeeded with no undefined-symbol or missing-header errors.
- **Committed in:** `6a6d123` (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Necessary for the plan's own CMake wiring step to actually compile; no scope creep -- this is the exact class of link fix the plan itself anticipated ("If link errors surface for gui symbols, add...").

## Issues Encountered
None -- both GREEN and the pluginval regression gate passed on the first attempt, no debugging iterations needed.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- The generation-guarded cancel-and-restart mechanism (`analysisPool.removeAllJobs` + `analysisGeneration`) is proven and ready for Phase 5's GEN-04 (regenerate rows on region change) to reuse verbatim
- `PluginProcessor::getAnalysisResult()`/`isAnalyzing()`/`getAnalysisProgress()`/`analysisBroadcaster` are ready for Plan 04-02 (or a later Phase 4 plan) to wire into the Editor's chord-timeline display and progress UI
- No blockers; ANL-04 ("analysis runs on a background thread, UI stays responsive, progress is shown") is now fully evidenced by automated tests for the cancel/restart/no-op-guard semantics -- the remaining manual-only checkpoints (Standalone responsiveness during a real 3-minute analysis, progress-indicator visibility, Release-build timing) are deferred to this phase's own checkpoint plan per 04-VALIDATION.md, not this plan's scope

---
*Phase: 04-analysis-ui-integration*
*Completed: 2026-07-13*

## Self-Check: PASSED

All created files and both task commits (`6a6d123`, `500feb8`) verified present on disk / in git history.
