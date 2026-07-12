---
phase: 01-plugin-foundation
plan: 02
subsystem: testing
tags: [pluginval, auval, vst3, au, standalone, validation, rt-safety]

# Dependency graph
requires:
  - phase: 01-plugin-foundation
    provides: Building JUCE plugin (VST3+AU+Standalone) with APVTS state and RT-safe processBlock, installed bundles in ~/Library/Audio/Plug-Ins
provides:
  - Idempotent pluginval fetch script (tools/fetch-pluginval.sh) — gitignored binary, committed script
  - Standalone launch smoke test (tools/smoke-test-standalone.sh) — liveness + crash-report check
  - Green auval PASS for the AU component (aufx Cha1 Chai)
  - Green pluginval strictness-5 SUCCESS for both VST3 and AU, including state round-trip
  - Confirmed RT-safety of processBlock via grep audit (no mutex/alloc/I-O)
affects: [01-plugin-foundation-plan-03, all-later-phases]

# Tech tracking
tech-stack:
  added: ["pluginval 1.0.4 (Tracktion, prebuilt macOS release, gitignored binary)"]
  patterns:
    - "Automated pre-DAW validation gate: auval + pluginval strictness 5 + Standalone smoke test, all scriptable and rerunnable after any rebuild"
    - "pluginval binaries fetched via idempotent script, never committed; only the fetch script is source-controlled"
    - "Standalone smoke test uses direct binary launch + PID liveness check + DiagnosticReports diff, no GUI automation needed"

key-files:
  created:
    - tools/fetch-pluginval.sh
    - tools/smoke-test-standalone.sh
  modified: []

key-decisions:
  - "No contingency fixes were needed — auval, both pluginval strictness-5 runs, and the RT-safety grep audit all passed cleanly against the Plan 01 skeleton as-is (empty ParameterLayout, stereo bus config). The zero-parameter and bus-config contingencies from research Open Question 3 did not trigger."

patterns-established:
  - "Pattern: validation-logs/ is gitignored (evidence, not source) — pluginval output-dir logs stay local, not committed"

requirements-completed: [PLT-01]

# Metrics
duration: 7min
completed: 2026-07-12
---

# Phase 1 Plan 2: Automated Validation Gate Summary

**auval + pluginval strictness-5 (VST3 and AU) both green, Standalone smoke test passing, RT-safety grep-confirmed — all against the unmodified Plan 01 skeleton, with zero contingency fixes needed.**

## Performance

- **Duration:** ~7 min
- **Started:** 2026-07-12T16:33:00Z (approx, following Plan 01 completion)
- **Completed:** 2026-07-12T16:40:00Z
- **Tasks:** 3/3
- **Files modified:** 2 (tools/fetch-pluginval.sh, tools/smoke-test-standalone.sh)

## Accomplishments
- `tools/fetch-pluginval.sh`: idempotent curl+unzip of the latest Tracktion/pluginval macOS release; verified idempotency (second run prints "pluginval already present" and exits 0 without re-downloading)
- `tools/smoke-test-standalone.sh`: launches the Standalone `.app` binary directly, confirms 5s liveness via PID check, kills cleanly, diffs `~/Library/Logs/DiagnosticReports` against a pre-launch marker — PASS on first run
- `auval -v aufx Cha1 Chai` → AU VALIDATION SUCCEEDED (full PASS across parameter info, format tests, and render tests including MIDI and bad-max-frames edge cases)
- `pluginval --strictness-level 5 --validate-in-process` → SUCCESS on both the installed VST3 and AU component, covering the state save/restore round-trip test and RealtimeCheck automation sweeps across 44.1/48/96 kHz and block sizes 64-1024
- RT-safety grep audit of `processBlock` (isolated via awk, comments excluded) confirmed zero occurrences of `std::mutex`, `CriticalSection`, `new`, `malloc`, `resize`, `fopen`, `juce::String`

## Task Commits

Each task was committed atomically:

1. **Task 1: Create tools/fetch-pluginval.sh and fetch the binary** - `0f029e0` (feat)
2. **Task 2: Create tools/smoke-test-standalone.sh and run it** - `33c6e2e` (feat)
3. **Task 3: Run full validation suite (auval + pluginval strictness 5) and fix findings** - no separate commit (verification-only task; auval, both pluginval runs, and the RT grep audit all passed on first attempt, no source changes required)

**Plan metadata:** (this commit) `docs(01-02): complete plan`

## Files Created/Modified
- `tools/fetch-pluginval.sh` - Idempotent pluginval binary fetch (curl + unzip of GitHub latest release); binary itself gitignored
- `tools/smoke-test-standalone.sh` - Standalone launch smoke test: 5s liveness check + DiagnosticReports crash-report diff

## Decisions Made
- No contingency fixes applied to `Source/PluginProcessor.{h,cpp}` — the plan's own contingencies (placeholder parameter for zero-parameter warnings, bus/channel-config adjustments) were prepared but not needed, since auval and pluginval strictness 5 both passed cleanly against the empty `ParameterLayout` and explicit stereo bus config from Plan 01.

## Deviations from Plan

None - plan executed exactly as written. All three verification gates (auval, pluginval x2, smoke test) passed on the first attempt against the unmodified Plan 01 skeleton; the plan's own contingency branches (Task 3, step 4) were evaluated and correctly not triggered.

## Issues Encountered

pluginval logged one informational line during the AU run — `!!! WARNING: Current program is -1... Is this correct?` under "Plugin programs" — but the test section still reported `Completed tests` and the overall run ended `SUCCESS`. This is a known pluginval benign warning for plugins with a single default program (JUCE default `getCurrentProgram() == 0` vs. host-side `-1` sentinel before first explicit selection) and is not a strictness-5 failure; no action taken, no source changes made.

## User Setup Required
None - no external service configuration required. pluginval was fetched automatically via the authorized fetch script (Tracktion's official GitHub releases).

## Next Phase Readiness
- Full automated validation gate (build + auval + pluginval strictness 5 + Standalone smoke test) is now scriptable and green, satisfying PLT-01 success criteria 2-5's automatable half.
- State round-trip (getStateInformation/setStateInformation) is proven by pluginval's "Plugin state" strictness-5 test on both formats.
- Remaining PLT-01 evidence is the manual DAW load matrix — deferred to Plan 03's checkpoint as designed.
- No blockers for Plan 03.

---
*Phase: 01-plugin-foundation*
*Completed: 2026-07-12*

## Self-Check: PASSED

All created files verified present (tools/fetch-pluginval.sh, tools/smoke-test-standalone.sh, 01-02-SUMMARY.md). All commit hashes (0f029e0, 33c6e2e) verified present in git log.
