---
phase: 4
slug: analysis-ui-integration
status: approved
nyquist_compliant: true
wave_0_complete: false
created: 2026-07-13
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 + CTest (уже підключено, таргет ChordAITests) |
| **Config file** | CMakeLists.txt — catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.") |
| **Quick run command** | `ctest --test-dir build -R "ChordAITests.(AnalysisPipeline\|ChordNameFormatter\|ChordTimelineLayout)" --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | quick ~10-30 s; full ~60-90 s (включно з PerformanceBudget) |

---

## Sampling Rate

- **After every task commit:** quick run (нові тести фази)
- **After every plan wave:** full ctest + pluginval strictness 5 (VST3+AU) — гейт Фаз 2/3
- **Before `/gsd:verify-work`:** full suite green + ручний чекпоінт: відзивність Standalone під час аналізу реального 3-хв треку, видимість прогрес-індикатора, читабельність таймлайна акордів, одноразовий замір часу на Release-збірці
- **Max feedback latency:** 90 s

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| (планувальник) | TBD | TBD | ANL-04 | unit/integration | `ctest -R AnalysisPipelineTests.CancelAndRestart` (generation guard: два setSelectedRegion поспіль → результат лише останнього) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | ANL-04 | unit | `ctest -R AnalysisPipelineTests.NoOpRegionDoesNotRetrigger` | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | ANL-04 | unit | `ctest -R ClassicDspChordAnalyzerTests.ProgressMonotonic` + `.PerformanceBudget` | ✅ existing | ⬜ pending |
| (планувальник) | TBD | TBD | ANL-05 | unit | `ctest -R ChordNameFormatterTests` (4 quality × representative pitch classes; "" / "m" / "7" / "N.C.") | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | ANL-05 | unit | `ctest -R ChordTimelineLayoutTests.LabelCollision` (shouldDrawLabel на вузьких сегментах) + existing `WaveformRegionTests.PixelTimeConversion` | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | regression | smoke/CLI | pluginval strictness 5 (VST3+AU) | ✅ tools/pluginval.app | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `Tests/AnalysisPipelineTests.cpp` — ANL-04 cancel/restart + no-op guard через публічний API PluginProcessor
- [ ] `Tests/ChordNameFormatterTests.cpp` — ANL-05 мапінг назв акордів
- [ ] `Tests/ChordTimelineLayoutTests.cpp` — ANL-05 label-collision чиста логіка
- [ ] `CMakeLists.txt` — додати `Source/Analysis/AnalysisPipeline.cpp` і `Source/UI/ChordTimelineView.cpp` у target_sources обох таргетів
- [ ] Одноразова Release-конфігурація: `cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release` (для чекпоінт-заміру реального часу аналізу)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| UI відзивний під час аналізу 3-хв треку | ANL-04 | Responsiveness під реальним OS-шедулінгом не юніт-тестується | Закинути 3+ хв файл у Standalone, тягати регіон/ресайзити вікно під час аналізу — без фрізів |
| Прогрес-індикатор видимий під час аналізу | ANL-04 | Немає visual-regression тулінгу | Візуальна перевірка в Standalone |
| Реальний ~3-хв трек аналізується за секунди на Release | ANL-04 | Release-CI не існує; одноразовий замір | Release-збірка + RealTrackHarness або таймер у Standalone |
| Таймлайн акордів читабельний, вирівняний по waveform | ANL-05 | Піксель-арт легібіліті — візуальне судження | Візуальна перевірка на реальному треку |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (4 manual-only — обґрунтовані)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 90s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-07-13
