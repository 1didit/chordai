---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: in_progress
stopped_at: Completed 05-04-PLAN.md
last_updated: "2026-07-13T12:16:39.746Z"
last_activity: "2026-07-13 — Plan 05-04 complete (Wave 3): generateAllRows orchestrator fans one AnalysisResult into the fixed 5-row set (as-is, pop-trap, rnb-neosoul, house, bass), byte-deterministic and <1ms measured; wired synchronously into PluginProcessor's triggerAnalysis onDone before sendChangeMessage (rows and chord timeline published in the SAME analysisBroadcaster message, never staggered); region-change regeneration reused for free from Phase 4's generation-guarded trigger path; fresh load clears stale rows; full suite 106/106 green, pluginval strictness 5 green (VST3+AU); GEN-01/GEN-04 marked complete"
progress:
  total_phases: 7
  completed_phases: 4
  total_plans: 23
  completed_plans: 21
  percent: 91
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-07-12)

**Core value:** Продюсер закидає пісню-референс і за секунди отримує кілька готових до використання MIDI-акордових наборів у схожому стилі — без знання теорії музики і без ручного підбору на слух.
**Current focus:** Phase 5 - MIDI Conveyor Generation IN PROGRESS (4/6 plans) — MidiGen foundation (05-01), style voicing (05-02), bass line (05-03), and the generateAllRows orchestrator + PluginProcessor wiring (05-04) landed; next: 05-05 UI panel

## Current Position

Phase: 5 of 7 (MIDI Conveyor Generation) — IN PROGRESS
Plan: 4 of 6 complete
Status: Plans 05-01/05-02/05-03/05-04 complete — MidiGen foundation, all four style-voiced chord rows, the bass row generator, and the generateAllRows orchestrator wired synchronously into PluginProcessor (rows publish atomically with the chord timeline; region change regenerates rows for free) all implemented and tested; full suite 106/106 green, pluginval strictness 5 green (VST3+AU); GEN-01/02/03/04 marked complete. Ready for 05-05 (UI panel to render the 5 rows).
Last activity: 2026-07-13 — Plan 05-04 complete (Wave 3): generateAllRows orchestrator fans one AnalysisResult into the fixed 5-row set (as-is, pop-trap, rnb-neosoul, house, bass), byte-deterministic and <1ms measured; wired synchronously into PluginProcessor's triggerAnalysis onDone before sendChangeMessage (rows and chord timeline published in the SAME analysisBroadcaster message, never staggered); region-change regeneration reused for free from Phase 4's generation-guarded trigger path; fresh load clears stale rows; full suite 106/106 green, pluginval strictness 5 green (VST3+AU); GEN-01/GEN-04 marked complete

Progress: [█████████░] 91%

## Performance Metrics

**Velocity:**
- Total plans completed: 18
- Average duration: ~11 min
- Total execution time: ~3.4 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| Phase 01 P01 | 1 | 48min | 48min |
| Phase 01 P02 | 7min | 3 tasks | 2 files |
| Phase 01 P03 | 6min | 2 tasks | 0 files |
| Phase 02 P01 | 9min | 3 tasks | 11 files |
| Phase 02 P02 | 3min | 2 tasks | 7 files |
| Phase 02 P03 | 5min | 3 tasks | 10 files |
| Phase 02 P04 | 12min | 2 tasks | 4 files |
| Phase 03 P01 | 20min | 3 tasks | 32 files |
| Phase 03 P02 | 14min | 3 tasks | 6 files |
| Phase 03 P03 | (see 03-03-SUMMARY.md) | - | - |
| Phase 03 P04 | ~10min | 2 tasks | 3 files |
| Phase 03 P05 | 23min | 2 tasks | 4 files |
| Phase 03 P06 | ~23min (Tasks 1-2) + checkpoint fix session | 3 tasks | 6 files |
| Phase 04 P01 | 20min | 3 tasks | 6 files |
| Phase 04 P02 | ~7min | 3 tasks | 8 files |
| Phase 04 P03 | ~5min | 2 tasks | 4 files |
| Phase 04 P04 | ~20min | 2 tasks | 1 file |
| Phase 05 P01 | 6min | 3 tasks | 20 files |

