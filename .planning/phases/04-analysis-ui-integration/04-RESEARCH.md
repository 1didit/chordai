# Phase 4: Analysis UI Integration - Research

**Researched:** 2026-07-13
**Domain:** JUCE cross-thread UI wiring — `juce::ThreadPool`/`ThreadPoolJob` background execution, message-thread progress/result marshalling, and a new chord-timeline display component layered over the existing waveform/region UI
**Confidence:** HIGH (every finding below is verified directly against this repo's own vendored `external/JUCE` 8.0.14 source, the project's own already-complete Phase 2/3 source files, or the frozen `ChordAnalyzer`/`AnalysisResult` contracts — not against training-data assumptions. No WebSearch was needed; this phase is pure JUCE-in-this-repo wiring on top of already-built, already-tested code.)

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-------------------|
| ANL-04 | Analysis runs on a background thread — UI stays responsive, progress is shown; a 3-minute song analyzes in seconds | `juce::ThreadPool`/`ThreadPoolJob` pattern (new `AnalysisPipeline` class, name taken verbatim from `ChordAnalyzer.h`'s own doc comment), `CancelToken` adapter (`job.shouldExit()`), generation-guarded atomic-shared_ptr result/progress publish (extends the already-established `loadedAudio` pattern), existing `ClassicDspChordAnalyzerTests.PerformanceBudget` regression test (Debug, 54s ceiling for a 180s song) plus a new manual Release-build timing checkpoint for the real "seconds not minutes" UX claim |
| ANL-05 | Detected chords are displayed as named chords (Am, Cmaj7, F/A) on a timeline over the waveform | `ChordSegment`/`ChordSymbol`/`ChordQuality` frozen structs (already produced by Phase 3, absolute source-file time), `WaveformMath.h`'s `timeToX`/`xToTime` reused verbatim (already unit-tested), a new dedicated `ChordTimelineView` band, and a shared chord-name-formatting helper (pattern already prototyped in `Tests/ClassicDspChordAnalyzerTests.cpp`'s `RealTrackHarness`) — **see Open Question 1: the requirement text's own examples ("Cmaj7", "F/A") are not achievable with the frozen `ChordQuality` enum (`Major`/`Minor`/`Dominant7`/`NoChord` only, no 7th-extension-beyond-dominant, no inversion/bass field)** |

</phase_requirements>

## Summary

