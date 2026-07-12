# Feature Research

**Domain:** Chord-detection / chord-generation audio plugins (VST3/AU/Standalone) for producers without music theory
**Researched:** 2026-07-12
**Confidence:** MEDIUM-HIGH (table stakes and competitor feature sets verified across multiple official product pages and independent reviews; accuracy figures partly extrapolated from academic MIREX benchmarks; "no direct conveyor competitor" claim is an absence-of-evidence finding, not exhaustive)

## Competitors Surveyed

- **Scaler 2 / Scaler 3** (Plugin Boutique / Scaler Music) — market leader, MIDI+audio chord detection, huge scale/chord theory engine
- **Scaler Detector** (Scaler Music, Oct 2025) — standalone spinoff, audio/MIDI key+chord+BPM detection only, built on zplane TONART V3
- **Captain Chords / Captain Plugins Epic** (Mixed In Key) — chord progression composer with genre preset library
- **Chord Prism 2** (Mozaic Beats) — MIDI FX chord/pattern generator (not audio detection)
- **Prism Audio-to-MIDI** (Aurally Sound) — general audio-to-MIDI incl. chords, not chord-specific
- **HoRNet SongKey MK4** — dedicated key/chord/tempo finder, MIDI + audio
- **RipX DAW (Pro)** (Hit'n'Mix) — stem separation + note-level audio editing + audio-to-MIDI + chord/scale detection
- **Orb Producer Suite 3 / LANDR Composer** (Hexachords/LANDR) — AI chord/melody/bass/arp generator modules
- **Chordjam** (Audiomodern) — randomized chord/progression generator with strum/humanize/MPE
- **InstaComposer 3** (W.A. Production) — multi-track (chords/bass/melody/drums) MIDI generator with one-click song arrangement
- **Chord Genie** (Unison Audio, 2025) — AI chord progression generator, part of an ecosystem (MIDI Wizard, Bass Dragon)
- **Hooktheory / Hookpad / TheoryTab** — web-based chord progression reference/education tool (not a plugin, but shapes user mental model of roman-numeral/chord display)

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete or unreliable compared to Scaler/Captain Chords.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Audio file drag-and-drop import (WAV/MP3/AIFF) | Scaler 2, RipX, HoRNet SongKey all support dropping a file straight onto the plugin window | LOW-MEDIUM | JUCE handles file drag-in natively; format decoding via JUCE `AudioFormatManager` |
| Key detection | Every competitor in this space (Scaler, Scaler Detector, HoRNet SongKey, Auto-Key, Mixed In Key, TONIC) leads with key detection as the anchor feature | MEDIUM | zplane TONART is the de facto industry algorithm several competitors license; a home-grown chromagram+template approach is acceptable at MVP but expect it to be judged against these |
| Chord detection from audio | The category-defining feature; users install these tools specifically to avoid picking out chords by ear | HIGH | Realistic accuracy ceiling per MIREX academic benchmarks is ~70-83% on clean/isolated material; competitors are visibly weaker on dense mixed pop/EDM audio (see Pitfalls below) — set user expectations accordingly, don't oversell "perfect" detection |
| Chord name display (e.g., Cmaj7, Am, F/A) | Baseline literacy — every competitor shows chord names; users without theory still recognize letter names from DAW piano roll | LOW | Simple lookup: root + quality label from detected chord template |
| Tempo/BPM detection | HoRNet SongKey, Scaler Detector, Auto-Key 2, Mixed In Key Live all bundle BPM with key detection | MEDIUM | Needed to quantize detected chords to a bar grid, not just cosmetic — feeds directly into MIDI export timing |
| MIDI drag-and-drop export to DAW | Scaler 2, Chord Genie, Chordjam, XO, Virtual Pianist all support this; it is the default expectation for "grab this MIDI" workflows in 2025-2026 plugin design | MEDIUM | JUCE `DragAndDropContainer::performExternalDragDropOfFiles()` — write a temp `.mid`, drag it out; some DAW-specific edge cases reported on JUCE forum (works in most hosts, occasional issues after project reload) |
| Save-to-disk `.mid` export as fallback | Not every DAW/workflow supports drag-and-drop reliably; a manual export path is the safety net | LOW | Simple `MidiFile::writeTo()` |
| Built-in audition sound (piano/pad) to preview chords before committing | Scaler 2 ships 33 sounds, defaults to Felt Piano; Captain Chords and Chord Genie both include built-in sounds/piano roll preview — users expect to *hear* a chord row before dragging it in | MEDIUM | Needs a lightweight internal sampler (not a full synth engine) — one or two decent piano/pad samples is sufficient at MVP |
| Preset/progression library browsable by genre | Captain Chords ships 100+ genre-tagged progressions (Trap, Neo-Soul, Deep House, etc.); Chord Genie and Chordjam both organize by style/mood | MEDIUM | Directly maps to ChordAI's Pop/Hip-hop/Trap, R&B/Neo-soul, Electronic/House voicing styles — this is both table stakes (having named styles) and where the differentiator lives (see below) |
| Session/state persistence with DAW project | Captain Chords is explicitly criticized for **not** reliably saving plugin state with the host project, forcing users to redo work | MEDIUM | Must get plugin state save/restore right (JUCE `getStateInformation`/`setStateInformation`) — this is a known competitor failure point, treat as non-negotiable |
| Host tempo/transport sync | Standard JUCE/VST3/AU capability; users expect chord playback and grid alignment to follow DAW tempo/transport when auditioning in-plugin | MEDIUM | Less critical for pure file-drag-drop analysis (source audio has its own tempo), but auditioning generated MIDI rows should sync to host if the plugin is inserted on a track |
| Multi-DAW compatibility (Ableton, FL Studio, Logic) | Baseline expectation for any commercial plugin; drag-and-drop MIDI mechanics are known to behave differently per host | MEDIUM-HIGH | Must explicitly test drag-out behavior in each target DAW — this is a recurring pain point discussed on JUCE/KVR forums, not a "build once, works everywhere" feature |