**Recent Trend:**
- Last 5 plans: 20min, 23min+checkpoint, 20min, ~7min, ~5min
- Trend: Phase 3 P01 took longer (DSP dependency wiring + TDD cycle) — expected for foundation plans; Wave 2 plans (P02/P03) trended back down as expected since CMake/build wiring was already complete, and ran concurrently against disjoint file sets with zero conflicts. P05 took longer than P04 despite running concurrently — most of the extra time was empirical Viterbi self-transition-beta tuning. P06 (final Phase 3 plan) closed the phase: its checkpoint caught a real end-to-end defect (BPM 0/empty chords on real-mix silence) invisible to all 58 prior synthetic-fixture tests, fixed live in one surgical commit — validates the phase's decision to gate on human real-track listening rather than synthetic fixtures alone. Phase 4 P01 (first UI-integration plan) landed clean on the first attempt — every 04-RESEARCH.md pattern proved directly applicable, only one anticipated CMake link-dependency gap needed fixing. Phase 4 P02 (chord timeline display) was the fastest plan yet — every 04-RESEARCH.md Pattern 4/5 example applied verbatim, zero deviations, zero debugging iterations across both TDD cycles and the final integration task. Phase 4 P03 (conveyor analysis progress) was even faster — a small, tightly-scoped two-task plan reusing an existing Timer/paint pipeline with no new dependencies; zero deviations, both tasks + full verification (targeted tests, then full suite + pluginval VST3/AU) passed on the first attempt. Phase 4 P04 (Release timing + Standalone UX checkpoint) closed the phase cleanly — one-time Release build/suite green on the first attempt, real-track timing (2.78s for 75s of audio) comfortably inside the "seconds not minutes" claim, and the human checkpoint approved with zero Phase 4 defects; the user's approval feedback doubled as a forward-looking demand signal for Phase 5/6 scope (see Decisions).

