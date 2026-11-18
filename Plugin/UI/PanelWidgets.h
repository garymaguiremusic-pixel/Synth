#pragma once
// Widgets of the main window: the oscillator readout (octave / semi / fine and the wave),
// octave down / up buttons, and the output EQ curve with draggable band points.

#include <juce_gui_basics/juce_gui_basics.h>
#include "ParamAdapters.h"

/** Recessed screen: OCT, SEMI and FINE (or DET) values over the oscillator's waveform. */
class OscDisplay : public juce::Component
{
public:
    ParamAdapter* oct = nullptr;
    ParamAdapter* semi = nullptr;
    ParamAdapter* fine = nullptr;
    juce::String fineLabel = "FINE";
    std::function<float (float)> wave;
    void paint (juce::Graphics&) override;
};

/** Two small buttons that step a value down / up by one (octave). */
class StepperButtons : public juce::Component
{
public:
    explicit StepperButtons (ParamAdapter& a);
    void resized() override;
    std::function<void()> onTouched;
private:
    ParamAdapter& adapter;
    juce::TextButton down { juce::String::fromUTF8 ("\xe2\x96\xbc") }, up { juce::String::fromUTF8 ("\xe2\x96\xb2") };
};

/** The output EQ: grid, the summed response of the three bands, a point per band.
    Drag a point: left / right = frequency, up / down = gain. Clicking a point selects its band. */
class EqGraph : public juce::Component
{
public:
    EqGraph (std::array<ParamAdapter*, 3> freq, std::array<ParamAdapter*, 3> q, std::array<ParamAdapter*, 3> gain,
             std::function<double (int)> freqHz, std::function<double (int)> qValue);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int selected = 1;
    std::function<void (int)> onSelect;
    std::function<bool()> enabled;

private:
    float xForHz (double hz) const;
    double hzForX (float x) const;
    float yForDb (double db) const;
    double responseDb (double hz) const;
    juce::Point<float> nodePos (int band) const;
    std::array<ParamAdapter*, 3> f, q, g;
    std::function<double (int)> toHz, toQ;
    int dragging = -1;
};
