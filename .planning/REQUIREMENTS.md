# Requirements: ChordAI

**Defined:** 2026-07-12
**Core Value:** Продюсер закидає пісню-референс і за секунди отримує кілька готових до використання MIDI-акордових наборів у схожому стилі — без знання теорії музики і без ручного підбору на слух.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Import

- [x] **IMP-01**: User can drag-and-drop an audio file (WAV/MP3/AIFF/FLAC) onto the plugin/standalone window and it loads (MP3/AAC decoded via macOS CoreAudio)
- [ ] **IMP-02**: User sees the waveform of the loaded file
- [ ] **IMP-03**: User can select a region on the waveform for analysis, or analyze the whole file (default)

### Analysis

- [ ] **ANL-01**: Key of the analyzed region is detected automatically
- [ ] **ANL-02**: Tempo (BPM) and bar grid are detected automatically
- [ ] **ANL-03**: Chord progression is detected (chromagram + template matching + Viterbi smoothing) and aligned to the bar grid
- [ ] **ANL-04**: Analysis runs on a background thread — UI stays responsive, progress is shown; a 3-minute song analyzes in seconds
- [ ] **ANL-05**: Detected chords are displayed as named chords (Am, Cmaj7, F/A) on a timeline over the waveform
- [ ] **ANL-06**: Detection engine sits behind a `ChordAnalyzer` interface so an ML (ONNX) backend can replace the DSP backend in v2 without UI/generation changes

### Conveyor Generation

- [ ] **GEN-01**: One analysis pass simultaneously produces multiple MIDI rows: detected progression as-is + style-voicing variants + bass line
- [ ] **GEN-02**: Style variants v1 — Pop/Hip-hop/Trap (triads, dark minor), R&B/Neo-soul (7th/9th/11th, smooth voice leading), Electronic/House (stabs, rhythmic patterns) — applied to the *detected* progression, not preset lookups
- [ ] **GEN-03**: Bass row follows detected chord roots with style-appropriate rhythm
- [ ] **GEN-04**: Rows regenerate when the user changes the analysis region or style settings

### Preview

- [ ] **PRV-01**: User can audition any MIDI row with a built-in piano/pad sound before dragging it out

### Export

- [ ] **EXP-01**: User can drag any MIDI row from the plugin straight into the DAW piano roll (temp .mid + external file drag)
- [ ] **EXP-02**: User can save any MIDI row to disk as a .mid file (fallback path)
- [ ] **EXP-03**: Exported MIDI is bar-aligned and carries the detected tempo

### Platform

- [x] **PLT-01**: Project builds as VST3, AU, and Standalone app on macOS (JUCE 8, C++20, CMake)
- [ ] **PLT-02**: Plugin state (source file reference, analysis result, generated rows, settings) persists with the DAW project save/reload
- [ ] **PLT-03**: Drag-and-drop MIDI export verified in Ableton Live, FL Studio, Logic Pro (per-DAW test matrix)

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Input

- **INP-01**: Real-time "Listen" capture from the DAW track (streaming analysis mode)

### Analysis

- **ANL-07**: ML (ONNX Runtime) detection backend swap-in for higher accuracy
- **ANL-08**: Roman numeral display toggle alongside chord names

### Generation

- **GEN-05**: Expanded voicing-style library / more genre presets
- **GEN-06**: Preset favoriting and tagging of detected sessions

### Platform

- **PLT-04**: Windows build (VST3 + Standalone)
- **PLT-05**: Extended DAW verification (Studio One, Cubase, Reaper)
- **PLT-06**: Licensing/copy-protection + signed/notarized installer for commercial release

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Deep music-theory workbench (scale explorer, modulation tools) | Scaler 2's #1 complaint is UI overwhelm; wrong persona — target user has no theory background |
| Stem separation before detection | Heavy ML dependency, contradicts "seconds not minutes"; chromagram works on full mix |
| Cloud/account dependency for core features | Named competitor complaint (Captain Chords); studio must work offline |
| Melody/vocal transcription | Different, harder problem than chord estimation |
| One-click song-structure generation | Conflates arrangement with harmony; different product |
| MPE / live chord-performance triggering | Target user drags MIDI clips, doesn't perform live |
| Per-step humanization mixing console | Dilutes conveyor simplicity; humanization baked into style presets instead |
| GPL/AGPL DSP libraries (aubio, Essentia, Chordino, BTrack) | License-incompatible with closed-source commercial product — detection built in-house from MIT/BSD primitives |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PLT-01 | Phase 1 - Plugin Foundation | Complete |
| IMP-01 | Phase 2 - Audio Import & Waveform | Complete |
| IMP-02 | Phase 2 - Audio Import & Waveform | Pending |
| IMP-03 | Phase 2 - Audio Import & Waveform | Pending |
| ANL-01 | Phase 3 - Core Chord-Detection Engine | Pending |
| ANL-02 | Phase 3 - Core Chord-Detection Engine | Pending |
| ANL-03 | Phase 3 - Core Chord-Detection Engine | Pending |
| ANL-06 | Phase 3 - Core Chord-Detection Engine | Pending |
| ANL-04 | Phase 4 - Analysis UI Integration | Pending |
| ANL-05 | Phase 4 - Analysis UI Integration | Pending |
| GEN-01 | Phase 5 - MIDI Conveyor Generation | Pending |
| GEN-02 | Phase 5 - MIDI Conveyor Generation | Pending |
| GEN-03 | Phase 5 - MIDI Conveyor Generation | Pending |
| GEN-04 | Phase 5 - MIDI Conveyor Generation | Pending |
| PRV-01 | Phase 6 - Row Preview & Export | Pending |
| EXP-01 | Phase 6 - Row Preview & Export | Pending |
| EXP-02 | Phase 6 - Row Preview & Export | Pending |
| EXP-03 | Phase 6 - Row Preview & Export | Pending |
| PLT-02 | Phase 7 - Persistence & Multi-DAW Verification | Pending |
| PLT-03 | Phase 7 - Persistence & Multi-DAW Verification | Pending |

**Coverage:**
- v1 requirements: 20 total (corrected from initial count of 19 — recount at roadmap creation found 20 listed IDs)
- Mapped to phases: 20
- Unmapped: 0 ✓

---
*Requirements defined: 2026-07-12*
*Last updated: 2026-07-12 after roadmap creation (7 phases, full coverage)*
