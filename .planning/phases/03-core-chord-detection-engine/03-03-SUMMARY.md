---
phase: 03-core-chord-detection-engine
plan: 03
subsystem: audio-dsp
tags: [juce-dsp, fft, ellis-2007, tempo, beat-tracking, dynamic-programming, catch2, tdd]

# Dependency graph
requires:
  - phase: 03-core-chord-detection-engine
    plan: 01
    provides: "Frozen AudioPreprocessing/BeatGrid/OnsetEnvelopeResult contracts, Tests/SyntheticFixtures.h renderClickTrack fixture, ChordAITests build wiring"
provides:
  - "Real OnsetEnvelope.cpp: Ellis (2007) §3.1 mel-based onset strength envelope at a true 250Hz rate (256/32-sample centered STFT, 40-band HTK mel, dB, half-wave-rectified diff, 0.4Hz HP, ~20ms Gaussian smooth, std-normalize)"
  - "Real TempoBeatTracker.cpp: perceptually-weighted autocorrelation tempo estimate with explicit TPS2/TPS3 octave-error mitigation, DP beat backtrace (direct Ellis 2007 Fig. 1 port), v1 4/4 bar grid"
  - "10 new [chordanalysis]-tagged tests (TempoBeatTrackerTests.*) covering onset rate/peak-alignment/normalization and tempo/beat/bar-grid/degenerate-input behavior"
affects: [03-05-chord-recognition, 03-06-classic-dsp-orchestrator]

# Tech tracking
tech-stack:
  added: []
  patterns: ["Centered-frame STFT convention (librosa stft(center=True)-style zero-padding) so envelope.size() tracks duration*rateHz rather than undercounting by one window", "Ellis 2007 DP beat backtrace ported verbatim from 03-RESEARCH.md's Fig. 1 C++ transcription", "Named constants for every paper-sourced parameter (kEnvelopeHighPassHz, kGaussianSigmaFrames, kTau0Seconds, kSigmaTauOctaves, kBeatTightness) traceable back to 03-RESEARCH.md"]

key-files:
  created: []
  modified:
    - Source/Analysis/OnsetEnvelope.h
    - Source/Analysis/OnsetEnvelope.cpp
    - Source/Analysis/TempoBeatTracker.h
    - Source/Analysis/TempoBeatTracker.cpp
    - Tests/TempoBeatTrackerTests.cpp

key-decisions:
  - "Implemented centered (zero-padded) STFT framing instead of the plan's implicit non-overlap-safe framing — a naive 'numFrames = 1 + (N-fftSize)/hop' undercounts envelope length by one window's worth (993 vs 1000 frames for a 4s/8kHz input), failing the OnsetEnvelopeRate ±2-frame tolerance; librosa's stft(center=True) zero-padding convention (pad fftSize/2 on each side) makes numFrames = 1 + N/hop, matching the promised 250Hz rate exactly and improving click-to-peak time alignment as a side effect"
  - "Octave-error mitigation implemented per PLAN.md's literal formula (TPS2(tau)=TPS(tau)+TPS(2·tau), TPS3(tau)=TPS(tau)+TPS(3·tau), winning tau = argmax across original+both composites) rather than 03-RESEARCH.md's alternate phrasing ('resample TPS to 1/2 and 1/3 length') — both describe the same Ellis 2007 mitigation; the PLAN's exact wording was treated as authoritative since it is what the tests were written against"
  - "bpm computed from the median inter-beat interval of the DP backtrace output (not the raw autocorrelation tau) per PLAN.md's explicit instruction — more robust to any small phase/period drift the DP smooths over"

requirements-completed: []  # ANL-02 not yet marked complete in REQUIREMENTS.md pending phase-level roll-up decision; BeatGrid behavior is fully evidenced by this plan's tests

duration: 10min
completed: 2026-07-12
---

# Phase 3 Plan 03: Tempo, Beat Grid & Bar Grid Detection Summary

**Ellis (2007) "Beat Tracking by Dynamic Programming" implemented with the paper's own published parameters end-to-end: an 8kHz/32ms/4ms mel onset envelope feeding a perceptually-weighted autocorrelation tempo estimate with explicit octave-error mitigation and a direct-ported DP beat backtrace, producing a `BeatGrid` (bpm, beat times, every-4th-beat bar grid) that is the timing skeleton for Phase 3's chord segmentation and Phase 4-6's bar-aligned MIDI export.**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-07-12T21:31:00Z (approx, from RED commit)
- **Completed:** 2026-07-12T21:40:01Z
- **Tasks:** 2 (both TDD RED→GREEN, sharing one RED test commit)
- **Files modified:** 5 (2 headers, 2 implementation files, 1 test file) across 3 commits

