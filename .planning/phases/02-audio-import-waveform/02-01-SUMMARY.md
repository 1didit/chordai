---
phase: 02-audio-import-waveform
plan: 01
subsystem: audio-import
tags: [juce, catch2, ctest, threadpool, apvts, audio-formats, tdd]

# Dependency graph
requires:
  - phase: 01-plugin-foundation
    provides: APVTS-based PluginProcessor skeleton with getState/setStateInformation XML round-trip already wired
provides:
  - Catch2 v3.7.1 + CTest test infrastructure (ChordAITests console-app target, first in the project)
  - loadAudioFileSync free function decoding WAV/AIFF/FLAC/MP3 into an immutable LoadedAudio value type
  - AudioFileLoadJob (ThreadPool background decode wrapper, message-thread-safe completion delivery)
  - RegionState namespace (ValueTree read/write/clamp helpers for the analysis region)
  - ChordAIAudioProcessor::loadAudioFile/getLoadedAudio/get+setSelectedRegion/getLastLoadError/loadBroadcaster public API
affects: [02-02-conveyor-ui, 02-03-waveform-display, 02-04-regression-gate, 07-persistence]

# Tech tracking
tech-stack:
  added: ["Catch2 v3.7.1 (FetchContent)", "CTest"]
  patterns:
    - "Testable-core + thin-async-wrapper: loadAudioFileSync (pure, testable) wrapped by AudioFileLoadJob (ThreadPoolJob) for background use"
    - "Atomic shared_ptr publication via std::atomic_load/atomic_store (not std::atomic<shared_ptr<T>> — incomplete on Apple libc++)"
    - "weak_ptr<int> aliveToken guard on async callAsync callbacks to make post-destruction delivery a safe no-op"
    - "Custom non-parameter properties written directly onto apvts.state so they ride along the existing XML getState/setState round-trip for free"

key-files:
  created:
    - Source/Import/LoadedAudio.h
    - Source/Import/AudioFileLoader.h
    - Source/Import/AudioFileLoader.cpp
    - Source/Import/RegionState.h
    - Tests/InfrastructureTests.cpp
    - Tests/AudioFileLoaderTests.cpp
    - Tests/RegionStateTests.cpp
    - Tests/fixtures/silence_1s.mp3
  modified:
    - CMakeLists.txt
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp

key-decisions:
  - "RegionState::clampRegion takes raw (double, double) endpoints, not juce::Range<double> — Range's own constructor forces end = jmax(start, end) at construction time, so an inverted pair like {8.0, 2.0} silently collapses to {8.0, 8.0} before a Range-typed parameter could ever observe the inversion. A Range-typed convenience overload delegates to the raw-double version for already-ordered callers."
  - "AIFF test fixture written at 16-bit, not 32-bit float — JUCE's AiffAudioFormat writer only supports 8/16/24-bit (getPossibleBitDepths), unlike WAV which accepts 32-bit float directly."
  - "MP3 fixture committed to git (ffmpeg-generated, ~17KB) rather than generated at test time — JUCE cannot write MP3 (CoreAudioFormat::createWriterFor is a stub) and afconvert has no MP3 encoder either."

patterns-established:
  - "Pattern: message-thread-only processor API — loadAudioFile/getLoadedAudio/get+setSelectedRegion are documented and enforced by convention as message-thread-only; processBlock never touches any of this state, keeping RT-safety discipline established in Phase 1 intact."
  - "Pattern: ThreadPool member declared after the members its jobs reference (formatManager, apvts) so C++'s reverse-declaration-order destruction runs the pool's job-draining destructor before those dependencies are torn down."

requirements-completed: []  # IMP-01 backend decode path proven by this plan; full IMP-01 (drag-and-drop UI) completes in Plan 02-02/02-03 — not marked complete yet, see Next Phase Readiness.

# Metrics
duration: 9min
completed: 2026-07-12
---

# Phase 2 Plan 01: Test Infrastructure + Audio Import Backend Summary

