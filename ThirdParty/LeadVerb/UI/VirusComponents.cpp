#include "VirusComponents.h"
#include "../CustomLookAndFeel.h"
#include "../Parameters.h"

using namespace juce;

//==============================================================================
// VirusKnob
//==============================================================================
VirusKnob::VirusKnob (Size s, const String& labelText)
    : Slider (Slider::RotaryVerticalDrag, Slider::NoTextBox), knobSize (s), label (labelText)
{
    setRange (0.0, 127.0, 0.0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setName (labelText);
}

String VirusKnob::getTooltip()
{
    const auto v = (float) getValue();
    String text = label.toUpperCase() + "  " + lv::formatKnobValue (v);
    if (describeValue)
        text << "   (" << describeValue (v) << ")";
    if (! isEnabled() && inactiveHint.isNotEmpty())
        text << "\n" << inactiveHint;
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

        const bool showValue = (hovering || dragging) && isEnabled();
        if (showValue)
        {
            g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
            g.setColour (VirusColours::blue);
            g.drawText (lv::formatKnobValue ((float) getValue()), labelArea, Justification::centred);
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

double VirusKnob::snapToStep (double v, double step) const
{
    return jlimit (getMinimum(), getMaximum(), std::round (v / step) * step);
}

void VirusKnob::setValueWithGesture (double v)
{
    if (onGestureBegin) onGestureBegin();
    setValue (jlimit (getMinimum(), getMaximum(), v), sendNotificationSync);
    if (onGestureEnd) onGestureEnd();
}

void VirusKnob::mouseDown (const MouseEvent& e)
{
    if (! isEnabled())
        return;

    if (e.mods.isPopupMenu() && onRightClick)
    {
        onRightClick();
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
    if (e.mods.isPopupMenu())
        return;

    if (! isEnabled() || ! dragging)
        return;

    // Incremental drag, so pressing/releasing shift mid-drag never jumps. Normal drags
    // cover the full range in 200 px and move in steps of 1; shift moves in steps of
    // 0.1, one step per 3 px, whatever the knob's range.
    const bool fine = e.mods.isShiftDown();
    const double perPixel = fine ? kFineStep / 3.0 : (getMaximum() - getMinimum()) / 200.0;
    dragValue = jlimit (getMinimum(), getMaximum(), dragValue + (double) (lastDragY - e.position.y) * perPixel);
    lastDragY = e.position.y;

    const double v = snapToStep (dragValue, fine ? kFineStep : kStep);
    if (std::abs (v - getValue()) > 1.0e-9)
        setValue (v, sendNotificationSync);
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

void VirusKnob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (! isEnabled())
        return;

    const float dir = (wheel.isReversed ? -1.0f : 1.0f) * (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX);
    if (std::abs (dir) < 1.0e-6f)
        return;

    // One wheel notch = one step of 1 (0.1 with shift). Trackpads send many small
    // smooth deltas, so those are added up and a step is taken every 0.1.
    int steps = 0;
    if (wheel.isSmooth)
    {
        constexpr float perStep = 0.1f, tolerance = 1.0e-4f;   // tolerance: summed float deltas fall just short
        wheelAccumulator += dir;
        while (wheelAccumulator >= perStep - tolerance)  { ++steps; wheelAccumulator -= perStep; }
        while (wheelAccumulator <= -perStep + tolerance) { --steps; wheelAccumulator += perStep; }
    }
    else
    {
        steps = dir > 0.0f ? 1 : -1;
    }

    if (steps != 0)
    {
        const double step = e.mods.isShiftDown() ? kFineStep : kStep;
        setValueWithGesture (snapToStep (getValue() + steps * step, step));
    }
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
    knob.setRange (range.start, range.end, 0.0);
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
// ReverbVisualizer
//==============================================================================
void ReverbVisualizer::update (const State& s, float dt)
{
    state = s;
    elapsedMs += (double) dt * 1000.0;
    repaint();
}

void ReverbVisualizer::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    CustomLookAndFeel::drawInsetBox (g, b, 4.0f);
    auto area = b.reduced (4.0f, 2.0f);

    const float pd = state.predelayMs;
    const float rtMs = std::max (50.0f, state.decaySeconds * 1000.0f);
    const float tailMs = rtMs * 0.6f;                     // ~-36 dB, the quietest bar drawn
    const float spanMs = (pd + tailMs) * 1.06f;

    // lane labels
    auto labels = area.removeFromLeft (14.0f);
    g.setFont (CustomLookAndFeel::uiFont (9.5f, true));
    g.setColour (VirusColours::labelText);
    g.drawText ("L", labels.withTrimmedBottom (labels.getHeight() * 0.5f), Justification::centred);
    g.drawText ("R", labels.withTrimmedTop (labels.getHeight() * 0.5f), Justification::centred);

    auto info = area.removeFromRight (150.0f);
    auto lanes = area.reduced (2.0f, 1.0f);
    const float laneH = lanes.getHeight() * 0.5f;

    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) (lanes.getY() + laneH), lanes.getX(), lanes.getRight());

    const float playMs = (float) std::fmod (elapsedMs, (double) spanMs);
    auto xFor = [&] (float ms) { return lanes.getX() + lanes.getWidth() * ms / spanMs; };

    // dry hit at t = 0
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillRect (xFor (0.0f), lanes.getY() + 1.0f, 2.0f, lanes.getHeight() - 2.0f);

    // Reflections: a few sparse early ones, then an ever denser tail. The pattern is
    // fixed per mode; Wide sets how different the two sides are.
    Random rng (0x1ead + state.mode * 7919);
    const float spread = jlimit (0.0f, 1.0f, state.width * 0.6f);
    constexpr int numEvents = 44;

    for (int k = 0; k < numEvents; ++k)
    {
        // a few spaced early reflections, then the tail getting denser
        const float early = tailMs * (0.015f + 0.03f * (float) k);
        const float late  = tailMs * (0.14f + 0.86f * std::pow ((float) (k - 4) / (float) (numEvents - 4), 1.25f));
        const float t = pd + (k < 4 ? early : late) + (rng.nextFloat() - 0.5f) * tailMs * 0.012f;
        const float amp = std::pow (10.0f, -3.0f * std::max (0.0f, t - pd) / rtMs) * (0.7f + 0.3f * rng.nextFloat());
        const float r = rng.nextFloat() * 2.0f - 1.0f;
        const float ampL = amp * (1.0f + spread * r) / (1.0f + spread);
        const float ampR = amp * (1.0f - spread * r) / (1.0f + spread);

        const float x = xFor (t);
        const float sincePlay = playMs - t;
        const bool lit = sincePlay >= 0.0f && sincePlay < 90.0f;

        for (int lane = 0; lane < 2; ++lane)
        {
            const float a = lane == 0 ? ampL : ampR;
            if (a < 0.02f || x > lanes.getRight())
                continue;

            const float h = std::max (2.0f, (laneH - 3.0f) * std::sqrt (a));
            const float laneBottom = lanes.getY() + laneH * (float) (lane + 1) - 1.0f;
            auto bar = Rectangle<float> (x - 1.0f, laneBottom - h, 2.0f, h);

            if (lit)
            {
                g.setColour (VirusColours::blue.withAlpha (0.4f));
                g.fillRect (bar.expanded (2.0f, 1.0f));
            }
            g.setColour ((lit ? Colour (0xffc9e6ff) : VirusColours::blue).withAlpha (0.35f + 0.65f * a));
            g.fillRect (bar);
        }
    }

    // playhead
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawVerticalLine ((int) xFor (playMs), lanes.getY(), lanes.getBottom());

    // info text
    const String stereo = state.width < 0.005f ? "MONO" : (state.width > 1.005f ? "WIDE" : "STEREO");
    g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
    g.setColour (Colours::white.withAlpha (0.85f));
    g.drawFittedText (lv::engineModeName (state.mode).toUpperCase() + "  " + stereo,
                      info.withTrimmedLeft (8.0f).withTrimmedBottom (info.getHeight() * 0.5f).toNearestInt(),
                      Justification::centredLeft, 1, 0.8f);
    g.setColour (VirusColours::labelText);
    g.setFont (CustomLookAndFeel::uiFont (10.5f));
    g.drawFittedText (lv::formatMs (pd) + " / " + lv::formatSeconds (state.decaySeconds),
                      info.withTrimmedLeft (8.0f).withTrimmedTop (info.getHeight() * 0.5f).toNearestInt(),
                      Justification::centredLeft, 1, 0.8f);
}

//==============================================================================
// TempoField
//==============================================================================
TempoField::TempoField (RangedAudioParameter& p)
    : param (p),
      attachment (p, [this] (float v) { internalBpm = v; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::UpDownResizeCursor);
}

void TempoField::setTempo (float bpm, bool fromHost)
{
    if (std::abs (bpm - shownBpm) > 0.01f || fromHost != hostTempo)
    {
        shownBpm = bpm;
        hostTempo = fromHost;
        setTooltip (fromHost ? "Tempo is provided by the host"
                             : "Internal tempo (no host tempo). Drag, shift-drag for fine, double-click = 120");
        setMouseCursor (fromHost ? MouseCursor::NormalCursor : MouseCursor::UpDownResizeCursor);
        repaint();
    }
}

void TempoField::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    CustomLookAndFeel::drawInsetBox (g, b);

    auto tag = b.removeFromLeft (38.0f).reduced (3.0f);
    g.setColour (hostTempo ? Colour (0xff2a6fb8) : Colour (0xff4b5056));
    g.fillRoundedRectangle (tag, 2.0f);
    g.setColour (Colours::white);
    g.setFont (CustomLookAndFeel::uiFont (9.5f, true));
    g.drawText (hostTempo ? "HOST" : "INT", tag, Justification::centred);

    g.setFont (CustomLookAndFeel::uiFont (12.0f, true));
    g.drawText (String (hostTempo ? shownBpm : internalBpm, 1) + " BPM", b.reduced (4.0f, 0.0f), Justification::centredRight);
}

void TempoField::mouseDown (const MouseEvent& e)
{
    if (hostTempo)
        return;
    dragging = true;
    lastY = e.position.y;
    attachment.beginGesture();
}

void TempoField::mouseDrag (const MouseEvent& e)
{
    if (! dragging)
        return;
    const float perPixel = e.mods.isShiftDown() ? 0.1f : 0.5f;
    const float delta = (lastY - e.position.y) * perPixel;
    lastY = e.position.y;
    const auto range = param.getNormalisableRange();
    attachment.setValueAsPartOfGesture (jlimit (range.start, range.end, internalBpm + delta));
}

void TempoField::mouseUp (const MouseEvent&)
{
    if (dragging)
        attachment.endGesture();
    dragging = false;
}

void TempoField::mouseDoubleClick (const MouseEvent&)
{
    if (! hostTempo)
        attachment.setValueAsCompleteGesture (lv::kDefaultBpm);
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
    g.fillAll (Colours::black.withAlpha (0.72f));

    auto box = getLocalBounds().toFloat().reduced (16.0f, 20.0f);
    CustomLookAndFeel::drawPanel (g, box, "Lead Verb - Quick Help");

    const char* lines[] =
    {
        "MODE 1 / MODE 2:  choose Spire's reverb (Mode 1, the top panels) or Massive's (Mode 2)",
        "MODE 1 MODE:  Spire's seven reverbs, Plate1 - Aura.  PREDELAY (SYNC = note values 1/64 - 16/1)",
        "DECAY:  Spire's decay; the readout shows the time (right-click to type one).  DAMP:  darker tail",
        "WIDE:  0 = mono, 50 = Spire's natural width, 100 = wider.  COLOR:  Spire's tilt before the reverb",
        "DRY/WET:  Spire's law, the dry stays at full level up to about 85",
        "MODULATION:  vibrato.  HI SHELF:  +12 dB above 4 kHz.  LIFT:  octave-up copy into the reverb only",

        "MODE 2:  Massive's Reverb or Small Reverb.  SIZE:  bigger and longer.  DENSITY:  smoother",
        "COLOR:  darker to brighter tail.  DRY/WET:  equal-power crossfade",
        "GLOBAL (both modes):  SIDECHAIN ducks the reverb (EXT = the sidechain input); DRY SOURCE adds untouched dry",
        "PINGPONG DELAY (both modes):  wide, moving echoes of the reverb, synced, with its own mix",
        "Knobs: drag = steps of 1, shift = 0.1, wheel notch = 1, double-click = default. Drag the corner to resize.",
        "",
        "Click anywhere to close."
    };

    auto text = box.reduced (12.0f, 0.0f).withTrimmedTop (30.0f);
    g.setFont (CustomLookAndFeel::uiFont (11.0f));
    for (auto* line : lines)
    {
        g.setColour (String (line).startsWith ("Click") ? VirusColours::blue : Colour (0xffdfe3e6));
        g.drawFittedText (line, text.removeFromTop (19.0f).toNearestInt(), Justification::centredLeft, 1, 0.8f);
    }
}
