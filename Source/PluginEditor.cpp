#include "PluginEditor.h"

ChordAIAudioProcessorEditor::ChordAIAudioProcessorEditor (ChordAIAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (conveyor);
    addAndMakeVisible (chordTimeline);
    addAndMakeVisible (waveformView);
    addAndMakeVisible (regionOverlay); // on top of waveformView (z-order) — handles all mouse input
    addAndMakeVisible (midiSetsPanel);

    conveyor.onFileDropped = [this] (juce::File f) { processor.loadAudioFile (std::move (f)); };
    regionOverlay.onRegionChanged = [this] (juce::Range<double> r) { processor.setSelectedRegion (r); };

    // Row audition/export hooks (PRV-01/EXP-01/EXP-02) -- assigned BEFORE the
    // editor-reopen restore branch below, which may call setRows immediately.
    // Capturing `this` is safe: midiSetsPanel is a member of this editor, so
    // these hooks cannot outlive it.
    midiSetsPanel.onStopAudition = [this] { processor.stopAudition(); };
    midiSetsPanel.getBpmForExport = [this]
    {
        auto r = processor.getAnalysisResult();
        return (r != nullptr && r->bpm > 0.0) ? r->bpm : 120.0;
    };
    midiSetsPanel.getKeyForExport = [this]
    {
        auto r = processor.getAnalysisResult();
        return r != nullptr ? r->key : KeyResult {};
    };
    midiSetsPanel.onAuditionToggle = [this] (const MidiSetRow& row)
    {
        if (processor.isAuditionPlaying() && processor.getAuditionRowId() == row.id)
            processor.stopAudition();
        else
            processor.startAudition (row);
    };
    midiSetsPanel.isRowPlaying = [this] (const juce::String& id)
    {
        return processor.isAuditionPlaying() && processor.getAuditionRowId() == id;
    };

    processor.loadBroadcaster.addChangeListener (this);
    processor.analysisBroadcaster.addChangeListener (this);

    setSize (800, 520);

    // Editor-reopen case: a DAW may close and reopen the editor while the
    // processor keeps its state (no re-drop happens). Restore the waveform
    // + region immediately if a file is already loaded.
    if (auto loaded = processor.getLoadedAudio(); loaded != nullptr)
        handleLoadComplete (*loaded);
}

ChordAIAudioProcessorEditor::~ChordAIAudioProcessorEditor()
{
    // Dangling-listener crash on editor close otherwise.
    processor.loadBroadcaster.removeChangeListener (this);
    processor.analysisBroadcaster.removeChangeListener (this);
}

void ChordAIAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d12));
}

void ChordAIAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    conveyor.setBounds (bounds.removeFromTop (120));
    midiSetsPanel.setBounds (bounds.removeFromBottom (140));

    chordTimeline.setBounds (bounds.removeFromTop (28));

    waveformArea = bounds; // remaining middle (~230 px)
    waveformView.setBounds (waveformArea);
    regionOverlay.setBounds (waveformArea); // overlay MUST keep identical bounds — owns all mouse input
}

bool ChordAIAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ConveyorBeltComponent::isSupportedAudioFile (files);
}

void ChordAIAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    conveyor.setExternalDragHover (true);
}

void ChordAIAudioProcessorEditor::fileDragExit (const juce::StringArray&)
{
    conveyor.setExternalDragHover (false);
}

void ChordAIAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    conveyor.setExternalDragHover (false);

    if (files.size() == 1)
        processor.loadAudioFile (juce::File (files[0]));
}

void ChordAIAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.analysisBroadcaster)
    {
        // Fires on trigger, progress, and completion. A superseded run never
        // republishes, and the processor doesn't clear the result during
        // re-analysis, so the last good chord timeline stays visible until a
        // fresher one lands.
        chordTimeline.setResult (processor.getAnalysisResult());
        midiSetsPanel.setRows (processor.getMidiSetRows());

        const bool nowAnalyzing = processor.isAnalyzing();
        conveyor.setAnalysisProgress (processor.getAnalysisProgress(), nowAnalyzing);

        // Chunk falls exactly on the analyzing->idle transition with a
        // published result — a superseded/cancelled run never flips
        // analyzing false without publishing (04-01's generation guard), so
        // this never fires spuriously.
        if (wasAnalyzing && ! nowAnalyzing && processor.getAnalysisResult() != nullptr)
            conveyor.triggerChunkFallStub();

        wasAnalyzing = nowAnalyzing;
        return;
    }

    if (auto loaded = processor.getLoadedAudio(); loaded != nullptr && processor.getLastLoadError().isEmpty())
        handleLoadComplete (*loaded);
    else if (processor.getLastLoadError().isNotEmpty())
        waveformView.setErrorMessage (processor.getLastLoadError());
}

void ChordAIAudioProcessorEditor::handleLoadComplete (const LoadedAudio& loaded)
{
    waveformView.setSource (loaded.sourceFile);
    regionOverlay.setTotalLength (loaded.lengthSeconds);

    chordTimeline.setTotalLength (loaded.lengthSeconds);
    // Restores the timeline on editor reopen (result already published) and
    // blanks it on fresh load (processor cleared the result to nullptr
    // before broadcasting, per 04-01).
    chordTimeline.setResult (processor.getAnalysisResult());
    // Same dual role as the timeline: restores rows on editor reopen (already
    // published) and blanks them on fresh load (processor cleared to nullptr
    // before broadcasting).
    midiSetsPanel.setRows (processor.getMidiSetRows());

    // Editor reopened mid-analysis (or after one already ran): show the
    // belt's current busy state immediately instead of waiting for the next
    // analysisBroadcaster message.
    conveyor.setAnalysisProgress (processor.getAnalysisProgress(), processor.isAnalyzing());
    wasAnalyzing = processor.isAnalyzing();
}
