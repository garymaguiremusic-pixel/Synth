#include "VirusComponents.h"
#include "../CustomLookAndFeel.h"
#include "../Parameters.h"
#include "../DSP/Lfo.h"

using namespace juce;

//==============================================================================
// VirusKnob
//==============================================================================
VirusKnob::VirusKnob (Size s, const String& labelText)
    : Slider (Slider::RotaryVerticalDrag, Slider::NoTextBox), knobSize (s), label (labelText)
{
    setRange (0.0, 100.0, 0.0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setName (labelText);
}

String VirusKnob::getTooltip()
{
    const auto v = (float) getValue();
    String text = label.toUpperCase() + "  " + vd::formatKnobValue (v);
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
            g.drawFittedText (displayValue ? displayValue (getValue()) : vd::formatKnobValue (getValue()),
                              labelArea.toNearestInt(), Justification::centred, 1, 0.7f);
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
    if (onGestureBegin) onGestureBegin();
    setValue (jlimit (getMinimum(), getMaximum(), v), sendNotificationSync);
    if (onGestureEnd) onGestureEnd();
}

void VirusKnob::mouseDown (const MouseEvent& e)
{
    if (! isEnabled())
        return;

    dragging = true;
    lastDragY = e.position.y;
    dragValue = getValue();
    dragShift = e.mods.isShiftDown();
    if (onGestureBegin) onGestureBegin();
    repaint();
}

void VirusKnob::mouseDrag (const MouseEvent& e)
{
    if (! isEnabled() || ! dragging)
        return;

    // Incremental drag: whole steps normally, 0.1 steps with Shift. Pressing or
    // releasing Shift mid-drag continues from the current value.
    const bool shift = e.mods.isShiftDown();
    if (shift != dragShift)
    {
        dragShift = shift;
        dragValue = getValue();
    }

    const double pixels = (double) (lastDragY - e.position.y);
    lastDragY = e.position.y;

    dragValue += shift ? pixels * 0.05                                    // 0.1 every 2 pixels
                       : pixels * (getMaximum() - getMinimum()) / 200.0;  // full range in 200 pixels
    dragValue = jlimit (getMinimum(), getMaximum(), dragValue);

    const double snapped = shift ? std::round (dragValue * 10.0) / 10.0 : std::round (dragValue);
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

void VirusKnob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (! isEnabled())
        return;

    const float dir = (wheel.isReversed ? -1.0f : 1.0f) * (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX);
    if (std::abs (dir) < 1.0e-6f)
        return;

    // One notch = 1.0, or 0.1 with Shift.
    const bool shift = e.mods.isShiftDown();
    const double step = shift ? 0.1 : 1.0;
    const double target = getValue() + (dir > 0.0f ? step : -step);
    setValueWithGesture (shift ? std::round (target * 10.0) / 10.0 : std::round (target));
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
// LfoPreview
//==============================================================================
void LfoPreview::setState (Kind newKind, int newShape, float newPhase, float depthNorm)
{
    kind = newKind;
    shape = newShape;
    phase = newPhase;
    depth = depthNorm;
    repaint();
}

float LfoPreview::valueAt (double ph) const
{
    const float twoPi = MathConstants<float>::twoPi;
    switch (kind)
    {
        case Kind::TapeWowFlutter:  // slow wow with fast flutter on top (6.5 Hz / 0.6 Hz)
            return 0.78f * std::sin (twoPi * (float) ph) + 0.22f * std::sin (twoPi * 10.8f * (float) ph);
        case Kind::Doppler:
            return std::sin (twoPi * (float) ph);
        case Kind::Lfo:
        default:
            return vd::Lfo::shapeValue (shape, ph);
    }
}

void LfoPreview::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    CustomLookAndFeel::drawLcd (g, b, false);

    auto plot = b.reduced (6.0f, 6.0f);

    // centre line & grid
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 4.0f), plot.getY(), plot.getBottom());

    // waveform: two cycles; scale amplitude a little with depth so it "reads" live
    const float amp = plot.getHeight() * 0.5f * (0.35f + 0.65f * std::sqrt (depth));
    Path wave;
    const int points = (int) plot.getWidth();
    for (int i = 0; i <= points; ++i)
    {
        const double ph = 2.0 * (double) i / (double) points;
        const float v = valueAt (ph - std::floor (ph));
        const float x = plot.getX() + (float) i;
        const float y = plot.getCentreY() - v * amp;
        if (i == 0) wave.startNewSubPath (x, y);
        else        wave.lineTo (x, y);
    }

    g.setColour (Colours::white.withAlpha (0.25f));
    g.strokePath (wave, PathStrokeType (3.5f));
    g.setColour (Colours::white);
    g.strokePath (wave, PathStrokeType (1.4f));

    if (kind == Kind::Doppler)
    {
        // second head moves the opposite way
        g.setColour (Colour (0xffc9e6ff).withAlpha (0.55f));
        g.strokePath (wave, PathStrokeType (1.0f), AffineTransform::verticalFlip (plot.getCentreY() * 2.0f));
    }

    // moving dot at the current phase (first cycle)
    const float px = plot.getX() + phase * 0.5f * plot.getWidth();
    const float py = plot.getCentreY() - valueAt (phase) * amp;
    g.setColour (Colour (0xffc9e6ff).withAlpha (0.35f));
    g.fillEllipse (px - 5.0f, py - 5.0f, 10.0f, 10.0f);
    g.setColour (Colours::white);
    g.fillEllipse (px - 2.5f, py - 2.5f, 5.0f, 5.0f);
}

