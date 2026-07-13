#include "GenreRegistry.h"
#include "GrooveEngine.h"

// Genre library data (06.1-RESEARCH.md Pattern 1 / per-genre spec table).
// Task 1: the 5 main genres (trap, uk-drill, boom-bap, rnb-neosoul, house) --
// selectable by default, given full research-table authenticity detail.
// Task 2 appends the remaining 5 starter genres.
//
// Every onset/span literal below is a rational fraction whose denominator
// divides 960 (TPQN) -- enforced by GenreRegistryTests.AllRhythmPoolOnsetsDivide960Exactly
// (Task 2). NEVER a decimal swing literal (Pitfall C) -- swing always goes
// through GrooveEngine's kSwingLight/kSwingTriplet constants.
//
// BassLine slot policy (PatternArchetype::bassRhythmPool doc comment): a
// non-empty bassRhythmPool means PatternEngine delegates to the existing
// generateBassRow (Phase 5); an EMPTY bassRhythmPool means the slot instead
// goes through the generic toneSet+rhythmPool+octaveOffsetPool path like any
// other slot (used where the fixed BassRhythm enum has no matching shape,
// e.g. boom bap's swung root+octave alternation).

namespace
{
    GenreSpec buildTrapSpec()
    {
        GenreSpec spec;
        spec.id = "trap"; spec.label = "Trap"; spec.shortLabel = "TRAP";

        // Slot 0: SustainedChords -- root-dropped triad, C3 anchor, staccato
        // ("leave the root for the 808"; "long sustains clash with the 808").
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::Triad;
        sustained.dropRoot = true;
        sustained.registerAnchor = kAnchorPopTrap; // 48
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 0.85 } };
        sustained.baseVelocity = 0.75f; sustained.jitterRange = 0.05f;

        // Slot 1: RhythmicChords -- half-bar re-strike, strong/weak accent.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::Triad;
        rhythmic.registerAnchor = kAnchorPopTrap;
        rhythmic.rhythmPool = {
            RhythmVariant { { 0.0, 2.0 }, 4.0, 0.9 },   // variant A -- straight half-bar
            RhythmVariant { { 0.0, 2.5 }, 4.0, 0.85 },  // variant B -- syncopated re-hit
        };
        rhythmic.accentPattern = { 1.0f, 0.7f };
        rhythmic.baseVelocity = 0.78f; rhythmic.jitterRange = 0.06f;

        // Slot 2: StabArp -- syncopated dotted-8th stabs, extensions only,
        // root-dropped (tick-exact, GrooveEngine Pattern 5).
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::SeventhExtension; stab.dropRoot = true;
        stab.registerAnchor = 60;
        stab.rhythmPool = {
            RhythmVariant { { 0.0, 0.75, 1.5, 2.5 }, 4.0, 0.3 },   // variant A
            RhythmVariant { { 0.0, 1.5, 2.25, 3.0 }, 4.0, 0.3 },   // variant B
        };
        stab.baseVelocity = 0.82f; stab.jitterRange = 0.1f;

        // Slot 3: BassLine -- delegates to BassLineGenerator.h's existing
        // TrapSustain shape (Pattern 10).
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.registerAnchor = kAnchorBass; // 36
        bass.bassRhythmPool = { BassRhythm::TrapSustain };

        // Slot 4: TopLineMotif -- sparse syncopated single-tone motif, dark.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = { RhythmVariant { { 0.0, 1.5 }, 4.0, 0.6 } };
        top.octaveOffsetPool = { 0, 12 };
        top.baseVelocity = 0.7f; top.jitterRange = 0.08f; top.ornamentProbability = 0.15f;

        return spec;
    }

    GenreSpec buildUkDrillSpec()
    {
        GenreSpec spec;
        spec.id = "uk-drill"; spec.label = "UK Drill"; spec.shortLabel = "DRILL";

        // Slot 0: SustainedChords -- SINGLE sustained top/extension tone, NOT
        // a full triad (research: "many UK drill beats do not have block
        // chords").
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::SingleTopTone;
        sustained.registerAnchor = 72;
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 0.9 } };
        sustained.baseVelocity = 0.7f; sustained.jitterRange = 0.05f;

        // Slot 1: RhythmicChords -- minimal dropped-root stab, tresillo-like
        // {0, 0.75, 1.25} (1.25 = 5/4, tick-exact), staccato.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::Triad; rhythmic.dropRoot = true;
        rhythmic.registerAnchor = kAnchorPopTrap;
        rhythmic.rhythmPool = {
            RhythmVariant { { 0.0, 0.75, 1.25 }, 2.0, 0.3 },  // variant A
            RhythmVariant { { 0.0, 0.5, 1.25 }, 2.0, 0.3 },   // variant B
        };
        rhythmic.baseVelocity = 0.78f; rhythmic.jitterRange = 0.08f;

        // Slot 2: StabArp -- fast triplet-grid run through chord tones (no
        // pitch-bend slide -- deferred per research Alternatives Considered).
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::SeventhExtension;
        stab.registerAnchor = 60;
        stab.rhythmPool = {
            RhythmVariant { { 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0, 4.0 / 3.0, 5.0 / 3.0 }, 2.0, 0.25 }, // variant A
            RhythmVariant { { 0.0, 1.0 / 3.0, 1.0, 4.0 / 3.0 }, 2.0, 0.25 },                        // variant B
        };
        stab.baseVelocity = 0.85f; stab.jitterRange = 0.1f;

        // Slot 3: BassLine -- root sustain (BassRhythm has no grace-note
        // glide shape; pitch bend is out of scope per research, so the
        // approximated glide pickup is NOT modelled here -- documented
        // choice, delegates to the existing TrapSustain shape instead of a
        // custom rhythmPool).
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.registerAnchor = kAnchorBass;
        bass.bassRhythmPool = { BassRhythm::TrapSustain };

        // Slot 4: TopLineMotif -- 1-2 bar repeating minor motif, call-response.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = {
            RhythmVariant { { 0.0, 0.75, 2.0, 2.75 }, 4.0, 0.6 },  // variant A
            RhythmVariant { { 0.0, 1.25, 2.75 }, 4.0, 0.6 },       // variant B
        };
        top.baseVelocity = 0.72f; top.jitterRange = 0.1f; top.ornamentProbability = 0.2f;

        return spec;
    }

    GenreSpec buildBoomBapSpec()
    {
        GenreSpec spec;
        spec.id = "boom-bap"; spec.label = "Boom Bap"; spec.shortLabel = "BOOM BAP";

        // Slot 0: SustainedChords -- simple 7ths (rnbExtensionIntervals
        // actually yields 9th/11th tones -- acceptable v1 approximation,
        // per plan's own explicit note), mid register.
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::SeventhExtension;
        sustained.registerAnchor = kAnchorRnbSeed; // 60
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 0.9 } };
        sustained.baseVelocity = 0.75f; sustained.jitterRange = 0.06f;

        // Slot 1: RhythmicChords -- swung 8th re-strike: {0, kSwingLight, 1,
        // 1+kSwingLight, ...} across a 4-beat span (each offset a tick-exact
        // n + 7/12).
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::Triad;
        rhythmic.registerAnchor = kAnchorRnbSeed;
        rhythmic.rhythmPool = {
            RhythmVariant { { 0.0, kSwingLight, 1.0, 1.0 + kSwingLight,
                               2.0, 2.0 + kSwingLight, 3.0, 3.0 + kSwingLight }, 4.0, 0.85 }, // variant A -- full density
            RhythmVariant { { 0.0, kSwingLight, 2.0, 2.0 + kSwingLight }, 4.0, 0.85 },        // variant B -- half density
        };
        rhythmic.accentPattern = { 1.0f, 0.7f };
        rhythmic.baseVelocity = 0.76f; rhythmic.jitterRange = 0.07f;

        // Slot 2: StabArp -- swung 16th ascending arp through chord tones;
        // per-beat grid {0, 7/24, 1/2, 1/2+7/24} at kSwingLight, tiled every
        // beat (spanBeats = 1.0).
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::SeventhExtension;
        stab.registerAnchor = 60;
        stab.rhythmPool = {
            RhythmVariant { { 0.0, 7.0 / 24.0, 0.5, 0.5 + 7.0 / 24.0 }, 1.0, 0.4 }, // variant A -- full swung 16ths
            RhythmVariant { { 0.0, 0.5 }, 1.0, 0.4 },                               // variant B -- sparse swung 8ths
        };
        stab.baseVelocity = 0.8f; stab.jitterRange = 0.09f;

        // Slot 3: BassLine -- syncopated root+octave alternation, swung;
        // BassRhythm has no matching enum shape, so this uses the generic
        // toneSet+rhythmPool+octaveOffsetPool path (bassRhythmPool empty).
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.toneSet = ToneSetKind::RootOnly;
        bass.registerAnchor = kAnchorBass;
        bass.octaveOffsetPool = { 0, 12 };
        bass.rhythmPool = { RhythmVariant { { 0.0, kSwingLight, 2.0, 2.0 + kSwingLight }, 4.0, 0.85 } };
        bass.baseVelocity = 0.8f; bass.jitterRange = 0.05f;

        // Slot 4: TopLineMotif -- call-response 7th/9th-tone phrase, swung.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = {
            RhythmVariant { { 0.5, 1.0, 2.5, 3.0 }, 4.0, 0.7 },              // variant A
            RhythmVariant { { 0.0, kSwingLight, 2.5, 3.0 }, 4.0, 0.7 },      // variant B
        };
        top.baseVelocity = 0.74f; top.jitterRange = 0.1f; top.ornamentProbability = 0.25f;

        return spec;
    }

    GenreSpec buildRnbNeoSoulSpec()
    {
        GenreSpec spec;
        spec.id = "rnb-neosoul"; spec.label = "R&B / Neo-Soul"; spec.shortLabel = "R&B";

        // Slot 0: SustainedChords -- extended (maj9/min11/dom9) + voice
        // leading, legato, soft velocity (Phase 5's kVelRnb precedent).
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::SeventhExtension;
        sustained.useVoiceLeading = true;
        sustained.registerAnchor = kAnchorRnbSeed;
        sustained.registerLow = kRnbRegisterLow; sustained.registerHigh = kRnbRegisterHigh;
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 1.0 } };
        sustained.baseVelocity = 0.62f; sustained.jitterRange = 0.08f;

        // Slot 1: RhythmicChords -- gentle syncopated re-strike, voice-led.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::SeventhExtension;
        rhythmic.useVoiceLeading = true;
        rhythmic.registerAnchor = kAnchorRnbSeed;
        rhythmic.rhythmPool = {
            RhythmVariant { { 0.0, 2.5 }, 4.0, 0.95 },        // variant A
            RhythmVariant { { 0.0, 1.75, 3.0 }, 4.0, 0.9 },   // variant B
        };
        rhythmic.baseVelocity = 0.6f; rhythmic.jitterRange = 0.08f;

        // Slot 2: StabArp -- soft broken-chord arpeggio through extension
        // tones, straight 8ths.
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::SeventhExtension;
        stab.useVoiceLeading = true;
        stab.registerAnchor = kAnchorRnbSeed;
        stab.rhythmPool = {
            RhythmVariant { { 0.0, 0.5, 1.0, 1.5 }, 2.0, 0.85 }, // variant A
            RhythmVariant { { 0.0, 0.5, 1.0 }, 2.0, 0.85 },      // variant B
        };
        stab.baseVelocity = 0.65f; stab.jitterRange = 0.07f;

        // Slot 3: BassLine -- delegates to BassLineGenerator.h's existing
        // RnbRootFifth shape.
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.registerAnchor = kAnchorBass;
        bass.bassRhythmPool = { BassRhythm::RnbRootFifth };

        // Slot 4: TopLineMotif -- legato top-note motif, voice-led.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.useVoiceLeading = true;
        top.registerAnchor = 72;
        top.rhythmPool = {
            RhythmVariant { { 0.0, 1.5, 3.0 }, 4.0, 0.95 }, // variant A
            RhythmVariant { { 0.0, 2.0 }, 4.0, 0.95 },      // variant B
        };
        top.baseVelocity = 0.6f; top.jitterRange = 0.08f; top.ornamentProbability = 0.3f;

        return spec;
    }

    GenreSpec buildHouseSpec()
    {
        GenreSpec spec;
        spec.id = "house"; spec.label = "Electronic / House"; spec.shortLabel = "HOUSE";

        // Slot 0: SustainedChords -- pad triad, soft velocity, wide register.
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::Triad;
        sustained.registerAnchor = kAnchorHouse; // 72
        sustained.registerLow = 60; sustained.registerHigh = 84;
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 1.0 } };
        sustained.baseVelocity = 0.6f; sustained.jitterRange = 0.05f;

        // Slot 1: RhythmicChords -- syncopated chord hits matching a
        // four-on-the-floor pulse.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::Triad;
        rhythmic.registerAnchor = kAnchorHouse;
        rhythmic.rhythmPool = {
            RhythmVariant { { 0.0, 1.5, 2.0, 3.5 }, 4.0, 0.5 }, // variant A -- syncopated
            RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.5 }, // variant B -- straight four-on-floor
        };
        rhythmic.baseVelocity = 0.8f; rhythmic.jitterRange = 0.08f;

        // Slot 2: StabArp -- existing kHouseStabOffsetsBeats off-beat stabs
        // (Phase 5 logic absorbed as data, research Pattern 10), C5 anchor,
        // 0.25-beat length (matches kHouseStabLengthBeats).
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::Triad;
        stab.registerAnchor = kAnchorHouse;
        stab.rhythmPool = {
            RhythmVariant { { kHouseStabOffsetsBeats[0], kHouseStabOffsetsBeats[1],
                               kHouseStabOffsetsBeats[2], kHouseStabOffsetsBeats[3] }, 4.0, 0.25 }, // variant A
            RhythmVariant { { 0.5, 1.0, 2.5, 3.0 }, 4.0, 0.25 },                                     // variant B
        };
        stab.baseVelocity = 0.88f; stab.jitterRange = 0.1f; // kVelHouse precedent

        // Slot 3: BassLine -- delegates to BassLineGenerator.h's existing
        // HouseFourOnFloor shape.
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.registerAnchor = kAnchorBass;
        bass.bassRhythmPool = { BassRhythm::HouseFourOnFloor };

        // Slot 4: TopLineMotif -- rising arp, straight 8ths, bright register
        // (clamp-checked -- intervalsToMidiNotes jlimits into 0-127).
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::Triad;
        top.registerAnchor = 84;
        top.rhythmPool = {
            RhythmVariant { { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5 }, 4.0, 0.6 }, // variant A -- full bar straight 8ths
            RhythmVariant { { 0.0, 0.5, 1.0, 1.5 }, 2.0, 0.6 },                      // variant B -- half-bar
        };
        top.baseVelocity = 0.75f; top.jitterRange = 0.08f;

        return spec;
    }

    // ---- Remaining 5 starter genres (Task 2) --------------------------
    // Solid starters per the research's starter-guidance row -- not given
    // the same full multi-variant treatment as the 5 main genres, but every
    // onset/span is still tick-exact against 960 (swept by
    // AllRhythmPoolOnsetsDivide960Exactly, written first as the guardrail).

    GenreSpec buildPopSpec()
    {
        GenreSpec spec;
        spec.id = "pop"; spec.label = "Pop"; spec.shortLabel = "POP";

        // Slot 0: SustainedChords -- full triad, warm register, straight grid.
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::Triad;
        sustained.registerAnchor = kAnchorAsIs; // 60
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 0.9 } };
        sustained.baseVelocity = 0.72f; sustained.jitterRange = 0.05f;

        // Slot 1: RhythmicChords -- straight-8th re-strike.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::Triad;
        rhythmic.registerAnchor = kAnchorAsIs;
        rhythmic.rhythmPool = {
            RhythmVariant { { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5 }, 4.0, 0.85 }, // variant A -- full straight 8ths
            RhythmVariant { { 0.0, 0.5, 1.0, 1.5 }, 2.0, 0.85 },                      // variant B -- half density
        };
        rhythmic.baseVelocity = 0.76f; rhythmic.jitterRange = 0.06f;

        // Slot 2: StabArp -- rising root-3rd-5th-octave arpeggio, straight 16ths.
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::Triad;
        stab.registerAnchor = kAnchorAsIs;
        stab.rhythmPool = {
            RhythmVariant { { 0.0, 0.25, 0.5, 0.75 }, 1.0, 0.4 }, // variant A -- full straight 16ths
            RhythmVariant { { 0.0, 0.25, 0.5 }, 1.0, 0.4 },       // variant B -- 3 of 4 sixteenths
        };
        stab.baseVelocity = 0.78f; stab.jitterRange = 0.07f;

        // Slot 3: BassLine -- root on beats 1 and 3; no matching BassRhythm
        // shape, uses the generic toneSet+rhythmPool path.
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.toneSet = ToneSetKind::RootOnly;
        bass.registerAnchor = kAnchorBass;
        bass.rhythmPool = { RhythmVariant { { 0.0, 2.0 }, 4.0, 0.9 } };
        bass.baseVelocity = 0.8f; bass.jitterRange = 0.05f;

        // Slot 4: TopLineMotif -- stepwise chord-tone melody, straight.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = {
            RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.7 },                       // variant A -- quarter-note steps
            RhythmVariant { { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5 }, 4.0, 0.5 },    // variant B -- eighth-note steps
        };
        top.baseVelocity = 0.7f; top.jitterRange = 0.08f; top.ornamentProbability = 0.2f;

        return spec;
    }

    GenreSpec buildLofiSpec()
    {
        GenreSpec spec;
        spec.id = "lofi"; spec.label = "Lofi"; spec.shortLabel = "LOFI";

        // Slot 0: SustainedChords -- warm extended chord, legato, soft.
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::SeventhExtension;
        sustained.registerAnchor = kAnchorRnbSeed;
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 1.0 } };
        sustained.baseVelocity = 0.55f; sustained.jitterRange = 0.06f;

        // Slot 1: RhythmicChords -- swung (kSwingLight) gentle re-strikes.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::SeventhExtension;
        rhythmic.registerAnchor = kAnchorRnbSeed;
        rhythmic.rhythmPool = { RhythmVariant { { 0.0, kSwingLight, 2.0, 2.0 + kSwingLight }, 4.0, 0.9 } };
        rhythmic.baseVelocity = 0.56f; rhythmic.jitterRange = 0.06f;

        // Slot 2: StabArp -- gentle swung broken-chord arpeggio.
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::SeventhExtension;
        stab.registerAnchor = kAnchorRnbSeed;
        stab.rhythmPool = { RhythmVariant { { 0.0, kSwingLight }, 1.0, 0.6 } };
        stab.baseVelocity = 0.58f; stab.jitterRange = 0.08f;

        // Slot 3: BassLine -- root sustain with an occasional sparse passing
        // tone near the end of the bar (ornament-free, per plan); no
        // matching BassRhythm shape, uses the generic path.
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.toneSet = ToneSetKind::RootOnly;
        bass.registerAnchor = kAnchorBass;
        bass.rhythmPool = {
            RhythmVariant { { 0.0 }, 4.0, 0.95 },      // variant A -- full sustain
            RhythmVariant { { 0.0, 3.0 }, 4.0, 0.4 },  // variant B -- sparse passing-tone feel
        };
        bass.baseVelocity = 0.6f; bass.jitterRange = 0.05f;

        // Slot 4: TopLineMotif -- slow sparse swung entrances.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = {
            RhythmVariant { { 0.0, 2.0 + kSwingLight }, 4.0, 0.5 },   // variant A
            RhythmVariant { { kSwingLight, 3.0 }, 4.0, 0.5 },         // variant B
        };
        top.baseVelocity = 0.55f; top.jitterRange = 0.1f; top.ornamentProbability = 0.3f;

        return spec;
    }

    GenreSpec buildAfrobeatsSpec()
    {
        GenreSpec spec;
        spec.id = "afrobeats"; spec.label = "Afrobeats"; spec.shortLabel = "AFRO";

        // Slot 0: SustainedChords -- 7th/9th-leaning chord.
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::SeventhExtension;
        sustained.registerAnchor = kAnchorRnbSeed;
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 0.85 } };
        sustained.baseVelocity = 0.75f; sustained.jitterRange = 0.08f;

        // Slot 1: RhythmicChords -- clave-ish re-strike, explicit onset list
        // (straight grid, NOT swing), quarter-beat multiples.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::Triad;
        rhythmic.registerAnchor = kAnchorRnbSeed;
        rhythmic.rhythmPool = { RhythmVariant { { 0.0, 0.75, 1.5, 2.5, 3.25 }, 4.0, 0.6 } };
        rhythmic.baseVelocity = 0.78f; rhythmic.jitterRange = 0.08f;

        // Slot 2: StabArp -- log-drum-style plucks, syncopated, staccato,
        // alternating strong/weak accent.
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::PowerChord;
        stab.registerAnchor = kAnchorPopTrap;
        stab.rhythmPool = { RhythmVariant { { 0.0, 0.75, 1.5, 2.5 }, 4.0, 0.28 } };
        stab.accentPattern = { 1.0f, 0.7f };
        stab.baseVelocity = 0.78f; stab.jitterRange = 0.08f;

        // Slot 3: BassLine -- follows the clave onsets rather than a
        // straight sustain; no matching BassRhythm shape, generic path.
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.toneSet = ToneSetKind::RootOnly;
        bass.registerAnchor = kAnchorBass;
        bass.rhythmPool = { RhythmVariant { { 0.0, 0.75, 2.5 }, 4.0, 0.8 } };
        bass.baseVelocity = 0.8f; bass.jitterRange = 0.05f;

        // Slot 4: TopLineMotif -- call-response, clave-aligned.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = { RhythmVariant { { 0.0, 1.5, 2.5, 3.25 }, 4.0, 0.6 } };
        top.baseVelocity = 0.72f; top.jitterRange = 0.09f; top.ornamentProbability = 0.2f;

        return spec;
    }

    GenreSpec buildReggaetonSpec()
    {
        GenreSpec spec;
        spec.id = "reggaeton"; spec.label = "Reggaeton"; spec.shortLabel = "DEMBOW";

        // Slot 0: SustainedChords -- minor-leaning triad pad.
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::Triad;
        sustained.registerAnchor = kAnchorAsIs;
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 0.9 } };
        sustained.baseVelocity = 0.7f; sustained.jitterRange = 0.05f;

        // Slot 1: RhythmicChords -- tresillo {0, 0.75, 1.5} per 2-beat span.
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::Triad;
        rhythmic.registerAnchor = kAnchorAsIs;
        rhythmic.rhythmPool = { RhythmVariant { { 0.0, 0.75, 1.5 }, 2.0, 0.5 } };
        rhythmic.baseVelocity = 0.8f; rhythmic.jitterRange = 0.08f;

        // Slot 2: StabArp -- same tresillo rhythm, arpeggiated root-fifth
        // (+octave via octaveOffsetPool).
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::PowerChord;
        stab.registerAnchor = kAnchorAsIs;
        stab.octaveOffsetPool = { 0, 12 };
        stab.rhythmPool = { RhythmVariant { { 0.0, 0.75, 1.5 }, 2.0, 0.35 } };
        stab.baseVelocity = 0.82f; stab.jitterRange = 0.1f;

        // Slot 3: BassLine -- dembow-following: root on downbeat + tresillo
        // "and"; no matching BassRhythm shape, generic path.
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.toneSet = ToneSetKind::RootOnly;
        bass.registerAnchor = kAnchorBass;
        bass.rhythmPool = { RhythmVariant { { 0.0, 0.75, 1.5 }, 2.0, 0.85 } };
        bass.baseVelocity = 0.82f; bass.jitterRange = 0.05f;

        // Slot 4: TopLineMotif -- simple top melody following tresillo accents.
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = { RhythmVariant { { 0.0, 0.75, 1.5 }, 2.0, 0.5 } };
        top.baseVelocity = 0.72f; top.jitterRange = 0.08f; top.ornamentProbability = 0.15f;

        return spec;
    }

    GenreSpec buildTechnoSpec()
    {
        GenreSpec spec;
        spec.id = "techno"; spec.label = "Techno"; spec.shortLabel = "TECHNO";

        // Slot 0: SustainedChords -- power-voicing long pad, very static
        // (minimal jitter by design).
        auto& sustained = spec.patterns[(size_t) PatternKind::SustainedChords];
        sustained.kind = PatternKind::SustainedChords;
        sustained.toneSet = ToneSetKind::PowerChord;
        sustained.registerAnchor = kAnchorPopTrap;
        sustained.rhythmPool = { RhythmVariant { { 0.0 }, 4.0, 1.0 } };
        sustained.baseVelocity = 0.65f; sustained.jitterRange = 0.03f;

        // Slot 1: RhythmicChords -- pulsing quarter-note re-strike (root+fifth).
        auto& rhythmic = spec.patterns[(size_t) PatternKind::RhythmicChords];
        rhythmic.kind = PatternKind::RhythmicChords;
        rhythmic.toneSet = ToneSetKind::PowerChord;
        rhythmic.registerAnchor = kAnchorPopTrap;
        rhythmic.rhythmPool = { RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.5 } };
        rhythmic.baseVelocity = 0.75f; rhythmic.jitterRange = 0.04f;

        // Slot 2: StabArp -- hypnotic repeating root-fifth 8th arpeggio.
        auto& stab = spec.patterns[(size_t) PatternKind::StabArp];
        stab.kind = PatternKind::StabArp;
        stab.toneSet = ToneSetKind::PowerChord;
        stab.registerAnchor = kAnchorRnbSeed;
        stab.rhythmPool = { RhythmVariant { { 0.0, 0.5, 1.0, 1.5 }, 2.0, 0.45 } };
        stab.baseVelocity = 0.78f; stab.jitterRange = 0.05f;

        // Slot 3: BassLine -- straight quarter-note pulse, single pitch; no
        // matching BassRhythm shape, generic path.
        auto& bass = spec.patterns[(size_t) PatternKind::BassLine];
        bass.kind = PatternKind::BassLine;
        bass.toneSet = ToneSetKind::RootOnly;
        bass.registerAnchor = kAnchorBass;
        bass.rhythmPool = { RhythmVariant { { 0.0, 1.0, 2.0, 3.0 }, 4.0, 0.6 } };
        bass.baseVelocity = 0.75f; bass.jitterRange = 0.03f;

        // Slot 4: TopLineMotif -- simple 2-note hypnotic phrase, unchanging
        // (ornamentProbability 0.0 -- static by design).
        auto& top = spec.patterns[(size_t) PatternKind::TopLineMotif];
        top.kind = PatternKind::TopLineMotif;
        top.toneSet = ToneSetKind::SingleTopTone;
        top.registerAnchor = 72;
        top.rhythmPool = { RhythmVariant { { 0.0, 1.75 }, 4.0, 0.5 } };
        top.baseVelocity = 0.7f; top.jitterRange = 0.02f; top.ornamentProbability = 0.0f;

        return spec;
    }
}

const std::vector<GenreSpec>& allGenres()
{
    static const std::vector<GenreSpec> genres {
        buildTrapSpec(),
        buildUkDrillSpec(),
        buildBoomBapSpec(),
        buildRnbNeoSoulSpec(),
        buildHouseSpec(),
        buildPopSpec(),
        buildLofiSpec(),
        buildAfrobeatsSpec(),
        buildReggaetonSpec(),
        buildTechnoSpec(),
    };
    return genres;
}

const GenreSpec* findGenre (const juce::String& id)
{
    for (const auto& genre : allGenres())
        if (genre.id == id)
            return &genre;

    return nullptr;
}
