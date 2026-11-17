#include "VirusComponents.h"
#include "../CustomLookAndFeel.h"

using namespace juce;

//==============================================================================
// VirusKnob
//==============================================================================
VirusKnob::VirusKnob (Size s, const String& labelText)
    : Slider (Slider::RotaryVerticalDrag, Slider::NoTextBox), knobSize (s), label (labelText)
{
    setRange (0.0, 127.0, 1.0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setName (labelText);
    setWantsKeyboardFocus (false);
}

String VirusKnob::getTooltip()
{
    return label.toUpperCase() + "  " + textFor (getValue());
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
            g.drawFittedText (textFor (getValue()), labelArea.toNearestInt(), Justification::centred, 1, 0.7f);
        }
        else
        {
            CustomLookAndFeel::drawSmallLabel (g, label, labelArea);
        }
    }

    getLookAndFeel().drawRotarySlider (g, (int) knobArea.getX(), (int) knobArea.getY(),
                                       (int) knobArea.getWidth(), (int) knobArea.getHeight(),
                                       pos, rotary.startAngleRadians, rotary.endAngleRadians, *this);
}

void VirusKnob::setValueWithGesture (double v)
{
    if (onTouched) onTouched();
    if (onGestureBegin) onGestureBegin();
    setValue (jlimit (getMinimum(), getMaximum(), std::round (v)), sendNotificationSync);
    if (onGestureEnd) onGestureEnd();
}

void VirusKnob::mouseDown (const MouseEvent& e)
{
    if (! isEnabled())
        return;
    if (onTouched) onTouched();
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

    // full range in ~200 px (Shift: ~800 px). Values always land on whole steps (1.0).
    const double range = getMaximum() - getMinimum();
    const double perPixel = std::max (range / (e.mods.isShiftDown() ? 800.0 : 200.0), 0.02);
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

void VirusKnob::mouseDoubleClick (const MouseEvent&)
{
    if (isEnabled())
        setValueWithGesture (defaultValue);
}

void VirusKnob::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (! isEnabled())
        return;
    const float dir = (wheel.isReversed ? -1.0f : 1.0f) * (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX);
    if (std::abs (dir) < 1.0e-6f)
        return;
    // one notch = exactly 1.0
    setValueWithGesture (getValue() + (dir > 0.0f ? 1.0 : -1.0));
}

void VirusKnob::mouseEnter (const MouseEvent&) { hovering = true;  repaint(); }
void VirusKnob::mouseExit (const MouseEvent&)  { hovering = false; repaint(); }

//==============================================================================
// Attachments
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

void VirusCombo::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (! isEnabled() || getNumItems() == 0) return;
    const float dir = (wheel.isReversed ? -1.0f : 1.0f) * (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX);
    if (std::abs (dir) < 1.0e-6f) return;
    if (onTouched) onTouched();
    // wheel up = next value (+1), down = previous (-1)
    const int idx = jlimit (0, getNumItems() - 1, getSelectedItemIndex() + (dir > 0.0f ? 1 : -1));
    setSelectedItemIndex (idx, sendNotificationSync);
}

ComboAttachment::ComboAttachment (RangedAudioParameter& p, VirusCombo& b, int minV)
    : box (b), minValue (minV),
      attachment (p, [this] (float v) { box.setSelectedId (roundToInt (v) - minValue + 1, dontSendNotification); })
{
    box.onChange = [this] { const int id = box.getSelectedId(); if (id > 0) attachment.setValueAsCompleteGesture ((float) (id - 1 + minValue)); };
    attachment.sendInitialUpdate();
}

ComboAttachment::~ComboAttachment() { box.onChange = nullptr; }

ToggleAttachment::ToggleAttachment (RangedAudioParameter& p, Button& b)
    : button (b),
      attachment (p, [this] (float v) { button.setToggleState (v > 0.5f, dontSendNotification); })
{
    button.setClickingTogglesState (true);
    button.onClick = [this] { attachment.setValueAsCompleteGesture (button.getToggleState() ? 1.0f : 0.0f); };
    attachment.sendInitialUpdate();
}

