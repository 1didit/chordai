# Roadmap: ChordAI

## Overview

ChordAI goes from an empty, loadable plugin shell to a commercial-shaped conveyor: drop in a song, watch the plugin detect key/tempo/chords in the background, see the result on the waveform, get back several style-voiced MIDI rows plus a bass line, audition and drag any of them into the DAW piano roll, and have all of it survive a DAW project reload across Ableton, FL Studio, and Logic Pro. The path is strictly bottom-up — plugin shell first (nothing works without a loadable host build), then audio in, then the headless detection engine (isolated and test-harness-verifiable before touching UI), then the UI wiring for that engine, then the MIDI generation logic that is the product's actual differentiator, then the row-level preview/export interactions, and finally persistence + per-DAW hardening as a release gate.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Plugin Foundation** - Empty VST3/AU/Standalone plugin loads cleanly in all target hosts with real-time-safety and state-persistence patterns established (completed 2026-07-12)
- [x] **Phase 2: Audio Import & Waveform** - User drags a song in, sees its waveform, and picks what to analyze (completed 2026-07-12)
- [x] **Phase 3: Core Chord-Detection Engine** - Headless engine detects key, tempo, and bar-aligned chord progression behind a swappable interface (completed 2026-07-13)
- [x] **Phase 4: Analysis UI Integration** - Detected key/tempo/chords appear live on the waveform timeline without freezing the UI (completed 2026-07-13)
- [x] **Phase 5: MIDI Conveyor Generation** - One analysis produces multiple simultaneous MIDI rows: as-is progression, style variants, and bass line (completed 2026-07-13)
- [ ] **Phase 6: Row Preview & Export** - User auditions any row and gets it into the DAW via drag-out or .mid save
- [ ] **Phase 7: Persistence & Multi-DAW Verification** - Plugin state survives DAW project reload; drag-and-drop export verified per-DAW

## Phase Details

### Phase 1: Plugin Foundation
**Goal**: A working, loadable plugin shell exists in all three formats and hosts, with real-time-safety and state-persistence discipline established from day one so it doesn't have to be retrofitted later.
**Depends on**: Nothing (first phase)
**Requirements**: PLT-01
**Success Criteria** (what must be TRUE):
  1. Plugin loads without crashing as VST3 in Ableton Live and FL Studio
  2. Plugin loads without crashing as AU in Logic Pro and passes `auval`
  3. Standalone app launches on macOS
  4. `processBlock` runs as a real-time-safe skeleton (zero allocation/locking/I-O) even though v1 does no live audio processing
  5. Plugin state round-trips through a save/reload of an empty session (`getStateInformation`/`setStateInformation` work)
**Plans**: 3/3 plans executed

Plans:
- [x] 01-01-PLAN.md — Toolchain install (cmake/ninja), JUCE 8.0.x submodule pin, CMake + APVTS plugin skeleton, first build of VST3/AU/Standalone (completed 2026-07-12)
- [x] 01-02-PLAN.md — Validation tooling (fetch-pluginval, standalone smoke script) + green auval/pluginval strictness-5 gate (completed 2026-07-12)
- [x] 01-03-PLAN.md — Manual DAW load matrix checkpoint (Ableton, FL Studio, Logic Pro, Standalone; cold scans) (completed 2026-07-12)

### Phase 2: Audio Import & Waveform
**Goal**: User can bring a reference song into the plugin and choose what portion to analyze.
**Depends on**: Phase 1
**Requirements**: IMP-01, IMP-02, IMP-03
**Success Criteria** (what must be TRUE):
  1. User can drag-and-drop a WAV, MP3, AIFF, or FLAC file onto the plugin/standalone window and it loads
  2. User sees the waveform of the loaded file rendered in the UI
  3. User can select a region on the waveform to constrain analysis to that region
  4. If no region is selected, the whole file is analyzed by default
**Plans**: 4/4 plans executed

Plans:
- [x] 02-01-PLAN.md — Test infrastructure (Catch2/CTest, MP3 fixture) + background audio-import backend with APVTS region state (completed 2026-07-12)
- [x] 02-02-PLAN.md — Pixel-art conveyor belt UI (drop target, Timer animation) + three-band editor layout + MIDI-sets placeholder (completed 2026-07-12)
- [x] 02-03-PLAN.md — Waveform display (AudioThumbnail) + region selection overlay + editor wiring (completed 2026-07-12)
- [x] 02-04-PLAN.md — Full regression gate + manual Standalone drag-and-drop verification checkpoint (completed 2026-07-12)

