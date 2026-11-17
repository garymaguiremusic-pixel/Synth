#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Rotary knob with Virus-style interaction:
      - vertical drag in steps of 1; shift = fine steps of 0.1
      - scroll wheel: one notch = 1 (shift = 0.1)
      - double-click resets to the default value
      - small knobs show their label underneath, which swaps to the knob value
        while hovering or dragging
      - tooltips show real units
    Drawing is delegated to CustomLookAndFeel::drawRotarySlider.
*/
class VirusKnob : public juce::Slider
{
public:
    enum class Size { Large, Small };

    VirusKnob (Size, const juce::String& labelText);

    Size getKnobSize() const noexcept           { return knobSize; }
    void setDefaultValue (double v) noexcept    { defaultValue = v; }

    std::function<void()> onGestureBegin, onGestureEnd;
    std::function<void()> onRightClick;                  // e.g. type in a value
    std::function<juce::String (float)> describeValue;   // real units for the tooltip
    juce::String inactiveHint;                           // shown in the tooltip while disabled

    juce::String getTooltip() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr double kStep = 1.0, kFineStep = 0.1;

private:
    void setValueWithGesture (double v);
    double snapToStep (double v, double step) const;

    Size knobSize;
    juce::String label;
    double defaultValue = 500.0;
    float lastDragY = 0.0f, wheelAccumulator = 0.0f;
    double dragValue = 0.0;
    bool dragging = false, hovering = false;
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
/** Blue LCD readout, e.g. "DECAY     2.40s". */
class LcdDisplay : public juce::Component
{
public:
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && onRightClick) onRightClick(); }

    void setContent (const juce::String& name, const juce::String& value);
    void setDimmed (bool shouldDim);
    void paint (juce::Graphics&) override;

private:
    juce::String nameText, valueText;
    bool dimmed = false;
};

//==============================================================================
/** Bottom strip: an animated picture of the reverb tail in the L and R lanes. */
class ReverbVisualizer : public juce::Component
{
public:
    struct State
    {
        int mode = 0;
        float predelayMs = 0.0f;
        float decaySeconds = 2.0f;
        float width = 1.0f;        // 0 .. 2
    };

    void update (const State&, float deltaSeconds);
    void paint (juce::Graphics&) override;

private:
    State state;
    double elapsedMs = 0.0;
};

//==============================================================================
/** Tempo readout. Shows the host tempo, or the draggable internal fallback BPM. */
class TempoField : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    explicit TempoField (juce::RangedAudioParameter& bpmParam);

    void setTempo (float bpm, bool fromHost);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float internalBpm = 120.0f, shownBpm = 120.0f, lastY = 0.0f;
    bool hostTempo = false, dragging = false;
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
