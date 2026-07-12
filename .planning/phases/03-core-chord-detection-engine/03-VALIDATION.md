---
phase: 3
slug: core-chord-detection-engine
status: approved
nyquist_compliant: true
wave_0_complete: false
created: 2026-07-12
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 + CTest (уже підключено; 15 наявних тестів) |
| **Config file** | CMakeLists.txt — catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.") |
| **Quick run command** | `cmake --build build --target ChordAITests && "./build/ChordAITests_artefacts/Debug/ChordAITests" "[chordanalysis]"` (тег-фільтр нових тестів фази) |
| **Full suite command** | `cmake --build build --target ChordAITests && ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | quick ~5-20 s; full ~30-60 s |

---

## Sampling Rate

- **After every task commit:** quick run (тег `[chordanalysis]`)
- **After every plan wave:** full ctest (ловить регресії Фаз 1-2)
- **Before `/gsd:verify-work`:** full suite green + ручна перевірка на реальному треку (музично правдоподібна прогресія)
- **Max feedback latency:** 60 s

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| (планувальник) | TBD | TBD | ANL-01 | unit | `ctest -R KeyDetectorTests` (C-major fixture → C major; A-minor → не relative-major) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | ANL-02 | unit | `ctest -R TempoBeatTrackerTests` (click 90/120/160 BPM ± tolerance; octave-error resistance; BarGrid — кожен 4-й біт) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | ANL-03 | unit/integration | `ctest -R ChordDecoderTests` (SyntheticProgression точна послідовність; BassRootBias; SegmentsAlignToBeats) + `ChromaExtractorTests.PercussionRobustness` + `TuningEstimatorTests` (-30 cents) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | ANL-06 | unit | `ctest -R ClassicDspChordAnalyzerTests` (HeadlessInvocation без GUI/ThreadPool; Cancellation; ProgressMonotonic) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | NFR perf | unit (timed) | `ctest -R ClassicDspChordAnalyzerTests.PerformanceBudget` (3-хв синтетика < ~5-10 s) | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `Tests/SyntheticFixtures.h` — renderChordProgression / renderClickTrack (in-memory AudioBuffer, без файлового I/O) + варіанти: перкусійні бурсти, детюн у центах, синкоповані патерни
- [ ] `Tests/{ChromaExtractor,TuningEstimator,TempoBeatTracker,KeyDetector,ChordDecoder,ClassicDspChordAnalyzer}Tests.cpp` — не існують; `Source/Analysis/` теж
- [ ] `constant-q-cpp` через FetchContent + ручний перелік .cpp/.c у CMake (апстрім без CMakeLists) + текст ліцензії у `THIRD_PARTY_LICENSES.md` (PITFALLS #1)
- [ ] Додати `Source/Analysis/*.cpp` у target_sources обох таргетів (ChordAI + ChordAITests)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Реальний трек → музично правдоподібна прогресія | ANL-03 (якість) | Немає ground-truth корпусу в репо; сприйняття на слух | Прогнати реальний трек через scratch-харнес, порівняти зі слухом/відомими акордами пісні |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (1 manual-only — обґрунтовано)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 60s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-07-12
