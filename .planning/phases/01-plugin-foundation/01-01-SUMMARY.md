---
phase: 01-plugin-foundation
plan: 01
subsystem: infra
tags: [juce, cmake, vst3, au, standalone, plugin-scaffolding, apvts]

# Dependency graph
requires: []
provides:
  - Building CMake-based JUCE plugin project (VST3 + AU + Standalone from one CMakeLists.txt)
  - external/JUCE submodule pinned at exact tag 8.0.14
  - AudioProcessor/AudioProcessorEditor skeleton with APVTS state persistence
  - RT-safe processBlock discipline established from commit 1
  - Fixed plugin identity (PLUGIN_CODE Cha1 / PLUGIN_MANUFACTURER_CODE Chai / com.chordai-dev.chordai)
affects: [01-plugin-foundation-plan-02, all-later-phases]

# Tech tracking
tech-stack:
  added: ["JUCE 8.0.14 (git submodule)", "CMake 4.4.0", "Ninja 1.13.2"]
  patterns:
    - "juce_add_plugin() single-target CMake scaffolding for VST3+AU+Standalone"
    - "APVTS constructed in member-init list, wired to getStateInformation/setStateInformation via copyState/copyXmlToBinary and getXmlFromBinary/replaceState"
    - "processBlock kept provably inert (ScopedNoDenormals + channel clear only) as the RT-safety baseline for all future phases"
    - "project() must declare LANGUAGES C CXX at root, not CXX-only, when add_subdirectory()-ing JUCE (JUCE's own project() call enables C only within its own subdirectory scope)"

key-files:
  created:
    - CMakeLists.txt
    - Source/PluginProcessor.h
    - Source/PluginProcessor.cpp
    - Source/PluginEditor.h
    - Source/PluginEditor.cpp
    - .gitignore
    - .gitmodules
    - external/JUCE (submodule, pinned 8.0.14)
  modified: []

key-decisions:
  - "Pinned JUCE at 8.0.14 (newest 8.0.x tag found at execution time via git ls-remote-equivalent tag listing), not the 8.0.13 floor research suggested as safe minimum"
  - "Root CMakeLists.txt declares LANGUAGES C CXX (not CXX-only) to work around a CMake directory-scoping issue where JUCE's own nested project(LANGUAGES C CXX) call only establishes C-language generate rules within its own subdirectory scope"
  - "Bus layout set to explicit stereo in/out (not bare BusesProperties()) per plan's deviation from research Pattern 2, since processBlock's channel-clear logic and AU channel-config validation require buses to exist"

patterns-established:
  - "Pattern: AudioProcessor factory functions (createEditor, createPluginFilter) legitimately use `new` on the host/main thread — RT-safety grep audits must scope to processBlock and its callees only, not the whole file"

requirements-completed: [PLT-01]

# Metrics
duration: 48min
completed: 2026-07-12
---

# Phase 1 Plan 1: Plugin Foundation Skeleton Summary

**JUCE 8.0.14 CMake plugin skeleton building VST3, AU, and Standalone from one CMakeLists.txt, with APVTS state persistence and a provably RT-safe processBlock.**

## Performance

- **Duration:** ~48 min
- **Started:** 2026-07-12T15:46:00Z (approx, toolchain install)
- **Completed:** 2026-07-12T16:30:09Z
- **Tasks:** 3/3
- **Files modified:** 8 (7 new source/config files + external/JUCE submodule pointer)

## Accomplishments
- Installed cmake 4.4.0 and ninja 1.13.2 via Homebrew (neither was present on this machine)
- Pinned JUCE as a git submodule at exact tag 8.0.14 (newest 8.0.x available, supersedes the 8.0.13 floor from research)
- Wrote CMakeLists.txt with explicit, stable plugin identity (PLUGIN_CODE Cha1, PLUGIN_MANUFACTURER_CODE Chai, BUNDLE_ID com.chordai-dev.chordai) and minimal link surface (no juce_dsp/juce_audio_formats)
- Implemented ChordAIAudioProcessor with APVTS wired from commit 1 (empty parameter layout, defensive state round-trip) and a provably RT-safe processBlock (ScopedNoDenormals + channel clear only)
- Built all three targets (ChordAI_VST3, ChordAI_AU, ChordAI_Standalone) with zero errors; VST3 and AU bundles auto-installed to `~/Library/Audio/Plug-Ins`; Standalone `.app` produced under `build/ChordAI_artefacts/Debug/Standalone/`

## Task Commits

Each task was committed atomically:

1. **Task 1: Install build toolchain and pin JUCE submodule** - `1d92d30` (chore)
2. **Task 2: Create CMakeLists.txt and JUCE plugin skeleton sources** - `d615d97` (feat)
3. **Task 3: Build all three formats and verify installed artifacts** - no separate commit (verification-only task; build succeeded with zero errors on first attempt, no source changes required)

**Plan metadata:** (this commit) `docs(01-01): complete plan`

## Files Created/Modified
- `CMakeLists.txt` - juce_add_plugin() target with explicit identity, VST3+AU+Standalone formats, minimal link surface
- `Source/PluginProcessor.h` - ChordAIAudioProcessor declaration with public APVTS member
- `Source/PluginProcessor.cpp` - APVTS wiring, state save/restore, RT-safe processBlock, createEditor, createPluginFilter
- `Source/PluginEditor.h` - Minimal AudioProcessorEditor shell declaration
- `Source/PluginEditor.cpp` - Editor paint (dark grey fill) / resized (empty) implementation
- `.gitignore` - Excludes build/, validation-logs/, tools/pluginval.app/.zip, .DS_Store
- `.gitmodules` / `external/JUCE` - JUCE submodule pinned at tag 8.0.14

