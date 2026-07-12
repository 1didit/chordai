# Stack Research

**Domain:** Commercial audio plugin (VST3/AU/Standalone) — offline chord/key/tempo detection + MIDI generation, macOS-first
**Researched:** 2026-07-12
**Confidence:** HIGH (JUCE/CMake/licensing verified against official docs, GitHub source, and license files; MEDIUM on ONNX integration specifics and notarization tooling, verified via multiple community sources but not official JUCE docs)

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| JUCE | 8.0.x (latest verified: **8.0.13**, released May 2026 — track `8.0.x`/`master`) | Cross-platform plugin framework: audio I/O, GUI, VST3/AU/AAX wrapping, MIDI, drag-and-drop | Industry-standard for commercial plugins; one codebase builds VST3 + AU + Standalone. JUCE 8 modernized the GUI layer (flexbox-like layouts, Direct2D on Windows) but the DSP/audio-format/MIDI/drag-drop APIs this project relies on are stable since JUCE 6/7. |
| CMake | ≥ 3.22 (JUCE's hard minimum) — use **3.25+** in practice | Build system for all three plugin formats + Standalone from one `CMakeLists.txt` | JUCE's official CMake API (`docs/CMake API.md`) requires 3.22+; 3.25+ avoids known target-property edge cases used by common JUCE CMake templates (pamplejuce, melatonin's guide). Projucer (the old IDE-project-file generator) is legacy — CMake is the current standard workflow for JUCE 7/8 projects. |
| C++20 | — | Language standard | JUCE 8 only *requires* C++17, so C++20 (already decided in PROJECT.md) is fully compatible and gives you concepts/ranges for the DSP/analyzer code without any framework friction. Do not go below C++17 — JUCE hard-fails the build otherwise. |
| Xcode | current stable (Xcode 16.x line) | macOS toolchain, code signing, AU validation (`auval`) | Required for AU builds specifically (Apple's AU wrapper needs `CoreAudioKit`/`AudioToolbox`, only available via Xcode's SDKs). VST3 and Standalone also build fine via Xcode-generated CMake projects. |

**Licensing note (critical for a commercial closed-source product):** JUCE itself is dual-licensed. The free path (AGPLv3) requires you to open-source ChordAI — **not compatible** with PROJECT.md's "commercial product" goal. The commercial path has three tiers: **Starter** (free, revenue < $20k/year, closed-source allowed, no splash screen requirement in JUCE 8), **Indie** ($40/mo or $800 one-time per developer, revenue < $300k/year), **Pro** ($175/mo or $3,500 one-time, no revenue cap). Start on Starter during development; budget for Indie before/at commercial launch. Confidence: HIGH (juce.com pricing page + JUCE forum threads on the 2024 AGPLv3 licensing change, cross-checked).

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::CoreAudioFormat` (JUCE built-in, `juce_audio_formats`) | ships with JUCE 8 | Decode MP3/AAC/ALAC/M4A on macOS via `AudioToolbox` system codecs | **Primary decode path for v1 (macOS-only).** Apple already pays MP3/AAC codec royalties for its system frameworks, so decoding through `CoreAudioFormat` carries zero additional licensing cost or risk. |
| `juce::MP3AudioFormat` (JUCE built-in, opt-in via `JUCE_USE_MP3AUDIOFORMAT`) | ships with JUCE 8 | Cross-platform, read-only MP3 decoder (no encode) | Needed only when you port to Windows (no CoreAudio there). The MP3 patent pool (Fraunhofer/Technicolor) expired in 2017, so risk is low, but JUCE's own header explicitly disclaims warranty of freedom from 3rd-party IP claims — note this, don't ignore it, when the Windows build is scoped. |
| `juce::FlacAudioFormat`, `juce::WavAudioFormat`, `juce::AiffAudioFormat` (JUCE built-in) | ships with JUCE 8 | WAV/AIFF/FLAC read+write | Enabled by default in `juce_audio_formats`; covers 3 of your 4 required input formats with zero extra dependencies or licensing questions (bundled libFLAC is BSD-style). |
| `juce::dsp::FFT` (JUCE built-in, `juce_dsp`) | ships with JUCE 8 | STFT for chromagram/spectral-flux input | Use for v1. On macOS it transparently wraps Apple's **Accelerate/vDSP**, which is hardware-accelerated and requires no extra dependency or license. Simplest option while you're macOS-only. |
| pffft (Julien Pommier, BSD-like license) | latest (no formal version tags; pin a commit) | Drop-in replacement FFT if you outgrow `dsp::FFT`'s portability (e.g., Windows without vDSP/IPP) | Only needed once you target Windows and want SIMD performance without Accelerate. Small, single-file, license explicitly compatible with proprietary/closed-source use. |
| constant-q-cpp (Chris Cannam / Queen Mary University London, **MIT-style license**, bundles KissFFT BSD internally) | latest `master` (no tagged releases; pin commit) | Constant-Q Transform + reference `Chromagram.cpp` — the standard MIR building block for chord/key detection | **Recommended CQT engine for v1's offline analyzer.** Built by the same research group (QMUL Centre for Digital Music) behind the well-known chord-recognition literature; license verified permissive (checked `COPYING` file directly). Log-frequency bins are a better fit than raw STFT for resolving bass notes/chord roots — directly relevant since ChordAI also needs a bass line output. |
| rt-cqt (Jonas Merkt, **BSD-3-Clause**, header-only C++11) | latest (pin commit) | Real-time-capable streaming CQT | Not needed for v1 (analysis is offline/file-based). Keep in mind for the v2 "Listen" (live DAW input) feature explicitly marked out-of-scope for v1 in PROJECT.md — swap in then instead of constant-q-cpp. |
| — (no library — custom implementation) | — | Chord template matching + **Viterbi/HMM smoothing** over chroma frames | This is a well-published algorithm (Bello & Pickens 2005; Sheh & Ellis 2003), not a piece of licensed code — implement it directly against your own chroma vectors. No dependency = no licensing question, and it's a small amount of code (dynamic-programming Viterbi is ~50-100 lines). |
| — (no library — custom implementation) | — | Onset detection + tempo/beat estimation | See "What NOT to Use" below — the well-known C++ libraries in this space are GPL/AGPL. Implement spectral-flux onset detection (reuse the FFT/CQT frames you already compute for the chromagram) + autocorrelation or comb-filterbank tempo estimation (Ellis 2007-style dynamic-programming beat tracker). Published algorithm, not licensed code. |
| `juce::MidiFile` / `juce::MidiMessageSequence` (JUCE built-in, `juce_audio_basics`) | ships with JUCE 8 | Build and write Standard MIDI Files for the generated chord/voicing/bassline rows | No external MIDI library needed — JUCE's MIDI file writer covers this fully. |
| `juce::DragAndDropContainer::performExternalDragDropOfFiles(...)` (JUCE built-in, `juce_gui_basics`) | ships with JUCE 8 | Drag a generated `.mid` row from the plugin editor straight into the DAW's piano roll | **Confirmed this is a `static` function** (verified against `docs.juce.com`) — you do *not* need your `AudioProcessorEditor` to be a full `DragAndDropContainer` ancestor to call it, which simplifies plugin-editor usage. Standard trick: on `mouseDrag`, write the row to a temp `.mid` file (`File::getSpecialLocation(tempDirectory)`), then call this with that path. Known compatibility caveats exist in some DAW/host versions (e.g., a reported Logic Pro 12.2.1 regression) — test explicitly against Ableton Live, FL Studio, and Logic Pro per PROJECT.md's requirement, don't assume universal support. |
| ONNX Runtime (Microsoft, **MIT license**) | latest stable 1.2x.x at integration time | v2 ML inference backend for `ChordAnalyzer` | Not needed for v1 (PROJECT.md explicitly defers ML to v2). When you get there: link the **dynamic/shared** build (`.dylib` on macOS) rather than static — community reports (JUCE forum "Our first commercial plugin using ONNX") confirm ORT isn't reliably built as a static lib, and shipping a shared lib alongside the plugin bundle is the practical pattern used by shipped commercial JUCE+ONNX plugins. Keep model inference off the audio thread; this is an offline/background-analysis use case already, so that constraint is easy to satisfy here. |
| Catch2 | v3.7+ (pin via CMake `GIT_TAG`) | Unit tests for the DSP/analyzer/MIDI-generation code | De facto standard test framework in the current JUCE ecosystem (used by `pamplejuce`, the most widely adopted JUCE+CMake template). GoogleTest is a valid alternative (JUCE itself doesn't care which you use) but Catch2's header-only, BDD-style syntax fits solo/small-team plugin projects better. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| pluginval (Tracktion) | Automated VST3/AU validation (load, parameter fuzzing, state save/restore, crash checks) | GPLv3-licensed **tool**, but it runs your built plugin binary out-of-process as a validator — it does not get linked into your product, so GPL does not propagate to ChordAI. Run in CI at **strictness level 5** minimum (community-recognized floor for host-compatibility confidence); JUCE 8 is the officially tested/supported target version. |
| CTest (bundled with CMake) | Runs Catch2 test binaries in CI | Standard pairing with Catch2 + CMake; no extra setup beyond `enable_testing()`. |
| GitHub Actions (or equivalent CI) | Build VST3/AU/Standalone, run pluginval + unit tests on every push | Not evaluated in depth here (out of scope for this research pass) — `pamplejuce` is a good reference workflow to adapt. |
| `xcrun notarytool` + `codesign` (Apple, bundled with Xcode CLT) | Code signing + notarization for macOS distribution | **Note only — later phase per PROJECT.md scope.** Requires a paid Apple Developer Program membership ($99/yr), a "Developer ID Application" certificate, and (since 2022) `notarytool` for a streamlined CI-friendly notarization flow — no further tooling changes reported through 2025-2026. Sign each VST3/AU/.app individually, then notarize the containing ZIP/PKG/DMG and staple the ticket. |

## Installation

This is a CMake-based C++ project, not an npm project — dependency setup is via `add_subdirectory`/`FetchContent`, not a package manager.

```cmake
# --- JUCE: git submodule (large repo, pin an exact tag, avoid re-fetching on every clean build) ---
# git submodule add https://github.com/juce-framework/JUCE.git external/JUCE
# git -C external/JUCE checkout 8.0.13
add_subdirectory(external/JUCE)

# --- Smaller deps: FetchContent, pinned by tag/commit ---
include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1)
FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(constant_q_cpp
    GIT_REPOSITORY https://github.com/cannam/constant-q-cpp.git
    GIT_TAG        master)   # pin to a specific commit SHA in production
FetchContent_MakeAvailable(constant_q_cpp)

# pluginval: download prebuilt binary in CI rather than building from source (faster, official releases published)
```

```bash
# pluginval (CI / local validation) — grab prebuilt binary, don't build from source
# https://github.com/Tracktion/pluginval/releases
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|--------------------------|
| `juce::dsp::FFT` (wraps Accelerate/vDSP on macOS) | pffft | Once Windows is in scope and you want consistent SIMD performance without Accelerate/IPP; also useful outside the JUCE dependency tree entirely. |
| constant-q-cpp (offline CQT + chromagram base) | rt-cqt | When the v2 "Listen" real-time input feature is built — rt-cqt is purpose-built for streaming/low-latency use, constant-q-cpp is not. |
| Custom Viterbi chord recognizer + custom onset/tempo detector | Essentia (AGPLv3, commercial license available from MTG-UPF) | If in-house DSP accuracy plateaus below target and you have budget/time to negotiate and pay for Essentia's commercial license — this trades cash + integration complexity for a much larger, battle-tested MIR algorithm library (HPCP, RhythmExtractor2013, key detection all included). Evaluate only after shipping a working custom v1, not before. |
| CMake submodule for JUCE | CMake `FetchContent` for JUCE too | Some newer community templates FetchContent JUCE as well; submodule is preferred here mainly for pinning a large repo without re-downloading on clean builds, and is what the official JUCE CMake docs demonstrate via `add_subdirectory`. Either works — this is a minor preference, not a hard constraint. |
| Catch2 | GoogleTest | If team/hiring familiarity favors GoogleTest, or if you need death tests / more elaborate mocking (`gmock`) that Catch2 doesn't provide as directly. |
| ONNX Runtime | LibTorch (PyTorch C++), TensorFlow Lite | Only relevant in v2; ONNX Runtime is recommended because it's the most common target format from both PyTorch and TensorFlow training pipelines and has the most JUCE-plugin prior art (multiple shipped commercial examples found). |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|--------------|
| aubio | **GPLv3.** Any closed-source product linking it must either be GPL itself or get a separate commercial license directly from the author (no public pricing found) — legally incompatible with a closed-source commercial plugin without that negotiation. | Custom onset detection (spectral flux) + custom tempo estimation (autocorrelation/comb-filter), built from published algorithms. |
| BTrack | **GPLv3.** Same problem as aubio — real-time beat tracker, GPL-licensed, no commercial alternative offered by the author found. | Same as above. |
| NNLS Chroma / Chordino | **GPLv2-or-later.** This is literally a chord-transcription library (closest existing match to ChordAI's core feature) but it's GPL, so it cannot be linked into a closed-source product. | Build your own HPCP-style chromagram (using constant-q-cpp, MIT) + your own chord-template/Viterbi matcher, informed by the same published research this library implements (Mauch & Dixon; Gómez 2006). |
| Essentia (default license) | **AGPLv3** by default — stricter than GPL (network-use copyleft). A commercial license exists but requires direct negotiation/fee with Music Technology Group (UPF); do not assume it's free just because it's "open source." | Custom DSP for v1 (see above); revisit Essentia's *paid* commercial license only as a deliberate, budgeted v2 decision if accuracy demands it. |
| FFTW3 (default license) | **GPL**, with a separate paid commercial license (historically expensive, priced per-project via MIT's TLO) required for closed-source use. | `juce::dsp::FFT` (v1) or pffft/KissFFT (both permissive) if you need to swap FFT engines later. |
| JUCE under the free AGPLv3 track | Requires releasing ChordAI's full source — directly contradicts PROJECT.md's "commercial product" requirement. | JUCE Starter (free, closed-source, <$20k/yr revenue) now, budgeted upgrade to Indie ($40/mo or $800 one-time, <$300k/yr) at/before commercial launch. |
| Projucer-generated IDE projects as the primary build path | Legacy workflow; JUCE 7/8 ecosystem (templates, CI examples, community tooling like pluginval integration) has moved to CMake as the standard. | CMake, as already decided in PROJECT.md. |

## Stack Patterns by Variant

**If staying macOS-only (current v1 phase):**
- Use `CoreAudioFormat` for MP3/AAC decode (system codecs, zero licensing cost) and `juce::dsp::FFT` (auto-wraps Accelerate/vDSP) — this gets you WAV/AIFF/FLAC/MP3 decode and fast FFT with **zero extra third-party dependencies** beyond JUCE itself.
- Because Apple already covers codec royalties for its system APIs, and vDSP is the fastest FFT path available on the platform without any extra integration work.

**If/when porting to Windows (post-v1, per PROJECT.md's "Out of Scope" section):**
- Switch MP3 decode to JUCE's bundled `MP3AudioFormat` (cross-platform, read-only) since `CoreAudioFormat` is macOS/iOS-only; re-evaluate FFT (Accelerate isn't available) — pffft is the most direct portable swap-in for `dsp::FFT`.
- Because the mac-native system-codec shortcut disappears outside Apple platforms, and FFT performance parity needs a cross-platform SIMD library at that point.

**If/when the v2 ML backend (ONNX-based `ChordAnalyzer`) ships:**
- Link ONNX Runtime as a **dynamic/shared library**, bundled next to the plugin binary, behind the same `ChordAnalyzer` interface the DSP-based v1 implementation already uses (per PROJECT.md's "ML-ready architecture" decision).
- Because static linking of ORT is unreliable per community reports, and the interface-based design means the backend swap shouldn't require touching UI or MIDI-generation code at all.

**If/when the v2 "Listen" (live audio input) feature ships:**
- Swap constant-q-cpp's offline CQT for rt-cqt's streaming/real-time implementation in the analyzer's DSP core.
- Because constant-q-cpp is not optimized for continuous low-latency streaming, while rt-cqt is purpose-built for exactly that use case.

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|------------------|-------|
| JUCE 8.0.13 | CMake ≥ 3.22 (JUCE's documented hard minimum) | Use 3.25+ in practice to match what current community CMake templates (e.g. `pamplejuce`) assume, avoiding edge-case target-property issues. |
| JUCE 8.0.13 | C++17 minimum, C++20 confirmed compatible | JUCE will hard-fail the build below C++17 (`"JUCE requires c++17 or later"`); C++20 (PROJECT.md's choice) works without friction. |
| Catch2 v3.7+ | CMake `FetchContent`, CTest | Standard, widely used pairing in current JUCE+CMake templates. |
| pluginval (latest release) | JUCE 8 | Tracktion's own docs state JUCE 8 is the currently tested/supported target for their newest validation method. |
| constant-q-cpp | KissFFT (bundled internally, BSD) | No conflict — constant-q-cpp vendors its own small BSD-licensed FFT (`src/ext/kissfft`), doesn't need JUCE's FFT. |

## Sources

- https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md — CMake minimum version (3.22), `add_subdirectory` as documented integration path. HIGH confidence (official docs).
- https://github.com/juce-framework/JUCE/releases — JUCE 8.0.13 as latest verified release (May 2026). HIGH confidence (official GitHub releases).
- https://juce.com/get-juce/ — JUCE 8 license tiers (Starter/Indie/Pro) and pricing. HIGH confidence (official pricing page, fetched directly).
- JUCE forum: "JUCE8 License and Open-Source Projects" (forum.juce.com/t/juce8-license-and-open-source-projects/60987) — confirms JUCE 8's shift from GPLv3 to AGPLv3 for the free/open-source track, Starter tier $20k revenue cap. MEDIUM-HIGH confidence (community forum, but consistent across multiple threads).
- https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_formats/codecs/juce_MP3AudioFormat.h — MP3AudioFormat read-only, IP-risk disclaimer in source comments. HIGH confidence (source code itself).
- https://docs.juce.com/master/classCoreAudioFormat.html — CoreAudioFormat uses AudioToolbox system codecs (macOS/iOS). HIGH confidence (official docs).
- https://hackaday.com/2025/02/08/freed-at-last-from-patents-does-anyone-still-care-about-mp3/ and 2017 patent-expiry coverage — MP3 patents expired 2017. MEDIUM confidence (tech journalism, not a legal opinion).
- https://github.com/berndporr/kiss-fft/blob/master/LICENSE — KissFFT BSD-3-Clause confirmed by direct license file read. HIGH confidence.
- https://bitbucket.org/jpommier/pffft/ + JUCE forum FFT comparison threads — pffft BSD-like, proprietary-compatible. MEDIUM-HIGH confidence.
- https://github.com/cannam/constant-q-cpp/blob/master/COPYING — direct license file read: MIT-style (QMUL) + bundled BSD KissFFT. HIGH confidence.
- https://github.com/jmerkt/rt-cqt/blob/main/LICENSE — direct license file read: BSD-3-Clause. HIGH confidence.
- https://aubio.org/ + https://github.com/aubio/aubio — GPLv3, "contact the author" for commercial use, no public commercial pricing found. HIGH confidence.
- https://github.com/adamstark/BTrack — GPLv3. HIGH confidence (GitHub repo license).
- https://github.com/c4dm/nnls-chroma + code.soundsoftware.ac.uk/projects/nnls-chroma — GPLv2-or-later. HIGH confidence.
- https://essentia.upf.edu/licensing_information.html — AGPLv3 default, commercial license available via MTG-UPF direct negotiation. HIGH confidence (official Essentia licensing page).
- https://docs.juce.com/master/classDragAndDropContainer.html — `performExternalDragDropOfFiles` is `static`, confirmed signature. HIGH confidence (official docs).
- JUCE forum threads on MIDI drag-to-DAW (forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816, .../draganddropcontainer-in-logic-pro-macos-12-2-1/50467) — temp-file `.mid` drag pattern is the community-standard approach; known host-specific compatibility issues reported (Logic Pro regression). MEDIUM confidence (community reports, not official JUCE guarantee — flagged as a testing risk, not a blocker).
- JUCE forum: "Our first commercial plugin using ONNX" (forum.juce.com/t/our-first-commercial-plugin-using-onnx/58195) and github.com/leocolliz/Audio-pattern-detection-juce-plugin — ONNX Runtime + JUCE integration pattern (dynamic linking preferred). MEDIUM confidence (real shipped-product community reports, not official ONNX/JUCE joint documentation).
- https://github.com/Tracktion/pluginval — strictness levels, JUCE 8 support, tool's own GPLv3 license (does not propagate to validated plugins since it's an external validator process). HIGH confidence (official repo + README).
- https://github.com/sudara/pamplejuce — reference template confirming current community-standard combination: JUCE 8 (submodule) + Catch2 v3 (FetchContent) + pluginval + CMake 3.25+. MEDIUM-HIGH confidence (widely adopted community template, not official JUCE deliverable).
- https://melatonin.dev/blog/how-to-code-sign-and-notarize-macos-audio-plugins-in-ci/ + KVR Audio "HOWTO macOS notarization" thread — code signing/notarization process stable since `notarytool` (2022); note-only per PROJECT.md scope. MEDIUM confidence (community + Apple-adjacent sources, not Apple's own docs directly fetched).

---
*Stack research for: commercial JUCE/C++ audio plugin (chord detection + MIDI generation)*
*Researched: 2026-07-12*
