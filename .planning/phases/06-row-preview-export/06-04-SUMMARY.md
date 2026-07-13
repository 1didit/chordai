---
phase: 06-row-preview-export
plan: 04
subsystem: ui
tags: [pluginval, ctest, standalone, midi-export, human-checkpoint]

# Dependency graph
requires:
  - phase: 06-row-preview-export
    provides: "06-01 MidiFileWriter; 06-02 AuditionRenderer/RT-safe processBlock playback; 06-03 MidiRowView interactive play/save icons + drag-out + save dialog"
provides:
  - "Phase 6 closing gate: fresh Release build, full suite (134/134), pluginval strictness 5 (VST3+AU) SUCCESS x2"
  - "Human checkpoint evidence on the user's real DAW (FL Studio) and real track (TOCK.mp3): audition, drag-into-piano-roll, save dialog all confirmed working"
  - "PRV-01/EXP-01/EXP-02/EXP-03 fully evidenced end-to-end (all four already marked complete in REQUIREMENTS.md by 06-01/06-02/06-03; this plan supplies the missing manual DAW/audition evidence)"
affects: [07-persistence-multi-daw-verification]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Checkpoint lesson: 'open App.app' on an already-running instance only refocuses the existing (possibly stale) process instead of launching the freshly built binary -- always kill any running Standalone process before a human-verify checkpoint and relaunch from the just-built artefact path"

key-files:
  created:
    - .planning/phases/06-row-preview-export/06-04-SUMMARY.md
  modified: []

key-decisions:
  - "REQUIREMENTS.md required no edits in this plan -- PRV-01/EXP-01/EXP-02/EXP-03 were already flipped to [x] with Complete traceability rows across 06-01/06-02/06-03; this plan's job was supplying the last missing manual evidence (FL Studio drag, audition sound, save dialog), not the checkbox flip itself"
  - "Large new feature direction from the user (genre system, per-row regenerate/randomize, premium conveyor idle/animate states, max-quality MIDI generation) recorded in STATE.md as the driver for an upcoming inserted Phase 6.1 -- not implemented, no roadmap/requirements change made in this plan"

patterns-established:
  - "Stale-Standalone-instance checkpoint lesson captured for future checkpoint instructions: kill+relaunch, don't rely on `open` to refresh an already-running app"

requirements-completed: [PRV-01, EXP-01, EXP-02, EXP-03]

# Metrics
duration: ~58min (incl. human checkpoint troubleshooting + real-DAW verification)
completed: 2026-07-13
---

# Phase 6 Plan 04: Phase Gate + FL Studio Human Checkpoint Summary

**Phase 6 closing gate (134/134 suite, pluginval strictness 5 VST3+AU SUCCESS x2) plus real FL Studio checkpoint confirming audition, MIDI drag-into-piano-roll, and native save dialog all work end-to-end on the user's own track.**

## Performance

- **Duration:** ~58 min (fresh Release build/suite/pluginval gate + human checkpoint, including a stale-Standalone-instance troubleshooting detour)
- **Started:** 2026-07-13T15:45:33+01:00 (immediately after 06-03 completion)
- **Completed:** 2026-07-13T16:43:00+01:00 (approx, user final approval)
- **Tasks:** 3 (Task 1 auto gate, Task 2 human checkpoint, Task 3 requirements verification)
- **Files modified:** 0 code files (docs-only plan; REQUIREMENTS.md already correct from prior plans)

## Accomplishments
- Fresh Release rebuild (`build-release/`, CMAKE_BUILD_TYPE=Release) — all targets including Standalone
- Full ctest suite green: 134/134 (confirmed via `build-release/Testing/Temporary/LastTest.log`, last case `ProcessorAuditionTests.PrepareToPlayStopsStaleAudition` passed 15:52 WEST, zero entries in `LastTestsFailed.log`)
- pluginval strictness 5 SUCCESS on both VST3 and AU (Standalone-audition RT path, first wave to exercise the full row-interaction editor code under the Editor Automation pass at this strictness)
- Standalone smoke-launch confirmed (`build-release/ChordAI_artefacts/Release/Standalone/ChordAI.app`)
- Human checkpoint (Parts A-C) approved by the user on FL Studio + TOCK.mp3, after a process-level troubleshooting detour (see Issues Encountered): audition play/stop/one-at-a-time/auto-stop/region-change-stop all confirmed; drag-out into FL Studio's piano roll confirmed with correct notes/bar alignment/tempo across multiple rows including repeat-drag; native save dialog confirmed (pre-filled MIDI-pack-style name, default `~/Documents/ChordAI MIDI/` folder, last-dir memory) — user's final word: "супер"
- REQUIREMENTS.md verified: PRV-01/EXP-01/EXP-02/EXP-03 already `[x]` with "Complete" traceability rows (flipped incrementally across 06-01/06-02/06-03) — no duplicate edit needed this plan