**Catch2/CTest test harness (first in the project) plus a background-decoded, ThreadPool-backed audio import path (WAV/AIFF/FLAC/MP3) with the analysis region persisted as custom APVTS state properties.**

## Performance

- **Duration:** ~9 min (commit-to-commit span)
- **Started:** 2026-07-12T19:24:14+01:00
- **Completed:** 2026-07-12T19:32:33+01:00
- **Tasks:** 3 (Task 2 and 3 followed TDD red→green)
- **Files modified:** 11 (8 created, 3 modified)

## Accomplishments
- Wired the project's first test infrastructure: Catch2 v3.7.1 via FetchContent, a `ChordAITests` console-app CMake target, and `catch_discover_tests` CTest wiring with a `ChordAITests.` prefix
- Committed an ffmpeg-generated MP3 fixture (`Tests/fixtures/silence_1s.mp3`, confirmed MPG3 via `afinfo`) since neither JUCE nor `afconvert` can encode MP3
- `loadAudioFileSync` decodes WAV/AIFF/FLAC/MP3 into an immutable `LoadedAudio` value type; returns `nullptr` on unsupported/corrupt input without crashing — all 5 decode test cases green
- `AudioFileLoadJob` wraps the sync core for background use on a single-worker `juce::ThreadPool`, delivering results (or a failure message) to the message thread via `MessageManager::callAsync`
- `RegionState` namespace: pure ValueTree helpers (`write`/`readSourceFile`/`readRegion`/`clampRegion`) proven to survive an XML round-trip — all 4 test cases green
- `ChordAIAudioProcessor` now exposes `loadAudioFile`/`getLoadedAudio`/`get+setSelectedRegion`/`getLastLoadError`/`loadBroadcaster` for the editor to consume in Plans 02-02/02-03
- Full `ChordAITests` suite: 10/10 green; pluginval strictness 5: SUCCESS on both VST3 and AU with the new backend linked in; `processBlock` verified byte-for-byte unchanged

## Task Commits

