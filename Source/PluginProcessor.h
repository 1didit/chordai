#pragma once

#include <JuceHeader.h>

#include "Import/LoadedAudio.h"
#include "Analysis/AnalysisResult.h"

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

    // Bumped by every triggerAnalysis() call; progress/completion callbacks
    // compare their captured generation against this to detect supersession.
    std::atomic<uint64_t> analysisGeneration { 0 };

    // Message-thread only (like selectedRegion below) -- not touched from any
    // analysis-thread code; callbacks marshal back via callAsync before
    // writing these.
    bool analyzingFlag = false;
    double analysisProgress = 0.0;

    juce::Range<double> selectedRegion; // message-thread only
    juce::String lastLoadError;

    // The background job's completion callback runs later, asynchronously, via
    // MessageManager::callAsync — possibly after this processor has already
    // been destroyed. It captures a std::weak_ptr of this token (never `this`
    // directly) and bails out if the token has expired, instead of touching a
    // dangling processor.
    std::shared_ptr<int> aliveToken = std::make_shared<int> (0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordAIAudioProcessor)
};