### Phase 3: Core Chord-Detection Engine
**Goal**: The plugin can correctly determine key, tempo, and chord progression from a decoded audio buffer, isolated behind an interface that a v2 ML backend can later replace without touching UI or generation code.
**Depends on**: Phase 2 (needs a decoded audio buffer/region to analyze)
**Requirements**: ANL-01, ANL-02, ANL-03, ANL-06
**Success Criteria** (what must be TRUE):
  1. Given a loaded song/region, the engine outputs a detected key
  2. Given a loaded song/region, the engine outputs a detected tempo (BPM) and bar grid
  3. Given a loaded song/region, the engine outputs a chord progression aligned to the bar grid
  4. Detection is invocable and verifiable through a `ChordAnalyzer` interface via a standalone test harness, independent of any UI
**Plans**: 6/6 plans executed

Plans:
- [x] 03-01-PLAN.md — Foundation: frozen ChordAnalyzer/AnalysisResult contracts, module skeletons + CMake wiring, constant-q-cpp (MIT, pinned) + THIRD_PARTY_LICENSES.md, synthetic fixtures, dual-rate preprocessing (Wave 1) (completed 2026-07-12)
- [x] 03-02-PLAN.md — Chroma path: CQT wrapper, tuning estimation, percussion suppression, dual harmonic+bass chroma fold (Wave 2) (completed 2026-07-12)
- [x] 03-03-PLAN.md — Tempo/beat path: Ellis 2007 onset envelope + weighted-autocorrelation tempo + DP beat backtrace + 4/4 bar grid (Wave 2) (completed 2026-07-12)
- [x] 03-04-PLAN.md — Key detection: Krumhansl-Kessler 24-profile correlation + audio-integration tests (Wave 3) (completed 2026-07-12)
- [x] 03-05-PLAN.md — Chord recognition: 36 binary templates, beat-sync averaging, bass-root bias, log-Viterbi, beat-aligned segments (Wave 3) (completed 2026-07-12)
- [x] 03-06-PLAN.md — ClassicDspChordAnalyzer facade (progress/cancel), performance budget test, real-track listening checkpoint (Wave 4) (completed 2026-07-13)

### Phase 4: Analysis UI Integration
**Goal**: User sees the engine's results live in the plugin, with the interface staying responsive throughout analysis.
**Depends on**: Phase 3
**Requirements**: ANL-04, ANL-05
**Success Criteria** (what must be TRUE):
  1. UI remains responsive (no freeze, no blocked window) while a song analyzes
  2. A progress/busy indicator is visible for the duration of analysis
  3. A 3-minute song completes analysis in seconds, not minutes
  4. Detected chords appear as named chords (e.g., Am, Cmaj7, F/A) on a timeline positioned over the waveform
     (v1 display vocabulary: root-position maj/min/dom7/N.C. — the Cmaj7/F/A examples are illustrative; frozen Phase 3 ChordQuality contract, see 04-RESEARCH.md Open Question 1)
**Plans**: 4/4 plans executed

Plans:
- [x] 04-01-PLAN.md — AnalysisPipeline ThreadPoolJob + generation-guarded cancel-and-restart on PluginProcessor (auto-trigger on load/region change) (Wave 1) (completed 2026-07-13)
- [x] 04-02-PLAN.md — ChordNameFormatter + ChordTimelineView band over the waveform + editor wiring (Wave 2) (completed 2026-07-13)
- [x] 04-03-PLAN.md — Conveyor progress fill + belt speed-up; chunk-fall moved to analysis-complete (Wave 3) (completed 2026-07-13)
- [x] 04-04-PLAN.md — One-time Release build + human checkpoint: real-track timing (2.78s for a real 75s track), responsiveness, timeline legibility (Wave 4) (completed 2026-07-13)

### Phase 5: MIDI Conveyor Generation
**Goal**: A single analysis pass fans out into several ready-to-use MIDI outputs in different styles, the product's core differentiator.
**Depends on**: Phase 3 (consumes `AnalysisResult`; can be stubbed with fixture data to start in parallel)
**Requirements**: GEN-01, GEN-02, GEN-03, GEN-04
**Success Criteria** (what must be TRUE):
  1. After analysis, user sees multiple MIDI rows at once: detected-as-is progression, style-voicing variants, and a bass line
  2. Pop/Hip-hop/Trap (triads, dark minor), R&B/Neo-soul (7th/9th/11th, smooth voice leading), and Electronic/House (stabs, rhythmic patterns) rows are each present and audibly distinct
  3. Style variants are built from the user's actual detected progression, not from static preset lookups
  4. The bass row follows the detected chord roots with style-appropriate rhythm
  5. Rows regenerate automatically when the user changes the analysis region or style settings
