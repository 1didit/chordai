#include "ConveyorBeltComponent.h"

#include <cmath>

namespace
{
    constexpr int pixelScale = 4; // logical render is 1/4 resolution, upscaled nearest-neighbour

    const juce::Colour backgroundColour { 0xff16161e };
    const juce::Colour beltColour       { 0xff3a3a4a };
    const juce::Colour slatColour       { 0xff23232e };
    const juce::Colour rollerColour     { 0xff5a5a6e };
    const juce::Colour edgeHighlight    { 0x33ffffff };
    const juce::Colour dragOutline      { 0xffe8c547 };
    const juce::Colour progressFill     { 0xffe8c547 }; // same gold accent as dragOutline
    const juce::Colour progressTrack    { 0x33e8c547 }; // dim variant — 0% still reads as "armed"
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
    // UI-01 (locked): the belt is a moving metaphor only while a file is
    // dragged over the editor or while analysis is running. Idle, it is
    // completely STOPPED — beltOffset freezes exactly where it was, so the
    // slats (and, later, the roller rotation cue) stop dead too.
    bool active = dragHover || analyzing;
    if (active)
        beltOffset = (beltOffset + (analyzing ? 2 : 1)) % slatSpacing;

    // Advance falling chunks: gravity + horizontal carry, with a single
    // small bounce off the "floor" before falling through and despawning.
    constexpr float gravity = 0.6f;
    auto bounceY = (float) getHeight() * 0.92f;
    for (auto& c : chunks)
    {
        c.vy += gravity;
        c.y += c.vy;
        c.x += c.vx;

        if (! c.bounced && c.y >= bounceY)
        {
            c.vy = -c.vy * 0.35f;
            c.vx *= 0.7f;
            c.bounced = true;
        }
    }

    auto belowBounds = (float) getHeight() + 8.0f;
    chunks.erase (std::remove_if (chunks.begin(), chunks.end(),
                                   [belowBounds] (const FallingChunk& c) { return c.y > belowBounds; }),
                  chunks.end());

    // Nothing is moving (belt frozen, no chunks in flight) — skip the 30Hz
    // repaint churn while fully idle/static. The state-change setters below
    // (setExternalDragHover, setAnalysisProgress, fileDragEnter/Exit) still
    // call repaint() directly, so transitions into/out of idle repaint
    // immediately regardless of this guard.
    if (active || ! chunks.empty())
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
    auto beltBase = dragHover ? beltColour.brighter (0.3f) : beltColour;

    // Two-tone shading (lighter top half / darker bottom half) for depth.
    auto topHalfHeight = beltHeight / 2;
    fg.setColour (beltBase.brighter (0.12f));
    fg.fillRect (0, beltTop, logicalW, topHalfHeight);
    fg.setColour (beltBase.darker (0.12f));
    fg.fillRect (0, beltTop + topHalfHeight, logicalW, beltHeight - topHalfHeight);

    // Deterministic dither: sparse darker pixels indexed off (x + beltOffset)
    // so the texture travels WITH the belt when moving and freezes when
    // idle — a pure function of state, no randomness involved.
    fg.setColour (beltBase.darker (0.35f));
    for (int x = 0; x < logicalW; ++x)
        for (int y = 0; y < beltHeight; ++y)
            if ((x + beltOffset) % 4 == 0 && y % 3 == 1)
                fg.fillRect (x, beltTop + y, 1, 1);

    // Tread slats travelling left->right (shifted by +beltOffset each
    // frame), alternating shade for relief plus a 1px highlight on each
    // slat's leading (rightmost, direction-of-travel) edge.
    int slatIndex = 0;
    for (int x = -slatSpacing + beltOffset; x < logicalW; x += slatSpacing, ++slatIndex)
    {
        fg.setColour ((slatIndex % 2 == 0) ? slatColour.brighter (0.15f) : slatColour);
        fg.fillRect (x, beltTop, 2, beltHeight);
        fg.setColour (edgeHighlight);
        fg.fillRect (x + 1, beltTop, 1, beltHeight);
    }

    // Roller circles at the far left and right ends, each with a rotation
    // cue (two notches + a static axle pixel) synced to belt motion via
    // beltOffset — freezes dead when idle, same as the slats.
    auto rollerDiameter = beltHeight;
    fg.setColour (rollerColour);
    fg.fillEllipse ((float) 0, (float) beltTop, (float) rollerDiameter, (float) rollerDiameter);
    fg.fillEllipse ((float) (logicalW - rollerDiameter), (float) beltTop, (float) rollerDiameter, (float) rollerDiameter);