### Differentiators (Competitive Advantage)

Features that set the product apart. Not required, but this is where ChordAI competes — directly maps to the "conveyor" vision in PROJECT.md.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Conveyor batch output: detected-as-is row + N style-voicing variant rows + bass row, all generated simultaneously and each independently draggable | No surveyed competitor does this. Scaler 2's "Suggest mode" is a sequential, manual, one-chord-at-a-time theory assistant — user must still build a progression by hand. Chord Genie/Captain Chords generate one progression at a time from scratch (not from *your* reference audio). ChordAI's "song in → multiple finished MIDI sets out" in one pass is the core value prop and has no direct precedent found | HIGH | This is the product's reason to exist; everything else in this table is secondary. Architecturally this means the detection pipeline must feed multiple parallel transform stages (identity, N voicing engines, bass generator) that all consume the same detected-progression object |
| Waveform display with selectable analysis region | Every audio-detection competitor surveyed (Scaler 2, Scaler Detector, HoRNet SongKey, RipX) uses "drop a whole file, get a result" — none show an interactive waveform with drag-to-select a region for analysis. Users on Scaler's own forum complain detection is "hit or miss" partly because they can't isolate the relevant section (e.g., skip an intro with no harmonic content) | MEDIUM | JUCE `AudioThumbnail` + a selection overlay component; directly addresses a named competitor weak point rather than a hypothetical one |
| Genre-specific voicing engine applied to *your* detected progression (not a static preset library) | Captain Chords' genre packs are pre-written progressions you browse and drop in — they are not a transformation applied to a progression extracted from the user's own reference track. ChordAI restyles the *actual* detected chords (same roots/functions, different voicings/extensions/rhythm) into Pop/Trap, R&B/Neo-soul, Electronic/House — closer to a "style transfer" than a preset browser | MEDIUM-HIGH | Requires a voicing-rules engine per style (inversion choice, extension addition for 7th/9th/11th, rhythmic patterning for stabs) operating on the detected chord sequence, not a lookup table |
| Auto-generated bass line as part of the same output, matched to detected roots | Orb Producer's Bass module is a separate plugin instance the user must load and route independently; it is not bundled with the detection step of a reference track | MEDIUM | Root-note-following bass generation with simple rhythmic patterning per style is sufficient at MVP; full "intelligent bassist" harmony analysis (Orb's pitch) is a stretch goal, not MVP |
| Zero-theory workflow: no key/scale selection, no roman-numeral input required before getting output | Scaler 2 is widely criticized (KVR forum, MusicTech, SoS reviews) as "the most confusing plugin," overwhelming for exactly ChordAI's target user (beatmakers without theory background); HoRNet SongKey and Scaler Detector are detection-only utilities that still hand the user a raw chord list to interpret manually | LOW-MEDIUM | Mostly a UX/scope discipline decision (see Anti-Features) rather than new engineering — resist the temptation to expose Scaler-style scale/theory controls |
| Fully offline/local analysis, no account or internet requirement | Captain Chords users explicitly complain "AVOID — must be connected to internet"; this is a direct, named competitor pain point in studio/no-connectivity settings | LOW | Already a PROJECT.md constraint; worth stating explicitly as user-facing value, not just an internal architecture decision |
| ML-ready/upgradeable detection backend (swap classic DSP for ONNX model later without UX change) | Not user-visible at launch, but positions ChordAI to close the accuracy gap with zplane-TONART-powered competitors (Scaler Detector, HoRNet SongKey MK4, Mixed In Key) over time without a rewrite | MEDIUM (architecture only) | Already decided in PROJECT.md (`ChordAnalyzer` interface with swappable backend) — listed here to connect it to the competitive-accuracy narrative |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but would blow up scope or contradict the "conveyor, not workbench" positioning.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|------------------|-------------|
| Full stem separation before chord detection (RipX-style) | Seems like it should improve detection accuracy on busy mixes | Heavy ML dependency (large models, real GPU/CPU cost, slower analysis — contradicts "seconds not minutes" performance constraint); RipX itself is sold as a heavyweight, separate "AI DAW," not a lightweight chord-conveyor tool | Chromagram/detection runs on the full mix, matching Scaler 2's own approach; revisit stem-assisted detection only if accuracy on real user tracks proves insufficient post-launch (already flagged Out of Scope in PROJECT.md) |
| Deep music-theory workbench (scale explorer, modulation pathfinder, key-switch performance mapping, parallel-harmony generators) | Scaler 2 built exactly this and it's the market leader | Directly causes Scaler 2's #1 complaint: "most confusing plugin I've ever used," "severe headaches," GUI described as "a mess" — wrong tradeoff for a target user who explicitly lacks theory background | Keep the surface area to: detected progression → style variants → drag out. Theory power-features are a different product for a different (theory-literate) user |
| Mandatory internet/account/cloud dependency for core detection or generation | Easy to justify if using a cloud ML model; SaaS teams default to this | Named, specific competitor complaint (Captain Chords "must be connected to internet" — users flag it as a reason to avoid the product); also contradicts PROJECT.md's offline/local requirement for studio use | Local DSP/ONNX inference on-device; if a cloud ML tier is ever added, it must be optional/additive, never load-bearing |
| Real-time DAW audio capture ("Listen" button) in v1 | Natural-feeling feature — "just let it listen to what's playing" | Real-time capture adds audio routing complexity, latency handling, and a different analysis mode (streaming vs. file) that competes for v1 engineering time against the core conveyor pipeline | Already correctly deferred to v2 per PROJECT.md; file drag-and-drop is the proven, lower-risk v1 input method used by Scaler 2's own audio-detect feature |
| Per-note/per-parameter humanization mixing console (StepStrum/Chordjam-style: separate spread, timing, swing, gate, velocity sliders per step) | Power users on forums ask for granular control | Adds a second UI paradigm (sequencer/knob-farm) on top of the conveyor rows, diluting the "grab and drag" simplicity that is the whole pitch; risks becoming "Scaler 2 confusing" again | Bake humanization (light timing/velocity variation, voicing-appropriate strum) into each style preset as a fixed characteristic of that style, not an exposed control surface, for v1 |
| One-click full song-structure generation (Intro/Verse/Chorus/Bridge arrangement, InstaComposer-style) | Looks impressive in marketing, "finish a whole song" | Out of scope for a chord/bass conveyor tool; conflates arrangement (song-form) with harmony (chord content) — a much bigger, different problem | Stay scoped to the chord+bass MIDI conveyor; arrangement tools are a plausible separate future product, not a v1/v2 feature |
| MPE / advanced live-performance chord triggering (Chordjam-style pad performance, polyphonic glide) | Appeals to keyboard performers | Target user (beatmaker dragging finished MIDI into a piano roll) doesn't perform live chords; adds MIDI routing complexity for a use case outside the core persona | None needed — MVP output is static MIDI clips, not a performance instrument |
| Melody/vocal transcription | Natural "while we're detecting audio, why not melody too" request | Explicitly a different, harder problem (monophonic pitch tracking vs. polyphonic chord estimation); already flagged Out of Scope in PROJECT.md | Stay chord/bass-only; melody generation (not transcription) could be a v2+ module following Orb Producer's separate-module pattern, if ever pursued |

