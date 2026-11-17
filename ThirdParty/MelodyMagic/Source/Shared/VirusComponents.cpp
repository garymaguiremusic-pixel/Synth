#include "VirusComponents.h"
#include "CustomLookAndFeel.h"

using namespace juce;

//==============================================================================
// VirusKnob
//==============================================================================
VirusKnob::VirusKnob (Size s, const String& labelText)
    : Slider (Slider::RotaryVerticalDrag, Slider::NoTextBox), knobSize (s), label (labelText)
{
    setRange (0.0, 100.0, 1.0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setName (labelText);
}

String VirusKnob::valueText() const
{
    return describeValue ? describeValue (getValue()) : String (roundToInt (getValue()));
}

String VirusKnob::getTooltip()
{
    String text = label.toUpperCase() + "  " + valueText();
    if (extraTooltip.isNotEmpty())
        text << "\n" << extraTooltip;
    if (lockable)
        text << (locked ? "\nLocked: Randomize leaves it alone (right-click to unlock)"
                        : "\nRight-click to lock it against Randomize");
    return text;
}

void VirusKnob::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float pos = (float) valueToProportionOfLength (getValue());
    const auto rotary = getRotaryParameters();

    Rectangle<float> knobArea;
    if (knobSize == Size::Large)
    {
        knobArea = bounds;
    }
    else
    {
        auto labelArea = bounds.removeFromBottom (14.0f);
        const float side = std::min (bounds.getWidth(), bounds.getHeight());
        knobArea = bounds.withSizeKeepingCentre (side, side);

        if ((hovering || dragging) && isEnabled())
        {
            g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
            g.setColour (VirusColours::blue);
            g.drawFittedText (valueText(), labelArea.toNearestInt(), Justification::centred, 1, 0.7f);
        }
        else
        {
            CustomLookAndFeel::drawSmallLabel (g, label, labelArea);
        }
    }

    getLookAndFeel().drawRotarySlider (g, (int) knobArea.getX(), (int) knobArea.getY(),
                                       (int) knobArea.getWidth(), (int) knobArea.getHeight(),
                                       pos, rotary.startAngleRadians, rotary.endAngleRadians, *this);

    if (lockable && locked)
    {
        // small padlock at the top right of the knob
        auto lock = Rectangle<float> (9.0f, 11.0f).withPosition (knobArea.getRight() - 10.0f, knobArea.getY());
        auto body = lock.removeFromBottom (6.5f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (body.translated (0.0f, 1.0f), 1.5f);
        g.setColour (VirusColours::blue);
        g.fillRoundedRectangle (body, 1.5f);
        Path shackle;
        shackle.addCentredArc (lock.getCentreX(), body.getY(), 2.6f, 3.2f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
        g.strokePath (shackle, PathStrokeType (1.4f));
    }
}

void VirusKnob::setValueWithGesture (double v)
{
    if (onGestureBegin) onGestureBegin();
    setValue (jlimit (getMinimum(), getMaximum(), std::round (v)), sendNotificationSync);
    if (onGestureEnd) onGestureEnd();
}

void VirusKnob::mouseDown (const MouseEvent& e)
{
    if (! isEnabled())
        return;

    if (e.mods.isPopupMenu())
    {
        if (lockable)
        {
            setLocked (! locked);
            if (onLockChanged) onLockChanged (locked);
        }
        return;
    }

    dragging = true;
    lastDragY = e.position.y;
    dragValue = getValue();
    if (onGestureBegin) onGestureBegin();
    repaint();
}

void VirusKnob::mouseDrag (const MouseEvent& e)
{
    if (! isEnabled() || ! dragging)
        return;

    const double pixels = (double) (lastDragY - e.position.y);
    lastDragY = e.position.y;

    // Full range in 200 pixels (Shift: 4x slower). The value always moves in whole steps.
    const double range = getMaximum() - getMinimum();
    const double perPixel = std::max (range / 200.0, 0.08) * (e.mods.isShiftDown() ? 0.25 : 1.0);
    dragValue = jlimit (getMinimum(), getMaximum(), dragValue + pixels * perPixel);

    const double snapped = std::round (dragValue);
    if (std::abs (snapped - getValue()) > 1.0e-9)
        setValue (snapped, sendNotificationSync);
}

void VirusKnob::mouseUp (const MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    if (onGestureEnd) onGestureEnd();
    repaint();
}

void VirusKnob::mouseDoubleClick (const MouseEvent& e)
{
    if (isEnabled() && ! e.mods.isPopupMenu())
        setValueWithGesture (defaultValue);
}

void VirusKnob::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (! isEnabled())
        return;

    const float dir = (wheel.isReversed ? -1.0f : 1.0f) * (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX);
    if (std::abs (dir) < 1.0e-6f)
        return;

    // One notch = 1.0
    setValueWithGesture (std::round (getValue()) + (dir > 0.0f ? 1.0 : -1.0));
}

void VirusKnob::mouseEnter (const MouseEvent&) { hovering = true;  repaint(); }
void VirusKnob::mouseExit (const MouseEvent&)  { hovering = false; repaint(); }

//==============================================================================
// KnobAttachment
//==============================================================================
KnobAttachment::KnobAttachment (RangedAudioParameter& p, VirusKnob& k)
    : knob (k),
      attachment (p, [this] (float v) { knob.setValue (v, dontSendNotification); knob.repaint(); })
{
    const auto range = p.getNormalisableRange();
    knob.setRange (range.start, range.end, 1.0);
    knob.setDefaultValue (p.convertFrom0to1 (p.getDefaultValue()));

    knob.onGestureBegin = [this] { attachment.beginGesture(); };
    knob.onGestureEnd   = [this] { attachment.endGesture(); };
    knob.onValueChange  = [this] { attachment.setValueAsPartOfGesture ((float) knob.getValue()); };

    attachment.sendInitialUpdate();
}

KnobAttachment::~KnobAttachment()
{
    knob.onGestureBegin = nullptr;
    knob.onGestureEnd = nullptr;
    knob.onValueChange = nullptr;
}

//==============================================================================
// LcdDisplay
//==============================================================================
void LcdDisplay::setContent (const String& name, const String& value)
{
    if (name != nameText || value != valueText)
    {
        nameText = name;
        valueText = value;
        repaint();
    }
}

void LcdDisplay::setDimmed (bool shouldDim)
{
    if (dimmed != shouldDim)
    {
        dimmed = shouldDim;
        repaint();
    }
}

void LcdDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    CustomLookAndFeel::drawLcd (g, b, dimmed);

    const auto textColour = dimmed ? Colour (0xff8a9097) : Colours::white;
    auto textArea = b.reduced (6.0f, 0.0f);

    const auto font = CustomLookAndFeel::uiFont (11.0f, true);
    g.setFont (font);

    // value on the right; the name gets whatever is left (squeezed if needed)
    const float valueWidth = valueText.isEmpty() ? 0.0f : GlyphArrangement::getStringWidth (font, valueText) + 4.0f;
    auto nameArea = textArea.withTrimmedRight (valueWidth).toNearestInt();

    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawFittedText (nameText, nameArea.translated (0, 1), Justification::centredLeft, 1, 0.75f);
    g.drawText (valueText, textArea.translated (0.0f, 1.0f), Justification::centredRight);
    g.setColour (textColour);
    g.drawFittedText (nameText, nameArea, Justification::centredLeft, 1, 0.75f);
    g.drawText (valueText, textArea, Justification::centredRight);
}

