#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Rotary knob with Virus-style interaction:
      - vertical drag and scroll wheel move in whole steps (1.0);
        hold Shift for 0.1 steps (64.1, 64.2 ...)
      - double-click resets to the default value
      - small knobs show their label underneath, which swaps to the 0..100 value
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
    std::function<juce::String (float)> describeValue;   // real units for the tooltip
    std::function<juce::String (double)> displayValue;   // optional hover text (e.g. "1/8") instead of the number
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

private:
    void setValueWithGesture (double v);

    Size knobSize;
    juce::String label;
    double defaultValue = 64.0;
    float lastDragY = 0.0f;
    double dragValue = 0.0;          // un-snapped drag position
    bool dragShift = false;
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
/** Blue LCD readout, e.g. "TIME      96". */
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
/** Small animated LFO waveform preview. */
class LfoPreview : public juce::Component
{
public:
    enum class Kind { Lfo, TapeWowFlutter, Doppler };

    void setState (Kind, int shape, float phase, float depthNorm);
    void paint (juce::Graphics&) override;

private:
    float valueAt (double phase) const;

    Kind kind = Kind::Lfo;
    int shape = 0;
    float phase = 0.0f, depth = 0.0f;
};

//==============================================================================
/** Bottom strip: L/R repeat pattern for digital modes, spinning reels for tape. */
class ModeVisualizer : public juce::Component
{
public:
    struct State
    {
        int mode = 0;
        int classicType = 1;
        float baseDelayMs = 500.0f;
        float feedback = 0.0f;        // 0..0.95
        float tapeDelayMsL = 500.0f;  // current (gliding) tape delays
        float tapeDelayMsR = 500.0f;
        float tapeSpeed = 1.0f;       // read head speed ratio
        juce::String clockText;       // e.g. "L 3/16  R 4/16"
        float spireDelayMsL = 500.0f;
        float spireDelayMsR = 500.0f;
        float spireFeedback = 0.0f;
        float spireWide = 1.0f;       // -1 .. +1
        bool spirePingPong = false;
    };

    void update (const State&, float deltaSeconds);
    void paint (juce::Graphics&) override;

private:
    void paintPattern (juce::Graphics&, juce::Rectangle<float>);
    void paintTape (juce::Graphics&, juce::Rectangle<float>);
    void paintSpire (juce::Graphics&, juce::Rectangle<float>);

    struct RepeatEvent { float timeMs, ampL, ampR; };
    void drawRepeatLanes (juce::Graphics&, juce::Rectangle<float>, const RepeatEvent*, int numEvents,
                          const juce::String& title, const juce::String& subtitle);
    static void drawReel (juce::Graphics&, juce::Point<float> centre, float radius, float angle);

    State state;
    double elapsedMs = 0.0;
    float reelAngle = 0.0f;
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
    float internalBpm = 120.0f, shownBpm = 120.0f, lastY = 0.0f, dragBpm = 120.0f;
    bool hostTempo = false, dragging = false, dragShift = false;
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
