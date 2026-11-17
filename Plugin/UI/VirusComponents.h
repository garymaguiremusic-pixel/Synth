#pragma once
// UI components in the Virus Delay style (procedurally drawn, no image assets).

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

//==============================================================================
/**
    Rotary knob with Virus-style interaction:
      - vertical drag, the scroll wheel and the arrow keys move in whole steps (1.0)
        (Shift = slower drag, still whole steps)
      - double-click resets to the default value
      - the label underneath swaps to the value text while hovering or dragging
      - the tooltip shows the parameter name and its value text
    Drawing is delegated to CustomLookAndFeel::drawRotarySlider.
*/
class VirusKnob : public juce::Slider
{
public:
    enum class Size { Large, Small };

    VirusKnob (Size, const juce::String& labelText);

    Size getKnobSize() const noexcept           { return knobSize; }
    void setDefaultValue (double v) noexcept    { defaultValue = v; }
    void setBipolar (bool b) noexcept           { bipolar = b; }
    bool isBipolar() const noexcept             { return bipolar; }
    void setLabel (const juce::String& s)       { label = s; repaint(); }

    std::function<void()> onGestureBegin, onGestureEnd, onTouched;
    std::function<juce::String (double)> valueText;      // value -> display text

    juce::String getTooltip() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void setValueWithGesture (double v);
    juce::String textFor (double v) const { return valueText ? valueText (v) : juce::String (juce::roundToInt (v)); }

    Size knobSize;
    juce::String label;
    double defaultValue = 0.0;
    float lastDragY = 0.0f;
    double dragValue = 0.0;
    bool dragging = false, hovering = false, bipolar = false;
};

//==============================================================================
/** Connects a VirusKnob to a parameter with proper host gesture handling. */
class KnobAttachment
{
public:
    KnobAttachment (juce::RangedAudioParameter&, VirusKnob&);
    ~KnobAttachment();

private:
    VirusKnob& knob;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** Dropdown for list parameters: the scroll wheel steps one entry (whole steps). */
class VirusCombo : public juce::ComboBox
{
public:
    VirusCombo() { setScrollWheelEnabled (true); }
    std::function<void()> onTouched;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown (const juce::MouseEvent& e) override { if (onTouched) onTouched(); juce::ComboBox::mouseDown (e); }
};

/** Connects a VirusCombo to an integer parameter (item id = value - min + 1). */
class ComboAttachment
{
public:
    ComboAttachment (juce::RangedAudioParameter&, VirusCombo&, int minValue);
    ~ComboAttachment();

private:
    VirusCombo& box;
    int minValue;
    juce::ParameterAttachment attachment;
};

/** On/off button for 0..1 parameters. */
class ToggleAttachment
{
public:
    ToggleAttachment (juce::RangedAudioParameter&, juce::Button&);
    ~ToggleAttachment();

private:
    juce::Button& button;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** Blue LCD readout, e.g. "CUTOFF      96". */
class LcdDisplay : public juce::Component
{
public:
    void setContent (const juce::String& name, const juce::String& value);
    void setDimmed (bool shouldDim);
    void paint (juce::Graphics&) override;

private:
    juce::String nameText, valueText;
    bool dimmed = false;
};

//==============================================================================
/** Small LCD plot of a function on 0..1 (envelopes, LFO shapes, oscillator waves). */
class CurvePreview : public juce::Component
{
public:
    std::function<float (float)> fn;   // x 0..1 -> y -1..1
    juce::String caption;
    void paint (juce::Graphics&) override;
};

//==============================================================================
/** Round "?" button in the bottom bar. */
class HelpButton : public juce::Button
{
public:
    HelpButton() : juce::Button ("help") {}
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
};

/** Quick reference shown over the editor; click anywhere to close. */
class HelpOverlay : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};
