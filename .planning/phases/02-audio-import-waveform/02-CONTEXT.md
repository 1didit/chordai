# Phase 2: Audio Import & Waveform - Context

**Gathered:** 2026-07-12
**Status:** Ready for planning
**Source:** User design direction (conversation, 2026-07-12)

<domain>
## Phase Boundary

Phase 2 delivers: drag-and-drop audio file import (WAV/MP3/AIFF/FLAC), waveform display, analysis-region selection — AND establishes the plugin's visual identity: the pixel-art conveyor belt UI metaphor that the whole product is built around.

</domain>

<decisions>
## Implementation Decisions

### Conveyor UI metaphor (USER DECISION — locked)
- The plugin window features a **pixel-art animated conveyor belt** running **left → right** across the window
- Dropping a song/segment onto the window feeds it onto the conveyor (the drop target IS the conveyor area)
- At the right end of the conveyor, **piano-roll note chunks "fall off"** — this is the visual for generated MIDI output (full behavior lands in Phases 4-6 when analysis/generation exist; Phase 2 establishes the conveyor visual + animation framework and the drop interaction)
- **Bottom of the window: a list of the generated chord/melody MIDI sets** (the conveyor rows) for piano roll — Phase 2 reserves this layout region (placeholder/empty state); Phase 5 populates it, Phase 6 makes rows auditionable/draggable

### Phase 2 scope of the metaphor
- Conveyor belt animation visible and running (idle loop) in the editor
- File drop onto the conveyor area loads the file; the loaded waveform is displayed (waveform can sit on/above the belt — layout at Claude's discretion)
- Region selection on the waveform per REQUIREMENTS (IMP-03)
- Falling piano-roll chunks: at most a visual stub/placeholder animation trigger — real chunks appear only when generation exists (deferred)

### Claude's Discretion
- Pixel-art style specifics (palette, tile size, frame count/rate), implemented with JUCE Graphics/Timer — no heavy dependencies
- Exact layout proportions (conveyor strip height, waveform area, bottom list height)
- Animation performance approach (Timer-driven repaint of the belt region only; must not violate RT-safety — animation lives entirely on the message thread)
- Empty-state design for the bottom sets list

</decisions>

<specifics>
## Specific Ideas

- User's words: "піксельна анімація конвеєра який рухається зліва направо і при закидуванні туди відрізку трека з конвеєра справа випадають ноти piano roll куски"; "знизу список цих наборів акордів чи мелодій для piano roll"
- The conveyor is not decoration — it's the product metaphor (song in on the left → MIDI sets out on the right), reinforcing the core value

</specifics>

<deferred>
## Deferred Ideas

- Note chunks falling out tied to real generated MIDI — Phase 5 (generation) / Phase 4 (analysis completion events)
- Bottom list populated with real MIDI sets — Phase 5
- Audition + drag-out of list rows — Phase 6
</deferred>

---

*Phase: 02-audio-import-waveform*
*Context gathered: 2026-07-12 from user design direction*
