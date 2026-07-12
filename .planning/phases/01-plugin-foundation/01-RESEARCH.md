# Phase 1: Plugin Foundation - Research

**Researched:** 2026-07-12
**Domain:** JUCE 8 / CMake plugin scaffolding (VST3 + AU + Standalone, macOS) — real-time-safety and state-persistence discipline from first commit
**Confidence:** HIGH (JUCE CMake API, pluginval, auval all verified against official docs/repos; JUCE exact patch version MEDIUM due to conflicting date metadata across sources)

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-------------------|
| PLT-01 | Project builds as VST3, AU, and Standalone app on macOS (JUCE 8, C++20, CMake) | CMake skeleton (`## Code Examples`), `juce_add_plugin()` parameter table (`## Standard Stack`), toolchain prerequisite check (`## Environment Check`), validation commands mapped per format (`## Validation Architecture`) |
</phase_requirements>

## Summary

Phase 1 is pure scaffolding: one `CMakeLists.txt` calling `juce_add_plugin()` with `FORMATS "VST3;AU;Standalone"`, a minimal `AudioProcessor`/`AudioProcessorEditor` pair, and an `AudioProcessorValueTreeState` wired for save/restore even though it will hold zero or near-zero real parameters in v1. Nothing here touches DSP — the only "logic" is the real-time-safety discipline in `processBlock` (empty/pass-through, zero allocation) and the state round-trip in `getStateInformation`/`setStateInformation`. This phase's specific risk is not code complexity, it's environment/toolchain and per-format validation: JUCE must be pinned to an exact commit/tag, the dev machine's build toolchain must be confirmed (checked directly on this machine — CMake is **not currently installed**, Xcode 26.6 full IDE is), and every plugin format needs its own automated-vs-manual verification path so success criteria 1-3 (Ableton/FL Studio/Logic/Standalone loading) aren't purely manual guesswork during planning.

