#include "MidiRowBuilder.h"

#include "AsIsRowGenerator.h"
#include "BassLineGenerator.h"
#include "StyleVoicingGenerators.h"

// The one public MidiGen entry point (GEN-01): fans one AnalysisResult out
// into the 5 fixed rows, in fixed order, in one synchronous call. Pure
// function -- same input always produces byte-identical output (GEN-04
// determinism contract, 05-RESEARCH.md Pitfall 2), proven by
// MidiRowBuilderTests.SameInputProducesByteIdenticalRows.
std::vector<MidiSetRow> generateAllRows (const AnalysisResult& result, const GenerationSettings& settings)
{
    // Bass rhythm: BassRhythm::TrapSustain is the shipped v1 default (Claude's
    // discretion, documented in 05-04-PLAN.md) -- a sparse sustained root is
    // the most universally usable bass for piano-roll drag-out, and matches
    // Tests/SyntheticFixtures.h's existing "bass = root" convention.
    // RnbRootFifth/HouseFourOnFloor remain implemented/tested (05-03) for a
    // future per-row style/rhythm control (05-RESEARCH.md Open Question 1);
    // `settings` is threaded through unused (v1 no-op) so that future control
    // can call this same entry point without a shape change.
    return {
        { "as-is",       "Detected",           RowStyle::DetectedAsIs,   generateAsIsRow (result) },
        { "pop-trap",    "Pop / Trap",         RowStyle::PopHipHopTrap,  generatePopTrapRow (result, settings) },
        { "rnb-neosoul", "R&B / Neo-Soul",     RowStyle::RnbNeoSoul,     generateRnbNeoSoulRow (result, settings) },
        { "house",       "Electronic / House", RowStyle::ElectronicHouse, generateElectronicHouseRow (result, settings) },
        { "bass",        "Bass",               RowStyle::Bass,           generateBassRow (result, BassRhythm::TrapSustain) },
    };
}