//==============================================================================
// ModeVisualizer
//==============================================================================
void ModeVisualizer::update (const State& s, float dt)
{
    state = s;
    elapsedMs += (double) dt * 1000.0;

    if (vd::isTapeMode (state.mode))
    {
        // Tape speed is inversely proportional to delay time (fixed head gap).
        const float avgMs = 0.5f * (state.tapeDelayMsL + state.tapeDelayMsR);
        const float revsPerSecond = jlimit (0.08f, 6.0f, 250.0f / std::max (1.0f, avgMs));
        reelAngle = std::fmod (reelAngle + dt * revsPerSecond * MathConstants<float>::twoPi, MathConstants<float>::twoPi);
    }
    repaint();
}

void ModeVisualizer::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    CustomLookAndFeel::drawInsetBox (g, b, 4.0f);

    if (vd::isTapeMode (state.mode))
        paintTape (g, b.reduced (4.0f, 2.0f));
    else if (vd::isSpireMode (state.mode))
        paintSpire (g, b.reduced (4.0f, 2.0f));
    else
        paintPattern (g, b.reduced (4.0f, 2.0f));
}

void ModeVisualizer::paintPattern (Graphics& g, Rectangle<float> area)
{
    RepeatEvent events[8];
    int numEvents = 0;

    const float T = std::max (1.0f, state.baseDelayMs);
    const float fb = state.feedback;

    if (state.classicType == vd::kClassicOff)
    {
        g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
        g.setColour (VirusColours::labelText);
        g.drawText ("CLASSIC  -  DELAY OFF", area, Justification::centred);
        return;
    }

    if (vd::isPingPong (state.classicType) || vd::isPattern (state.classicType))
    {
        float ra, rb;
        vd::getLineRatios (state.classicType, ra, rb);
        const bool pingPong = vd::isPingPong (state.classicType);
        float t = 0.0f;
        for (int k = 0; k < 8; ++k)
        {
            const bool lineA = (k % 2) == 0;
            t += T * (lineA ? ra : rb);
            const float amp = std::pow (fb, (float) (k / 2));
            const float strong = amp, weak = pingPong ? 0.0f : amp * 0.55f;
            events[numEvents++] = { t, lineA ? strong : weak, lineA ? weak : strong };
        }
    }
    else
    {
        for (int k = 1; k <= 8; ++k)
        {
            const float amp = std::pow (fb, (float) (k - 1));
            events[numEvents++] = { T * (float) k, amp, amp };
        }
    }

    drawRepeatLanes (g, area, events, numEvents,
                     String (vd::getClassicType (state.classicType).name).toUpperCase(),
                     "BASE " + vd::formatMs (state.baseDelayMs));
}

