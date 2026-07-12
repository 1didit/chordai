#include "MidiSetsPlaceholder.h"

void MidiSetsPlaceholder::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101018));

    g.setColour (juce::Colour (0xff2a2a38));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    g.setColour (juce::Colour (0xff5a5a6e));
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.drawText (juce::String (juce::CharPointer_UTF8 ("MIDI SETS \xe2\x80\x94 ANALYZE A TRACK TO FILL THE CONVEYOR")),
                getLocalBounds(),
                juce::Justification::centred);
}
