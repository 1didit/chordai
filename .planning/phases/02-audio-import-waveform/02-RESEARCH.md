# Phase 2: Audio Import & Waveform - Research

**Researched:** 2026-07-12
**Domain:** JUCE audio file import (drag-and-drop + decode), AudioThumbnail waveform rendering, custom region-selection UI, and a from-scratch pixel-art Timer-driven animation layer
**Confidence:** HIGH (JUCE APIs verified directly against the vendored `external/JUCE` 8.0.14 source in this repo — module manifests, class headers, and a real shipped JUCE example; MEDIUM on macOS host-sandboxing specifics, verified via JUCE forum + official CMake docs rather than a live sandboxed-host test)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Conveyor UI metaphor (USER DECISION — locked)**
- The plugin window features a **pixel-art animated conveyor belt** running **left → right** across the window
- Dropping a song/segment onto the window feeds it onto the conveyor (the drop target IS the conveyor area)
- At the right end of the conveyor, **piano-roll note chunks "fall off"** — this is the visual for generated MIDI output (full behavior lands in Phases 4-6 when analysis/generation exist; Phase 2 establishes the conveyor visual + animation framework and the drop interaction)
- **Bottom of the window: a list of the generated chord/melody MIDI sets** (the conveyor rows) for piano roll — Phase 2 reserves this layout region (placeholder/empty state); Phase 5 populates it, Phase 6 makes rows auditionable/draggable

**Phase 2 scope of the metaphor**
- Conveyor belt animation visible and running (idle loop) in the editor
- File drop onto the conveyor area loads the file; the loaded waveform is displayed (waveform can sit on/above the belt — layout at Claude's discretion)
- Region selection on the waveform per REQUIREMENTS (IMP-03)
- Falling piano-roll chunks: at most a visual stub/placeholder animation trigger — real chunks appear only when generation exists (deferred)

### Claude's Discretion
- Pixel-art style specifics (palette, tile size, frame count/rate), implemented with JUCE Graphics/Timer — no heavy dependencies
- Exact layout proportions (conveyor strip height, waveform area, bottom list height)
- Animation performance approach (Timer-driven repaint of the belt region only; must not violate RT-safety — animation lives entirely on the message thread)
- Empty-state design for the bottom sets list

### Deferred Ideas (OUT OF SCOPE)
- Note chunks falling out tied to real generated MIDI — Phase 5 (generation) / Phase 4 (analysis completion events)
- Bottom list populated with real MIDI sets — Phase 5
- Audition + drag-out of list rows — Phase 6
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-------------------|
| IMP-01 | User can drag-and-drop an audio file (WAV/MP3/AIFF/FLAC) onto the plugin/standalone window and it loads (MP3/AAC decoded via macOS CoreAudio) | `FileDragAndDropTarget` interface (verified in-source), `AudioFormatManager::registerBasicFormats()` format coverage (verified in-source: WAV/AIFF/FLAC direct, MP3/AAC/M4A via `CoreAudioFormat` on macOS), explicit confirmation `JUCE_USE_MP3AUDIOFORMAT` defaults OFF (no IP risk), background decode pattern to keep the message thread unblocked |
| IMP-02 | User sees the waveform of the loaded file | `juce::AudioThumbnail` + `AudioThumbnailCache` pattern, verified against JUCE's own shipped `AudioPlaybackDemo.h` example; async internal scanning, `drawChannels()` API |
| IMP-03 | User can select a region on the waveform for analysis, or analyze the whole file (default) | Custom overlay pattern built on the same demo's `timeToX`/`xToTime` conversion helpers; `juce::Range<double>` for the selected region; default-to-whole-file behavior as an explicit initial state, not an edge case |
</phase_requirements>

## Summary

Phase 2 is almost entirely JUCE-native: every required capability (OS file drag-in, WAV/AIFF/FLAC/MP3 decode, waveform thumbnail rendering, pixel-perfect nearest-neighbour image scaling) is already covered by modules the project either already links (`juce_audio_utils`, which transitively pulls in `juce_audio_formats`) or gets for free from `juce_graphics`/`juce_events`. The one genuinely custom piece is UI: JUCE has no built-in "select a region on a waveform" component, so that has to be hand-built on top of `AudioThumbnail`'s pixel↔time conversion pattern — and JUCE's own shipped example (`examples/Audio/AudioPlaybackDemo.h`, vendored in this repo under `external/JUCE`) already contains the exact `timeToX`/`xToTime` scaffolding needed, just wired for playhead-position instead of a two-ended selection range.

The second genuinely custom piece is the pixel-art conveyor belt (a locked user decision, not negotiable). No sprite assets exist in the repo yet, so the pragmatic Phase 2 approach is procedural pixel-art (draw a small logical-resolution `Image`, scale it up with `Graphics::lowResamplingQuality` for the chunky nearest-neighbour look) rather than blocking on commissioned artwork; `juce_add_binary_data` remains the documented upgrade path once real sprite sheets exist. A `Timer`-driven child `Component` confined to the belt's own bounds keeps the animation's repaint cost isolated and automatically satisfies the "message-thread only" constraint, since `juce::Timer` callbacks are always delivered on the message thread by construction.

Two research findings meaningfully sharpen the plan: (1) `juce_audio_formats` is **already linked transitively** through `juce_audio_utils`'s own module manifest (verified in JUCE's CMake support code) — no new `target_link_libraries` entry is strictly required, though adding it explicitly is still recommended for clarity; and (2) `CoreAudioFormat::createWriterFor` is an unimplemented stub in JUCE (`jassertfalse; return nullptr;`) — meaning an MP3 test fixture **cannot** be generated at test time the way WAV/AIFF/FLAC fixtures can, and must instead be a small file committed to the repo.

