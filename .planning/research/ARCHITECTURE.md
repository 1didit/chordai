# Architecture Research

**Domain:** JUCE audio plugin (VST3/AU/Standalone) — offline chord detection + MIDI generation with drag-out to DAW
**Researched:** 2026-07-12
**Confidence:** MEDIUM-HIGH (JUCE threading/API patterns HIGH — verified against JUCE docs/forum; MIR chord-detection pipeline HIGH — verified against academic sources; project-specific component split MEDIUM — synthesized from patterns, not a single canonical reference for "song-in, MIDI-set-out" plugins)

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│  HOST INTEGRATION LAYER (VST3 / AU / Standalone — JUCE wrapper)       │
│  Owns process lifecycle; hosts calls processBlock(), getState, etc.   │
├──────────────────────────────────────────────────────────────────────┤
│  MESSAGE THREAD                         │  AUDIO THREAD                │
│  ┌────────────────────────────────┐    │  ┌────────────────────────┐ │
│  │ PluginEditor (UI)               │    │  │ PluginProcessor          │ │
│  │  - WaveformView (AudioThumbnail)│    │  │  processBlock()          │ │
│  │  - ChordTimelineView            │    │  │  - preview playback only │ │
│  │  - MidiRowList (drag sources)   │    │  │  - NO analysis here      │ │
│  │  - Transport/controls           │    │  │  - lock-free param reads │ │
│  └───────────────┬──────────────────┘    │  └────────────┬─────────────┘ │
│                  │ APVTS attachments,     │               │ atomics /     │
│                  │ AsyncUpdater callbacks │               │ APVTS params  │
├──────────────────┴─────────────────────────────────────────┴────────────┤
│  ANALYSIS THREAD(S) — juce::ThreadPool (background, off message+audio)  │
│  ┌───────────────────────────────────────────────────────────────┐    │
│  │ AnalysisPipeline (ThreadPoolJob)                                │    │
│  │  Decode → Resample → STFT/CQT → Chromagram → Chord HMM/Viterbi  │    │
│  │  → Beat/Tempo/Key detect → Beat-grid alignment → AnalysisResult │    │
│  │  behind ChordAnalyzer interface (swappable DSP/ML backend)      │    │
│  └───────────────────────────┬───────────────────────────────────┘    │
│                               │ immutable AnalysisResult (handoff)      │
├───────────────────────────────┴──────────────────────────────────────────┤
│  MIDI GENERATION ENGINE (pure logic, runs on message thread — fast)     │
│  ProgressionModel → VoicingPresets (Pop/Trap, R&B/Neo-soul, House)      │
│  → VoiceLeadingEngine → BassLineGenerator → juce::MidiFile per row      │
├───────────────────────────────────────────────────────────────────────┤
│  STATE / SESSION LAYER                                                  │
│  APVTS (automatable params) + custom ValueTree branch (session data:    │
│  source file ref, AnalysisResult cache, generated row settings)         │
└───────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|-------------------------|
| PluginProcessor (`AudioProcessor`) | Host integration, `processBlock()`, parameter registration, state save/load, owns AnalysisEngine + MidiGenEngine instances | Subclass of `juce::AudioProcessor`; runs on **audio thread** for `processBlock`, but the object itself is also touched from the message thread (state, editor creation) — internal cross-thread guards required |
| PluginEditor (`AudioProcessorEditor`) | All UI: waveform, chord timeline, MIDI row list, controls | Subclass of `juce::AudioProcessorEditor`; **message thread only**; talks to Processor only via APVTS attachments + a thread-safe result pointer, never touches DSP state directly |
| WaveformView | Renders waveform, lets user select analysis range | `juce::AudioThumbnail` + `AudioThumbnailCache`, async-loads and repaints via its own `ChangeBroadcaster` (verified: JUCE tutorial) |
| ChordTimelineView | Renders detected chord/beat grid over time | Custom `Component`, reads from immutable `AnalysisResult` snapshot, repaints on `AsyncUpdater` callback from analysis completion |
| MidiRowList / MidiRowComponent | One row per style variant; each row is a `DragAndDropContainer` source | `juce::Component` + `mouseDrag()` override calling `performExternalDragDropOfFiles()` with a temp `.mid` path (verified pattern, JUCE forum) |
| AnalysisPipeline | Decode → resample → STFT/CQT → chroma → HMM/Viterbi → beat/key → beat-grid align | Runs inside a `juce::ThreadPoolJob::runJob()`; must poll `shouldExit()` for cancellation (verified: JUCE ThreadPool docs) |
| ChordAnalyzer (interface) | Abstraction boundary: audio+sampleRate in, chord/beat/key result out | Pure interface (Strategy pattern); `ClassicDspChordAnalyzer` (chroma+HMM) now, `OnnxChordAnalyzer` later — no other component depends on the concrete backend |
| AnalysisResult | Immutable value object: chord sequence, beat grid, tempo, key, confidence | Plain data struct (or `juce::ValueTree` snapshot), handed across threads via `std::atomic<std::shared_ptr<const AnalysisResult>>` (constructed once, never mutated) |
| MidiGenEngine | Turns AnalysisResult + style choice into concrete MIDI notes | Pure C++ logic: `ProgressionModel`, `VoicingPresetTable`, `VoiceLeadingEngine` (nearest-voicing minimization), `BassLineGenerator` — no JUCE audio dependency, easily unit-testable |
| StateManager | Persists plugin state across DAW save/reload | `AudioProcessorValueTreeState` for automatable params + a sibling `ValueTree` branch under the same root for session data (source file ref, cached AnalysisResult, row configs) — `apvts.state` can hold extra properties (verified: JUCE forum) |

