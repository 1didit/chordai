#pragma once

#include <JuceHeader.h>

// Pixel-art conveyor belt strip — the plugin's locked visual identity
// (02-CONTEXT.md, USER DECISION). Animates left->right on a juce::Timer and
// doubles as the OS file-drag-and-drop target for audio import.
//
// RT-safety: this class lives entirely on the message thread (JUCE Timer
// callbacks always run there). It must NEVER be referenced from
// AudioProcessor::processBlock or any audio-thread code path.
class ConveyorBeltComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               private juce::Timer
{
public:
    ConveyorBeltComponent();
    ~ConveyorBeltComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Fired with the dropped audio file; caller (PluginEditor) wires this to
    // processor.loadAudioFile.
    std::function<void (juce::File)> onFileDropped;

    // Single source of truth for which files the conveyor accepts. Also used
    // by PluginEditor's whole-window drop target so both paths stay in sync.
    static bool isSupportedAudioFile (const juce::StringArray& files);

    // Lets PluginEditor light up the belt while a file is dragged anywhere
    // over the editor (the belt stays the visual metaphor even though the
    // whole window accepts drops).
    void setExternalDragHover (bool shouldHighlight);

    // Falling-chunk visual STUB (locked scope: "at most a visual stub/
    // placeholder animation trigger"). Pushes one chunk that falls off the
    // right end of the belt. Nothing calls this yet in Phase 2 — Plan 02-03
    // fires it once on load-complete. Real chunks arrive with generation
    // (Phase 5); do not extend this beyond the stub.
    void triggerChunkFallStub();

private:
    void timerCallback() override;

    struct FallingChunk
    {
        float x, y, vy;
    };

    static constexpr int slatSpacing = 8; // logical px between tread slats

    int beltOffset = 0;
    bool dragHover = false;

    juce::Image frame; // small logical-resolution buffer, upscaled nearest-neighbour in paint()
    std::vector<FallingChunk> chunks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConveyorBeltComponent)
};