## Feature Dependencies

```
Audio file import (drag-drop)
    └──requires──> Waveform display + region selection (enhances, not blocking)
    └──requires──> Key/tempo detection
                       └──requires──> Chord detection (chromagram + Viterbi, bar-grid quantized)
                                          ├──feeds──> Conveyor row: detected-as-is MIDI
                                          ├──feeds──> Conveyor row(s): style-voicing variants (Pop/Trap, R&B/Neo-soul, Electronic/House)
                                          └──feeds──> Conveyor row: auto-generated bass line

Conveyor rows (all)
    └──requires──> Built-in audition sound (preview before drag)
    └──requires──> MIDI drag-and-drop export to DAW
                       └──fallback──> Save-to-disk .mid export

Session/state persistence ──enhances──> all of the above (re-open project, rows still there)
Host tempo sync ──enhances──> in-plugin audition playback (not required for file-based detection)
ML-ready analyzer backend ──enhances──> Chord detection (v2 accuracy upgrade path, no UX change)
```

### Dependency Notes

- **Chord detection requires key/tempo detection:** key context sharpens chord-template matching (e.g., disambiguating relative major/minor), and tempo is required to quantize detected chord changes to a bar grid before they can become clean MIDI regions.
- **All three conveyor row types (as-is, style variants, bass) require chord detection as their single upstream input:** this is the architectural crux of the "conveyor" differentiator — one detection pass must fan out into multiple independent generation stages, not three separate re-analyses.
- **Waveform region selection enhances rather than blocks detection:** v1 could ship with whole-file analysis only (matching Scaler 2's baseline) and add region selection as a fast-follow; but it's cheap relative to detection itself and directly answers a known competitor complaint, so bundling it into v1 is recommended.
- **Built-in audition sound depends on conveyor rows existing:** no point building a sampler before there's MIDI to preview; but users will judge "does this sound decent" within seconds of first use (Scaler 2's Felt Piano default sets the bar), so it can't be an afterthought.
- **Session persistence enhances everything:** it doesn't block any single feature, but its *absence* was the single most specific negative-review pattern found for Captain Chords, so it should not be treated as a nice-to-have.
- **Anti-feature stem separation would conflict with the "seconds not minutes" performance constraint:** flagged as a dependency conflict, not just a feature choice — including it would force async job queuing/progress UI that the current one-shot analysis pipeline doesn't need.