void ModeVisualizer::paintSpire (Graphics& g, Rectangle<float> area)
{
    // Trace an impulse through the Spire routing: each entry is one repeat
    // leaving line A (first/left) or line B (second/right).
    struct Hit { float timeMs; int line; float amp; };
    Hit hits[24];
    int numHits = 0;

    const float len[2] = { std::max (1.0f, state.spireDelayMsL), std::max (1.0f, state.spireDelayMsR) };
    const float fb = state.spireFeedback;
    const bool pp = state.spirePingPong;
    const bool cross = pp && state.spireWide < 0.0f;

    // Ping pong: L -> R is unattenuated, R -> L uses the feedback (as in the DSP).
    auto trace = [&] (int line, bool crossing, bool pingPongHops)
    {
        float t = 0.0f, amp = 1.0f;
        for (int k = 0; k < 8 && numHits < 24; ++k)
        {
            t += len[line];
            hits[numHits++] = { t, line, amp };
            amp *= (pingPongHops && line == 0) ? 1.0f : fb;
            if (crossing)
                line = 1 - line;
        }
    };

    if (pp && ! cross)
        trace (0, true, true);                    // ping pong: L -> R -> L ...
    else
    {
        trace (0, cross, false);                  // cross feedback or independent lines
        trace (1, cross, false);
    }

    std::sort (hits, hits + numHits, [] (const Hit& a, const Hit& b) { return a.timeMs < b.timeMs; });

    // Wide: line A goes to L with (1 + w) / 2 and to R with (1 - w) / 2 (ping-pong modes use |w|).
    const float w = pp ? std::abs (state.spireWide) : state.spireWide;
    RepeatEvent events[10];
    const int numEvents = std::min (numHits, 10);
    for (int i = 0; i < numEvents; ++i)
    {
        const float toSame = 0.5f * (1.0f + w), toOther = 0.5f * (1.0f - w);
        const float a = hits[i].line == 0 ? hits[i].amp : 0.0f;
        const float b = hits[i].line == 1 ? hits[i].amp : 0.0f;
        events[i] = { hits[i].timeMs, a * toSame + b * toOther, a * toOther + b * toSame };
    }

    String routing = ! pp ? (std::abs (state.spireWide) < 0.002f ? "MONO" : (state.spireWide < 0.0f ? "SWAPPED" : "STEREO"))
                          : (std::abs (state.spireWide) < 0.002f ? "MONO" : (cross ? "CROSS FEEDBACK" : "PING PONG"));

    drawRepeatLanes (g, area, events, numEvents, "SPIRE  " + routing,
                     vd::formatMs (len[0]) + " / " + vd::formatMs (len[1]));
}

