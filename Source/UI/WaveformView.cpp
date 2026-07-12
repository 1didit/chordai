#include "WaveformView.h"

namespace
{
    const juce::Colour backgroundColour { 0xff14141c };
    const juce::Colour errorColour      { 0xffc85050 };
    const juce::Colour hintColour       { 0xff5a5a6e };
    const juce::Colour waveformColour   { 0xff7ec8c8 };
}

WaveformView::WaveformView()
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener (this);
}

WaveformView::~WaveformView()
{
    thumbnail.removeChangeListener (this);
}

void WaveformView::setSource (const juce::File& file)
{
    errorMessage.clear();
    thumbnail.setSource (new juce::FileInputSource (file));
    repaint();
}

void WaveformView::setErrorMessage (juce::String message)
{
    errorMessage = std::move (message);
    repaint();
}

double WaveformView::getTotalLength() const
{
    return thumbnail.getTotalLength();
}

void WaveformView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The thumbnail broadcasts as its async scan progresses — repaint so the
    // waveform fills in live.
    repaint();
}

void WaveformView::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    if (errorMessage.isNotEmpty())
    {
        g.setColour (errorColour);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText (errorMessage, getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (getTotalLength() <= 0.0)
    {
        g.setColour (hintColour);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText ("DROP A TRACK ONTO THE CONVEYOR", getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour (waveformColour);
    thumbnail.drawChannels (g, getLocalBounds().reduced (4), 0.0, thumbnail.getTotalLength(), 0.95f);
}