**Primary recommendation:** Build a `LoadedAudio` value type (file ref + native-sample-rate `AudioBuffer<float>` + `Range<double> selectedRegion`) populated by a background `ThreadPool` job per the project's established atomic-shared_ptr handoff pattern; drive `AudioThumbnail` off the same dropped file directly (its own internal async scan is already non-blocking); build region selection as a custom overlay component reusing the demo's pixel↔time math; and keep the conveyor animation as a self-contained `Timer`+`Component` unit rendering procedural pixel-art, with `juce_add_binary_data` documented but deferred until real sprite assets exist.

## Standard Stack

### Core

| Library/Module | Version | Purpose | Why Standard |
|-----------------|---------|---------|---------------|
| `juce::FileDragAndDropTarget` (`juce_gui_basics`, already linked via `juce_gui_extra`) | JUCE 8.0.14 | Receive OS-level file drags onto the editor/conveyor component | Pure virtual mixin (`isInterestedInFileDrag`, `filesDropped`), verified in-source; works uniformly across VST3/AU/Standalone — `juce_StandaloneFilterWindow.h` has no override that would change this behavior in Standalone |
| `juce::AudioFormatManager` + built-in codecs (`juce_audio_formats`) | JUCE 8.0.14 | Decode WAV/AIFF/FLAC directly, MP3/AAC/M4A via `CoreAudioFormat` on macOS | `registerBasicFormats()` source-verified to register Wav, Aiff, Flac (`JUCE_USE_FLAC` defaults 1), OggVorbis, and `CoreAudioFormat` (macOS/iOS only) — covers all 4 required formats with zero extra dependencies |
| `juce::AudioThumbnail` + `juce::AudioThumbnailCache` (`juce_audio_utils`, already linked) | JUCE 8.0.14 | Async waveform scan + `drawChannels()` rendering | Verified against JUCE's own shipped `examples/Audio/AudioPlaybackDemo.h`; async, low-memory (stores a low-res summary, not the full file), self-repaints via `ChangeBroadcaster` |
| `juce::ThreadPool` / `juce::ThreadPoolJob` (`juce_core`, already linked) | JUCE 8.0.14 | Background full-buffer decode (`AudioFormatReader::read()` into `AudioBuffer<float>`) off the message thread | Already the project's established pattern (see `.planning/research/ARCHITECTURE.md` Pattern 2); `AudioFormatReader::read()` is a blocking call with no async variant, so decode must not run inline in `filesDropped` |
| `juce::Timer` (`juce_events`, already linked) | JUCE 8.0.14 | Drive the conveyor belt animation frame-by-frame | Callbacks are always delivered on the message thread by construction — satisfies the locked "animation on message thread only" constraint with no manual thread-marshaling code |
| `juce::Graphics` / `juce::Image` (`juce_graphics`, already linked) | JUCE 8.0.14 | Procedural pixel-art rendering, nearest-neighbour upscaling | `Graphics::ResamplingQuality::lowResamplingQuality` is source-documented as "nearest-neighbour" — exactly the chunky pixel-art look, no extra dependency |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::Range<double>` (`juce_core`) | JUCE 8.0.14 | Represents the selected analysis region in seconds | Already used by the vendored `AudioPlaybackDemo.h` example for `visibleRange`; reuse the same type for `selectedRegion` |
| `juce::FileInputSource` (`juce_core`) | JUCE 8.0.14 | Wraps a `File` for `AudioThumbnail::setSource()` | Confirmed present in `juce_core/streams/juce_FileInputSource.h`; standard pairing shown in the shipped demo |
| `juce_add_binary_data` (JUCE CMake helper, `JUCEUtils.cmake`) | JUCE 8.0.14 | Embed real pixel-art sprite sheets once designed | **Not needed for Phase 2** (no art assets exist yet) — documented here as the upgrade path so it isn't rediscovered later; produces a static-lib target consumed via `target_link_libraries` |
| Catch2 (v3.7+, per project STACK.md) | v3.7+ | Unit-test the pure-logic pieces (pixel↔time conversion, default-region logic, fixture-based decode) | Not yet wired into this repo's CMake — see Validation Architecture below; recommended to introduce now rather than defer, since Phase 3 needs it anyway |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Procedural pixel-art (drawn `Image`, scaled with `lowResamplingQuality`) | `juce_add_binary_data` + real PNG sprite sheets | Real sprites look better but require commissioned/designed art that doesn't exist yet; blocks the phase on an asset pipeline. Procedural is the pragmatic Phase 2 choice; swap in real sprites later without touching the Timer/Component structure. |
| Catch2 (project-standard per STACK.md) | `juce::UnitTest` (built into `juce_core`, zero extra CMake setup) | `juce::UnitTest` needs no FetchContent wiring at all and is already linked — a legitimate lower-friction fallback if Wave 0 time is tight. Recommend Catch2 anyway for consistency with the already-locked project-wide stack decision and because Phase 3's heavier DSP testing will want its assertion/BDD ergonomics. |
| Custom `Range<double>`-based region selection UI | None found — no JUCE built-in for this | JUCE ships waveform *display* (`AudioThumbnail`) but no selection-overlay component; this is expected hand-rolled code, not a "don't hand-roll" violation. |

**Installation:**
```cmake
# CMakeLists.txt additions for Phase 2 — juce_audio_formats is already transitively
# linked via juce_audio_utils's own module manifest (dependencies: juce_audio_processors,
# juce_audio_formats, juce_audio_devices — verified in
# external/JUCE/modules/juce_audio_utils/juce_audio_utils.h). Adding it explicitly below
# is not strictly required but documents the real dependency rather than relying on an
# implementation detail of juce_audio_utils.
target_link_libraries(ChordAI PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_audio_formats)   # explicit, even though already transitive

