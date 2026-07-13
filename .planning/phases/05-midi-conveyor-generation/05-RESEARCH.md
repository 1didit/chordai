# Phase 5: MIDI Conveyor Generation - Research

**Researched:** 2026-07-13
**Domain:** Pure C++ music-generation logic (chord voicing, voice leading, rhythmic patterning) inside a JUCE 8 plugin — no new external dependencies
**Confidence:** MEDIUM-HIGH (data model, threading/wiring, and testing conventions are HIGH — verified directly against this repo's frozen contracts and JUCE 8 official docs; the concrete per-style voicing/rhythm algorithms are MEDIUM — grounded in cross-verified production-technique sources and standard voice-leading theory, but they are product/music-design decisions, not library facts, so treat the specific numbers as a starting point to confirm by ear once Phase 6 audition exists)

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-------------------|
| GEN-01 | One analysis pass produces multiple MIDI rows simultaneously: detected-as-is + style variants + bass | `generateAllRows()` orchestrator (Architecture Pattern 1) fans one `AnalysisResult` out into 5 `MidiSetRow`s in one synchronous call; wiring pattern reuses `analysisBroadcaster` (Pattern 4) so all rows appear together, atomically, exactly when analysis completes |
| GEN-02 | Pop/Hip-hop/Trap (triads, dark minor), R&B/Neo-soul (7th/9th/11th, smooth voice leading), Electronic/House (stabs, rhythmic patterns) — applied to the *detected* progression, not preset lookups | Concrete per-style algorithms in "Voicing Engines Per Style" (Architecture Pattern 2), each takes the real `ChordSegment.chord` (pitchClass+quality) as input, never a static table lookup; "Style Distinctness" test category directly targets the "audibly distinct" success criterion |
| GEN-03 | Bass row follows detected chord roots with style-appropriate rhythm | "Bass Row Generation" (Architecture Pattern 3) — root pitch class always derived from `chord.pitchClass`, three concrete per-style rhythm patterns (trap sustain, R&B root+fifth walk, house four-on-the-floor) |
| GEN-04 | Rows regenerate when the user changes the analysis region or style settings | "Regeneration Wiring" (Architecture Pattern 4) reuses the existing generation-guarded `triggerAnalysis()`/`analysisBroadcaster` pattern (04-01's established precedent) — region change already re-triggers analysis, which now also regenerates rows synchronously in the same completion callback; "style settings" has no UI yet in v1 scope (flagged in Open Questions with a forward-compatible `GenerationSettings` parameter) |

</phase_requirements>

## Summary

Phase 5 is pure logic, not new JUCE surface area: everything it needs (immutable `AnalysisResult`, beat-indexed `ChordSegment`s, an established generation-guarded regenerate-on-change wiring pattern, and a Catch2/CTest harness already exercising `PluginProcessor`'s public API) already exists and is frozen from Phases 3-4. The work is: (1) define a small, pure-C++, header-testable data model (`NoteEvent` in beat units, `MidiSetRow` grouping them by style), (2) implement five deterministic generator functions that turn a `ChordSegment` sequence into concrete MIDI note lists per style, and (3) wire a `generateAllRows()` orchestrator into the exact same completion callback that already publishes `analysisResult`, so rows appear atomically with the chord timeline via the existing `analysisBroadcaster`.

The single most important cross-cutting decision is **timing domain**: work entirely in beat units (`ChordSegment.startBeatIndex`/`endBeatIndex`, already 0-based and region-relative per the frozen `AnalysisResult` contract) rather than seconds. This makes bar-alignment (EXP-03, Phase 6) a non-issue by construction — a beat-unit note position is bar-aligned for free, since `AnalysisResult.bpm` is a single global tempo and `barStartBeatIndices` marks every 4th beat. Converting to `juce::MidiFile` ticks later is a one-line multiply (`ticks = beats * ticksPerQuarterNote`), confirmed against JUCE 8's official `MidiFile`/`MidiMessageSequence` docs — Phase 5 does not need to touch `juce::MidiFile` at all, only produce the beat-domain `NoteEvent`s that Phase 6 will convert.

The second most important decision is **determinism**: GEN-04 requires rows to regenerate predictably when the region changes, and every generator (voicing, humanization, rhythmic patterning) must be a pure function of `AnalysisResult` with zero wall-clock or run-to-run randomness — this is both a testability requirement and a product-trust requirement (the same detected progression must always produce the same-sounding rows).

**Primary recommendation:** Build `Source/MidiGen/` as a pure-C++ module (no GUI includes, `juce::String`/basic JUCE types only) with one `generateAllRows(const AnalysisResult&) -> std::vector<MidiSetRow>` entry point; call it synchronously inside `PluginProcessor::triggerAnalysis()`'s existing `onDone` callback, publish via the same atomic-shared_ptr pattern as `analysisResult`, and let the Editor's existing `analysisBroadcaster` listener pick up rows in the same change message that already updates `chordTimeline`.

## Standard Stack

No new external dependencies. Everything below already ships with JUCE 8 (already linked in this project) or is in-house.

### Core
| Component | Purpose | Why Standard |
|-----------|---------|---------------|
| Plain C++ structs (`NoteEvent`, `MidiSetRow`) | Internal MIDI representation for the generation layer | Pure-C++, trivially Catch2-testable without constructing any JUCE audio/GUI object — matches ARCHITECTURE.md's explicit "MidiGen/ has zero JUCE-audio dependency" decision |
| `juce::String` (`juce_core`, already linked) | Row `id`/`label` fields | Already used pervasively (`ChordNameFormatter.h` etc.); no reason to introduce `std::string` inconsistently |
| `juce::jlimit`/`juce::jmax` (`juce_core`) | Clamping MIDI note numbers/registers | Already used throughout `Source/Analysis/*` — reuse, don't hand-roll `std::clamp` wrappers |
| Catch2 v3.7.1 via `ChordAITests` (already wired) | Unit tests for every generator function | Established test infrastructure since Phase 2; no new framework needed |

### Deferred to Phase 6 (do not build now)
| Component | Purpose | When |
|-----------|---------|------|
| `juce::MidiMessageSequence` / `juce::MidiFile` (`juce_audio_basics`, already linked) | Convert `NoteEvent`s to a Standard MIDI File for drag-out/save | Phase 6 (EXP-01/02/03) — Phase 5 only needs to produce beat-domain `NoteEvent`s that convert cleanly; do not build the conversion function in this phase, but see Code Examples for the exact conversion shape so the `NoteEvent` design doesn't paint Phase 6 into a corner |
| `juce::MidiMessage::noteOn/noteOff` | Individual note events inside a `MidiMessageSequence` | Phase 6 |

### Alternatives Considered
| Instead of | Could use | Tradeoff |
|------------|-----------|----------|
| Beat-domain `NoteEvent` (this recommendation) | Seconds-domain `NoteEvent` (store `startSeconds`/`lengthSeconds` like `ChordSegment` does) | Seconds-domain requires re-deriving bar alignment from `bpm` at export time (`beats = seconds * bpm / 60`), reintroducing float rounding exactly at the boundary EXP-03 cares about most (bar alignment). Beat-domain is bar-aligned by construction since it inherits `ChordSegment.startBeatIndex` directly — no reason to choose seconds here. |
| Plain `std::vector<NoteEvent>` per row (this recommendation) | `juce::MidiMessageSequence` per row, built directly during generation | Would couple the generation/voicing logic (the actual differentiator, needs to be trivially unit-testable with exact expected values) to a JUCE ticks/timeFormat concept it doesn't need yet, and makes "assert exact expected notes" tests more awkward (`MidiMessageSequence` pairs note-on/off events, not a flat note list) — defer the JUCE type to the one place that actually needs it (Phase 6 export). |
| Deterministic hash-based humanization jitter (this recommendation) | `juce::Random` seeded once at plugin startup | Breaks GEN-04: the same `AnalysisResult` must regenerate to the *same* rows (region-change-and-back should not audibly change a row that didn't musically change) — a `juce::Random` instance carries mutable state across calls, so two regenerations in the same session would diverge. A per-note deterministic hash of stable inputs (note index + pitch + chord index) has no mutable state and is trivially testable. |

**No installation step needed** — every type used already exists in linked JUCE modules (`juce_core`, and `juce_audio_basics` for the Phase-6-only `MidiFile` boundary).

## Architecture Patterns

### Recommended Project Structure

```
Source/MidiGen/                       # pure C++, zero GUI includes (ARCHITECTURE.md precedent)
├── NoteEvent.h                       # struct { startBeats, lengthBeats, pitch, velocity }
├── MidiSetRow.h                      # struct { id, label, style, notes } + RowStyle enum
├── GenerationSettings.h              # empty v1 placeholder (see Open Questions #1)
├── ChordToneMapper.h                 # header-only: triad/extension interval tables, pitch-class -> MIDI note helpers
├── VoiceLeadingEngine.h              # header-only: nearestOctaveNote() register-aware voice-leading helper
├── Humanization.h                    # header-only: deterministicJitter() velocity variation
├── AsIsRowGenerator.h/.cpp           # detected-as-is row (root-position, exact detected quality)
├── PopTrapVoicing.h/.cpp             # Pop/Hip-hop/Trap: triads, dark/close, half-bar re-strike
├── RnbNeoSoulVoicing.h/.cpp          # R&B/Neo-soul: 7th/9th/11th extensions + voice leading
├── ElectronicHouseVoicing.h/.cpp     # Electronic/House: off-beat stabs
├── BassLineGenerator.h/.cpp          # per-style bass rhythm, root-pitch-class-only
└── MidiRowBuilder.h/.cpp             # generateAllRows() orchestrator — the one public entry point

Source/UI/
├── MidiSetsPanel.h/.cpp              # replaces MidiSetsPlaceholder.h/.cpp; owns 5 MidiRowView children
├── MidiRowView.h/.cpp                # one row's mini piano-roll preview (display-only in Phase 5)
└── MidiRowLayout.h                   # header-only: pitchToY()/beatsToX() pure layout functions (testable, mirrors WaveformMath.h)

Tests/
├── MidiGenFixtures.h                 # hand-built fixture AnalysisResult (no audio rendering needed)
├── ChordToneMapperTests.cpp
├── StyleVoicingTests.cpp             # Pop/Trap + R&B + House + cross-style distinctness assertions
├── BassLineGeneratorTests.cpp
├── MidiRowBuilderTests.cpp           # orchestration, determinism, NoChord handling, performance
├── PluginProcessorMidiGenTests.cpp   # integration: regenerate-on-region-change through the real processor
└── MidiSetsPanelLayoutTests.cpp      # pure UI layout math
```

**File-count note:** the three per-style voicing files (`PopTrapVoicing`, `RnbNeoSoulVoicing`, `ElectronicHouseVoicing`) can be consolidated into one `StyleVoicingGenerators.h/.cpp` if the planner prefers fewer files — functional separation (one function per style, independently testable) matters more than file count. `ChordToneMapper.h`/`VoiceLeadingEngine.h`/`Humanization.h` are small enough to be header-only `inline` functions, matching this repo's existing convention (`ChordTemplates.h`, `ChordNameFormatter.h`, `WaveformMath.h` are all header-only pure helpers; `ChordDecoder.cpp`, `KeyDetector.cpp` are `.cpp` for the larger stateful logic — `MidiRowBuilder.cpp` and the three voicing generators fall on the `.cpp` side of that same line).

### Pattern 1: Beat-domain `NoteEvent` + `MidiSetRow` data model

**What:** A pure struct pair. `NoteEvent` timing is in beat units matching `ChordSegment.startBeatIndex`/`endBeatIndex` directly (1 unit = 1 detected beat = 1 quarter note at `AnalysisResult.bpm`, confirmed 4/4-quarter-note convention by the frozen contract's own comment "`barStartBeatIndices`: v1 every 4th beat index (4/4 assumption)").

```cpp
// Source/MidiGen/NoteEvent.h
#pragma once

// Pure value type -- no JUCE dependency at all. Timing is in BEATS, not
// seconds: 1.0 == one detected beat == one quarter note at AnalysisResult.bpm.
// This makes every generated note bar-aligned by construction (inherits
// ChordSegment's own beat-index alignment), and the eventual Phase 6
// juce::MidiFile export is a one-line multiply: ticks = startBeats *
// ticksPerQuarterNote. See "Code Examples" for that conversion shape.
struct NoteEvent
{
    double startBeats = 0.0;
    double lengthBeats = 0.0;
    int pitch = 60;          // MIDI note number, 0-127 (60 = middle C, matches
                              // Tests/SyntheticFixtures.h's existing "60 + pitchClass"
                              // chord-tone-octave convention -- reuse it, don't invent a new one)
    float velocity = 0.8f;   // 0.0-1.0, matches juce::MidiMessage::noteOn's float overload
};
```

```cpp
// Source/MidiGen/MidiSetRow.h
#pragma once

#include <JuceHeader.h>
#include "NoteEvent.h"
#include <vector>

enum class RowStyle { DetectedAsIs, PopHipHopTrap, RnbNeoSoul, ElectronicHouse, Bass };

struct MidiSetRow
{
    juce::String id;      // stable key: "as-is", "pop-trap", "rnb-neosoul", "house", "bass"
    juce::String label;    // display label: "Detected", "Pop / Trap", "R&B / Neo-Soul", "Electronic / House", "Bass"
    RowStyle style = RowStyle::DetectedAsIs;
    std::vector<NoteEvent> notes;
};
```

**When to use:** Every generator function (`AsIsRowGenerator`, `PopTrapVoicing`, etc.) returns `std::vector<NoteEvent>`; `MidiRowBuilder::generateAllRows` wraps each in a `MidiSetRow` with the right `id`/`label`/`style`.

### Pattern 2: Voicing engines per style (concrete algorithms)

All four voiced rows (as-is, Pop/Trap, R&B, House) share one foundation — turning a `ChordSymbol` (pitchClass + quality) into concrete semitone intervals, matching `Source/Analysis/ChordTemplates.h`'s own root/third/fifth/seventh convention exactly (do not diverge from that table — it's the same interpretation of "quality" the detector itself uses):

```cpp
// Source/MidiGen/ChordToneMapper.h
#pragma once

#include "../Analysis/AnalysisResult.h"
#include <vector>

// Plain triad intervals, identical semitone offsets to ChordTemplates.h's
// buildTemplate() (root/minor-or-major-third/fifth/dominant-seventh) -- reuse
// the same interpretation of "quality" the detector itself used, don't invent
// a second one.
inline std::vector<int> triadIntervals (ChordQuality quality)
{
    switch (quality)
    {
        case ChordQuality::Minor:     return { 0, 3, 7 };
        case ChordQuality::Dominant7: return { 0, 4, 7, 10 };
        case ChordQuality::Major:
        default:                     return { 0, 4, 7 };
    }
}

// R&B/Neo-soul extension sets. Concrete GEN-02 "7th/9th/11th" mapping:
//   Major      -> maj9  (root, 3, 5, maj7, 9)               -- 5 tones
//   Minor      -> min11 (root, b3, 5, b7, 9, 11)             -- 6 tones
//   Dominant7  -> dom9  (root, 3, 5, b7, 9)                  -- 5 tones
// Minor gets the 11th (not Major/Dominant7): an 11th against a major 3rd is a
// dissonant semitone clash (the "avoid note"), but is consonant against a
// minor 3rd -- standard jazz/neo-soul voicing practice, and keeps this table
// a fixed, testable, per-quality lookup rather than a runtime dissonance check.
inline std::vector<int> rnbExtensionIntervals (ChordQuality quality)
{
    switch (quality)
    {
        case ChordQuality::Minor:     return { 0, 3, 7, 10, 14, 17 };
        case ChordQuality::Dominant7: return { 0, 4, 7, 10, 14 };
        case ChordQuality::Major:
        default:                     return { 0, 4, 7, 11, 14 };
    }
}

// Anchors a set of semitone intervals at a concrete MIDI root note, clamped
// into the valid 0-127 range.
inline std::vector<int> intervalsToMidiNotes (int rootMidiNote, const std::vector<int>& intervals)
{
    std::vector<int> notes;
    notes.reserve (intervals.size());
    for (int iv : intervals)
        notes.push_back (juce::jlimit (0, 127, rootMidiNote + iv));
    return notes;
}
```

**Pop/Hip-hop/Trap (`PopTrapVoicing`)** — root-position triads only, always dropped to the plain triad even if the source chord was `Dominant7` (GEN-02 says "triads, dark minor" — extensions are explicitly the R&B row's job, not this one's):
- Register anchor: MIDI 48 (`C3`) + `pitchClass`, i.e. root lands in C3-B3. Chosen deliberately lower/darker than the as-is row's C4 anchor and *not* as low as the bass row's C2 — this matches the researched production guidance that trap chord voicings "leave room in the low end for the 808" (WebSearch, cross-verified across multiple production-technique sources) while still reading as "dark."
- Rhythm: one voicing per `ChordSegment`, but re-struck every 2 beats (half a 4/4 bar) rather than tied across the whole segment — "sustained whole/half-bar notes" per GEN-02. Segment length not evenly divisible by 2 beats: final chunk holds until segment end.
- Humanization: velocity base 0.78, deterministic jitter ±0.06 (see Pattern 5).

**R&B/Neo-soul (`RnbNeoSoulVoicing`)** — extended chords via `rnbExtensionIntervals` above, **with voice leading between consecutive chords** (Pattern 3), register band clamped to MIDI 48-72 (C3-C5) per this phase's own register guidance.

**Electronic/House (`ElectronicHouseVoicing`)** — plain triad (`triadIntervals`, same as Pop/Trap — House stabs don't need extensions per GEN-02's "stabs, rhythmic patterns" phrasing), register anchor MIDI 72 (`C5`) + `pitchClass` for a bright, tight, close voicing. Rhythm: fixed off-beat stab pattern, 4 stabs per 4-beat span starting at the segment's own start (see Pattern 3's "segment-relative rhythm" note):

```cpp
// Beat offsets relative to the start of each 4-beat span within a chord
// segment -- the "and" of every beat, matching the researched house-stab
// convention (WebSearch: "stabs on off-beat 16th-notes... leaving out the
// downbeat"). Short (16th-note) stab length, tight close voicing.
constexpr double kHouseStabOffsetsBeats[4] = { 0.5, 1.5, 2.5, 3.5 };
constexpr double kHouseStabLengthBeats     = 0.25;
```

### Pattern 3: Rhythmic segmentation is chord-segment-relative, not global-bar-grid-relative

**What:** `ChordSegment`s are "beat-boundary-aligned" per the frozen `AnalysisResult.h` contract comment, but **not** guaranteed to start on a bar boundary (`barStartBeatIndices` marks bars in the *overall* beat grid, independent of where chord changes happen to fall). For v1, every rhythmic pattern (Pop/Trap's half-bar re-strike, House's off-beat stabs, R&B's/House's bass walk) is generated **relative to each `ChordSegment`'s own start**, not snapped to the nearest global bar boundary.

**When to use:** Any per-style generator that subdivides a segment into multiple sub-hits.

**Trade-offs:** This is a deliberate v1 simplification — it keeps every generator a pure function of one `ChordSegment` at a time (no need to cross-reference `barStartBeatIndices` mid-generation), and in practice the current chord detector's segments are usually bar-periodic anyway (Phase 3's beat-synchronized aggregation tends to produce segment boundaries that land on bar lines for most real material). If a chord changes mid-bar, the rhythmic pattern restarts at that mid-bar point rather than snapping to the absolute grid — flagged explicitly in Open Questions as a known limitation to revisit if it sounds wrong once Phase 6 audition exists.

```cpp
// Illustrative: chunking a segment into fixed-size re-strike windows,
// relative to the segment's own start (Pop/Trap half-bar sustain shown; the
// same shape drives House's per-beat bass and R&B's root+fifth walk).
inline std::vector<NoteEvent> chunkSegment (double segStartBeats, double segLengthBeats,
                                             double chunkBeats, int pitch, float velocity)
{
    std::vector<NoteEvent> notes;
    for (double offset = 0.0; offset < segLengthBeats; offset += chunkBeats)
    {
        double len = juce::jmin (chunkBeats, segLengthBeats - offset);
        notes.push_back ({ segStartBeats + offset, len, pitch, velocity });
    }
    return notes;
}
```

### Pattern 4: Regeneration wiring — reuse the existing generation-guarded pattern

**What:** `PluginProcessor::triggerAnalysis()` already has exactly the right hook: its `AnalysisPipeline::CompletionCallback onDone` lambda is where `analysisResult` gets published via `std::atomic_store` + `analysisBroadcaster.sendChangeMessage()`. Row generation is pure math over a few dozen chords (trivially sub-millisecond, see Pattern 6) — call `generateAllRows()` synchronously, right there, and publish the result the exact same way.

```cpp
// PluginProcessor.h additions (same atomic_load/atomic_store discipline as
// loadedAudio/analysisResult -- comment already explains why: incomplete
// std::atomic<shared_ptr<T>> on Apple libc++)
std::shared_ptr<const std::vector<MidiSetRow>> getMidiSetRows() const
{
    return std::atomic_load (&midiSetRows);
}
// private:
std::shared_ptr<const std::vector<MidiSetRow>> midiSetRows;
```

```cpp
// PluginProcessor.cpp -- inside triggerAnalysis()'s existing onDone lambda,
// immediately after the existing std::atomic_store(&analysisResult, result):
auto rows = std::make_shared<const std::vector<MidiSetRow>> (
    result != nullptr ? generateAllRows (*result) : std::vector<MidiSetRow>{});
std::atomic_store (&midiSetRows, rows);
// analyzingFlag/analysisProgress/analysisBroadcaster.sendChangeMessage() unchanged below.
```

Also clear `midiSetRows` to empty at the same point `loadAudioFile`'s callback already clears `analysisResult` to `nullptr` on a fresh file drop (same "must never briefly show the old song's data" rule, now extended to rows).

```cpp
// PluginEditor.cpp -- one new line inside changeListenerCallback's existing
// analysisBroadcaster branch, alongside the existing chordTimeline.setResult call:
if (source == &processor.analysisBroadcaster)
{
    chordTimeline.setResult (processor.getAnalysisResult());
    midiSetsPanel.setRows (processor.getMidiSetRows());   // NEW
    // ... existing progress/chunk-fall logic unchanged
}
```
And the same one-line addition inside `handleLoadComplete` (editor-reopen restore path), mirroring how `chordTimeline.setResult` is already restored there.

**When to use:** Any time `AnalysisResult` changes for any reason (fresh load, region change) — this is the *only* trigger needed for GEN-04's "regenerate on region change," since region changes already flow through `triggerAnalysis()` → this same callback.

**Trade-offs:** No new `ThreadPool`/`ChangeBroadcaster` needed — reusing `analysisBroadcaster` means rows and the chord timeline are always visually in sync (same change message, same frame), which is strictly better UX than a separate async row-regeneration path that could theoretically lag behind. The only real cost is coupling row-generation failure (should never realistically throw — it's arithmetic) to the analysis-completion path; acceptable given generation cannot fail in any way `AnalysisResult`'s own construction hasn't already guarded against (see Pitfall 4, NoChord handling).

### Pattern 5: Deterministic humanization

**What:** Small, fixed-seed, pure hash-based velocity jitter — explicitly **not** `juce::Random` seeded from wall-clock time, and **no timing-offset jitter in v1** (onset times stay exactly on the beat grid).

```cpp
// Source/MidiGen/Humanization.h
#pragma once
#include <cstdint>

// Deterministic per-note velocity jitter. Pure function of noteIndex (a
// stable, caller-assigned running count within one generateAllRows() call)
// and a per-style seed constant -- NOT juce::Random with a time-based seed.
// GEN-04 requires that regenerating the SAME AnalysisResult always produces
// the SAME rows (the user must be able to trust that toggling the region
// selector back to an identical range doesn't change how a row sounds) --
// any run-to-run randomness here would silently break that guarantee and
// would also make exact-expected-NoteEvent tests impossible to write.
inline float deterministicJitter (uint32_t noteIndex, uint32_t styleSeed, float range)
{
    uint32_t h = noteIndex * 2654435761u + styleSeed; // Knuth multiplicative hash
    h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16;
    float unit = (float) (h % 10000u) / 9999.0f;      // [0, 1]
    return (unit - 0.5f) * range;                      // [-range/2, range/2]
}
```

**Recommended base velocity / jitter range per row** (product-design defaults, MEDIUM confidence — confirm by ear once Phase 6 audition exists, not sourced from an external spec):

| Row | Base velocity | Jitter range | Rationale |
|-----|---------------|--------------|-----------|
| Detected (as-is) | 0.75, no jitter | 0.0 | Reference/utility row — reads as "raw data," not "produced" |
| Pop/Hip-hop/Trap | 0.78 | ±0.06 | Punchy but not extreme |
| R&B/Neo-soul | 0.62 | ±0.04 | Smoother, softer dynamic range fits the style |
| Electronic/House | 0.88 | ±0.05 | Stabs read as percussive/punchy |
| Bass | 0.85 | ±00.03 | Consistent — bass wants less dynamic variance than chords |

**Why no timing-offset humanization in v1:** the research prompt explicitly flags this as needing a reproducibility answer. Timing jitter risks (a) small negative-time notes crossing segment/bar boundaries, complicating the "exported MIDI is bar-aligned" guarantee (EXP-03) that beat-domain `NoteEvent`s currently provide "for free," and (b) doubles the state that must stay deterministic for GEN-04. Recommend: keep onset times exactly quantized to the beat grid in v1; leave a documented extension point (an unused `humanizeSeed` field on `GenerationSettings`, see Open Questions #1) for a v1.x timing-humanization pass once the audition/export path (Phase 6) exists to actually judge the result by ear.

### Pattern 6: Synchronous generation, measured not assumed

**What:** Generation is pure arithmetic over `AnalysisResult.chords` (typically dozens of segments) × 5 rows × a handful of notes each — a few hundred `NoteEvent`s total, no I/O, no allocation-heavy work beyond `std::vector` growth. Run it synchronously on the message thread inside the existing `onDone` callback (Pattern 4) — no `ThreadPool` job needed.

**When to use:** Confirm this empirically, following this project's own established "measure, don't assume" discipline (Phase 4's Release-build timing measurement, 04-04, is the precedent) — add one Catch2 timing assertion (not a hard requirement, just a documented sub-millisecond expectation) against a real-track-sized fixture (`~150 chord segments`, matching the scale seen in the 75s `TOCK.mp3` real-track harness already used by `ClassicDspChordAnalyzerTests`).

**Trade-offs:** If a future v1.x style adds genuinely expensive computation (unlikely for rule-based voicing, but worth naming), the same generation-guarded `ThreadPool` pattern already proven for `AnalysisPipeline` is directly reusable without redesigning the wiring — Pattern 4's synchronous call is a `MidiRowBuilder::generateAllRows(result)` call site, trivially swappable for a `ThreadPoolJob` later if the measured time ever stops being trivial.

### Anti-Patterns to Avoid

- **Coupling `MidiGen/` to `Source/Analysis/` internals beyond `AnalysisResult.h`:** already an explicit Anti-Pattern in `.planning/research/ARCHITECTURE.md` — every generator function must take `const AnalysisResult&` (or a single `const ChordSegment&`), never chroma vectors, HMM states, or other analyzer-internal types. Enforced by folder boundary (`Source/MidiGen/` never `#include`s anything under `Source/Analysis/` except `AnalysisResult.h`/`ChordTemplates.h` for the shared interval-offset convention).
- **Re-deriving bar alignment from seconds/BPM inside a generator:** don't call `chord.startSeconds * bpm / 60.0` anywhere in `MidiGen/` — every `ChordSegment` already carries `startBeatIndex`/`endBeatIndex`, which is the bar-aligned source of truth. Recomputing from seconds reintroduces float rounding at exactly the point EXP-03 cares about.
- **Generating one `MidiSetRow` per detected chord instead of one row spanning the whole progression:** `MidiSetRow.notes` holds every `NoteEvent` for the *entire* analyzed region for that style — five rows total (not five rows per chord). Each generator iterates `result.chords` internally and appends to one flat `notes` vector.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|--------------|-----|
| MIDI file writing / SMF byte format | A custom `.mid` binary writer | `juce::MidiFile`/`juce::MidiMessageSequence` (Phase 6 only — confirmed API in Code Examples below) | JUCE's writer is spec-correct, already linked, and zero-cost to defer — Phase 5 never needs to touch it |
| Individual note-on/off byte packing | Manual MIDI status-byte construction | `juce::MidiMessage::noteOn(channel, note, velocity)`/`noteOff(...)` (Phase 6, confirmed via docs.juce.com: channel 1-16, velocity accepts either `float` 0-1 or `uint8` 0-127) | Same reasoning — this is a solved, official-API problem, not something Phase 5's generators need to reach for at all |
| General music-theory chord-spelling library (arbitrary chord symbols, alterations, slash chords, inversions-as-input) | A generalized chord-parsing/spelling engine | The fixed `ChordQuality` enum (Major/Minor/Dominant7/NoChord) + fixed interval-offset tables in `ChordToneMapper.h`, mirroring `ChordTemplates.h`'s existing convention exactly | The detector's own frozen contract only ever produces these 4 qualities — building a more general theory engine here would be solving a problem the input data structurally cannot pose (over-engineering, contradicts the project's own "zero-theory workflow" positioning from FEATURES.md) |
| Run-to-run random humanization | `juce::Random`/`std::mt19937` seeded at construction or wall-clock time | The small deterministic hash in `Humanization.h` (Pattern 5) | JUCE's `Random` and the standard library's PRNGs are correct tools for *actual* randomness, but this problem needs *reproducible pseudo-variation*, not randomness — no existing library targets that specific need, so the small in-house hash is the right amount of custom code, not a violation of "don't hand-roll" |

**Key insight:** almost nothing in this phase should reach for a library at all — it's rule-based music generation over an already-frozen, already-simple data contract (4 chord qualities, single global BPM, 4/4 assumption). The "don't hand-roll" risk in this phase is the opposite direction from most phases: over-building a general-purpose music-theory/voicing engine when the actual input space is small and fixed. Keep every table a literal fixed lookup, not a parameterized rule system, until real user feedback demands more.

## Common Pitfalls

### Pitfall 1: NoChord segments emitting garbage notes
**What goes wrong:** `ChordSegment.chord.quality` can be `ChordQuality::NoChord` (silence/no detected harmonic content — `AnalysisResult`'s frozen contract explicitly includes this value). A generator that doesn't special-case it will either crash (interval tables above assert/branch only on Major/Minor/Dominant7 in `ChordTemplates.h`'s existing convention) or silently emit a bogus C-major chord.
**Why it happens:** Easy to forget when writing the "happy path" voicing logic first and only handling the 3 real qualities.
**How to avoid:** Every generator's per-segment loop starts with `if (segment.chord.quality == ChordQuality::NoChord) continue;` — no notes emitted for that segment, in every row including bass. `Tests/fixtures/silence_1s.mp3` and existing Phase 3 synthetic fixtures already produce `NoChord` segments; reuse them as the negative-case fixture rather than inventing a new one.
**Warning signs:** A generated row containing a note during a passage the user knows was silent/unpitched in the source audio.

### Pitfall 2: Non-deterministic regeneration breaking GEN-04's trust contract
**What goes wrong:** Any wall-clock-seeded or mutable-state RNG in humanization (or anywhere else in the generation path) means regenerating an *identical* `AnalysisResult` (e.g., dragging the region handle back to exactly where it was) produces audibly different rows.
**Why it happens:** `juce::Random` is the reflexive JUCE choice for "add some variation," and its default constructor seeds from the system clock.
**How to avoid:** Pattern 5's deterministic hash, and a dedicated regression test (`MidiRowBuilderTests.SameInputProducesByteIdenticalRows`) that calls `generateAllRows()` twice on the same fixture and asserts every `NoteEvent` field is exactly equal.
**Warning signs:** A test that passes once and fails intermittently, or a `juce::Random` instance anywhere under `Source/MidiGen/`.

### Pitfall 3: Register clamp forcing voice-crossing on extended R&B chords
**What goes wrong:** The R&B voice-leading register clamp (MIDI 48-72, Pattern 2) can force a high extension tone (e.g. the 11th, +17 semitones, on a chord rooted near the top of the register band) down below another chord tone that started lower, producing an audible "crossed voices" artifact.
**Why it happens:** A hard register clamp is a simplification of proper voice-leading (which would reassign voices rather than just clamp one tone's octave) — acceptable for v1's rule-based approach but a real limitation.
**How to avoid:** Not fully avoidable without a more complex assignment-based voice-leading algorithm (explicitly out of scope for v1 per SUMMARY.md's own note that "voicing-rule design is a product/music decision more than a technical-research one" — don't over-build this in Phase 5). Document the limitation, and add it to Open Questions as a candidate v1.x refinement once Phase 6 audition makes it audible/judgeable.
**Warning signs:** A voice-leading test fixture with a high-root minor chord (e.g. B minor, pitchClass 11) producing an 11th that lands below the chord's own fifth.

### Pitfall 4: Segment-relative rhythm drifting from the visual bar grid
**What goes wrong:** Because rhythmic patterns are generated relative to each `ChordSegment`'s own start (Pattern 3), a chord that changes mid-bar produces stabs/re-strikes that don't line up with the bar lines the `ChordTimelineView` (Phase 4) visually draws over the waveform — a user comparing the two could perceive a mismatch.
**Why it happens:** Deliberate v1 simplification to keep every generator a pure per-segment function (see Pattern 3's trade-off discussion).
**How to avoid:** Not a Phase 5 defect to fix now — flagged as an accepted v1 simplification. If it proves visually/audibly confusing once rows are previewable (Phase 6), the fix is to compute rhythmic offsets relative to the nearest preceding `barStartBeatIndices` entry instead of the segment start — a contained change inside the affected generators only, no data-model change needed.
**Warning signs:** User feedback that a style row's rhythm "feels offset" from the visible bar grid on a track with frequent mid-bar chord changes.

### Pitfall 5: Testing "distinctness" by note count instead of content
**What goes wrong:** GEN-02's success criterion is that the three style rows are "audibly distinct (content-wise)." A shallow test (`CHECK(popRow.notes.size() != rnbRow.notes.size())`) can pass while two styles coincidentally produce overlapping pitch/rhythm content for a specific test chord, giving false confidence.
**Why it happens:** Note-count is the easiest thing to assert; content-set comparison requires slightly more test-fixture design.
**How to avoid:** Write an explicit `StyleDistinctnessTests` case that, for the same fixture chord, asserts the *pitch class multisets* (not just counts) differ pairwise across Pop/Trap, R&B, and House, and that at least one style's rhythm (onset beat positions) differs from the others.
**Warning signs:** A distinctness test that only checks `.size()`.

## Code Examples

### `generateAllRows` orchestrator (the one public entry point)

```cpp
// Source/MidiGen/MidiRowBuilder.h
#pragma once
#include "../Analysis/AnalysisResult.h"
#include "MidiSetRow.h"
#include "GenerationSettings.h"

// The single entry point PluginProcessor calls. Pure function: same
// AnalysisResult (+ settings) always produces byte-identical output
// (see Pitfall 2). Empty AnalysisResult.chords (or all-NoChord) produces 5
// rows with empty `notes` vectors, never a crash.
std::vector<MidiSetRow> generateAllRows (const AnalysisResult& result,
                                          const GenerationSettings& settings = {});
```

```cpp
// Source/MidiGen/MidiRowBuilder.cpp (shape, not full listing)
#include "MidiRowBuilder.h"
#include "AsIsRowGenerator.h"
#include "PopTrapVoicing.h"
#include "RnbNeoSoulVoicing.h"
#include "ElectronicHouseVoicing.h"
#include "BassLineGenerator.h"

std::vector<MidiSetRow> generateAllRows (const AnalysisResult& result, const GenerationSettings& settings)
{
    return {
        { "as-is",       "Detected",               RowStyle::DetectedAsIs,   generateAsIsRow (result) },
        { "pop-trap",    "Pop / Trap",              RowStyle::PopHipHopTrap,  generatePopTrapRow (result, settings) },
        { "rnb-neosoul", "R&B / Neo-Soul",          RowStyle::RnbNeoSoul,     generateRnbNeoSoulRow (result, settings) },
        { "house",       "Electronic / House",      RowStyle::ElectronicHouse, generateElectronicHouseRow (result, settings) },
        { "bass",        "Bass",                    RowStyle::Bass,           generateBassRow (result, settings) },
    };
}
```

### Register/root helper (shared by every generator)

```cpp
// pitchClass: 0-11, matches AnalysisResult.h's 0=C..11=B convention exactly.
// anchorOctaveBase: the style's chosen register anchor (see Pattern 2/Bass table).
inline int rootMidiNote (int pitchClass, int anchorOctaveBase)
{
    return juce::jlimit (0, 127, anchorOctaveBase + pitchClass);
}
```

| Row | Anchor (MIDI) | Anchor note | Precedent |
|-----|----------------|-------------|-----------|
| Detected (as-is) | 60 | C4 | `Tests/SyntheticFixtures.h`'s existing "chord tones around octave 4" (`60 + pc`) |
| Pop/Hip-hop/Trap | 48 | C3 | Dark/close per researched trap production guidance |
| R&B/Neo-soul | 60 (first chord seed only; then voice-led, clamped to 48-72) | C4 seed | Per this phase's own "register ~C3-C5" guidance |
| Electronic/House | 72 | C5 | Bright/tight per researched house-stab guidance |
| Bass (all styles) | 36 | C2 | `Tests/SyntheticFixtures.h`'s existing "bass note ~2 octaves below" (`36 + bassPitchClass`) |

### R&B voice-leading between consecutive chords

```cpp
// Source/MidiGen/VoiceLeadingEngine.h
#pragma once
#include <JuceHeader.h>

// Places `noteClass` (a 0-11 pitch class, already reduced mod 12) in the
// octave nearest to `previousNote`, clamped into [registerLow, registerHigh].
// Deterministic: depends only on its three inputs, no history/state beyond
// the single previous note the caller passes in.
inline int nearestOctaveNote (int noteClass, int previousNote, int registerLow = 48, int registerHigh = 72)
{
    int best = noteClass;
    int bestDist = std::abs (best - previousNote);
    for (int candidate = ((noteClass % 12) + 12) % 12; candidate <= 127; candidate += 12)
    {
        int dist = std::abs (candidate - previousNote);
        if (dist < bestDist) { bestDist = dist; best = candidate; }
    }
    return juce::jlimit (registerLow, registerHigh, best);
}
```

Usage inside `RnbNeoSoulVoicing.cpp`: the first chord in the progression anchors directly at `rootMidiNote(chord.pitchClass, 60)` plus `rnbExtensionIntervals`; every subsequent chord computes each extension tone's target pitch class (`(chord.pitchClass + interval) % 12`), then calls `nearestOctaveNote(targetClass, previousChordMeanPitch)` — where `previousChordMeanPitch` is the mean MIDI pitch of the *previous* chord's voiced notes — to place it near the previous voicing, minimizing perceived jump distance (the practical, testable approximation of "minimal-motion" voice leading this phase adopts; see Pitfall 3 for its known limitation).

### Phase 6 conversion point (reference only — do not implement in Phase 5)

Confirmed against JUCE 8's official docs (`docs.juce.com/master/classMidiFile.html`, `classMidiMessageSequence.html`, `classMidiMessage.html`) so the `NoteEvent` shape above doesn't paint Phase 6 into a corner:

```cpp
// Phase 6 shape, NOT part of this phase's deliverable:
juce::MidiMessageSequence toMidiSequence (const std::vector<NoteEvent>& notes, int ticksPerQuarterNote)
{
    juce::MidiMessageSequence seq;
    for (const auto& n : notes)
    {
        double onTick  = n.startBeats * ticksPerQuarterNote;
        double offTick = (n.startBeats + n.lengthBeats) * ticksPerQuarterNote;
        seq.addEvent (juce::MidiMessage::noteOn  (1, n.pitch, n.velocity), onTick);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch),             offTick);
    }
    seq.updateMatchedPairs();
    seq.sort();
    return seq;
}

// juce::MidiFile file;
// file.setTicksPerQuarterNote (960);   // must be set BEFORE addTrack
// file.addTrack (toMidiSequence (row.notes, 960));
// file.writeTo (outputStream);
```
`MidiMessageSequence::addEvent(message, timeAdjustment)` timestamps are in whatever unit the caller chooses consistently — for `MidiFile` output, that unit must be **ticks** (confirmed: "the timestamps... will represent their positions in terms of midi ticks", docs.juce.com), matching `setTicksPerQuarterNote`'s resolution. `MidiMessage::noteOn/noteOff` channel is 1-based (1-16); velocity accepts either a `float` (0-1, used above, matches `NoteEvent.velocity`'s type) or `uint8` (0-127) overload.

## State of the Art

| Approach | Who uses it | ChordAI's approach |
|----------|-------------|----------------------|
| Static preset-progression library (browse pre-written progressions by genre tag) | Captain Chords, Chordjam, Chord Genie | Style engines transform the user's *own detected* progression — never a lookup table (FEATURES.md's "genre-specific voicing engine applied to your detected progression" differentiator, directly implemented by Pattern 2's `ChordSegment`-driven generators) |
| Manual, sequential, one-chord-at-a-time "suggest" assistance | Scaler 2's Suggest mode | One `generateAllRows()` call fans out synchronously into 5 finished, independently-usable rows per analysis pass — no per-chord user interaction required |

No further "old vs. new" migration applies here — this is greenfield logic with no prior implementation in this codebase to deprecate.

## Open Questions

1. **GEN-04's "style settings change" clause has no corresponding UI/settings surface in v1 scope**
   - What we know: REQUIREMENTS.md's GEN-04 text says rows "regenerate when the user changes the analysis region or style settings," but no phase (5, 6, or later) currently defines a user-facing style-configuration control — v1's 3 styles are fixed, always-generated, non-configurable per GEN-01/02's own wording.
   - What's unclear: Whether "style settings" is forward-looking language anticipating a not-yet-specified future control, or an oversight in the requirement text.
   - Recommendation: `generateAllRows(result, settings)` already accepts a `GenerationSettings` parameter (currently empty) specifically so a future settings-changed callback can call the *same* function without a shape change — region-change regeneration is fully solved today via Pattern 4; leave the settings half as an explicit no-op placeholder rather than building speculative UI. Confirm with the user at plan-check whether any v1 style toggle is actually wanted, or whether this is purely forward-compatible plumbing.

2. **Exact register anchors, velocity/jitter numbers, and MidiRowView pixel-art palette are product/design defaults, not locked decisions**
   - What we know: `02-CONTEXT.md` (Phase 2) explicitly places "pixel-art style specifics (palette...)" at Claude's discretion, and this phase's numeric choices (Pattern 2/5's anchors and velocities) are cross-verified against production-technique research but are not literal specifications from any source.
   - What's unclear: Whether the specific numbers above will "feel right" once actually audible (Phase 6).
   - Recommendation: Treat every number in this document as a well-reasoned starting point, not a frozen contract (unlike `AnalysisResult.h`/`ChordAnalyzer.h`) — safe to adjust during planning or after Phase 6 listening tests without re-deriving the algorithm shapes themselves.

3. **Voice-leading register clamp can force voice-crossing on extended R&B chords (Pitfall 3)**
   - What we know: the nearest-octave-with-clamp algorithm (Pattern 2/Code Examples) is a practical approximation, not a full assignment-based voice-leading optimizer.
   - What's unclear: Whether this is audible enough to matter for the fixed 3-style v1 test set.
   - Recommendation: Ship the simple version now (matches SUMMARY.md's own guidance that voicing-rule design is a music decision, not a research gap to solve exhaustively pre-implementation); revisit only if Phase 6 audition surfaces a real problem.

4. **Segment-relative (not global-bar-grid-relative) rhythmic patterning (Pitfall 4)**
   - What we know: chosen for generator simplicity/purity; documented trade-off above.
   - What's unclear: How often real detected progressions actually change chord mid-bar (untested against a large real-track corpus).
   - Recommendation: Ship as-is; treat any user-visible "rhythm looks offset from the grid" report as the trigger to revisit, not a pre-emptive fix.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 via CTest (already wired, `ChordAITests` target) |
| Config file | `CMakeLists.txt` (root) — `catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.")` |
| Quick run command | `ctest --test-dir build -R "ChordAITests.(ChordToneMapper\|StyleVoicing\|BassLineGenerator\|MidiRowBuilder)" --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|--------------------|-------------|
| GEN-01 | `generateAllRows()` returns exactly 5 rows (as-is + 3 styles + bass) with correct `RowStyle`/`id`/`label` for a known multi-chord fixture | unit | `ctest -R ChordAITests.MidiRowBuilderTests.FiveRowsForKnownProgression` | ❌ Wave 0 |
| GEN-01 | The as-is row's chord content matches the detected progression exactly (root-position triad/7th per segment, no transformation) | unit | `ctest -R ChordAITests.MidiRowBuilderTests.AsIsRowMatchesDetectedProgression` | ❌ Wave 0 |
| GEN-01 | Rows appear together (same `analysisBroadcaster` message) — not staggered across multiple UI updates | unit/integration | `ctest -R ChordAITests.PluginProcessorMidiGenTests.RowsPublishedSynchronouslyWithAnalysisResult` | ❌ Wave 0 |
| GEN-02 | Pop/Trap row: plain triad only (7th dropped even from a Dominant7 source chord), dark/close register (MIDI 48-66 band) | unit | `ctest -R ChordAITests.StyleVoicingTests.PopTrapTriadOnlyCloseRegister` | ❌ Wave 0 |
| GEN-02 | R&B row: exact extension-interval set per quality (maj9/min11/dom9 per `rnbExtensionIntervals`) | unit | `ctest -R ChordAITests.StyleVoicingTests.RnbExtensionsMatchQualityTable` | ❌ Wave 0 |
| GEN-02 | R&B row: voice leading reduces total movement across a chord transition vs. a naive "always re-anchor at the same register" baseline | unit | `ctest -R ChordAITests.StyleVoicingTests.RnbVoiceLeadingMinimizesMovement` | ❌ Wave 0 |
| GEN-02 | House row: exact 4-stab-per-4-beat-span pattern (0.5/1.5/2.5/3.5 beat offsets, 0.25-beat length), tight close triad | unit | `ctest -R ChordAITests.StyleVoicingTests.HouseStabPatternExactTiming` | ❌ Wave 0 |
| GEN-02 | Cross-style distinctness: for the same fixture chord, Pop/Trap, R&B, and House produce different pitch-class multisets and/or different rhythm | unit | `ctest -R ChordAITests.StyleVoicingTests.ThreeStylesProduceDistinctContent` | ❌ Wave 0 |
| GEN-02 | Style variants are built from the actual detected progression (change the fixture's chords, output changes accordingly) — not a static table lookup | unit | `ctest -R ChordAITests.StyleVoicingTests.OutputTracksInputProgression` | ❌ Wave 0 |
| GEN-03 | Bass row root pitch class matches `chord.pitchClass` exactly for every non-`NoChord` segment, across all 3 rhythm styles | unit | `ctest -R ChordAITests.BassLineGeneratorTests.RootPitchClassMatchesDetectedChord` | ❌ Wave 0 |
| GEN-03 | Trap bass: one sustained note per segment (sparse, full segment length, low register ~C2) | unit | `ctest -R ChordAITests.BassLineGeneratorTests.TrapBassSustainsFullSegment` | ❌ Wave 0 |
| GEN-03 | R&B bass: root+fifth walk on a 4-beat segment (first half root, second half fifth); shorter segments sustain root only | unit | `ctest -R ChordAITests.BassLineGeneratorTests.RnbBassRootFifthWalk` | ❌ Wave 0 |
| GEN-03 | House bass: one note per beat (four-on-the-floor), short length, same register | unit | `ctest -R ChordAITests.BassLineGeneratorTests.HouseBassFourOnTheFloor` | ❌ Wave 0 |
| GEN-04 | Changing the selected region (via `PluginProcessor::setSelectedRegion`) re-triggers analysis and regenerates rows to match the new region's detected progression | unit/integration | `ctest -R ChordAITests.PluginProcessorMidiGenTests.RowsRegenerateOnRegionChange` (drive via the processor's public API, message-pump technique already established in `AnalysisPipelineTests`/`WaveformRegionTests`) | ❌ Wave 0 |
| GEN-04 | Regenerating the same `AnalysisResult` twice produces byte-identical `NoteEvent` sequences (determinism guard, Pitfall 2) | unit | `ctest -R ChordAITests.MidiRowBuilderTests.SameInputProducesByteIdenticalRows` | ❌ Wave 0 |
| (supporting) | `NoChord` segments emit zero notes across every row (Pitfall 1) | unit | `ctest -R ChordAITests.MidiRowBuilderTests.NoChordSegmentEmitsNoNotes` | ❌ Wave 0 |
| (supporting) | `generateAllRows()` completes in well under 1ms on a real-track-sized fixture (Pattern 6 — measured, not assumed) | unit (timing) | `ctest -R ChordAITests.MidiRowBuilderTests.GenerationPerformanceBudget` | ❌ Wave 0 |
| (supporting, UI) | `MidiRowLayout.h`'s `pitchToY`/`beatsToX` pure functions are mathematically correct (mirrors `ChordTimelineLayoutTests` pattern) | unit | `ctest -R ChordAITests.MidiSetsPanelLayoutTests.PitchToYAndBeatsToX` | ❌ Wave 0 |
| GEN-01 (visual) | 5 rows render as visible, distinguishable mini piano-roll strips in the Standalone editor's bottom band | manual-only | Manual: load a real track in Standalone, confirm 5 labeled rows are visible and legible in the existing 140px band (28px/row); justification: pixel-art visual legibility has no automated visual-regression tooling in this project (same justification pattern as Phase 4's manual chord-timeline-legibility check) | n/a |
| GEN-02 (audible content, not sound) | The 3 style rows read as visually/structurally distinct in the piano-roll preview even though actual audio audition is Phase 6 | manual-only | Manual: visual comparison of the 3 style rows' note patterns in Standalone against the same source track; justification: this phase's success criterion is explicitly "audibly distinct (content-wise)... audition sound itself is Phase 6" — visual content distinctness is checkable now, audible distinctness is not until Phase 6 exists | n/a |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -R "ChordAITests.(ChordToneMapper|StyleVoicing|BassLineGenerator|MidiRowBuilder)" --output-on-failure`
- **Per wave merge:** Full `ctest --test-dir build --output-on-failure` + a `pluginval` strict-mode pass (VST3+AU), matching Phase 2/3/4's own established gate
- **Phase gate:** Full suite green + manual checkpoint (5 rows visible in Standalone, legible, visually distinct per style, regenerate on a region-selector drag) before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `Tests/MidiGenFixtures.h` — hand-built fixture `AnalysisResult` (no audio rendering needed, unlike `Tests/SyntheticFixtures.h`; a literal struct-literal progression with known BPM/beat grid/chords is sufficient and faster to write/read than an audio-rendering fixture)
- [ ] `Tests/ChordToneMapperTests.cpp`
- [ ] `Tests/StyleVoicingTests.cpp`
- [ ] `Tests/BassLineGeneratorTests.cpp`
- [ ] `Tests/MidiRowBuilderTests.cpp`
- [ ] `Tests/PluginProcessorMidiGenTests.cpp`
- [ ] `Tests/MidiSetsPanelLayoutTests.cpp`
- [ ] `Source/MidiGen/*` sources added to both `ChordAI` and `ChordAITests` CMake targets (new `CHORDAI_MIDIGEN_SOURCES` list, mirroring the existing `CHORDAI_ANALYSIS_SOURCES` pattern in `CMakeLists.txt`)
- [ ] `Source/UI/MidiSetsPanel.*`/`MidiRowView.*`/`MidiRowLayout.h` added to both targets; `Source/UI/MidiSetsPlaceholder.*` removed from both targets' source lists (and the files deleted) once `PluginEditor.h`/`.cpp` no longer reference it
- Framework install: none — Catch2/CTest already fully wired since Phase 2

## Sources

### Primary (HIGH confidence)
- `Source/Analysis/AnalysisResult.h` (this repo, frozen contract) — beat-index/`ChordSegment`/`ChordQuality` shape
- `Source/Analysis/ChordTemplates.h` (this repo) — canonical root/third/fifth/seventh interval convention, reused verbatim by `ChordToneMapper.h`
- `Source/PluginProcessor.h`/`.cpp`, `Source/PluginEditor.h`/`.cpp` (this repo) — existing `analysisBroadcaster`/generation-guard/atomic-publish wiring pattern, directly reused
- `Tests/SyntheticFixtures.h` (this repo) — existing register-anchor convention (`60 + pc` chord tones, `36 + bassPitchClass` bass), reused rather than reinvented
- [docs.juce.com/master/classMidiMessageSequence.html](https://docs.juce.com/master/classMidiMessageSequence.html) — `addEvent`, `updateMatchedPairs`, `sort` signatures (fetched directly, HIGH confidence, official docs)
- [docs.juce.com/master/classMidiFile.html](https://docs.juce.com/master/classMidiFile.html) — `setTicksPerQuarterNote`, `addTrack`, `writeTo`; confirms event timestamps are in ticks, matching `setTicksPerQuarterNote`'s resolution (fetched directly, HIGH confidence)
- [docs.juce.com/master/classMidiMessage.html](https://docs.juce.com/master/classMidiMessage.html) — `noteOn`/`noteOff` exact overload signatures, 1-based channel confirmation (fetched directly, HIGH confidence)
- `.planning/research/ARCHITECTURE.md`, `.planning/research/STACK.md`, `.planning/research/SUMMARY.md` (this project's own prior research) — `Source/MidiGen/` module naming/structure precedent, `juce::MidiFile` stack decision already made

### Secondary (MEDIUM confidence)
- WebSearch: "nearest neighbor voice leading algorithm chord voicing minimal movement inversion selection" — cross-referenced academic/practical voice-leading-minimization sources (Dmitri Tymoczko's chord-space geometry work, CMUSE voice-leading calculator write-up) grounding the nearest-octave-placement algorithm as a recognized simplification of proper voice-leading optimization
- WebSearch: "trap hip hop chord voicing dark minor close position octave root MIDI production technique" — cross-referenced production-technique articles (Unison Audio, Chordoo) confirming "leave low-end space for the 808," minor/dark tonality, close voicing conventions used to justify Pattern 2's Pop/Trap register choice
- WebSearch: "neo soul R&B chord voicing 9th 11th smooth voice leading register piano production" — cross-referenced multiple piano-lesson/production sources confirming the 9th/11th extension convention and "common tone on top" voice-leading practice, informing `rnbExtensionIntervals`
- WebSearch: "house music chord stab rhythm pattern offbeat production technique four on the floor bass" — cross-referenced production-technique articles (Attack Magazine, EDMProd, Producerstack) confirming off-beat/16th-note stab placement and four-on-the-floor bass convention, informing Pattern 2/House bass rhythm

### Tertiary (LOW confidence, flagged for validation)
- Exact numeric defaults (velocity bases/jitter ranges, specific register anchor MIDI numbers beyond the two reused-from-fixtures anchors, exact stab beat offsets) are this document's own designed defaults, not drawn from an external spec — flagged throughout as "confirm by ear once Phase 6 audition exists," not treated as authoritative

## Metadata

**Confidence breakdown:**
- Data model / timing domain (beat-based `NoteEvent`, ticks conversion): HIGH — verified directly against this repo's frozen `AnalysisResult` contract and JUCE 8's official `MidiFile`/`MidiMessageSequence`/`MidiMessage` docs
- Wiring / regeneration pattern: HIGH — directly reuses this repo's own established, already-shipping generation-guarded `analysisBroadcaster` pattern (04-01), no new architecture invented
- Per-style voicing/rhythm algorithms: MEDIUM — grounded in cross-verified production-technique research and standard voice-leading theory, but numeric specifics are product/design decisions this document proposes rather than facts it verifies; explicitly flagged as adjustable
- Testing strategy: HIGH — directly extends this repo's own established Catch2/CTest conventions and naming patterns (verified against `Tests/ChordTimelineLayoutTests.cpp`, `Tests/AnalysisPipelineTests.cpp`, `.planning/phases/04-analysis-ui-integration/04-RESEARCH.md`'s Validation Architecture section)

**Research date:** 2026-07-13
**Valid until:** No external dependency exists to go stale (pure in-house logic + already-linked JUCE APIs) — revalidate only if `AnalysisResult.h`'s frozen contract changes, or after Phase 6 audition surfaces a voicing/rhythm change request (i.e., event-driven, not time-driven staleness)

---
*Research for: Phase 5 - MIDI Conveyor Generation*
*Researched: 2026-07-13*
