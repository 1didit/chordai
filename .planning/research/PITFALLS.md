# Pitfalls Research

**Domain:** JUCE-based chord-detection / MIDI-generation audio plugin (VST3/AU/Standalone, macOS, commercial)
**Researched:** 2026-07-12
**Confidence:** HIGH (JUCE/DAW mechanics and licensing verified against official docs, GitHub, forum threads; MIR/DSP failure modes verified against academic literature and library docs)

## Critical Pitfalls

### Pitfall 1: GPL/AGPL contamination from off-the-shelf MIR libraries

**What goes wrong:**
Team links aubio, Essentia, or madmom's pretrained models into the plugin to get chord/beat/key detection "for free," then discovers the binary cannot legally be sold closed-source.

**Why it happens:**
These are the most-cited MIR libraries in tutorials and papers, so they look like the fastest path to a working detector. Their licenses are easy to overlook because the C++ API itself looks like any other library.

- **aubio** — GPLv3. No commercial exception offered. Any binary statically or dynamically linking aubio must itself be GPL (source-available) if distributed — incompatible with a closed-source commercial VST3/AU. [aubio.org](https://aubio.org/) / [COPYING](https://github.com/aubio/aubio/blob/master/COPYING)
- **Essentia** — dual-licensed: AGPLv3 for open-source/non-commercial use, or a paid commercial license negotiated directly with UPF's Music Technology Group. Using it commercially without buying that license is the same problem as aubio, and AGPL is even stricter (network-use clause). [essentia.upf.edu/licensing_information.html](https://essentia.upf.edu/licensing_information.html)
- **madmom** — source code is BSD (fine), but its pretrained neural-net models (beat/downbeat/chord DBNs) are CC-BY-NC-SA — non-commercial only. Shipping the code without the models is legal; shipping the models in a commercial product is not. [github.com/CPJKU/madmom](https://github.com/CPJKU/madmom)

**How to avoid:**
- Build the chord/beat/key pipeline on permissively-licensed building blocks: JUCE's own DSP module (FFT, `dsp::FFT`, filters) for chromagram extraction, and write the template-matching + Viterbi/HMM smoothing in-house (this is a well-documented, implementable algorithm — not proprietary tech).
- If a third-party DSP library is used for anything (onset detection, resampling, pitch), audit its license file directly (not just the README) before adding the dependency. MIT/BSD/Apache-2.0/zlib only.
- Treat "trained model weights" as a separate license question from "source code license" — a BSD-licensed repo can still ship non-commercial-only weights.
- Document every third-party dependency's license in one place (e.g., `THIRD_PARTY_LICENSES.md`) from the first dependency added, not retroactively before release.

**Warning signs:**
- Any `#include` from aubio, Essentia, madmom, librosa (Python, not even usable in C++ directly), or any repo whose LICENSE file says GPL/AGPL without a "commercial license available" carve-out you've actually purchased.
- A dependency that ships `.h5`/`.onnx`/`.pkl` pretrained weights with no explicit commercial-use grant in its license file.

**Phase to address:**
Stack/dependency selection, before any DSP code is written (earliest phase — this is a legal gate, not a refactor-later problem).

---

### Pitfall 2: Chromagram computed on the full mix without accounting for percussion, bass leakage, and tuning drift

**What goes wrong:**
Chord detector produces noisy, rapidly flickering, or systematically wrong chord labels because the 12-bin chroma vector is built directly from a full-mix FFT that includes drums (broadband noise), sub-bass, and vocal formants, and assumes A440 tuning.

**Why it happens:**
The naive pipeline (STFT → sum magnitude into 12 pitch classes → template match) is the "hello world" of chord recognition and works fine on synthesized MIDI or isolated guitar/piano test files. It falls apart on real mixed masters because:
- Drums/percussion contribute broadband energy to every chroma bin, flattening the vector and reducing template-matching confidence.
- Basslines and sub-bass energy below ~80Hz smear into low chroma bins if the analysis range isn't restricted, biasing root detection.
- Guitar-heavy pop/rock tracks are frequently tuned slightly off A440 (drop tunings, tape-era pitch drift, deliberate flat tuning) — an un-corrected chroma binning smears energy across adjacent bins and directly degrades template match accuracy.

**How to avoid:**
- Use log-frequency / Constant-Q–based chroma (not raw linear-FFT binning) so pitch classes align with musical semitones regardless of octave.
- Add an explicit tuning-estimation pass (find the dominant tuning offset from concert pitch across the analysis window) and rotate/re-bin chroma accordingly before template matching.
- Apply spectral whitening or harmonic/percussive source separation (HPSS) — even a cheap median-filtering HPSS — to suppress drum transients before chroma extraction; do not attempt full stem separation (explicitly out of scope per PROJECT.md, and unnecessary for this).
- Restrict/attenuate very low frequency bins in the main chroma (used for chord quality) and compute a **separate bass chromagram** over ~55–250Hz specifically for root/inversion detection (see Pitfall 3).
- Normalize chroma energy per-frame (loudness-invariant) so quiet intros/bridges don't get systematically misclassified.

**Warning signs:**
- Chord labels change on every analysis frame/beat even during a held chord in the reference audio.
- Detector consistently favors chords a semitone off from the correct one on tracks known to be tuned flat/sharp.
- Accuracy is noticeably worse on tracks with prominent live drums/percussion than on programmed/electronic tracks.

**Phase to address:**
Core chord-detection engine phase (chromagram extraction step), before Viterbi/segmentation work begins — get the input features right first, since smoothing cannot fix a systematically wrong feature vector.

---

### Pitfall 3: Bass note register causes wrong root/inversion detection

**What goes wrong:**
Chromagram is octave-agnostic by design (it folds all octaves of a pitch class into one bin), so the detector cannot tell a root-position chord from an inversion, and can pick the wrong root entirely when the bass note is weak in the full-chroma vector relative to mid-range harmony instruments.

**Why it happens:**
Standard 12-bin chroma discards octave information, which is precisely what makes chord templates robust to voicing — but it also means the algorithm has no direct signal for "what note is in the bass," which is often what most strongly signals the intended root, especially over slash chords and inversions common in pop/R&B.

**How to avoid:**
- Compute a dedicated low-frequency (bass-range, ~55–250Hz) chromagram in parallel with the main harmonic chromagram, and use the bass chroma to bias/verify root selection in the template-matching or Viterbi decoding step — not just the full-mix chroma.
- Decide explicitly (as a product/UX decision, not an accident) whether v1 chord output includes inversions/slash chords or normalizes everything to root position — and communicate that decision to the accuracy expectations set for users (ties to Pitfall 9).

**Warning signs:**
- Detector frequently reports the relative chord a third away from ground truth on tracks with prominent walking bass or inversions (e.g., reports Am when the actual harmony is F/A).
- Root note in exported MIDI bass line doesn't match what a musician hears as the bass note.

**Phase to address:**
Core chord-detection engine phase, same feature-extraction step as Pitfall 2. Also directly relevant to the bass-line generation feature (Active requirement: "стильові варіації + бас-лінія").

---

### Pitfall 4: Chord segmentation without a beat/bar grid produces chattering, musically meaningless output

**What goes wrong:**
Running chord classification independently on every short analysis frame (e.g., every ~100ms) and emitting a chord label per frame yields dozens of spurious chord changes per bar — transient noise, passing tones, and vibrato get misread as chord changes.

**Why it happens:**
Frame-by-frame classification without temporal context is the simplest thing to implement first, and it looks correct in isolation (each individual frame's guess may be locally plausible) — the failure only shows up when you look at the full sequence and see it flickering between 3-4 different chords within a single held chord.

**How to avoid:**
- Beat-synchronize the chromagram: estimate beat positions first (with octave-error correction, see Pitfall 5), then aggregate (median or mean) chroma between consecutive beats before template matching — this is the standard MIR technique and directly reduces flicker.
- Apply Viterbi/HMM decoding with a "sticky" self-transition prior (high probability of staying on the same chord) across the beat-synchronized sequence, not on raw per-frame estimates — this is already the intended architecture per PROJECT.md, but the beat-sync step upstream of it is easy to skip and is what makes the smoothing actually work well instead of just delaying the flicker.
- Only commit to chord-per-bar or chord-per-half-bar output granularity for the "detected progression as-is" MIDI row — don't expose frame-level jitter to the user even if internal analysis is finer-grained.

**Warning signs:**
- Same musical section (verse, chorus) produces a different number of "chords" each time segmentation runs, or a visibly higher chord-change rate than the song's actual harmonic rhythm.
- Detected progression has chord changes that don't land on beat boundaries when overlaid on the waveform/grid.

**Phase to address:**
Core chord-detection engine phase, specifically the segmentation/smoothing step — must be designed together with beat/tempo detection (Pitfall 5), not bolted on after.

---

### Pitfall 5: Tempo/beat-grid detection has a systematic half-time/double-time (octave) error

**What goes wrong:**
Beat tracker locks onto a metrical level that's twice or half the "true" musical tempo (e.g., detects 90 BPM when the track is 180, or vice versa), which then misaligns the beat grid used for chord segmentation (Pitfall 4) and for any bar-synced MIDI export.

**Why it happens:**
Autocorrelation/onset-based tempo estimators find the strongest periodic pulse in the signal, but syncopated genres (hip-hop, trap, funk, breakbeat-influenced pop — squarely this product's target genres) create strong sub-beat or off-beat energy that can dominate the autocorrelation peak, and there's no ground truth inside the algorithm to know which metrical level a human would call "the tempo."

**How to avoid:**
- Apply an explicit octave-correction step after initial tempo estimation (favor tempo estimates in the plausible range for the target genres — typically ~70–180 BPM perceptual range — over raw autocorrelation peaks that may sit outside it).
- Where possible, cross-check tempo against the detected chord-change rate (chords rarely change faster than once per beat in this product's target genres) as a secondary signal.
- Surface the detected tempo/grid to the user (even minimally) so an obviously-wrong grid (e.g., "240 BPM" on a clearly mid-tempo track) is visible and correctable rather than silently baked into MIDI export timing.

**Warning signs:**
- Detected BPM is exactly double or half of the tempo a human would tap along to.
- Exported MIDI chord changes land on off-beats or half-bar boundaries that sound wrong when played back against the original audio.

**Phase to address:**
Core chord-detection engine phase (tempo/key detection is an Active requirement), before beat-grid-dependent segmentation is finalized.

---

### Pitfall 6: Real-time-unsafe code inside `processBlock` (allocation, locks, file I/O)

**What goes wrong:**
Plugin causes audio dropouts, crackles, or full DAW freezes/crashes because something in the real-time audio callback allocates memory, takes a blocking lock, or touches the filesystem — even though the actual "heavy" work (chord analysis) is supposed to be offline/background.

**Why it happens:**
Even a plugin whose core feature is "analyze a dropped-in file, not the live audio stream" still has a `processBlock` that JUCE/the DAW calls continuously (for pass-through audio, metering, or even just an inert audio effect shell). It's easy to reach for `std::mutex`, `std::vector::push_back`, `juce::String` concatenation, or a naive flag-check-then-read pattern to shuttle analysis results or UI state between the background analysis thread and `processBlock`, all of which can allocate or block. `std::mutex` is unsafe on the audio thread even with `try_lock()`, because if another thread holds it, the OS scheduler call to wake it up is itself non-realtime-safe.

**How to avoid:**
- Enforce the three-scope rule throughout: allocate everything (buffers, FFT plans) in `prepareToPlay`; `processBlock` does zero allocation, zero locking, zero I/O, zero exceptions; all file I/O and analysis happens on a background thread or `juce::ThreadPool` job.
- Use `juce::AudioProcessorValueTreeState` for any parameter state shared between UI and audio thread (atomic, lock-free by design).
- For passing analysis-thread results (chord grid, waveform data) into anything the audio thread reads, use lock-free FIFOs (`juce::AbstractFifo`) or atomics — never a raw mutex-protected shared struct.
- Run this plugin through `pluginval` (strict mode) and JUCE's own real-time-safety checks regularly, not just before release.

**Warning signs:**
- Audio glitches/dropouts correlate with UI interactions (opening file browser, dragging waveform) rather than CPU load from actual audio processing.
- `processBlock` contains any `new`, `malloc`, `std::vector` resize, `std::mutex::lock()`, `juce::String` building, or file access — grep for these explicitly during code review.

**Phase to address:**
Plugin shell / audio engine scaffolding phase, established as an architectural rule from the first `processBlock` implementation — retrofitting real-time safety after analysis/UI code is entangled with the audio thread is expensive.

---

### Pitfall 7: Attempting live MIDI output from an audio-input plugin instead of drag-and-drop

**What goes wrong:**
Team assumes the plugin can just "output MIDI to the track" like a normal MIDI effect once a chord is detected, discovers VST3/AU audio-effect plugins generally cannot route generated MIDI back into a DAW's MIDI track/piano-roll live, and burns time on a routing approach most major DAWs don't support for this plugin category.

**Why it happens:**
VST3 MIDI output exists as an API but Steinberg has stated MIDI output isn't a first-class citizen of the audio-oriented VST3 API (uses a "legacy" mechanism), and most DAW hosts don't route an audio-effect plugin's generated MIDI into a track's MIDI lane the way they route an instrument's MIDI. This is exactly why established competitors (Scaler, Captain Chords) ship chords via **drag-and-drop of a MIDI file/clip out of the plugin window**, not via live MIDI routing — and why this project's own constraints already correctly scope v1 to drag-and-drop only. The risk is a team member re-deriving "why not just output MIDI" mid-project and re-opening a settled architecture question.

**How to avoid:**
- Treat "no live MIDI-out routing in v1" as a locked architectural decision (already reflected in PROJECT.md's Out of Scope), not a to-be-revisited detail — document the reasoning (DAW routing limitations) alongside the decision so it isn't re-litigated later.
- Implement export via `juce::DragAndDropContainer::performExternalDragDropOfFiles`, writing a temporary `.mid` file per draggable row, exactly matching how Scaler/Captain Chords work.
- If a "live MIDI effect" mode is ever wanted in v2, scope it as a separate plugin format/wrapper (MIDI effect, not audio effect) rather than trying to make one plugin instance do both audio-in-analysis and MIDI-out-generation simultaneously.

**Warning signs:**
- Design discussions proposing "just send MIDI out of the plugin" without a per-DAW routing/compatibility check.
- Confusion in support/testing about why generated chords don't appear on the track automatically.

**Phase to address:**
Plugin shell / architecture phase (already resolved by existing Key Decisions — verify it stays resolved through MIDI export phase).

---

### Pitfall 8: Drag-and-drop MIDI export breaks or crashes in specific DAWs

**What goes wrong:**
`performExternalDragDropOfFiles` works in one DAW during testing (commonly Logic Pro, since AU behaves most predictably there) but fails silently, doesn't trigger `isInterestedInFileDrag`, or actively crashes the host in another — reported specifically for Ableton Live (crash after certain UI sequences, e.g., opening a `FileBrowserComponent` first) and FL Studio (drag callback never fires at all in some configurations).

**Why it happens:**
External drag-and-drop is implemented via OS-level drag APIs that each DAW's plugin host wrapper intercepts differently; JUCE's cross-platform abstraction can't paper over host-specific bugs or unsupported interaction patterns. This is a known, actively-discussed class of issue on the JUCE forum, not a one-off bug.

**How to avoid:**
- Treat "verified in Ableton Live, FL Studio, Logic Pro" (already an Active requirement) as requiring **manual per-DAW interaction testing**, not just a single successful drag in one host — test dragging to the plugin's own track vs. other tracks, before/after opening file browsers or other modal UI, and after save/reopen of the DAW session (a specifically reported trigger for stale-state crashes).
Sources: [forum.juce.com/t/performexternaldragdropoffiles-crashes-in-ableton-after-specific-ui-scenarios](https://forum.juce.com/t/performexternaldragdropoffiles-crashes-in-ableton-after-specific-ui-scenarios/65028), [forum.juce.com/t/drag-and-drop-audio-file-from-fl-studio-into-plugin](https://forum.juce.com/t/drag-and-drop-audio-file-from-fl-studio-into-plugin/28086)
- Keep the drag payload minimal and synchronous (temp `.mid` file already fully written before the drag call starts) — don't kick off drag while background analysis/UI state is mid-transition, which correlates with reported Ableton crashes.
- Have a documented fallback (e.g., a plain "Export .mid" button/file save) so a DAW-specific drag failure doesn't leave users with no way to get the MIDI out — this also directly satisfies the existing "Export у .mid файл" requirement as a safety net.

**Warning signs:**
- Drag works reliably in manual dev testing on Logic but QA reports "nothing happens" or a hard crash in Ableton/FL Studio.
- Crash reports cluster around specific UI sequences (e.g., always after opening a file browser first).

**Phase to address:**
MIDI export / drag-and-drop phase — needs its own explicit per-DAW manual test pass, not just "works on my machine" sign-off, before calling this feature done.

---

### Pitfall 9: Users benchmark accuracy against Chordify/ear and the product looks "broken" on hard material

**What goes wrong:**
Users drop in a track with 7ths, suspensions, or inversions, get simplified/incorrect chord labels, and conclude the product doesn't work — even though the underlying accuracy is in line with (or better than) established competitors, because expectations weren't set.

**Why it happens:**
Published state-of-the-art chord estimation research tops out around 75–85% frame-level accuracy on standard majmin+7ths test sets even for well-resourced academic/commercial systems; Chordify itself is widely reported to default to the nearest simple triad on diminished/half-diminished/suspended/extended chords rather than getting them right. This is a domain ceiling, not an implementation bug — but if the product doesn't communicate that, every miss reads as "this is broken" rather than "this is close, adjust it."

**How to avoid:**
- Explicitly scope and communicate the chord vocabulary the detector targets (e.g., triads + common 7ths, matching PROJECT.md's stated ~75-80% accuracy target) rather than implying full jazz-harmony accuracy.
- Since the actual deliverable is MIDI the user drags and edits (not a definitive chord chart), frame the product around "fast starting point you shape further," not "perfect transcription" — this also matches the stated core value prop (style variations, voicings) more than raw detection accuracy alone.
- Consider surfacing confidence or alternate candidate chords for ambiguous segments in a later phase, rather than presenting single best-guess labels as ground truth.

**Warning signs:**
- Early user feedback repeatedly cites "wrong chords" on tracks known to have jazz/extended harmony (R&B/neo-soul target genre is exactly where this risk concentrates, per PROJECT.md's own target audience).
- Support burden concentrated on accuracy complaints rather than usability issues.

**Phase to address:**
Product/UX framing — relevant from the first user-facing demo/beta phase onward; also a input to the "Detection: chromagram + Viterbi, ~75-80% accuracy" key decision already logged in PROJECT.md.

---

### Pitfall 10: Long analysis blocks the UI thread, making the plugin appear frozen

**What goes wrong:**
Dropping in a 3-minute file triggers chord/key/tempo analysis synchronously on the message thread (because it was easiest to call directly from the drag-drop callback), so the plugin UI freezes — unresponsive, no spinner, sometimes flagged by the OS as "not responding" — for the several seconds analysis takes, and can trigger DAW-level "plugin not responding" warnings if it runs long enough.

**Why it happens:**
`isInterestedInFileDrag`/`filesDropped` callbacks and most UI event handlers run on the JUCE message thread; the natural first implementation calls the analyzer function directly inline, which works in testing on short clips and only becomes visibly bad on full-length songs or slower hardware.

**How to avoid:**
- Run all analysis (chromagram extraction, Viterbi decoding, tempo/key detection) on a background thread (`juce::ThreadPool` job or dedicated `juce::Thread`), never inline in a UI callback.
- Post progress/completion back to the UI via `juce::MessageManager::callAsync` or an `AsyncUpdater`, not direct UI mutation from the background thread (JUCE UI calls like `getBounds()`/repaint must stay on the message thread).
- Show explicit progress/busy state (not just "hope it's fast") — this is already implied by the stated performance constraint ("аналіз у фоновому потоці, UI не блокується") but needs a concrete design (progress bar, cancel option) rather than just backgrounding the work silently.

**Warning signs:**
- UI is unresponsive (can't move window, can't click cancel) for multiple seconds after dropping a file.
- Any direct call from `filesDropped`/button handler into the analysis function without dispatching to a background thread first.

**Phase to address:**
Audio import & analysis pipeline phase — should be designed background-first from the start, not retrofitted once the DSP engine already exists as a blocking call.

---

### Pitfall 11: AU validation (auval) failures and plugin-scan crashes get the plugin silently blacklisted

**What goes wrong:**
Plugin fails Apple's `auval` validation or crashes during a DAW's plugin-scan pass, and the DAW (Logic Pro, GarageBand, and others that gate AU loading on auval) silently refuses to load it or blacklists it — with the failure often invisible to the developer if they only ever manually load a previously-scanned/cached plugin instance during development.

**Why it happens:**
`auval` runs a battery of strict checks (parameter ranges/automation, bus layout consistency between declared and actual, state save/restore round-trip, threading behavior) that day-to-day manual testing in a DAW doesn't exercise the same way a cold scan does. AU-specific bus/channel-layout mismatches, or crashes triggered only during the scan sequence (not during normal use), are a recurring, well-documented category of JUCE AU issues.

**How to avoid:**
- Run `auval -v aufx/aumu <mfr> <subtype>` (or the appropriate AU type) locally after every significant change to `getStateInformation`/`setStateInformation`, bus layout, or parameter list — not just before release.
- Run `pluginval` in strict mode across VST3 and AU builds as part of the regular build/test loop (CI if feasible), since it catches many of the same classes of issue as auval plus additional cross-format checks.
- Test a full "cold scan" in each target DAW (delete plugin caches / use a clean DAW profile) periodically, not just incremental reloads of an already-scanned plugin — scan-time crashes often don't reproduce once a plugin is already cached as valid.

**Warning signs:**
- Plugin works fine when manually loaded in a DAW session that already has it cached, but disappears/fails to appear after clearing the DAW's plugin cache or on a clean install.
- `auval` reports failures on state round-trip or bus layout even though the plugin "looks fine" in manual testing.

**Phase to address:**
Plugin shell/scaffolding phase for the baseline check, then re-verified at the end of every phase that touches state persistence, parameters, or bus layout (MIDI export, packaging).

---

### Pitfall 12: Plugin state persistence breaks preset recall or crashes on project reload

**What goes wrong:**
DAW project is saved and reopened (or a preset is recalled), and the plugin either loses its analysis/UI state, throws on `setStateInformation`, or shows a blank/default UI — because `setStateInformation` can be called by the host before the editor exists, or because the ValueTree structure isn't defensively validated before use.

**Why it happens:**
The common tutorial pattern (serialize a `ValueTree` to XML in `getStateInformation`, parse and `replaceState` in `setStateInformation`) is correct as far as it goes, but doesn't account for host call-order quirks: some hosts call `setStateInformation` during plugin construction/project load before any editor is open, and naive implementations that assume the editor exists (to push loaded data into UI components directly) will crash or silently no-op.

**How to avoid:**
- Store all persisted state in the processor's ValueTree/APVTS (source of truth), and have the editor read from it when constructed/shown — never treat "editor exists" as a precondition for `setStateInformation` succeeding.
- Defensively validate incoming state in `setStateInformation`: check the XML parsed successfully and the root element's tag matches the expected ValueTree type before calling `replaceState`; fall back to default state rather than crashing on malformed/foreign data.
- Explicitly test: save DAW project → close DAW entirely → reopen → verify state (not just "undo/redo within the same session," which exercises a different code path than a cold project reload).

**Warning signs:**
- Plugin works correctly within a single DAW session but loses settings after a full close/reopen of the project file.
- Crash logs showing `setStateInformation` on the call stack during project load.

**Phase to address:**
Plugin shell phase for the base mechanism; re-verified whenever new persisted state is added (voicing preferences, last-used file path, etc.).

---

### Pitfall 13: macOS code signing/notarization treated as a late "packaging" afterthought

**What goes wrong:**
A working, tested plugin fails to launch for real users because Gatekeeper blocks an unsigned/unnotarized VST3 or AU bundle, or because notarization was only tested on a `.dmg` without realizing each plugin format (VST3 bundle, AU component, standalone app) must be signed and (for direct distribution) notarized as its own item — discovered only when preparing the first external build near release, at which point iterating is expensive (Apple review turnaround, certificate/agreement issues).

**Why it happens:**
Code signing/notarization isn't needed for local development (Xcode/CMake builds run unsigned locally), so it's easy to defer indefinitely — but it requires an active $99/year Apple Developer Program membership, a Developer ID certificate, the hardened runtime entitlement, and (per Apple's current process) uploading each signed artifact for notarization and stapling the ticket, with separate signing required per plugin format since writing into an already-signed bundle invalidates its signature.

**How to avoid:**
- Set up Developer ID signing + notarization + `pluginval`/auval validation as part of the build pipeline early (even before commercial launch is imminent), so packaging issues surface while there's time to fix them, not during a release crunch.
- Sign with `--options=runtime` (hardened runtime, required for notarization); sign the bundle as a whole (not individual internal binaries after the fact); notarize and staple VST3, AU, and the standalone app each as separate artifacts.
- Track the Apple Developer Program agreement — re-signing/notarizing can silently start failing after Apple issues a new agreement version until it's re-accepted in the developer account.

**Warning signs:**
- Plugin only ever tested via local unsigned builds or Xcode "run" — never through an actual signed-and-notarized distributable.
- No CI/build step currently produces a notarized artifact; signing is "something we'll figure out before release."

**Phase to address:**
Should be validated (not necessarily fully productionized) early — at least a signed+notarized smoke-test build during the plugin-shell phase — and finalized in the packaging/release phase.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|-----------------|------------------|
| Frame-level chord classification without beat-sync (skip Pitfall 4 fix) | Faster to get a first "it detects something" demo | Flickering, musically nonsensical output that undermines the whole product's credibility | Only for an internal proof-of-concept spike, never in anything shown to a user |
| Enable `JUCE_USE_MP3AUDIOFORMAT` instead of relying on platform-native decoding | One-line JUCE flag, works cross-platform in theory | JUCE explicitly disclaims IP-infringement risk for this code path; adds legal ambiguity for a commercial product for no real benefit on macOS | Never for macOS-only v1 — use `CoreAudioFormat` (native AudioToolbox MP3 decoding) instead, which carries no such disclaimer |
| Synchronous inline analysis call (skip Pitfall 10 fix) | Simpler code, easier to debug in early dev | UI freezes become a released-product bug, not just a dev-time annoyance | Only during earliest DSP-algorithm prototyping (e.g., a standalone command-line test harness, not the plugin UI) |
| Defer signing/notarization setup (skip Pitfall 13) | No Apple Developer account friction while iterating on core features | Packaging problems discovered late, when there's no slack to fix them before a promised release date | Acceptable to defer *finalizing* distribution packaging, but a signed+notarized smoke build should exist well before release, not first attempted at release |
| Use aubio/Essentia "just to get something working," swap later | Fast bring-up using a mature, tested library | Either a full detector rewrite before launch, or an accidental GPL/AGPL-tainted release | Never for code intended to ship — acceptable only as a throwaway offline research/comparison script never linked into the plugin |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|--------------|-----------------|-------------------|
| Ableton Live (drag-and-drop MIDI) | Assuming one successful drag test = compatible; not testing after opening file browsers or across saved/reopened sessions | Test drag to own track and other tracks, before/after other UI interactions, and after a full project save/reopen |
| FL Studio (drag-and-drop MIDI) | Assuming `isInterestedInFileDrag`/drag callbacks behave the same as Ableton/Logic | Explicitly verify the drag callback actually fires in FL Studio; some configurations reportedly never trigger it — treat as its own test matrix, not "should work since Ableton works" |
| Logic Pro / AU host | Testing only in "already scanned/cached" state during dev | Periodically clear AU cache / run a cold `auval` pass, not just interactive testing in an already-trusted plugin instance |
| macOS Gatekeeper/notarization | Treating signed-`.dmg`-only distribution as sufficient | Notarize each individual plugin bundle format (VST3, AU, standalone app), not just the installer/dmg wrapper |
| MP3 decoding | Enabling JUCE's bundled `MP3AudioFormat` for convenience | Use `juce::CoreAudioFormat` on macOS (native AudioToolbox decode, no IP disclaimer) for MP3/AAC/etc. |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|-----------------|
| Full-resolution STFT chromagram at high frame rate over a whole song | Analysis of a 3-minute song takes tens of seconds instead of the targeted "seconds" | Beat-synchronize early (reduces frames from ~thousands to ~hundreds), use appropriately-sized FFT/hop, run on background thread pool | Noticeable once songs exceed ~2-3 minutes or on lower-end hardware if not addressed from the start |
| Synchronous file decode + full-song load into memory before analysis starts | UI appears to hang before analysis progress even begins, on longer files | Stream/decode incrementally on the background thread; show a distinct "decoding" vs "analyzing" progress stage | Breaks user-perceived responsiveness on any file more than ~1 minute long, worse on slower drives |
| Recomputing full analysis on every minor UI interaction (e.g., adjusting selected region) | Sluggish UI when scrubbing/resizing the analysis region selector | Cache chroma/feature extraction results, only re-run the (cheap) segmentation/Viterbi step when the selected region changes, not full feature extraction | Breaks as soon as region-selection UI is interactive rather than a one-shot "analyze whole file" action |

## Security Mistakes

Not a networked/server product (fully offline per PROJECT.md), so classic web security concerns don't apply — the domain-relevant risks are IP/licensing and code-signing integrity instead:

| Mistake | Risk | Prevention |
|---------|------|------------|
| Linking GPL/AGPL-licensed DSP code (aubio, Essentia) into the commercial binary | Product cannot legally be sold closed-source as built; potential legal exposure if discovered post-launch | License-audit every dependency before adding it (see Pitfall 1); prefer MIT/BSD/Apache/zlib-licensed or in-house code |
| Shipping non-commercial-only pretrained models (e.g., madmom's CC-NC weights) | Same commercial-use violation risk, easy to miss since the *code* license looks fine | Check model/weight licenses separately from source-code licenses |
| Unsigned/ad-hoc-signed builds distributed outside the Mac App Store | Gatekeeper blocks users from running it at all, or users have to bypass security warnings (bad first impression for a paid product) | Developer ID signing + notarization as part of the standard build pipeline, not a manual pre-release step |
| No license/serial validation strategy decided before release (explicitly listed as post-MVP in PROJECT.md) | Fine to defer per current scope, but retrofitting anti-piracy/licensing into an already-built plugin can require touching state persistence and startup flow broadly | When this phase arrives, design it as an addition to the existing state/startup architecture, not a bolt-on that has to fight the existing `getStateInformation` flow |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-------------------|
| Presenting single best-guess chord labels with no indication of confidence/ambiguity | Users lose trust in the whole tool after ~one wrong chord on complex material (see Pitfall 9) | Frame output as a fast starting point to drag/edit, not a definitive transcription; consider surfacing alternate candidates for low-confidence segments later |
| No visible feedback during multi-second analysis | Plugin feels frozen/broken (see Pitfall 10) | Explicit progress indicator with distinct decode/analyze stages, cancel option |
| Drag-and-drop as the *only* way to get MIDI out, with no fallback when it fails in a given DAW | Users hit a dead end in DAWs where drag-and-drop misbehaves (Pitfall 8) | Keep the "Export to .mid file" requirement as a guaranteed-to-work fallback path, not just a nice-to-have |
| Silent wrong tempo/key baked into MIDI export | User only discovers the grid is off (e.g., half-time) after dragging into their DAW and it sounds wrong | Surface detected tempo/key in the UI before export so obviously-wrong detections are visible and (ideally) correctable |

## "Looks Done But Isn't" Checklist

- [ ] **Chord detection engine:** Passing on a handful of hand-picked demo songs — verify against a broader, genre-matched test set (pop/hip-hop/trap, R&B/neo-soul, EDM per target audience) including tracks with live drums, non-standard tuning, and syncopated tempo, not just clean/electronic references.
- [ ] **Drag-and-drop MIDI export:** "Works in Logic" — verify separately in Ableton Live and FL Studio, including after project save/reopen and after opening other plugin UI elements first (Pitfall 8).
- [ ] **Plugin state save/load:** Works across undo/redo in one session — verify across a full DAW quit-and-reopen of the project file (Pitfall 12).
- [ ] **AU build:** Loads manually in Logic during dev — run an actual `auval` pass and a cold plugin-cache scan, not just interactive use of an already-scanned instance (Pitfall 11).
- [ ] **Real-time safety:** "No audible glitches in casual testing" — actually grep `processBlock` (and anything it calls) for allocation/locking/I-O, and run `pluginval` strict mode (Pitfall 6).
- [ ] **MP3 import:** "MP3 files load fine" — verify which decode path is active (`CoreAudioFormat` vs `JUCE_USE_MP3AUDIOFORMAT`) and that it's the IP-safe one (Pitfall 8/Technical Debt table).
- [ ] **Third-party dependency licenses:** "We used library X, it's open source" — verify the *specific* license (not just "it's on GitHub") and that it explicitly permits commercial closed-source distribution (Pitfall 1).
- [ ] **Packaging:** "Builds successfully" — verify it's actually signed with Developer ID, hardened runtime, and notarized/stapled per plugin format, tested on a clean machine without the dev's own certificates/Gatekeeper exceptions (Pitfall 13).

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|-----------------|------------------|
| GPL/AGPL library already linked into shipped/near-shipped binary | HIGH | Full rewrite of the affected DSP component with a clean-room/in-house or permissively-licensed implementation; cannot be resolved by relicensing after the fact without the original library's copyright holder's consent |
| Chromagram/segmentation producing flickery output (Pitfalls 2-4 combined) | MEDIUM | Add beat-sync aggregation and tuning correction as an upstream feature-extraction pass; existing Viterbi/template-matching logic downstream typically doesn't need to be rewritten, just fed better input |
| Real-time-unsafe code discovered in `processBlock` late | MEDIUM | Move offending logic to background thread + lock-free handoff (AbstractFifo/atomics); usually localized to specific call sites once identified via `pluginval`/profiling, not a full-engine rewrite |
| Drag-and-drop broken in a specific DAW near release | LOW–MEDIUM | Ship the "Export to .mid file" fallback (already a planned requirement) as the primary workaround while root-causing the DAW-specific drag issue; doesn't block release if fallback exists |
| Signing/notarization failing close to release | LOW–MEDIUM | Usually a process/config fix (re-accept Apple Developer Agreement, fix entitlements, re-sign whole bundle) rather than a code problem — but needs Apple's notarization turnaround time factored into the release schedule |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|--------------------|----------------|
| 1. GPL/AGPL contamination | Stack/dependency selection (earliest) | Dependency license audit checklist reviewed before each new library is added |
| 2. Full-mix chromagram noise (no tuning/percussion handling) | Core chord-detection engine | Accuracy test on genre-matched real-world tracks (not just clean references) |
| 3. Bass octave/root errors | Core chord-detection engine | Dedicated bass-chroma test cases with known inversions/slash chords |
| 4. Segmentation flicker without beat grid | Core chord-detection engine | Visual/manual review of chord-change rate vs. actual harmonic rhythm on test songs |
| 5. Tempo octave error | Core chord-detection engine (tempo/key detection) | Test set specifically including syncopated hip-hop/trap/funk-influenced tracks |
| 6. Real-time-unsafe `processBlock` | Plugin shell/audio engine scaffolding | `pluginval` strict mode in regular build/test loop; code review grep for locks/allocation in audio callback |
| 7. Live MIDI-out routing mistakenly attempted | Plugin shell/architecture (already decided) | Architecture decision doc referenced whenever MIDI export design is revisited |
| 8. Drag-and-drop DAW-specific breakage | MIDI export/drag-and-drop phase | Manual per-DAW test matrix (Ableton/FL Studio/Logic) including post-reopen and post-other-UI-interaction cases |
| 9. Unrealistic accuracy expectations | Product/UX framing, first user-facing beta | Beta feedback specifically tagged/reviewed for accuracy-vs-usability complaint ratio |
| 10. UI thread blocking during analysis | Audio import & analysis pipeline phase | Manual test with full-length (3+ min) songs on non-dev-grade hardware; UI must stay responsive throughout |
| 11. AU validation/scan crashes | Plugin shell (baseline), re-verified every phase touching state/params/buses | `auval` + cold plugin-cache scan pass before each release candidate |
| 12. State persistence breaking on reload | Plugin shell (baseline), re-verified when new state is added | Full DAW quit-and-reopen test (not just in-session undo/redo) |
| 13. Signing/notarization deferred too long | Validated early (plugin shell), finalized at packaging/release phase | A signed+notarized smoke-test build exists well before the release date, tested on a clean machine |

## Sources

- [JUCE Forum: Understanding Lock in Audio Thread](https://forum.juce.com/t/understanding-lock-in-audio-thread/60007) — real-time safety, mutex/SpinLock guidance
- [timur.audio: Using locks in real-time audio processing, safely](https://timur.audio/using-locks-in-real-time-audio-processing-safely) — real-time lock-free patterns
- [JUCE Forum: Locks and memory allocations in the processing thread](https://forum.juce.com/t/locks-and-memory-allocations-in-the-processing-thread/39964)
- [aubio.org](https://aubio.org/) and [aubio COPYING (GitHub)](https://github.com/aubio/aubio/blob/master/COPYING) — GPLv3 license confirmation
- [Essentia Licensing Information](https://essentia.upf.edu/licensing_information.html) — AGPLv3 / commercial dual license
- [madmom GitHub / LICENSE](https://github.com/CPJKU/madmom/blob/main/LICENSE) — BSD code, CC-BY-NC-SA pretrained models
- Automatic Chord Estimation review literature (chromagram octave-invariance, bass chroma for inversions, beat-synchronized aggregation, MIREX ~75-85% accuracy ceiling) — academic MIR survey sources located via search (Vicar, "Automatic Chord Estimation from Audio: A Review of the State of the Art")
- [JUCE Forum: performExternalDragDropOfFiles crashes in Ableton after specific UI scenarios](https://forum.juce.com/t/performexternaldragdropoffiles-crashes-in-ableton-after-specific-ui-scenarios/65028)
- [JUCE Forum: Drag and drop audio file from FL Studio into plugin](https://forum.juce.com/t/drag-and-drop-audio-file-from-fl-studio-into-plugin/28086)
- [KVR Audio: how to work around midi in VST3?](https://www.kvraudio.com/forum/viewtopic.php?t=538083) — VST3 MIDI output as a "legacy"/non-first-class API, DAW support gaps
- [Scaler 3 MIDI Output DAW Routing Guide (scalermusic.com)](https://scalermusic.com/wp-content/uploads/2025/06/Scaler-3-MIDI-Output-DAW-Routing-Guide.pdf) — competitor's drag-and-drop-first MIDI delivery approach
- [JUCE MP3AudioFormat class reference](https://docs.juce.com/master/classMP3AudioFormat.html) and [JUCE Forum: Is MP3 really free?](https://forum.juce.com/t/is-mp3-really-free/24654) — JUCE's own IP-risk disclaimer on bundled MP3 decode
- [Melatonin: How to code sign and notarize macOS audio plugins in CI](https://melatonin.dev/blog/how-to-code-sign-and-notarize-macos-audio-plugins-in-ci/)
- [Moonbase: Code signing audio plugins in 2025, a round-up](https://moonbase.sh/articles/code-signing-audio-plugins-in-2025-a-round-up/)
- [Apple Developer Forums: Conflict between Gatekeeper and AU/VST2/VST3 plug-ins](https://developer.apple.com/forums/thread/666938)
- Tempo octave-error literature ("Exploiting global features for tempo octave correction" and related MIR sources) — half/double tempo detection failure mode
- Chordify limitations discussion (musicianwave.com and related chord-identifier comparison sources) — user-facing accuracy ceiling on complex chords, competitor benchmark for expectation-setting
- [JUCE Forum: Multithreading / Load samples on background thread](https://forum.juce.com/t/load-samples-on-background-thread/46337) and [JUCE ThreadPool source](https://github.com/juce-framework/JUCE/blob/master/modules/juce_core/threads/juce_ThreadPool.h) — background analysis threading patterns

---
*Pitfalls research for: JUCE chord-detection/MIDI-generation commercial audio plugin (ChordAI)*
*Researched: 2026-07-12*