Each task was committed atomically (Tasks 2 and 3's TDD portions produced separate RED/GREEN commits):

1. **Task 1: Wire Catch2 + CTest, commit MP3 fixture, link juce_audio_formats explicitly** - `2d57449` (chore)
2. **Task 2a: Failing AudioFileLoader decode tests (RED)** - `4fe92fa` (test)
2. **Task 2b: AudioFileLoader sync decode + ThreadPoolJob wrapper (GREEN)** - `d58a991` (feat, includes AIFF-bit-depth auto-fix)
3. **Task 3a: Failing RegionState tests (RED)** - `4e799c3` (test)
3. **Task 3b: RegionState ValueTree helpers (GREEN)** - `686268c` (feat, includes clampRegion signature auto-fix)
3. **Task 3c: PluginProcessor load/region API wiring** - `ca0d5c8` (feat)

_TDD tasks produced test → feat commit pairs; no refactor commits were needed (implementations were clean on first GREEN)._

## Files Created/Modified
- `CMakeLists.txt` - Catch2 FetchContent + ChordAITests target/CTest wiring; explicit `juce::juce_audio_formats` link on ChordAI; AudioFileLoader.cpp added to both targets
- `Tests/InfrastructureTests.cpp` - Sanity test proving Catch2 + JUCE headers + link work
- `Tests/fixtures/silence_1s.mp3` - Committed MP3 decode fixture (ffmpeg-generated, ~1.045s)
- `Tests/AudioFileLoaderTests.cpp` - WAV/AIFF/FLAC/MP3 decode tests + unsupported-file-fails test
- `Tests/RegionStateTests.cpp` - Write/read roundtrip, XML survival, clamp, and defaults tests
- `Source/Import/LoadedAudio.h` - Immutable decode result value type (sourceFile, buffer, sampleRate, lengthSeconds)
- `Source/Import/AudioFileLoader.h` / `.cpp` - `loadAudioFileSync` (testable core) + `AudioFileLoadJob` (ThreadPoolJob async wrapper)
- `Source/Import/RegionState.h` - ValueTree property read/write/clamp helpers for the analysis region
- `Source/PluginProcessor.h` / `.cpp` - load/region public API, ThreadPool + AudioFormatManager members, aliveToken async-safety guard

## Decisions Made
- `RegionState::clampRegion` takes raw `(double, double)` endpoints instead of `juce::Range<double>`, because `Range`'s constructor already forces `end = jmax(start, end)` at construction — an inverted pair like `{8.0, 2.0}` collapses to `{8.0, 8.0}` before the function body ever runs, silently destroying the "user dragged right-to-left" case the plan's test wanted to exercise. A `Range`-typed convenience overload remains for callers that already have an ordered range.
- AIFF test fixtures write at 16-bit (JUCE's `AiffAudioFormat::getPossibleBitDepths()` returns `{8, 16, 24}` — no 32-bit float support, unlike WAV).
- Kept the MP3 fixture as a committed binary (not generated at test time) since neither JUCE's `CoreAudioFormat::createWriterFor` nor `afconvert` can encode MP3 — this matches the plan's own research findings.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] AIFF test fixture used an unsupported 32-bit bit depth**
- **Found during:** Task 2 (AudioFileLoaderTests.AiffDecode)
- **Issue:** Test helper wrote AIFF fixtures at 32-bit like the WAV case; `AiffAudioFormat::createWriterFor` returned `nullptr` because AIFF only supports 8/16/24-bit
- **Fix:** Changed the AIFF fixture's bit depth to 16
- **Files modified:** Tests/AudioFileLoaderTests.cpp
- **Commit:** d58a991

**2. [Rule 1 - Bug] clampRegion's inverted-input test case was unreachable through juce::Range<double>**
- **Found during:** Task 3 (RegionStateTests.ClampRegion)
- **Issue:** `juce::Range<double>`'s constructor computes `end = jmax(start, end)` at construction time, so a braced-init `{8.0, 2.0}` literal already collapses to `{8.0, 8.0}` — the function's `isEmpty()`-based "whole file" branch then fired incorrectly on what should have been a normalize-and-clamp case
- **Fix:** Changed `clampRegion` to take raw `(double a, double b, double totalLengthSeconds)` so it can genuinely observe an inverted pair before packing it into a `Range`; added a `Range<double>`-typed overload that delegates to it; updated the test to call the raw-double overload for the inversion case
- **Files modified:** Source/Import/RegionState.h, Tests/RegionStateTests.cpp
- **Commit:** 686268c

---

**Total deviations:** 2 auto-fixed (both Rule 1 - bugs found via TDD's own RED→GREEN cycle, both fully within the current task's own new code)
**Impact on plan:** Both fixes were necessary for correctness; no scope creep, no architectural changes.

## Issues Encountered
None beyond the two auto-fixed TDD discoveries above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- The processor's load/region API (`loadAudioFile`, `getLoadedAudio`, `get/setSelectedRegion`, `getLastLoadError`, `loadBroadcaster`) is ready for Plan 02-02 (conveyor UI) and Plan 02-03 (waveform display) to consume directly
- **IMP-01 requirement status:** this plan proves the decode path (backend) for all four formats but does NOT implement drag-and-drop UI — that lands in Plan 02-02/02-03. IMP-01 is intentionally left unchecked in REQUIREMENTS.md until the user-facing drag-and-drop behavior exists; do not mark it complete based on this plan alone.
- File path + region persistence (`RegionState`) is proven to survive an XML round-trip now, ahead of Phase 7's actual DAW-reload verification
- No blockers for Plan 02-02

---
*Phase: 02-audio-import-waveform*
*Completed: 2026-07-12*

## Self-Check: PASSED

All 12 claimed files verified present on disk; all 6 claimed commit hashes verified present in git history.