# Catch2 (Wave 0 gap — not present yet in this repo's CMakeLists.txt)
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1)
FetchContent_MakeAvailable(Catch2)
```

## Architecture Patterns

### Recommended Project Structure (Phase 2 additions)

```
Source/
├── Import/
│   ├── LoadedAudio.h            # value struct: file ref, decoded buffer, sampleRate, selectedRegion
│   ├── AudioFileLoader.h/.cpp   # ThreadPoolJob: AudioFormatReader -> AudioBuffer<float>, publishes via atomic shared_ptr
├── UI/
│   ├── ConveyorBeltComponent.h/.cpp   # Timer + procedural pixel-art belt; also the FileDragAndDropTarget
│   ├── WaveformView.h/.cpp            # AudioThumbnail + AudioThumbnailCache wrapper, drawChannels()
│   ├── RegionSelectorOverlay.h/.cpp   # mouseDown/mouseDrag region select on top of WaveformView, timeToX/xToTime
│   └── MidiSetsPlaceholder.h/.cpp     # empty-state bottom band (Phase 5 populates later)
Tests/
├── AudioFileLoaderTests.cpp     # WAV/AIFF/FLAC fixtures generated at test time; MP3 via committed fixture
├── WaveformRegionTests.cpp      # pixel<->time conversion, default-whole-file logic
└── fixtures/
    └── silence_1s.mp3           # committed — JUCE cannot write MP3 (see Pitfall: no MP3 encoder)
```

### Pattern 1: File drop → background decode → published `LoadedAudio`

**What:** `filesDropped()` on the conveyor component does the minimum possible on the message thread: validate the extension, kick off a `ThreadPoolJob`, and return. The job does `AudioFormatManager::createReaderFor(file)` → `reader->read(&buffer, ...)` → wraps the result in a `LoadedAudio`, then hands it back via `MessageManager::callAsync`, exactly matching the project's already-established `AnalysisPipeline` handoff pattern (`.planning/research/ARCHITECTURE.md` Pattern 2) one phase earlier in the pipeline.
**When to use:** Any file whose decode time is not provably instant — which is every real-world audio file, especially MP3 (`CoreAudioFormat` decode is not free) and anything on a slow drive (Pitfall 10 in `.planning/research/PITFALLS.md` already flags this exact failure mode).
**Example:**
```cpp
// Source: pattern verified against external/JUCE/modules/juce_audio_formats/format/juce_AudioFormatManager.h
// (createReaderFor signature) and this project's own ARCHITECTURE.md Pattern 2.
struct LoadedAudio
{
    juce::File sourceFile;
    juce::AudioBuffer<float> buffer;     // native channel count, no forced downmix
    double sampleRate = 0.0;             // native sample rate — Phase 3's ChordAnalyzer
                                          // already takes sampleRate as a parameter, so no
                                          // resample decision needs to be made here
    juce::Range<double> selectedRegion;  // seconds; defaults to {0.0, buffer length}
};

class AudioFileLoadJob : public juce::ThreadPoolJob
{
public:
    AudioFileLoadJob (juce::File file,
                       juce::AudioFormatManager& fm,
                       std::function<void (std::shared_ptr<const LoadedAudio>)> onDone)
        : ThreadPoolJob ("AudioFileLoad"), fileToLoad (std::move (file)),
          formatManager (fm), callback (std::move (onDone)) {}

    JobStatus runJob() override
    {
        if (shouldExit())
            return jobHasFinished;

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (fileToLoad));
        if (reader == nullptr)
            return jobHasFinished;   // TODO: surface a load-failed state to the UI

        auto result = std::make_shared<LoadedAudio>();
        result->sourceFile = fileToLoad;
        result->sampleRate = reader->sampleRate;
        result->buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&result->buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        result->selectedRegion = { 0.0, reader->lengthInSamples / reader->sampleRate }; // default: whole file

        juce::MessageManager::callAsync ([cb = callback, result] { cb (result); });
        return jobHasFinished;
    }

private:
    juce::File fileToLoad;
    juce::AudioFormatManager& formatManager;
    std::function<void (std::shared_ptr<const LoadedAudio>)> callback;
};
```

### Pattern 2: Waveform + pixel↔time conversion (verified against JUCE's own shipped example)

**What:** `AudioThumbnail` handles its own async scanning internally (its `AudioThumbnailCache` runs a shared background thread) — calling `setSource()` on the message thread is cheap and non-blocking by design, unlike the full-buffer decode in Pattern 1 above. The pixel↔time conversion needed for both waveform zoom/scroll and region selection is already solved in JUCE's own vendored example.
**When to use:** IMP-02 (waveform display) and as the foundation for IMP-03 (region selection).
**Example:**
```cpp
// Source: external/JUCE/examples/Audio/AudioPlaybackDemo.h (DemoThumbnailComp), ISC-licensed,
// vendored in this repo's external/JUCE submodule — verified in-repo, not reconstructed from memory.
class WaveformView : public juce::Component, public juce::ChangeListener
{
public:
    WaveformView (juce::AudioFormatManager& fm)
        : thumbnail (512, fm, thumbnailCache) { thumbnail.addChangeListener (this); }

