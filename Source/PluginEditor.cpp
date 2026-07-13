#include "PluginEditor.h"

ChordAIAudioProcessorEditor::ChordAIAudioProcessorEditor (ChordAIAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (conveyor);
    addAndMakeVisible (chordTimeline);
    addAndMakeVisible (waveformView);
    addAndMakeVisible (regionOverlay); // on top of waveformView (z-order) — handles all mouse input
    addAndMakeVisible (midiSetsPlaceholder);

    conveyor.onFileDropped = [this] (juce::File f) { processor.loadAudioFile (std::move (f)); };
    regionOverlay.onRegionChanged = [this] (juce::Range<double> r) { processor.setSelectedRegion (r); };

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
    midiSetsPlaceholder.setBounds (bounds.removeFromBottom (140));

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
    conveyor.triggerChunkFallStub(); // placeholder "something came off the belt" feedback

    chordTimeline.setTotalLength (loaded.lengthSeconds);
    // Restores the timeline on editor reopen (result already published) and
    // blanks it on fresh load (processor cleared the result to nullptr
    // before broadcasting, per 04-01).
    chordTimeline.setResult (processor.getAnalysisResult());
}
