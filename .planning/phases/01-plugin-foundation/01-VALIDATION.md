---
phase: 1
slug: plugin-foundation
status: draft
nyquist_compliant: false
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
| 1-XX (build) | TBD | 1 | PLT-01 | build | `cmake -B build -G Ninja && cmake --build build` | ❌ W0 (CMakeLists.txt) | ⬜ pending |
| 1-XX (AU validity) | TBD | 1 | PLT-01 | smoke/CLI | `auval -v aufx <SUBT> <MANU>` | ❌ W0 (потрібен .component) | ⬜ pending |
| 1-XX (cross-format) | TBD | 1 | PLT-01 | smoke/CLI | `pluginval --strictness-level 5 --validate-in-process --output-dir ./validation-logs <path>` per format | ❌ W0 (pluginval binary) | ⬜ pending |
| 1-XX (standalone) | TBD | 1 | PLT-01 | smoke/script | `tools/smoke-test-standalone.sh` — запуск, N с, процес живий, немає нових crash reports | ❌ W0 (скрипт) | ⬜ pending |
| 1-XX (RT-safety) | TBD | 1 | PLT-01 | static + CLI | grep `processBlock`/callees на `new`/`malloc`/resize/`std::mutex`/I-O + pluginval RealtimeCheck (не ловить mutex — grep обов'язковий) | ❌ W0 (коду нема) | ⬜ pending |
| 1-XX (state round-trip) | TBD | 1 | PLT-01 | smoke/CLI | входить у стандартний pluginval strictness-5 прогін (randomize + restore) | ❌ W0 | ⬜ pending |

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

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
