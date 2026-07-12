---
phase: 2
slug: audio-import-waveform
status: approved
nyquist_compliant: true
wave_0_complete: false
created: 2026-07-12
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7+ (STACK.md decision) — Wave 0 цієї фази вперше під'єднує його до CMake |
| **Config file** | CMakeLists.txt (FetchContent Catch2 + Tests/ таргет + enable_testing()/CTest) — Wave 0 |
| **Quick run command** | `ctest --test-dir build -R ChordAITests --output-on-failure` |
| **Full suite command** | те саме + pluginval strict-mode прогін (VST3+AU) — цієї фази додаються нові модулі/компоненти |
| **Estimated runtime** | ~10-40 s (fixture-based decode + чиста математика pixel↔time; без аудіодевайса) |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build -R ChordAITests --output-on-failure`
- **After every plan wave:** Full ctest + `pluginval --strictness-level 5` (VST3 та AU) як регресійний гейт
- **Before `/gsd:verify-work`:** Full suite green + ручний drag-and-drop smoke у Standalone (WAV/MP3/AIFF/FLAC); повна DAW-матриця — обов'язок Фази 7 (PLT-03)
- **Max feedback latency:** 60 s

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| (планувальник заповнить ID) | TBD | TBD | IMP-01 | unit | `ctest -R AudioFileLoaderTests` (WAV/AIFF/FLAC fixtures генеруються JUCE-райтерами на сетапі; MP3 — закомічений fixture, JUCE не вміє писати MP3) | ❌ W0 | ⬜ pending |
| (планувальник заповнить ID) | TBD | TBD | IMP-02 | unit | `ctest -R WaveformRegionTests.ThumbnailPopulates` (getTotalLength() > 0 після прокрутки message loop) | ❌ W0 | ⬜ pending |
| (планувальник заповнить ID) | TBD | TBD | IMP-03 | unit | `ctest -R WaveformRegionTests` (PixelTimeConversion, DefaultWholeFile, DragSelectionClamped — синтетичні координати) | ❌ W0 | ⬜ pending |
| (планувальник заповнить ID) | TBD | TBD | regression | smoke/CLI | pluginval strictness 5 на VST3+AU після додавання модулів/UI | ✅ (tools/pluginval.app з Фази 1) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] CMakeLists.txt: Catch2 v3.7+ через FetchContent + Tests/ виконуваний таргет + enable_testing()/CTest
- [ ] `Tests/AudioFileLoaderTests.cpp` — IMP-01 (WAV/AIFF/FLAC fixtures через JUCE-райтери на test-setup; MP3 через закомічений fixture)
- [ ] `Tests/fixtures/silence_1s.mp3` — ~1 с тиші, згенерувати один раз через `afconvert` (CoreAudioFormat::createWriterFor — незаімплементований stub у JUCE)
- [ ] `Tests/WaveformRegionTests.cpp` — IMP-02/IMP-03 (pixel↔time, default whole-file, clamped drag)
- [ ] Явно додати `juce::juce_audio_formats` у target_link_libraries (зараз транзитивно через juce_audio_utils — задокументувати реальну залежність)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| OS-level drag-and-drop доставляє файл у filesDropped | IMP-01 | Симуляція OS-драгу непрактична; Pitfall 8 — поведінка драгу host-специфічна | Перетягнути WAV/MP3/AIFF/FLAC на вікно Standalone — файл вантажиться |
| Waveform читабельно рендериться | IMP-02 | Немає visual-regression тулінгу | Візуальна перевірка в Standalone після завантаження файлу |
| Background decode не блокує message thread | supporting | Responsiveness під реальним OS-шедулінгом не юніт-тестується | Закинути 3+ хв файл — редактор лишається відзивним (конвеєр-анімація не фрізиться) |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (3 manual-only — обґрунтовані вище)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 60s (ctest швидкий; pluginval — per-wave)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-07-12
