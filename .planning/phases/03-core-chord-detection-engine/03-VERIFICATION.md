---
phase: 03-core-chord-detection-engine
verified: 2026-07-13T09:05:08Z
status: passed
score: 4/4 must-haves verified (29/29 plan-level truths across 6 plans verified)
---

# Phase 3: Core Chord-Detection Engine Verification Report

**Phase Goal:** The plugin can correctly determine key, tempo, and chord progression from a decoded audio buffer, isolated behind an interface that a v2 ML backend can later replace without touching UI or generation code.
**Verified:** 2026-07-13T09:05:08Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Given a loaded song/region, the engine outputs a detected key | ✓ VERIFIED | `KeyDetector::detectKey` (K-K 24-profile Pearson correlation); `KeyDetectorTests.CMajorFixture`/`RelativeMinor`/`DetunedKeyStillDetected` pass through the real audio→chroma→key chain; human-verified real track detected G# minor |
| 2 | Given a loaded song/region, the engine outputs a detected tempo (BPM) and bar grid | ✓ VERIFIED | `TempoBeatTracker::trackBeats` (Ellis 2007); `Detects90/120/160Bpm`, `OctaveErrorResistance`, `BarGrid` all pass; real track measured BPM 170.455 with 53 bars |
| 3 | Given a loaded song/region, the engine outputs a chord progression aligned to the bar grid | ✓ VERIFIED | `ChordDecoder::decodeChords` (beat-sync chroma, 36-template cosine+bass-bias, log-Viterbi, N-state override); `SyntheticProgression`/`SegmentsAlignToBeats` prove exact-order decode and beat-grid-exact boundaries; real track produced a coherent diatonic 7-chord loop |
| 4 | Detection is invocable and verifiable through a `ChordAnalyzer` interface via a standalone test harness, independent of any UI | ✓ VERIFIED | `ClassicDspChordAnalyzerTests.HeadlessInvocation` calls through `ChordAnalyzer&` (not the concrete type); zero GUI/ThreadPool/MessageManager includes anywhere in `Source/Analysis/` (verified by grep — only comments reference Phase 4's future `juce::ThreadPoolJob`) |

**Score:** 4/4 truths verified

### Per-Plan Must-Haves (all six PLAN.md frontmatter blocks)

All 29 `must_haves.truths` entries across 03-01..03-06 are backed by a passing, non-empty `TEST_CASE` exercising real (non-stub) implementation code — cross-checked by name against `ctest` output (64/64 passed) and by reading the actual assertions:

| Plan | Truths | Status |
|------|--------|--------|
| 03-01 (foundation) | Build all targets + 15 pre-existing tests green; frozen headers zero-GUI/ThreadPool; CQT sanity; synthetic fixtures render; dual-rate preprocessing | ✓ 5/5 |
| 03-02 (chroma path) | Triad top-3 pitch classes; detune corrected; percussion-robust; bass 55-250Hz / harmonic ≥80Hz isolation; CQT full-duration coverage | ✓ 5/5 |
| 03-03 (tempo/beat) | 90/120/160 BPM ±3; syncopation resists octave error; beats ±70ms/80%; bar grid every-4th-beat | ✓ 4/4 |
| 03-04 (key detection) | C major detected; A minor (with E-major dominant) not confused with C major; confidence reflects margin | ✓ 3/3 |
| 03-05 (chord recognition) | C-F-G-Am decodes in exact order; segments beat-grid-aligned; bass-root bias resolves ambiguous chroma both directions; silence → NoChord | ✓ 4/4 |
| 03-06 (orchestrator) | Headless `analyse()` correct key/BPM/chords; cancellation mid-run; monotonic progress; 3-min song within budget; full suite green; real-track human-verified plausibility | ✓ 6/6 |

### Required Artifacts (representative sample, all six plans)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `Source/Analysis/ChordAnalyzer.h` | Pure-virtual interface, `analyse()` signature | ✓ VERIFIED | 37 lines; `virtual AnalysisResult analyse (...)` present; no GUI/ThreadPool includes |
| `Source/Analysis/AnalysisResult.h` | Value types incl. `struct AnalysisResult` | ✓ VERIFIED | 45 lines; `struct AnalysisResult` present |
| `THIRD_PARTY_LICENSES.md` | constant-q-cpp license + pinned SHA | ✓ VERIFIED | 105 lines; verbatim MIT COPYING + bundled KissFFT (BSD-3-Clause); no GPL/AGPL text anywhere |
| `Tests/SyntheticFixtures.h` | Chord/click/percussion/Goertzel generators | ✓ VERIFIED | 221 lines (min 80) |
| `Source/Analysis/AudioPreprocessing.cpp` | Dual-rate downmix/resample | ✓ VERIFIED | 89 lines (min 40) |
| `Source/Analysis/ConstantQAnalysis.cpp` | Streaming CQT wrapper | ✓ VERIFIED | 79 lines (min 50) |
| `Source/Analysis/TuningEstimator.cpp` | Circular cents-histogram estimate | ✓ VERIFIED | 66 lines (min 30) |
| `Source/Analysis/HarmonicPercussiveFilter.cpp` | Median-filter percussion suppression | ✓ VERIFIED | 89 lines (min 25) |
| `Source/Analysis/ChromaExtractor.cpp` | Dual harmonic+bass chroma fold | ✓ VERIFIED | 87 lines (min 50) |
| `Source/Analysis/OnsetEnvelope.cpp` | Ellis 2007 §3.1 onset envelope | ✓ VERIFIED | 225 lines (min 60) |
| `Source/Analysis/TempoBeatTracker.cpp` | Autocorrelation + DP + bar grid | ✓ VERIFIED | 153 lines (min 60) |
| `Source/Analysis/KeyDetector.cpp` | K-K 24-profile correlation | ✓ VERIFIED | 121 lines (min 50); contains `6.35` (K-K major profile literal) |
| `Source/Analysis/ChordTemplates.h` | 36 binary templates + index mapping | ✓ VERIFIED | 93 lines (min 40) |
| `Source/Analysis/ChordDecoder.cpp` | Beat-sync avg + Viterbi + segment merge | ✓ VERIFIED | 289 lines (min 100) |
| `Source/Analysis/ClassicDspChordAnalyzer.cpp` | Full pipeline orchestration | ✓ VERIFIED | 169 lines (min 80) |
| `Tests/ClassicDspChordAnalyzerTests.cpp` | Headless/cancel/progress/perf/real-track tests | ✓ VERIFIED | 460 lines (min 80) |

All 23 `Source/Analysis/` module files and all 8 Phase-3 test files exist on disk with real (non-stub) bodies.

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `CMakeLists.txt` | constant-q-cpp static lib | `FetchContent` + explicit source enumeration, linked into both targets | ✓ WIRED | `add_library(constant_q_cpp STATIC ...)` present, linked into `ChordAI`+`ChordAITests` |
| `CMakeLists.txt` | `Source/Analysis/*.cpp` | `target_sources` of both targets | ✓ WIRED | 10 matches of `Source/Analysis/` in CMakeLists.txt |
| `Tests/ChromaExtractorTests.cpp` | `CQSpectrogram` | `process()` + `getRemainingOutput()` streaming | ✓ WIRED | `getRemainingOutput()` called in both the test and `ConstantQAnalysis.cpp` |
| `ChromaExtractor.cpp` | `CqtFrames.binFrequenciesHz` | per-bin queried frequency fold | ✓ WIRED | `cqt.binFrequenciesHz[bin]` used, never hardcoded ordering |
| `ChromaExtractor.cpp` | `TuningEstimator` cents | tuning-corrected pitch-class rounding | ✓ WIRED | `pitchClassForBin(freqHz, tuningCents)` consumes the passed-in tuning estimate |
| `OnsetEnvelope.cpp` | `juce::dsp::FFT` | `performFrequencyOnlyForwardTransform` on Hann-windowed frames | ✓ WIRED | call present at line 122 |
| `TempoBeatTracker.cpp` | `OnsetEnvelopeResult` | autocorrelation + DP at `rateHz` | ✓ WIRED | `onset.rateHz` consumed throughout |
| `TempoBeatTracker.cpp` | `BeatGrid.barStartBeatIndices` | every-4th-beat downbeat assignment | ✓ WIRED | `grid.barStartBeatIndices.push_back(i)` on 4-beat stride |
| `KeyDetector.cpp` | `ChromaSequence` | energy-weighted (`harmonicPreNormL2`) accumulation | ✓ WIRED | `frame.harmonic[pc] * frame.harmonicPreNormL2` |
| `KeyDetector.cpp` | `KeyResult` | tonic/isMajor/confidence population | ✓ WIRED | `KeyResult result;` populated and returned |
| `ChordDecoder.cpp` | `ChromaSequence + BeatGrid` | per-beat-interval chroma averaging | ✓ WIRED | `beats.beatTimesSeconds` drives `computeBeatSyncChroma` |
| `ChordDecoder.cpp` | `ChordSegment` | merged Viterbi states, beat-index+seconds boundaries | ✓ WIRED | `std::vector<ChordSegment> segments` built and returned |
| `ClassicDspChordAnalyzer.cpp` | full module chain | sequential synchronous orchestration with progress+cancel | ✓ WIRED | `decodeChords` called as final stage; `wasCancelled` set at every stage-guard |
| `Tests/ClassicDspChordAnalyzerTests.cpp` | `ChordAnalyzer` interface | invocation through `ChordAnalyzer&` | ✓ WIRED | `ChordAnalyzer& iface = analyzer;` used in 7 test cases, proving the swappable-interface contract |

### Automated Regression Checks

| Check | Command | Result |
|-------|---------|--------|
| Full test suite | `ctest --test-dir build --output-on-failure` | **64/64 passed** (100%), 96.46s real time — matches 15 pre-existing + 49 new `[chordanalysis]` tests exactly |
| Frozen contracts unchanged | `git diff 722703d..HEAD -- Source/Analysis/ChordAnalyzer.h Source/Analysis/AnalysisResult.h` | **Empty diff** — both headers byte-identical to their 03-01 frozen commit |
| No GPL/AGPL dependencies | `THIRD_PARTY_LICENSES.md` review | Only constant-q-cpp (MIT-style) + bundled KissFFT (BSD-3-Clause); no GPL text anywhere in the file |
| No GUI/ThreadPool coupling | `grep -rn "juce::ThreadPool\|MessageManager\|juce_gui" Source/Analysis/*.{h,cpp}` | Only comments referencing Phase 4's *future* `juce::ThreadPoolJob` wrapper — zero actual `#include`s of GUI/ThreadPool headers in any Analysis module |
| Test-case inventory | `grep TEST_CASE` across all 8 Phase-3 test files | 49 real `TEST_CASE`s, all matching plan-specified behavior names (e.g. `RelativeMinor`, `OctaveErrorResistance`, `BassRootBias`, `PercussionRobustness`) — none stubbed out |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|---|---|---|---|---|
| ANL-01 | 03-04 | Key of the analyzed region is detected automatically | ✓ SATISFIED | `KeyDetector` K-K correlation; audio-integration tests green; real track G# minor |
| ANL-02 | 03-03 | Tempo (BPM) and bar grid are detected automatically | ✓ SATISFIED | `TempoBeatTracker` Ellis 2007; 90/120/160 BPM tests green; real track 170.455 BPM/53 bars |
| ANL-03 | 03-01, 03-02, 03-05 | Chord progression detected (chromagram + template matching + Viterbi) aligned to bar grid | ✓ SATISFIED | `ChordDecoder` 36-template Viterbi; exact-sequence + beat-aligned-segment tests green |
| ANL-06 | 03-01, 03-06 | Detection engine behind `ChordAnalyzer` interface, ML-swappable in v2 | ✓ SATISFIED | `ClassicDspChordAnalyzer` implements `ChordAnalyzer`; `HeadlessInvocation` test proves invocation through the interface reference, not the concrete type |

REQUIREMENTS.md marks all four as `[x]` — consistent with codebase evidence. No orphaned requirements found for Phase 3 in REQUIREMENTS.md.

### Anti-Patterns Found

None. Scanned all `Source/Analysis/*.h`/`*.cpp` for TODO/FIXME/XXX/HACK/PLACEHOLDER, "not implemented", empty-return stubs, and console-log-only bodies:

- Zero TODO/FIXME/placeholder comments in any Analysis module.
- The three `return {};` instances found (`AudioPreprocessing.cpp` x2, `ChordDecoder.cpp` x1) were inspected in context — all are legitimate degenerate-input guard clauses (empty/invalid input → empty-but-valid result), matching the project's documented "never throws" convention from `Source/Import/AudioFileLoader.h`, not unfinished stubs.

### Documented Deviations (accepted, not gaps)

Per task instructions, the following were checked against the codebase and confirmed accurately documented — treated as accepted, not gaps:

1. **Debug perf budget raised 10s → 54s**: `Tests/ClassicDspChordAnalyzerTests.cpp` line 319, `kBudgetSeconds = 54.0 // ceil(kMeasuredSeconds * 1.5)`, with a full profiling narrative in the preceding comment block (HarmonicPercussiveFilter median-filter hotspot fixed for a genuine 2.4x speedup; remaining cost attributed to legitimate, previously-decided DSP choices — WindowedSincInterpolator, vendored CQT — out of scope to rewrite). Comment explicitly states a Release build meets the original ~5-10s budget. Confirmed present exactly as described.
2. **kSelfTransition retuned 0.85 → 0.04**: `Source/Analysis/ChordDecoder.cpp` line 150, with an empirical-tuning rationale comment (literature-tuned betas were far too sticky for this project's lower-contrast L1-normalized observation matrix; 0.03 was the empirical floor, 0.04 keeps margin above it). Confirmed present.
3. **Onset envelope half-wave rectification fix (d4eced3)**: Confirmed in `Source/Analysis/OnsetEnvelope.cpp` and `03-06-SUMMARY.md` — checkpoint-discovered defect (BPM 0/empty chords on real-mix silence), root-caused via new `CHORDAI_DIAG` diagnostics, fixed by clamping negative envelope values post-smoothing/pre-normalization, full suite re-verified green (64/64) after the fix.
4. **Real-track listening check**: Human-approved 2026-07-13 per `03-06-SUMMARY.md` — user's own 75s trap beat (TOCK.mp3) analyzed to BPM 170.455, key G# minor, coherent diatonic progression; user (track author) confirmed "так десь такі" (roughly right). Treated as verified human input per task instructions — no further human verification needed for this phase.

### Human Verification Required

None. The phase's only human-verification gate (03-06 Task 3, real-track listening checkpoint) was already executed and approved on 2026-07-13, and is documented with concrete evidence (BPM/key/progression values, user confirmation quote) in `03-06-SUMMARY.md`. No additional items require human testing to confirm this phase's goal achievement.

### Gaps Summary

No gaps found. All four ROADMAP success criteria are verified, all 29 plan-level must-have truths across the six plans are backed by passing tests against real (non-stub) implementations, all frozen contracts (`ChordAnalyzer.h`/`AnalysisResult.h`) are byte-identical to their 03-01 origin, the license gate is clean (MIT/BSD only), the interface is demonstrably GUI/ThreadPool-free, and the full regression suite is green at 64/64. Documented deviations (perf budget, Viterbi beta, onset-envelope fix) are transparently rationalized in-code and in SUMMARY.md, and the mandatory human musical-plausibility checkpoint was completed and approved. Phase 3's goal — key/tempo/chord-progression detection behind a swappable `ChordAnalyzer` interface — is achieved.

---

*Verified: 2026-07-13T09:05:08Z*
*Verifier: Claude (gsd-verifier)*