Two independent, non-DAW-dependent tools exist and should be wired into the dev loop from day one: `auval` (Apple's own AU validator, ships with macOS/Xcode CLT) and `pluginval` (Tracktion's cross-format validator, GPLv3 tool but doesn't taint the product since it runs out-of-process against the built binary). Critically, current `pluginval` (macOS build) has a **real-time-safety check mode** (`RealtimeCheck` option, backed by an internal `rtcheck` library) that can automatically flag allocation/syscall violations inside `processBlock` — this directly automates part of success criterion 4, though it explicitly cannot detect mutex lock/unlock misuse, so a manual code-review grep for locks is still required.

**Primary recommendation:** Pin JUCE via git submodule at an exact tagged commit (not `master`/`develop`), scaffold with `juce_add_plugin(FORMATS "VST3;AU;Standalone" ...)` and an APVTS-backed processor/editor pair from the first commit, and treat `auval` + `pluginval --strictness-level 5 --validate-in-process` as the per-task verification loop — manual DAW loads (Ableton/FL/Logic) are a periodic spot-check, not the primary feedback loop, because they're slow and not scriptable.

## Environment Check (this machine, verified 2026-07-12)

| Tool | Status | Action needed |
|------|--------|----------------|
| Xcode | 26.6 (full IDE installed at `/Applications/Xcode.app`), CLT selected | None — sufficient for VST3/AU/Standalone builds (AUv3 would need Xcode CMake generator specifically, not needed in v1) |
| macOS / SDK | macOS 26.5 (build 25F71), SDK 26.5 | None |
| CMake | **NOT INSTALLED** (`cmake` not found on PATH) | `brew install cmake` — required before any build attempt; this is a Wave 0 blocker, not optional |
| Homebrew | 6.0.8 installed | None |
| git | 2.50.1 (Apple Git) | None — sufficient for submodule workflow |
| `auval` | present at `/usr/bin/auval` (macOS ≥15: symlink to `auvaltool`) | None — usable immediately once an AU is built |

**This is a hard planning input:** the first task of Phase 1 must install CMake (`brew install cmake`) before any `juce_add_plugin()` scaffolding can be configured/built. Do not assume CMake is present.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|----------------|
| JUCE | **8.0.13** (verified via official GitHub Releases, dated May 2026 — HIGH confidence, matches `.planning/research/STACK.md`). A newer `8.0.14` tag was found via WebSearch/WebFetch but with **conflicting/implausible date metadata** across sources (one fetch reported June 2024, which predates JUCE 8's existence) — treat `8.0.14`'s existence as MEDIUM confidence and **re-verify the exact latest 8.0.x tag directly against `github.com/juce-framework/JUCE/releases` at implementation time** before pinning. | Cross-platform plugin framework: `juce_add_plugin()` CMake target generates VST3+AU+Standalone from one build | Confirmed HIGH via STACK.md's independent research pass; do not re-litigate the JUCE-vs-alternative decision, it's locked |
| CMake | ≥ 3.22 (JUCE's documented hard minimum, confirmed via official `docs/CMake API.md`: "All project types require CMake 3.22 or higher") | Build system generating all 3 targets from one `CMakeLists.txt` | Official JUCE-supported path since Projucer was deprecated as the primary workflow |
| C++20 | — | Language standard (`CXX_STANDARD 20` on the target) | Already locked in PROJECT.md/STACK.md; JUCE 8 only requires C++17, so this is friction-free |

### Supporting (Phase 1 scope only)

| Module | Purpose | When to Use |
|--------|---------|-------------|
| `juce_audio_utils` | Pulls in `juce_audio_processors` + GUI helper glue commonly used by plugin templates | Link this instead of hand-picking every module individually for the shell — standard in every JUCE plugin CMake example |
| `juce_audio_processors` | `AudioProcessor`, `AudioProcessorValueTreeState`, plugin-format wrapper base classes | Core of the processor |
| `juce_gui_basics` / `juce_gui_extra` | `AudioProcessorEditor`, basic `Component` | Editor shell only — no waveform/timeline UI needed yet (that's Phase 2+) |
| `juce_dsp`, `juce_audio_formats` | NOT needed in Phase 1 | Deferred to Phase 2 (audio import/decode) and Phase 3 (chromagram/FFT) — do not add these dependencies yet, keep this phase's link surface minimal |

### Installation

```cmake
# CMakeLists.txt (top-level) — minimum viable version pin
cmake_minimum_required(VERSION 3.25)   # JUCE hard minimum is 3.22; 3.25+ avoids known target-property edge cases (STACK.md finding)
project(ChordAI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/JUCE)   # git submodule, pinned to an exact tag — NOT FetchContent for JUCE itself
```

```bash
# One-time setup (this machine specifically needs cmake installed first)
brew install cmake

git submodule add https://github.com/juce-framework/JUCE.git external/JUCE
git -C external/JUCE checkout 8.0.13   # exact tag — re-verify latest 8.0.x at implementation time
git submodule update --init --recursive
```

**Rationale for submodule over FetchContent for JUCE specifically:** confirmed by official JUCE CMake docs' own primary example path (`add_subdirectory`) and cross-checked against community consensus (CMake Discourse, JUCE forum "What is best practice for managing JUCE projects?") — submodule avoids re-fetching JUCE's large repo on every clean CI build and pins an exact commit deterministically. `FetchContent` is a documented, valid alternative (some templates use it) but is not the primary official pattern. This matches and confirms STACK.md's existing recommendation — not re-litigated here, just re-verified against 2026-current sources.

## Architecture Patterns

### Pattern 1: `juce_add_plugin()` full parameter set for this phase

**What:** One CMake call declares all three build targets (VST3 static-lib-backed wrapper targets + AU wrapper + Standalone app) from a single processor/editor source set.
**When to use:** Exactly once, at the top of the plugin's `CMakeLists.txt`.
**Example:**
```cmake
# Source: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md (official)
juce_add_plugin(ChordAI
    VERSION 0.1.0
    COMPANY_NAME "YourCompanyName"                # TODO: decide before Phase 1 — see Open Questions
    BUNDLE_ID "com.yourcompany.chordai"            # TODO: decide before Phase 1 — see Open Questions
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE                   # installs to system plugin folders after each build — enables immediate manual DAW testing
    PLUGIN_MANUFACTURER_CODE Chai                  # 4 chars, >=1 uppercase (AU requirement) — placeholder, must be globally distinct in practice
    PLUGIN_CODE Cha1                                # 4 chars, exactly 1 uppercase (AU/GarageBand requirement: first upper, rest lower)
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ChordAI")
```
**Notes verified from official docs:**
- `FORMATS` valid values: `Standalone Unity VST3 AU AUv3 AAX VST LV2`. AU/AUv3 only enabled on macOS; **AUv3 additionally requires the Xcode CMake generator** — not relevant here since v1 targets classic AU, not AUv3.
- `PLUGIN_MANUFACTURER_CODE` defaults to `Manu` if omitted (must contain ≥1 uppercase letter for AU compatibility).
- `PLUGIN_CODE` defaults to a **randomly-generated code per build** if omitted — this is a trap: never omit it, or the plugin's identity changes across builds, breaking DAW preset/plugin recognition and re-scans. Must be explicit and stable from commit 1.
- `BUNDLE_ID` form is `com.yourcompany.productname`; auto-generated if omitted, but should be set explicitly and never changed after any real testing begins (host plugin databases key on it).
- `COPY_PLUGIN_AFTER_BUILD TRUE` copies to the default system locations (`~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`) — this is what makes "just build and check Ableton/Logic" actually work without a manual copy step.

### Pattern 2: Minimal `AudioProcessor` with APVTS wired from commit 1 (even with ~0 real parameters)

**What:** Construct `AudioProcessorValueTreeState` in the processor's member-init list using a `createParameterLayout()` static helper, and implement `getStateInformation`/`setStateInformation` via `apvts.copyState()`/`apvts.replaceState()` + XML serialization — the officially-documented pattern (verified against `docs.juce.com` `AudioProcessorValueTreeState` reference and the JUCE "Saving and loading your plug-in state" tutorial).
**When to use:** From the very first `PluginProcessor.cpp`, not retrofitted once real parameters/analysis state exist (this is Pitfall 12 in `.planning/research/PITFALLS.md` — retrofitting is expensive; the pattern costs nothing to establish now).
**Example:**
```cpp
// PluginProcessor.h
class ChordAIAudioProcessor : public juce::AudioProcessor
{
public:
    ChordAIAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordAIAudioProcessor)
};
```
```cpp
// PluginProcessor.cpp — state save/restore (Source: docs.juce.com AudioProcessorValueTreeState + JUCE APVTS tutorial pattern)
ChordAIAudioProcessor::ChordAIAudioProcessor()
    : AudioProcessor (BusesProperties()) // no audio buses needed yet — v1 does no live audio I/O
    , apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordAIAudioProcessor::createParameterLayout()
{
    // Deliberately near-empty in Phase 1 — real parameters arrive in later phases.
    // Verify empirically with auval/pluginval whether a fully empty layout is accepted;
    // if either tool warns, add one placeholder param (see Open Questions).
    return {};
}

void ChordAIAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void ChordAIAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Defensive: host may call this before the editor exists, or with foreign/corrupt data (Pitfall 12).
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
```

### Pattern 3: Real-time-safe `processBlock` skeleton (zero allocation/locking/I-O)

**What:** `processBlock` in v1 does no live audio analysis — it must still exist (JUCE/hosts call it continuously) and must do provably nothing unsafe.
**When to use:** From commit 1 — this is success criterion 4, and Pitfall 6 in PITFALLS.md explicitly warns against retrofitting this later.
**Example:**
```cpp
void ChordAIAudioProcessor::prepareToPlay (double, int)
{
    // All allocation happens here, not in processBlock. Nothing to allocate yet in Phase 1
    // (no DSP state exists) — this function exists as the designated allocation site for
    // every future phase to use, establishing the discipline now.
}

void ChordAIAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;   // standard JUCE idiom, verified pattern
    juce::ignoreUnused (midiMessages);

    // v1 does no live audio processing. Pass audio through untouched (or silence if you prefer
    // an explicit "inert" contract) — zero heap allocation, zero locks, zero file I/O, zero
    // juce::String construction. This function must stay this simple until a later phase
    // deliberately adds real-time DSP with its own lock-free handoff design.
    for (int ch = getTotalNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}
```
**Verification tie-in:** `pluginval`'s real-time-check mode (macOS, `rtcheck`-backed) can catch allocation/syscall violations here automatically; it explicitly cannot catch mutex misuse, so a manual grep of `processBlock` (and anything it transitively calls) for `new`, `malloc`, container resizes, `std::mutex`, and file access remains part of the Definition of Done for this phase, not just an automated pass.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|--------------|-----|
| Plugin format wrapping (VST3/AU/Standalone binary shells, Info.plist generation, bundle structure) | Custom per-format build scripts / manual Info.plist editing | `juce_add_plugin()` CMake function | This is exactly what it exists for; hand-rolling bundle structure is how AU validation/scan crashes (Pitfall 11) get introduced |
| Plugin identity/versioning across formats | Ad-hoc string constants scattered across files | `juce_add_plugin()`'s `VERSION`, `PLUGIN_CODE`, `PLUGIN_MANUFACTURER_CODE`, `BUNDLE_ID` args (single source of truth) | Prevents drift between what VST3 reports vs what AU reports vs what the DAW cached from a previous scan |
| Cross-thread parameter/state bridging | Custom mutex-protected struct between UI and audio thread | `AudioProcessorValueTreeState` | Already the established project pattern (ARCHITECTURE.md); building a custom bridge here reintroduces exactly the lock-in-audio-thread risk (Pitfall 6) the architecture is designed to avoid |
| Plugin validation / crash-on-scan detection | Manual "load in each DAW and see" as the only test loop | `auval` (AU) + `pluginval` (cross-format, incl. real-time-check mode) | Both are free, scriptable, and catch state-round-trip/bus-layout/RT-safety classes of bug that manual DAW use during active development typically doesn't exercise (Pitfall 11) |

**Key insight:** Everything in this phase has an official, JUCE-maintained or community-standard answer (CMake function, APVTS, pluginval/auval). The only genuinely custom work in Phase 1 is wiring these together correctly and pinning versions — there is no algorithmic content yet.

## Common Pitfalls

(Full detail already captured in `.planning/research/PITFALLS.md` Pitfalls 6, 11, 12, 13 — summarized here with Phase-1-specific emphasis; do not duplicate that research, reference it.)

### Pitfall: Omitting `PLUGIN_CODE` / letting it randomize per build
**What goes wrong:** If `PLUGIN_CODE` is left unset, JUCE generates a random 4-char code on every configure, so the plugin's identity changes between builds — DAWs treat it as a different plugin each time, breaking cached scans and any saved-project references.
**Why it happens:** It's an optional CMake argument with a "convenient" default that silently does the wrong thing for iterative development.
**How to avoid:** Set `PLUGIN_CODE` and `PLUGIN_MANUFACTURER_CODE` explicitly in the very first `juce_add_plugin()` call, before any DAW testing begins.
**Warning signs:** Plugin appears to "duplicate" or DAW re-scans/re-caches it on every build during dev.

### Pitfall: Treating "loads in a DAW that already scanned it" as proof of success
**What goes wrong:** Matches Pitfall 11 in PITFALLS.md exactly — a plugin can load fine in a session that already trusts a cached scan, then fail (or get silently blacklisted) on a cold scan/clean profile.
**How to avoid:** Run `auval` and `pluginval` after every change to state/bus/parameter code, and periodically do one clean-profile DAW load, not just incremental reloads.
**Warning signs:** Never having cleared the DAW's plugin cache during the entire phase.

### Pitfall: Zero-parameter APVTS layout — unverified edge case
**What goes wrong:** Unknown/unconfirmed — no direct source found confirming whether a completely empty `ParameterLayout` causes any auval/pluginval warning (e.g., "plugin exposes no parameters"). This is a LOW-confidence gap in research, not a confirmed pitfall.
**How to avoid:** Treat it as an empirical Wave 0 check: build the skeleton with zero parameters, run `auval`/`pluginval`, and only add a placeholder parameter if either tool actually complains.

## Code Examples

### Full CMake target declaration (Phase 1 scope)
```cmake
# Source: JUCE official CMake API docs (github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)
juce_add_plugin(ChordAI
    VERSION 0.1.0
    COMPANY_NAME "TODO"
    BUNDLE_ID "com.todo.chordai"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Chai
    PLUGIN_CODE Cha1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ChordAI")

target_sources(ChordAI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_link_libraries(ChordAI PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra)

target_compile_definitions(ChordAI PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

juce_generate_juce_header(ChordAI)
```

### Minimal `AudioProcessorEditor`
```cpp
// PluginEditor.h/.cpp — deliberately trivial, no waveform/timeline yet (Phase 2+)
class ChordAIAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChordAIAudioProcessorEditor (ChordAIAudioProcessor& p)
        : AudioProcessorEditor (&p), processor (p)
    {
        setSize (400, 300);
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::darkgrey); }
    void resized() override {}
private:
    ChordAIAudioProcessor& processor;
};
```

### Local validation commands (dev loop)
```bash
# AU — Apple's own validator, exact type/subtype/manufacturer per the plugin's declared codes
# Source: auval man page / auvaltool docs (macOS >=15: auval is a symlink to auvaltool)
auval -v aufx Cha1 Chai         # aufx = effect type; adjust if declared as aumu (instrument)
auval -v aufx Cha1 Chai -de     # stop at first error
auval -a                        # list all installed AUs (sanity check it's registered at all)

# pluginval — cross-format (VST3 + AU), incl. real-time-safety check mode on macOS
# Source: github.com/Tracktion/pluginval docs/"Adding pluginval to CI.md"
curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip" -o pluginval.zip
unzip pluginval.zip
./pluginval.app/Contents/MacOS/pluginval \
    --strictness-level 5 \
    --validate-in-process \
    --output-dir "./validation-logs" \
    "~/Library/Audio/Plug-Ins/VST3/ChordAI.vst3"

./pluginval.app/Contents/MacOS/pluginval \
    --strictness-level 5 \
    --validate-in-process \
    --output-dir "./validation-logs" \
    "~/Library/Audio/Plug-Ins/Components/ChordAI.component"
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|-------------------|----------------|--------|
| Projucer-generated Xcode/VS project files as the primary JUCE workflow | CMake (`juce_add_plugin()`) as the primary, community- and JUCE-team-supported workflow | Established well before JUCE 7/8; confirmed current as of 2026 research pass | Planner should never generate Projucer `.jucer` files for this project — CMake only |
| Manual DAW-only plugin testing | `auval` (AU-native) + `pluginval` (cross-format, now with an RT-safety check mode) as scriptable pre-DAW gates | `pluginval`'s real-time-check feature is a relatively recent addition (exact JUCE-forum thread undated but discusses it as current) | Changes the recommended dev loop: automate first, manual DAW spot-check second, not the reverse |

**Deprecated/outdated:** Projucer as primary project-generation tool (legacy, not used in this project per STACK.md decision already locked).

## Open Questions

1. **Exact JUCE 8.0.x patch to pin**
   - What we know: 8.0.13 is confirmed via official GitHub Releases (May 2026, HIGH confidence, matches STACK.md). A possible 8.0.14 was surfaced by a secondary search pass but with internally inconsistent date metadata (one source implied a 2024 date, which cannot be correct for a JUCE 8 patch).
   - What's unclear: Whether 8.0.14 (or later) is actually released and stable as of plan/implementation time.
   - Recommendation: At the start of Phase 1 execution, run `git ls-remote --tags https://github.com/juce-framework/JUCE.git` (or check the Releases page directly) and pin whatever the latest `8.0.x` tag is at that moment — treat 8.0.13 as the safe floor, not a hard ceiling.

2. **COMPANY_NAME / BUNDLE_ID / PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE values**
   - What we know: PROJECT.md has no registered company name yet ("Робоча назва: ChordAI, може змінитися до релізу"); these are required, non-optional `juce_add_plugin()` arguments with real consequences if changed later (DAW plugin-database identity, cached scans).
   - What's unclear: The actual legal/company name and final product name aren't locked.
   - Recommendation: Use clearly-marked placeholders now (e.g., `com.chordai-dev.chordai`, manufacturer code `Chai`) — cheap to rename in one CMake call before any real user/beta testing, expensive to rename after DAW users have cached/saved projects referencing the old identity. Planner should create a task to explicitly confirm/lock these values, not silently invent permanent-sounding ones.

3. **Does a fully empty `AudioProcessorValueTreeState::ParameterLayout` pass `auval`/`pluginval` cleanly?**
   - What we know: The documented construction pattern accepts any `ParameterLayout`, including one built from an empty initializer list; no direct source confirms or denies special handling for zero parameters.
   - What's unclear: Whether either validator emits a warning (not necessarily a failure) for a plugin exposing no automatable parameters.
   - Recommendation: Treat as an empirical first-task check, not a research gap requiring further web investigation — run both tools against the actual built skeleton and adjust only if a real warning appears.

4. **Standalone "launches without crashing" — how much can be automated?**
   - What we know: `auval`/`pluginval` validate VST3/AU by loading them as hosted plugins; neither tool validates a Standalone `.app` the same way (Standalone isn't hosted by a validator, it's launched directly by the OS).
   - What's unclear: No existing tool in this research pass automates "does the Standalone app render a window and stay alive," beyond a basic process-liveness smoke test.
   - Recommendation: Script a minimal smoke test — launch the built `.app`'s binary directly (e.g., `open -W --args` with a timeout, or launch + `sleep` + check the process is still running + check `~/Library/Logs/DiagnosticReports` for no new crash report) — see Validation Architecture below. Full visual "a window appeared and looks right" confirmation is a one-time manual check per build, not something to force into pure automation for this phase.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | No unit-test framework needed for Phase 1 itself (no algorithmic logic to unit test yet — Catch2 is already decided in STACK.md and will be set up starting Phase 3 when DSP code exists). Phase 1's "tests" are plugin-validator tools + build-success checks. |
| Config file | none yet — `CMakeLists.txt` itself is the only build config; Catch2/CTest wiring deferred |
| Quick run command | `cmake --build build --target ChordAI_VST3 ChordAI_AU ChordAI_Standalone` (build all three formats) |
| Full suite command | Build + `auval` + `pluginval --strictness-level 5 --validate-in-process` against both VST3 and AU artifacts, plus the Standalone smoke-launch script |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|---------------------|--------------|
| PLT-01 (build) | Project configures and builds all 3 targets with no errors | build | `cmake -B build -G Ninja && cmake --build build` | ❌ Wave 0 — CMakeLists.txt doesn't exist yet |
| PLT-01 (AU validity, ties to success criterion 2) | AU passes Apple's own validator | smoke/CLI | `auval -v aufx Cha1 Chai` (adjust codes/type once locked) | ❌ Wave 0 — needs built `.component` first |
| PLT-01 (cross-format validity, criterion 1+2) | VST3 and AU pass pluginval strict-mode incl. state round-trip + RT-safety check | smoke/CLI | `pluginval --strictness-level 5 --validate-in-process --output-dir ./validation-logs <path>` (run once per format) | ❌ Wave 0 — pluginval binary must be downloaded, not yet present in repo/tooling |
| PLT-01 (Standalone launch, criterion 3) | `.app` launches, stays alive, no crash report | smoke/script | custom smoke script: launch binary, wait N seconds, check process alive + no new `~/Library/Logs/DiagnosticReports/ChordAI*` entry | ❌ Wave 0 — script doesn't exist yet |
| PLT-01 (RT-safety, criterion 4) | `processBlock` performs zero allocation/locking/I-O | static + automated | manual grep of `processBlock`/callees for `new`/`malloc`/container-resize/`std::mutex`/file-I-O **plus** `pluginval` real-time-check mode (`RealtimeCheck` option — catches allocation/syscalls, NOT mutex misuse) | ❌ Wave 0 — no code exists yet; grep is a review-checklist item, not a script artifact |
| PLT-01 (state round-trip, criterion 5) | `getStateInformation`/`setStateInformation` round-trip an empty session | smoke/CLI | `pluginval`'s built-in state-restoration test (part of its standard strictness-5 suite — parameter randomize + restore is included by default, no separate flag needed) | ❌ Wave 0 — same as above, part of the pluginval run once the plugin exists |
| PLT-01 (manual DAW checks, criteria 1-3 as literally stated) | Ableton Live / FL Studio / Logic Pro / macOS standalone all load the plugin without crashing | manual-only | N/A — requires the actual DAW applications; justification: no CLI/headless mode exists for Ableton/FL Studio/Logic plugin loading | manual — no automation possible, budget explicit time for this in the phase's Definition of Done |

### Sampling Rate
- **Per task commit:** build all 3 targets (`cmake --build`) — fastest possible signal, catches compile/link breakage immediately.
- **Per wave merge:** `auval` + `pluginval --strictness-level 5 --validate-in-process` against VST3 and AU, plus the Standalone smoke-launch script.
- **Phase gate:** All of the above green, **plus** at least one manual load in each of Ableton Live, FL Studio, and Logic Pro (cold, not from an already-cached scan — per Pitfall 11), before `/gsd:verify-work`.

### Wave 0 Gaps
- [ ] `brew install cmake` — CMake is not installed on this machine; nothing else in this phase can proceed without it.
- [ ] `CMakeLists.txt` + `Source/PluginProcessor.{h,cpp}` + `Source/PluginEditor.{h,cpp}` — none exist yet (repo currently contains only `.planning/`).
- [ ] `external/JUCE` git submodule — needs to be added and pinned to an exact tag.
- [ ] `pluginval` binary — download prebuilt macOS release into a tooling/vendored location (not committed to source control as a binary blob; document the download step in a script instead, e.g. `tools/fetch-pluginval.sh`).
- [ ] Standalone smoke-launch script (e.g. `tools/smoke-test-standalone.sh`) — does not exist yet, needs to be authored as part of this phase's tasks.
- [ ] Decision/placeholder values for `COMPANY_NAME`, `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE` — must be chosen (even as clearly-marked placeholders) before the first `juce_add_plugin()` call, since omitting `PLUGIN_CODE` specifically causes a per-build-random identity bug.

## Sources

### Primary (HIGH confidence)
- https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md — `juce_add_plugin()` full parameter reference, CMake 3.22 minimum, FORMATS values, PLUGIN_CODE/MANUFACTURER_CODE defaults and AU naming rules, BUNDLE_ID format
- https://docs.juce.com/master/classAudioProcessorValueTreeState.html — APVTS constructor pattern, `copyState()`/`replaceState()` thread-safety semantics
- https://github.com/Tracktion/pluginval and `docs/Adding pluginval to CI.md` — exact CI invocation (`--validate-in-process --output-dir`), strictness-level default of 5, real-time-check (`RealtimeCheck`) option existing in `PluginTests.h`, explicit statement that mutex checks are disabled/unsupported
- auval/auvaltool man pages (unix.com mirror) + Moonbase/CommandMasters guides — `-v TYPE SUBT MANU` syntax, `-de`/`-dw`/`-a`/`-strict` flags, macOS ≥15 auval-as-symlink-to-auvaltool detail
- Direct tool checks on this machine (2026-07-12): `xcode-select -p`, `xcodebuild -version`, `sw_vers`, `cmake --version` (not found), `brew --version`, `git --version`, `xcrun --find auval` — HIGH confidence, first-party verification of the actual dev environment

### Secondary (MEDIUM confidence)
- WebSearch summaries of JUCE GitHub Releases page — JUCE 8.0.13 (May 2026) corroborated by STACK.md; JUCE 8.0.14 existence flagged but with inconsistent date metadata across two separate fetches — needs re-verification at implementation time, not treated as settled fact here
- JUCE forum "Pluginval: Real-time safety checking" thread (title/URL surfaced via search, content summarized) — real-time-check modes (disabled/enabled/relaxed) and the explicit mutex-check limitation

### Tertiary (LOW confidence)
- Whether an empty `ParameterLayout` triggers any auval/pluginval warning — no source found either way; flagged as an open, empirically-resolvable question rather than asserted in either direction

## Metadata

**Confidence breakdown:**
- Standard stack (CMake/juce_add_plugin/JUCE version): HIGH for CMake API and 8.0.13 pin; MEDIUM on whether a newer 8.0.x patch should be used instead — re-verify at implementation time
- Architecture (APVTS/processBlock skeleton): HIGH — directly sourced from official JUCE docs and already-verified project ARCHITECTURE.md/PITFALLS.md patterns, not new invention
- Validation tooling (auval/pluginval commands): HIGH for command syntax; MEDIUM for the newer real-time-check pluginval feature (undated forum source, but internally consistent with the tool's own header/source structure)
- Pitfalls: HIGH — sourced from both this pass's verification and the project's existing PITFALLS.md (Pitfalls 6, 11, 12, 13 directly apply to this phase)

**Research date:** 2026-07-12
**Valid until:** ~30 days for the CMake/APVTS/validator patterns (stable JUCE 8 API surface); re-check the exact JUCE patch tag and pluginval release version immediately before Phase 1 implementation regardless of this window, since both are fast-moving version numbers rather than API shape.

---
*Research for: Phase 1 - Plugin Foundation (ChordAI)*
*Researched: 2026-07-12*
