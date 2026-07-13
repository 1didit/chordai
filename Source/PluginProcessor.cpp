#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "Analysis/AnalysisPipeline.h"
#include "Audio/AuditionRenderer.h"
#include "Import/AudioFileLoader.h"
#include "Import/RegionState.h"
#include "MidiGen/GenreRegistry.h"
#include "MidiGen/GenreState.h"
#include "MidiGen/MidiRowBuilder.h"

ChordAIAudioProcessor::ChordAIAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
      // Stereo in/out (rather than a bare BusesProperties()) is declared explicitly:
      // processBlock below clears channels beyond getTotalNumOutputChannels(), which
      // requires buses to actually exist, and an aufx (AU effect) channel-config check
      // expects at least one valid I/O configuration. Stereo in/out is the standard
      // JUCE effect default and keeps the plugin inert-but-valid.
    , apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    // Editor-reopen/state-restore defense (RegionState precedent): a fresh
    // ctor has an empty apvts.state, so these fall back to "trap"/the
    // default main-5 -- setStateInformation re-reads the same way once a DAW
    // project actually loads.
    activeGenreId = GenreState::readActiveGenre (apvts.state, "trap");
    mainGenreIds = GenreState::readMainGenres (apvts.state, kDefaultMainGenreIds);
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordAIAudioProcessor::createParameterLayout()
{
    // Deliberately empty in Phase 1 — real parameters arrive in later phases.
    // If Plan 02's auval/pluginval run complains about zero parameters, a placeholder
    // param gets added then (empirical check, not preemptively added here).
    return {};
}

void ChordAIAudioProcessor::prepareToPlay (double, int)
{
    // All allocation happens here, not in processBlock. Nothing to allocate yet in
    // Phase 1 (no DSP state exists) — this function is the designated allocation site
    // for every future phase to use, establishing the discipline now.

    // A sample-rate/block-size change invalidates any rendered audition buffer
    // (it was rendered at the OLD sample rate) -- stop rather than risk
    // playing it back at the wrong pitch/speed on the new rate.
    stopAudition();
}

void ChordAIAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    // v1 does no live audio processing beyond the PRV-01 audition mix below.
    // Zero heap allocation, zero locks, zero file I/O, zero juce::String
    // ops, zero shared_ptr/atomic_load/atomic_store -- this IS the
    // deliberate real-time DSP addition the old comment warned future
    // phases about; it uses its own lock-free plain-atomics double-buffer
    // handoff (06-RESEARCH.md Pattern 3 / Pitfall 2), not the shared_ptr
    // publication idiom used elsewhere in this file.
    for (int ch = getTotalNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auditionPlaying.load (std::memory_order_acquire))
    {
        const int idx = auditionActiveIndex.load (std::memory_order_acquire);
        const int len = auditionActiveLength.load (std::memory_order_relaxed);
        const int pos = auditionReadPos.load (std::memory_order_relaxed);
        auto& src = auditionBuffers[idx]; // audio thread NEVER writes/resizes this --
                                           // message thread only ever touches the OTHER index

        const int toCopy = juce::jmin (len - pos, buffer.getNumSamples());
        if (toCopy > 0)
        {
            const int srcChannels = src.getNumChannels();
            for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch)
                buffer.addFrom (ch, 0, src, juce::jmin (ch, srcChannels - 1), pos, toCopy);
        }

        const int newPos = pos + toCopy;
        auditionReadPos.store (newPos, std::memory_order_relaxed);
        if (newPos >= len)
            auditionPlaying.store (false, std::memory_order_release); // auto-stop at end
    }
}

juce::AudioProcessorEditor* ChordAIAudioProcessor::createEditor()
{
    return new ChordAIAudioProcessorEditor (*this);
}

void ChordAIAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void ChordAIAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Defensive: host may call this before the editor exists, or with foreign/corrupt data.
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // Re-read GenreState (same fallbacks as the ctor) so a DAW project
    // reload's genre selection is reflected in the message-thread cache --
    // Phase 7 verifies the full persistence story end-to-end, this plan just
    // doesn't strand it. UnknownActiveGenreIdFallsBackSafely (Pitfall F)
    // covers a corrupt/foreign "activeGenreId" property landing here.
    activeGenreId = GenreState::readActiveGenre (apvts.state, "trap");
    mainGenreIds = GenreState::readMainGenres (apvts.state, kDefaultMainGenreIds);
}

void ChordAIAudioProcessor::loadAudioFile (const juce::File& file)
{
    std::weak_ptr<int> weakAlive (aliveToken);

    AudioFileLoadJob::Callback callback = [this, weakAlive] (std::shared_ptr<const LoadedAudio> result, juce::String errorMessage)
    {
        // The processor may have been destroyed between the background decode
        // finishing and this callAsync-delivered callback running on the
        // message thread. Bail rather than touch a dangling `this`.
        if (weakAlive.expired())
            return;

        if (result != nullptr)
        {
            std::atomic_store (&loadedAudio, result);
            selectedRegion = { 0.0, result->lengthSeconds }; // whole-file default, IMP-03
            lastLoadError.clear();
            RegionState::write (apvts.state, result->sourceFile, selectedRegion);

            // A new song must never briefly show the old song's chords --
            // clear before broadcasting so the Editor never observes a stale
            // result alongside the new waveform.
            std::atomic_store (&analysisResult, std::shared_ptr<const AnalysisResult>());
            std::atomic_store (&midiSetRows, std::shared_ptr<const std::vector<MidiSetRow>>());
        }
        else
        {
            lastLoadError = errorMessage;
        }

        loadBroadcaster.sendChangeMessage();

        if (result != nullptr)
            triggerAnalysis();
    };

    loaderPool.addJob (new AudioFileLoadJob (file, formatManager, callback), true);
}

std::shared_ptr<const LoadedAudio> ChordAIAudioProcessor::getLoadedAudio() const
{
    return std::atomic_load (&loadedAudio);
}

juce::Range<double> ChordAIAudioProcessor::getSelectedRegion() const
{
    return selectedRegion;
}

void ChordAIAudioProcessor::setSelectedRegion (juce::Range<double> regionSeconds)
{
    auto audio = getLoadedAudio();
    if (audio == nullptr)
        return; // no-op if nothing loaded

    auto clamped = RegionState::clampRegion (regionSeconds.getStart(), regionSeconds.getEnd(), audio->lengthSeconds);
    if (clamped == selectedRegion)
        return; // no-op guard: RegionSelectorOverlay::setTotalLength refires the whole-file
                // default on every editor reopen -- don't re-trigger analysis for that.

    selectedRegion = clamped;
    RegionState::write (apvts.state, audio->sourceFile, selectedRegion);
    // Do NOT broadcast — the UI initiated this change.

    triggerAnalysis();
}

juce::String ChordAIAudioProcessor::getLastLoadError() const
{
    return lastLoadError;
}

// --- Background chord analysis API ------------------------------------------