//==============================================================================
// HelpButton / HelpOverlay
//==============================================================================
void HelpButton::paintButton (Graphics& g, bool isHighlighted, bool isDown)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (b.translated (0.0f, 1.0f));
    g.setGradientFill (ColourGradient (isHighlighted ? Colour (0xff8a9096) : Colour (0xff6f757b), 0.0f, b.getY(),
                                       Colour (0xff35393d), 0.0f, b.getBottom(), false));
    g.fillEllipse (isDown ? b.reduced (0.5f) : b);
    g.setColour (VirusColours::bevelDark);
    g.drawEllipse (b, 1.0f);
    g.setColour (Colours::white);
    g.setFont (CustomLookAndFeel::uiFont (12.0f, true));
    g.drawText ("?", b, Justification::centred);
}

void HelpOverlay::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.75f));

    auto box = getLocalBounds().toFloat().reduced (24.0f, 24.0f);
    CustomLookAndFeel::drawPanel (g, box, title);

    auto text = box.reduced (14.0f, 0.0f).withTrimmedTop (28.0f);
    const float lineH = jmin (22.0f, (text.getHeight() - 8.0f) / (float) jmax (1, lines.size() + 1));
    g.setFont (CustomLookAndFeel::uiFont (11.5f));
    for (auto& line : lines)
    {
        g.setColour (line.startsWith ("Click") ? VirusColours::blue : Colour (0xffdfe3e6));
        g.drawFittedText (line, text.removeFromTop (lineH).toNearestInt(), Justification::centredLeft, 1, 0.8f);
    }
}