## Recommended Project Structure

```
Source/
├── PluginProcessor.h/.cpp     # AudioProcessor — audio thread entry, owns engines
├── PluginEditor.h/.cpp        # AudioProcessorEditor — message thread entry
├── Analysis/
│   ├── ChordAnalyzer.h        # abstract interface (ML-ready boundary)
│   ├── ClassicDspAnalyzer.*   # chroma + HMM/Viterbi implementation (v1)
│   ├── Chromagram.*           # STFT/CQT → 12-bin pitch class profile
│   ├── ChordHmm.*             # chord template states + Viterbi decode
│   ├── BeatTracker.*          # tempo/beat-grid detection
│   ├── AnalysisResult.h       # immutable result value object
│   └── AnalysisPipeline.*     # ThreadPoolJob orchestrating the above
├── MidiGen/
│   ├── ProgressionModel.*     # progression variation logic
│   ├── VoicingPresets.*       # Pop/Trap, R&B/Neo-soul, House tables
│   ├── VoiceLeadingEngine.*   # minimal-motion voicing transitions
│   ├── BassLineGenerator.*
│   └── MidiRowBuilder.*       # AnalysisResult+style → juce::MidiFile
├── UI/
│   ├── WaveformView.*
│   ├── ChordTimelineView.*
│   ├── MidiRowComponent.*     # drag source
│   └── LookAndFeel.*
├── State/
│   ├── StateManager.*         # APVTS + session ValueTree glue
│   └── Parameters.*           # parameter ID/layout definitions
└── Utils/
    ├── AudioFileLoader.*      # decode + resample off the audio thread
    └── ThreadSafety.*         # small atomics/FIFO helpers for cross-thread handoff
```

### Structure Rationale

- **Analysis/ is isolated from UI/ and MidiGen/:** enforces the ML-ready boundary from PROJECT.md — swapping `ClassicDspAnalyzer` for an ONNX backend later touches only this folder plus the interface, never UI or MIDI generation.
- **MidiGen/ has zero JUCE-audio dependency (only `juce::MidiFile`/`MidiMessage`):** keeps the music-theory logic unit-testable without instantiating a full plugin/audio device, and reusable if a companion desktop app or CLI is ever built.
- **UI/ never imports Analysis/ internals**, only `AnalysisResult.h` — the UI reads a finished snapshot, it does not reach into pipeline internals.
- **State/ centralizes all persistence** so both `getStateInformation`/`setStateInformation` and any future "save session" feature go through one place, avoiding scattered ad-hoc XML/ValueTree code.

## Architectural Patterns

