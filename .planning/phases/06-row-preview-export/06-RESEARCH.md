# Phase 6: Row Preview & Export - Research

**Researched:** 2026-07-13
**Domain:** JUCE 8 Standard MIDI File writing, OS-level external drag-and-drop, native save dialogs, and a real-time-safe internal audio preview path inside a currently-inert `processBlock`
**Confidence:** HIGH (JUCE 8 APIs verified directly against the vendored `external/JUCE` source tree in this repo, not training-data recall; DAW drag-drop specifics verified against multiple independent JUCE forum reports; RT-safety guidance extends this project's own established, already-shipping `PluginProcessor` conventions)

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PRV-01 | User can audition any MIDI row with a built-in piano/pad sound before dragging it out | Pattern 3 (RT-safe double-buffer preview renderer using `juce::ADSR` + additive sine synthesis, driven from `processBlock`) |
| EXP-01 | User can drag any MIDI row from the plugin straight into the DAW piano roll (temp `.mid` + external file drag) | Pattern 1 (`MidiFileWriter`), Pattern 2 (drag-out flow via `performExternalDragDropOfFiles`), Pitfall 1 (Ableton delete-in-callback root cause + fix) |
| EXP-02 | User can save any MIDI row to disk as a `.mid` file (fallback path) | Pattern 4 (`juce::FileChooser` async save flow), reuses the same `MidiFileWriter` as EXP-01 |
| EXP-03 | Exported MIDI is bar-aligned and carries the detected tempo | Pattern 1's tick-conversion code (verified against `docs.juce.com`/vendored source), Code Examples' round-trip test |
</phase_requirements>

## Summary

Everything this phase needs is already a linked JUCE module — no new dependency, no `FetchContent` addition. The MIDI-writing half (EXP-01/02/03) is small, mechanical, and already sketched at the API level in `05-RESEARCH.md`'s "Phase 6 conversion point": convert each row's beat-domain `NoteEvent`s into a `juce::MidiMessageSequence` (ticks = `beats * ticksPerQuarterNote`), add a tempo meta-event derived from `AnalysisResult.bpm`, add a 4/4 time-signature meta-event (matches `AnalysisResult.barStartBeatIndices`' existing 4/4 assumption), write it into a `juce::MidiFile`, and stream it to disk. The only real design decisions are (a) file-naming convention and (b) format-0-vs-1 — this research recommends format 1 with tempo/time-sig on their own first track and notes on a second track, the most broadly-compatible shape across Ableton/FL Studio/Logic.

The drag-out half (EXP-01) is a single `static` call — `juce::DragAndDropContainer::performExternalDragDropOfFiles` — confirmed callable from `MidiRowView` without any ancestor needing to derive from `DragAndDropContainer`. The DAW fragility flagged as CRITICAL in `PITFALLS.md` has a concrete, verified root cause for the worst-reported case (Ableton): a completion callback that deletes the temp file races the host's own asynchronous read of it, and Ableton reports "could not be opened" rather than crashing. The fix is well-documented and simple: never delete the temp file in the drag completion callback; defer cleanup to the *start* of the next drag (or plugin shutdown). This turns a "fragile, DAW-voodoo" pitfall into a "known bug class with a known fix," which changes how defensively Phase 6 needs to be planned.

The audition half (PRV-01) is the one genuinely new architectural piece: `processBlock` is currently a real-time-safe no-op (zero allocation, zero locks, per Phase 1's established rule), and it must stay that way while gaining the ability to play back a rendered preview. The recommended design pre-renders a row's notes to a plain PCM buffer on the **message thread** (using `juce::ADSR` + a tiny hand-rolled additive/sine voice — no sample playback, no new dependency) into one of two pre-existing raw `juce::AudioBuffer<float>` slots, then flips a small set of plain `std::atomic<int>`/`std::atomic<bool>` indices to hand it to `processBlock`, which does nothing but copy samples out. This deliberately avoids reusing this codebase's own existing `std::atomic_load`/`atomic_store` `shared_ptr` publication idiom for this one case — that idiom is safe today only because every existing use is message-thread-to-message-thread; feeding it into the actual audio callback would reintroduce exactly the shared_ptr-refcount audio-thread risk `PITFALLS.md` Pitfall 6 already warns against, in a new place the existing pitfall doc didn't anticipate. This is a novel, project-specific finding (not previously documented) and is called out explicitly below.

**Primary recommendation:** Build a small new `Source/MidiGen/MidiFileWriter.h/.cpp` (pure function, `MidiSetRow` + `bpm` → written file, unit-testable without JUCE GUI) for EXP-01/02/03; wire `MidiRowView` for click-to-audition and drag-to-export using `performExternalDragDropOfFiles` with the deferred-cleanup pattern; add a small `AuditionRenderer` (message-thread-only render function) plus a double-buffer handoff into `PluginProcessor::processBlock` for PRV-01. No third-party audio synthesis library, no sample playback — `juce::ADSR` (already in `juce_audio_basics`) covers the only genuinely reusable piece (click-free envelopes); the tone itself is a few lines of additive sine math, consistent with this codebase's existing `writeToneWavFixture()` test-fixture style.

## User Constraints

No `06-CONTEXT.md` exists for this phase (checked: `.planning/phases/06-row-preview-export/` contains no `*-CONTEXT.md` file) — `/gsd:discuss-phase` has not been run for Phase 6. This research is therefore unconstrained by locked user decisions beyond what's already fixed in `REQUIREMENTS.md` and the phase description supplied by the orchestrator (built-in piano/pad audition sound, temp-`.mid`-drag primary export path, `.mid` save-to-disk fallback, bar-aligned + tempo-carrying export). The one relevant standing decision from `STATE.md`'s Decisions log: *"Preview (PRV-01) grouped with Export (Phase 6) rather than Generation (Phase 5) — both are row-level interactions in the same UI component (`MidiRowView`)."* Treat `MidiRowView` as the single component gaining both audition and drag/save interaction, not two separate components.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `juce::MidiFile` / `juce::MidiMessageSequence` / `juce::MidiMessage` (`juce_audio_basics`, already linked) | ships with JUCE 8.0.14 (pinned submodule) | Build + write the Standard MIDI File for a row | Verified directly against vendored `external/JUCE/modules/juce_audio_basics/midi/juce_MidiFile.h` — `setTicksPerQuarterNote(int)`, `addTrack(const MidiMessageSequence&)`, `writeTo(OutputStream&, int midiFileType = 1)`. No SMF byte-format code to hand-roll. |
| `juce::DragAndDropContainer::performExternalDragDropOfFiles` (`juce_gui_basics`, already linked) | ships with JUCE 8.0.14 | Drag a temp `.mid` file out of the plugin editor into the DAW | Confirmed `static` in the vendored header (`juce_DragAndDropContainer.h` line 204) — callable directly from `MidiRowView::mouseDrag` with no `DragAndDropContainer` ancestor requirement. This is JUCE's only supported mechanism for plugin-to-host file drag (see `PITFALLS.md` Pitfall 7 — live MIDI-out routing is explicitly out of scope). |
| `juce::FileChooser` (`juce_gui_basics`, already linked) | ships with JUCE 8.0.14 | Native async save dialog for EXP-02 | Verified `launchAsync(int flags, std::function<void(const FileChooser&)>, ...)` exists (non-`JUCE_MODAL_LOOPS_PERMITTED`-gated, safe for a plugin editor). `FileBrowserComponent::FileChooserFlags::saveMode \| canSelectFiles \| warnAboutOverwriting` confirmed in vendored `juce_FileBrowserComponent.h`. No sandbox entitlement blocks this — confirmed no `APP_SANDBOX`/hardened-runtime restriction is declared anywhere in this project's `CMakeLists.txt` or generated `.entitlements` (checked `build/`/`build-release/` artefacts: JUCE's defaults only, no custom entitlements file in `Source/` or project root). |
| `juce::ADSR` (`juce_audio_basics`, already linked) | ships with JUCE 8.0.14 | Click-free amplitude envelope for the audition synth voice | Verified class exists at `external/JUCE/modules/juce_audio_basics/utilities/juce_ADSR.h` (`Parameters(attack, decay, sustain, release)`, `noteOn()`/`noteOff()`, `applyEnvelopeToBuffer`/per-sample `getNextSample()`). JUCE's own shipped unit test (`juce_ADSR_test.cpp`) already covers the envelope-shape edge cases (zero-length stages, retrigger) — reuse it instead of hand-rolling attack/decay/release ramp math. |
| `juce::File::getSpecialLocation(tempDirectory)` (`juce_core`, already linked) | ships with JUCE 8.0.14 | Location for the drag-out temp `.mid` | Confirmed enum value at `juce_File.h` line 937. |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Plain `std::sin` additive synthesis (no library) | — | The audition "piano-ish" tone itself | Matches this codebase's own existing convention (`Tests/PluginProcessorMidiGenTests.cpp`'s `writeToneWavFixture()` already generates tone via a raw `std::sin` loop) — a 2-4 partial additive tone (fundamental + quiet 2nd/3rd harmonic, slight detune) is a handful of lines, trivially deterministic/testable, and explicitly scoped as "keep tiny" — do not reach for `juce::dsp::Oscillator`'s per-sample processing-graph machinery for something this small. |
| `std::atomic<int>` / `std::atomic<bool>` (no library) | — | Message-thread → audio-thread handoff for the pre-rendered preview buffer | See Pitfall 2 below — this is a **deliberate departure** from this codebase's existing `atomic_load`/`atomic_store` `shared_ptr` idiom, which is unsafe to call from `processBlock`. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Pre-rendered double-buffer audition (recommended) | Live polyphonic synth voice state machine running inside `processBlock` itself (option (a) from the phase brief) | More "correct" real-time synth architecture, but strictly more state and more surface area for RT-safety bugs (voice allocation, note-on/off scheduling inside the audio callback) for a feature whose entire job is "play a short, fully-known-in-advance sequence." The pre-render approach produces byte-identical, directly-unit-testable output and needs zero synth-voice logic inside `processBlock` at all — just a buffer copy. Recommended default; revisit only if a future v1.x feature needs live retriggering mid-playback. |
| `juce::MidiFile` format 1 (recommended, see Pattern 1) | Format 0 (single track, tempo/time-sig interleaved with notes) | Format 0 is simpler to write and technically valid, but interleaving meta-events with note events on one track is a less common shape for "MIDI pack"-style drag files; format 1's separate tempo track is the more conventional shape and avoids any host-specific quirk about meta-events appearing after note-on events on the same track. Both are accepted by all three target DAWs per verified forum reports (Ableton/Logic/FL Studio/Reaper/Studio One/Pro Tools/GarageBand) — this is a "pick the more conventional one," not a compatibility-blocking choice. |
| Deferred-cleanup temp file (recommended, see Pitfall 1) | Delete temp file synchronously in the `performExternalDragDropOfFiles` completion callback | This is the exact pattern independently reported to break Ableton (host reads the file asynchronously after the callback fires, sees it gone, shows "could not be opened"). Not a real alternative — documented here because it's the *first thing anyone tries* and the thing to explicitly avoid. |

**Installation:**
No new dependency. All types above are in `juce_audio_basics` and `juce_gui_basics`, both already in `ChordAI`'s and `ChordAITests`' `target_link_libraries` in `CMakeLists.txt`. New source files only:
```
Source/MidiGen/MidiFileWriter.h
Source/MidiGen/MidiFileWriter.cpp
Source/Audio/AuditionRenderer.h        # or Source/MidiGen/ -- planner's call, see Open Questions
Source/Audio/AuditionRenderer.cpp
```
Add both `.cpp` files to `CHORDAI_MIDIGEN_SOURCES` (or a new `CHORDAI_AUDITION_SOURCES` list) and to both `target_sources(ChordAI ...)` and `target_sources(ChordAITests ...)` in `CMakeLists.txt`, exactly as every prior phase's new module was wired in (e.g. 05-01's `CHORDAI_MIDIGEN_SOURCES` addition).