Phase 4 is pure wiring, not new DSP. Every piece of data this phase needs already exists and is frozen: `ChordAnalyzer::analyse()` is synchronous, headless, cancellable, and progress-reporting (`ChordAnalyzer.h`'s own doc comment literally names the class Phase 4 should build — `AnalysisPipeline`, a `juce::ThreadPoolJob` — and gives the exact one-line `CancelToken` adapter: `bool shouldCancel() const override { return job.shouldExit(); }`). The project has already built and proven this exact cross-thread pattern once, for file decode (`AudioFileLoadJob` in `Source/Import/AudioFileLoader.h`, wired into `PluginProcessor::loadAudioFile`): background `ThreadPoolJob` → `juce::MessageManager::callAsync` → atomic-shared_ptr publish → `juce::ChangeBroadcaster` → Editor `ChangeListener`. Phase 4's job is to run that same pattern a second time for analysis, plus handle the one genuinely new wrinkle: analysis must be **cancellable and restartable**, because it needs to re-run automatically on region change (and this same cancel-restart plumbing is exactly what Phase 5's GEN-04 will later reuse for regeneration-on-region-change).

Two verified findings meaningfully simplify the phase beyond what the phase brief assumed. First, `ChordAnalyzer::ProgressCallback` only fires **five times** per full analysis run (`"decoding"` 0.10 → `"beat"` 0.35 → `"chroma"` 0.70 → `"key"` 0.80 → `"chords"` 1.0 — confirmed by reading `ClassicDspChordAnalyzer.cpp`'s five `reportProgress` call sites), not continuously per-frame — so the "rate-limit progress updates" concern doesn't apply; a direct `MessageManager::callAsync` per callback (the project's existing idiom) is correct and sufficient, no `atomic<double>` + polling `Timer` needed. Second, `RegionSelectorOverlay::onRegionChanged` already fires **only on drag-end** (`mouseUp`/`endDrag`), never on intermediate `mouseDrag` events — so "debounce region-change-triggered analysis" is already solved by the existing overlay; no additional debounce logic is needed, only a redundant-no-op guard (see Pitfall 5).

The one requirement-vs-frozen-contract mismatch that must be surfaced to the user/planner: REQUIREMENTS.md's and ROADMAP.md's own text for ANL-05 uses "Cmaj7" and "F/A" as example chord names, but the frozen `ChordQuality` enum (locked in Phase 3, `ChordAnalyzer.h`/`AnalysisResult.h` marked "FROZEN CONTRACT") only has four values — `Major`, `Minor`, `Dominant7`, `NoChord` — with no inversion/bass-note field on `ChordSymbol` at all. Reopening that contract is explicitly out of this phase's scope (it would ripple back into Phase 3's already-complete, human-verified code). This phase should display exactly what the data supports (`C`, `Am`, `G7`, `N.C.`) using the naming convention already prototyped in the test suite, not attempt maj7/slash-chord rendering.

**Primary recommendation:** Build one new `Source/Analysis/AnalysisPipeline.h/.cpp` (`ThreadPoolJob` wrapping `ClassicDspChordAnalyzer::analyse`), a new dedicated `analysisPool` on `PluginProcessor` (separate from `loaderPool`, same size-1 sizing rationale), a generation counter to make cancel-and-restart safe against out-of-order completions, and a new `ChordTimelineView` component occupying its own thin band (not layered on top of `RegionSelectorOverlay`) so chord blocks never fight the existing region-dim overlay for z-order or legibility.

## Standard Stack

### Core

| Library/Module | Version | Purpose | Why Standard |
|-----------------|---------|---------|---------------|
| `juce::ThreadPool` / `juce::ThreadPoolJob` (`juce_core`, already linked) | 8.0.14 | Background analysis execution + cooperative cancellation | Already the project's established pattern for exactly this kind of work (`loaderPool`/`AudioFileLoadJob`); `ChordAnalyzer.h`'s own doc comment names the expected Phase 4 adapter class and its one-line `shouldExit()` implementation |
| `juce::MessageManager::callAsync` (`juce_events`, already linked) | 8.0.14 | Marshal analysis-thread progress/result back to the message thread | Verified identical usage already shipping in `AudioFileLoadJob::runJob()`; `ProgressCallback` fires only ~5 times/run (verified in `ClassicDspChordAnalyzer.cpp`), so no throttling machinery is needed |
| `std::atomic_load`/`std::atomic_store` on `std::shared_ptr<const T>` (`juce_core`/`<memory>`) | — | Cross-thread immutable result handoff | Already established for `loadedAudio` (`PluginProcessor.h` comment: "Access ONLY via std::atomic_load/std::atomic_store — NOT std::atomic\<std::shared_ptr\<T\>\>, which is incomplete on Apple libc++"); reuse verbatim for `analysisResult` |
| `juce::ChangeBroadcaster` (`juce_events`, already linked) | 8.0.14 | Notify the Editor of analysis start / progress / completion | Mirrors the existing `loadBroadcaster` exactly — same `ChangeListener` idiom the Editor already implements |
| `Source/UI/WaveformMath.h` (`timeToX`/`xToTime`, project-local, `juce_core` types only) | — | Pixel↔time conversion for chord-segment block layout | Already unit-tested (`WaveformRegionTests.PixelTimeConversion`) and already reused by `RegionSelectorOverlay` — reuse verbatim for the new timeline, do not reimplement |
| `AnalysisResult`/`ChordSegment`/`ChordSymbol`/`ChordQuality` (`Source/Analysis/AnalysisResult.h`, frozen) | — | The data this phase renders | Already produced end-to-end by `ClassicDspChordAnalyzer::analyse`, absolute source-file time (Phase 3's orchestrator already shifts region-relative times by `regionStartSeconds`) — no transformation needed before feeding pixel math |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::ThreadPool::removeAllJobs(true, 0)` | 8.0.14 | Non-blocking cancel-signal of any in-flight analysis job before submitting a new one | Region change or new file load while a previous analysis is still running — see Pattern 2 |
| `juce::ThreadPoolJob::shouldExit()` | 8.0.14 | Cooperative cancellation check, adapted to `ChordAnalyzer::CancelToken` | One-line adapter, exact signature already documented in `ChordAnalyzer.h` |
| `std::weak_ptr<int>` "alive token" (project-local pattern, `PluginProcessor.h`'s `aliveToken`) | — | Guard against a completion callback running after the processor is destroyed | Reuse the exact existing pattern (`loadAudioFile` already does this) for the analysis completion/progress callbacks |
| `std::atomic<uint64_t>` generation counter (new, project-local) | — | Discard stale/superseded analysis callbacks (see Pitfall 1) | Needed because `AnalysisPipeline` jobs are cancel-and-restarted, unlike the one-shot `AudioFileLoadJob` |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Direct `MessageManager::callAsync` per `ProgressCallback` invocation (chosen) | `std::atomic<double>` progress value + a dedicated polling `Timer` | `ProgressCallback` fires only ~5 times per full analysis run (verified: `"decoding"`/`"beat"`/`"chroma"`/`"key"`/`"chords"`), not per-frame — polling/throttling machinery is unneeded complexity for this call frequency |
| `juce::MessageManager::callAsync` (chosen, matches project idiom) | `juce::AsyncUpdater` | Project already established `callAsync` as its cross-thread-handoff idiom (`AudioFileLoadJob`); introducing `AsyncUpdater` alongside it would be a second, inconsistent pattern solving the same problem |
| A dedicated thin `ChordTimelineView` band, own bounds, no mouse interception (chosen) | Semi-transparent chord blocks painted directly over `WaveformView`'s full bounds, sharing z-order with `RegionSelectorOverlay` | Sharing bounds means chord-block colour/text has to visually coexist with `RegionSelectorOverlay`'s own dim/tint-outside-region overlay (`0x99000000`/`0x22e8c547`) — legibility risk. A separate band with its own bounds sidesteps z-order/mouse-interception questions entirely, since chords are read-only display in Phase 4 (no click-to-seek yet) |
| Progress folded into `ConveyorBeltComponent`'s existing 30Hz `Timer`/`paint()` (chosen) | Stock `juce::ProgressBar` (`juce_gui_basics`, confirmed present, runs its own internal `Timer` polling a `double&`) | The conveyor pixel-art metaphor is a locked user decision (`02-CONTEXT.md`); `ProgressBar`'s look-and-feel-driven bar would visually clash. The belt already repaints at 30Hz — cheaper to extend its existing paint than add a second UI element and a second Timer |
| A dedicated `analysisPool { 1 }` ThreadPool member (chosen) | Reuse the existing `loaderPool` for analysis jobs too | A new-file-drop mid-re-analysis would queue the decode job behind a still-cancelling analysis job on a shared size-1 pool, delaying the new file's load; a separate pool avoids this contention and keeps cancellation scope (`removeAllJobs`) from ever touching an unrelated decode job |

**Installation:** No new dependency. All primitives above are already linked (`juce_core`, `juce_events`, `juce_gui_basics` — all already in `target_link_libraries(ChordAI ...)` per `CMakeLists.txt`). Only new project source files and `target_sources`/`ChordAITests` entries are needed:

```cmake
# CMakeLists.txt additions (both ChordAI and ChordAITests targets need the new .cpp files)
target_sources(ChordAI PRIVATE
    ...
    Source/Analysis/AnalysisPipeline.cpp
    Source/UI/ChordTimelineView.cpp)

target_sources(ChordAITests PRIVATE
    ...
    Tests/AnalysisPipelineTests.cpp
    Source/Analysis/AnalysisPipeline.cpp   # + ${CHORDAI_ANALYSIS_SOURCES} already present
)
```

## Architecture Patterns

### Recommended Project Structure (Phase 4 additions)

```
Source/
├── Analysis/
│   └── AnalysisPipeline.h/.cpp       # NEW — ThreadPoolJob wrapping ChordAnalyzer::analyse; CancelToken adapter;
│                                      #       captures LoadedAudio by shared_ptr + region by value (no live refs
│                                      #       into processor state, mirrors AudioFileLoadJob's own discipline)
├── UI/
│   ├── ChordTimelineView.h/.cpp      # NEW — renders ChordSegment blocks + names in its own dedicated band
│   ├── ChordNameFormatter.h          # NEW — pure fn: (pitchClass, ChordQuality) -> juce::String ("C","Am","G7","N.C.")
│   └── ConveyorBeltComponent.h/.cpp  # MODIFIED — add setAnalysisProgress(fraction, isAnalyzing); belt speed/fill
│                                      #       reacts in the existing 30Hz timerCallback/paint, no new Timer
├── PluginProcessor.h/.cpp            # MODIFIED — analysisPool, analysisGeneration, analysisResult (atomic shared_ptr),
│                                      #       triggerAnalysis(), analysisBroadcaster, getAnalysisResult()/
│                                      #       isAnalyzing()/getAnalysisProgress()
└── PluginEditor.h/.cpp               # MODIFIED — owns a ChordTimelineView, listens to analysisBroadcaster,
│                                      #       feeds progress into conveyor.setAnalysisProgress()
Tests/
├── AnalysisPipelineTests.cpp         # NEW — cancel/restart generation semantics via PluginProcessor's public API
│                                      #       + pumped message loop (same technique as WaveformRegionTests.ThumbnailPopulates)
└── ChordNameFormatterTests.cpp       # NEW — pure formatting fn: all 4 ChordQuality values x all 12 pitch classes
```

### Pattern 1: `AnalysisPipeline` — `ThreadPoolJob` wrapping the frozen `ChordAnalyzer` interface

**What:** A `ThreadPoolJob` subclass that owns a `ClassicDspChordAnalyzer` (stateless — construct one locally, no need to hold it on the processor), a `std::shared_ptr<const LoadedAudio>` snapshot, a region snapshot, and a generation id captured at submit time. `runJob()` calls `analyse()` synchronously (this IS the background thread), then marshals both progress and the final result back via `callAsync`, filtering both by generation.

**When to use:** Every analysis run — initial (post-load) and every re-analysis (region change).

**Example:**
```cpp
// Source: pattern name and CancelToken adapter taken verbatim from Source/Analysis/ChordAnalyzer.h's
// own doc comment ("Phase 4's AnalysisPipeline (juce::ThreadPoolJob) implements a one-line adapter");
// structure mirrors Source/Import/AudioFileLoader.h's AudioFileLoadJob (already shipping, same idiom).
class AnalysisPipeline : public juce::ThreadPoolJob
{
public:
    using ProgressCallback = std::function<void (uint64_t generation, double fraction, const juce::String& stage)>;
    using CompletionCallback = std::function<void (uint64_t generation, std::shared_ptr<const AnalysisResult>)>;

    AnalysisPipeline (std::shared_ptr<const LoadedAudio> audioIn, juce::Range<double> regionIn,
                       uint64_t generationIn, ProgressCallback onProgressIn, CompletionCallback onDoneIn)
        : ThreadPoolJob ("ChordAnalysis"),
          audio (std::move (audioIn)), region (regionIn), generation (generationIn),
          onProgress (std::move (onProgressIn)), onDone (std::move (onDoneIn)) {}

    JobStatus runJob() override
    {
        struct Adapter : ChordAnalyzer::CancelToken
        {
            ThreadPoolJob& job;
            explicit Adapter (ThreadPoolJob& j) : job (j) {}
            bool shouldCancel() const override { return job.shouldExit(); }
        } cancelToken (*this);

        ClassicDspChordAnalyzer analyzer;
        ChordAnalyzer::ProgressCallback progress = [this] (double fraction, const juce::String& stage)
        {
            auto gen = generation; auto cb = onProgress;
            juce::MessageManager::callAsync ([cb, gen, fraction, stage] { if (cb) cb (gen, fraction, stage); });
        };

        auto result = std::make_shared<const AnalysisResult> (
            analyzer.analyse (audio->buffer, audio->sampleRate, region, progress, cancelToken));

        auto gen = generation; auto cb = onDone;
        juce::MessageManager::callAsync ([cb, gen, result] { if (cb) cb (gen, result); });
        return JobStatus::jobHasFinished;
    }

private:
    std::shared_ptr<const LoadedAudio> audio;
    juce::Range<double> region;
    uint64_t generation;
    ProgressCallback onProgress;
    CompletionCallback onDone;
};
```

### Pattern 2: Generation-guarded cancel-and-restart

**What:** `PluginProcessor` keeps `std::atomic<uint64_t> analysisGeneration{0}`. `triggerAnalysis()` (message-thread only, like the rest of the load/region API) does: `analysisPool.removeAllJobs (true, 0)` (non-blocking interrupt signal — pool holds at most one job by design, so this is simpler and safer than tracking a raw `ThreadPoolJob*` and calling `removeJob` on it, which risks a dangling/ABA pointer if the job already self-finished-and-was-deleted), increment the generation, set `isAnalyzing = true` **synchronously** (no flash of "not busy" between supersede and the new job's first callback), snapshot `loadedAudio`/`selectedRegion`, and submit a new `AnalysisPipeline`. The progress and completion callbacks both compare their captured generation against `analysisGeneration.load()`; a mismatch means this job was superseded — discard silently (do not touch `analysisResult`, do not clear `isAnalyzing`).

**When to use:** Any time analysis needs to restart before the previous run finished — this is the exact mechanism Phase 5's GEN-04 ("rows regenerate when region changes") will need to reuse for regeneration, so get it right here.

**Example:**
```cpp
// Source: original code, built from juce::ThreadPool::removeAllJobs's documented semantics
// (external/JUCE/modules/juce_core/threads/juce_ThreadPool.h) + this project's own weak_ptr
// aliveToken guard pattern (PluginProcessor.h, already shipping for loadAudioFile).
void ChordAIAudioProcessor::triggerAnalysis()
{
    auto audio = getLoadedAudio();
    if (audio == nullptr)
        return;

    analysisPool.removeAllJobs (true, 0); // non-blocking cooperative-cancel signal; see Pitfall 3

    const uint64_t generation = ++analysisGeneration;
    isAnalyzing = true;
    analysisBroadcaster.sendChangeMessage(); // Editor picks up "busy" state immediately

    std::weak_ptr<int> weakAlive (aliveToken);

    AnalysisPipeline::ProgressCallback onProgress = [this, weakAlive] (uint64_t gen, double fraction, const juce::String& stage)
    {
        if (weakAlive.expired() || gen != analysisGeneration.load()) return; // stale — discard
        analysisProgress = fraction; juce::ignoreUnused (stage);
        analysisBroadcaster.sendChangeMessage();
    };

    AnalysisPipeline::CompletionCallback onDone = [this, weakAlive] (uint64_t gen, std::shared_ptr<const AnalysisResult> result)
    {
        if (weakAlive.expired() || gen != analysisGeneration.load()) return; // superseded — keep last good result on screen
        std::atomic_store (&analysisResult, result);
        isAnalyzing = false;
        analysisBroadcaster.sendChangeMessage();
    };

    analysisPool.addJob (new AnalysisPipeline (audio, selectedRegion, generation, onProgress, onDone), true);
}
```

### Pattern 3: Message-thread trigger points, with a no-op guard

**What:** `triggerAnalysis()` is called from exactly two places, both already message-thread-only per the existing API's own convention: (1) the tail of `loadAudioFile`'s completion callback, on success — but first **clear** `analysisResult` (publish `nullptr`) so the new song never briefly shows the old song's chords; (2) the tail of `setSelectedRegion()` — but **only if the clamped region actually changed**, guarded by comparing against the previous `selectedRegion`. This guard matters because `RegionSelectorOverlay::setTotalLength()` (called from `handleLoadComplete` on every editor open/reopen) always fires `onRegionChanged` with the whole-file default even when nothing changed, which would otherwise re-trigger analysis on every editor reopen (see Pitfall 5).

**When to use:** Both call sites; do not add a third trigger point — keep all "when does analysis run" logic inside `PluginProcessor`, never in the Editor (matches the project's own established boundary: "Processor ↔ Editor: APVTS attachments + published result pointer... Editor never calls Processor's DSP methods directly").

**Example:**
```cpp
// setSelectedRegion, extended (existing body per PluginProcessor.cpp unchanged above the new guard)
void ChordAIAudioProcessor::setSelectedRegion (juce::Range<double> regionSeconds)
{
    auto audio = getLoadedAudio();
    if (audio == nullptr) return;

    auto clamped = RegionState::clampRegion (regionSeconds.getStart(), regionSeconds.getEnd(), audio->lengthSeconds);
    if (clamped == selectedRegion) return; // no-op guard — see Pitfall 5

    selectedRegion = clamped;
    RegionState::write (apvts.state, audio->sourceFile, selectedRegion);
    triggerAnalysis();
}
```

### Pattern 4: Dedicated `ChordTimelineView` band with label-collision guarding

**What:** Give the chord timeline its own thin band (e.g. reserve ~24-32px from the top of the current `waveformArea`, shrinking the waveform's own area slightly) rather than overlaying it on the waveform. Render each `ChordSegment` as `timeToX(seg.startSeconds, visibleRange, width)` → `timeToX(seg.endSeconds, ...)`, `visibleRange = {0, totalLength}` (full file — `AnalysisResult` already carries absolute time, matching `RegionSelectorOverlay`'s own `visibleRange` convention exactly). Since `AnalysisResult.chords` only ever spans the analyzed region, chord blocks will naturally only appear inside the currently-selected region — which visually reinforces (never conflicts with) `RegionSelectorOverlay`'s own dim-outside-region treatment, because the two components occupy different bands. Skip drawing the text label (draw only the coloured block + boundary lines) when the segment's pixel width is below a minimum label width, to avoid garbled overlapping text on short/fast chord changes.

**When to use:** ANL-05's timeline display.

**Example:**
```cpp
// Original code — pixel math reuses Source/UI/WaveformMath.h verbatim (already unit-tested).
bool shouldDrawLabel (float segmentWidthPx, float minLabelWidthPx = 24.0f)
{
    return segmentWidthPx >= minLabelWidthPx;
}

void ChordTimelineView::paint (juce::Graphics& g)
{
    auto result = latestResult; // std::shared_ptr<const AnalysisResult>, set via processor.getAnalysisResult()
    if (result == nullptr || totalLength <= 0.0) return;

    juce::Range<double> visibleRange (0.0, totalLength);
    for (const auto& seg : result->chords)
    {
        auto x0 = timeToX (seg.startSeconds, visibleRange, getWidth());
        auto x1 = timeToX (seg.endSeconds, visibleRange, getWidth());
        auto blockBounds = juce::Rectangle<float> (x0, 0.0f, x1 - x0, (float) getHeight());

        g.setColour (colourForQuality (seg.chord.quality));
        g.fillRect (blockBounds);

        if (shouldDrawLabel (blockBounds.getWidth()))
            g.drawText (chordName (seg.chord), blockBounds.reduced (1.0f), juce::Justification::centred, false);
    }
}
```

### Pattern 5: Chord-name formatting — one shared pure function

**What:** `Tests/ClassicDspChordAnalyzerTests.cpp`'s `RealTrackHarness` test already prototyped the exact mapping needed: `kNoteNames[12]` + a `ChordQuality` switch (`Major` → `""`, `Minor` → `"m"`, `Dominant7` → `"7"`, `NoChord` → `"N.C."`). Extract this into `Source/UI/ChordNameFormatter.h` as a pure free function so both the UI and (optionally, as cleanup) the test harness consume the same implementation.

**Example:**
```cpp
// Source: naming convention lifted verbatim from Tests/ClassicDspChordAnalyzerTests.cpp's
// RealTrackHarness chordName lambda (already shipping, already exercised on a real 75s track).
inline juce::String chordName (const ChordSymbol& chord)
{
    static constexpr const char* kNoteNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    if (chord.quality == ChordQuality::NoChord) return "N.C.";

    const juce::String suffix = chord.quality == ChordQuality::Major     ? ""
                               : chord.quality == ChordQuality::Minor    ? "m"
                                                                          : "7"; // Dominant7
    return juce::String (kNoteNames[chord.pitchClass]) + suffix;
}
```

### Anti-Patterns to Avoid

- **Publishing an analysis result without a generation check:** A superseded (region-changed-away-from) job can legitimately finish *after* the newer job, because cancellation is cooperative, not instant (Pitfall 1/3). Publishing unconditionally would flicker the UI back to a stale or empty result.
- **Tracking a raw `ThreadPoolJob*` for cancellation:** Use `analysisPool.removeAllJobs(true, 0)` instead of `removeJob(ptr, ...)` — the pool only ever holds one analysis job by design, and `removeAllJobs` avoids ever holding a pointer that might reference already-freed memory.
- **Re-triggering analysis on a no-op region "change":** `RegionSelectorOverlay::setTotalLength()` fires `onRegionChanged` unconditionally on every editor (re)open, even with an unchanged region — guard `setSelectedRegion` with an equality check before calling `triggerAnalysis()` (Pattern 3).
- **Building an `atomic<double>` + polling `Timer` for progress:** Unneeded — `ProgressCallback` fires only ~5 times per run; direct `callAsync` per call is simpler and matches the project's existing idiom.
- **Attempting `Cmaj7`/`F/A`-style display:** The frozen `ChordQuality` enum and `ChordSymbol` struct cannot represent these — see Open Question 1.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|--------------|-----|
| Cross-thread cancel-and-restart bookkeeping | A custom "job version" atomic + manual thread interrupt/join logic | `juce::ThreadPool::removeAllJobs(true, 0)` + `ThreadPoolJob::shouldExit()` (already used by `loaderPool`/`AudioFileLoadJob`) | JUCE's `ThreadPool` already handles interrupt-signal + safe internal-list locking (`CriticalSection lock` guards the job array — verified in `juce_ThreadPool.h`); reinventing it risks exactly the dangling-pointer/ABA class of bug a hand-rolled version would hit |
| Progress UI polling/throttling machinery | A rate-limited `Timer` sampling an `atomic<double>` | Direct `MessageManager::callAsync` per `ProgressCallback` invocation | Verified call frequency (~5 times per run, at pipeline stage boundaries) makes throttling unneeded complexity |
| Chord name formatting | Ad hoc string building duplicated in UI code and in tests | One shared pure function (`ChordNameFormatter.h`), consumed by both `ChordTimelineView` and (optionally) the existing `RealTrackHarness` test | `Tests/ClassicDspChordAnalyzerTests.cpp` already has this logic inline — duplicating it in UI code would let the two drift out of sync |
| Pixel↔time conversion for the new timeline | A second `timeToX`/`xToTime` implementation | Reuse `Source/UI/WaveformMath.h` verbatim (already used by `RegionSelectorOverlay`, already unit-tested in `WaveformRegionTests.PixelTimeConversion`) | Exact same math; a second implementation is pure duplication risk |
| Busy/progress notification to the Editor | A new bespoke callback/observer type | `juce::ChangeBroadcaster` + `ChangeListener`, mirroring the existing `loadBroadcaster` | Zero new plumbing concepts for the Editor to learn; same idiom it already implements for load state |

**Key insight:** Phase 4 has no genuinely novel DSP or JUCE-primitive work — every mechanism (background job, cancellation, cross-thread publish, change notification, pixel math) is either a JUCE built-in already linked, or a pattern this exact project already built and shipped once in Phase 2/3. The only new design decision is the cancel-and-restart generation guard (Pattern 2), because it's the first time this project needs a job that can be superseded before it finishes.

## Common Pitfalls

### Pitfall 1: Stale/superseded analysis callback overwriting a fresher result
**What goes wrong:** A region-changed-away-from analysis job (cancellation requested, but cooperative — see Pitfall 3) finishes *after* the newer job that superseded it, and its completion callback overwrites `analysisResult` with a stale or empty (`wasCancelled == true`, `chords.empty()`) result, flickering the UI backward.
**Why it happens:** `analyse()`'s cancellation is checked only at stage boundaries (plus per-CQT-chunk in the chroma stage); a job that was mid-way through a fast stage when cancelled can still finish and callback shortly after a job submitted later.
**How to avoid:** Generation-guard both the progress and completion callbacks (Pattern 2) — compare the job's captured generation against the processor's current `analysisGeneration` before touching any shared state.
**Warning signs:** Chord timeline briefly showing an empty/wrong state right after a region drag, then correcting itself a moment later.

### Pitfall 2: Requirement text describes chord names the frozen data model cannot produce
**What goes wrong:** Attempting to render `Cmaj7` or `F/A` (slash/inversion notation) per REQUIREMENTS.md's/ROADMAP.md's own illustrative examples for ANL-05.
**Why it happens:** Those examples predate Phase 3's contract freeze — `.planning/research/FEATURES.md` (project-level, pre-Phase-3) already listed "Cmaj7, Am, F/A" as an aspirational baseline-literacy feature, but Phase 3's `ChordQuality` enum (`Major`/`Minor`/`Dominant7`/`NoChord`) and `ChordSymbol` struct (`pitchClass` + `quality` only, no bass/inversion field) were frozen without ever adding major-7th or inversion support.
**How to avoid:** Display exactly what the frozen data supports: `""`/`"m"`/`"7"`/`"N.C."` suffixes (Pattern 5's `chordName`). Do not add fields to `ChordSymbol`/`AnalysisResult` in this phase — both headers are explicitly marked `// FROZEN CONTRACT — do not change shape without updating Phase 4 plans`, meaning Phase 4 is expected to consume them as-is, not extend them.
**Warning signs:** Any task that proposes adding an `isMajor7`/`bassPitchClass` field to `ChordSymbol`, or a chord-quality-upgrade pass in the analyzer — flag for explicit user confirmation before proceeding (see Open Question 1).

### Pitfall 3: Treating cancellation as instant
**What goes wrong:** Assuming `analysisPool.removeAllJobs(true, 0)` immediately frees the pool's one worker thread, and the newly-submitted job starts running right away.
**Why it happens:** Cancellation is cooperative — the old job's `runJob()` (already executing `analyse()`) only notices `shouldExit()` at its next checkpoint (stage boundary or CQT chunk boundary per `ClassicDspChordAnalyzer.cpp`'s explicit per-stage checks). On a size-1 `ThreadPool`, the new job is queued and cannot start until the old job's `runJob()` actually returns.
**How to avoid:** Design the UI/UX around this: `isAnalyzing`/progress state should already read "busy" the instant `triggerAnalysis()` runs (set synchronously in Pattern 2, not waiting for the new job's first callback), so there's no visible gap even though the new job hasn't literally started computing yet.
**Warning signs:** None functionally — flagged so a plan doesn't try to build a "wait for old job to actually stop" synchronous check on the message thread (which would block it, defeating the entire point of this phase).

### Pitfall 4: Debug-build performance numbers misrepresent the "seconds not minutes" UX target
**What goes wrong:** Treating the existing `ClassicDspChordAnalyzerTests.PerformanceBudget` test (Debug build, ~35.4s measured / 54s ceiling for a 180s synthetic song — see that test file's own extensive comment) as proof that ANL-04's "3-minute song completes analysis in seconds" criterion is met.
**Why it happens:** The project currently only configures a Debug build (`build/` was configured with `-DCMAKE_BUILD_TYPE=Debug`, confirmed via `CMakeCache.txt` presence and Phase 1's own configure command; no Release build has been exercised anywhere in this repo yet). The existing test's own comment states explicitly: "A Release build (-O2/-O3, what actually ships) comfortably meets the original ~5-10s research budget for this same clip."
**How to avoid:** Keep the existing Debug-build `PerformanceBudget` test as the CI regression gate (no change needed — it already exists and passes). Additionally, as a manual Phase 4 checkpoint, configure and build a **Release** build (`cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-release`) and re-run the `ClassicDspChordAnalyzerTests.RealTrackHarness` (or a similar timed run) once against a real ~3-minute track to confirm the felt "seconds not minutes" experience — this is a one-time human-in-the-loop validation, not a new automated test, since the project has no Release CI target today.
**Warning signs:** Treating 35s (Debug) as "the number" for success-criterion 3 without ever measuring a Release build.

### Pitfall 5: Redundant `onRegionChanged` calls re-triggering analysis on every editor reopen
**What goes wrong:** `RegionSelectorOverlay::setTotalLength()` (called from `PluginEditor::handleLoadComplete`, which runs both on fresh load AND on every editor reopen when a file is already loaded — see the ctor's "Editor-reopen case" comment in `PluginEditor.cpp`) always fires `onRegionChanged` with the whole-file default, even when the region hasn't actually changed. Without a guard, every editor close/reopen cycle would silently kick off a full re-analysis.
**Why it happens:** The overlay's contract is "fired on every selection change, **including the initial whole-file default**" (its own header comment) — this is correct behavior for the overlay itself, but naive downstream wiring (`setSelectedRegion` unconditionally calling `triggerAnalysis()`) would treat every such call as a real change.
**How to avoid:** Guard `setSelectedRegion()` with an equality check against the current `selectedRegion` before calling `triggerAnalysis()` (Pattern 3) — cheap, correct, and also naturally covers the redundant call that happens right after a fresh load (since `loadAudioFile`'s own callback already sets `selectedRegion` to the same whole-file default before the Editor's `handleLoadComplete` runs).
**Warning signs:** Analysis visibly re-running (progress indicator flashing) every time the plugin editor window is closed and reopened with no user interaction in between.

### Pitfall 6: Chord blocks sharing bounds/z-order with `RegionSelectorOverlay`
**What goes wrong:** Painting chord-name blocks directly on top of the waveform (same bounds as `WaveformView`/`RegionSelectorOverlay`) fights visually with the region overlay's own dim (`0x99000000`) and tint (`0x22e8c547`) treatment, and raises z-order/mouse-interception questions the phase doesn't otherwise need to answer (chords are read-only display in Phase 4).
**Why it happens:** The path of least resistance is "just paint on the existing waveform component."
**How to avoid:** Give the timeline its own dedicated band (Pattern 4) — a design choice, not a JUCE constraint, but one that avoids an entire class of legibility/layering bugs for free.
**Warning signs:** Chord labels becoming unreadable inside a dimmed (outside-region) area, or a region drag visually "erasing" chord blocks underneath it.

## Code Examples

Verified patterns, either lifted directly from this repo's own already-shipping code or built from JUCE APIs read directly from the vendored `external/JUCE` 8.0.14 source:

### `ThreadPoolJob`'s cancellation contract (verified in-repo)
```cpp
// Source: external/JUCE/modules/juce_core/threads/juce_ThreadPool.h
bool shouldExit() const noexcept { return shouldStop; }
void signalJobShouldExit();
```

### `ThreadPool::removeAllJobs` non-blocking cancel signal (verified in-repo)
```cpp
// Source: external/JUCE/modules/juce_core/threads/juce_ThreadPool.h — doc comment:
// "if true, then all running jobs will have their ThreadPoolJob::signalJobShouldExit()
//  methods called to try to interrupt them ... returns true if all jobs are successfully
//  stopped and removed; false if the timeout period expires"
analysisPool.removeAllJobs (/*interruptRunningJobs*/ true, /*timeOutMilliseconds*/ 0);
```

### Existing atomic-shared_ptr publish pattern (already shipping, reuse verbatim for `analysisResult`)
```cpp
// Source: Source/PluginProcessor.h/.cpp (already shipping, loadedAudio)
std::atomic_store (&loadedAudio, result);
// ...
std::shared_ptr<const LoadedAudio> ChordAIAudioProcessor::getLoadedAudio() const
{
    return std::atomic_load (&loadedAudio);
}
```

### Pixel↔time conversion (already shipping, unit-tested — reuse verbatim)
```cpp
// Source: Source/UI/WaveformMath.h (already shipping, already unit-tested in WaveformRegionTests)
inline float timeToX (double time, juce::Range<double> visibleRange, int width);
inline double xToTime (float x, juce::Range<double> visibleRange, int width);
```

## State of the Art

Not applicable in the usual sense — this is new code inside an actively-developed, single-codebase project (Phase 3 completed the day before this research). There are no deprecated JUCE APIs affecting this phase's scope in the vendored 8.0.14: `ThreadPool`/`ThreadPoolJob`, `MessageManager::callAsync`, `ChangeBroadcaster`, and `ProgressBar` are all stable, unchanged APIs per the project's own prior research (`02-RESEARCH.md`'s "State of the Art" finding already covers this for the same JUCE version).

| Old Approach (this project) | Current Approach (Phase 4) | When Changed | Impact |
|--------------|-------------------|---------------|--------|
| One-shot background job (`AudioFileLoadJob`) with no cancel-and-restart need | Cancel-and-restart background job (`AnalysisPipeline`) with a generation guard | This phase — first time the project needs a supersedable job | Establishes the exact pattern Phase 5's GEN-04 (regenerate rows on region/style change) will reuse |

## Open Questions

1. **ANL-05's own example chord names ("Cmaj7", "F/A") are not representable by the frozen `ChordQuality`/`ChordSymbol` contract**
   - What we know: `ChordQuality` has exactly 4 values (`Major`, `Minor`, `Dominant7`, `NoChord`); `ChordSymbol` has no bass/inversion field. Both `ChordAnalyzer.h` and `AnalysisResult.h` are explicitly marked frozen, and this is a Phase 3 decision, not something Phase 4 is scoped to change.
   - What's unclear: Whether the user still wants the REQUIREMENTS.md/ROADMAP.md wording updated to match reality, or whether a future phase (v2, or a Phase 3 contract revision) should add major-7th/inversion support.
   - Recommendation: Ship Phase 4 displaying exactly `{"", "m", "7", "N.C."}` suffixes (Pattern 5). Treat the REQUIREMENTS.md/ROADMAP.md wording as illustrative/aspirational, not literal — flag this explicitly during `/gsd:plan-phase` or the phase's own checkpoint so the user can confirm before implementation, rather than silently under- or over-building.

2. **Exact belt-progress visual treatment**
   - What we know: The conveyor is a locked pixel-art metaphor (`02-CONTEXT.md`); no Phase 4 CONTEXT.md exists yet to lock specifics, so this remains Claude's discretion per the phase brief.
   - What's unclear: Whether "belt speeds up" alone, a thin fill-bar along one edge (matching `RegionSelectorOverlay`'s existing 1px gold edge-highlight styling, `0xffe8c547`), or both together, best fits the pixel-art aesthetic.
   - Recommendation: Default to a thin fill-bar along the belt's top edge (cheap, unambiguous progress signal, consistent with existing edge-highlight visual language) plus a modest belt-speed increase while `isAnalyzing` is true (reuses the existing `beltOffset` increment, no new rendering primitive). Confirm visually at the phase's manual checkpoint.

3. **`triggerChunkFallStub()` timing — load-complete only, analysis-complete only, or both**
   - What we know: Currently fires once on load-complete (Phase 2 stub, explicitly documented as "at most a visual stub"). The phase brief suggests moving/adding it to analysis-complete, since that's the more meaningful "something was computed" moment.
   - What's unclear: Whether keeping the load-complete trigger too feels redundant or is still useful as instant "something is on the belt" feedback.
   - Recommendation: Keep the existing load-complete trigger (cheap, already shipping, gives instant feedback) and add a second trigger on analysis-complete (more meaningful — real computed data exists now, even though it isn't MIDI yet). Confirm with the user at plan-check if this reads as visual clutter.

4. **No Release build configuration exists anywhere in this repo yet**
   - What we know: `build/` was configured Debug-only (`CMakeCache.txt` confirms); Phase 1's own documented configure command is `-DCMAKE_BUILD_TYPE=Debug`. No script or CI step builds Release.
   - What's unclear: Whether formalizing a Release CTest target/script belongs in this phase (to make Pitfall 4's manual check repeatable) or is better deferred to Phase 7 (release hardening).
   - Recommendation: Treat the Release-build timing check as a one-time manual step for this phase's checkpoint (ad hoc `cmake -B build-release ... -DCMAKE_BUILD_TYPE=Release`), not a new permanent CI target — defer formalizing Release CI to Phase 7 unless the user wants it sooner.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 via CTest (already wired, `ChordAITests` target) |
| Config file | `CMakeLists.txt` (root) — `catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.")` |
| Quick run command | `ctest --test-dir build -R "ChordAITests.(AnalysisPipeline\|ChordNameFormatter)" --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ANL-04 | `analysisPool.removeAllJobs` + generation guard correctly discards a superseded job's result, publishes only the latest | unit/integration | `ctest -R ChordAITests.AnalysisPipelineTests.CancelAndRestart` (drive via `PluginProcessor`'s public API: `loadAudioFile` a fixture, call `setSelectedRegion` twice in quick succession, pump the message loop per `WaveformRegionTests.ThumbnailPopulates`'s existing technique, assert `getAnalysisResult()` reflects only the last region) | ❌ Wave 0 |
| ANL-04 | Redundant no-op `setSelectedRegion` calls do not re-trigger analysis | unit | `ctest -R ChordAITests.AnalysisPipelineTests.NoOpRegionDoesNotRetrigger` | ❌ Wave 0 |
| ANL-04 | Progress callback fires with monotonically non-decreasing fraction, reaches 1.0 on success (already proven for the synchronous `analyse()` call itself) | unit | `ctest -R ChordAITests.ClassicDspChordAnalyzerTests.ProgressMonotonic` | ✅ (existing, no change needed) |
| ANL-04 | A 3-minute (180s) synthetic song stays within the Debug performance budget (regression gate for "not degrading" — not the literal UX claim) | unit | `ctest -R ChordAITests.ClassicDspChordAnalyzerTests.PerformanceBudget` | ✅ (existing, no change needed) |
| ANL-04 | A real ~3-minute track analyzes in single-digit-to-low-double-digit seconds on a **Release** build (the actual UX claim) | manual-only | Manual: configure+build Release, run `ClassicDspChordAnalyzerTests.RealTrackHarness` (or time the Standalone UI directly) against a real track; justification: no Release CI target exists in this project yet (see Open Question 4), and felt UI responsiveness isn't meaningfully unit-testable | n/a |
| ANL-04 | UI remains responsive (no freeze) while a 3-minute song analyzes | manual-only | Manual: drop a 3+ minute file in Standalone, drag the region selector and resize the window while analysis runs, confirm no stall; justification: message-thread responsiveness under real OS scheduling isn't meaningfully unit-testable (same justification pattern as Phase 2's equivalent manual check) | n/a |
| ANL-04 | A progress/busy indicator is visibly present during analysis | manual-only | Manual: visual check in Standalone; justification: no visual-regression tooling exists in this project | n/a |
| ANL-05 | Chord-name formatting is correct for all 4 `ChordQuality` values × representative pitch classes | unit | `ctest -R ChordAITests.ChordNameFormatterTests` | ❌ Wave 0 |
| ANL-05 | Chord-segment pixel layout (`timeToX`/`xToTime` reuse) is mathematically correct | unit | `ctest -R ChordAITests.WaveformRegionTests.PixelTimeConversion` | ✅ (existing, no change needed — same helpers reused) |
| ANL-05 | Label-collision guard (`shouldDrawLabel`) correctly suppresses text on narrow segments | unit | `ctest -R ChordAITests.ChordTimelineLayoutTests.LabelCollision` | ❌ Wave 0 |
| ANL-05 | Chord timeline renders legibly, aligned to the waveform, over a real analyzed track | manual-only | Manual: visual check in Standalone against a real track; justification: pixel-art legibility is a visual judgment, no visual-regression tooling exists | n/a |
| (supporting) | pluginval strict mode still green after adding the new job/UI wiring | automated (existing tooling) | `tools/pluginval.app/Contents/MacOS/pluginval --strictness-level 5 --validate-in-process --validate "$HOME/Library/Audio/Plug-Ins/VST3/ChordAI.vst3"` (and `.component` for AU) | ✅ (tool exists, rerun as regression gate) |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -R "ChordAITests.(AnalysisPipeline|ChordNameFormatter|ChordTimelineLayout)" --output-on-failure`
- **Per wave merge:** Full `ctest --test-dir build --output-on-failure` + a `pluginval` strict-mode pass (VST3+AU), matching Phase 2/3's own established gate
- **Phase gate:** Full suite green + manual checkpoint covering: Standalone responsiveness during a real 3-minute analysis, progress indicator visibility, chord-timeline legibility over a real track, and the one-time Release-build timing measurement — before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `Tests/AnalysisPipelineTests.cpp` — covers ANL-04's cancel/restart + no-op-guard behavior via `PluginProcessor`'s public API
- [ ] `Tests/ChordNameFormatterTests.cpp` — covers ANL-05's chord-name-string mapping
- [ ] `Tests/ChordTimelineLayoutTests.cpp` (or folded into an existing UI test file) — covers ANL-05's label-collision pure-function logic
- [ ] `CMakeLists.txt` — add `Source/Analysis/AnalysisPipeline.cpp` and `Source/UI/ChordTimelineView.cpp` to both `ChordAI` and `ChordAITests` targets' `target_sources`
- [ ] One-time manual Release build configure (`cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release`) — no script exists yet for this; ad hoc for this phase's checkpoint per Open Question 4

*(No framework-level gaps — Catch2/CTest and `tools/pluginval.app` are both already wired and require no new setup.)*

## Sources

### Primary (HIGH confidence — verified directly against this repo's own source, vendored JUCE, or already-shipping tests)
- `Source/Analysis/ChordAnalyzer.h` — frozen `CancelToken`/`ProgressCallback` contract, including the doc comment that names the expected Phase 4 `AnalysisPipeline` class and its one-line `shouldExit()` adapter
- `Source/Analysis/AnalysisResult.h` — frozen `ChordQuality`/`ChordSymbol`/`ChordSegment`/`AnalysisResult` shapes
- `Source/Analysis/ClassicDspChordAnalyzer.cpp` — verified exact `reportProgress` call sites/fractions/stage labels (5 calls per run), absolute-time-shift behavior, per-stage + per-CQT-chunk cancellation checkpoints
- `Tests/ClassicDspChordAnalyzerTests.cpp` — `PerformanceBudget` test's measured Debug (~35.4s/180s) vs documented Release (~5-10s) performance story; `RealTrackHarness`'s existing chord-name-formatting prototype; `ProgressMonotonic`/`Cancellation` test patterns
- `Source/PluginProcessor.h`/`.cpp` — existing `loadAudioFile`/`getLoadedAudio`/`setSelectedRegion`/`loadBroadcaster` API and its atomic-shared_ptr + weak_ptr aliveToken + `MessageManager::callAsync` pattern, reused verbatim for analysis
- `Source/Import/AudioFileLoader.h` — `AudioFileLoadJob`, the exact `ThreadPoolJob` pattern this phase's `AnalysisPipeline` mirrors
- `Source/PluginEditor.cpp`/`.h` — existing z-order (`conveyor`, `waveformView`, `regionOverlay`, `midiSetsPlaceholder`), `handleLoadComplete`, editor-reopen restore path (source of Pitfall 5)
- `Source/UI/RegionSelectorOverlay.h`/`.cpp`, `Source/UI/RegionSelectionModel.h` — confirmed `onRegionChanged` fires only on drag-end (`mouseUp`/`endDrag`) and on `setTotalLength`, never on intermediate `mouseDrag`
- `Source/UI/WaveformMath.h`, `Tests/WaveformRegionTests.cpp` — pixel↔time conversion helpers and their existing unit-test coverage
- `Source/UI/ConveyorBeltComponent.h`/`.cpp` — existing 30Hz `Timer`, `triggerChunkFallStub()`, procedural pixel-art paint structure
- `external/JUCE/modules/juce_core/threads/juce_ThreadPool.h` — `ThreadPoolJob::shouldExit()`/`signalJobShouldExit()`, `ThreadPool::removeJob`/`removeAllJobs`/`addJob` documented semantics, internal `CriticalSection lock` guarding the job array
- `external/JUCE/modules/juce_gui_basics/widgets/juce_ProgressBar.h` — confirmed stock `ProgressBar` exists (own internal `Timer` polling a `double&`), considered and explicitly not chosen (pixel-art aesthetic conflict)
- `.planning/research/ARCHITECTURE.md` — project-level threading model (Pattern 2: background analysis job with immutable-snapshot handoff), Anti-Pattern 5 (cancellation must exist from the start)
- `.planning/phases/02-audio-import-waveform/02-RESEARCH.md` — precedent for this project's own Validation Architecture format and Wave 0 gap style
- `CMakeLists.txt` — confirmed no new external dependency needed; current `target_sources`/`target_link_libraries` structure for both `ChordAI` and `ChordAITests` targets
- `build/CMakeCache.txt` (presence, contents not altered) — confirms only a Debug configuration currently exists in this repo (source of Open Question 4/Pitfall 4)
- `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md` — exact ANL-04/ANL-05 wording, including the "Cmaj7"/"F/A" examples that motivated Pitfall 2/Open Question 1
- `.planning/research/FEATURES.md` — confirms the "Cmaj7, Am, F/A" chord-display idea predates Phase 3's contract freeze (project-level competitive research, not Phase-3-aware)

### Secondary (MEDIUM confidence)
- None — every finding in this document was verifiable directly against in-repo source (this project's own code, its own tests, or its own vendored JUCE copy).

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — every primitive is already linked and already used identically elsewhere in this exact codebase; no new dependency
- Architecture: HIGH — cancel-and-restart generation-guard pattern is original design work for this phase (first supersedable job in the project), but built entirely from documented, in-repo-verified JUCE `ThreadPool` semantics plus the project's own already-proven atomic-publish idiom
- Pitfalls: HIGH for the JUCE-mechanics and in-repo-behavior pitfalls (generation races, cooperative-cancel latency, redundant `onRegionChanged` firing, Debug-vs-Release performance — all directly verified by reading the actual source); the "Cmaj7/F-A mismatch" pitfall is HIGH confidence as a factual finding (verified against the frozen headers) though the *resolution* is a product decision flagged for user confirmation (Open Question 1)

**Research date:** 2026-07-13
**Valid until:** ~30 days (stable JUCE APIs, no external dependency; revisit sooner only if the frozen `ChordAnalyzer`/`AnalysisResult` contracts are revised, or if the JUCE submodule is bumped past 8.0.14)