void ModeVisualizer::drawRepeatLanes (Graphics& g, Rectangle<float> area, const RepeatEvent* events, int numEvents,
                                      const String& title, const String& subtitle)
{
    if (numEvents == 0)
        return;

    const float spanMs = events[numEvents - 1].timeMs * 1.06f;

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

    for (int i = 0; i < numEvents; ++i)
    {
        const auto& e = events[i];
        const float x = xFor (e.timeMs);
        const float sincePlay = playMs - e.timeMs;
        const bool lit = sincePlay >= 0.0f && sincePlay < 120.0f;

        for (int lane = 0; lane < 2; ++lane)
        {
            const float amp = lane == 0 ? e.ampL : e.ampR;
            if (amp < 0.02f)
                continue;

            const float h = std::max (2.0f, (laneH - 3.0f) * std::sqrt (amp));
            const float laneBottom = lanes.getY() + laneH * (float) (lane + 1) - 1.0f;
            auto bar = Rectangle<float> (x - 1.5f, laneBottom - h, 3.0f, h);

            if (lit)
            {
                g.setColour (VirusColours::blue.withAlpha (0.4f));
                g.fillRect (bar.expanded (2.0f, 1.0f));
            }
            g.setColour ((lit ? Colour (0xffc9e6ff) : VirusColours::blue).withAlpha (0.35f + 0.65f * amp));
            g.fillRect (bar);
        }
    }

    // playhead
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawVerticalLine ((int) xFor (playMs), lanes.getY(), lanes.getBottom());

    // info text
    g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
    g.setColour (Colours::white.withAlpha (0.85f));
    g.drawFittedText (title, info.withTrimmedLeft (8.0f).withTrimmedBottom (info.getHeight() * 0.5f).toNearestInt(),
                      Justification::centredLeft, 1, 0.8f);
    g.setColour (VirusColours::labelText);
    g.setFont (CustomLookAndFeel::uiFont (10.5f));
    g.drawFittedText (subtitle, info.withTrimmedLeft (8.0f).withTrimmedTop (info.getHeight() * 0.5f).toNearestInt(),
                      Justification::centredLeft, 1, 0.8f);
}

void ModeVisualizer::drawReel (Graphics& g, Point<float> c, float r, float angle)
{
    const auto outer = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xffd9dde1), outer.getX(), outer.getY(),
                                       Colour (0xff70777e), outer.getRight(), outer.getBottom(), false));
    g.fillEllipse (outer);

    // tape pack
    g.setColour (Colour (0xff2b2118));
    g.fillEllipse (outer.reduced (r * 0.22f));

    // hub with three cut-outs that make the rotation visible
    g.setColour (Colour (0xffb8bec4));
    g.fillEllipse (outer.reduced (r * 0.5f));
    g.setColour (Colour (0xff15171a));
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 3.0f;
        const float hx = c.x + std::sin (a) * r * 0.32f, hy = c.y - std::cos (a) * r * 0.32f;
        g.fillEllipse (hx - r * 0.11f, hy - r * 0.11f, r * 0.22f, r * 0.22f);
    }
    g.fillEllipse (c.x - r * 0.08f, c.y - r * 0.08f, r * 0.16f, r * 0.16f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawEllipse (outer, 0.8f);
}

void ModeVisualizer::paintTape (Graphics& g, Rectangle<float> area)
{
    const float r = area.getHeight() * 0.5f - 0.5f;
    const Point<float> left (area.getX() + 18.0f, area.getCentreY());
    const Point<float> right (area.getX() + 82.0f, area.getCentreY());

    // tape path along the bottom, past the heads
    g.setColour (Colour (0xff3b2c1f));
    g.drawLine (left.x, left.y + r * 0.78f, right.x, right.y + r * 0.78f, 1.6f);

    const float headY = area.getBottom() - 6.0f;
    g.setColour (Colour (0xff8e959c));
    g.fillRoundedRectangle (area.getX() + 42.0f, headY - 3.0f, 6.0f, 5.0f, 1.0f);   // record head
    g.fillRoundedRectangle (area.getX() + 54.0f, headY - 3.0f, 6.0f, 5.0f, 1.0f);   // playback head

    drawReel (g, left, r * 0.9f, reelAngle);
    drawReel (g, right, r * 0.9f, reelAngle * 1.15f);

    auto text = area.withTrimmedLeft (110.0f);
    const float speed = state.tapeSpeed;
    const float semis = 12.0f * std::log2 (std::max (0.01f, speed));

    g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
    g.setColour (Colours::white.withAlpha (0.85f));
    g.drawText (vd::getModeNames()[state.mode].toUpperCase(), text.removeFromLeft (100.0f), Justification::centredLeft);

    g.setFont (CustomLookAndFeel::uiFont (11.0f));
    g.setColour (VirusColours::labelText);
    const bool split = std::abs (state.tapeDelayMsL - state.tapeDelayMsR) > 0.5f;
    g.drawText (state.clockText + "   " + (split ? vd::formatMs (state.tapeDelayMsL) + " / " + vd::formatMs (state.tapeDelayMsR)
                                                : vd::formatMs (state.tapeDelayMsL)),
                text.removeFromLeft (170.0f), Justification::centredLeft);

    const bool gliding = std::abs (semis) > 0.05f;
    g.setColour (gliding ? VirusColours::blue : VirusColours::labelText);
    g.drawText ("SPEED x" + String (speed, 2) + (gliding ? (semis > 0 ? "  PITCH UP" : "  PITCH DOWN") : String()),
                text, Justification::centredLeft);
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
    dragBpm = internalBpm;
    dragShift = e.mods.isShiftDown();
    attachment.beginGesture();
}