## Accomplishments
- `OnsetEnvelope.cpp`: real Ellis (2007) §3.1 onset strength envelope — 256-sample/32-sample-hop (32ms/4ms @ 8kHz) centered STFT via `juce::dsp::FFT` order 8 + Hann window, 40-band HTK mel filterbank (0-4000Hz), dB conversion, half-wave-rectified first-order time-diff summed across bands, 0.4Hz one-pole high-pass, ~20ms Gaussian smoothing, std-deviation normalization — all as named, paper-traceable constants
- `TempoBeatTracker.cpp`: real global tempo estimate via autocorrelation (0.2-4.0s period range) with log-Gaussian perceptual weighting (tau0=0.5s/120BPM, sigmaTau=1.4 octaves) plus explicit TPS2/TPS3 octave-error mitigation, followed by a direct C++ port of Ellis (2007) Fig. 1's DP beat backtrace (kBeatTightness=100, librosa's cross-checked default), bpm computed from the median inter-beat interval
- `barStartBeatIndices` implements the v1 explicit "every 4th beat = downbeat" 4/4 assumption, now documented directly in `TempoBeatTracker.h` per 03-RESEARCH.md Open Question 2 so downstream phases don't assume true meter/downbeat detection
- 10 new `[chordanalysis]`-tagged tests in `Tests/TempoBeatTrackerTests.cpp`: envelope rate/peak-alignment/zero-mean-std-normalize (Task 1), 90/120/160 BPM detection, syncopated-fixture octave-error resistance, beat-to-click alignment, bar-grid exactness, degenerate-input safety (Task 2) — all TDD'd RED (against 03-01's stubs) then GREEN

## Task Commits

1. **RED — failing tests for both tasks** - `2676ed9` (test)
2. **Task 1: OnsetEnvelope implementation** - `5f42c16` (feat)
3. **Task 2: TempoBeatTracker implementation** - `f591d52` (feat)

_TDD flow: one shared RED commit covers both tasks' behavior tests (`TempoBeatTrackerTests.cpp` holds both Task 1's onset-envelope tests and Task 2's tempo/beat tests), confirmed 9/10 failing against the 03-01 stub implementations (`DegenerateInput` trivially passed against the stub's default-constructed `BeatGrid`). Each task's GREEN commit only touches that task's own `.h`/`.cpp` files. No REFACTOR commit needed._

## Files Created/Modified
- `Source/Analysis/OnsetEnvelope.h` - unchanged (frozen skeleton from 03-01, only `.cpp` implemented)
- `Source/Analysis/OnsetEnvelope.cpp` - real Ellis 2007 §3.1 implementation (see Accomplishments)
- `Source/Analysis/TempoBeatTracker.h` - added the v1 4/4-limitation doc comment (per Open Question 2); struct shape unchanged (FROZEN contract preserved)
- `Source/Analysis/TempoBeatTracker.cpp` - real Ellis 2007 §2/§3.2 implementation (see Accomplishments)
- `Tests/TempoBeatTrackerTests.cpp` - 10 new tests, replacing the empty 03-01 stub

## Decisions Made
- Centered/zero-padded STFT framing (see key-decisions above) instead of a naive non-centered frame count — resolves the ±2-frame rate tolerance and improves click-peak time alignment.
- Followed PLAN.md's literal TPS2/TPS3 octave-mitigation formula rather than 03-RESEARCH.md's alternate "resample to 1/2 and 1/3 length" phrasing (equivalent intent, PLAN's wording is what the tests target).
- bpm derived from the DP backtrace's median inter-beat interval, not the raw autocorrelation tau, per PLAN.md's explicit instruction for robustness.

## Deviations from Plan

None — plan executed exactly as written; the only refinement (centered-frame STFT padding) was a design detail underspecified by the plan's action text ("STFT: 256-sample window, 32-sample hop") and resolved in favor of matching the plan's own must-have truth ("envelope.size() ≈ 4.0 * 250 (±2 frames)"), not a deviation from any explicit instruction.

## Issues Encountered
None beyond the framing-convention refinement noted above (resolved within Task 1, before its GREEN commit — not a post-hoc fix).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `BeatGrid` (bpm, beatTimesSeconds, barStartBeatIndices) is fully real and tested; ready for 03-05's beat-synchronized chroma averaging and 03-06's orchestrator.
- Every Ellis-2007-sourced numeric parameter (256/32/40/0.4Hz/20ms/tau0=0.5/sigmaTau=1.4/alpha=100) is a named constant in `OnsetEnvelope.cpp`/`TempoBeatTracker.cpp`, traceable back to 03-RESEARCH.md's "Beat & Tempo Detection" section.
- Octave-error mitigation is demonstrably working: `OctaveErrorResistance` test confirms the syncopated 100 BPM fixture detects 100 BPM (not 50 or 200) within ±3 BPM.
- `CMakeLists.txt` untouched, as required (files were already registered by 03-01).
- This plan ran in parallel with 03-02 (chroma path); only this plan's five listed files were touched. 03-02's `ChromaExtractorTests`/`HarmonicPercussiveFilter`/etc. were mid-TDD-cycle (5 failing assertions observed at time of this plan's final full-tag run) — that is 03-02's own in-flight work, out of this plan's scope, not a regression introduced here.

---
*Phase: 03-core-chord-detection-engine*
*Completed: 2026-07-12*

## Self-Check: PASSED

All modified files verified present on disk (OnsetEnvelope.h/.cpp, TempoBeatTracker.h/.cpp, Tests/TempoBeatTrackerTests.cpp). All 3 commits (2676ed9, 5f42c16, f591d52) verified present in git log. `TempoBeatTrackerTests.*` (10 test cases, 32 assertions) verified passing in isolation, independent of 03-02's concurrent in-progress work.