    void setSource (const juce::File& file)
    {
        thumbnail.setSource (new juce::FileInputSource (file));
        visibleRange = { 0.0, thumbnail.getTotalLength() };
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey);
        if (thumbnail.getTotalLength() > 0.0)
        {
            g.setColour (juce::Colours::lightblue);
            thumbnail.drawChannels (g, getLocalBounds(),
                                     visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
        }
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    // Pattern verified verbatim (renamed) from AudioPlaybackDemo.h's timeToX/xToTime:
    float timeToX (double time) const
    {
        if (visibleRange.getLength() <= 0) return 0.0f;
        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    double xToTime (float x) const
    {
        return (x / (float) getWidth()) * visibleRange.getLength() + visibleRange.getStart();
    }

private:
    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail;
    juce::Range<double> visibleRange;
};
```

### Pattern 3: Region selection overlay (custom — no JUCE built-in)

**What:** The shipped demo's `mouseDown`/`mouseDrag` sets a *playhead position*. IMP-03 needs a two-ended *range* instead. Reuse the same `timeToX`/`xToTime` helpers but track a drag-start time and a live drag-end time, clamped to `[0, totalLength]`, and default to the whole file until the user actually drags.
**When to use:** IMP-03, layered on top of Pattern 2's `WaveformView` (either as a child component or the same component extended).
**Example:**
```cpp
// Original code — no direct JUCE precedent for a *range* selection, only position (Pattern 2).
void RegionSelectorOverlay::mouseDown (const juce::MouseEvent& e)
{
    dragStartTime = juce::jlimit (0.0, waveform.getTotalLength(), (double) waveform.xToTime ((float) e.x));
    selectedRegion = { dragStartTime, dragStartTime };
    repaint();
}

void RegionSelectorOverlay::mouseDrag (const juce::MouseEvent& e)
{
    auto t = juce::jlimit (0.0, waveform.getTotalLength(), (double) waveform.xToTime ((float) e.x));
    selectedRegion = { juce::jmin (dragStartTime, t), juce::jmax (dragStartTime, t) };
    repaint();
}

// Default (no drag yet performed): selectedRegion == { 0.0, waveform.getTotalLength() } — set at load time.
```

### Pattern 4: Conveyor belt — isolated Timer + procedural pixel-art

**What:** A dedicated child `Component` (not the whole editor) owns the `Timer`. `repaint()` on a `Component` marks only that component's region dirty (source-verified: `Component::repaint()` docs — "marks the given region... dirty"), so confining the belt to its own bounds keeps the animation's compositing cost proportional to the belt strip, not the full editor.
**When to use:** The locked conveyor UI decision, satisfied with zero new dependencies.
**Example:**
```cpp
// Original code, built from verified JUCE primitives: Timer (juce_events), Image/Graphics::
// ResamplingQuality::lowResamplingQuality = "nearest-neighbour" (verified in
// external/JUCE/modules/juce_graphics/contexts/juce_GraphicsContext.h).
class ConveyorBeltComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               private juce::Timer
{
public:
    ConveyorBeltComponent() { startTimerHz (30); }   // 30fps: smooth enough for a small looping strip, cheap on CPU

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        static const juce::StringArray accepted { ".wav", ".mp3", ".aiff", ".aif", ".flac" };
        return files.size() == 1
            && accepted.contains (juce::File (files[0]).getFileExtension().toLowerCase());
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        onFileDropped (juce::File (files[0]));   // caller wires this to Pattern 1's AudioFileLoadJob
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality); // nearest-neighbour: chunky pixel look
        g.drawImageWithin (beltFrame (currentFrame), 0, 0, getWidth(), getHeight(),
                            juce::RectanglePlacement::stretchToFit);
    }

    std::function<void (juce::File)> onFileDropped;

private:
    void timerCallback() override
    {
        currentFrame = (currentFrame + 1) % numFrames;
        repaint();   // only this component's bounds are marked dirty, not the full editor
    }

