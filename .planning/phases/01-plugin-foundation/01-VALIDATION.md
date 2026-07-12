---
phase: 1
slug: plugin-foundation
status: approved
nyquist_compliant: true
wave_0_complete: false
created: 2026-07-12
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None for Phase 1 (no algorithmic logic yet; Catch2 arrives in Phase 3). Phase 1 validation = build success + plugin-validator CLIs (auval, pluginval) + smoke scripts |
| **Config file** | none — Wave 0 creates `CMakeLists.txt` |
| **Quick run command** | `cmake --build build` (all three formats: VST3, AU, Standalone) |
| **Full suite command** | build + `auval -v aufx <SUBT> <MANU>` + `pluginval --strictness-level 5 --validate-in-process <plugin>` (VST3 та AU) + `tools/smoke-test-standalone.sh` |
| **Estimated runtime** | ~120-300 s (перша збірка JUCE довша; інкрементальна ~10-30 s) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build` — миттєвий сигнал compile/link breakage
- **After every plan wave:** Run full suite (auval + pluginval strictness 5 + standalone smoke)
- **Before `/gsd:verify-work`:** Full suite green **плюс** ручне завантаження в Ableton Live, FL Studio, Logic Pro (холодний скан, не з кешу)
- **Max feedback latency:** 300 s

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 01-01-T1 (toolchain+JUCE pin) | 01-01 | 1 | PLT-01 | env/CLI | `cmake --version && ninja --version && git -C external/JUCE describe --tags --exact-match` | ❌ W0 (submodule) | ⬜ pending |
| 01-01-T2 (configure) | 01-01 | 1 | PLT-01 | build | `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | ❌ W0 (CMakeLists.txt) | ⬜ pending |
| 01-01-T3 (build+RT grep) | 01-01 | 1 | PLT-01 | build + static | `cmake --build build` + перевірка бандлів у ~/Library/Audio/Plug-Ins + grep processBlock на alloc/mutex/I-O | ❌ W0 (коду нема) | ⬜ pending |
| 01-02-T1 (pluginval fetch) | 01-02 | 2 | PLT-01 | tooling | `bash tools/fetch-pluginval.sh && pluginval --version` | ❌ W0 (скрипт) | ⬜ pending |
| 01-02-T2 (standalone smoke) | 01-02 | 2 | PLT-01 | smoke/script | `bash tools/smoke-test-standalone.sh` — запуск, процес живий, немає нових crash reports | ❌ W0 (скрипт) | ⬜ pending |
| 01-02-T3 (auval+pluginval) | 01-02 | 2 | PLT-01 | smoke/CLI | `auval -v aufx Cha1 Chai` + `pluginval --strictness-level 5 --validate-in-process` (VST3 та AU; включає state round-trip і RT-check) | ❌ W0 (потрібні бандли) | ⬜ pending |
| 01-03-T1 (pre-flight) | 01-03 | 3 | PLT-01 | build+CLI | rebuild + auval + smoke перед ручною перевіркою | — | ⬜ pending |
| 01-03-T2 (DAW loads) | 01-03 | 3 | PLT-01 | manual-only | MISSING — checkpoint:human-verify (Ableton/FL/Logic не мають headless-режиму) | — | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `brew install cmake` (+ ninja) — CMake відсутній на машині; без нього фаза не рухається
- [ ] `CMakeLists.txt` + `Source/PluginProcessor.{h,cpp}` + `Source/PluginEditor.{h,cpp}` — репозиторій зараз містить лише `.planning/`
- [ ] `external/JUCE` git submodule, запінений на точний тег (8.0.13 — перевірити наявність новішого 8.0.x перед пінінгом)
- [ ] `tools/fetch-pluginval.sh` — скрипт завантаження prebuilt pluginval (бінарник не комітимо)
- [ ] `tools/smoke-test-standalone.sh` — smoke-запуск Standalone
- [ ] Зафіксувати (хоч плейсхолдерами) `COMPANY_NAME`, `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE` — пропуск PLUGIN_CODE дає рандомну ідентичність плагіна на кожну збірку

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Плагін вантажиться в Ableton Live (VST3) | PLT-01 | Немає headless-режиму сканування плагінів | Відкрити Live → Preferences → Plug-ins → rescan → вставити ChordAI на трек → вікно відкривається, без крешу |
| Плагін вантажиться в FL Studio (VST3) | PLT-01 | Немає headless-режиму | FL → Plugin Manager → find plugins → додати на канал → без крешу |
| Плагін вантажиться в Logic Pro (AU) | PLT-01 | auval покриває валідність, але не реальний host-load | Logic → створити трек → MIDI FX/AU список → вставити → без крешу |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (єдиний MISSING — ручні DAW-завантаження, обґрунтовано)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 300s (виняток: перша повна збірка JUCE ~5-10 хв, одноразово — зафіксовано план-чекером як неминуче)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-07-12 (plan-checker: 0 blockers)