### Pattern 1: Strategy interface for the analysis backend (ChordAnalyzer)

**What:** `ChordAnalyzer` is a pure abstract class; `ClassicDspChordAnalyzer` is the only implementation in v1. All callers (AnalysisPipeline, UI, MidiGen) depend only on the interface and `AnalysisResult`, never on chroma/HMM internals.
**When to use:** Any time a v2 ML backend (ONNX) is a stated goal (it is, per PROJECT.md) — build the seam before you need it, since retrofitting it after UI/MidiGen code has coupled to DSP internals is expensive.
**Trade-offs:** Slight indirection overhead for a benefit realized only in v2; worth it here because the interface is explicitly a Key Decision in PROJECT.md.

**Example:**
```cpp
class ChordAnalyzer
{
public:
    virtual ~ChordAnalyzer() = default;
    // audioBuffer is already decoded+resampled; job used for cooperative cancellation
    virtual AnalysisResult analyse (const juce::AudioBuffer<float>& audioBuffer,
                                     double sampleRate,
                                     juce::ThreadPoolJob& job) = 0;
};

class ClassicDspChordAnalyzer : public ChordAnalyzer
{
public:
    AnalysisResult analyse (const juce::AudioBuffer<float>&, double, juce::ThreadPoolJob&) override;
    // chromagram -> chord template match -> Viterbi -> beat grid, internally
};
```

### Pattern 2: Background analysis job with immutable-snapshot handoff