    juce::Image beltFrame (int frameIndex) const; // procedural: draw logical-res pixels into a small Image
    int currentFrame = 0;
    static constexpr int numFrames = 4; // Claude's discretion per CONTEXT.md — placeholder value
};
```

### Anti-Patterns to Avoid

- **Decoding or scanning the file inside `paint()`:** Already flagged in `.planning/research/ARCHITECTURE.md` (Anti-Pattern 4) — `paint()` can be called many times a second; all decode work belongs in the background job (Pattern 1) or `AudioThumbnail`'s own async scan (Pattern 2), never inline in painting code.
- **Calling `AudioFormatReader::read()` directly from `filesDropped`/`isInterestedInFileDrag`:** These run on the message thread; a multi-minute file's full decode is exactly Pitfall 10 from `.planning/research/PITFALLS.md` ("Long analysis blocks the UI thread"). Even though Phase 2 isn't running chord analysis yet, the same failure mode applies to raw decode.
- **Repainting the whole editor on every belt animation frame:** Defeats the point of isolating the animation; confine the `Timer`'s `repaint()` calls to the belt component's own bounds (Pattern 4).
- **Enabling `JUCE_USE_MP3AUDIOFORMAT`:** Already a locked project-wide decision (`.planning/research/PITFALLS.md` Technical Debt table) — `CoreAudioFormat` on macOS already covers MP3 via system codecs with no IP disclaimer; do not add the flag.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|--------------|-----|
| WAV/AIFF/FLAC/MP3 file parsing | A custom RIFF/AIFF/FLAC/MP3 parser | `juce::AudioFormatManager` + `registerBasicFormats()` | Already covers all 4 required formats on macOS with zero extra dependencies (verified in-source); a hand-rolled parser is pure risk for no benefit |
| Waveform min/max downsampling for display | Custom peak-picking / decimation code | `juce::AudioThumbnail` + `AudioThumbnailCache` | Async, cached, self-repainting via `ChangeBroadcaster`; verified against JUCE's own shipped example |
| Nearest-neighbour pixel-art image scaling | Manual per-pixel blit/replicate loop | `Graphics::setImageResamplingQuality (Graphics::lowResamplingQuality)` + `drawImage`/`drawImageWithin` | Source-verified as "nearest-neighbour" resampling; already exactly the desired pixel-art look with one call |
| OS-level file-drag detection (per-platform NSDraggingDestination / Win32 OLE / X11) | Platform-specific drag APIs | `juce::FileDragAndDropTarget` mixin | Already abstracts macOS/Windows/Linux uniformly; verified to work the same in VST3/AU/Standalone (no Standalone-specific override found in `juce_StandaloneFilterWindow.h`) |
| Cross-thread result publication (decode thread → UI) | A mutex-guarded shared struct with a `bool ready` flag | `std::atomic<std::shared_ptr<const LoadedAudio>>` + `MessageManager::callAsync` | Already the project's established pattern (ARCHITECTURE.md Anti-Pattern 3 explicitly calls out the naive-flag version as a data race) |

**Key insight:** Phase 2's only genuinely novel code is UI that JUCE doesn't ship a component for (region *selection*, as opposed to display, and the product-specific pixel-art belt) — everything else (decode, waveform display, drag detection, cross-thread handoff, nearest-neighbour scaling) is a JUCE built-in the project already links or gets transitively.

## Common Pitfalls

### Pitfall 1: Assuming `juce_audio_formats` needs to be added to `target_link_libraries`

**What goes wrong:** Time spent "fixing" a supposed missing-module problem that doesn't actually exist, or worse, confusion when headers already resolve and a redundant link step is treated as load-bearing.
**Why it happens:** The current `CMakeLists.txt` (from Phase 1) only lists `juce_audio_utils`, `juce_audio_processors`, `juce_gui_extra` — `juce_audio_formats` is nowhere in sight, so it looks unlinked.
**How to avoid:** `juce_audio_utils`'s own module manifest declares `dependencies: juce_audio_processors, juce_audio_formats, juce_audio_devices` (verified in `external/JUCE/modules/juce_audio_utils/juce_audio_utils.h`), and JUCE's CMake support code links each module's declared dependencies as `INTERFACE` (verified in `external/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake:646-647`). `juce_audio_formats` headers and link symbols are already available transitively. Add it explicitly anyway for documentation clarity, but don't expect it to "unlock" anything that was actually broken.
**Warning signs:** None — this is a non-issue if verified upfront; flagging it here specifically to prevent wasted debugging time.

### Pitfall 2: MP3 test fixtures cannot be generated by JUCE itself

**What goes wrong:** A test setup helper tries to synthesize an MP3 fixture the same way it synthesizes WAV/AIFF/FLAC fixtures (write a `AudioFormatWriter` for a few test tones) and silently fails or asserts.
**Why it happens:** `CoreAudioFormat::createWriterFor` is an **unimplemented stub** in JUCE 8.0.14 — verified directly in source: `external/JUCE/modules/juce_audio_formats/codecs/juce_CoreAudioFormat.cpp` line ~657, body is `jassertfalse; return nullptr;`. JUCE has no bundled MP3 encoder at all (only the opt-in, explicitly-not-recommended `MP3AudioFormat` reader, and that's read-only).
**How to avoid:** Commit a tiny (~1 second, silence is fine — no copyright concerns, MP3 patents already expired per `.planning/research/STACK.md`) MP3 fixture file to `Tests/fixtures/`, produced once outside the JUCE build (e.g., via `afconvert`/`ffmpeg` on a dev machine), rather than trying to generate it at test time.
**Warning signs:** `createWriterFor` returning `nullptr` for an MP3/CoreAudioFormat writer attempt, or a `jassertfalse` firing in debug builds.

### Pitfall 3: Treating `AudioThumbnail`'s async scan and the analysis-buffer decode as the same operation

**What goes wrong:** Code path confusion where the waveform "loads" (thumbnail visible) but the `LoadedAudio.buffer` needed for Phase 3's analyzer is still empty/mid-decode, or vice versa — leading to a UI that looks ready but analysis would operate on stale/absent data.
**Why it happens:** Both operations are triggered by the same file-drop event and both are "async," but they are two independent subsystems: `AudioThumbnail::setSource()` triggers JUCE's own internal background scan (cheap, thumbnail-only, no full buffer produced), while Pattern 1's `ThreadPoolJob` is a separate, manually-written full decode into an `AudioBuffer<float>`.
**How to avoid:** Keep them explicitly separate in code and state (two independent completion signals), and be deliberate about what each one is for: thumbnail = visual only; `LoadedAudio.buffer` = future analysis input. Phase 2 only strictly needs the thumbnail for IMP-02; the full buffer decode exists now so Phase 3 doesn't have to invent the loading path from scratch, but don't conflate "waveform is showing" with "buffer is ready."
**Warning signs:** Code that reads `thumbnail.isFullyLoaded()` as a proxy for "the analysis buffer is ready," or vice versa.

### Pitfall 4: macOS App Sandbox concerns are largely inapplicable to this phase's target hosts

**What goes wrong:** Time spent building security-scoped-bookmark handling or other sandbox workaround machinery that the project doesn't actually need yet.
**Why it happens:** JUCE forum threads (e.g., the SFZ-sampler AUv3 case) describe real sandbox failures — but those are for AUv3-style sandboxed hosts accessing files the user did *not* directly select (indirect references), and/or plugins that opt into `APP_SANDBOX_ENABLED`.
**How to avoid:** This project's `CMakeLists.txt` does not set `APP_SANDBOX_ENABLED` or `AU_SANDBOX_SAFE` (both default `FALSE` per JUCE's CMake API docs, verified in `external/JUCE/docs/CMake API.md`), and the three required target DAWs (Ableton Live, FL Studio, Logic Pro — per `PLT-03`) are not App-Sandboxed hosts the way GarageBand is. Phase 2's file access is also the simpler case: the user directly drags the exact file being read (direct pasteboard file-URL access), not an indirect reference to a second file the user never selected. No special entitlement/bookmark handling is needed for Phase 2's scope. Revisit only if GarageBand support or `AU_SANDBOX_SAFE` is explicitly requested later (not currently a requirement).
**Warning signs:** N/A for current scope — flagged here so it isn't mistakenly treated as a blocking unknown.

### Pitfall 5: Sizing/proportioning the belt animation without a stop condition

**What goes wrong:** The `Timer` keeps firing (and repainting) even when the plugin editor is closed/hidden, wasting cycles in a host that keeps the processor alive with the editor destroyed.
**Why it happens:** `Timer` is owned by the belt `Component`; if the `Component`/`Editor` is destroyed, the `Timer` destructor stops it automatically (safe), but if instead the component is merely *hidden* (not destroyed) while the editor stays around, a naive implementation might keep ticking unnecessarily.
**How to avoid:** Not a correctness bug (JUCE's `Timer` destructor handles the destroyed case safely), just an efficiency note — consider `stopTimer()`/`startTimerHz()` on visibility change if profiling shows it matters. Not required for Phase 2 to ship correctly; flagged as a minor discretionary polish item, not a blocker.

## Code Examples

Verified patterns from official/in-repo sources:

### Registering formats and creating a reader
```cpp
// Source: external/JUCE/modules/juce_audio_formats/format/juce_AudioFormatManager.cpp (registerBasicFormats)
// and juce_AudioFormatManager.h (createReaderFor signature) — both verified in-repo.
juce::AudioFormatManager formatManager;
formatManager.registerBasicFormats();  // WAV, AIFF, FLAC, OggVorbis, and (macOS/iOS only) CoreAudioFormat
                                        // — CoreAudioFormat covers MP3/AAC/ALAC/M4A via AudioToolbox.
                                        // JUCE_USE_MP3AUDIOFORMAT is NOT enabled (defaults to 0) — do not enable it.

