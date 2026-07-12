# Phase 3: Core Chord-Detection Engine - Research

**Researched:** 2026-07-12
**Domain:** In-house music-information-retrieval (MIR) DSP pipeline — chromagram/CQT extraction, autocorrelation+DP beat tracking, Krumhansl-Schmuckler key detection, template+Viterbi chord recognition — implemented from published algorithms against MIT/BSD primitives only (no GPL/AGPL code or ports)
**Confidence:** MEDIUM-HIGH (JUCE APIs and constant-q-cpp API verified directly against source; Ellis 2007 beat-tracker parameters verified against the primary paper's equations; Krumhansl-Kessler key profile values cross-verified against two independent sources; chord-template/Viterbi self-transition values are literature-typical ranges, not a single canonical number — flagged as tunable)

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-------------------|
| ANL-01 | Key of the analyzed region is detected automatically | Krumhansl-Kessler/Temperley 24-profile correlation over accumulated harmonic chroma (see "Key Detection"). `KeyDetector` module + `KeyResult` in `AnalysisResult`. |
| ANL-02 | Tempo (BPM) and bar grid are detected automatically | Ellis (2007) onset-envelope + autocorrelation tempo estimate + DP beat backtrace (see "Beat & Tempo Detection"), with explicit v1 4/4-assumption bar grid (every 4th beat = downbeat). `TempoBeatTracker` module. |
| ANL-03 | Chord progression detected (chromagram + template matching + Viterbi smoothing), bar-aligned | CQT-based dual (harmonic+bass) chroma → beat-synchronized averaging → 36-template (maj/min/dom7) scoring with bass-root bias → Viterbi decode with self-transition bias (see "Chroma Extraction" and "Chord Recognition"). `ChromaExtractor` + `ChordTemplates` + `ChordDecoder` modules. |
| ANL-06 | Detection engine sits behind a swappable `ChordAnalyzer` interface | Pure-virtual `ChordAnalyzer` interface refined from ARCHITECTURE.md's sketch to depend only on `juce_core`/`juce_audio_basics` types (no GUI), with an abstract `CancelToken` + `ProgressCallback` instead of a direct `juce::ThreadPoolJob&` dependency, so it is unit-testable with zero threading infrastructure (see "ChordAnalyzer Interface & AnalysisResult"). |
</phase_requirements>

## Summary

This phase has no safe off-the-shelf shortcut: every GPL-licensed MIR library that does this well (aubio, Essentia, NNLS Chroma/Chordino, BTrack, madmom) is legally excluded per STACK.md/PITFALLS.md, so the chromagram, beat tracker, key detector, and chord recognizer must all be built in-house from published, non-code sources. The good news is that this exact pipeline shape — CQT/chroma → beat-synchronized template matching → Viterbi smoothing, plus a separate onset-envelope → autocorrelation → dynamic-programming beat tracker — is one of the best-documented pipelines in MIR research, with concrete, reproducible parameters available directly from primary sources (Ellis 2007's beat tracker gives exact equations and numbers; Krumhansl & Kessler's 1982 key profiles are a fixed, republished 24-vector table; Bello & Pickens 2005 / Sheh & Ellis 2003's HMM chord recognizer is well characterized by the AudioLabs Erlangen FMP notebooks). The one MIT-licensed building block worth adding as a dependency, `constant-q-cpp` (already vetted in STACK.md), was read directly from its GitHub source in this research pass — its exact API (`CQParameters`, `CQSpectrogram`, `Chromagram`) is documented below with real method signatures, not paraphrased guesses.

The highest-leverage design decision is to **not** use `constant-q-cpp`'s high-level `Chromagram` wrapper (which force-folds octaves into a single fixed 12/36-bin vector), but its lower-level `CQSpectrogram`/`CQParameters` pair instead, which exposes per-bin magnitudes and a `getBinFrequency(bin)` accessor. This lets ChordAI implement two custom chroma folds from one CQT pass — a full-range "harmonic" chroma for chord-quality template matching and a restricted ~55–250 Hz "bass" chroma for root disambiguation (PITFALLS.md #2/#3) — plus a tuning-offset correction step (Harte & Sandler 2005-style cents histogram), all from a single CQT computation.

**Primary recommendation:** Two independent downsample/analysis paths off one mono buffer — an 8 kHz / 32 ms-window / 4 ms-hop mel-onset path (Ellis 2007's own exact parameters) driving tempo/beat detection, and an 11.025 kHz CQT path (C1–C8, 36 bins/octave via `constant-q-cpp`) driving chroma, key, and chord recognition — with a synchronous `ClassicDspChordAnalyzer : ChordAnalyzer` that takes a `ProgressCallback` + abstract `CancelToken` (not a JUCE threading type), so the whole pipeline is provably headless and unit-testable via Catch2/CTest before any UI/threading code exists.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `juce::dsp::FFT` | ships with JUCE 8.0.14 | STFT magnitude spectrogram for the Ellis onset-envelope path only | Already vetted in STACK.md; wraps Accelerate/vDSP on macOS. Confirmed API (read from `juce_FFT.h`): `FFT(int order)` (size = 2^order), `performFrequencyOnlyForwardTransform(float* inputOutputData, bool onlyCalculateNonNegativeFrequencies=false)` is the correct call for magnitude-only onset detection (no need for `performRealOnlyForwardTransform` + manual magnitude). |
| `juce::dsp::WindowingFunction<float>` | ships with JUCE 8.0.14 | Window for the onset-envelope STFT | Confirmed API: construct with `(size_t size, WindowingMethod, bool normalise=true)`; `hann` is the standard choice for STFT-magnitude analysis (Ellis's paper does not name a window explicitly — this is a reasoned standard-practice default, not quoted from the paper). |
| `juce::WindowedSincInterpolator` | ships with JUCE 8.0.14 (`juce_audio_basics`) | Downsampling native-rate mono audio to the two internal analysis rates | Confirmed via source read (`juce_Interpolators.h`): a 200-tap (100 zero-crossing) Hann-windowed sinc kernel, JUCE's own doc comment calls it "recommended for high quality resampling." **Recommended over `LagrangeInterpolator`** (5-tap, `algorithmicLatency=2`) specifically because this phase downsamples ~4x (44.1/48kHz → ~11kHz) from a full commercial mix; Lagrange's short kernel has materially worse stopband rejection, and aliasing energy folding back into the 32Hz–4.2kHz CQT range would directly cause PITFALLS.md #2's chroma-noise failure mode. This is a reasoned recommendation (not sourced from an external "use WindowedSinc for downsampling" claim) — acceptable because analysis is offline/background, so the extra CPU cost of the larger kernel doesn't matter. |
| `constant-q-cpp` (Chris Cannam / QMUL) | pin a commit on `master` (no tagged releases) | Constant-Q Transform → CQT magnitude columns, used as the input to chroma extraction | Already vetted MIT-style-licensed in STACK.md. **Use `CQSpectrogram` + `CQParameters` directly (`cq/CQSpectrogram.h`, `cq/CQParameters.h`), not the `Chromagram` wrapper** — see "Architecture Patterns" for why. Verified by reading the actual header files from `github.com/cannam/constant-q-cpp` at research time (not paraphrased from a summary). |
| Catch2 v3.7.1 + CTest | already wired (Phase 2) | Unit/regression tests for every DSP module | Existing project convention: `TEST_CASE ("<ModuleTests>.<CaseName>", "[tag]")`, discovered via `catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.")`, binary at `build/ChordAITests_artefacts/Debug/ChordAITests` (confirmed by running `ctest -N` in this research pass — 15 existing tests). |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| — (in-house, ~50-150 lines) | — | Ellis (2007) onset-envelope + autocorrelation tempo estimate + DP beat backtrace | Core of ANL-02; see exact equations below. No dependency — this is the published-algorithm-not-code pattern STACK.md already committed to. |
| — (in-house, ~30-60 lines) | — | Krumhansl-Kessler/Temperley 24-key-profile Pearson correlation | Core of ANL-01; the profile values themselves are the "library" (a fixed 24-float table), not code to import. |
| — (in-house, ~100-200 lines) | — | Binary chord templates (maj/min/dom7) + beat-sync chroma averaging + bass-root-biased scoring + Viterbi decode | Core of ANL-03; informed by Bello & Pickens 2005 / Sheh & Ellis 2003 (templates+HMM) and Mauch & Dixon 2010 ("Approximate note transcription for the improved identification of difficult chords" — bass/root disambiguation), reference-only per STACK.md's licensing gate. |
| — (in-house, ~20-40 lines) | — | Median-filter harmonic/percussive suppression on CQT magnitude columns (Fitzgerald 2010-style) | Percussion-suppression step feeding chroma extraction (PITFALLS.md #2). Small, well-understood algorithm (median filter along the time axis of each frequency bin), not the full source-separation library some implementations wrap it in. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `constant-q-cpp`'s `CQSpectrogram`+manual folding | `constant-q-cpp`'s `Chromagram` wrapper | Wrapper is less code (no manual bin→pitch-class folding needed) but forces uniform-octave folding with no way to build a frequency-restricted bass chroma or apply a custom tuning correction inline — would require running CQT twice (once full-range, once bass-range) instead of once. Not recommended for this project's explicit bass-chroma requirement. |
| STFT-linear chroma (naive Fujishima 1999 binning) | Raw `juce::dsp::FFT`-only chroma (no CQT) | Simpler (no extra dependency) but PITFALLS.md #2 explicitly calls out linear-FFT chroma as the "hello world" pipeline that fails on real mixed masters — log-frequency/CQT binning is directly recommended there. Not used. |
| Binary (0/1) chord templates | Gaussian/EM-trained templates (Bello & Pickens' fuller method) | EM training needs a labeled chord corpus, which introduces its own dataset-licensing question and is out of scope for a from-scratch v1; binary templates are the well-documented, zero-training-data baseline (Sheh & Ellis 2003's simpler approach). Revisit only if v1 accuracy is below target. |
| Bar grid = "assume 4/4, every 4th beat" | True downbeat/meter detection (spectral-flux periodicity at bar level, or Goto & Muraoka-style chord-change-rate cross-check) | Real downbeat detection is a materially harder, less-published-with-parameters problem (even AudioLabs Erlangen's own FMP notebook explicitly notes measure-level tracking "fails" on their own worked example in triple meter). Deferred — see "Open Questions". |

**Installation (new CMake wiring needed — not yet present in `CMakeLists.txt`):**
```cmake
FetchContent_Declare(constant_q_cpp
    GIT_REPOSITORY https://github.com/cannam/constant-q-cpp.git
    GIT_TAG        <pin to a specific commit SHA, not master, before this phase ships>)
FetchContent_MakeAvailable(constant_q_cpp)
# constant-q-cpp ships plain .cpp/.h files under cq/ and src/, no CMakeLists.txt of its own
# (verified: repo root only has Makefile.* for linux/osx/mingw, no CMakeLists.txt) —
# add its src/*.cpp + src/dsp/*.cpp + src/ext/kissfft/*.c as an INTERFACE or STATIC target manually.
```
This is a **Wave 0 gap** (see Validation Architecture) — no CMake integration exists yet for this dependency, and because it has no upstream `CMakeLists.txt`, ChordAI's own build must declare the source-file list explicitly.

## Architecture Patterns

### Recommended Project Structure

```
Source/Analysis/
├── ChordAnalyzer.h                 # abstract interface: analyse(), ProgressCallback, CancelToken
├── AnalysisResult.h                # immutable value structs: ChordSymbol, ChordSegment, KeyResult, AnalysisResult
├── AudioPreprocessing.h/.cpp       # mono downmix + dual WindowedSincInterpolator resample (8kHz + 11025Hz paths)
├── ConstantQAnalysis.h/.cpp        # thin wrapper: CQParameters/CQSpectrogram -> raw per-bin magnitude columns
├── TuningEstimator.h/.cpp          # Harte & Sandler-style cents-histogram tuning offset estimate
├── HarmonicPercussiveFilter.h/.cpp # median-filter (Fitzgerald 2010-style) percussion suppression on CQT columns
├── ChromaExtractor.h/.cpp          # folds CQT bins -> harmonic (full-range) + bass (55-250Hz) 12-bin chroma
├── OnsetEnvelope.h/.cpp            # Ellis (2007) mel-spectrogram onset strength envelope
├── TempoBeatTracker.h/.cpp         # Ellis (2007) autocorrelation tempo + DP beat backtrace + 4/4 bar grid
├── KeyDetector.h/.cpp              # Krumhansl-Kessler/Temperley 24-profile correlation
├── ChordTemplates.h                # 36 binary templates (12 maj + 12 min + 12 dom7), pitch-class helpers
├── ChordDecoder.h/.cpp             # beat-sync chroma averaging + bass-root bias + Viterbi decode + segment merge
└── ClassicDspChordAnalyzer.h/.cpp  # orchestrates the above behind ChordAnalyzer; synchronous, reports progress
```

Matches and extends the `Analysis/` folder ARCHITECTURE.md already sketched (that document's `Chromagram.*`/`ChordHmm.*`/`BeatTracker.*` map onto `ChromaExtractor`/`ChordDecoder`/`TempoBeatTracker` above, split further because each stage now has a concrete, independently-testable algorithm behind it).

### Pattern 1: Dual-rate preprocessing, single mono buffer

**What:** Downmix the input `juce::AudioBuffer<float>` to mono once (average all channels), then resample it twice with two independent `WindowedSincInterpolator` instances — once to 8000 Hz for the onset/tempo path (matching Ellis 2007's own parameters exactly), once to 11025 Hz for the CQT/chroma path (C1 32.7Hz to C8 4186.01Hz comfortably fits under the 5512.5Hz Nyquist).
**When to use:** Always, as the first pipeline stage. The two paths do not depend on each other and could run in parallel later, but for v1's synchronous engine they simply run sequentially.
**Trade-offs:** Two resample passes cost more CPU than one, but each pass is tuned to what its downstream algorithm actually needs (Ellis needs 8kHz for his mel-band scheme to work as published; the CQT needs higher bandwidth for the top of the chord range) — using one compromise rate for both would deviate from the verified parameters of both algorithms.

### Pattern 2: CQT-then-fold, not chroma-wrapper

**What:** Construct one `CQSpectrogram` (via `CQParameters(11025.0, /*minFreq*/32.70, /*maxFreq*/4186.01, /*binsPerOctave*/36)`, defaults otherwise: `q=1.0`, `atomHopFactor=0.25`, `threshold=0.0005`, `window=SqrtBlackmanHarris`, `decimator=BetterDecimator`). Feed it the whole 11025Hz mono buffer via `process()` + a final `getRemainingOutput()` (verified streaming pattern from `CQSpectrogram.h`: "Any samples left over ... are saved for the next call"). Each output `RealColumn` is one CQT time-frame; `getBinFrequency(bin)` (verified in `CQBase.h`, "does not have to be an integer") gives the exact Hz for any bin index, which is what makes the frequency-restricted bass fold possible.
**When to use:** Once per analysis run — the only CQT computation needed; both harmonic and bass chroma are folded from the same magnitude columns.
**Trade-offs:** More manual code than the `Chromagram` wrapper (must write the bin→pitch-class fold, tuning-offset rotation, and frequency-range masking by hand), but this is the only way to get a bass-restricted chroma without a second CQT pass, and to apply a custom tuning correction before folding rather than after.

**Example (bin → pitch-class mapping, standard MIDI/A440 convention, tuning-offset `thetaCents` applied):**
```cpp
// Source: derived from CQBase::getBinFrequency() (constant-q-cpp, verified from source)
//         + standard 12-TET pitch-class convention (A440 = MIDI 69, C = pitch class 0)
int pitchClassForBin (const CQSpectrogram& cq, int bin, double thetaCents)
{
    double freqHz = cq.getBinFrequency ((double) bin);
    double correctedA4 = 440.0 * std::pow (2.0, thetaCents / 1200.0);
    double midiNote = 69.0 + 12.0 * std::log2 (freqHz / correctedA4);
    int pitchClass = ((int) std::lround (midiNote)) % 12;
    return pitchClass < 0 ? pitchClass + 12 : pitchClass;
}
```

### Pattern 3: Two chroma variants from one fold pass, frequency-masked

**What:** While folding CQT bins into 12-dim vectors, maintain two accumulators per frame: `harmonicChroma[12]` (all bins with `freqHz >= ~80 Hz`, i.e. excluding sub-bass/rumble per PITFALLS.md #2) and `bassChroma[12]` (only bins with `55 Hz <= freqHz <= 250 Hz`, the exact range PITFALLS.md #3 specifies). Both are L2-normalized per frame (with a floor threshold — if the vector norm is near zero, treat as silence rather than dividing by ~0, matching the FMP notebook's "thresholded normalization to avoid division by zero").
**When to use:** Every CQT output frame, before beat-synchronized averaging.
**Trade-offs:** None significant — this is "free" once the CQT columns and `getBinFrequency` are available; it's the direct implementation of two already-decided pitfalls-doc recommendations.

### Pattern 4: Beat-synchronized chroma averaging before Viterbi (not frame-level)

**What:** Once beat times are known (from the independent tempo/beat path), average all `harmonicChroma`/`bassChroma` frames whose timestamp falls within `[beat[i], beat[i+1])` into one beat-synchronous chroma pair per beat interval. Feed only this beat-rate sequence to chord template scoring + Viterbi.
**When to use:** Always — this is PITFALLS.md #4's explicit fix for chord-segmentation flicker, and it also collapses the analysis from CQT-frame-rate (order of hundreds of frames for a whole song) down to beat-rate (order of a few hundred beats for a 3-minute song at typical tempos), which is what keeps Viterbi's O(states² × frames) cost trivial.
**Trade-offs:** Chord boundaries can now only fall on detected beat boundaries — acceptable and desired (PITFALLS.md #4 explicitly wants this), but means any beat-detection error (Pattern 5 mitigates) directly limits chord-boundary accuracy too.

### Pattern 5: Octave-error-aware tempo estimation (Ellis 2007, exact equations)

**What:** From the 8kHz onset-envelope path (see "Beat & Tempo Detection" below for exact numbers), compute tempo period strength via autocorrelation weighted by a log-Gaussian centered on 120 BPM, then explicitly check `(0.33, 0.5, 2, 3) ×` the primary estimate for a stronger peak before committing — this is Ellis's own published fix for the human two-metrical-level ambiguity, and directly addresses PITFALLS.md #5's tempo octave-error concern.
**When to use:** Always, as the tempo-estimation step before the DP beat backtrace.
**Trade-offs:** None — this is a verified, cheap (couple of vector operations), already-published mitigation; skipping it would reintroduce exactly the risk PITFALLS.md #5 flags.

### Anti-Patterns to Avoid

- **Running Viterbi directly on CQT-frame-rate chroma (no beat-sync):** Reproduces PITFALLS.md #4's flicker — beat-synchronize first (Pattern 4).
- **Trusting the raw autocorrelation peak as tempo without the 120 BPM perceptual-weighting + octave cross-check:** Reproduces PITFALLS.md #5 — always apply Ellis's `W(τ)` weighting and secondary-tempo check.
- **Computing chroma from `juce::dsp::FFT` linear bins instead of CQT:** Reproduces PITFALLS.md #2's noisy/flickering chord labels on real mixed masters — CQT is mandatory for the harmonic/bass chroma paths (linear FFT is fine, and used, for the separate onset-envelope path only, where log-frequency resolution doesn't matter).
- **Depending on `juce::ThreadPoolJob&` directly inside `ChordAnalyzer::analyse()`:** Couples the interface to a JUCE-threading type the Phase 3 test harness shouldn't need to construct — use the abstract `CancelToken` (see next section) so a bare Catch2 test can pass a trivial no-op token.

## ChordAnalyzer Interface & AnalysisResult

Refines ARCHITECTURE.md's `ChordAnalyzer` sketch (which passed a `juce::ThreadPoolJob&` directly) to satisfy this phase's explicit success criterion 4 ("invocable and verifiable through a `ChordAnalyzer` interface via a standalone test harness, independent of any UI") and the additional requirement for a progress callback usable by Phase 4's UI:

```cpp
// Source/Analysis/ChordAnalyzer.h — pure C++, only juce_core/juce_audio_basics types, no GUI dependency
#pragma once
#include <JuceHeader.h>
#include "AnalysisResult.h"

class ChordAnalyzer
{
public:
    virtual ~ChordAnalyzer() = default;

    // Abstract cancellation contract: decouples the interface from any specific
    // threading primitive. Phase 3 test harness passes a trivial always-false
    // token; Phase 4's AnalysisPipeline (juce::ThreadPoolJob) implements a
    // one-line adapter: `bool shouldCancel() const override { return job.shouldExit(); }`
    struct CancelToken
    {
        virtual ~CancelToken() = default;
        virtual bool shouldCancel() const = 0;
    };

    // fractionComplete in [0,1]; stage is a short label ("decoding","chroma","beat","key","chords")
    // for Phase 4's progress UI. May be called from whatever thread analyse() runs on —
    // caller (Phase 4) is responsible for marshalling to the message thread.
    using ProgressCallback = std::function<void (double fractionComplete, const juce::String& stage)>;

    // Synchronous. audio is already decoded (LoadedAudio::buffer); analyse() does its own
    // mono downmix + resample internally. Returns a fully-populated AnalysisResult, or a
    // result with chords.empty() if cancelled mid-run (never throws, never asserts on bad input
    // — matches existing loadAudioFileSync() convention in Source/Import/AudioFileLoader.h).
    virtual AnalysisResult analyse (const juce::AudioBuffer<float>& audio,
                                     double sampleRate,
                                     juce::Range<double> regionSeconds,
                                     const ProgressCallback& onProgress,
                                     const CancelToken& cancelToken) = 0;
};
```

```cpp
// Source/Analysis/AnalysisResult.h — immutable value types, safe to publish via
// std::atomic<std::shared_ptr<const AnalysisResult>> per ARCHITECTURE.md Pattern 2 (Phase 4)
#pragma once
#include <JuceHeader.h>
#include <vector>

enum class ChordQuality { Major, Minor, Dominant7, NoChord };

// pitchClass: 0=C, 1=C#, 2=D, ... 11=B (standard 12-TET / A440 convention)
struct ChordSymbol
{
    int pitchClass = 0;
    ChordQuality quality = ChordQuality::NoChord;
};

struct ChordSegment
{
    ChordSymbol chord;
    double startSeconds = 0.0, endSeconds = 0.0;
    int startBeatIndex = 0, endBeatIndex = 0;   // indices into AnalysisResult::beatTimesSeconds
    float confidence = 0.0f;                     // Viterbi/template match score, 0..1
};

struct KeyResult
{
    int tonicPitchClass = 0;   // 0=C..11=B
    bool isMajor = true;
    float confidence = 0.0f;   // normalized Krumhansl-Kessler correlation margin
};

struct AnalysisResult
{
    double sampleRate = 0.0;
    juce::Range<double> analyzedRegionSeconds;

    double bpm = 0.0;
    std::vector<double> beatTimesSeconds;      // full beat grid
    std::vector<int> barStartBeatIndices;       // v1: every 4th beat index (4/4 assumption)

    KeyResult key;
    std::vector<ChordSegment> chords;           // beat-boundary-aligned, adjacent-equal merged

    bool wasCancelled = false;
};
```

**Threading split confirmed:** Per ARCHITECTURE.md's own suggested build order (#3) and Anti-Pattern 1/5, `ClassicDspChordAnalyzer::analyse()` is fully synchronous — it does not spawn threads, does not touch `juce::MessageManager`, and has no JUCE-GUI dependency. Phase 4 wraps a call to it inside `AnalysisPipeline : juce::ThreadPoolJob` (already sketched in ARCHITECTURE.md Pattern 2) and supplies the `ThreadPoolJob`-backed `CancelToken` adapter. This phase's test harness therefore never needs a `ThreadPool`, `MessageManager`, or plugin editor to exercise the full pipeline — only a decoded `juce::AudioBuffer<float>` (reusable from Phase 2's `AudioFileLoader`/synthetic fixtures) and a no-op `CancelToken`/`ProgressCallback`.

## Pipeline Stage 1: Preprocessing

| Stage | Parameter | Value | Source/Confidence |
|-------|-----------|-------|--------------------|
| Mono downmix | Method | Average all input channels | Standard practice; STACK.md/ARCHITECTURE.md already assume mono downmix. HIGH (uncontested convention). |
| Resample (onset path) | Target rate | 8000 Hz | Matches Ellis (2007) §3.1 exactly ("input sound is resampled to 8 kHz"). HIGH (primary source, verified from PDF). |
| Resample (chroma path) | Target rate | 11025 Hz | Within STACK.md's stated "~11–22 kHz internal rate is fine" budget; chosen at the low end to minimize CQT compute for a 6-octave-plus span while keeping Nyquist (5512.5Hz) comfortably above C8 (4186.01Hz). MEDIUM (reasoned choice within an explicitly-approved range, not itself sourced). |
| Interpolator | `juce::WindowedSincInterpolator` | 200-tap windowed sinc, `process(speedRatio, in, out, numOut)` | HIGH (verified from `juce_Interpolators.h`/`juce_GenericInterpolator.h` source). Reset between independent buffers via `.reset()` (stateful, per JUCE doc comment). |

## Pipeline Stage 2: Chroma Extraction

| Stage | Parameter | Value | Source/Confidence |
|-------|-----------|-------|--------------------|
| CQT engine | `CQParameters(sampleRate, minFrequency, maxFrequency, binsPerOctave)` | `(11025.0, 32.70 /*C1*/, 4186.01 /*C8*/, 36)` | HIGH (constructor signature verified from `cq/CQParameters.h` source read). Constructor comment: "actual minimum will normally be calculated as a round number of octaves below the maximum frequency" — do not assume `getMinFrequency()` returns exactly 32.70 at runtime; query it. |
| CQT defaults kept | `q=1.0, atomHopFactor=0.25, threshold=0.0005, window=SqrtBlackmanHarris, decimator=BetterDecimator` | library defaults | HIGH (verified from source); `BetterDecimator` explicitly chosen over `FasterDecimator` since this phase's performance budget is "seconds, not real-time." |
| Frame processing | `CQSpectrogram::process(RealSequence)` + final `getRemainingOutput()` | Streaming call pattern | HIGH (verified from `CQSpectrogram.h` doc comments — "leftover samples ... saved for next call," `getRemainingOutput()` flushes at end). |
| Frame rate | `getColumnHop()` samples at 11025Hz | Not hand-computed here — query at runtime, don't hardcode | HIGH-confidence *method*, MEDIUM on exact resulting value (depends on `atomHopFactor`/octave count interaction internal to the library) — expect on the order of 10-50ms/frame; verify empirically in Wave 0 and assert a sane range in a unit test rather than a hardcoded frame count. |
| Bins/octave choice (36) | 3 bins/semitone = ~33.3 cents/bin, ~±16.7 cents resolution | Matches `librosa.feature.chroma_cqt`'s own default (`bins_per_octave=36`) | MEDIUM-HIGH (cross-verified: constant-q-cpp's `Chromagram::Parameters` default is also `binsPerOctave=36`; librosa's independent default agrees). Sufficient resolution for the requested ±50 cents tuning-drift correction. |
| Tuning estimation | Weighted cents-offset histogram across strong frames, informed by Harte & Sandler (2005) "Automatic Chord Identification Using a Quantised Chromagram" | For each frame's dominant bins, accumulate `(midiNote - round(midiNote)) * 100` cents into a histogram weighted by magnitude; take the circular-weighted-mean offset as `thetaCents`; re-fold using the `pitchClassForBin(..., thetaCents)` formula in Pattern 2 | MEDIUM (paper's existence/approach confirmed via search — "compensates for possible mistuning ... by reallocating peaks based on peak distribution" — exact histogram bucketing not independently re-derived from the primary source PDF in this pass; treat the concrete formula above as this project's own reasoned implementation of the published concept, validate against a deliberately-detuned synthetic fixture in Wave 0). |
| Percussion suppression | Median filter along time axis of each CQT frequency bin (Fitzgerald 2010-style harmonic enhancement) | Kernel length ~200-400ms of frames (convert using measured `getColumnHop()`); apply to CQT magnitude columns before pitch-class folding | MEDIUM (Fitzgerald 2010's median-filtering HPSS approach confirmed as the standard lightweight technique in this space; exact kernel-length parameter is this project's own tuning choice, not quoted from the paper — validate on a synthetic fixture with an injected percussive click track, per Wave 0). |
| Harmonic chroma frequency mask | Include bins with `freqHz >= ~80 Hz` | Excludes sub-bass/rumble from the chord-quality chroma | MEDIUM (direct implementation of PITFALLS.md #2's explicit recommendation; the 80Hz cutoff itself is a reasoned choice, tune empirically). |
| Bass chroma frequency mask | Include bins with `55 Hz <= freqHz <= 250 Hz` | Feeds root-disambiguation bias only, not exposed as inversions in v1 output | HIGH for the range itself (verbatim from this project's own PITFALLS.md #3), MEDIUM for how it's used (see "Chord Recognition" below — this project's own scoring-bias design, informed-by-not-copied-from Mauch & Dixon 2010). |
| Per-frame normalization | L2-normalize, with a floor threshold before dividing (treat near-zero-norm frames as silence, don't divide by ~0) | Matches FMP notebook's "thresholded normalization to avoid division by zero" | MEDIUM-HIGH (directly quoted concept from AudioLabs Erlangen FMP C5S3 notebook, fetched in this research pass). |

## Beat & Tempo Detection (Ellis 2007 — exact parameters)

Source: Ellis, D.P.W. "Beat Tracking by Dynamic Programming," *Journal of New Music Research* 36(1), 2007 — PDF read directly in this research pass (not a secondary summary). HIGH confidence for every number below; this is the primary, peer-reviewed source.

### Onset strength envelope (§3.1)
1. Resample input to 8 kHz mono.
2. STFT magnitude, **32 ms window, 4 ms hop** (both exact at 8kHz = 256-sample window / 32-sample hop — conveniently already a power of two, `juce::dsp::FFT` order 8).
3. Map to **40 Mel bands** via weighted summing of the spectrogram (auditory frequency scale).
4. Convert Mel spectrogram to dB.
5. First-order difference along time in each band; **half-wave rectify** (negative values → 0); sum the positive differences across all 40 bands → raw onset strength.
6. High-pass filter, cutoff **~0.4 Hz**, to make the envelope locally zero-mean.
7. Smooth by convolving with a **~20 ms-wide Gaussian**.
8. Normalize per-excerpt by **dividing by the envelope's own standard deviation** (accounts for instrumentation/spectrum differences between songs).

### Global tempo estimate (§3.2)
- Autocorrelate the onset envelope: `TPS(τ) = W(τ) · Σ_t O(t)O(t-τ)`.
- Perceptual weighting: `W(τ) = exp{ -½ (log2(τ/τ0) / στ)² }`, with **τ0 = 0.5s (120 BPM)** and **στ = 1.4 octaves** (both are the paper's own tuned-against-human-tapping-data values — MIREX-06 training set, 77% agreement).
- Primary tempo estimate = τ maximizing `TPS(τ)`.
- **Octave-error mitigation (explicit, built into the algorithm):** compute `TPS` resampled to 1/2 and 1/3 length, add to the original, and choose the largest peak **across both** the original and resampled `TPS` — i.e., explicitly re-check `(0.33, 0.5, 2, 3)×` the primary estimate for a stronger candidate before committing. This directly satisfies PITFALLS.md #5's octave-error concern using the paper's own published fix, not an ad hoc patch.

### Dynamic-programming beat backtrace (§2)
- Objective: `C({tᵢ}) = Σ O(tᵢ) + α · Σ F(tᵢ - tᵢ₋₁, τp)` where `τp` is the target inter-beat interval from the tempo estimate.
- Transition penalty: `F(Δt, τ) = -(log(Δt/τ))²` — symmetric on a log-time axis (equal penalty for tempo doubling vs halving locally, which is what makes the DP itself robust to *local* octave slips even after the global tempo estimate is fixed).
- Recursion: `C*(t) = O(t) + max_τ { α·F(t-τ, τp) + C*(τ) }`, search range `τ = t - 2τp ... t - τp/2` (bounded search, not full history — keeps this O(N) not O(N²)).
- Backtrace from the largest final `C*` value back through recorded predecessors to recover the full beat sequence.
- Paper's own working code (Fig. 1, MATLAB) confirms the pattern is ~10 lines for the core forward pass + backtrace — directly portable to a small C++ function operating on `std::vector<double>`.
- `α` (transition-cost weight) and time-quantization step (paper used 4ms/250Hz) are implementation knobs — the paper doesn't state a single universal `α`; **librosa's own reimplementation of this exact algorithm defaults its analogous `tightness` parameter to 100** (cross-checked via librosa docs in this pass) — use as the v1 starting point, tune against synthetic fixtures.

### Bar grid (v1 explicit decision)
**Assume 4/4 time signature.** No downbeat/meter detection in v1 — bar boundaries = every 4th detected beat, with the first detected beat treated as downbeat 1 of bar 1. This is a stated, deliberate limitation (not an oversight): true downbeat detection is a harder, less parameter-published problem (even AudioLabs Erlangen's own FMP beat-tracking notebook, fetched in this pass, explicitly notes its own worked triple-meter example fails at measure-level tracking). `AnalysisResult::barStartBeatIndices` should still be a distinct field from `beatTimesSeconds` so a future true-downbeat detector can populate it independently without changing the struct shape.

## Key Detection (Krumhansl-Kessler / Temperley profiles)

Source: Krumhansl & Kessler (1982) probe-tone key profiles, as republished/cross-verified via two independent sources (`rnhart.net/articles/key-finding`, `mashav.com`'s Krumhansl-Schmuckler Praat script docs) in this research pass. MEDIUM-HIGH confidence (both sources agree on all 24 values; this is the standard, widely-republished K-K table, not independently re-derived from Krumhansl's 1990 book in this pass).

**Major profile** (index 0=tonic..11, semitone steps):
`[6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]`

**Minor profile:**
`[6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]`

**Algorithm:**
1. Accumulate one global 12-bin chroma vector for the whole analyzed region — sum (or energy-weighted mean) of the `harmonicChroma` frames from Pipeline Stage 2, tuning-corrected, over the entire region (not beat-synchronized; key detection wants maximum data, not per-beat granularity).
2. For each of the 24 candidate keys (12 tonics × {major, minor}), rotate the corresponding profile to that tonic and compute the **Pearson correlation** between the rotated profile and the accumulated chroma vector.
3. Pick the key with the highest correlation; `KeyResult::confidence` = normalized margin between the best and second-best correlation (a large gap = confident; a narrow gap = ambiguous, worth surfacing honestly rather than hiding).

## Chord Recognition (templates + beat-sync + bass bias + Viterbi)

### v1 chord vocabulary — explicit decision
**12 major + 12 minor + 12 dominant-7th = 36 chord classes.** No maj7/min7/sus/dim/aug, no inversions/slash chords in v1's detected output (all chords always labeled in root position, e.g. always "Am" never "Am/C" even if the bass chroma suggests an inversion). No-chord ("N", for intros/outros/pure-percussion) is **not** a 37th HMM state — it's applied as a deterministic post-Viterbi override: if a beat-synchronized `harmonicChroma` vector's pre-normalization L2 norm is below an empirically-tuned floor, force that beat's segment to `ChordQuality::NoChord` regardless of the Viterbi state.

Rationale: REQUIREMENTS.md's ANL-05 UI example ("Am, Cmaj7, F/A") belongs to Phase 4 (display), not this phase's ANL-03 success criterion, which only requires "a chord progression ... aligned to the bar grid," not a specific quality vocabulary. Keeping v1's template set small directly reduces template confusability under real-mix noise (PITFALLS.md #2) and keeps the Viterbi state space tiny (36² transitions, trivially fast). This is a **decision, not a default** — flagged explicitly per the research brief's instruction; document in STATE.md if the planner or a later phase revisits it. maj7/min7/sus/dim and inversions are the natural v1.1/v2 extension path once real-track accuracy is measured against PITFALLS.md #9's ~75-80% ceiling.

### Binary templates (Sheh & Ellis 2003 / Bello & Pickens 2005 baseline, confirmed via AudioLabs Erlangen FMP notebook fetch)
For root pitch class `r` (0=C..11=B):
- **Major:** active = `{r, (r+4)%12, (r+7)%12}` (root, major third, perfect fifth)
- **Minor:** active = `{r, (r+3)%12, (r+7)%12}` (root, minor third, perfect fifth)
- **Dominant 7th:** active = `{r, (r+4)%12, (r+7)%12, (r+10)%12}` (root, major third, fifth, minor seventh)

Templates are binary (1 for active pitch classes, 0 otherwise), L2-normalized. This is confirmed as the standard, simplest baseline directly from the FMP notebook fetch in this pass ("a simple binary mask chord model assigns an amplitude of 1 to the chromas defining the chord ... 0 to the other chromas ... For C major an amplitude of 1 is given to C, E and G").

### Observation scoring — cosine similarity + bass-root bias
```
rawScore(template_c, beatChroma) = cosine(harmonicChroma_beat, template_c)
                                  + γ · bassChroma_beat[root(c)] / (Σ bassChroma_beat + ε)
```
where `γ ≈ 0.2-0.3` (tunable) is a small bonus weighting the bass chroma's energy at the template's root pitch class. This is this project's own synthesis — informed by, not copied from, Mauch & Dixon's ISMIR 2010 paper "Approximate note transcription for the improved identification of difficult chords" (confirmed to exist and address exactly this bass/root disambiguation problem; exact formula not independently verified from the paper text in this pass, so treat as MEDIUM confidence / a documented hypothesis to validate against the bass-inversion synthetic fixtures in Wave 0) — and directly implements PITFALLS.md #3's requirement that bass chroma "bias/verify root selection ... not just the full-mix chroma."

Per the FMP notebook's confirmed approach: column-wise **L1-normalize** the 36 raw scores per beat frame to form the Viterbi observation-likelihood matrix `B(i, n)`.

### Viterbi transition matrix — self-transition bias
Uniform transition matrix with self-transition weight **β**, all other transitions `(1-β)/35`:
```
A(i,j) = β        if i == j
A(i,j) = (1-β)/35 otherwise
```
Literature-reported self-transition values for chord HMMs cluster in the **0.8–0.99** range (cross-referenced via multiple implementation write-ups in this pass — no single canonical number). **Recommend β = 0.85 as the v1 starting point**, applied at **beat-synchronized** granularity (not raw-frame granularity, per Pattern 4) — beat-sync input already removes most flicker, so v1 doesn't need the more extreme values (0.95-0.99) seen in frame-level implementations. Flag as a tunable hyperparameter; validate against the synthetic progression fixtures (a progression with 1-chord-per-bar vs. a faster 1-chord-per-beat progression should both decode correctly at the chosen β).

Standard/log-domain Viterbi decode (for numerical stability with many beats) over the beat-rate likelihood sequence, uniform initial-state prior (1/36).

### Segment construction
Merge consecutive equal-chord Viterbi states into `ChordSegment`s; `startSeconds`/`endSeconds` and `startBeatIndex`/`endBeatIndex` come directly from the beat grid (chord boundaries always land exactly on beat boundaries by construction — this **is** what "aligned to the bar grid" means operationally: every chord change coincides with a beat, and bars are the every-4th-beat overlay from the tempo/beat stage).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Constant-Q Transform kernel construction (sparse kernel matrix, per-octave decimation) | A custom CQT implementation | `constant-q-cpp`'s `ConstantQ`/`CQKernel`/`CQSpectrogram` (MIT-style, vetted in STACK.md) | Correct, numerically-stable CQT kernel generation (sparse kernel truncation, atom hop factors, multi-octave decimation filterbank) is a substantial, easy-to-get-subtly-wrong piece of DSP that this specific MIT-licensed library already solves — reinventing it duplicates exactly the effort STACK.md already spent vetting an alternative to the GPL options. |
| High-quality bandlimited resampling | A custom sinc/polyphase resampler | `juce::WindowedSincInterpolator` | Already ships with JUCE; correct anti-aliasing resampling has enough subtle failure modes (aliasing, DC offset, ringing) that hand-rolling it risks silently corrupting every downstream chroma bin. |
| FFT | A custom radix-2/mixed-radix FFT | `juce::dsp::FFT` (onset path) / `constant-q-cpp`'s bundled KissFFT (CQT path, internal to the library) | Already decided in STACK.md; wraps hardware-accelerated Accelerate/vDSP on macOS for the onset path. |

**Key insight:** Everything genuinely *algorithm*-shaped in this phase (Viterbi decode, key-profile correlation, chord templates, DP beat backtrace) is deliberately hand-rolled per STACK.md's licensing gate — that's correct and expected, not a shortcut to fix. The *transform/signal-processing primitives underneath* those algorithms (CQT, FFT, resampling) are exactly where "don't hand-roll" still applies, because permissively-licensed, already-vetted implementations exist for those specific pieces.

## Common Pitfalls

(This phase's dedicated risk areas are already fully catalogued in `.planning/research/PITFALLS.md` #1-#5; this section adds pipeline-specific implementation traps discovered during this research pass that aren't already covered there.)

### Pitfall: Treating `constant-q-cpp`'s streaming `process()`/`getRemainingOutput()` contract as a single-shot call
**What goes wrong:** Calling `process()` once with the whole buffer and ignoring `getRemainingOutput()` silently drops the tail of the song (the last partial CQT block never flushes).
**Why it happens:** The API is a streaming design (`process(RealSequence)` can be called multiple times with successive chunks) even though this phase's usage is a single "whole buffer at once" call — but the *tail* still needs an explicit flush.
**How to avoid:** Always call `getRemainingOutput()` after the single `process()` call and append its columns before treating the CQT output as complete. Unit-test explicitly: total output frame count should account for `getLatency()`/`getColumnHop()` covering the buffer's full duration, not just `bufferLength / columnHop` rounded down.
**Warning signs:** Detected chord progression is systematically short by a beat or two at the end of the song; last chord segment's `endSeconds` doesn't reach the region's actual end.

### Pitfall: Hardcoding CQT bin count / frame rate instead of querying the library
**What goes wrong:** Assuming `binsPerOctave × octaveCount` gives the exact total bin count, or assuming a specific hop-in-samples, when `CQParameters`' constructor comment explicitly states the actual min frequency (and therefore total octave/bin count) "may differ" from what was requested.
**How to avoid:** Always query `getTotalBins()`, `getMinFrequency()`, `getColumnHop()`, `getLatency()` at runtime after construction; assert sane ranges in a unit test rather than hardcoding expected values from hand-calculation.

### Pitfall: Two independent resample paths drifting in beat/frame timestamp alignment
**What goes wrong:** The onset/tempo path (8kHz) and the chroma path (11025Hz) are two separate resamples of the same source audio; if their frame timestamps aren't computed back to a common "seconds from region start" basis (accounting for each interpolator's own latency), beat-synchronized chroma averaging (Pattern 4) will silently misalign by tens of milliseconds.
**How to avoid:** Convert every frame index to absolute seconds via `frameIndex * (columnHop / analysisSampleRate)` (accounting for `getLatency()`), not via a shared frame-index assumption between the two paths. Unit-test with a synthetic click-track fixture where expected beat times are known exactly.

## Code Examples

### Chord template construction
```cpp
// Source/Analysis/ChordTemplates.h
#pragma once
#include <array>

enum class ChordQuality { Major, Minor, Dominant7, NoChord };

inline std::array<float, 12> buildTemplate (int root, ChordQuality quality)
{
    std::array<float, 12> t {};
    auto set = [&] (int semitoneOffset) { t[(root + semitoneOffset) % 12] = 1.0f; };
    set (0);                                   // root, all qualities
    set (quality == ChordQuality::Minor ? 3 : 4); // minor third vs major third
    set (7);                                   // perfect fifth
    if (quality == ChordQuality::Dominant7)
        set (10);                              // minor seventh
    // L2-normalize
    float norm = 0.0f;
    for (float v : t) norm += v * v;
    norm = std::sqrt (norm);
    for (float& v : t) v /= norm;
    return t;
}
```

### DP beat backtrace core (direct C++ port of Ellis 2007 Fig. 1's published MATLAB)
```cpp
// Source: Ellis (2007) "Beat Tracking by Dynamic Programming", Fig. 1 (verified from primary PDF)
// localScore: onset strength envelope; periodSamples: target inter-beat interval; alpha: transition weight
std::vector<int> dpBeatTrack (const std::vector<double>& localScore, double periodSamples, double alpha)
{
    const int n = (int) localScore.size();
    std::vector<double> cumScore = localScore;
    std::vector<int> backlink (n, -1);

    int lo = (int) std::round (-2.0 * periodSamples);
    int hi = (int) std::round (-0.5 * periodSamples);

    for (int i = 0; i < n; ++i)
    {
        double best = -1e300; int bestTau = -1;
        for (int d = lo; d <= hi; ++d)
        {
            int tau = i + d;
            if (tau < 0) continue;
            double f = -std::pow (std::log ((double) -d / periodSamples), 2.0); // F(delta,period)
            double score = alpha * f + cumScore[tau];
            if (score > best) { best = score; bestTau = tau; }
        }
        cumScore[i] = (bestTau >= 0 ? best : 0.0) + localScore[i];
        backlink[i] = bestTau;
    }

    int endIdx = (int) (std::max_element (cumScore.begin(), cumScore.end()) - cumScore.begin());
    std::vector<int> beats;
    for (int t = endIdx; t >= 0; t = backlink[t])
    {
        beats.push_back (t);
        if (backlink[t] < 0) break;
    }
    std::reverse (beats.begin(), beats.end());
    return beats;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|-------------------|---------------|--------|
| Linear-FFT chroma binning (Fujishima 1999) | Log-frequency/CQT-based chroma | Standard since ~2005 (Harte & Sandler) | This project already committed to CQT via STACK.md; confirmed as still the correct choice, no newer displacement found. |
| Hand-tuned chord HMMs (Bello & Pickens 2005, Sheh & Ellis 2003) | Deep-learning chord recognizers (CNN/CRNN, e.g. "Deep Chroma Extractor," structured CRF models) | ~2016 onward (ISMIR literature) | Explicitly **not applicable to v1** — GPL/AGPL/training-data licensing constraints and "no ML in v1" (ANL-07 is v2-deferred per REQUIREMENTS.md) rule out this whole newer branch of the literature for this phase. Noted for completeness only; do not let it influence v1 scope. |

**Deprecated/outdated:** None of the specific algorithms recommended here (Ellis 2007 beat tracker, Krumhansl-Kessler key profiles, template+HMM chord recognition) have been superseded by a newer *classical* (non-ML) approach with comparably well-published parameters — they remain the standard in-house-implementable baseline as of this research date.

## Open Questions

1. **Exact CQT frame rate / `getColumnHop()` value at the chosen parameters**
   - What we know: `getColumnHop()`/`getLatency()` are the correct runtime accessors (verified from source); order-of-magnitude expectation is 10-50ms/frame.
   - What's unclear: The precise value depends on `atomHopFactor`/octave-count interaction internal to the library, not hand-computable from the header alone without either running the library or reading its `.cpp` implementation in more depth than this pass covered.
   - Recommendation: First implementation task in Wave 0 should log/assert the actual `getColumnHop()`/`getTotalBins()`/`getMinFrequency()` values against a short known-duration test signal, and hardcode expectations nowhere else in the codebase.

2. **Downbeat/bar-grid accuracy on non-4/4 or syncopated material**
   - What we know: v1 explicitly assumes 4/4 and uses "every 4th beat" — a stated, deliberate limitation, not a bug.
   - What's unclear: How badly this degrades on 3/4 or 6/8 source material, or how often the *first* detected beat is genuinely the downbeat vs. an off-beat pickup.
   - Recommendation: Document this limitation directly in the phase's own test/README notes so Phase 4/6 (bar-aligned MIDI export) don't silently assume perfect bar-grid accuracy; revisit with real downbeat detection only if user feedback flags it (ties to PITFALLS.md #9's broader "communicate the domain ceiling" guidance).

3. **Bass-root-bias weight `γ` and self-transition `β` are both stated as starting points, not validated numbers**
   - What we know: Literature ranges (`β` 0.8-0.99) and a reasoned starting point for each (`β=0.85`, `γ≈0.2-0.3`).
   - What's unclear: The actual optimal values for this specific pipeline (CQT parameters, chord vocabulary, and bass-chroma formula are all this project's own combination, not a single paper's exact recipe) can only be determined empirically.
   - Recommendation: Expose both as named constants (not magic numbers inline) so they're trivially tunable once synthetic + real-track fixtures exist; do not treat the stated starting values as "correct," treat them as "a defensible place to start measuring."

4. **Real-track accuracy validation has no bundled ground-truth corpus yet**
   - What we know: Synthetic fixtures (Validation Architecture below) fully exercise the algorithm's correctness on deterministic input.
   - What's unclear: PITFALLS.md #9's ~75-80% real-world accuracy ceiling can only be confirmed against actual mixed-genre music with known ground truth, and no such fixture exists in the repo yet (well-known corpora like Isophonics' Beatles annotations pair with copyrighted audio that can't be bundled).
   - Recommendation: Treat real-track accuracy as a manual/exploratory verification step for this phase (drop in a handful of the developer's own royalty-cleared tracks and listen/compare), not a CTest-automatable regression gate in Phase 3 — see Validation Architecture "Manual" row.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 + CTest (already wired, confirmed via `ctest -N` — 15 existing tests, e.g. `ChordAITests.AudioFileLoaderTests.WavDecode`) |
| Config file | `CMakeLists.txt` (root) — `catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.")` |
| Quick run command | `cmake --build build --target ChordAITests && "./build/ChordAITests_artefacts/Debug/ChordAITests" "[chordanalysis]"` (tag-filtered; verified binary path from this research pass) |
| Full suite command | `cmake --build build --target ChordAITests && ctest --test-dir build --output-on-failure` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|---------------------|-------------|
| ANL-01 | Synthetic C-major-triad-progression chroma correlates highest with C major profile | unit | `ctest --test-dir build -R "KeyDetectorTests" --output-on-failure` | ❌ Wave 0 |
| ANL-01 | Synthetic A-minor progression detects A minor (not C major relative-key confusion) | unit | `ctest --test-dir build -R "KeyDetectorTests.RelativeMinor"` | ❌ Wave 0 |
| ANL-02 | Click track at known BPM (e.g. 120, 90, 160) detects correct tempo within tolerance | unit | `ctest --test-dir build -R "TempoBeatTrackerTests"` | ❌ Wave 0 |
| ANL-02 | Syncopated/off-beat-heavy click pattern does not lock to half/double tempo | unit | `ctest --test-dir build -R "TempoBeatTrackerTests.OctaveErrorResistance"` | ❌ Wave 0 |
| ANL-02 | Bar grid = every 4th beat, `barStartBeatIndices` populated correctly | unit | `ctest --test-dir build -R "TempoBeatTrackerTests.BarGrid"` | ❌ Wave 0 |
| ANL-03 | Synthetic 4-chord progression (sine/saw stacks, known chords+timing) decodes to the exact expected chord sequence | unit/integration | `ctest --test-dir build -R "ChordDecoderTests.SyntheticProgression"` | ❌ Wave 0 |
| ANL-03 | Progression with an injected percussive click track still decodes correctly (percussion-suppression check) | unit | `ctest --test-dir build -R "ChromaExtractorTests.PercussionRobustness"` | ❌ Wave 0 |
| ANL-03 | Deliberately-detuned (e.g. -30 cents) synthetic fixture still decodes correctly (tuning-correction check) | unit | `ctest --test-dir build -R "TuningEstimatorTests"` | ❌ Wave 0 |
| ANL-03 | Bass-note-in-inversion fixture (e.g. C major triad over an A bass) biases root correctly without surfacing an inversion label | unit | `ctest --test-dir build -R "ChordDecoderTests.BassRootBias"` | ❌ Wave 0 |
| ANL-03 | Chord segment boundaries always fall exactly on beat-grid timestamps | unit | `ctest --test-dir build -R "ChordDecoderTests.SegmentsAlignToBeats"` | ❌ Wave 0 |
| ANL-06 | `ClassicDspChordAnalyzer` fully implements `ChordAnalyzer` and is callable with a no-op `CancelToken`/`ProgressCallback`, zero `juce::ThreadPool`/GUI dependency | unit | `ctest --test-dir build -R "ClassicDspChordAnalyzerTests.HeadlessInvocation"` | ❌ Wave 0 |
| ANL-06 | A `CancelToken` returning `true` immediately stops mid-pipeline and returns `wasCancelled=true` | unit | `ctest --test-dir build -R "ClassicDspChordAnalyzerTests.Cancellation"` | ❌ Wave 0 |
| ANL-06 | `ProgressCallback` is invoked with monotonically increasing `fractionComplete` across pipeline stages | unit | `ctest --test-dir build -R "ClassicDspChordAnalyzerTests.ProgressMonotonic"` | ❌ Wave 0 |
| — (NFR) | 3-minute synthetic song analyzes in under ~5-10s wall clock on dev hardware | unit (timed) | `ctest --test-dir build -R "ClassicDspChordAnalyzerTests.PerformanceBudget"` | ❌ Wave 0 |
| — (manual) | Real (non-synthetic) royalty-cleared track produces a musically plausible progression | manual | Listen/compare in a scratch harness; not CTest-automatable per Open Question 4 | N/A |

### Sampling Rate
- **Per task commit:** `cmake --build build --target ChordAITests && "./build/ChordAITests_artefacts/Debug/ChordAITests" "[chordanalysis]"` (fast, tag-filtered to this phase's new tests only)
- **Per wave merge:** `ctest --test-dir build --output-on-failure` (full 15-existing + new suite, catches regressions in Phase 1/2 modules too)
- **Phase gate:** Full suite green before `/gsd:verify-work`, plus the manual real-track listening check from the table above (not automatable, but should be performed and noted before declaring the phase done — ties to PITFALLS.md's "Looks Done But Isn't" checklist item on chord-detection accuracy).

### Wave 0 Gaps
- [ ] `Tests/SyntheticFixtures.h` — shared audio-generation helpers: `renderChordProgression(chords, bpm, sampleRate, secondsPerChord)`, `renderClickTrack(bpm, sampleRate, durationSeconds)`, both producing an in-memory `juce::AudioBuffer<float>` (no file I/O, matching the existing `writeSineFixture`-in-`AudioFileLoaderTests.cpp` style but skipping the file round-trip since this phase's tests feed buffers directly). Include variants with injected percussive noise bursts, deliberate cents-detuning, and syncopated/off-beat click patterns for the pitfall-specific tests above.
- [ ] `Tests/ChromaExtractorTests.cpp`, `Tests/TuningEstimatorTests.cpp`, `Tests/TempoBeatTrackerTests.cpp`, `Tests/KeyDetectorTests.cpp`, `Tests/ChordDecoderTests.cpp`, `Tests/ClassicDspChordAnalyzerTests.cpp` — none exist yet; `Source/Analysis/` folder doesn't exist yet either.
- [ ] `constant-q-cpp` FetchContent + manual source-file-list CMake target — not yet wired into `CMakeLists.txt` (currently only Catch2 is FetchContent'd); the library has no upstream `CMakeLists.txt` of its own, so ChordAI's build must enumerate its `.cpp`/`.c` files explicitly, and the `COPYING` file's exact license text should be captured into a `THIRD_PARTY_LICENSES.md` per PITFALLS.md #1's recommendation, from the commit this project pins.
- [ ] Both `ChordAI` (plugin) and `ChordAITests` targets need the new `Source/Analysis/*.cpp` files added to `target_sources` once they exist.

## Sources

### Primary (HIGH confidence)
- Ellis, D.P.W., "Beat Tracking by Dynamic Programming," *JNMR* 36(1), 2007 — PDF fetched and read directly (`ee.columbia.edu/~dpwe/pubs/Ellis07-beattrack.pdf`); all onset-envelope, autocorrelation-tempo, and DP-backtrace parameters in this document are quoted/derived from that direct read, not a secondary summary.
- `github.com/cannam/constant-q-cpp` — `cq/Chromagram.h`, `cq/CQParameters.h`, `cq/CQSpectrogram.h`, `cq/CQBase.h` read directly via `gh api` in this research pass (not paraphrased).
- `external/JUCE/modules/juce_dsp/frequency/juce_FFT.h`, `juce_Windowing.h`, `external/JUCE/modules/juce_audio_basics/utilities/juce_Interpolators.h`, `juce_GenericInterpolator.h` — read directly from this project's vendored JUCE 8.0.14 submodule.
- This project's own `CMakeLists.txt`, `Tests/AudioFileLoaderTests.cpp`, `Source/Import/AudioFileLoader.h`, `Source/Import/LoadedAudio.h` — read directly to confirm existing conventions (test naming, fixture style, interface style) this phase should match.
- `ctest -N` run against the existing `build/` directory in this research pass — confirmed exact test-binary path and current 15-test baseline.

### Secondary (MEDIUM confidence)
- Krumhansl-Kessler (1982) key profile values — cross-verified via two independent sources (`rnhart.net/articles/key-finding`, `mashav.com` Krumhansl-Schmuckler Praat script docs); both agree on all 24 numbers.
- AudioLabs Erlangen FMP notebooks (`audiolabs-erlangen.de/resources/MIR/FMP/C5/C5S3_ChordRec_HMM.html`, `.../C6/C6S3_BeatTracking.html`) — fetched and summarized in this pass; binary-template definition, thresholded-normalization concept, and cosine-similarity/L1-normalize likelihood construction quoted from this source. Already cited HIGH in ARCHITECTURE.md; this pass independently re-confirmed the specifics used here.
- librosa documentation (`librosa.org/doc/main/generated/librosa.beat.beat_track.html`, `.../librosa.feature.chroma_cqt.html`) — used only for cross-validating parameter choices (tightness=100, bins_per_octave=36, n_octaves=7) against an independent, widely-used reimplementation of the same published algorithms; librosa itself is not a dependency of this project (Python, not linkable into a C++ plugin) and is cited for parameter cross-reference only.
- Bello & Pickens (2005), Sheh & Ellis (2003) — chord-template + HMM baseline, referenced (not read primary-source in this pass) via the FMP notebook's characterization and general MIR literature search results; algorithm-only reference per STACK.md's licensing stance (no code from any implementation of these papers was consulted or copied).
- Mauch & Dixon (2010), "Approximate note transcription for the improved identification of difficult chords," ISMIR — paper's existence and topic (bass/root disambiguation) confirmed via search; exact formula not independently re-derived from the paper text — the bass-root-bias scoring formula in this document is this project's own synthesis, explicitly flagged as such.
- Harte & Sandler (2005), "Automatic Chord Identification Using a Quantised Chromagram," AES 118th Convention — paper's existence and general tuning-histogram approach confirmed via search; exact bucketing algorithm not independently re-derived — this document's tuning-estimation formula is this project's own reasoned implementation of the published concept.
- Fitzgerald, D. (2010), "Harmonic/Percussive Separation Using Median Filtering," DAFx — paper's existence and general median-filter-along-time-axis approach confirmed via search; exact kernel-size parameter not sourced from the paper, stated as this project's own tunable choice.

### Tertiary (LOW confidence)
- None used as load-bearing — all pipeline-critical numeric parameters above trace to at least a Secondary-or-better source, with any project-original synthesis explicitly flagged inline rather than presented as sourced fact.

## Metadata

**Confidence breakdown:**
- Standard stack (constant-q-cpp API, JUCE FFT/Windowing/Interpolator APIs): HIGH — every signature verified by reading actual source files in this pass, not summarized.
- Beat/tempo algorithm (Ellis 2007): HIGH — primary paper read directly, exact equations and numbers transcribed.
- Key detection (Krumhansl-Kessler profiles): MEDIUM-HIGH — values cross-verified across two independent republications, not re-derived from the original 1990 book.
- Chord recognition (templates, Viterbi self-transition, bass-root bias): MEDIUM — algorithm shape and template definitions confirmed against a primary-adjacent source (FMP notebook); specific tunable numbers (β, γ, percussion-filter kernel size) are this project's own reasoned starting points, explicitly flagged for empirical validation rather than presented as settled.
- Architecture (`ChordAnalyzer` interface refinement, module breakdown): MEDIUM-HIGH — directly extends ARCHITECTURE.md's already-HIGH-confidence sketch, refined specifically to satisfy this phase's headless-testability success criterion.

**Research date:** 2026-07-12
**Valid until:** Stable for this project's lifetime for the algorithm/parameter choices (published, non-moving-target math); re-check constant-q-cpp's pinned commit and JUCE version compatibility if either is upgraded (~90 days is a reasonable revisit horizon for dependency-version drift, not for the underlying DSP research itself).

---
*Research for: Phase 3 - Core Chord-Detection Engine (ChordAI)*
*Researched: 2026-07-12*