void TempoField::mouseDrag (const MouseEvent& e)
{
    if (! dragging)
        return;
    // Whole BPM steps normally, 0.1 steps with Shift.
    const bool shift = e.mods.isShiftDown();
    if (shift != dragShift)
    {
        dragShift = shift;
        dragBpm = internalBpm;
    }
    const float pixels = lastY - e.position.y;
    lastY = e.position.y;
    const auto range = param.getNormalisableRange();
    dragBpm = jlimit (range.start, range.end, dragBpm + pixels * (shift ? 0.05f : 0.5f));
    attachment.setValueAsPartOfGesture (shift ? std::round (dragBpm * 10.0f) / 10.0f : std::round (dragBpm));
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
        attachment.setValueAsCompleteGesture (vd::kDefaultBpm);
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
    CustomLookAndFeel::drawPanel (g, box, "Virus Delay - Quick Help");

    const char* lines[] =
    {
        "MODES (tabs):  Simple, Ping Pong (Ratio), Pattern, Tape Clocked, Tape Free, Tape Doppler, Spire",
        "SIMPLE / PING PONG / PATTERN:  TIME (Clock Off) or CLOCK, FEEDBACK, COLOR, LFO Rate / Depth / Shape",
        "TAPE CLOCKED:  separate CLOCK LEFT and CLOCK RIGHT (1/32 - 5/16 of a beat)",
        "TAPE FREE / DOPPLER:  one CLOCK. Doppler swoops harder and moves L/R apart",
        "CENTER FREQ / BANDWIDTH:  band-pass filter inside the tape loop (wide = full range)",
        "SPIRE:  Delay L/R (Sync = note values), Ping-Pong + WIDE routing, Color hi/low cut, Dry/Wet",
        "DRY SOURCE:  adds untouched dry signal back in, in every mode (never ducked or filtered)",
        "WIDTH:  0 = mono repeats, 50 = normal, 100 = extra wide (mono-safe)",
        "DUCK:  repeats move out of the way while the source plays and swell back in the gaps",
        "MOVEMENT:  sweeping high-pass, Phaser and Flanger layers (shared Rate / Depth / Mix)",
        "RANDOMISE:  picks a random mode and settings (Duck, Dry Source, Width and Movement stay)",
        "Knobs: 0 - 100 in steps of 1, Shift = 0.1 steps, double-click = default. Drag the corner to resize.",
        "",
        "Click anywhere to close."
    };

    auto text = box.reduced (12.0f, 0.0f).withTrimmedTop (30.0f);
    g.setFont (CustomLookAndFeel::uiFont (11.0f));
    for (auto* line : lines)
    {
        g.setColour (String (line).startsWith ("Click") ? VirusColours::blue : Colour (0xffdfe3e6));
        g.drawFittedText (line, text.removeFromTop (24.0f).toNearestInt(), Justification::centredLeft, 1, 0.8f);
    }
}