std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (droppedFile));
if (reader != nullptr)
{
    // reader->sampleRate, reader->numChannels, reader->lengthInSamples all available here
}
```

### Nearest-neighbour pixel-art scaling
```cpp
// Source: external/JUCE/modules/juce_graphics/contexts/juce_GraphicsContext.h
// (ResamplingQuality enum: lowResamplingQuality = "Just uses a nearest-neighbour algorithm")
g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
g.drawImageWithin (pixelArtImage, destX, destY, destWidth, destHeight,
                    juce::RectanglePlacement::stretchToFit);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|-------------------|---------------|--------|
| Projucer-generated IDE projects | CMake (`juce_add_plugin`, `juce_generate_juce_header`) | Already the project's standard since Phase 1 | No change needed for Phase 2 — same CMake-based workflow, just new `target_sources`/possibly a new binary-data target later |

No JUCE API deprecations affect Phase 2's scope in 8.0.14 — `AudioThumbnail`, `AudioFormatManager`, `FileDragAndDropTarget`, and `Timer` are all stable APIs unchanged since JUCE 6/7 per `.planning/research/STACK.md`'s existing finding.

## Open Questions

1. **Exact editor size/layout proportions for the three vertical bands (conveyor / waveform / bottom list)**
   - What we know: CONTEXT.md explicitly leaves this at Claude's discretion; the current `PluginEditor.cpp` still has Phase 1's placeholder `setSize (400, 300)`.
   - What's unclear: Whether 400x300 is remotely adequate once a horizontal conveyor strip, a legible waveform, and a reserved bottom list all need to fit — likely needs to grow significantly (e.g., 700-900px wide, 450-600px tall) to keep the waveform usable.
   - Recommendation: Treat initial sizing as a planning-time decision (task-level), not something to lock further in research; prioritize waveform legibility and belt width since those are the load-bearing interactions for IMP-01/02/03.

2. **Pixel-art frame count/rate and tile size**
   - What we know: Explicitly "Claude's Discretion" per CONTEXT.md; no existing assets or palette in the repo to anchor a decision.
   - What's unclear: Whether procedural (Phase 2's recommended approach) will look acceptable enough to ship, or whether a fast follow-up with real sprite assets is expected soon after.
   - Recommendation: Build the `Timer`/`Component` scaffolding (Pattern 4) so it's asset-format-agnostic — swapping procedural drawing for `juce_add_binary_data`-embedded sprites later should only touch `beltFrame()`'s implementation, not the surrounding animation/drag-drop logic.

