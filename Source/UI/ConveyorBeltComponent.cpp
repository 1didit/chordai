#include "ConveyorBeltComponent.h"

namespace
{
    constexpr int pixelScale = 4; // logical render is 1/4 resolution, upscaled nearest-neighbour

    const juce::Colour backgroundColour { 0xff16161e };
    const juce::Colour beltColour       { 0xff3a3a4a };
    const juce::Colour slatColour       { 0xff23232e };
    const juce::Colour rollerColour     { 0xff5a5a6e };
    const juce::Colour edgeHighlight    { 0x33ffffff };
    const juce::Colour dragOutline      { 0xffe8c547 };
    const juce::Colour chunkColour      { 0xff7ec850 };
}

ConveyorBeltComponent::ConveyorBeltComponent()
{
    startTimerHz (30);
}

ConveyorBeltComponent::~ConveyorBeltComponent()
{
    stopTimer();
}

void ConveyorBeltComponent::resized()
{
    auto w = juce::jmax (1, getWidth() / pixelScale);
    auto h = juce::jmax (1, getHeight() / pixelScale);
    frame = juce::Image (juce::Image::ARGB, w, h, true);
}

void ConveyorBeltComponent::timerCallback()
{
    beltOffset = (beltOffset + 1) % slatSpacing;

    // Advance falling chunks (stub physics only).
    constexpr float gravity = 0.6f;
    for (auto& c : chunks)
    {
        c.vy += gravity;
        c.y += c.vy;
    }

    auto belowBounds = (float) getHeight() + 8.0f;
    chunks.erase (std::remove_if (chunks.begin(), chunks.end(),
                                   [belowBounds] (const FallingChunk& c) { return c.y > belowBounds; }),
                  chunks.end());

    repaint(); // dirties only this component's own bounds
}

void ConveyorBeltComponent::paint (juce::Graphics& g)
{
    if (frame.isNull())
        return;

    juce::Graphics fg (frame);
    fg.fillAll (backgroundColour);

    auto logicalW = frame.getWidth();
    auto logicalH = frame.getHeight();

    // Belt surface band across the middle ~60% of the strip.
    auto beltTop = juce::roundToInt (logicalH * 0.2f);
    auto beltHeight = juce::roundToInt (logicalH * 0.6f);
    juce::Rectangle<int> beltRect (0, beltTop, logicalW, beltHeight);
    fg.setColour (dragHover ? beltColour.brighter (0.3f) : beltColour);
    fg.fillRect (beltRect);

    // Tread slats travelling left->right (shifted by +beltOffset each frame).
    fg.setColour (slatColour);
    for (int x = -slatSpacing + beltOffset; x < logicalW; x += slatSpacing)
        fg.fillRect (x, beltTop, 2, beltHeight);

    // Roller circles at the far left and right ends.
    auto rollerDiameter = beltHeight;
    fg.setColour (rollerColour);
    fg.fillEllipse ((float) 0, (float) beltTop, (float) rollerDiameter, (float) rollerDiameter);
    fg.fillEllipse ((float) (logicalW - rollerDiameter), (float) beltTop, (float) rollerDiameter, (float) rollerDiameter);

    // 1-px top/bottom edge highlights for depth.
    fg.setColour (edgeHighlight);
    fg.drawHorizontalLine (beltTop, 0.0f, (float) logicalW);
    fg.drawHorizontalLine (beltTop + beltHeight - 1, 0.0f, (float) logicalW);

    // Falling-chunk stub: piano-roll note look.
    fg.setColour (chunkColour);
    for (auto& c : chunks)
        fg.fillRect (juce::Rectangle<float> (c.x, c.y, 3.0f, 2.0f));

    // Blit the logical frame up to full component size — nearest-neighbour
    // upscale IS the pixel-art look.
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImageWithin (frame, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);

    if (dragHover)
    {
        g.setColour (dragOutline);
        g.drawRect (getLocalBounds(), 2);
    }
}

bool ConveyorBeltComponent::isSupportedAudioFile (const juce::StringArray& files)
{
    if (files.size() != 1)
        return false;

    // .m4a/.aac decode for free on macOS via CoreAudioFormat (same path MP3
    // already uses) — common for tracks from Apple Music/iTunes rips.
    auto ext = juce::File (files[0]).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".aiff" || ext == ".aif"
        || ext == ".flac" || ext == ".m4a" || ext == ".aac";
}

bool ConveyorBeltComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return isSupportedAudioFile (files);
}

void ConveyorBeltComponent::setExternalDragHover (bool shouldHighlight)
{
    if (dragHover != shouldHighlight)
    {
        dragHover = shouldHighlight;
        repaint();
    }
}

void ConveyorBeltComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void ConveyorBeltComponent::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void ConveyorBeltComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    repaint();

    if (onFileDropped && files.size() == 1)
        onFileDropped (juce::File (files[0]));
}

void ConveyorBeltComponent::triggerChunkFallStub()
{
    auto beltTopPx = getHeight() * 0.2f;
    chunks.push_back ({ (float) getWidth() - 4.0f, beltTopPx, 0.0f });
}
