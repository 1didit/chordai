---
phase: 5
slug: midi-conveyor-generation
status: approved
nyquist_compliant: true
wave_0_complete: false
created: 2026-07-13
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 + CTest (уже підключено, ChordAITests) |
| **Config file** | CMakeLists.txt — catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.") |
| **Quick run command** | `ctest --test-dir build -R "ChordAITests.(ChordToneMapper\|StyleVoicing\|BassLineGenerator\|MidiRowBuilder)" --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | quick ~10-30 s; full ~60-90 s |

---

## Sampling Rate

- **After every task commit:** quick run (нові MidiGen-тести)
- **After every plan wave:** full ctest + pluginval strictness 5 (VST3+AU)
- **Before `/gsd:verify-work`:** full suite green + ручний чекпоінт: 5 рядів видно в Standalone, читабельні, візуально різні по стилях, регенеруються при зміні регіону
- **Max feedback latency:** 90 s

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| (планувальник) | TBD | TBD | GEN-01 | unit | `ctest -R MidiRowBuilderTests` (FiveRowsForKnownProgression; AsIsRowMatchesDetectedProgression; NoChordSegmentEmitsNoNotes; SameInputProducesByteIdenticalRows — детермінізм; GenerationPerformanceBudget <1ms) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | GEN-01 | integration | `ctest -R PluginProcessorMidiGenTests.RowsPublishedSynchronouslyWithAnalysisResult` | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | GEN-02 | unit | `ctest -R StyleVoicingTests` (PopTrapTriadOnlyCloseRegister; RnbExtensionsMatchQualityTable; RnbVoiceLeadingMinimizesMovement; HouseStabPatternExactTiming; ThreeStylesProduceDistinctContent; OutputTracksInputProgression) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | GEN-03 | unit | `ctest -R BassLineGeneratorTests` (RootPitchClassMatchesDetectedChord; TrapBassSustainsFullSegment; RnbBassRootFifthWalk; HouseBassFourOnTheFloor) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | GEN-04 | integration | `ctest -R PluginProcessorMidiGenTests.RowsRegenerateOnRegionChange` (через публічний API, message-pump) | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | UI layout | unit | `ctest -R MidiSetsPanelLayoutTests.PitchToYAndBeatsToX` | ❌ W0 | ⬜ pending |
| (планувальник) | TBD | TBD | regression | smoke/CLI | pluginval strictness 5 (VST3+AU) | ✅ tools/pluginval.app | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `Tests/MidiGenFixtures.h` — рукописний fixture AnalysisResult (struct-literal прогресія, без рендеру аудіо)
- [ ] `Tests/{ChordToneMapper,StyleVoicing,BassLineGenerator,MidiRowBuilder,PluginProcessorMidiGen,MidiSetsPanelLayout}Tests.cpp`
- [ ] `Source/MidiGen/*` у CMake обох таргетів (новий CHORDAI_MIDIGEN_SOURCES за зразком CHORDAI_ANALYSIS_SOURCES)
- [ ] `Source/UI/MidiSetsPanel.*`/`MidiRowView.*`/`MidiRowLayout.h` у обох таргетах; видалити MidiSetsPlaceholder.* після відв'язки від PluginEditor
- [ ] Детермінізм: hash-based velocity jitter, ЖОДНОГО juce::Random з time-seed; нульовий timing-jitter у v1 (захист EXP-03 bar-alignment)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 5 рядів рендеряться видимими/читабельними міні piano-roll у нижній панелі | GEN-01 | Немає visual-regression тулінгу; піксель-арт легібіліті | Завантажити реальний трек у Standalone, перевірити 5 підписаних рядів у 140px панелі |
| 3 стильові ряди візуально/структурно різні | GEN-02 | «Audibly distinct» перевіриться у Фазі 6 (audition); зараз — візуальний контент | Порівняти патерни нот трьох рядів на одному треку |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (2 manual-only — обґрунтовані)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 90s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-07-13