3. **Whether to surface a load-failure UI state in Phase 2**
   - What we know: `AudioFormatManager::createReaderFor()` returns `nullptr` on unsupported/corrupt files; Pattern 1's example currently just silently returns from the job in that case.
   - What's unclear: REQUIREMENTS.md's IMP-01 doesn't explicitly require error-state UI (only that valid files load).
   - Recommendation: At minimum, don't crash or hang on a bad drop; a full error-toast UI is not required by IMP-01/02/03 and can be deferred, but the planner should decide explicitly rather than leave it as a silent no-op with no visual feedback at all.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7+ (per project-wide STACK.md decision) — **not yet wired into this repo's CMake** |
| Config file | none — see Wave 0 Gaps below |
| Quick run command | `ctest --test-dir build -R ChordAITests --output-on-failure` (target name proposed; confirm/adjust at plan time) |
| Full suite command | Same as quick run for Phase 2 — test count is small enough that no separate "quick vs full" split is needed yet (revisit once Phase 3's DSP tests land) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|---------------------|--------------|
| IMP-01 | WAV file decodes via `AudioFormatManager` | unit | `ctest -R AudioFileLoaderTests.WavDecode` | ❌ Wave 0 |
| IMP-01 | AIFF file decodes via `AudioFormatManager` | unit | `ctest -R AudioFileLoaderTests.AiffDecode` | ❌ Wave 0 |
| IMP-01 | FLAC file decodes via `AudioFormatManager` | unit | `ctest -R AudioFileLoaderTests.FlacDecode` | ❌ Wave 0 |
| IMP-01 | MP3 file decodes via `CoreAudioFormat` (committed fixture, JUCE cannot write MP3) | unit | `ctest -R AudioFileLoaderTests.Mp3Decode` | ❌ Wave 0 (test + fixture) |
| IMP-01 | OS-level drag-and-drop actually delivers a file to `filesDropped` | manual-only | Manual: drag WAV/MP3/AIFF/FLAC onto Standalone window; justification: OS-level drag simulation isn't practical to automate, and `.planning/research/PITFALLS.md` Pitfall 8 already establishes DAW-specific drag behavior requires manual per-host verification | n/a |
| IMP-02 | Waveform thumbnail populates from a loaded file | unit | `ctest -R WaveformRegionTests.ThumbnailPopulates` (assert `getTotalLength() > 0` after pumping the message loop on a fixture) | ❌ Wave 0 |
| IMP-02 | Waveform renders correctly/legibly on screen | manual-only | Manual: visual check in Standalone; justification: no visual-regression tooling exists in this project yet | n/a |
| IMP-03 | Pixel↔time conversion (`timeToX`/`xToTime`) is mathematically correct | unit | `ctest -R WaveformRegionTests.PixelTimeConversion` | ❌ Wave 0 |
| IMP-03 | Default region is the whole file when no selection is made | unit | `ctest -R WaveformRegionTests.DefaultWholeFile` | ❌ Wave 0 |
| IMP-03 | Mouse-drag selection produces a correctly clamped `Range<double>` | unit | `ctest -R WaveformRegionTests.DragSelectionClamped` (drive via synthetic coordinates, not real `MouseEvent`s) | ❌ Wave 0 |
| (supporting) | Background decode doesn't block the message thread on a multi-minute file | manual-only | Manual: drop a 3+ minute file, confirm the editor stays responsive throughout; justification: message-thread responsiveness under real OS scheduling isn't meaningfully unit-testable | n/a |
| (supporting) | pluginval strict mode still green after linking `juce_audio_formats` explicitly + adding new UI components | automated (existing tooling) | `tools/pluginval.app` run, as already established in Phase 1 | ✅ (tool exists, rerun as regression gate) |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -R ChordAITests --output-on-failure` (fast — fixture-based decode + pure pixel/time math, no audio device needed)
- **Per wave merge:** Full `ctest` suite + a `pluginval` strict-mode pass (VST3+AU) since new modules/components are added this phase
- **Phase gate:** Full suite green + a manual Standalone drag-and-drop smoke test (WAV/MP3/AIFF/FLAC) covering IMP-01/02/03's acceptance criteria before `/gsd:verify-work`. Full Ableton/FL Studio/Logic Pro drag-and-drop matrix testing is owned by Phase 7 (`PLT-03`, per REQUIREMENTS.md traceability table) — Phase 2's manual pass only needs to confirm the interaction works at all in at least one real host, not the full per-DAW matrix.

### Wave 0 Gaps
- [ ] `CMakeLists.txt` — add Catch2 v3.7+ via `FetchContent` + a `Tests/` executable target + `enable_testing()`/CTest wiring (none exists yet; Phase 1 did not set this up)
- [ ] `Tests/AudioFileLoaderTests.cpp` — covers IMP-01 (WAV/AIFF/FLAC fixtures generated at test-setup time via `WavAudioFormat`/`AiffAudioFormat`/`FlacAudioFormat` writers — all three confirmed to support `createWriterFor` in source; MP3 via the committed fixture below)
- [ ] `Tests/fixtures/silence_1s.mp3` — a tiny (~1s silence) committed MP3 fixture; **must be produced once outside the JUCE build** (e.g. via `afconvert`/`ffmpeg`) since `CoreAudioFormat::createWriterFor` is an unimplemented stub in JUCE (verified in source) and JUCE has no bundled MP3 encoder
- [ ] `Tests/WaveformRegionTests.cpp` — covers IMP-02/IMP-03 pixel↔time conversion and default-selection logic (pure functions; only needs a pumped `MessageManager` loop for the thumbnail-populates case, not a real audio device)
- [ ] Explicitly add `juce::juce_audio_formats` to `target_link_libraries` in `CMakeLists.txt` — technically already transitively available via `juce_audio_utils`, but explicit declaration documents the real dependency (see Pitfall 1)

*(No further gaps — existing `tools/pluginval.app` and the Phase 1 CMake skeleton cover the rest.)*

## Sources

### Primary (HIGH confidence — verified directly against vendored `external/JUCE` 8.0.14 source in this repo)
- `external/JUCE/modules/juce_audio_utils/juce_audio_utils.h` — module manifest confirms `juce_audio_formats` is a declared dependency of the already-linked `juce_audio_utils`
- `external/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake` (line 646-647) — confirms JUCE's CMake support links each module's declared dependencies as `INTERFACE`, making `juce_audio_formats` transitively available
- `external/JUCE/modules/juce_audio_formats/format/juce_AudioFormatManager.cpp` — `registerBasicFormats()` body, confirms exact format list registered on macOS
- `external/JUCE/modules/juce_audio_formats/juce_audio_formats.h` — confirms `JUCE_USE_MP3AUDIOFORMAT` defaults to `0`
- `external/JUCE/modules/juce_audio_formats/codecs/juce_CoreAudioFormat.h`/`.cpp` — class doc ("should be able to understand formats such as mp3, m4a, etc."), `StreamKind::kMp3` enum entry, and confirmed-unimplemented `createWriterFor` (`jassertfalse; return nullptr;`)
- `external/JUCE/modules/juce_gui_basics/mouse/juce_FileDragAndDropTarget.h` — exact interface signature (`isInterestedInFileDrag`, `filesDropped`)
- `external/JUCE/modules/juce_audio_utils/gui/juce_AudioThumbnail.h` — `setSource`, `setReader`, `drawChannels`, `isFullyLoaded`, `getTotalLength` API surface
- `external/JUCE/examples/Audio/AudioPlaybackDemo.h` — real, ISC-licensed, shipped JUCE example (`DemoThumbnailComp`) demonstrating the exact `AudioThumbnail` + `FileDragAndDropTarget` + `timeToX`/`xToTime` pattern this phase needs
- `external/JUCE/modules/juce_graphics/contexts/juce_GraphicsContext.h` — `ResamplingQuality` enum, confirms `lowResamplingQuality` = nearest-neighbour
- `external/JUCE/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.h` (line 417) — confirms `state` is a public `ValueTree` member, supporting custom non-parameter properties for state persistence
- `external/JUCE/modules/juce_audio_formats/codecs/juce_FlacAudioFormat.h`, `juce_AiffAudioFormat.h` — confirm `createWriterFor` is implemented for FLAC/AIFF (usable for programmatic test fixtures), unlike CoreAudioFormat/MP3
- `external/JUCE/docs/CMake API.md` (lines 468-616) — `APP_SANDBOX_ENABLED`, `AU_SANDBOX_SAFE` both default `FALSE`, confirms this project currently opts into neither
- `external/JUCE/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h` — no `FileDragAndDropTarget`/`filesDropped` override found, confirming Standalone doesn't special-case file-drag handling
- This repo's own `CMakeLists.txt`, `Source/PluginProcessor.{h,cpp}`, `Source/PluginEditor.{h,cpp}` — current linked modules and APVTS wiring, read directly

### Secondary (MEDIUM confidence)
- [JUCE Forum: Apple Sandbox — Accessing files other than the one the user specified](https://forum.juce.com/t/apple-sandbox-accessing-files-other-than-the-one-the-user-specified/39977) — describes sandbox friction for *indirectly referenced* files (SFZ sample libraries) under AUv3-style sandboxing; confirmed this is a different scenario from Phase 2's direct single-file drag, but the underlying sandbox mechanics are consistent with what `APP_SANDBOX_ENABLED`/`AU_SANDBOX_SAFE` control
- [JUCE Forum: Main threads and synchronization for heavy visualization](https://forum.juce.com/t/main-threads-and-synchronization-for-heavy-visualization/31236) — already cited in `.planning/research/ARCHITECTURE.md`, reused here for the background-decode handoff pattern
- `.planning/research/ARCHITECTURE.md`, `.planning/research/STACK.md`, `.planning/research/PITFALLS.md` — project-level research this phase's findings build directly on top of (cited inline throughout)

### Tertiary (LOW confidence)
- None — all findings for this phase were verifiable either directly in the vendored JUCE source or cross-referenced against the project's own prior research documents.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — every module/class referenced was read directly from the vendored `external/JUCE` 8.0.14 source in this repo, not from training-data assumptions
- Architecture: HIGH — the decode/publish pattern reuses this project's own already-established, previously-researched `ThreadPool`+atomic-shared_ptr pattern; the waveform/region pattern is verified against a real shipped JUCE example present in the repo
- Pitfalls: HIGH for the JUCE-mechanics pitfalls (module linking, MP3 writer, sandbox flags — all source-verified); MEDIUM for the macOS-sandbox risk assessment specifically, since it's inferred from forum reports plus CMake docs rather than a live test against a sandboxed host

**Research date:** 2026-07-12
**Valid until:** ~30 days (stable JUCE APIs; revisit sooner only if the JUCE submodule is bumped past 8.0.14 or if real sprite assets change the pixel-art approach)