    {
        auto rollerPhase = (beltOffset / (double) slatSpacing) * juce::MathConstants<double>::twoPi;
        auto rollerRadius = rollerDiameter * 0.5f;
        auto notchRadius = rollerRadius * 0.7f;
        auto notchColour = rollerColour.darker (0.5f);

        auto drawRoller = [&] (float centreX)
        {
            auto centreY = (float) beltTop + rollerRadius;
            fg.setColour (notchColour);
            fg.fillRect (juce::Rectangle<float> (centreX - 0.5f, centreY - 0.5f, 1.0f, 1.0f)); // static axle

            for (int i = 0; i < 2; ++i)
            {
                auto angle = rollerPhase + i * juce::MathConstants<double>::pi;
                auto nx = centreX + notchRadius * (float) std::cos (angle);
                auto ny = centreY + notchRadius * (float) std::sin (angle);
                fg.fillRect (juce::Rectangle<float> (nx - 0.5f, ny - 0.5f, 1.0f, 1.0f));
            }
        };
        drawRoller (rollerRadius);
        drawRoller ((float) logicalW - rollerRadius);
    }

    // 1-px top/bottom edge highlights for depth, plus a drop-shadow line
    // just under the belt band for lift off the background.
    fg.setColour (edgeHighlight);
    fg.drawHorizontalLine (beltTop, 0.0f, (float) logicalW);
    fg.drawHorizontalLine (beltTop + beltHeight - 1, 0.0f, (float) logicalW);
    fg.setColour (juce::Colour (0x66000000));
    fg.drawHorizontalLine (beltTop + beltHeight, 0.0f, (float) logicalW);

    // Idle invitation text (UI-01, locked): shown whenever the belt is
    // stopped — i.e. no active drag-over and no analysis in progress — and
    // persists between loads (it is not tied to "no file loaded yet").
    // Drawn INTO the logical low-res frame so the existing nearest-neighbour
    // upscale below turns it into the plugin's pixel-font look; zero new
    // font/image assets. Disappears the moment the belt starts moving
    // (drag-hover or analysis), letting the belt metaphor take over.
    if (! dragHover && ! analyzing)
    {
        fg.setColour (juce::Colour (0xccd8d8e0)); // dim warm white — invitation, not an alert (gold is reserved)
        fg.setFont (juce::Font (juce::FontOptions (7.0f)));
        fg.drawFittedText ("drop song or melody here", beltRect, juce::Justification::centred, 1);
    }

    // Analysis progress fill: a flat gold bar sitting on the belt's top edge,
    // painted inside the logical frame so it inherits the pixel-art
    // chunkiness on upscale. Shown for the full duration of analysis, plus
    // any brief tail where fraction hasn't yet reached 1.0.
    if (analyzing || (analysisFraction > 0.0 && analysisFraction < 1.0))
    {
        constexpr int fillHeight = 2;
        auto filledWidth = juce::roundToInt (logicalW * (float) juce::jlimit (0.0, 1.0, analysisFraction));
        fg.setColour (progressTrack);
        fg.fillRect (0, beltTop, logicalW, fillHeight);
        fg.setColour (progressFill);
        fg.fillRect (0, beltTop, filledWidth, fillHeight);
    }

    // Falling chunks: piano-roll note look, with a 1px darker outline row
    // for read. Physics runs in full-component-resolution units (matches
    // triggerChunkFallStub's spawn point at the real right roller edge), so
    // positions are scaled down by pixelScale here to land correctly in the
    // logical frame's own coordinate space.
    auto outlineColour = chunkColour.darker (0.4f);
    for (auto& c : chunks)
    {
        juce::Rectangle<float> r (c.x / (float) pixelScale, c.y / (float) pixelScale, 4.0f, 2.0f);
        fg.setColour (chunkColour);
        fg.fillRect (r);
        fg.setColour (outlineColour);
        fg.fillRect (juce::Rectangle<float> (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f));
    }

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

void ConveyorBeltComponent::setAnalysisProgress (double fraction, bool nowAnalyzing)
{
    if (analysisFraction != fraction || analyzing != nowAnalyzing)
    {
        analysisFraction = fraction;
        analyzing = nowAnalyzing;
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
    // Small horizontal carry (vx) so the chunk arcs off the belt's right
    // roller instead of dropping straight down.
    chunks.push_back ({ (float) getWidth() - 4.0f, beltTopPx, 1.5f, 0.0f, false });
}
