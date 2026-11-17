#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Rotary knob with Virus Delay's look and interaction:
      - vertical drag and the mouse wheel move in whole steps (1.0); one wheel notch = 1.
        Shift makes the drag slower (still whole steps).
      - double-click resets to the default value
      - small knobs show their label underneath, which swaps to the value while
        hovering or dragging
      - optional lock (generator knobs): right-click toggles it, a padlock is drawn and
        Randomize leaves the knob alone
    Drawing is delegated to CustomLookAndFeel::drawRotarySlider.
*/
class VirusKnob : public juce::Slider
{
public:
    enum class Size { Large, Small };

    VirusKnob (Size, const juce::String& labelText);

    Size getKnobSize() const noexcept           { return knobSize; }
    void setDefaultValue (double v) noexcept    { defaultValue = v; }
    void setLabel (const juce::String& l)       { label = l; setName (l); repaint(); }

    // Lock support (generator knobs)
    void setLockable (bool shouldBeLockable)    { lockable = shouldBeLockable; }
    void setLocked (bool shouldBeLocked)        { locked = shouldBeLocked; repaint(); }
    bool isLocked() const noexcept              { return locked; }
    std::function<void (bool)> onLockChanged;

    std::function<void()> onGestureBegin, onGestureEnd;
    std::function<juce::String (double)> describeValue;   // text for the label / tooltip (default: the number)
    juce::String extraTooltip;

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
    juce::String valueText() const;

    Size knobSize;
    juce::String label;
    double defaultValue = 0.0;
    float lastDragY = 0.0f;
    double dragValue = 0.0;          // un-snapped drag position
    bool dragging = false, hovering = false;
    bool lockable = false, locked = false;
};

//==============================================================================
/** Connects a VirusKnob to an integer parameter with proper host gesture handling. */
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
/** Blue LCD readout: name on the left, value on the right. */
class LcdDisplay : public juce::Component,
                   public juce::SettableTooltipClient
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
    HelpOverlay (juce::String titleText, juce::StringArray helpLines)
        : title (std::move (titleText)), lines (std::move (helpLines)) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }

private:
    juce::String title;
    juce::StringArray lines;
};