## Architecture Patterns

### Recommended Project Structure
```
Source/
├── MidiGen/
│   ├── MidiSetRow.h            # existing, unchanged
│   ├── NoteEvent.h             # existing, unchanged
│   ├── MidiFileWriter.h        # NEW — pure function: MidiSetRow + bpm -> juce::MidiFile / write-to-disk
│   └── MidiFileWriter.cpp
├── Audio/                      # NEW folder — audio-thread-adjacent code, kept out of MidiGen's
│   │                           #   "no JUCE dependency" pure-value-type folder boundary rule
│   ├── AuditionRenderer.h      # NEW — pure-ish function: MidiSetRow + bpm + sampleRate -> AudioBuffer<float>
│   ├── AuditionRenderer.cpp
│   ├── AuditionVoice.h         # NEW — juce::ADSR + additive sine tone, one row's worth of mixed notes
│   └── AuditionVoice.cpp
├── UI/
│   ├── MidiRowView.h/.cpp      # MODIFIED — gains play/stop hit-zone, drag-out, save-icon hit-zone
│   └── MidiSetsPanel.h/.cpp    # unchanged structurally; stop-audition-on-rebuild hook, see Pitfall 3
└── PluginProcessor.h/.cpp      # MODIFIED — owns the double-buffer + playback API + processBlock read path
```

### Pattern 1: `MidiFileWriter` — beat-domain rows to a Standard MIDI File

