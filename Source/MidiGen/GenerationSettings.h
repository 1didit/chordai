#pragma once

// Empty forward-compatible v1 settings placeholder.
//
// GEN-04's requirement text says rows "regenerate when the user changes the
// analysis region or style settings," but v1 has no user-facing style-
// configuration control -- every genre/pattern slot is fixed data
// (GenreRegistry.h) and always generated. Region-change regeneration is
// fully solved via the existing analysisBroadcaster wiring (05-RESEARCH.md
// Pattern 4); this struct exists purely so a future settings-changed
// callback can call the SAME generateGenreRows(result, genre, settings)
// entry point (06.1-05) without a shape change once a real settings surface
// is designed. Documented forward-compat plumbing, not speculative UI (see
// 05-RESEARCH.md Open Question 1).
struct GenerationSettings
{
};