void ChordAIAudioProcessor::triggerAnalysis()
{
    auto audio = getLoadedAudio();
    if (audio == nullptr)
        return;

    // Non-blocking cooperative-cancel signal for any in-flight analysis job.
    // The pool holds at most one analysis job by design -- removeAllJobs
    // avoids ever tracking a raw ThreadPoolJob* that could dangle/ABA if the
    // job already self-finished. Cancellation is cooperative, not instant
    // (see AnalysisPipeline.cpp's shouldExit() checks inside analyse()): the
    // generation guard below is what actually prevents a late-finishing
    // superseded job from corrupting published state.
    analysisPool.removeAllJobs (true, 0);

    const uint64_t generation = ++analysisGeneration;

    // Set busy state synchronously -- no visible gap between supersede and
    // the new job's first callback, even though the old job hasn't actually
    // stopped running yet on a size-1 pool.
    analyzingFlag = true;
    analysisProgress = 0.0;
    analysisBroadcaster.sendChangeMessage();

    std::weak_ptr<int> weakAlive (aliveToken);

    AnalysisPipeline::ProgressCallback onProgress =
        [this, weakAlive] (uint64_t gen, double fraction, const juce::String& stage)
    {
        if (weakAlive.expired() || gen != analysisGeneration.load())
            return; // stale -- discard

        juce::ignoreUnused (stage);
        analysisProgress = fraction;
        analysisBroadcaster.sendChangeMessage();
    };

    AnalysisPipeline::CompletionCallback onDone =
        [this, weakAlive] (uint64_t gen, std::shared_ptr<const AnalysisResult> result)
    {
        if (weakAlive.expired() || gen != analysisGeneration.load())
            return; // superseded -- keep the last good result on screen

        std::atomic_store (&analysisResult, result);

        // A fresh analysis starts every slot back at its baseline variant --
        // matches setActiveGenre's own counter reset.
        patternVariationCounters = {};

        // Pitfall F: NEVER dereference findGenre() unchecked. activeGenreId
        // may be stale/corrupt/foreign (setStateInformation restored it from
        // a DAW project written by a future version, or by hand-edited XML);
        // fall back to "trap" rather than crash or publish empty rows.
        const auto* genre = findGenre (activeGenreId);
        if (genre == nullptr)
            genre = findGenre ("trap");

        // Row generation is pure math over a few dozen chords, measured
        // sub-millisecond even on the ~150-segment real-track-scale fixture
        // (PatternEngineTests.GenerationPerformanceBudget) -- safe to call
        // synchronously right here, on the message thread, before the
        // broadcast (05-RESEARCH.md Pattern 6). This ordering -- BEFORE
        // sendChangeMessage() -- IS the "same broadcast" guarantee (GEN-01):
        // rows and the chord timeline can never be observed out of sync.
        auto rows = std::make_shared<const std::vector<MidiSetRow>> (
            (result != nullptr && genre != nullptr) ? generateGenreRows (*result, *genre) : std::vector<MidiSetRow>{});
        std::atomic_store (&midiSetRows, rows);

        analyzingFlag = false;
        analysisProgress = 1.0;
        analysisBroadcaster.sendChangeMessage();
    };

    analysisPool.addJob (new AnalysisPipeline (audio, selectedRegion, generation, onProgress, onDone), true);
}

std::shared_ptr<const AnalysisResult> ChordAIAudioProcessor::getAnalysisResult() const
{
    return std::atomic_load (&analysisResult);
}

bool ChordAIAudioProcessor::isAnalyzing() const
{
    return analyzingFlag;
}

double ChordAIAudioProcessor::getAnalysisProgress() const
{
    return analysisProgress;
}

// --- Row audition API (PRV-01) -----------------------------------------------

void ChordAIAudioProcessor::startAudition (const MidiSetRow& row)
{
    // Message thread only: getAnalysisResult()'s atomic_load idiom is safe
    // HERE (message-thread-to-message-thread), unlike inside processBlock.
    auto result = getAnalysisResult();
    const double bpm = (result != nullptr && result->bpm > 0.0) ? result->bpm : 120.0;

    const int inactive = 1 - auditionActiveIndex.load (std::memory_order_relaxed);
    auditionBuffers[inactive] = AuditionRenderer::render (row, bpm, getSampleRate());
        // getSampleRate() is safe on the message thread here: by the time a
        // user can click Play, prepareToPlay has already run at least once.

    auditionRowId = row.id;

    auditionReadPos.store (0, std::memory_order_relaxed);
    auditionActiveLength.store (auditionBuffers[inactive].getNumSamples(), std::memory_order_relaxed);
    auditionActiveIndex.store (inactive, std::memory_order_release);  // publish buffer swap...
    auditionPlaying.store (true, std::memory_order_release);          // ...then arm playback
}

void ChordAIAudioProcessor::stopAudition()
{
    auditionPlaying.store (false, std::memory_order_release);
}

