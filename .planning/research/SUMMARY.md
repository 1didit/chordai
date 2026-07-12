# Project Research Summary

**Project:** ChordAI
**Domain:** Commercial audio plugin (VST3/AU/Standalone, JUCE/C++) — offline chord/key/tempo detection from a reference song + multi-style MIDI generation, macOS-first
**Researched:** 2026-07-12
**Confidence:** MEDIUM-HIGH

## Executive Summary

ChordAI is a commercial JUCE/C++ audio plugin that behaves as a "song in, MIDI sets out" conveyor: a producer drops an audio file, the plugin detects key/tempo/chord progression offline, and fans that single detection out into several parallel, independently-draggable MIDI outputs (detected-as-is, style-voicing variants, bass line). This is a well-precedented category — Scaler 2, Captain Chords, HoRNet SongKey, and RipX all solve pieces of it — but no surveyed competitor does the "one detection pass → multiple simultaneous finished MIDI sets" batch conveyor; that is ChordAI's actual differentiator and should be treated as the P1 feature that makes the product worth building, not a stretch goal. The recommended approach is JUCE 8 + CMake + C++20 for the plugin shell, a hand-rolled chromagram+Viterbi/HMM chord-detection pipeline built entirely from permissively-licensed building blocks (JUCE DSP + constant-q-cpp, MIT), and a strict `ChordAnalyzer` interface boundary so a v2 ONNX backend can be swapped in later without touching UI or MIDI-generation code — a boundary already decided in PROJECT.md and reinforced independently by both the architecture and stack research.

The single biggest risk is legal, not technical: the most-cited MIR libraries for exactly this problem (aubio, Essentia, NNLS Chroma/Chordino, BTrack) are GPL/AGPL and cannot be linked into a closed-source commercial product without a paid/negotiated license. This must be resolved before any DSP code is written, not discovered during a later audit. The second-tier risks are domain-specific DSP correctness issues that are easy to get "looks right in isolation, wrong in aggregate": full-mix chroma without tuning/percussion handling, octave-blind bass/root detection, chord-segmentation flicker without beat-sync, and half-time/double-time tempo errors — all concentrated in the same "core chord-detection engine" phase and all fixable at the feature-extraction/aggregation layer rather than requiring different algorithms downstream. The third risk category is JUCE/host-integration correctness: real-time-safety in `processBlock`, DAW-specific drag-and-drop quirks (Ableton crash reports, FL Studio callback-not-firing reports), AU validation (`auval`) failures that silently blacklist the plugin, and state-persistence across a full DAW quit/reopen — all well-documented, all avoidable with disciplined patterns established from day one rather than retrofitted late.

The recommended approach mitigates both risk tiers structurally: license risk is closed by a hard "MIT/BSD/Apache/zlib only, or in-house" rule enforced from the first dependency added, and DSP/host risk is closed by the suggested build order — stand up threading, drag-out, state persistence, and AU validation as architectural rules established at the plugin-shell stage, and build/validate the DSP core in an isolated, unit-testable module (console/test harness) before wiring it into UI/threading. Given the domain's published accuracy ceiling (~75-85% frame-level even in academic state-of-the-art), the product should be explicitly framed to users as "a fast starting point to drag and shape," not "perfect transcription" — this is a product-framing decision as much as an engineering one, and it directly protects the core value proposition from unrealistic-accuracy backlash.

## Key Findings

### Recommended Stack

Core plugin framework is JUCE 8.0.13 + CMake ≥3.22 (practically 3.25+) + C++20, targeting VST3/AU/Standalone from one codebase — the industry-standard combination, verified against official JUCE docs and current community templates (pamplejuce). Licensing is the critical constraint: JUCE's free tier is now AGPLv3 (requires open-sourcing the product), so ChordAI must run on the commercial track — Starter (free, <$20k/yr revenue, closed-source) during development, budgeted upgrade to Indie ($40/mo or $800 one-time, <$300k/yr) before/at commercial launch.

