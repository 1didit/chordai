#pragma once

#include <JuceHeader.h>

// Deterministic decaying piano-ish voice for one note: 0.6*triangle +
// 0.4*naive-saw oscillator (equal-temperament frequency from a MIDI pitch)
// through a one-pole lowpass (fixed ~2.5kHz cutoff) shaped by juce::ADSR.
// NO samples, NO randomness (no detune from RNG, no rand() anywhere) --
// determinism is a tested contract, see Source/Audio/AuditionRendererTests.cpp
// via Source/Audio/AuditionRenderer.h. Message-thread only: called from
// AuditionRenderer::render, which allocates freely (the AudioFileLoadJob
// decode-allocation category, NOT the processBlock "prepareToPlay only" rule).
namespace AuditionVoice
{
    constexpr double kAttackSeconds   = 0.005;
    constexpr double kDecaySeconds    = 0.4;
    constexpr float  kSustainLevel    = 0.25f;
    constexpr double kReleaseSeconds  = 0.15;
    constexpr float  kLowpassCutoffHz = 2500.0f;

    // Renders one note as a mono buffer: `noteOnSamples` of held tone (ADSR
    // noteOff fires exactly at that sample index), followed by however many
    // more samples the release stage takes to reach silence (juce::ADSR's
    // release rate is time-bounded, so this always terminates -- see .cpp).
    // gain is note.velocity (0..1), applied as a flat multiplier atop the
    // ADSR envelope. Deterministic: same pitch/velocity/noteOnSamples/
    // sampleRate always produces byte-identical output. noteOnSamples <= 0
    // is defensively clamped to 0 (immediate release, still deterministic).
    juce::AudioBuffer<float> render (int pitch, float velocity, int noteOnSamples, double sampleRate);
}