bool ChordAIAudioProcessor::isAuditionPlaying() const
{
    return auditionPlaying.load (std::memory_order_relaxed);
}

juce::String ChordAIAudioProcessor::getAuditionRowId() const
{
    return auditionRowId;
}

// --- Genre engine API (GEN-09/GEN-10/GEN-11) ----------------------------

void ChordAIAudioProcessor::setActiveGenre (const juce::String& genreId)
{
    if (genreId == activeGenreId)
        return; // no-op: already active

    const auto* genre = findGenre (genreId);
    if (genre == nullptr)
        return; // no-op: unknown id (Pitfall F -- never dereference unchecked)

    activeGenreId = genreId;
    GenreState::write (apvts.state, activeGenreId, mainGenreIds);
    patternVariationCounters = {}; // fresh genre -- every slot starts back at its baseline variant

    auto result = getAnalysisResult();
    auto rows = std::make_shared<const std::vector<MidiSetRow>> (
        result != nullptr ? generateGenreRows (*result, *genre) : std::vector<MidiSetRow>{});
    std::atomic_store (&midiSetRows, rows);

    analysisBroadcaster.sendChangeMessage();
}

void ChordAIAudioProcessor::setMainGenres (const juce::StringArray& exactlyFive)
{
    if (exactlyFive.size() != 5)
        return; // no-op: wrong size

    juce::StringArray seen;
    for (const auto& id : exactlyFive)
    {
        if (findGenre (id) == nullptr || seen.contains (id))
            return; // no-op: unknown id or duplicate -- defensive validate
        seen.add (id);
    }

    mainGenreIds = exactlyFive;
    GenreState::write (apvts.state, activeGenreId, mainGenreIds);
    analysisBroadcaster.sendChangeMessage();

    // FIFO eviction: the currently active genre fell out of the new five --
    // the newest-added genre (index 4) becomes active (documented discretion
    // call). setActiveGenre() performs its own write/broadcast/row rebuild.
    if (! mainGenreIds.contains (activeGenreId))
        setActiveGenre (mainGenreIds[4]);
}

void ChordAIAudioProcessor::regenerateRow (int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= 5)
        return; // no-op: out-of-range index, safe (no crash)

    auto result = getAnalysisResult();
    if (result == nullptr)
        return; // no-op: nothing analyzed yet

    auto rows = std::atomic_load (&midiSetRows);
    if (rows == nullptr || rows->size() != 5)
        return; // no-op: rows not published yet (defensive -- shouldn't happen once result is non-null)

    const auto* genre = findGenre (activeGenreId);
    if (genre == nullptr)
        genre = findGenre ("trap"); // Pitfall F
    if (genre == nullptr)
        return; // unreachable in practice ("trap" is always registered), stay safe regardless

    ++patternVariationCounters[(size_t) patternIndex]; // Pitfall B: increment BEFORE the call

    // ::regenerateRow is the free MidiRowBuilder.h function -- qualified
    // with :: because this member function's own name (regenerateRow)
    // otherwise hides it from unqualified lookup.
    const auto newRow = ::regenerateRow (*result, *genre, patternIndex, patternVariationCounters[(size_t) patternIndex]);

    // Pitfall D: full vector copy + atomic_store + broadcast, same
    // publication path as every other row update -- no partial-view/
    // single-row-update shortcut exists or is added here.
    std::vector<MidiSetRow> updatedRows = *rows;
    updatedRows[(size_t) patternIndex] = newRow;

    std::atomic_store (&midiSetRows, std::make_shared<const std::vector<MidiSetRow>> (std::move (updatedRows)));
    analysisBroadcaster.sendChangeMessage();
}

juce::String ChordAIAudioProcessor::getActiveGenreId() const
{
    return activeGenreId;
}

juce::StringArray ChordAIAudioProcessor::getMainGenreIds() const
{
    return mainGenreIds;
}

// This creates new instances of the plugin — required by every JUCE plugin, build
// fails at link without it.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordAIAudioProcessor();
}