**Core technologies:**
- JUCE 8.0.13 (commercial Starter→Indie tier): plugin framework, audio I/O, GUI, MIDI, drag-and-drop — one codebase for VST3+AU+Standalone
- CMake ≥3.25: build system, current standard workflow (Projucer is legacy)
- `juce::CoreAudioFormat` + `juce::dsp::FFT` (wraps Accelerate/vDSP): macOS-native, zero-extra-cost decode (WAV/AIFF/FLAC/MP3/AAC) and FFT for v1's macOS-only scope
- constant-q-cpp (MIT, QMUL): Constant-Q Transform / chromagram engine — the standard MIR building block, license-verified permissive, purpose-built by the same research group behind the chord-recognition literature
- Custom Viterbi/HMM chord matcher + custom onset/tempo detection: published algorithms (Bello & Pickens 2005; Sheh & Ellis 2003), implemented in-house specifically to avoid GPL/AGPL contamination from aubio/BTrack/NNLS Chroma
- `juce::MidiFile` + `DragAndDropContainer::performExternalDragDropOfFiles` (both JUCE built-in, the latter confirmed `static`): MIDI file generation and the temp-file drag-to-DAW mechanism
- pluginval (Tracktion, run out-of-process — GPL doesn't propagate) + Catch2/CTest: validation and testing tooling

### Expected Features

The conveyor batch-output concept (one detection pass → detected-as-is + N style-voicing variants + bass line, all generated together and independently draggable) has no direct precedent among surveyed competitors and is the product's core differentiator — everything else is table stakes that must exist for the product to be taken seriously, but doesn't by itself justify the product.

**Must have (table stakes):**
- Audio file drag-and-drop import (WAV/MP3/AIFF/FLAC)
- Key + tempo autodetection, chord detection from audio (chromagram+Viterbi, bar-grid aligned)
- Chord name display, built-in audition sound (piano/pad) per row
- MIDI drag-and-drop export to DAW from every row + `.mid` save-to-disk fallback
- Plugin state save/restore with DAW project (Captain Chords' worst-reviewed flaw — treat as non-negotiable)
- Multi-DAW compatibility verified individually in Ableton Live, FL Studio, Logic Pro

**Should have (competitive differentiators):**
- Conveyor batch output (as-is + style variants + bass, generated together) — the reason the product exists
- Waveform display with selectable analysis region — answers a specifically named competitor complaint (Scaler's whole-file-only detection)
- Genre-specific voicing engine applied to the user's *actual* detected progression, not a static preset browser
- Zero-theory workflow (no key/scale/roman-numeral controls exposed) — directly avoids Scaler 2's #1 complaint ("most confusing plugin")
- Fully offline/local, no account — directly avoids Captain Chords' #1 complaint ("must be connected to internet")

**Defer (v2+):**
- Real-time "Listen" DAW-track capture, ML/ONNX detection backend swap-in, melody/arp generation module, stem-aware detection — all correctly already Out of Scope in PROJECT.md
- Full music-theory workbench (scale explorer, modulation tools) — explicitly a "Won't build," wrong persona fit

### Architecture Approach

The system splits into four isolated layers connected only through narrow, well-defined boundaries: a message-thread UI layer, a near-empty audio thread (v1 doesn't require live audio processing), a background `juce::ThreadPool`-based analysis pipeline behind a `ChordAnalyzer` strategy interface, and a pure-logic MIDI generation engine with zero JUCE-audio dependency. The `ChordAnalyzer` interface and the `AnalysisResult` immutable value object are the two load-bearing abstractions in the whole architecture — they are what let the ML-ready backend swap (a stated PROJECT.md Key Decision) happen later without touching UI or MidiGen code, and what let MidiGen be unit-tested without a running plugin instance.

**Major components:**
1. `ChordAnalyzer` (interface) + `ClassicDspChordAnalyzer` (v1 impl) — chromagram → chord template + Viterbi → beat/key detection, swappable for `OnnxChordAnalyzer` in v2 with no other component change
2. `AnalysisPipeline` (ThreadPoolJob) — orchestrates decode→resample→CQT→chroma→HMM→beat-grid, publishes an immutable `AnalysisResult` via `std::atomic<std::shared_ptr<const AnalysisResult>>` + `AsyncUpdater`, never blocks the message thread
3. `MidiGenEngine` (ProgressionModel, VoicingPresets, VoiceLeadingEngine, BassLineGenerator) — pure C++ logic consuming only `AnalysisResult`, generates `juce::MidiFile`s per row, no DSP/JUCE-audio coupling
4. UI (WaveformView/AudioThumbnail, ChordTimelineView, MidiRowComponent drag sources) — message-thread only, reads finished `AnalysisResult` snapshots, never reaches into analyzer internals
5. `StateManager` (APVTS + sibling session ValueTree) — single place for all persistence, defensively validated for host call-order quirks

### Critical Pitfalls

1. **GPL/AGPL contamination from off-the-shelf MIR libraries (aubio, Essentia, madmom weights, NNLS Chroma)** — the most-cited chord/beat-detection libraries in this exact domain are all copyleft-incompatible with a closed-source commercial product. Avoid by building the pipeline from MIT/BSD-licensed primitives (JUCE DSP, constant-q-cpp) plus in-house Viterbi/template-matching code, and license-audit every dependency before it's added — this is a legal gate at the stack-selection phase, not a later refactor.
2. **Full-mix chromagram noise + bass-register root/inversion errors (Pitfalls 2-3)** — naive STFT chroma on real mixed masters (drums, sub-bass, off-A440 tuning) produces flickering/wrong chord labels, and octave-blind chroma can't distinguish root position from inversions. Avoid with CQT-based chroma, tuning correction, and a dedicated low-frequency bass chromagram feeding root disambiguation.
3. **Segmentation flicker and tempo octave errors without beat-sync (Pitfalls 4-5)** — frame-level chord classification without beat aggregation produces musically meaningless chattering output, and autocorrelation-based tempo estimation on syncopated genres (hip-hop/trap — this product's exact target) commonly locks onto half/double the true tempo. Avoid by beat-synchronizing chroma before Viterbi decoding and applying explicit octave-range correction to tempo estimates.
4. **Real-time-unsafe code in `processBlock` + UI-thread-blocking synchronous analysis (Pitfalls 6, 10)** — allocation/locks/file I/O on the audio thread causes dropouts/crashes; calling the analyzer inline from a drag-drop callback freezes the plugin window on full-length songs. Avoid by establishing the three-scope rule (`prepareToPlay`/`processBlock`/background) and the `ThreadPoolJob`+`AsyncUpdater` handoff pattern from the very first `processBlock`/analysis code, not retrofitted later.
5. **DAW-specific drag-and-drop breakage and AU validation blacklisting (Pitfalls 8, 11)** — `performExternalDragDropOfFiles` has documented crash/no-op reports in Ableton and FL Studio specifically, and AU builds can be silently blacklisted by a cold `auval`/plugin-cache-scan failure that never shows up in day-to-day cached testing. Avoid with a per-DAW manual test matrix (including post-reopen, post-other-UI-interaction cases) plus a guaranteed `.mid` file-export fallback, and a regular `auval`+cold-scan pass alongside `pluginval` strict mode.

## Implications for Roadmap

Based on combined research, the dependency chain is: plugin shell (with real-time-safety and state-persistence rules baked in from day one) → file import/decode/waveform → chord-detection DSP core (isolated, unit-tested standalone before UI wiring) → analysis-result UI integration → MIDI generation engine (can be built in parallel once `AnalysisResult`'s shape is stubbed) → drag-out/export → session persistence hardening → packaging/signing. The DSP core is the single highest-uncertainty, most research-dependent phase and should be isolated with a console/test harness so algorithm iteration doesn't get tangled with JUCE threading/UI debugging.

### Phase 1: Plugin Shell & Real-Time-Safety Foundation
**Rationale:** Everything downstream depends on a working VST3/AU/Standalone build; real-time-safety, state-persistence, and AU-validation rules are cheap to establish now and expensive to retrofit once analysis/UI code is entangled with the audio thread (Pitfalls 6, 11, 12).
**Delivers:** Empty-UI plugin loading in Ableton/FL Studio/Logic Pro, `processBlock` doing zero allocation/locking/I-O, basic `getStateInformation`/`setStateInformation` round-trip, `auval`+`pluginval` passing in CI.
**Addresses:** VST3/AU/Standalone builds requirement (PROJECT.md Active), verified in target DAWs.
**Avoids:** Pitfall 6 (real-time-unsafe `processBlock`), Pitfall 11 (AU validation blacklisting), Pitfall 12 (state persistence breaking on reload), Pitfall 13 (signing/notarization deferred too long — at least a smoke-test signed build here).

### Phase 2: Audio Import, Decode & Waveform Display
**Rationale:** Nothing in the detection pipeline can start without a decoded audio buffer; this phase is architecturally simple and low-risk relative to Phase 3, and directly answers a named competitor gap (waveform region selection).
**Delivers:** Drag-and-drop file import, background-thread decode via `CoreAudioFormat`, `AudioThumbnail`-based waveform view with selectable analysis region.
**Uses:** `juce::CoreAudioFormat`, `juce::dsp::FFT`/`AudioThumbnail` (Stack).
**Implements:** `AudioFileLoader` (Utils/), WaveformView (UI/) — Architecture component.

### Phase 3: Core Chord-Detection Engine (Chromagram → Viterbi → Beat/Key/Tempo)
**Rationale:** Highest-uncertainty, most research-dependent component in the whole project — a domain problem, not just an engineering one. Must be built and validated standalone (test harness, genre-matched real-world tracks) before wiring into threading/UI, per the suggested build order.
**Delivers:** `ChordAnalyzer` interface + `ClassicDspChordAnalyzer` implementation (CQT chromagram with tuning correction, dedicated bass chromagram, beat-synchronized aggregation, Viterbi/HMM smoothing, octave-corrected tempo estimation), wrapped in a `ThreadPoolJob` publishing immutable `AnalysisResult` snapshots.
**Addresses:** Key/tempo detection, chord detection from audio (FEATURES.md table stakes and P1 items).
**Avoids:** Pitfall 1 (GPL/AGPL contamination — build on constant-q-cpp/MIT + in-house Viterbi only), Pitfalls 2-3 (chroma noise, bass/root errors), Pitfall 4 (segmentation flicker), Pitfall 5 (tempo octave error), Pitfall 10 (UI-thread blocking — background from day one).

### Phase 4: Analysis Result UI Integration
**Rationale:** Depends on Phase 3's `AnalysisResult` shape stabilizing; surfacing detected key/tempo/chords in the UI before export is also a direct mitigation for silent wrong-tempo/wrong-key exports (Pitfall 5's UX corollary).
**Delivers:** `ChordTimelineView` rendering detected chords/beat grid over the waveform, async-publish/repaint wiring (`AsyncUpdater`), progress/busy UI during analysis.
**Implements:** Analysis-thread → message-thread handoff pattern (Architecture Pattern 2).
**Avoids:** Pitfall 10 (no visible feedback during multi-second analysis).

### Phase 5: MIDI Generation Engine (Conveyor Core)
**Rationale:** Pure logic with zero JUCE-audio dependency — can be developed and unit-tested in parallel with Phase 3/4 once `AnalysisResult`'s struct shape is drafted (stub with fixture data to unblock). This is the product's actual differentiator per FEATURES.md and must not be scoped down to "just detection."
**Delivers:** `ProgressionModel`, `VoicingPresetTable` (Pop/Trap, R&B/Neo-soul, Electronic/House), `VoiceLeadingEngine`, `BassLineGenerator`, `MidiRowBuilder` producing one `juce::MidiFile` per conveyor row.
**Addresses:** Conveyor batch output, genre-specific voicing, auto-generated bass line (FEATURES.md P1 differentiators).
**Avoids:** Pitfall 3's corollary (explicit product decision on inversions/slash chords), Anti-Pattern 2 (coupling MidiGen to DSP internals — enforce `AnalysisResult`-only dependency).

### Phase 6: MIDI Drag-Out Export & Multi-DAW Verification
**Rationale:** Depends on Phase 5's `MidiFile` objects existing; this is the "out the other side" mechanic that makes the conveyor usable, and is the single most host-fragile integration point found in research.
**Delivers:** `MidiRowComponent` drag sources using `performExternalDragDropOfFiles` with temp `.mid` files, plus `.mid` save-to-disk fallback, tested individually (not just once) in Ableton Live, FL Studio, and Logic Pro including post-reopen and post-other-UI-interaction scenarios.
**Addresses:** MIDI drag-and-drop export to DAW, `.mid` file save-to-disk fallback (FEATURES.md P1).
**Avoids:** Pitfall 7 (mistakenly attempting live MIDI-out routing — stay drag-and-drop only), Pitfall 8 (DAW-specific drag breakage).

### Phase 7: Session Persistence Hardening & Built-In Audition Sound
**Rationale:** Should happen after `AnalysisResult`'s shape has stabilized (Phase 3/4) to avoid repeated serialization-format migrations; the audition sound depends on conveyor rows existing (Phase 5) but must not be an afterthought since it's judged within seconds of first use.
**Delivers:** APVTS + session ValueTree persistence of source file ref, cached `AnalysisResult`, per-row voicing settings — validated across a full DAW quit/reopen, not just in-session undo/redo; one solid built-in piano/pad sampler for per-row preview.
**Addresses:** Session/state persistence (FEATURES.md P1 — Captain Chords' worst-reviewed flaw), built-in audition sound (P1).
**Avoids:** Pitfall 12 (state persistence breaking on reload).

### Phase 8: Packaging, Signing & Release Hardening
**Rationale:** Cross-cutting concern that should be validated early (smoke-test signed build in Phase 1) but finalized last, as a release gate — Apple review/certificate turnaround makes this expensive to discover late.
**Delivers:** Developer ID signing + hardened runtime + notarization/stapling for VST3, AU, and Standalone individually; final `auval`/`pluginval` strict-mode pass; cold plugin-cache scan verification in each target DAW.
**Avoids:** Pitfall 13 (signing/notarization treated as late afterthought).

### Phase Ordering Rationale

- **DSP core isolated before UI wiring:** Phase 3 is deliberately positioned so the highest-uncertainty component (chord detection accuracy) can be iterated in a standalone test harness without simultaneously debugging JUCE threading/UI — this directly follows the "Suggested Build Order" rationale in ARCHITECTURE.md.
- **MidiGen can start early via stubbed `AnalysisResult`:** Phase 5 doesn't strictly need to wait for Phase 3 to be fully accurate, only for the struct shape to be drafted — flagged as an explicit parallelization opportunity in research, worth exploiting in roadmap sequencing/staffing.
- **Persistence hardening deferred until after `AnalysisResult` stabilizes:** avoids serializing a data shape that's still changing, per ARCHITECTURE.md's explicit warning against "repeated format migrations."
- **Real-time-safety and AU-validation rules established at Phase 1, not retrofitted:** every pitfall source (both PITFALLS.md and ARCHITECTURE.md's Anti-Patterns) agrees retrofitting threading/safety discipline after code has grown synchronous/coupled assumptions is expensive — this is the strongest cross-file consensus in the research.
- **Legal/licensing gate precedes all DSP code:** Pitfall 1 is explicitly the earliest-addressable pitfall in PITFALLS.md's own mapping table and directly shapes which libraries Phase 3 is allowed to use — it's a stack-selection decision already resolved in STACK.md, but the roadmap should treat it as a hard checkpoint before Phase 3 begins, not an assumption.

### Research Flags

Needs research during phase planning:
- **Phase 3 (Core Chord-Detection Engine):** Domain-specific DSP/MIR algorithm design (CQT parameterization, HMM state/transition design, tuning-correction specifics, tempo octave-correction heuristics) — sparse canonical guidance beyond academic papers and FMP notebooks; recommend `/gsd:research-phase` before implementation to firm up the specific chroma/HMM parameters and test-set methodology.
- **Phase 6 (MIDI Drag-Out Export):** Host-specific quirks (Ableton crash triggers, FL Studio callback-not-firing reports) are documented as known issues but not as a definitive fix list — recommend a research/spike pass to build the specific per-DAW test matrix before considering this phase done.
- **Phase 8 (Packaging/Signing):** Apple's notarization process is stable but has enough moving parts (agreement re-acceptance, per-format signing order) that a dedicated walkthrough during planning is worthwhile, even though the mechanics themselves are well-documented.

Phases with standard, well-documented patterns (skip deep research-phase):
- **Phase 1 (Plugin Shell):** Standard JUCE/CMake/pluginval scaffolding, extensively covered by official docs and the pamplejuce reference template.
- **Phase 2 (Audio Import/Waveform):** `AudioFormatReader`/`AudioThumbnail` are official, tutorial-documented JUCE APIs.
- **Phase 5 (MIDI Generation Engine):** Pure C++ logic with no exotic dependencies; voicing-rule design is a product/music decision more than a technical-research one.
- **Phase 7 (Session Persistence):** `AudioProcessorValueTreeState` pattern is officially documented and widely used.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | JUCE/CMake/licensing verified against official juce.com pricing, GitHub releases, and license files directly; MEDIUM on ONNX-integration specifics and notarization tooling (community sources only, no official joint docs) |
| Features | MEDIUM-HIGH | Table stakes and competitor feature sets cross-verified across multiple official product pages and independent reviews; MIREX accuracy figures are extrapolated academic benchmarks, and the "no direct conveyor competitor" claim is an absence-of-evidence finding (not exhaustively provable) |
| Architecture | MEDIUM-HIGH | JUCE threading/API patterns HIGH (official docs + forum-verified), MIR chord-detection pipeline shape HIGH (academic sources), but the specific project component split is MEDIUM — synthesized from patterns rather than a single canonical reference for this exact "song-in, MIDI-set-out" plugin category |
| Pitfalls | HIGH | JUCE/DAW mechanics and licensing verified against official docs, GitHub license files, and forum threads; MIR/DSP failure modes verified against academic literature and library documentation |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- **Exact CQT/chroma parameterization and HMM design for Phase 3** are not resolved by this research (they're algorithm-design decisions, not library-selection ones) — plan a `/gsd:research-phase` or algorithm-design spike before implementation, using constant-q-cpp's reference `Chromagram.cpp` as a starting point.
- **Realistic accuracy target validation** — the ~75-80% accuracy figure already in PROJECT.md's Key Decisions is consistent with published MIREX benchmarks, but has not been validated against a genre-matched test set (pop/hip-hop/trap, R&B/neo-soul, EDM per PROJECT.md's target audience) — build this test set early in Phase 3, not as an afterthought.
- **Per-DAW drag-and-drop failure modes** (Ableton crash triggers, FL Studio callback issues) are documented as *known risk categories* from forum reports, not as a definitive root-cause/fix list — treat Phase 6 as needing its own investigation pass, not just implementation against a known-good pattern.
- **ONNX/v2 integration specifics** (exact linking pattern, model format, inference threading) are only MEDIUM confidence, sourced from community reports of shipped plugins rather than official documentation — acceptable to leave unresolved now since it's explicitly v2 scope, but should not be assumed "solved" when that phase arrives.
- **Windows-porting stack swaps** (MP3AudioFormat, pffft, rt-cqt) are noted as directional guidance only, not validated in depth — acceptable since Windows is explicitly Out of Scope for v1 per PROJECT.md, revisit when that phase is actually scheduled.

## Sources

### Primary (HIGH confidence)
- https://github.com/juce-framework/JUCE — official docs, releases, CMake API, license source files (`juce_MP3AudioFormat.h`, `CoreAudioFormat`, `DragAndDropContainer`)
- https://juce.com/get-juce/ — official JUCE 8 license tiers and pricing
- https://github.com/cannam/constant-q-cpp/blob/master/COPYING, https://github.com/jmerkt/rt-cqt/blob/main/LICENSE, https://github.com/berndporr/kiss-fft/blob/master/LICENSE — direct license file reads
- https://aubio.org/, https://github.com/aubio/aubio/blob/master/COPYING, https://github.com/adamstark/BTrack, https://github.com/c4dm/nnls-chroma, https://essentia.upf.edu/licensing_information.html, https://github.com/CPJKU/madmom/blob/main/LICENSE — GPL/AGPL/CC-NC license confirmations
- https://docs.juce.com/master/ (AudioProcessor, AudioProcessorValueTreeState, ThreadPoolJob, ThreadPool, dsp::FFT, DragAndDropContainer, CoreAudioFormat) — official class references
- https://github.com/Tracktion/pluginval — official repo, strictness levels, JUCE 8 support
- https://www.audiolabs-erlangen.de/resources/MIR/FMP/C5/C5S3_ChordRec_HMM.html, https://ccrma.stanford.edu/~kglee/pubs/klee-ismir06.pdf — academic chroma→HMM→Viterbi reference implementations

### Secondary (MEDIUM-HIGH confidence)
- forum.juce.com threads on MIDI drag-out, ONNX integration, real-time-safety, AU/drag crash reports (multiple threads, cross-checked)
- https://github.com/sudara/pamplejuce — widely-adopted community JUCE+CMake+Catch2+pluginval template
- Competitor product pages and reviews (Plugin Boutique Scaler 2/3, Mixed In Key Captain Chords, HoRNet SongKey, Hit'n'Mix RipX, Hexachords Orb Producer, Audiomodern Chordjam, Unison Audio Chord Genie) — official pages cross-checked against independent reviews (Sound on Sound, MusicRadar, KVR Audio forum)
- MIREX Audio Chord Detection Task wiki, Vicar "Automatic Chord Estimation from Audio" review — academic accuracy-ceiling benchmarks

### Tertiary (LOW-MEDIUM confidence, flagged for validation)
- JUCE forum reports of Logic Pro 12.2.1 / Ableton / FL Studio drag-and-drop regressions — community-reported, not officially confirmed by JUCE or the DAW vendors, needs direct verification during Phase 6
- Tech journalism on MP3 patent expiry (hackaday.com) — not a legal opinion, informational context only
- melatonin.dev / KVR notarization how-to threads — community/Apple-adjacent, not Apple's own docs directly

---
*Research completed: 2026-07-12*
*Ready for roadmap: yes*