## MVP Definition

### Launch With (v1)

Minimum viable product — matches PROJECT.md's "Active" requirements, validated against competitor table stakes above.

- [ ] Audio file drag-and-drop import (WAV/MP3/AIFF/FLAC) — table stakes, no product without it
- [ ] Waveform display + region selection (or whole-file fallback) — cheap, directly beats a named competitor gap
- [ ] Key + tempo autodetection — required input for chord detection and bar-grid quantization
- [ ] Chord detection (chromagram + Viterbi, bar-grid aligned) — the category-defining feature
- [ ] Conveyor output: detected-as-is row + voicing-style variant rows (Pop/Trap, R&B/Neo-soul, Electronic/House) + bass row, generated together — the core differentiator, must ship in v1 or the product is "just another Scaler"
- [ ] Built-in audition sound (one solid piano/pad, per-row preview) — users need to hear before they drag
- [ ] MIDI drag-and-drop export from any conveyor row to DAW piano roll — the "out the other side" mechanic
- [ ] `.mid` file save-to-disk export — safety-net fallback where drag-and-drop misbehaves in a given host
- [ ] Plugin state save/restore with DAW project — must not repeat Captain Chords' worst-reviewed flaw
- [ ] VST3/AU/Standalone builds on macOS, verified in Ableton Live, FL Studio, Logic Pro