## Decisions Made
- **JUCE 8.0.14 over 8.0.13:** Research flagged 8.0.14 as MEDIUM confidence with conflicting date metadata. At execution time, `git tag -l '8.0.*' --sort=-v:refname` against the live JUCE repo confirmed 8.0.14 exists and is a genuine, newer 8.0.x tag (not 9.x). Pinned per plan instruction to use the newest 8.0.x tag found.
- **`project(ChordAI VERSION 0.1.0 LANGUAGES C CXX)` instead of `LANGUAGES CXX`:** Required to fix a real CMake Generate-step failure (see Deviations below) — not a stylistic change.
- **Stereo in/out bus layout** (per plan's own explicit deviation from research Pattern 2): documented inline in PluginProcessor.cpp as a code comment.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] CMake Generate step failed with "CMAKE_C_COMPILE_OBJECT not set" — root project() needed LANGUAGES C CXX, not CXX-only**
- **Found during:** Task 2 (`cmake -B build -G Ninja` verification step)
- **Issue:** `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` consistently failed at the Generate step (reproduced identically across cmake 4.4.0 and cmake 3.29.6, and across both Ninja and Unix Makefiles generators, ruling out a cmake-version or generator-specific bug) with: `CMake Error: Error required internal CMake variable not set... Missing variable is: CMAKE_C_COMPILE_OBJECT`. Root cause: JUCE's own `external/JUCE/CMakeLists.txt` declares `project(JUCE VERSION 8.0.14 LANGUAGES C CXX)`. Because that `project()` call executes inside the `add_subdirectory(external/JUCE)` child scope, the C-language compile-rule variables it establishes (via `include(CMakeCInformation)`) are local to that subdirectory's variable scope per CMake's directory-scoping model, and do not propagate back to the root `CMakeLists.txt` scope. When JUCE-internal targets requiring C compilation (e.g. `juce_graphics_Sheenbidi.c`) are later processed in root scope at Generate time, `CMAKE_C_COMPILE_OBJECT` is undefined there.
- **Fix:** Changed root `project(ChordAI VERSION 0.1.0 LANGUAGES CXX)` to `project(ChordAI VERSION 0.1.0 LANGUAGES C CXX)`, establishing C-language generate rules in root scope from the start. Documented the rationale as an inline CMake comment.
- **Files modified:** `CMakeLists.txt`
- **Verification:** Clean `rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` completed with "Generating done (0.0s)" and no errors; confirmed `ChordAI_VST3`, `ChordAI_AU`, `ChordAI_Standalone` targets present via `ninja -t targets`.
- **Committed in:** `d615d97` (Task 2 commit)

**2. [Verification scoping clarification, no code change] Task 3's literal whole-file grep verify command produces false positives against required JUCE boilerplate**
- **Found during:** Task 3 verification
- **Issue:** The plan's `<verify><automated>` command greps all of `Source/PluginProcessor.cpp` for `new |malloc|std::mutex|fopen`, but `createEditor()` and the mandatory `createPluginFilter()` factory function both legitimately use `new` (per the plan's own Task 2 action text, which explicitly requires this exact code). These functions run on the host/main thread at plugin instantiation, never on the audio thread, so they are not RT-safety violations. The plan's own prose ("grep... inside processBlock and anything it calls") scopes the check to `processBlock`, but the literal automated command greps the entire file.
- **Fix:** No code change — verified `processBlock` in isolation (extracted via `awk` between its signature and closing brace) contains none of `new `, `malloc`, `std::mutex`, `resize`, `fopen`, or `juce::String`. Confirmed RT-safe per the task's actual intent.
- **Files modified:** none
- **Verification:** Isolated `processBlock` body reviewed directly — contains only `ScopedNoDenormals`, `ignoreUnused`, `getTotalNumOutputChannels()`, and `buffer.clear()`.
- **Committed in:** N/A (no code change, documentation only)

---

**Total deviations:** 2 (1 auto-fixed blocking build-config issue, 1 verification-scoping clarification)
**Impact on plan:** The CMake language fix was necessary for the project to build at all — no scope creep, pure build-config correction. The grep-scoping issue required no code change; the underlying processBlock code was already correct as written.

## Issues Encountered
- CMake 4.4.0 (installed fresh via `brew install cmake`) combined with JUCE 8.0.14's nested `project(LANGUAGES C CXX)` call inside `add_subdirectory(external/JUCE)` triggered a Generate-step failure not mentioned in research. Diagnosed by reproducing against both cmake 4.4.0 and a pinned cmake 3.29.6 (via a throwaway pip venv, since removed) and both Ninja and Unix Makefiles generators — all four combinations failed identically, ruling out a version/generator-specific cause and confirming the root cause was CMake's directory variable scoping, not a toolchain bug. Fixed by declaring `LANGUAGES C CXX` in the root `CMakeLists.txt` (see Deviations #1).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Build toolchain (cmake, ninja), JUCE submodule pin, CMakeLists.txt, and processor/editor skeleton are all in place and verified building.
- VST3 and AU bundles are live in `~/Library/Audio/Plug-Ins`; Standalone `.app` exists in build artefacts — ready for Plan 02's auval/pluginval validation pass and the zero-parameter APVTS empirical check (research Open Question 3).
- No blockers for Plan 02.

---
*Phase: 01-plugin-foundation*
*Completed: 2026-07-12*

## Self-Check: PASSED

All created files verified present (CMakeLists.txt, Source/PluginProcessor.{h,cpp}, Source/PluginEditor.{h,cpp}, .gitignore, .gitmodules, external/JUCE, SUMMARY.md). All commit hashes (1d92d30, d615d97) verified present in git log.
