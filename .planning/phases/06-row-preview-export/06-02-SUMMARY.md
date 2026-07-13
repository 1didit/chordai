---
phase: 06-row-preview-export
plan: 02
subsystem: audio
tags: [rt-safety, lock-free, adsr, audition, juce-audiobuffer, processblock]

# Dependency graph
requires:
  - phase: 06-row-preview-export
    provides: "06-01 MidiFileWriter core / MidiSetRow+NoteEvent beat-domain model"
provides:
  - "AuditionRenderer::render -- deterministic message-thread MidiSetRow -> mono PCM buffer"
  - "AuditionVoice::render -- deterministic decaying piano-ish voice (oscillator+filter+ADSR)"
  - "ChordAIAudioProcessor::startAudition/stopAudition/isAuditionPlaying/getAuditionRowId -- RT-safe playback API"
  - "Preallocated double-buffer + plain-atomics audio-thread handoff pattern, first use of processBlock beyond the Phase 1 no-op"
affects: [06-03, 06-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Lock-free double-buffer handoff for message-thread-rendered audio into processBlock: preallocated juce::AudioBuffer<float>[2] + plain std::atomic<int>/<bool> index/length/pos/playing -- NOT this codebase's shared_ptr atomic_load/atomic_store idiom, which is unsafe on the audio thread (06-RESEARCH.md Pitfall 2)"
    - "Deterministic audition voice: 0.6*triangle + 0.4*naive-saw oscillator, one-pole ~2.5kHz lowpass, juce::ADSR (5ms/400ms/0.25/150ms) -- every stage individually bounded to [-1,1] by construction, no randomness"
    - "Second (and last-planned) sanctioned beat->seconds conversion point in the codebase: AuditionRenderer's note startBeats/lengthBeats * (60/bpm) * sampleRate -- mirrors 06-01's beat->tick conversion, never re-derived elsewhere"

key-files:
  created:
    - Source/Audio/AuditionVoice.h
    - Source/Audio/AuditionVoice.cpp
    - Source/Audio/AuditionRenderer.h
    - Source/Audio/AuditionRenderer.cpp
    - Tests/AuditionRendererTests.cpp
    - Tests/ProcessorAuditionTests.cpp
  modified:
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - CMakeLists.txt

key-decisions:
  - "New Source/Audio/ folder (not Source/MidiGen/) for AuditionVoice/AuditionRenderer since they need juce::AudioBuffer/juce::ADSR, preserving MidiGen's 'pure value types, no JUCE audio dependency' folder-boundary rule -- per 06-RESEARCH.md Open Question 2's own recommendation"
  - "Test-harness fix: ProcessorAuditionTests.cpp calls setRateAndBufferSizeDetails() before prepareToPlay() on every test processor, matching real JUCE plugin-wrapper behavior (VST3/AU/Standalone/AAX all do this) -- a bare prepareToPlay() call leaves getSampleRate() at 0, which is a test-setup gap, not a production bug"

patterns-established:
  - "Preallocated double-buffer + plain-atomics is now the project's sanctioned pattern for any future message-thread-render-into-audio-thread-playback feature; the shared_ptr atomic_load/atomic_store idiom stays reserved for message-thread-to-message-thread publication only"

requirements-completed: [PRV-01]

# Metrics
duration: ~11min
completed: 2026-07-13
---

# Phase 6 Plan 02: Audition Engine (AuditionRenderer + RT-safe Processor Playback) Summary

**Deterministic message-thread MidiSetRow-to-PCM renderer (triangle/saw oscillator + one-pole filter + juce::ADSR voice) wired into processBlock via a preallocated double-buffer and plain std::atomic handoff -- zero allocation/locks/shared_ptr on the audio thread, pluginval strictness 5 green on VST3 and AU.**

## Performance

- **Duration:** ~11 min
- **Started:** 2026-07-13T14:15:55Z
- **Completed:** 2026-07-13T14:27:12Z
- **Tasks:** 2 completed (both TDD red -> green)
- **Files modified:** 9 (6 created, 3 modified)

## Accomplishments
- `AuditionVoice::render(pitch, velocity, noteOnSamples, sampleRate)` produces a deterministic, finite, [-1,1]-bounded mono note buffer: 0.6*triangle + 0.4*naive-saw oscillator at the note's equal-temperament frequency, through a one-pole ~2.5kHz lowpass, shaped by `juce::ADSR` (attack 5ms, decay 400ms, sustain 0.25, release 150ms); the release stage's time-bounded decay rate guarantees termination without relying on the added hard safety cap
- `AuditionRenderer::render(row, bpm, sampleRate)` sizes a mono buffer from the row's last note-end + `kReleaseTailSeconds` (0.2s), additively mixes every note's voice at its beat-derived sample offset, applies a 0.5 overall preview gain, and normalizes DOWN only (peak > 0.9 -> scale to 0.9) so quiet rows stay quiet
- `ChordAIAudioProcessor` gained `startAudition`/`stopAudition`/`isAuditionPlaying`/`getAuditionRowId`: `startAudition` renders into the currently-inactive buffer slot on the message thread and publishes index/length/playing via release-ordered atomic stores; `processBlock` only ever does an acquire-load + bounds-checked `addFrom` copy + relaxed position advance, auto-stopping when the buffer is exhausted
- `prepareToPlay` now calls `stopAudition()` so a sample-rate change can never play back a stale-rate buffer
- Which row is playing lives on the processor (`getAuditionRowId`), not on any `MidiRowView` -- ready for 06-03's UI wiring, which will be destroyed on every row regeneration (06-RESEARCH.md Pitfall 3)
- 7 new `[auditionrenderer]` tests + 5 new `[processoraudition]` tests, full suite 132/132 green; pluginval strictness 5 SUCCESS on both VST3 and AU (first wave to touch `processBlock`)

## Task Commits

Each task was committed atomically (TDD red -> green):

1. **Task 1: AuditionVoice + AuditionRenderer**
   - `66e03b7` test(06-02): add failing AuditionRenderer tests + Source/Audio CMake wiring (RED)
   - `3bc0c52` feat(06-02): implement AuditionVoice + deterministic AuditionRenderer (GREEN)
2. **Task 2: Processor double-buffer playback**
   - `9c7c81c` test(06-02): add failing processor audition playback tests (RED)
   - `a4b753b` feat(06-02): RT-safe audition playback via preallocated double-buffer in processBlock (GREEN)

**Plan metadata:** (this commit) docs(06-02): complete audition engine plan

## Files Created/Modified
- `Source/Audio/AuditionVoice.h` / `.cpp` - Deterministic one-note synth voice (oscillator + filter + ADSR)
- `Source/Audio/AuditionRenderer.h` / `.cpp` - Whole-row pre-renderer (mix + gain + normalize-down)
- `Tests/AuditionRendererTests.cpp` - 7 `[auditionrenderer]` tests (determinism, sample-count formula, finiteness, no-clip, empty-row, bpm-fallback, audibility floor)
- `Tests/ProcessorAuditionTests.cpp` - 5 `[processoraudition]` tests (playback, auto-stop, stopAudition, restart-while-playing, prepareToPlay-stops)
- `Source/PluginProcessor.h` - Public audition API + double-buffer/atomic private members, documented against the shared_ptr-atomics idiom
- `Source/PluginProcessor.cpp` - `startAudition`/`stopAudition`/`isAuditionPlaying`/`getAuditionRowId` implementations; `prepareToPlay` stops stale audition; `processBlock` gains the RT-safe mix-in read path
- `CMakeLists.txt` - New `CHORDAI_AUDIO_SOURCES` list wired into both `ChordAI`/`ChordAITests` targets; both new test files added to `ChordAITests`

## Decisions Made
- `Source/Audio/` introduced as a new folder (not `Source/MidiGen/`) since `AuditionVoice`/`AuditionRenderer` need `juce::AudioBuffer`/`juce::ADSR`, keeping `MidiGen/`'s "pure value types, no JUCE audio dependency" rule intact -- per 06-RESEARCH.md Open Question 2's own recommendation.
- Voice-character constants (oscillator mix, filter cutoff, ADSR times, overall gain/normalize threshold) set at the plan's suggested defaults; the human checkpoint in 06-04 is the actual judge of the sound, not this plan.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Test harness didn't replicate real-host prepareToPlay contract**
- **Found during:** Task 2 GREEN verification (`ChordAITests "[processoraudition]"` run)
- **Issue:** `Tests/ProcessorAuditionTests.cpp` called `proc.prepareToPlay(sampleRate, blockSize)` directly, but `juce::AudioProcessor::getSampleRate()` is only populated by a separate `setRateAndBufferSizeDetails()` call, which every real JUCE plugin wrapper (VST3/AU/Standalone/AAX, verified in `external/JUCE/modules/juce_audio_plugin_client/`) makes before invoking `prepareToPlay()`. Without it, `startAudition`'s `getSampleRate()` call returned 0, tripping `AuditionRenderer::render`'s `jassert (sampleRate > 0.0)`.
- **Fix:** Added `proc.setRateAndBufferSizeDetails(sampleRate, blockSize)` immediately before each `proc.prepareToPlay(...)` call in the test file, matching real host behavior. No production code change -- `startAudition`'s reliance on `getSampleRate()` is correct and matches 06-RESEARCH.md Pattern 3 exactly.
- **Files modified:** `Tests/ProcessorAuditionTests.cpp`
- **Verification:** All 5 `[processoraudition]` tests pass; full suite 132/132 green.
- **Committed in:** `a4b753b` (Task 2 GREEN commit)

---

**Total deviations:** 1 auto-fixed (1 blocking, test-only)
**Impact on plan:** Zero production-code impact. No scope creep.

## Issues Encountered
None beyond the one auto-fixed test-harness gap above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- PRV-01's engine half is complete: any `MidiSetRow` can be rendered deterministically and played back RT-safely with play/stop/auto-stop semantics
- 06-03's UI wiring can call `startAudition`/`stopAudition`/`isAuditionPlaying`/`getAuditionRowId` directly; the playing-row identity already lives on the processor, safe across `MidiSetsPanel::setRows()`'s unconditional `MidiRowView` teardown-and-rebuild (06-RESEARCH.md Pitfall 3) -- 06-03 still needs to call `stopAudition()` at the top of `setRows()` per that pitfall's guidance, since this plan only builds the engine, not the UI hook
- The double-buffer + plain-atomics pattern is documented in-code (`PluginProcessor.h`/`.cpp` comments) as the sanctioned audio-thread handoff for any future RT-safe feature
- Full suite green: 132/132 (120 baseline + 7 `[auditionrenderer]` + 5 `[processoraudition]`), zero regressions; pluginval strictness 5 SUCCESS on VST3 and AU (mandatory this wave, first `processBlock`-touching change)

---
*Phase: 06-row-preview-export*
*Completed: 2026-07-13*

## Self-Check: PASSED

All 6 created source/test files found on disk; all 4 task commit hashes (66e03b7, 3bc0c52, 9c7c81c, a4b753b) found in git log.