### Add After Validation (v1.x)

Features to add once the core conveyor is proven to work for real users.

- [ ] Chord name + roman numeral display toggle — nice literacy bridge once users trust the detection, not blocking initial value
- [ ] Expanded voicing-style library / more genre presets — trigger: users ask for styles beyond the initial three
- [ ] Preset/progression favoriting and tagging (Ripchord-style) — trigger: users start accumulating detected sessions they want to revisit
- [ ] Windows build — trigger: macOS core validated, commercial release requires cross-platform reach
- [ ] Deeper host-sync refinement / more DAWs verified (Studio One, Cubase, Reaper) — trigger: user reports of drag-out failures in specific hosts
- [ ] Licensing/copy-protection + signed installer — required before commercial release, deliberately after working MVP per PROJECT.md

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] Real-time "Listen" capture from DAW track — defer: adds routing/latency complexity that competes with core pipeline polish pre-launch
- [ ] ML/ONNX detection backend swap-in — defer: architecture is ready for it, but classic DSP should prove the UX first; upgrade accuracy once the conveyor concept is validated
- [ ] Melody/arp generation module (Orb Producer-style separate module) — defer: different problem from chords/bass, only pursue if conveyor traction justifies expanding scope
- [ ] Stem-aware/assisted detection for very dense mixes — defer: only if post-launch accuracy data shows full-mix chromagram is insufficient for target genres

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Chord detection from audio (chromagram+Viterbi) | HIGH | HIGH | P1 |
| Conveyor batch output (as-is + style variants + bass) | HIGH | HIGH | P1 |
| MIDI drag-and-drop export to DAW | HIGH | MEDIUM | P1 |
| Built-in audition sound | HIGH | MEDIUM | P1 |
| Key/tempo autodetection | HIGH | MEDIUM | P1 |
| Plugin state save/restore | HIGH | MEDIUM | P1 |
| Waveform display + region selection | MEDIUM | MEDIUM | P1 |
| .mid file save-to-disk fallback | MEDIUM | LOW | P1 |
| Chord name / roman numeral display | MEDIUM | LOW | P2 |
| More voicing styles / genre packs | MEDIUM | MEDIUM | P2 |
| Preset favoriting/tagging | LOW-MEDIUM | LOW-MEDIUM | P2 |
| Windows build | HIGH (commercial reach) | HIGH | P2 |
| Real-time "Listen" capture | MEDIUM | HIGH | P3 |
| ML/ONNX detection backend | MEDIUM (accuracy upside) | HIGH | P3 |
| Melody/arp generation module | LOW-MEDIUM | HIGH | P3 |
| Full theory workbench (scale explorer etc.) | LOW (wrong persona) | HIGH | Won't build |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | Scaler 2/3 | Captain Chords | HoRNet SongKey / RipX | ChordAI Approach |
|---------|-----------|-----------------|------------------------|-------------------|
| Audio-to-chord detection | Yes, but audio detection is weaker than MIDI detection; forum reports of "hit or miss," follows melody instead of basic chords on complex material | No (MIDI/manual composition only) | Yes (SongKey: decent on simple chords, confused by 5-6 note voicings and EDM; RipX: strong via full stem separation, but heavyweight) | Yes, chromagram+Viterbi baseline (~75-80% target on popular music), ML-swappable backend for future accuracy gains |
| Waveform + region selection for analysis | No — whole-file drag-drop only | N/A | No — whole-file/track analysis | Yes — waveform display with selectable region, addresses a named gap |
| Batch/conveyor multi-variant output | No — Suggest mode is sequential/manual, one chord at a time | No — browse and drop one preset progression at a time | No — single detection result | Yes — detected + N style variants + bass, generated together, this is the core differentiator |
| Genre-specific voicing applied to *detected* progression | Partial — Expressions/performances add feel, not genre-targeted restyling of a detected progression | Partial — genre packs are pre-written, not applied to user's own detected audio | No | Yes — voicing engine transforms the actual detected progression per style |
| Auto bass line bundled with chord output | No (separate workflow) | No | No | Yes — bass row generated alongside chord rows from the same detection pass |
| Built-in audition sound | Yes — 33 sounds, Felt Piano default | Yes | Limited/none (detection-focused utilities) | Yes — minimum one solid piano/pad at MVP |
| MIDI drag-and-drop to DAW | Yes | Yes (Canvas drag-drop of imported MIDI; less clear on generated-output drag-out) | N/A (SongKey is detection-only) / Yes (RipX) | Yes, from every conveyor row independently |
| Offline/local, no account required | Yes | No — user complaints cite mandatory internet connection | Yes | Yes — explicit product requirement |
| UI complexity / learnability | Widely criticized as confusing/overwhelming ("most confusing plugin," "severe headaches" — KVR forum) | Generally accessible, drag-and-drop friendly | Simple, narrow-scope utilities | Deliberately narrow: detect → conveyor rows → drag out, no exposed theory controls |
| Session/state persistence | Not flagged as a major complaint | Explicitly criticized — state not reliably saved with project | N/A | Must-have at MVP, treated as non-negotiable given competitor failure |

