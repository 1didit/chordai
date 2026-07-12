#include "PluginProcessor.h"
#include "PluginEditor.h"

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

// This creates new instances of the plugin — required by every JUCE plugin, build
// fails at link without it.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordAIAudioProcessor();
}