**Plans**: 6/6 plans executed

Plans:
- [x] 05-01-PLAN.md — MidiGen foundation: beat-domain NoteEvent/MidiSetRow model, frozen generator contracts, music-math helpers, struct-literal fixtures, one-time CMake wiring (Wave 1) (completed 2026-07-13)
- [x] 05-02-PLAN.md — As-is row + three style voicing engines (Pop/Trap triads, R&B extensions with voice leading, House off-beat stabs) with distinctness proofs (Wave 2) (completed 2026-07-13)
- [x] 05-03-PLAN.md — Bass line generator: root-following with trap sustain / R&B root-fifth walk / house four-on-the-floor rhythms (Wave 2) (completed 2026-07-13)
- [x] 05-04-PLAN.md — generateAllRows orchestrator (5 rows, determinism, <1ms budget) + synchronous PluginProcessor wiring with regenerate-on-region-change (Wave 3) (completed 2026-07-13)
- [x] 05-05-PLAN.md — MidiSetsPanel + MidiRowView mini piano-roll strips in the bottom band; MidiSetsPlaceholder deleted (Wave 4) (completed 2026-07-13)
- [x] 05-06-PLAN.md — Phase gate (full suite + pluginval) + human checkpoint: 5 rows visible/legible/distinct on a real track, regenerate on region drag (Wave 5) (completed 2026-07-13)

### Phase 6: Row Preview & Export
**Goal**: User can hear any generated row and get it out of the plugin into the DAW.
**Depends on**: Phase 5
**Requirements**: PRV-01, EXP-01, EXP-02, EXP-03
**Success Criteria** (what must be TRUE):
  1. User can audition any MIDI row with a built-in piano/pad sound before dragging it out
  2. User can drag any MIDI row directly from the plugin into the DAW's piano roll
  3. User can save any MIDI row to disk as a standalone .mid file (fallback path)
  4. MIDI produced by either export path is bar-aligned and carries the detected tempo
**Plans**: 4 plans

Plans:
- [ ] 06-01-PLAN.md — MidiFileWriter: beat-domain rows → format-1 SMF (TPQN 960, tempo + 4/4 meta), atomic write, MIDI-pack file naming, round-trip tests (Wave 1)
- [ ] 06-02-PLAN.md — Audition engine: deterministic AuditionVoice/Renderer pre-render + RT-safe double-buffer playback in processBlock, pluginval RT gate (Wave 2)
- [ ] 06-03-PLAN.md — Interactive rows: play/save gutter icons, drag-out with Ableton-safe deferred temp cleanup, async save dialog, stop-audition-on-regenerate wiring (Wave 3)
- [ ] 06-04-PLAN.md — Phase gate (suite + pluginval) + human checkpoint: drag into FL Studio piano roll, audition all rows, save dialog (Wave 4)

### Phase 7: Persistence & Multi-DAW Verification
**Goal**: The plugin's work survives a full DAW project save/reload, and the drag-out mechanic is confirmed solid in every target host — the release gate for v1.
**Depends on**: Phase 6 (rows and settings must exist to persist), Phase 1 (state plumbing)
**Requirements**: PLT-02, PLT-03
**Success Criteria** (what must be TRUE):
  1. Closing and reopening a DAW project restores the source file reference, analysis result, generated rows, and settings exactly as left
  2. Drag-and-drop MIDI export works correctly in Ableton Live, including after project reopen and other UI interaction
  3. Drag-and-drop MIDI export works correctly in FL Studio, including after project reopen and other UI interaction
  4. Drag-and-drop MIDI export works correctly in Logic Pro, including after project reopen and other UI interaction
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Plugin Foundation | 3/3 | Complete   | 2026-07-12 |
| 2. Audio Import & Waveform | 4/4 | Complete   | 2026-07-12 |
| 3. Core Chord-Detection Engine | 6/6 | Complete   | 2026-07-13 |
| 4. Analysis UI Integration | 4/4 | Complete   | 2026-07-13 |
| 5. MIDI Conveyor Generation | 6/6 | Complete   | 2026-07-13 |
| 6. Row Preview & Export | 0/4 | Planned | - |
| 7. Persistence & Multi-DAW Verification | 0/TBD | Not started | - |