## Task Commits

No code commits this plan — Task 1 was verification-only (no files), Task 2 was a manual checkpoint with zero code defects to fix, and Task 3 found REQUIREMENTS.md already correct (verify-only, no diff to commit).

**Plan metadata:** (this commit) docs(06-04): complete phase gate + FL Studio checkpoint, close Phase 6

## Files Created/Modified
- `.planning/phases/06-row-preview-export/06-04-SUMMARY.md` - this summary
- `.planning/STATE.md` - Phase 6 marked complete (4/4 plans), stale-instance checkpoint lesson, Phase 6.1 demand-signal decision logged
- `.planning/ROADMAP.md` - 06-04 checkbox checked, Phase 6 progress row 4/4 Complete

## Decisions Made
- No REQUIREMENTS.md edit needed — all four requirements were already `[x]`/Complete from 06-01 (EXP-03), 06-02 (PRV-01 engine half, later completed), and 06-03 (EXP-01/EXP-02, PRV-01 UI half). This plan's contribution is the missing manual evidence layer (real DAW, real ears, real dialog), not a checkbox flip.
- Checkpoint-process lesson recorded (not a code defect): `open App.app` on an already-running process only refocuses that process — it does not relaunch from a freshly rebuilt binary. Future checkpoints must explicitly kill any running Standalone instance (`pkill -f ChordAI.app/Contents/MacOS/ChordAI` or equivalent) before presenting `open build.../ChordAI.app` to the user, to guarantee the fresh build is what gets tested.
- User's large forward-looking feature request (genre system with chips + narrower waveform strip + genre-checkbox menu + 5 genre-matched MIDI patterns; per-row regenerate/randomize button; premium conveyor idle/animate rework; maximum-quality MIDI generation algorithms) logged in STATE.md as the demand signal for an upcoming inserted Phase 6.1 — explicitly not implemented, no ROADMAP/REQUIREMENTS change in this plan.

## Deviations from Plan

None — Task 1's gate and Task 3's requirements check both matched the plan exactly. Task 2's checkpoint surfaced one **process-level** issue, not a code defect: the user's first verification attempt ("нічого не грає, не перетягується") was against a stale pre-Phase-6 Standalone instance (PID 55706, started 14:19) that `open` had merely refocused rather than replaced with the freshly built 15:51-timestamped binary. No source change was needed — killing the stale process and relaunching the fresh build (confirmed via `ps aux`: fresh PID 62976 started 16:19, well after the 15:51:53 build timestamp) resolved it immediately, and the user then confirmed full pass ("супер").

## Issues Encountered
- **Stale Standalone instance masking the fresh build during Part A of the checkpoint.** Root cause: macOS `open` on an app bundle that already has a running process just brings that process to the foreground; it does not exec a new process from the (possibly newer) bundle contents. Since a pre-06-04 Standalone instance was already running from an earlier session, the first verification pass exercised old code with no audition/drag wiring, producing a false-negative ("nothing plays, nothing drags"). Resolved by killing the stale PID and reopening the app, which launched the actual fresh Release binary. Recorded as a checkpoint-process lesson for future plans (see Decisions) rather than a plan/code deviation, since zero source files changed.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness
- Phase 6 complete (4/4 plans): MIDI row preview & export fully shipped and human-verified end-to-end — audition (PRV-01), DAW drag-out (EXP-01), native save (EXP-02), and bar-aligned/tempo-correct export (EXP-03) all evidenced on the user's own FL Studio project and real track.
- Phase 7 (Persistence & Multi-DAW Verification) is next per ROADMAP.md — PLT-02 (DAW project save/reload persistence) and PLT-03 (full per-DAW drag-and-drop matrix: Ableton/FL Studio/Logic Pro) remain pending.
- New demand signal (genre system, per-row regenerate, premium conveyor rework, max-quality MIDI generation) logged in STATE.md — recommend addressing via `/gsd:plan-phase` for an inserted Phase 6.1 before or interleaved with Phase 7, per user's explicit framing, rather than folding it silently into Phase 7 scope.

---
*Phase: 06-row-preview-export*
*Completed: 2026-07-13*

## Self-Check: PASSED

134/134 suite pass confirmed via build-release/Testing/Temporary/LastTest.log (last test case timestamped Jul 13 15:52 WEST, zero entries in LastTestsFailed.log). REQUIREMENTS.md confirmed showing `[x]` for PRV-01/EXP-01/EXP-02/EXP-03 with all four traceability rows "Complete" (grep verified). Fresh Standalone process confirmed running from the 15:51:53-built binary (`ps aux` PID 62976 started 16:19, after the build timestamp) — supersedes the earlier stale-PID false negative.
