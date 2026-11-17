#pragma once
// Global arp section: Gary's MelodyMagic (whole plugin: MIDI in -> MIDI out, with its panel) and
// ArpSampler's step sequencer (its ArpEngine, pattern model and step grid; no sampler).
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace vendor
{
    std::unique_ptr<juce::AudioProcessor> createMelodyMagic();
    std::unique_ptr<juce::Component> createMelodyMagicPanel (juce::AudioProcessor&);
    juce::Point<int> melodyMagicPanelSize();

    /** ArpSampler's arp + step sequencer as a MIDI processor. */
    class StepSequencer
    {
    public:
        StepSequencer();
        ~StepSequencer();

        void prepare (double sampleRate);
        void reset();
        /** Replaces 'midi' with the sequencer's notes (keys in, arp notes out). */
        void process (juce::MidiBuffer& midi, int numSamples, juce::AudioPlayHead*, double fallbackBpm);
        int getCurrentStep() const;

        juce::AudioProcessorValueTreeState& state();

        /** Steps of an imported patch (transpose, velocity, tie per step) and its arp settings. */
        struct Step { int transpose = 0, velocity = 100; bool tie = false; };
        void loadPatchSteps (const std::vector<Step>& steps, bool arpOn, int mode, int octaves, int rateIndex,
                             int rateType, float gatePercent, float swingPercent, int velocityMode, int length);
        void loadPlainArp (bool arpOn, int mode, int octaves, int rateIndex, int rateType);

        std::unique_ptr<juce::Component> createPanel();

        juce::ValueTree saveState() const;
        void loadState (const juce::ValueTree&);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
