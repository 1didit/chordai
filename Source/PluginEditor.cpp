#include "PluginEditor.h"

ChordAIAudioProcessorEditor::ChordAIAudioProcessorEditor (ChordAIAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (conveyor);
    addAndMakeVisible (waveformView);
    addAndMakeVisible (regionOverlay); // on top of waveformView (z-order) — handles all mouse input
    addAndMakeVisible (midiSetsPlaceholder);

    conveyor.onFileDropped = [this] (juce::File f) { processor.loadAudioFile (std::move (f)); };
    regionOverlay.onRegionChanged = [this] (juce::Range<double> r) { processor.setSelectedRegion (r); };

    processor.loadBroadcaster.addChangeListener (this);

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

    waveformArea = bounds; // remaining middle (~260 px)
    waveformView.setBounds (waveformArea);
    regionOverlay.setBounds (waveformArea);
}

void ChordAIAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
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
}