**What:** One pure function converting a `MidiSetRow` + tempo into a fully-formed `juce::MidiFile`, plus a thin wrapper that streams it to a `juce::File`. This is the shared core both EXP-01 (temp file for drag) and EXP-02 (user-chosen save path) call — do not duplicate the conversion logic between the two entry points.

**When to use:** Both drag-out and save-to-disk. Both start from the exact same `MidiSetRow` (already held by `MidiRowView::getRow()`) and the exact same `AnalysisResult.bpm` (available via `PluginProcessor::getAnalysisResult()`).

**Example** (extends the exact shape already verified and sketched in `05-RESEARCH.md`'s "Phase 6 conversion point," confirmed again here against the vendored JUCE 8 source):
```cpp
// Source/MidiGen/MidiFileWriter.h
#pragma once
#include <JuceHeader.h>
#include "MidiSetRow.h"

namespace MidiFileWriter
{
    constexpr int kTicksPerQuarterNote = 960; // high-resolution, standard DAW-import value

    // Pure: same row + bpm always produces byte-identical MIDI file bytes.
    // bpm <= 0.0 is defensively treated as 120.0 (AnalysisResult.bpm should
    // never be 0 post-Phase-3's fix, but a MIDI file's tempo meta-event
    // cannot represent a zero/negative tempo -- guard here, not upstream).
    juce::MidiFile buildMidiFile (const MidiSetRow& row, double bpm);

    // Returns true on success. Writes via a fresh OutputStream each call --
    // caller is responsible for the target File already being writable
    // (parent directory exists).
    bool writeToFile (const MidiSetRow& row, double bpm, const juce::File& destination);
}
```
```cpp
// Source/MidiGen/MidiFileWriter.cpp
#include "MidiFileWriter.h"

namespace MidiFileWriter
{
    namespace
    {
        juce::MidiMessageSequence toNoteSequence (const std::vector<NoteEvent>& notes, int tpqn)
        {
            juce::MidiMessageSequence seq;
            for (const auto& n : notes)
            {
                if (n.lengthBeats <= 0.0)
                    continue; // zero/negative-length notes are dropped, not written --
                              // some hosts silently discard a zero-tick-length note anyway,
                              // and a defensive guard here is cheaper than relying on that.

                const double onTick  = n.startBeats * (double) tpqn;
                const double offTick = (n.startBeats + n.lengthBeats) * (double) tpqn;

                seq.addEvent (juce::MidiMessage::noteOn  (1, n.pitch, n.velocity), onTick);
                seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch, 0.0f),        offTick);
            }
            seq.updateMatchedPairs();
            seq.sort();
            return seq;
        }
    }

    juce::MidiFile buildMidiFile (const MidiSetRow& row, double bpm)
    {
        const double safeBpm = bpm > 0.0 ? bpm : 120.0;

        juce::MidiFile file;
        file.setTicksPerQuarterNote (kTicksPerQuarterNote); // MUST be set before addTrack

        // Track 0: tempo + time signature only (format-1 convention -- keeps
        // meta-events off the note track, the more broadly-conventional SMF
        // shape for drag-imported "MIDI pack"-style files).
        juce::MidiMessageSequence metaTrack;
        metaTrack.addEvent (juce::MidiMessage::tempoMetaEvent ((int) juce::roundToInt (60000000.0 / safeBpm)), 0.0);
        metaTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0); // v1: 4/4 only,
            // matches AnalysisResult.barStartBeatIndices' existing "every 4th beat" 4/4 assumption
        metaTrack.addEvent (juce::MidiMessage::endOfTrack(), 0.0);
        file.addTrack (metaTrack);

        // Track 1: the row's notes.
        auto noteTrack = toNoteSequence (row.notes, kTicksPerQuarterNote);
        noteTrack.addEvent (juce::MidiMessage::endOfTrack(),
                             noteTrack.getEndTime() /*already in ticks*/);
        file.addTrack (noteTrack);

        return file;
    }

    bool writeToFile (const MidiSetRow& row, double bpm, const juce::File& destination)
    {
        if (! destination.getParentDirectory().createDirectory())
            return false; // no-op if it already exists; false only on real failure

        juce::TemporaryFile temp (destination); // atomic replace -- see Pitfall 4
        {
            juce::FileOutputStream stream (temp.getFile());
            if (! stream.openedOk())
                return false;

            auto midiFile = buildMidiFile (row, bpm);
            if (! midiFile.writeTo (stream, 1)) // format 1
                return false;
        }
        return temp.overwriteTargetFileWithTemporary();
    }
}
```
Sources: `docs.juce.com/master/classMidiFile.html`, `classMidiMessageSequence.html`, `classMidiMessage.html`, cross-verified directly against `external/JUCE/modules/juce_audio_basics/midi/juce_MidiFile.h` and `juce_MidiMessage.h` (lines 260-304, 586-647) and `juce_MidiMessageSequence.h` (lines 140-241) in this repo's vendored JUCE 8.0.14.

### Pattern 2: Drag-out flow in `MidiRowView`

**What:** On a real drag gesture (not a click), write a fresh temp `.mid` file for the row under the mouse and hand it to `performExternalDragDropOfFiles`.

**When to use:** `MidiRowView::mouseDrag`, gated so it fires exactly once per drag gesture.

```cpp
// Source/UI/MidiRowView.h (added members)
void mouseDown (const juce::MouseEvent&) override;
void mouseDrag (const juce::MouseEvent&) override;
void mouseUp   (const juce::MouseEvent&) override;

std::function<double()> getBpmForExport; // supplied by MidiSetsPanel/editor wiring --
                                          // MidiRowView doesn't reach into PluginProcessor directly

private:
bool dragStarted = false;

// Source/UI/MidiRowView.cpp
void MidiRowView::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;
    // ... existing/new hit-test for play-icon / save-icon zones happens here,
    // see MidiRowLayout.h note in Architecture Patterns below.
}

void MidiRowView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || ! e.mouseWasDraggedSinceMouseDown())
        return;

    dragStarted = true; // guard: performExternalDragDropOfFiles must be called exactly
                         // once per gesture -- mouseDrag fires repeatedly while dragging

    const double bpm = getBpmForExport ? getBpmForExport() : 120.0;
    auto tempFile = ExportTempFiles::writeRowTempFile (row, bpm); // see Pattern 1 / Pitfall 1

    if (tempFile.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            { tempFile.getFullPathName() }, /*canMoveFiles*/ false, this, /*callback*/ nullptr);
            // NOTE: no cleanup callback here -- see Pitfall 1. canMoveFiles=false: the
            // DAW must copy/import the bytes, not take ownership of our temp file.
}

void MidiRowView::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted)
    {
        // treat as a click: hit-test play/save zones and act
    }
    dragStarted = false;
}
```
`performExternalDragDropOfFiles` is `static`, confirmed at `juce_DragAndDropContainer.h` line 204 — `MidiRowView` needs no `DragAndDropContainer` ancestor. `sourceComponent` (`this`) is passed explicitly per the header's own doc ("normally JUCE will assume the component under the mouse is the source... you can use this parameter to override this") — pass it explicitly since `MidiRowView` is a small nested component inside `MidiSetsPanel`.

### Pattern 3: RT-safe audition — pre-render + double-buffer handoff

**What:** Render a row's notes to a fixed PCM buffer on the message thread; hand it to `processBlock` via a lock-free double-buffer so the audio thread only ever does a bounds-checked memory copy.

**When to use:** PRV-01's play button.

```cpp
// Source/Audio/AuditionRenderer.h
#pragma once
#include <JuceHeader.h>
#include "../MidiGen/MidiSetRow.h"

namespace AuditionRenderer
{
    // Message-thread only. Allocates freely (same allocation-site category as
    // AudioFileLoadJob's decode -- NOT the processBlock "prepareToPlay only"
    // rule, which is about the AUDIO thread specifically, not all allocation
    // everywhere). bpm <= 0 defensively treated as 120.0, same guard as
    // MidiFileWriter.
    //
    // Deterministic: same row + bpm + sampleRate always produces
    // byte-identical output (unit-testable, see Validation Architecture).
    juce::AudioBuffer<float> render (const MidiSetRow& row, double bpm, double sampleRate);
}
```
```cpp
// Source/PluginProcessor.h (added members)
public:
    // Message-thread only. Renders row on the calling thread (cheap -- a few
    // seconds of audio, additive sine synthesis, sub-millisecond in practice)
    // and hands it to the audio thread. Stops any currently-playing audition.
    void startAudition (const MidiSetRow& row);
    void stopAudition();
    bool isAuditionPlaying() const { return auditionPlaying.load (std::memory_order_relaxed); }

private:
    // Double-buffer handoff, audio-thread-safe WITHOUT the shared_ptr
    // atomic_load/atomic_store idiom used elsewhere in this file -- see
    // 06-RESEARCH.md Pitfall 2 for why that idiom is unsafe here specifically.
    juce::AudioBuffer<float> auditionBuffers[2]; // resized on message thread only
    std::atomic<int>  auditionActiveIndex  { 0 };
    std::atomic<int>  auditionActiveLength { 0 }; // valid sample count in the active buffer
    std::atomic<int>  auditionReadPos      { 0 };
    std::atomic<bool> auditionPlaying      { false };
```
```cpp
// Source/PluginProcessor.cpp
void ChordAIAudioProcessor::startAudition (const MidiSetRow& row)
{
    auto result = getAnalysisResult();
    const double bpm = (result != nullptr && result->bpm > 0.0) ? result->bpm : 120.0;

    const int inactive = 1 - auditionActiveIndex.load (std::memory_order_relaxed);
    auditionBuffers[inactive] = AuditionRenderer::render (row, bpm, getSampleRate());
        // getSampleRate() is safe to call on the message thread here: by the time
        // a user can click Play, prepareToPlay has already run at least once.

    auditionReadPos.store (0, std::memory_order_relaxed);
    auditionActiveLength.store (auditionBuffers[inactive].getNumSamples(), std::memory_order_relaxed);
    auditionActiveIndex.store (inactive, std::memory_order_release);  // publish buffer swap...
    auditionPlaying.store (true, std::memory_order_release);          // ...then arm playback
}

void ChordAIAudioProcessor::stopAudition()
{
    auditionPlaying.store (false, std::memory_order_release);
}

void ChordAIAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    for (int ch = getTotalNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auditionPlaying.load (std::memory_order_acquire))
    {
        const int idx = auditionActiveIndex.load (std::memory_order_acquire);
        const int len = auditionActiveLength.load (std::memory_order_relaxed);
        const int pos = auditionReadPos.load (std::memory_order_relaxed);
        auto& src = auditionBuffers[idx]; // audio thread NEVER resizes/writes this --
                                           // message thread only ever touches the OTHER index

        const int toCopy = juce::jmin (len - pos, buffer.getNumSamples());
        if (toCopy > 0)
        {
            const int srcChannels = src.getNumChannels();
            for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch)
                buffer.addFrom (ch, 0, src, juce::jmin (ch, srcChannels - 1), pos, toCopy);
        }

        const int newPos = pos + toCopy;
        auditionReadPos.store (newPos, std::memory_order_relaxed);
        if (newPos >= len)
            auditionPlaying.store (false, std::memory_order_release); // auto-stop at end
    }
}
```
This is option (b) from the phase brief (pre-rendered buffer, atomic-index swap), deliberately **not** using a `shared_ptr` for the handoff — see Pitfall 2. `buffer.addFrom` mixes into whatever the host/Standalone is already passing through, matching this plugin's existing audio-effect (not synth/instrument) declaration (`IS_SYNTH FALSE` in `CMakeLists.txt`) — the plugin adds its own generated preview audio on top of the pass-through signal rather than replacing it, the same mechanism used by click-track/metronome-style JUCE plugins.

### Anti-Patterns to Avoid

- **Deleting the drag temp file in `performExternalDragDropOfFiles`'s completion callback.** Verified root cause of the exact "works everywhere except Ableton" failure mode `PITFALLS.md` flags as CRITICAL — see Pitfall 1.
- **Calling `std::atomic_load(&someSharedPtr)` from `processBlock`.** Safe everywhere else in this codebase (message-thread-to-message-thread only); unsafe here — see Pitfall 2.
- **Re-deriving MIDI tick positions from `NoteEvent.startBeats`/`lengthBeats` using seconds/BPM anywhere except the one `MidiFileWriter`/`AuditionRenderer` conversion points.** `NoteEvent`'s whole design point (per `05-RESEARCH.md`) is that beat-domain values are bar-aligned "for free" — don't reintroduce a seconds round-trip that could reintroduce drift.
- **Building a second, separate MIDI-writing code path for "save to disk" vs "drag out."** Both must call the same `MidiFileWriter::writeToFile`/`buildMidiFile` — divergence here is exactly how EXP-03's "carries the detected tempo" guarantee could silently break in one path and not the other.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Standard MIDI File byte format (header chunk, variable-length quantities, track chunks) | A custom `.mid` binary writer | `juce::MidiFile` / `juce::MidiMessageSequence` | Already linked, spec-correct, and the exact API `05-RESEARCH.md` already scoped this phase around — verified again here against the vendored source. |
| Note-on/off byte packing, running-status | Manual MIDI status-byte construction | `juce::MidiMessage::noteOn/noteOff` | Solved, official API; channel 1-16, velocity accepts `float` 0-1 (matches `NoteEvent.velocity`'s type exactly) or `uint8` 0-127. |
| Click-free amplitude envelopes (attack/decay/sustain/release ramp math, avoiding discontinuities at stage boundaries) | Hand-rolled linear/exponential ramp state machine | `juce::ADSR` | Already linked, already unit-tested by JUCE itself (`juce_ADSR_test.cpp` ships in the vendored tree), covers edge cases (zero-length stages, mid-envelope retrigger) that are easy to get subtly wrong by hand and would show up as audible clicks. |
| Async native save dialog (macOS `NSSavePanel` wrapping, sandboxed vs non-sandboxed paths, callback lifetime) | A custom file-save UI or raw platform API calls | `juce::FileChooser::launchAsync` | Already linked; handles the native macOS save panel, and this project has no `APP_SANDBOX` entitlement complicating it (verified — see Standard Stack table). |
| Atomic replace of an existing file on save (avoiding a half-written `.mid` if the app crashes mid-write) | Manual write-then-rename | `juce::TemporaryFile` (`overwriteTargetFileWithTemporary()`) | Already linked, already the pattern an independent JUCE forum report specifically recommended after hitting the append-vs-overwrite `FileOutputStream` gotcha (see Pitfall 4). |

**Key insight:** This phase's only genuinely new logic is (1) the tiny row→ticks conversion (a few lines, already sketched and verified in Phase 5's own research) and (2) the tiny additive-sine audition tone (a few lines, matches this codebase's existing test-fixture tone-generation style). Everything structurally interesting — SMF format, envelope shaping, native dialogs, atomic file replace, OS-level drag — is an already-linked JUCE API call, not new code to design.

## Common Pitfalls

### Pitfall 1: Ableton "could not be opened" — deleting the drag temp file too early

**What goes wrong:** Drag-and-drop of the temp `.mid` works reliably in Logic Pro, FL Studio, Studio One, Reaper, Pro Tools, and GarageBand, but Ableton shows an error dialog ("could not be opened") every time, with no crash.

**Why it happens:** A `performExternalDragDropOfFiles` completion callback that deletes the temp file (`[=]{ tempFile.deleteFile(); }`) runs before Ableton has finished asynchronously reading the file's bytes off disk — independently reported and root-caused on the JUCE forum. `PITFALLS.md`'s existing "Ableton crash after specific UI sequences" entry documents a related-but-distinct symptom (an actual crash from a different reported thread); this is the more common, non-crashing failure mode and has a confirmed, simple fix.

**How to avoid:** Do not pass a cleanup callback to `performExternalDragDropOfFiles` at all (pass `nullptr`, as in Pattern 2 above). Instead, clean up the *previous* drag's temp file at the **start** of the next drag (before writing the new one), and do a best-effort sweep of the whole `tempDirectory/ChordAI/` subfolder on `PluginProcessor` construction/destruction. Leaving a handful of small `.mid` files in the OS temp directory between drags is harmless — the OS temp dir is self-cleaning and each file is a few hundred bytes.

**Warning signs:** Drag works in every DAW during dev testing except Ableton specifically; Ableton shows a file-read error rather than crashing or silently doing nothing.

**Sources:** [forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816](https://forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816), [forum.juce.com/t/performexternaldragdropoffiles-not-working-in-ableton/42675](https://forum.juce.com/t/performexternaldragdropoffiles-not-working-in-ableton/42675) — both independently report the same root cause and fix. MEDIUM-HIGH confidence (community forum, but two independent threads agree, and the mechanism — async host-side file read racing a synchronous local delete — is mechanically plausible and consistent with how OS-level drag-and-drop is implemented).

---

### Pitfall 2: Reusing this codebase's `shared_ptr` atomic-publication idiom on the audio thread (NEW — not previously documented)

**What goes wrong:** `PluginProcessor.h` already has an established, working pattern — `std::atomic_load`/`std::atomic_store` on `shared_ptr<const T>` members (`loadedAudio`, `analysisResult`, `midiSetRows`) — explicitly chosen over `std::atomic<shared_ptr<T>>` because the latter "is incomplete on Apple libc++" (per the header's own comment). It would be natural to reach for the *same* idiom to hand a rendered preview buffer to `processBlock`. Doing so reintroduces a real-time-safety violation: the free-function `std::atomic_load`/`atomic_store` overloads for `shared_ptr` are not guaranteed lock-free by the standard, and on the platforms/standard-library versions relevant here they are commonly implemented with an internal mutex/spinlock guarding the control block's refcount — exactly the "even `try_lock()` is unsafe on the audio thread" class of problem `PITFALLS.md` Pitfall 6 already warns about, just via a shared_ptr instead of a visible `std::mutex`.

**Why it happens:** Every existing use of that idiom in this codebase is message-thread-to-message-thread (a background job thread publishes, the UI/editor reads on the message thread) — it has never been read from `processBlock` before, so the existing pattern "looks proven" and safe to extend by analogy. Phase 6 is the first time anything needs to reach a rendered audio buffer *into* the actual real-time audio callback.

**How to avoid:** Use plain preallocated `juce::AudioBuffer<float>` slots (not held by `shared_ptr`) plus plain `std::atomic<int>`/`std::atomic<bool>` index/length/position variables for the handoff, exactly as shown in Pattern 3. These are genuinely lock-free on every mainstream platform (`std::atomic<int>::is_lock_free()` is true everywhere relevant here) and the audio thread never touches the buffer object's lifetime (no copy, no refcount, no allocation) — only reads raw sample data at a bounds-checked offset.

**Warning signs:** Any `std::atomic_load`/`atomic_store`/`std::shared_ptr` construction or destruction reachable from inside `processBlock`; `pluginval` strict-mode real-time-safety checks (or a `grep processBlock` review, per this project's own existing Pitfall 6 practice) flagging it.

**Phase to address:** This phase, at the design stage of the audition feature — before any code exists, since retrofitting a lock-free handoff after the shared_ptr version is already wired through `MidiRowView`/`PluginProcessor` would touch more call sites than doing it right the first time.

---

### Pitfall 3: Row regeneration destroys `MidiRowView` instances mid-audition

**What goes wrong:** `MidiSetsPanel::setRows()` (already shipping, Phase 5) unconditionally does `rowViews.clear()` then rebuilds every `MidiRowView` from scratch on every call — including every GEN-04 region-change regeneration. If a row is mid-audition (playing) when the user drags the region selector, the `MidiRowView` the user clicked Play on gets destroyed while `PluginProcessor` may still be mid-playback of that row's rendered buffer.

**Why it happens:** This is existing, already-shipped Phase 5 behavior (not a bug to fix in Phase 5's scope, since Phase 5 had no interactive row state) — Phase 6 is the first phase to add row-local interactive state (`dragStarted`, and implicitly "am I the row currently playing"), and the natural place to store "is my play button showing stop" is on the `MidiRowView` instance itself, which is exactly what gets thrown away on regeneration.

**How to avoid:** Store *which row id is playing* (`juce::String`, matching `MidiSetRow.id`) on `PluginProcessor` (or `MidiSetsPanel`), not on `MidiRowView`. `MidiRowView::paint`/hit-test reads "is my row id the currently-playing one" each repaint rather than owning local boolean state. Additionally, call `stopAudition()` explicitly whenever `MidiSetsPanel::setRows()` is about to discard the currently-playing row's view (or, simpler and always-correct: call `stopAudition()` unconditionally at the top of every `setRows()` call — a regenerate always invalidates whatever was playing, since the old `MidiSetRow` object it referenced no longer exists).

**Warning signs:** Audition audio keeps playing (or the processor's `auditionBuffers` still points at now-stale data) after a region change; a play/stop icon shows the wrong state after regeneration; use-after-free-flavored crashes if any raw `MidiRowView*` is ever cached elsewhere across a `setRows()` call (nothing in the current codebase does this, but it's the shape of bug this pattern invites).

**Phase to address:** This phase — `stopAudition()`-on-`setRows()` is a one-line addition, cheap to get right now versus discovering it as a live-audio bug during the human checkpoint.

---

### Pitfall 4: `FileOutputStream` append-by-default silently corrupts a re-saved file

**What goes wrong:** Writing a `.mid` file a second time to the same path (e.g., user clicks "Save" on the same row twice, or the temp-file path is reused) can append new SMF bytes after old ones rather than replacing the file, because `juce::FileOutputStream`'s default behavior is to open for appending, not truncate-on-open.

**Why it happens:** This is a genuinely non-obvious JUCE default, independently flagged in the same forum thread that root-caused Pitfall 1 — "the FileOutputStream appends by default rather than overwriting."

**How to avoid:** Use `juce::TemporaryFile` (write to a fresh temp path, then `overwriteTargetFileWithTemporary()` does an atomic rename/replace) for the save-to-disk path (Pattern 1's `writeToFile` already does this), or explicitly delete/truncate the destination file before opening a `FileOutputStream` for the drag-temp-file path (which doesn't need atomic-replace semantics — it's always a fresh uniquely-named file per Pitfall 1's cleanup strategy, so this only matters if a fixed temp filename is reused, which this research recommends against anyway).

**Warning signs:** A `.mid` file that grows in size across repeated saves of the same row; a DAW reporting a corrupt/unreadable MIDI file after a second save to the same path.

**Sources:** [forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816](https://forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816). MEDIUM confidence (single forum source, but the underlying `FileOutputStream` append-by-default behavior is a documented JUCE characteristic, not a one-off report).

---

### Pitfall 5: Zero/negative-length or same-tick note-off/note-on pairs get silently dropped by some hosts

**What goes wrong:** A `NoteEvent` with `lengthBeats <= 0.0` (shouldn't occur given Phase 5's generators, but nothing currently asserts it at the `MidiFileWriter` boundary) converts to a note-on and note-off at the identical tick — some DAWs' MIDI importers silently drop zero-length notes rather than erroring, so the row's note count in the DAW quietly doesn't match what was previewed/visible in `MidiRowView`.

**How to avoid:** Guard explicitly in `MidiFileWriter::toNoteSequence` (shown in Pattern 1) — skip any note with `lengthBeats <= 0.0` rather than writing a degenerate event. This is defensive (Phase 5's generators are not expected to produce these), but it's a boundary this phase owns and should not trust silently.

**Phase to address:** This phase, inside `MidiFileWriter` itself — cheap, and makes EXP-03's round-trip test meaningfully assert "every visible note in `MidiRowView` round-trips," not just "the writer doesn't crash on well-formed input."

## Code Examples

### File naming convention (EXP-01/EXP-02, "MIDI-pack style" per the phase brief)

```cpp
// Source/MidiGen/MidiFileWriter.h (or a small ExportNaming.h companion)
namespace MidiFileWriter
{
    // e.g. "pop-trap_Am_128bpm.mid" -- row.id is already the stable,
    // filesystem-safe key ("as-is"/"pop-trap"/"rnb-neosoul"/"house"/"bass"),
    // keyLabel reuses the same 12-note-name convention as
    // Source/UI/ChordNameFormatter.h's chordName() (do not invent a second
    // note-name table -- extract ChordNameFormatter's kNoteNames to a shared
    // location, or duplicate the 12-entry array verbatim; either is fine,
    // planner's call).
    juce::String suggestedFileName (const MidiSetRow& row, const KeyResult& key, double bpm)
    {
        const auto keyLabel = juce::String (kNoteNames[key.tonicPitchClass]) + (key.isMajor ? "" : "m");
        const int bpmInt = juce::roundToInt (bpm > 0.0 ? bpm : 120.0);
        return row.id + "_" + keyLabel + "_" + juce::String (bpmInt) + "bpm.mid";
    }
}
```

### Save-to-disk flow (EXP-02) — async `FileChooser`, remembers last-used directory

```cpp
// Source/UI/MidiRowView.cpp (save-icon click handler, or a small MidiRowView::saveRow() method)
void MidiRowView::saveRow()
{
    static juce::File lastUsedDirectory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                               .getChildFile ("ChordAI MIDI");
        // v1 default per phase brief: ~/Documents/ChordAI MIDI/, remembers last-used dir for
        // the session (a static local is a pragmatic v1 choice -- promote to a persisted
        // apvts-backed setting only if a later phase needs it to survive across sessions).

    const double bpm = getBpmForExport ? getBpmForExport() : 120.0;
    const auto suggestedName = MidiFileWriter::suggestedFileName (row, /* KeyResult */ {}, bpm);

    lastUsedDirectory.createDirectory();
    auto initial = lastUsedDirectory.getChildFile (suggestedName);

    // fileChooser MUST outlive the async callback -- store as a member
    // (std::unique_ptr<juce::FileChooser>), not a local, or it's destroyed
    // before launchAsync's callback fires.
    fileChooser = std::make_unique<juce::FileChooser> ("Save MIDI row", initial, "*.mid");
    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, bpm] (const juce::FileChooser& fc)
        {
            auto chosen = fc.getResult();
            if (chosen == juce::File{})
                return; // user cancelled

            lastUsedDirectory = chosen.getParentDirectory();
            MidiFileWriter::writeToFile (row, bpm, chosen);
        });
}
```
`FileChooser::launchAsync` signature confirmed at `external/JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooser.h` line 216. The must-outlive-the-callback requirement is called out explicitly in the header's own doc comment ("You must ensure that the lifetime of the callback object is longer than...").

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|---------------|--------|
| Manual SMF byte-writer tutorials (common in older MIDI-file blog posts/Stack Overflow answers) | `juce::MidiFile`/`MidiMessageSequence` | N/A — JUCE has provided this since early versions | No SMF spec knowledge needed; already the plan per `05-RESEARCH.md`. |
| Deleting the drag temp file in the `performExternalDragDropOfFiles` completion callback (the first thing most JUCE devs try, per multiple forum threads) | Defer cleanup to the start of the next drag / app shutdown | Established via community trial-and-error, not a JUCE version change | Directly resolves the Ableton-specific failure this project's own `PITFALLS.md` flags as CRITICAL — changes this from "known-fragile, needs a spike" to "known bug class, known fix, plan around it directly." |

No JUCE-version-driven deprecations apply here — `juce::MidiFile`, `DragAndDropContainer`, `FileChooser`, and `ADSR` are all stable, unchanged APIs across the JUCE 7→8 line relevant to this project.

## Open Questions

1. **Exact hit-zone layout inside `MidiRowView`'s existing 92px gutter (play icon + save icon + label) is a UI-design detail, not resolved by this research.**
   - What we know: The gutter currently holds only the accent bar + label text (`MidiRowView.cpp`'s `kGutterWidth = 92`). Two new interactive zones (play/stop, save) need to fit without crowding the label or conflicting with "drag anywhere on the row" for export.
   - What's unclear: Exact pixel subdivision, icon glyphs (pixel-art aesthetic per `02-CONTEXT.md`'s locked palette direction), and whether play/save live in the gutter or as a small overlay on the note area's left edge.
   - Recommendation: Treat as a planning/implementation detail, not a research gap — reuse the existing gutter's accent-bar-plus-text layout conventions (`MidiRowView.cpp`'s `accentForStyle`/pixel-rect drawing style already established) and confirm the icon-vs-drag hit-test split doesn't fight `mouseWasDraggedSinceMouseDown()`'s gesture detection (Pattern 2 already handles the click-vs-drag split at the `mouseDown`/`mouseDrag`/`mouseUp` level; icon hit-testing is a `mouseUp`-time check within the "wasn't a drag" branch).

2. **Whether `Source/Audio/` is the right new folder, or whether `AuditionRenderer`/`AuditionVoice` should live under `Source/MidiGen/` instead.**
   - What we know: `MidiGen/NoteEvent.h` explicitly documents a "folder-boundary rule" — `Source/MidiGen/` never includes anything under `Source/Analysis/` beyond `AnalysisResult.h`, and is described as having "no JUCE dependency at all" for its pure value types (`NoteEvent`, `MidiSetRow`). `AuditionRenderer`/`AuditionVoice` fundamentally need `juce::AudioBuffer`/`juce::ADSR` (real JUCE audio types), which doesn't fit that "no JUCE dependency" framing even though they're conceptually adjacent to `MidiGen`.
   - What's unclear: Whether the planner should introduce a new `Source/Audio/` folder (this research's suggested structure) or relax/reinterpret the existing folder-boundary comment to allow JUCE audio types specifically (as opposed to `Analysis/` cross-includes, which is what that rule was actually guarding against).
   - Recommendation: New `Source/Audio/` folder (as sketched above) — keeps `MidiGen/`'s existing "pure value types, no JUCE audio dependency" character intact for `MidiFileWriter` too (it only needs `juce_audio_basics`' MIDI types, not audio-buffer/DSP types, so it can plausibly stay in `MidiGen/` if the planner prefers; `AuditionRenderer`/`AuditionVoice` are the ones that clearly want a new home).

3. **"Export all rows" folder option, mentioned as a maybe in the phase brief, is out of v1 scope per the brief's own guidance ("Keep v1: per-row save button + remember last dir").**
   - What we know: The phase brief explicitly resolves this itself — v1 ships per-row save only.
   - Recommendation: Not an open question for planning — noted here only so the planner doesn't accidentally re-litigate it. If wanted later, it's an additive feature (a "Save All" button calling `MidiFileWriter::writeToFile` in a loop over all 5 rows into one chosen folder) with no architecture change required.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 (already pinned via `FetchContent` in `CMakeLists.txt`), run through the `ChordAITests` console-app CTest target |
| Config file | `CMakeLists.txt` (`juce_add_console_app(ChordAITests ...)` + `catch_discover_tests`) — no separate test-framework config file |
| Quick run command | `cmake --build build --target ChordAITests && ./build/ChordAITests_artefacts/ChordAITests "[midifilewriter],[auditionrenderer]"` (tag-filtered, matches this codebase's existing per-file `[tagname]` convention, e.g. `[midisetspanellayout]` in `MidiSetsPanelLayoutTests.cpp`) |
| Full suite command | `ctest --test-dir build` (existing project convention, confirmed by `STATE.md`'s "109/109" full-suite gate references) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| EXP-03 | Written `.mid` round-trips: ticks, tempo meta, note pitch/start/length match the source `MidiSetRow` within one tick's rounding error | unit | `ChordAITests "[midifilewriter]"` | ❌ Wave 0 — `Tests/MidiFileWriterTests.cpp` |
| EXP-03 | Tempo meta-event's `microsecondsPerQuarterNote` matches `AnalysisResult.bpm` (and the `bpm <= 0` fallback triggers correctly) | unit | `ChordAITests "[midifilewriter]"` | ❌ Wave 0 — same file |
| EXP-02 / EXP-01 | `suggestedFileName` produces a filesystem-safe, uniquely-identifiable name per row/key/bpm combination | unit | `ChordAITests "[midifilewriter]"` | ❌ Wave 0 — same file |
| PRV-01 | `AuditionRenderer::render` is deterministic (same row+bpm+sampleRate → byte-identical buffer) and produces a finite (no NaN/Inf), non-clipping, correctly-sized buffer | unit | `ChordAITests "[auditionrenderer]"` | ❌ Wave 0 — `Tests/AuditionRendererTests.cpp` |
| PRV-01 | Empty row (`row.notes.empty()`) renders a valid zero-or-near-zero-length buffer, not a crash | unit | `ChordAITests "[auditionrenderer]"` | ❌ Wave 0 — same file |
| PRV-01 | `processBlock`'s audition read path never allocates/locks (code-review + `pluginval` strict-mode gate, not a Catch2 test) | manual + tooling | `pluginval --strictness-level 5 ...` (existing project gate, per `STATE.md`'s established Release-build checklist) | n/a (existing tool, already in the project's gate) |
| EXP-01 | Drag produces a real, non-empty, valid `.mid` file at the expected temp path before `performExternalDragDropOfFiles` is invoked | unit | `ChordAITests "[midifilewriter]"` (write path only — the actual OS-level drag itself is not unit-testable) | ❌ Wave 0 — same file as EXP-03's writer tests |
| EXP-01 | Drag into DAW piano roll actually works, with correct notes/tempo/key | manual | Manual: drag each of the 5 rows into FL Studio's channel rack/piano roll (user's DAW per `STATE.md`), verify notes land correctly and tempo matches; justification: OS-level native drag-and-drop cannot be automated/unit-tested | n/a |
| PLT-03 (Phase 7, referenced here since EXP-01 is its prerequisite) | Same drag verified in Ableton Live and Logic Pro | manual | Manual, out of this phase's own gate but should be spot-checked here if convenient — full matrix is Phase 7's job per `REQUIREMENTS.md` traceability | n/a |

### Sampling Rate
- **Per task commit:** `ChordAITests "[midifilewriter],[auditionrenderer]"` (targeted tags, < 5s)
- **Per wave merge:** `ctest --test-dir build` (full suite)
- **Phase gate:** Full suite green (extends the existing 109/109 baseline) + fresh Release `pluginval` strictness-5 pass (VST3+AU) + the manual FL Studio drag-and-drop checkpoint, before `/gsd:verify-work` — matches this project's own established Phase 4/5 closing-plan pattern (Release build + full suite + pluginval + human checkpoint).

### Wave 0 Gaps
- [ ] `Tests/MidiFileWriterTests.cpp` — covers EXP-01/EXP-02/EXP-03 (round-trip ticks/tempo/notes, zero-length-note guard, file naming)
- [ ] `Tests/AuditionRendererTests.cpp` — covers PRV-01 (determinism, no-NaN/no-clip, empty-row safety, correct sample count for a known row+bpm)
- [ ] Both new `.cpp`/`.h` pairs (`Source/MidiGen/MidiFileWriter.*`, `Source/Audio/AuditionRenderer.*` + `AuditionVoice.*`) added to `CHORDAI_MIDIGEN_SOURCES`/a new source list and both `target_sources(ChordAI ...)` / `target_sources(ChordAITests ...)` blocks in `CMakeLists.txt` — one-time CMake wiring task, same shape as every prior phase's Wave 1 foundation plan (e.g. 05-01, 03-01)
- [ ] No new test-framework install needed — Catch2/CTest infrastructure fully covers this phase's automatable surface already

## Sources

### Primary (HIGH confidence)
- `external/JUCE/modules/juce_audio_basics/midi/juce_MidiFile.h` (vendored, JUCE 8.0.14) — `setTicksPerQuarterNote`, `addTrack`, `writeTo`, `getLastTimestamp`
- `external/JUCE/modules/juce_audio_basics/midi/juce_MidiMessage.h` (vendored) — `noteOn`/`noteOff` overloads (lines 260-304), `tempoMetaEvent`/`timeSignatureMetaEvent`/`endOfTrack` (lines 586-647)
- `external/JUCE/modules/juce_audio_basics/midi/juce_MidiMessageSequence.h` (vendored) — `addEvent`, `updateMatchedPairs`, `sort` semantics (lines 140-241)
- `external/JUCE/modules/juce_gui_basics/mouse/juce_DragAndDropContainer.h` (vendored) — `performExternalDragDropOfFiles` is `static` (line 204)
- `external/JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooser.h` (vendored) — `launchAsync` signature (line 216), constructor doc
- `external/JUCE/modules/juce_gui_basics/filebrowser/juce_FileBrowserComponent.h` (vendored) — `FileChooserFlags::saveMode`/`canSelectFiles`/`warnAboutOverwriting`
- `external/JUCE/modules/juce_audio_basics/utilities/juce_ADSR.h` + `juce_ADSR_test.cpp` (vendored) — `ADSR::Parameters`, existing JUCE-shipped unit test coverage
- `external/JUCE/modules/juce_core/files/juce_File.h` (vendored) — `SpecialLocationType::tempDirectory` (line 937)
- `external/JUCE/modules/juce_gui_basics/mouse/juce_MouseEvent.h` (vendored) — `mouseWasDraggedSinceMouseDown`/`getDistanceFromDragStart`
- This repo's own `.planning/phases/05-midi-conveyor-generation/05-RESEARCH.md` — the original `toMidiSequence` conversion sketch this research extends and re-verifies
- This repo's own `Source/PluginProcessor.h`, `PluginProcessor.cpp`, `Source/UI/MidiRowView.h/.cpp`, `Source/UI/MidiSetsPanel.h/.cpp`, `Source/UI/RegionSelectorOverlay.h/.cpp`, `Source/MidiGen/MidiSetRow.h`, `NoteEvent.h`, `Source/Analysis/AnalysisResult.h`, `CMakeLists.txt` — current-state grounding for every recommendation above

### Secondary (MEDIUM confidence)
- [forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816](https://forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816) — temp-file drag pattern, `TemporaryFile`/append-mode gotcha, DAW compatibility list (Ableton/Logic/FL Studio/Reaper/Pro Tools/GarageBand confirmed working; Bitwig flagged as having issues)
- [forum.juce.com/t/performexternaldragdropoffiles-not-working-in-ableton/42675](https://forum.juce.com/t/performexternaldragdropoffiles-not-working-in-ableton/42675) — independent confirmation of the delete-in-callback root cause and fix
- Songscription AI blog ("How to Import MIDI Into a DAW") — general FL Studio/Ableton/Logic MIDI-file drag/import UX description, used only for high-level DAW-behavior framing, not for any JUCE-API decision

### Tertiary (LOW confidence)
- None used as the basis for any specific recommendation in this document — every WebSearch finding above was cross-verified against either the vendored JUCE source directly or at least two independent forum threads before being stated as fact.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — every JUCE API cited was read directly from the vendored `external/JUCE` source tree in this repo, not recalled from training data.
- Architecture (MIDI writing): HIGH — extends an already-verified sketch from this project's own `05-RESEARCH.md`, re-confirmed here.
- Architecture (audition RT-safety): HIGH for the general lock-free-handoff technique (standard, well-established real-time-audio pattern); MEDIUM for the specific tone/envelope design choices (product/sound-design judgment, not a verifiable fact — flagged accordingly, same treatment `05-RESEARCH.md` gave its own voicing-register numbers).
- Pitfalls (drag-and-drop): MEDIUM-HIGH — two independent forum threads agree on the Ableton root cause and fix; not an official JUCE guarantee, but a mechanically plausible, cross-confirmed community finding that directly resolves a CRITICAL item in this project's own `PITFALLS.md`.
- Pitfalls (shared_ptr-on-audio-thread): HIGH — grounded in this project's own already-established, correctly-reasoned Pitfall 6 (`PITFALLS.md`) extended to a case that specific document didn't yet cover; the underlying real-time-audio-safety principle (no non-lock-free atomic operations on the audio thread) is not in dispute.

**Research date:** 2026-07-13
**Valid until:** JUCE API portions (~90 days, stable APIs unlikely to change); DAW-specific drag-and-drop behavior and forum-sourced pitfalls (~30 days — re-verify against the actual FL Studio/Ableton/Logic installs during this phase's manual checkpoint rather than trusting forum reports alone, since host-side behavior can change with DAW updates).

---
*Research for: Phase 6 - Row Preview & Export (ChordAI)*
*Researched: 2026-07-13*
