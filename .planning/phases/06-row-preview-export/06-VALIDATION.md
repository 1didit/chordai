---
phase: 6
slug: row-preview-export
status: approved
nyquist_compliant: true
wave_0_complete: false
created: 2026-07-13
---

# Phase 6 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 + CTest (уже підключено, ChordAITests) |
| **Config file** | CMakeLists.txt |
| **Quick run command** | `cmake --build build --target ChordAITests && ./build/ChordAITests_artefacts/Debug/ChordAITests "[midifilewriter],[auditionrenderer]"` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | quick <5 s; full ~60-120 s |

---

## Sampling Rate

- **After every task commit:** quick run (теги midifilewriter/auditionrenderer)
- **After every plan wave:** full ctest + pluginval strictness 5 (VST3+AU) — audition-код торкається processBlock!
- **Before `/gsd:verify-work`:** full suite green + Release pluginval + ручний чекпоінт: drag у FL Studio piano roll/channel rack, прослуховування, save-діалог
- **Max feedback latency:** 120 s

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| (планувальник) | TBD | TBD | EXP-03 | unit | `[midifilewriter]` — round-trip: ticks/tempo meta (microsecondsPerQuarter з bpm + fallback bpm<=0)/pitch/start/length ±1 tick | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | EXP-01/02 | unit | `[midifilewriter]` — suggestedFileName filesystem-safe, унікальний (row/key/bpm); файл існує і валідний ДО performExternalDragDropOfFiles | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | PRV-01 | unit | `[auditionrenderer]` — детермінізм (byte-identical), no NaN/Inf/clip, empty row safe, правильний sample count | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | PRV-01 RT-safety | tooling | pluginval strictness 5 (RT-check) + код-рев'ю processBlock: нуль алокацій/локів у audition-шляху (raw double-buffer + atomic indices, НЕ shared_ptr!) | ✅ tools | ⬜ pending |
| (планувальник) | TBD | TBD | regression | smoke/CLI | full ctest + pluginval (VST3+AU) | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `Tests/MidiFileWriterTests.cpp` (EXP-01/02/03)
- [ ] `Tests/AuditionRendererTests.cpp` (PRV-01)
- [ ] `Source/MidiGen/MidiFileWriter.*` + `Source/Audio/AuditionRenderer.*`/`AuditionVoice.*` у CMake обох таргетів
- [ ] КРИТИЧНО: temp .mid НЕ видаляти у drag-callback (Ableton async-read race — PITFALLS) — відкладене прибирання на початку наступного drag
- [ ] КРИТИЧНО: audition → processBlock через preallocated double-buffer + atomic індекси; existing atomic shared_ptr idiom заборонений на аудіопотоці

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Drag ряду в FL Studio piano roll/channel rack — ноти/темп правильні | EXP-01 | OS-level drag не автоматизується | Перетягнути кожен із 5 рядів у FL Studio (DAW користувача); Ableton/Logic — spot-check (повна матриця = Фаза 7) |
| Audition звучить (піано-подібний тон, старт/стоп) | PRV-01 | Якість звуку — слухове судження | Play/stop на кожному ряду в Standalone і в FL |
| Save-діалог зберігає .mid у вибрану папку | EXP-02 | Нативний macOS діалог | Зберегти ряд, відкрити файл у FL |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (3 manual-only — обґрунтовані)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 120s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-07-13