**What:** Analysis runs inside a `juce::ThreadPoolJob`. On completion it builds a complete, immutable `AnalysisResult` and publishes it via `std::atomic<std::shared_ptr<const AnalysisResult>>` (or a lock-free single-slot FIFO), then triggers `AsyncUpdater::triggerAsyncUpdate()` so the Editor repaints on the message thread.
**When to use:** Any time work exceeds a few milliseconds and must not block the message thread (JUCE's own guidance for heavy visualization/analysis — verified via JUCE forum "Main threads and synchronization for heavy visualization").
**Trade-offs:** Requires discipline that `AnalysisResult` is never mutated after publish (treat as `const`); simpler and safer than fine-grained locking, at the cost of "whole result replaced at once" rather than incremental updates (acceptable — analysis is a one-shot batch operation here, not streaming).

**Example:**
```cpp
class AnalysisPipeline : public juce::ThreadPoolJob
{
public:
    AnalysisPipeline (juce::AudioBuffer<float> audio, double sr,
                       std::unique_ptr<ChordAnalyzer> analyzer,
                       std::function<void (std::shared_ptr<const AnalysisResult>)> onDone)
        : ThreadPoolJob ("ChordAnalysis"), buffer (std::move (audio)),
          sampleRate (sr), analyzer (std::move (analyzer)), callback (std::move (onDone)) {}

    JobStatus runJob() override
    {
        if (shouldExit()) return jobHasFinished;
        auto result = std::make_shared<const AnalysisResult> (
            analyzer->analyse (buffer, sampleRate, *this));
        juce::MessageManager::callAsync ([cb = callback, result] { cb (result); });
        return jobHasFinished;
    }
private:
    juce::AudioBuffer<float> buffer;
    double sampleRate;
    std::unique_ptr<ChordAnalyzer> analyzer;
    std::function<void (std::shared_ptr<const AnalysisResult>)> callback;
};
```

### Pattern 3: File-based external drag-out to DAW

**What:** Each MIDI row writes its current content to a temp `.mid` file (via `juce::MidiFile::writeTo`), and on `mouseDrag()` calls `DragAndDropContainer::performExternalDragDropOfFiles({ tempPath }, false)`. This is an OS-level drag, independent of plugin format — works the same from VST3, AU, and Standalone windows since it doesn't depend on a JUCE-internal drop target.
**When to use:** This is the only reliable cross-DAW mechanism found in the JUCE ecosystem for "drag MIDI pattern out of plugin into host piano roll" (verified via multiple JUCE forum threads and a real-world plugin write-up, Stepista).
**Trade-offs:** A few hosts have edge-case bugs with external MIDI drag (e.g., reported flakiness in Studio One — verified via JUCE forum); PROJECT.md's target DAWs (Ableton, FL Studio, Logic) are commonly reported as working. Regenerate the temp file on every content change (voicing edits) before the next drag, since the container just re-reads the file path.

## Data Flow

### Analysis → Generation → Export Flow

```
User drags audio file onto plugin window
    ↓ (message thread: FileDragAndDropTarget callback)
AudioFileLoader (background thread pool job)
    ↓ decode (AudioFormatReader) → resample if needed
AnalysisPipeline::runJob() [ThreadPoolJob, analysis thread]
    ↓ STFT/CQT → Chromagram frames
    ↓ Chord template match + HMM/Viterbi smoothing → raw chord sequence
    ↓ Beat/tempo/key detection → beat-grid alignment of chord sequence
    ↓ build immutable AnalysisResult
AsyncUpdater / MessageManager::callAsync → publish result pointer
    ↓ (message thread)
Editor updates: WaveformView + ChordTimelineView repaint from AnalysisResult
    ↓ user picks style presets (or defaults auto-generate on completion)
MidiGenEngine (message thread, fast/synchronous — no threading needed)
    ↓ ProgressionModel (as-is + variations) × VoicingPresets × VoiceLeadingEngine × BassLineGenerator
    ↓ MidiRowBuilder → juce::MidiFile per row
MidiRowComponent list populated, each row = drag source
    ↓ user drags a row onto DAW timeline/piano roll
performExternalDragDropOfFiles(tempMidiPath) → DAW's own drop handler takes over
```

### State Persistence Flow

```
DAW calls getStateInformation()  [message thread, NOT real-time-safe but not audio thread either]
    ↓
StateManager: apvts.copyState() (thread-safe snapshot)
    + attach session ValueTree branch (source file ref/hash, cached AnalysisResult serialized,
      per-row voicing settings)
    ↓ serialize combined tree to XML → binary blob returned to host

DAW calls setStateInformation() on reload
    ↓
StateManager restores APVTS state + session branch
    ↓ if cached AnalysisResult present → skip re-analysis, repopulate UI directly
    ↓ else (older session / large file not embeddable) → re-run AnalysisPipeline
```

### Key Data Flows

1. **Analysis handoff (analysis thread → message thread):** One-shot, immutable `AnalysisResult` published via atomic shared_ptr + `AsyncUpdater`. Never streamed incrementally in v1 (whole song is batch-analyzed).
2. **Generation (message thread, synchronous):** `AnalysisResult` + selected style → MIDI rows. Fast enough (music-theory rules, no DSP) to run directly on the message thread without a background job, but should still be wrapped in a job if progression variations are computed for many rows at once and get non-trivial (defer decision to implementation phase; keep the option open by not hard-coding it as UI-thread-blocking synchronous calls with no yield points).
3. **Export (message thread → OS → DAW):** Row content → temp `.mid` file on disk → OS drag-and-drop → DAW's own import path. ChordAI has no control past `performExternalDragDropOfFiles()`.
4. **Audio thread involvement:** Minimal in v1. `processBlock()` only needs to run for Standalone/plugin **preview playback** of the loaded file (optional) — it does not run analysis or generation. If preview playback isn't in v1 scope, `processBlock()` can be near-empty (pass-through or silence) — confirm against FEATURES.md/roadmap before assuming playback is required.

## Threading Model Rules

| Thread | Allowed | Forbidden | Notes |
|--------|---------|-----------|-------|
| **Audio thread** (`processBlock`) | Reading atomics/APVTS params, writing pre-allocated buffers, `juce::ScopedNoDenormals` | Heap allocation, `std::mutex`/locks (even `try_lock`), `std::shared_ptr` construction/destruction, file I/O, logging, calling into AnalysisPipeline/MidiGen directly | Verified: JUCE community consensus + timur.audio "Using locks in real-time audio processing, safely" — locks and shared_ptr refcounting introduce unbounded-time operations that cause dropouts |
| **Message thread** (Editor, UI callbacks, `AudioProcessorEditor`) | UI construction/painting, APVTS attachments, triggering ThreadPool jobs, reading published `AnalysisResult` snapshots, building `MidiFile`s, `performExternalDragDropOfFiles` | Long-running/blocking work (full-song analysis, large file decode) — freezes DAW UI if run here | Editor talks to Processor exclusively through APVTS attachments + published result pointers, never reaches into Processor internals directly (verified pattern) |
| **Analysis thread(s)** (`juce::ThreadPool` jobs) | Decode, resample, STFT/CQT, chroma, HMM/Viterbi, beat tracking, allocation, logging | Direct UI manipulation (must marshal back via `MessageManager::callAsync`/`AsyncUpdater`), touching audio-thread buffers | Poll `shouldExit()` periodically for cancellation (e.g., user drops a new file mid-analysis); one job per analysis run, pool sized modestly (2–4 threads is typical for this workload, not CPU-core-count scaling) |

**Core rule for this project:** the audio thread and the analysis thread never touch each other directly. All cross-thread communication goes through (a) APVTS atomic parameters, (b) an immutable `AnalysisResult` published via atomic shared_ptr, or (c) `AsyncUpdater`/`MessageManager::callAsync` for one-shot notifications. No component should invent a fourth communication path.

## Suggested Build Order

Dependencies flow left→right; each stage should be independently testable before the next begins.

1. **Plugin skeleton** — `PluginProcessor`/`PluginEditor` boilerplate, CMake target producing VST3+AU+Standalone, loads in Ableton/FL/Logic with an empty UI. *No dependencies; foundation for everything else.*
2. **File import + decode + WaveformView** — drag-and-drop file onto plugin, `AudioFormatReader` decode (background job, not message thread), `AudioThumbnail` renders waveform, region selection. *Depends on (1). Nothing downstream depends on internals here beyond "we have a decoded buffer."*
3. **ChordAnalyzer interface + ClassicDspChordAnalyzer (chroma→HMM/Viterbi→beat grid)** — build and unit-test the DSP pipeline standalone (console/test harness) before wiring into the plugin UI; then wrap in `AnalysisPipeline`/`ThreadPoolJob` and wire to (2)'s decoded buffer. *Depends on (2) for input data. This is the highest-risk, most research-worthy component — isolate it so it can be iterated without touching UI code.*
4. **AnalysisResult + async publish + ChordTimelineView** — cross-thread handoff pattern, UI renders detected chords over the waveform/beat grid. *Depends on (3)'s output shape (`AnalysisResult`) being stable.*
5. **MidiGenEngine (progression model, voicing presets, voice leading, bass)** — pure logic, unit-testable independent of JUCE audio/UI, consumes `AnalysisResult`. *Depends on (4)'s `AnalysisResult` shape, not on its UI.*
6. **MidiRowComponent list + drag-out export** — UI rows wired to MidiGenEngine output, `performExternalDragDropOfFiles` with temp `.mid` files; test in each target DAW (Ableton, FL, Logic) individually since drag-out is host-dependent. *Depends on (5).*
7. **State/session persistence (APVTS + session ValueTree)** — save/restore source file ref, `AnalysisResult` cache, row configs across DAW project save/reload; skip re-analysis on reload when cache is valid. *Depends on (3)/(4) result shape being serializable — do this after the shape stabilizes, not before, to avoid repeated format migrations.*
8. **`.mid` file export (explicit Save/Export)** — straightforward once (6)'s `MidiFile` construction exists; mostly a UI affordance (file chooser) reusing the same `MidiFile` objects. *Depends on (5)/(6).*
9. **Packaging/format hardening** — verify VST3/AU/Standalone parity, DAW-specific drag quirks, performance profiling of full-song analysis time. *Cross-cutting, do continuously from (3) onward but treat as a final gate before release.*

**Rationale for this order:** the DSP analysis core (step 3) is the highest-uncertainty, most research-dependent piece (chord detection accuracy is a domain problem, not just an engineering one) — building it in isolation with a test harness before wiring into threading/UI lets you validate the algorithm without debugging JUCE plumbing simultaneously. The MIDI generation engine (step 5) has zero JUCE-audio dependency and can be developed/unit-tested in parallel with step 3/4 once `AnalysisResult`'s shape is drafted (it only needs the struct definition, not a working analyzer) — consider stubbing `AnalysisResult` with fixture data to unblock steps 5/6 before step 3 is fully accurate.

## Anti-Patterns

### Anti-Pattern 1: Running analysis on the message thread "just for now"

**What people do:** Call the chroma/HMM pipeline directly inside a button click handler or `timerCallback()` to avoid setting up `ThreadPool` plumbing early.
**Why it's wrong:** Full-song analysis (STFT + HMM/Viterbi over a 3-minute track) takes long enough to freeze the DAW's UI thread if it's shared with the plugin's message thread in some hosts, and it always freezes the plugin window itself — this is precisely the "analysis blocks UI" failure PROJECT.md explicitly calls out as a constraint ("аналіз у фоновому потоці, UI не блокується").
**Instead:** Stand up the `ThreadPoolJob` + `AsyncUpdater` handoff pattern from day one (step 3 of build order), even with a stub analyzer — retrofitting threading after UI code has grown synchronous assumptions is expensive.

### Anti-Pattern 2: Coupling MidiGenEngine to ClassicDspChordAnalyzer internals

**What people do:** Have the voicing/bass generator directly reach into chroma vectors, HMM states, or other DSP-internal types instead of the public `AnalysisResult`.
**Why it's wrong:** Breaks the ML-ready boundary that's a stated Key Decision in PROJECT.md — swapping in an ONNX backend later would require rewriting MidiGen too, not just Analysis.
**Instead:** `MidiGenEngine` depends only on `AnalysisResult` (chord symbols + beat grid + key + tempo), never on analyzer-internal types. Enforce via folder/module boundaries (MidiGen/ never `#include`s anything under Analysis/ except `AnalysisResult.h`).

### Anti-Pattern 3: Sharing mutable analysis state via raw pointer or non-atomic flag

**What people do:** A raw `AnalysisResult*` member on `PluginProcessor`, written by the analysis thread and read by the Editor with a plain `bool analysisReady` flag and no memory barrier.
**Why it's wrong:** Data race / undefined behavior — the Editor may see `analysisReady = true` before the result's field writes are visible (no synchronization), causing intermittent garbage data or crashes that are hard to reproduce.
**Instead:** Publish the whole immutable result at once via `std::atomic<std::shared_ptr<const AnalysisResult>>` (construct-then-publish, never mutate after), or route it exclusively through `MessageManager::callAsync`'s captured-by-value lambda, which already gives correct happens-before semantics.

### Anti-Pattern 4: Re-decoding/re-scanning the audio file on every UI repaint

**What people do:** Call `AudioFormatReader`/waveform generation logic inside `paint()` instead of caching.
**Why it's wrong:** Decoding is comparatively expensive and `paint()` can be called many times per second (resize, hover, etc.) — this causes visible UI stutter.
**Instead:** Use `AudioThumbnail`'s built-in caching (`AudioThumbnailCache`) which decodes once, asynchronously, and only repaints via its own `ChangeBroadcaster` when new thumbnail data arrives (verified: JUCE tutorial).

### Anti-Pattern 5: Treating the `ChordAnalyzer` interface as a single-method black box with no cancellation

**What people do:** Design `ChordAnalyzer::analyse()` as a single blocking call with no way to abort mid-analysis.
**Why it's wrong:** If the user drops a new file while one is still analyzing, or closes the plugin window, an unabortable job either wastes CPU on discarded work or (worse) is still writing to a result buffer the UI has already discarded, risking a dangling reference.
**Instead:** Pass the owning `ThreadPoolJob&` (or an abort token) into `analyse()` so the implementation can poll `shouldExit()` between pipeline stages and return early; the pipeline should be structured as discrete, checkpointable stages for this reason (decode → chroma → HMM → beat-align), not one monolithic loop.

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| DAW host (Ableton/FL Studio/Logic Pro) | JUCE `AudioProcessor`/`AudioProcessorEditor` via VST3/AU wrapper; drag-out via OS-level `performExternalDragDropOfFiles` | No JUCE-side control over how the host imports the dropped `.mid` — verified quirks exist in some hosts (Studio One flakiness reported on JUCE forum), test explicitly in each of the project's three target DAWs |
| None (v1 is fully offline, no cloud/API) | — | Confirmed by PROJECT.md constraint: "Аналіз повністю локальний (офлайн)" |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Audio thread ↔ Message thread | APVTS atomic parameters only | Standard JUCE pattern; APVTS is explicitly the "mandatory thread-safe bridge" (verified) |
| Analysis thread ↔ Message thread | Immutable `AnalysisResult` via atomic shared_ptr + `AsyncUpdater`/`MessageManager::callAsync` | One-shot batch handoff, not streaming, for v1's offline-file use case |
| Analysis/ ↔ MidiGen/ | `AnalysisResult` value object only | Enforces ML-backend-swap boundary (Key Decision in PROJECT.md) |
| UI/ ↔ Analysis/ | Read-only `AnalysisResult` snapshot | UI never triggers re-analysis mid-pipeline except via a well-defined "cancel and restart" path |
| Processor ↔ Editor | APVTS attachments + published result pointer | Editor never calls Processor's DSP methods directly |

## Sources

- [juce::AudioProcessor Class Reference](https://docs.juce.com/master/classAudioProcessor.html) — HIGH confidence, official docs
- [Main threads and synchronization for heavy visualization — JUCE Forum](https://forum.juce.com/t/main-threads-and-synchronization-for-heavy-visualization/31236) — MEDIUM confidence, community-verified pattern (AsyncUpdater handoff)
- [Can one drag and drop MIDI from a JUCE plug-in to the DAW timeline? — JUCE Forum](https://forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816) — MEDIUM confidence, real-world implementation pattern
- [Drag generated audio file to DAW — JUCE Forum](https://forum.juce.com/t/drag-generated-audio-file-to-daw/52250) — MEDIUM confidence
- [Stepista: Building an AI MIDI Pattern Generator as a VST Plugin](https://mrjeffersonlive.com/blog/stepista-ai-midi-pattern-generator) — MEDIUM confidence, real shipped-plugin write-up confirming the same drag-out approach
- [Saving and loading your plug-in state — JUCE Tutorial](https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/) — HIGH confidence, official
- [juce::AudioProcessorValueTreeState Class Reference](https://docs.juce.com/master/classjuce_1_1AudioProcessorValueTreeState.html) — HIGH confidence, official docs
- [HMM-Based Chord Recognition — AudioLabs Erlangen (FMP notebooks)](https://www.audiolabs-erlangen.de/resources/MIR/FMP/C5/C5S3_ChordRec_HMM.html) — HIGH confidence, academic reference implementation of the exact chroma→HMM→Viterbi pipeline
- [Automatic Chord Recognition from Audio Using an HMM with Supervised Learning (K. Lee, ISMIR)](https://ccrma.stanford.edu/~kglee/pubs/klee-ismir06.pdf) — HIGH confidence, peer-reviewed
- [juce::ThreadPoolJob Class Reference](https://docs.juce.com/master/classThreadPoolJob.html) — HIGH confidence, official docs
- [juce::ThreadPool Class Reference](https://docs.juce.com/master/classThreadPool.html) — HIGH confidence, official docs
- [Tutorial: Draw audio waveforms — JUCE](https://juce.com/tutorials/tutorial_audio_thumbnail/) — HIGH confidence, official
- [juce::dsp::FFT Class Reference](https://docs.juce.com/master/classjuce_1_1dsp_1_1FFT.html) — HIGH confidence, official docs
- [The Constant-Q Transform — A Visual Guide](https://brendanjameslynskey.github.io/ConstantQ-Transform/) — MEDIUM confidence, well-established DSP theory reference for CQT→chromagram derivation
- [Using locks in real-time audio processing, safely — timur.audio](https://timur.audio/using-locks-in-real-time-audio-processing-safely) — MEDIUM-HIGH confidence, widely-cited real-time-audio engineering reference
- [JUCE Audio Plugin Development — DeepWiki](https://deepwiki.com/cline/prompts/4.3-juce-audio-plugin-development) — MEDIUM confidence, secondary source, cross-checked against official docs above

---
*Architecture research for: JUCE audio plugin — offline chord detection + MIDI generation*
*Researched: 2026-07-12*