ToggleAttachment::~ToggleAttachment() { button.onClick = nullptr; }

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
    if (dimmed != shouldDim) { dimmed = shouldDim; repaint(); }
}

void LcdDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    CustomLookAndFeel::drawLcd (g, b, dimmed);

    const auto textColour = dimmed ? Colour (0xff8a9097) : Colours::white;
    auto textArea = b.reduced (8.0f, 0.0f);
    const auto font = CustomLookAndFeel::uiFont (jmin (14.0f, b.getHeight() * 0.5f), true);
    g.setFont (font);

    const float valueWidth = GlyphArrangement::getStringWidth (font, valueText) + 4.0f;
    auto nameArea = textArea.withTrimmedRight (valueWidth).toNearestInt();

    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawFittedText (nameText, nameArea.translated (0, 1), Justification::centredLeft, 1, 0.75f);
    g.drawText (valueText, textArea.translated (0.0f, 1.0f), Justification::centredRight);
    g.setColour (textColour);
    g.drawFittedText (nameText, nameArea, Justification::centredLeft, 1, 0.75f);
    g.drawText (valueText, textArea, Justification::centredRight);
}

//==============================================================================
void CurvePreview::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    CustomLookAndFeel::drawLcd (g, b, false);
    auto plot = b.reduced (6.0f, 6.0f);

    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 4.0f), plot.getY(), plot.getBottom());

    if (fn)
    {
        Path p;
        const int n = jmax (2, (int) plot.getWidth());
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / (float) n;
            const float y = jlimit (-1.0f, 1.0f, fn (x));
            const float px = plot.getX() + x * plot.getWidth();
            const float py = plot.getCentreY() - y * plot.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (Colour (0x553fa9ff));
        g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white.withAlpha (0.92f));
        g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    if (caption.isNotEmpty())
    {
        g.setFont (CustomLookAndFeel::uiFont (9.5f, true));
        g.setColour (Colours::white.withAlpha (0.75f));
        g.drawText (caption, plot.toNearestInt(), Justification::topRight);
    }
}

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
    g.fillAll (Colours::black.withAlpha (0.72f));
    auto box = getLocalBounds().toFloat().reduced (60.0f, 50.0f);
    CustomLookAndFeel::drawPanel (g, box, "VC-Clone - Quick Help");

    const char* lines[] =
    {
        "MODE: VIRUS / SYLENTH / SPIRE picks the synth. Each has its own oscillators and filters (SYNTH tab) and modulation (MOD).",
        "GLOBAL (under SYNTH/MOD/MATRIX): Filter ADSR + Amp ADSR (Sync = note values at the host tempo; Global = use these",
        "    instead of the patch's own envelopes) and the Global Filter (Virus filter types), shared by all three synths.",
        "FX tab: Saturation (Virus curves), then your Virus Delay and LeadVerb, each with its own full panel and on/off switch.",
        "ARP tab: Off, Arp (MelodyMagic) or Step Seq (ArpSampler). Patches with arp steps open them in the step sequencer.",
        "LOAD / drag a file: Virus .mid/.syx, Sylenth .fxb/.fxp, Spire .spf2. The mode switches to match the patch.",
        "KNOBS: whole steps of 1. Drag (Shift = slower), mouse wheel = +/-1 per notch, double-click = default.",
        "DROPDOWNS: click, or use the mouse wheel to step one entry at a time. Drag the corner to resize (70 - 250 %).",
        "",
        "Click anywhere to close."
    };

    auto text = box.reduced (16.0f, 0.0f).withTrimmedTop (34.0f);
    g.setFont (CustomLookAndFeel::uiFont (12.0f));
    for (auto* line : lines)
    {
        g.setColour (String (line).startsWith ("Click") ? VirusColours::blue : Colour (0xffdfe3e6));
        g.drawFittedText (line, text.removeFromTop (28.0f).toNearestInt(), Justification::centredLeft, 1, 0.8f);
    }
}
