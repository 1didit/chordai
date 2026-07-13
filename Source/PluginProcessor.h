#pragma once

#include <JuceHeader.h>

#include "Import/LoadedAudio.h"
#include "Analysis/AnalysisResult.h"
#include "MidiGen/MidiSetRow.h"

#include <array>

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

    // --- Audio import / region API (message-thread only; processBlock never
    // touches any of this — RT-safety discipline stays untouched) -----------

    // Kicks off a background decode on loaderPool. The result (or failure) is
    // published back to the message thread via loadBroadcaster.
    void loadAudioFile (const juce::File& file);

    // Atomic-load publication pattern: safe to call from the message thread
    // at any time, including while a background decode is in flight.
    std::shared_ptr<const LoadedAudio> getLoadedAudio() const;

    juce::Range<double> getSelectedRegion() const;

    // Clamps against the loaded file's length via RegionState; no-op if
    // nothing is loaded yet. Does NOT broadcast — the UI initiated the change.
    void setSelectedRegion (juce::Range<double> regionSeconds);

    // Empty when the last load succeeded (or nothing has been loaded yet).
    juce::String getLastLoadError() const;

    // Fires on load complete AND on load failure.
    juce::ChangeBroadcaster loadBroadcaster;

    // --- Background chord analysis API (message-thread only) ---------------

    // Cancels any in-flight analysis and starts a new one over the current
    // loadedAudio/selectedRegion. Called internally after a successful load
    // and after a real (non-no-op) region change -- never call this from the
    // Editor directly.
    void triggerAnalysis();

    // Atomic-load publication pattern, same idiom as getLoadedAudio(). nullptr
    // while nothing has completed yet (or was just cleared by a new load).
    std::shared_ptr<const AnalysisResult> getAnalysisResult() const;

    bool isAnalyzing() const;
    double getAnalysisProgress() const;

    // Fires on analysis start / progress / completion.
    juce::ChangeBroadcaster analysisBroadcaster;

    // Atomic-load publication pattern, same idiom as getAnalysisResult().
    // Published in the SAME analysisBroadcaster message as analysisResult
    // (GEN-01) -- rows and the chord timeline can never be observed out of
    // sync. nullptr while nothing has completed yet (or was just cleared by
    // a new load).
    std::shared_ptr<const std::vector<MidiSetRow>> getMidiSetRows() const { return std::atomic_load (&midiSetRows); }

    // --- Row audition API (PRV-01) ------------------------------------------
    // Message-thread only unless noted. The rendered preview buffer is handed
    // to processBlock via a lock-free double-buffer of plain atomics -- NOT
    // this file's std::atomic_load/atomic_store shared_ptr idiom used above,
    // which is unsafe to call from the audio thread (06-RESEARCH.md Pitfall 2:
    // those free functions are not guaranteed lock-free and are commonly
    // implemented with an internal mutex/spinlock on the control block).

    // Message thread only; renders row on the calling thread (cheap -- a few
    // seconds of audio, additive synthesis, sub-millisecond in practice) and
    // hands it to the audio thread. Replaces any currently-playing audition.
    void startAudition (const MidiSetRow& row);

    // Atomic store only -- safe to call from anywhere.
    void stopAudition();

    // Relaxed atomic read -- safe to call from anywhere.
    bool isAuditionPlaying() const;

    // Message thread only. Stale after auto-stop is by design -- callers
    // must AND this with isAuditionPlaying() to know "is THIS row playing".
    juce::String getAuditionRowId() const;

    // --- Genre engine API (GEN-09/GEN-10/GEN-11; message-thread only) ------

    // Full 5-row rebuild for the new genre over the CURRENT AnalysisResult
    // (no re-analysis), variation counters reset, persisted via GenreState,
    // published via the same atomic_store + analysisBroadcaster path as
    // triggerAnalysis's onDone. No-op if genreId is unknown (Pitfall F) or
    // already active.
    void setActiveGenre (const juce::String& genreId);

    // Validates exactlyFive.size() == 5, every id resolves via findGenre(),
    // and no duplicates -- else no-op. Persists via GenreState and
    // broadcasts; does NOT regenerate rows UNLESS the currently active genre
    // was evicted from the new five, in which case the newest-added genre
    // (index 4) becomes active (documented discretion call), which itself
    // triggers a row rebuild via setActiveGenre().
    void setMainGenres (const juce::StringArray& exactlyFive);

    // Bumps patternVariationCounters[patternIndex] BEFORE regenerating
    // (Pitfall B), splices ONE row into a COPY of the current row vector,
    // publishes via the same atomic_store + analysisBroadcaster path (
    // Pitfall D: no partial-update shortcut exists or is added). Safe no-op
    // (no crash, no broadcast) if patternIndex is out of [0,4], or if there
    // is no current analysis result/rows yet.
    void regenerateRow (int patternIndex);

    juce::String getActiveGenreId() const;    // message-thread only
    juce::StringArray getMainGenreIds() const; // message-thread only

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioFormatManager formatManager; // registerBasicFormats() in ctor

    // Declared AFTER apvts/formatManager so it is destroyed FIRST: JUCE's
    // ThreadPool destructor waits for/cancels in-flight jobs, and those jobs
    // read formatManager, so formatManager must still be alive when that
    // happens (member destruction runs in reverse declaration order).
    juce::ThreadPool loaderPool { 1 };

    // Separate size-1 pool for analysis jobs -- NOT shared with loaderPool.
    // A new-file-drop mid-re-analysis must not queue behind a still-cancelling
    // analysis job (and vice versa); keeping the pools separate also keeps
    // removeAllJobs's cancellation scope from ever touching an unrelated
    // decode job. Same destruction-order rationale as loaderPool above: grouped
    // together, declared AFTER apvts/formatManager so both pools are destroyed
    // (and any in-flight jobs waited for/cancelled) before formatManager. The
    // analysis job itself only reads its own captured snapshots (no live
    // formatManager reference), but the pools stay grouped/documented together.
    juce::ThreadPool analysisPool { 1 };

    // Access ONLY via std::atomic_load/std::atomic_store — NOT
    // std::atomic<std::shared_ptr<T>>, which is incomplete on Apple libc++.
    std::shared_ptr<const LoadedAudio> loadedAudio;

    // Same atomic_load/atomic_store discipline as loadedAudio above.
    std::shared_ptr<const AnalysisResult> analysisResult;

    // Same atomic_load/atomic_store discipline as loadedAudio/analysisResult
    // above (incomplete std::atomic<shared_ptr> on Apple libc++). Published
    // synchronously alongside analysisResult, in the same onDone callback,
    // BEFORE analysisBroadcaster.sendChangeMessage() -- that ordering is the
    // "same broadcast" guarantee (GEN-01).
    std::shared_ptr<const std::vector<MidiSetRow>> midiSetRows;

    // Bumped by every triggerAnalysis() call; progress/completion callbacks
    // compare their captured generation against this to detect supersession.
    std::atomic<uint64_t> analysisGeneration { 0 };

    // Row audition double-buffer handoff (PRV-01). Deliberately NOT a
    // shared_ptr + atomic_load/atomic_store (see the startAudition doc
    // comment above and 06-RESEARCH.md Pitfall 2) -- these are plain,
    // genuinely lock-free atomics safe to touch from processBlock.
    // auditionBuffers[]: message thread resizes/writes the INACTIVE slot
    // only; the audio thread only ever reads the ACTIVE slot's raw samples.
    juce::AudioBuffer<float> auditionBuffers[2];
    std::atomic<int>  auditionActiveIndex  { 0 };
    std::atomic<int>  auditionActiveLength { 0 }; // valid sample count in the active buffer
    std::atomic<int>  auditionReadPos      { 0 };
    std::atomic<bool> auditionPlaying      { false };

    // Message-thread only (same discipline as selectedRegion below) -- which
    // row id is "the one playing" lives HERE, not on any MidiRowView, since
    // MidiSetsPanel::setRows() destroys every MidiRowView on every
    // regeneration (06-RESEARCH.md Pitfall 3).
    juce::String auditionRowId;

    // Message-thread only (like selectedRegion below) -- not touched from any
    // analysis-thread code; callbacks marshal back via callAsync before
    // writing these.
    bool analyzingFlag = false;
    double analysisProgress = 0.0;

    juce::Range<double> selectedRegion; // message-thread only
    juce::String lastLoadError;

    // Message-thread only (same discipline as selectedRegion above). Bumped
    // by regenerateRow(int) BEFORE each row-rebuild call (Pitfall B); reset
    // to {} on every setActiveGenre (a fresh genre starts every slot back at
    // its baseline variant).
    std::array<uint32_t, 5> patternVariationCounters {};

    // Message-thread only. Restored from GenreState in the ctor and again in
    // setStateInformation (RegionState precedent) so an editor-reopen/DAW
    // project reload lands coherent even before Phase 7's own persistence
    // verification.
    juce::String activeGenreId;
    juce::StringArray mainGenreIds;

    // The background job's completion callback runs later, asynchronously, via
    // MessageManager::callAsync — possibly after this processor has already
    // been destroyed. It captures a std::weak_ptr of this token (never `this`
    // directly) and bails out if the token has expired, instead of touching a
    // dangling processor.
    std::shared_ptr<int> aliveToken = std::make_shared<int> (0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordAIAudioProcessor)
};