## Sources

- [Plugin Boutique Scaler 2](https://www.pluginboutique.com/product/3-Studio-Tools/93-Music-Theory-Tools/6439-Scaler-2) — official product page
- [Scaler 2 Update Information / Changelog](https://help.pluginboutique.com/hc/en-us/articles/6232885883412-Scaler-2-Update-Information-Changelog)
- [Sound on Sound: Plugin Boutique Scaler 2 review](https://www.soundonsound.com/reviews/plugin-boutique-scaler-2)
- [Scaler 2: Workflows for Electronic Music Producers — Sound & Design](https://soundand.design/scaler-2-workflows-for-electronic-music-producers-3c8d12d73b71)
- [Scaler Detector — Scaler Music](https://scalermusic.com/products/scaler-detector/)
- [Scaler Detector: Focused Chord Analysis Tool — Audio Newsroom](https://audionewsroom.net/2025/10/scaler-detector-focused-chord-analysis-tool-at-just-9-or-free.html)
- [Bedroom Producers Blog: Scaler Detector release](https://bedroomproducersblog.com/2025/10/29/scaler-detector/)
- [zplane TONIC product page](https://products.zplane.de/products/tonic)
- [KVR Audio: "Scaler 2 is the most confusing plugin I have ever used"](https://www.kvraudio.com/forum/viewtopic.php?t=606139)
- [Scaler Music Forum: Scaler2 Audio Detection accuracy](https://forum.scalermusic.com/t/scaler2-audio-detection-any-way-to-improve-accuracy/17705)
- [Scaler Music Forum: "Audio Detection sucks?"](https://forum.scalermusic.com/t/audio-detection-sucks/8113)
- [Rekkerd: Scaler 2.5 Suggest mode update](https://rekkerd.org/scaler-2-5-update-suggest-mode-guitar-chord-charts-new-sounds-more/)
- [Mixed In Key: Captain Chords](https://mixedinkey.com/captain-plugins/captain-chords/)
- [Mixed In Key: How to make R&B/Hip Hop with Captain Plugins](https://mixedinkey.com/captain-plugins/wiki/how-to-make-rnb-hip-hop-with-captain-plugins/)
- [KVR Audio: "Captain Chords worth it?"](https://www.kvraudio.com/forum/viewtopic.php?t=579842)
- [KVR Audio: 2024 Captain Chords Epic 7.0 problems](https://www.kvraudio.com/forum/viewtopic.php?t=614975)
- [AudioCipher: Scaler 2 vs Captain Chords comparison](https://www.audiocipher.com/post/scaler-2-vs-captain-chords)
- [ChordPrism official site](https://www.chordprism.com/)
- [Aurally Sound: Prism Audio-to-MIDI](https://aurallysound.com/pages/prism-audio-to-midi)
- [HoRNet SongKey MK4 product page](https://www.hornetplugins.com/plugins/hornet-songkey-mk4/)
- [pluginreviewlab: 7 Best Key Detection Plugins (2026)](https://pluginreviewlab.com/best-key-detection-plugins/)
- [Hit'n'Mix RipX DAW official site](https://hitnmix.com/ripx-daw/)
- [RouteNote Create Blog: RipX DAW review](https://create.routenote.com/blog/rip-x-daw-stem-separator/)
- [LANDR: Producer Suite 3 / Orb Producer](https://www.landr.com/plugins/producer-suite-3)
- [Hexachords Orb Producer Suite — Gearspace](https://static.gearspace.com/gear/hexachords/orb-producer-suite)
- [Audiomodern: Chordjam](https://audiomodern.com/shop/plugins/chordjam/)
- [MusicRadar: Audiomodern Chordjam 1.5 review](https://www.musicradar.com/reviews/audiomodern-chordjam-15-review)
- [W.A. Production: InstaComposer 3](https://www.waproduction.com/plugins/view/instacomposer-3)
- [Unison Audio: Chord Genie](https://unison.audio/chord-genie)
- [Unison Audio: Scaler 3 vs Chord Genie](https://unison.audio/scaler-3-vs-chord-genie/)
- [Hooktheory: TheoryTab](https://www.hooktheory.com/theorytab)
- [Hooktheory: Trends tool](https://www.hooktheory.com/trends)
- [MIREX Audio Chord Detection Task 2008 wiki](https://www.music-ir.org/mirex/wiki/2008:Audio_Chord_Detection)
- [Vicar: Automatic Chord Estimation from Audio — Review of the State of the Art (PDF)](https://romisatriawahono.net/lecture/rm/survey/computer%20vision/Vicar%20-%20Automatic%20Chord%20Estimation%20from%20Audio%20-%202014.pdf)
- [JUCE Forum: Can one drag and drop MIDI from a JUCE plug-in to the DAW timeline?](https://forum.juce.com/t/can-one-drag-and-drop-midi-from-a-juce-plug-in-to-the-daw-timeline/27816)
- [JUCE Forum: Implementing drag & drop from JUCE to DAW](https://forum.juce.com/t/implementing-drag-drop-from-juce-to-daw/55905)
- [Ripchord preset management — MIDI Mighty guide](https://midimighty.com/blogs/resources/ripchord)
- Project context: `/Users/test/Documents/prjcts/chordai/.planning/PROJECT.md`

---
*Feature research for: chord-detection/generation audio plugins*
*Researched: 2026-07-12*
