#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "Analysis/AnalysisPipeline.h"
#include "Import/AudioFileLoader.h"
#include "Import/RegionState.h"
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
}

void ChordAIAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    // v1 does no live audio processing. Zero heap allocation, zero locks, zero file
    // I/O, zero juce::String construction. This function must stay this simple until
    // a later phase deliberately adds real-time DSP with its own lock-free handoff.
    for (int ch = getTotalNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
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

        // Row generation is pure math over a few dozen chords, measured
        // sub-millisecond even on the ~150-segment real-track-scale fixture
        // (MidiRowBuilderTests.GenerationPerformanceBudget) -- safe to call
        // synchronously right here, on the message thread, before the
        // broadcast (05-RESEARCH.md Pattern 6). This ordering -- BEFORE
        // sendChangeMessage() -- IS the "same broadcast" guarantee (GEN-01):
        // rows and the chord timeline can never be observed out of sync.
        auto rows = std::make_shared<const std::vector<MidiSetRow>> (
            result != nullptr ? generateAllRows (*result) : std::vector<MidiSetRow>{});
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
    juce::ignoreUnused (row);
    // RED stub -- GREEN phase (Task 2) implements the real double-buffer
    // render + publish sequence described in PluginProcessor.h.
}

void ChordAIAudioProcessor::stopAudition()
{
    // RED stub.
}

bool ChordAIAudioProcessor::isAuditionPlaying() const
{
    return false; // RED stub.
}

juce::String ChordAIAudioProcessor::getAuditionRowId() const
{
    return {}; // RED stub.
}

// This creates new instances of the plugin — required by every JUCE plugin, build
// fails at link without it.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordAIAudioProcessor();
}