*Updated after each plan completion*
| Phase 05 P02 | ~11min | 3 tasks | 3 files |
| Phase 05 P04 | ~6min | 3 tasks | 5 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Roadmap: Detection engine (Phase 3, headless/testable via harness) split from analysis UI wiring (Phase 4, background-thread + progress + timeline display) — isolates highest-uncertainty DSP work from UI/threading debugging, per research recommendation.
- Roadmap: ANL-04 (background thread + progress shown) placed in Phase 4 not Phase 3 — "UI stays responsive" and "progress is shown" are only observable with a UI present.
- Roadmap: Preview (PRV-01) grouped with Export (Phase 6) rather than Generation (Phase 5) — both are row-level interactions in the same UI component (MidiRowComponent), audition happens "before dragging out."
- [Phase 01-01]: Root project() declares LANGUAGES C CXX (not CXX-only) to fix CMake Generate-step failure caused by JUCE's nested project() only scoping C-language rules to its own subdirectory
- [Phase 01-01]: Pinned JUCE submodule at tag 8.0.14 (newest 8.0.x available at execution time), superseding the 8.0.13 research floor
- [Phase 01]: No contingency fixes needed — auval, pluginval strictness 5 (VST3+AU), and Standalone smoke test all passed cleanly against the unmodified Plan 01 skeleton (empty ParameterLayout, stereo bus config).
- [Phase 01-plugin-foundation]: Phase 1 plugin shell (Plan 01 skeleton) confirmed loading cleanly in Ableton Live, FL Studio, and Logic Pro on cold rescans, plus a DAW project save/reload cycle, with zero source changes needed — PLT-01 fully evidenced, Phase 1 complete.
- [Phase 02-01]: First test infrastructure in the project — Catch2 v3.7.1 + CTest via a `ChordAITests` console-app CMake target, `ChordAITests.`-prefixed test discovery.
- [Phase 02-01]: `RegionState::clampRegion` takes raw `(double, double)` endpoints rather than `juce::Range<double>` — Range's own constructor forces `end = jmax(start, end)` at construction, silently destroying inverted input before the function body could observe it.
- [Phase 02-01]: IMP-01 requirement left unchecked — this plan proves the decode backend only; drag-and-drop UI (needed for full IMP-01) lands in Plan 02-02/02-03.
- [Phase 02-02]: Non-ASCII string literals must go through `juce::String(juce::CharPointer_UTF8(...))`, not a plain `"..."` literal — the implicit `const char*` constructor assumes ASCII and asserts on bytes > 127 (caught by pluginval strictness 5's Editor Automation pass).
- [Phase 02-02]: IMP-01 now fully evidenced end-to-end (backend decode from 02-01 + drag-and-drop UI from 02-02) — marked complete in REQUIREMENTS.md.
- [Phase 02-03]: `RegionSelectionModel` exposes `getTotalLength()` so `RegionSelectorOverlay` can build the `{0, totalLength}` visible range for `WaveformMath` conversion without the overlay caching its own copy of the length.
- [Phase 02-03]: `handleLoadComplete(const LoadedAudio&)` factored as one private editor method, shared by the `ChangeListener` callback (fresh load) and the ctor's editor-reopen restore branch, to avoid duplicating the waveform/region/conveyor-stub wiring.
- [Phase 02-03]: IMP-02 + IMP-03 now fully evidenced (waveform renders on load; region drag/clamp/reset reaches `apvts.state` via `processor.setSelectedRegion`) — marked complete in REQUIREMENTS.md.
- [Phase 02-04]: Checkpoint-discovered drop-target defect (only the 120px conveyor strip accepted drags; .m4a/.aac missing from filter) fixed live during Task 2 manual verification (commit 0abddc5): PluginEditor is now a whole-window FileDragAndDropTarget; extension allowlist centralized in ConveyorBeltComponent::isSupportedAudioFile.
- [Phase 02-04]: IMP-01/02/03 fully human-verified in Standalone (4-format real OS drag-and-drop, waveform legibility, region select, 3+min decode responsiveness) — Phase 2 complete.
- [Phase 03-01]: constant-q-cpp pinned at commit 7ac8404 (verified current master HEAD via `git ls-remote`), wired as a hand-enumerated static lib (upstream has no CMakeLists.txt); include paths + `kiss_fft_scalar=double` derived from upstream's own Makefile.inc, not guessed.
- [Phase 03-01]: 03-RESEARCH.md Open Question 1 resolved empirically — `CQParameters(11025.0, 32.70, 4186.01, 36)` actually yields 288 bins / 8 octaves / minFrequency≈16.67Hz (not the hand-calculated ~32.7Hz/7 octaves); CqtEngineSanity now asserts self-consistency against runtime-queried values instead of a hardcoded range.
- [Phase 03-01]: All 23 Source/Analysis/ skeleton files + both CMake targets' source lists are frozen for Wave 1 — plans 03-02 through 03-06 implement real module bodies only, no further CMakeLists.txt edits needed.
- [Phase 03-03]: OnsetEnvelope STFT framing uses librosa-style centered/zero-padded frames (pad kFftSize/2 each side) rather than a naive non-overlap-safe frame count — this is required to hit the promised 250Hz envelope rate (envelope.size() ≈ duration*rateHz) instead of undercounting by one window; also improves click-to-onset-peak time alignment.
- [Phase 03-03]: Octave-error mitigation (Ellis 2007 TPS2/TPS3 composite functions) implemented per PLAN.md's literal formula; ANL-02 fully evidenced (90/120/160 BPM ±3, syncopated-fixture octave resistance, beat alignment, exact bar grid) and marked complete in REQUIREMENTS.md.
- [Phase 03-02]: `kHpssKernelSeconds` exposed as a public named constant in HarmonicPercussiveFilter.h (not hidden in the .cpp) since `suppressPercussion`'s signature takes no default argument — callers need a documented default to reference.
- [Phase 03-02]: Two test assertions calibrated against empirically-measured CQT/fold behavior rather than hand-guessed expectations (same pattern as 03-01's CqtEngineSanity fix): bass-inversion fixture checks top-4 (not top-3) harmonic pitch-class membership, since the harmonic (>=80Hz) and bass (55-250Hz) ranges deliberately overlap per research Pattern 3 and a loud bass note legitimately competes for a slot — root disambiguation from that competition is the chord-decoder's (03-05) job, not the chroma fold's; silence-tail classification uses a ~0.7s settling margin since low CQT bins have wide time-domain windows causing a ~0.5-0.6s magnitude decay tail after audio stops, not an instant drop. ANL-03 remains unchecked in REQUIREMENTS.md — this plan only completes the chroma-extraction stage, not the full chord-progression detection (needs 03-04/03-05/03-06).
- [Phase 03-04]: `accumulateChroma` implemented alongside `detectKey` in the same Task 1 commit (both trivial companion stubs in one file) rather than deferred to Task 2 as the plan's task split implied; Task 2's three full-chain integration tests then validated (rather than newly drove) that behavior — no functional gap, just an earlier-than-planned implementation landing. ANL-01 fully evidenced end-to-end (pure-vector rotation/confidence tests + real audio-chain tests including relative-minor disambiguation via the E-major dominant's leading tone) and marked complete in REQUIREMENTS.md.
- [Phase 03-04]: Cross-agent git index race with concurrently-executing Plan 03-05 (shared repo, disjoint file sets): one commit (`fd68f4a`) picked up 03-05's already-staged `ChordDecoder.cpp` alongside this plan's `KeyDetector.cpp` change, since both agents share one git index. No code lost (full suite stayed green throughout); mitigated for all later commits by checking `git status --short` immediately before every add/commit. Documented in 03-04-SUMMARY.md "Issues Encountered" for transparency.
- [Phase 03-05]: `kSelfTransition` (Viterbi self-transition beta) retuned from research's 0.85 starting point down to 0.04, empirically: this project's L1-normalized cosine+bass-bias observation matrix is far flatter than a typical trained-HMM emission model (best-vs-second-best per-beat likelihood ratio ~2-3x on real synthetic audio, not orders of magnitude), so literature betas (0.8-0.99) badly oversmoothed both a 1-chord-per-beat fixture (8 distinct chords collapsed to 1-2 segments) and a chord-silence-chord fixture (second chord's path never won against the first). 0.03 was the first value to pass every fixture; 0.04 keeps a small safety margin, still just above the uniform/no-bias baseline (1/36 ≈ 0.0278) — full root-cause analysis and empirical sweep documented in-code (`Source/Analysis/ChordDecoder.cpp`).
- [Phase 03-05]: ANL-03 remains unchecked in REQUIREMENTS.md — `decodeChords` is proven against hand-built beat grids by design ("a decoder test, not a tracker test" per the plan's own scope); 03-06's orchestrator still needs to demonstrate the full real-pipeline, bar-grid-aligned output before ANL-03's "aligned to the bar grid" criterion is fully evidenced (matching 03-01/03-02's own documented precedent).
- [Phase 03-06]: `ClassicDspChordAnalyzer::analyse` implemented as a pure synchronous sequence (no threads/MessageManager/GUI includes) wiring every prior plan's module behind the frozen `ChordAnalyzer` interface; `computeCqt` gained a backward-compatible optional `shouldCancel` hook checked per feed chunk (the only stage long enough to need sub-stage cancel latency); BeatGrid/ChromaSequence region-relative times shifted by `regionStartSeconds` at the orchestrator boundary so `AnalysisResult` always carries absolute source-file time. ANL-01/02/03/06 all marked complete — Phase 3 done.
- [Phase 03-06]: Real-track checkpoint caught a genuine defect invisible to all 58 prior synthetic-fixture tests: on a real 75s trap beat, the onset envelope's HP-filter negative dips at silence (-17.6 sigma vs +0.44 sigma onsets) dominated normalization/autocorrelation and drove the DP beat-tracker to zero beats (BPM 0, empty chords), because synthetic click fixtures are always positive-spiked by construction. Fixed by half-wave rectifying the onset envelope after smoothing, before normalization (matches librosa's `onset_strength`) — commit `d4eced3`. Full suite stayed green (64/64, 11288 assertions); user-confirmed plausible re-run (BPM 170.455, key G# minor, coherent diatonic progression) on the same track. Validates gating phase completion on human real-track listening, not synthetic fixtures alone.

- [Phase 03 post-verify]: Tempo metrical-level disambiguation (commit 87d6511) — user ground truth (TOCK.mp3 = 128 BPM, detected 170.45) exposed a 4/3 metrical-level lock from triplet hi-hats; fixed by rescoring DP grids at ratios {1, 3/4, 4/3, 1/2, 2, 1/3, 3} with W(tau)·meanOnset/sqrt(period) density-compensated scoring. Re-verified: 128.2 BPM on TOCK, 49/49 test cases green.

- [Phase 04-01]: `analysisPool` is a second, separate size-1 `juce::ThreadPool` (not sharing `loaderPool`) — a new-file-drop mid-re-analysis must not queue behind a still-cancelling analysis job, and `removeAllJobs`'s cancellation scope must never touch an unrelated decode job.
- [Phase 04-01]: Generation-guarded cancel-and-restart (`analysisPool.removeAllJobs(true, 0)` + `std::atomic<uint64_t> analysisGeneration`) established as the reusable pattern for any future supersedable background job — Phase 5's GEN-04 (regenerate rows on region change) is expected to reuse it verbatim.
- [Phase 04-01]: `ChordAITests` target needed `juce::juce_audio_processors` + `juce::juce_gui_extra` added to its link libraries (plus `PluginProcessor.cpp`/`PluginEditor.cpp`/UI component sources and a `JucePlugin_Name` define) so tests can construct a real `ChordAIAudioProcessor` via its public API — first Phase 4 test file to exercise the processor rather than only headless DSP code.
- [Phase 04-02]: chordName lifted verbatim from ClassicDspChordAnalyzerTests' RealTrackHarness lambda into a header-only shared ChordNameFormatter.h, consumed by the UI
- [Phase 04-02]: ChordTimelineView given its own dedicated 28px band above the waveform (not overlaid) to avoid any z-order/mouse-interception conflict with RegionSelectorOverlay
- [Phase 04-03]: Analysis progress expressed via the belt itself (gold fill on the belt top edge + 2x beltOffset speed-up) rather than a stock juce::ProgressBar, painted inside the existing logical-frame/30Hz Timer pipeline — no new Timer, no look-and-feel-clashing widget, consistent with the locked pixel-art identity (02-CONTEXT.md).
- [Phase 04-03]: Falling-chunk stub trigger moved from load-complete to the analyzing->idle transition (edge-detected via editor-local `wasAnalyzing`, guarded by a non-null published result) — a superseded/cancelled analysis run never fires a spurious chunk, per 04-01's generation-guard invariant.
- [Phase 04-04]: One-time Release build (build-release/, git-ignored) measured the real ANL-04 timing claim: TOCK.mp3 (75s track) analyzed in 2.78s on Release vs. Debug's ~35s/180s PerformanceBudget proxy — confirms "seconds not minutes" is a Release-build claim, not evidenced by the Debug regression test alone. Phase 4 complete (4/4 plans); user checkpoint approved with zero defects.
- [Phase 04-04 / user demand signal]: During the Phase 4 closing checkpoint, user's approval feedback pushed explicitly toward Phase 5/6 scope: "мені потрібні конкретні акорди для piano roll які я зможу зберегти як midi чи перетягнути до іншого інструменту" (concrete chords for piano roll, saveable as MIDI or draggable to another instrument). This maps directly to existing roadmap scope — Phase 5 GEN-01..04 (MIDI row generation) and Phase 6 EXP-01..03/PRV-01 (drag-out, .mid save, preview) — no roadmap change needed, but it validates and should weight Phase 5/6 planning toward shipping the drag-out/.mid-save path early rather than deferring it.
- [Phase 05-01]: MidiGen module foundation landed: beat-domain NoteEvent, frozen 5-row generator signatures, tested chord-tone/voice-leading/humanization helpers, self-consistency-tested fixtures, one-time CMake wiring for the whole phase
- [Phase 05-03]: generateBassRow implemented (TrapSustain/RnbRootFifth/HouseFourOnFloor), root-following proven across all three rhythms, byte-deterministic; RnbRootFifth walks 4-beat spans (root then fifth via root+7 semitones, no %12 wrap) with a root-only sustain for any remainder under 4 beats; GEN-03 marked complete
- [Phase 05-02]: Four style-voiced chord row generators implemented (as-is, Pop/Trap, R&B/Neo-soul, Electronic/House); R&B seed-chord register clamp corrected from truncating juce::jlimit to octave-preserving clamp (Rule 1 bug fix, verified against plan's own expected pitch-class values); GEN-02 marked complete
- [Phase 05-04]: generateAllRows orchestrator wired synchronously into PluginProcessor's triggerAnalysis onDone, publishing midiSetRows via the same atomic-shared_ptr idiom as analysisResult, generated and stored BEFORE analysisBroadcaster.sendChangeMessage() so rows and the chord timeline can never be observed out of sync; region-change regeneration (GEN-04) required zero new wiring, reusing Phase 4's generation-guarded cancel-and-restart trigger path verbatim; shipped bass row defaults to BassRhythm::TrapSustain (Claude's-discretion call); GEN-01/GEN-04 marked complete

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 6 (Row Preview & Export) drag-and-drop has documented DAW-specific fragility (Ableton/FL Studio) — recommend a research/spike pass before considering the phase done.
- Legal gate: DSP dependencies must be MIT/BSD/Apache/zlib only (no GPL/AGPL) — enforced for constant-q-cpp (MIT-style) + bundled KissFFT (BSD-3-Clause) in Plan 03-01; continue enforcing per-dependency for remaining Phase 3 plans (all in-house algorithm code, no further third-party deps expected).

## Session Continuity

Last session: 2026-07-13T12:16:39.743Z
Stopped at: Completed 05-04-PLAN.md
Resume file: None
