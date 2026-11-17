#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

//==============================================================================
/** Bottom strip: the held chord as the arpeggiator sees it, and a scrolling view of
    the notes the plugin sends out. */
class MidiMonitor : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit MidiMonitor (MelodyMagicProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    struct Shown { int note, vel, channel; double on, off; };

    MelodyMagicProcessor& proc;
    std::vector<Shown> notes;
    std::vector<int> chord;
    juce::String lastText;
    int sentCount = 0;
    static constexpr double kWindowMs = 4000.0;
};
