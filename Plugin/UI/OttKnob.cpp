#include "OttKnob.h"
#include "../CustomLookAndFeel.h"

using namespace juce;

namespace
{
    constexpr float kStart = MathConstants<float>::pi * 1.25f;
    constexpr float kEnd   = MathConstants<float>::pi * 2.75f;
}

OttKnob::OttKnob (ParamAdapter& a, String t) : adapter (a), title (std::move (t))
{
    setRepaintsOnMouseActivity (true);
}

float OttKnob::proportion() const
{
    const int span = adapter.maximum() - adapter.minimum();
    return span > 0 ? (float) (adapter.get() - adapter.minimum()) / (float) span : 0.0f;
}

void OttKnob::refresh()
{
    const int v = adapter.get();
    if (v != shown) { shown = v; repaint(); }
}

String OttKnob::getTooltip()
{
    return adapter.name() + ":  " + adapter.text (adapter.get())
         + "\nDrag (Shift = fine), wheel = 1 step, double-click = default, right-click = type a value.";
}

void OttKnob::setFromUser (int v)
{
    v = jlimit (adapter.minimum(), adapter.maximum(), v);
    adapter.beginGesture();
    adapter.set (v);
    adapter.endGesture();
    if (onTouched) onTouched();
    refresh();
}

void OttKnob::mouseDown (const MouseEvent& e)
{
    if (! isEnabled()) return;
    if (e.mods.isPopupMenu() || e.mods.isCommandDown()) { showEditor(); return; }
    dragging = true;
    dragProportion = proportion();
    lastDragY = e.position.y;
    adapter.beginGesture();
    if (onTouched) onTouched();
    repaint();
}

void OttKnob::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    // incremental, so pressing or releasing shift mid-drag never jumps (OTT)
    const float pixels = e.mods.isShiftDown() ? 1500.0f : 200.0f;
    dragProportion = jlimit (0.0f, 1.0f, dragProportion + (lastDragY - e.position.y) / pixels);
    lastDragY = e.position.y;
    const int v = adapter.minimum() + roundToInt (dragProportion * (float) (adapter.maximum() - adapter.minimum()));
    if (v != adapter.get()) { adapter.set (v); if (onTouched) onTouched(); }
    refresh();
}

void OttKnob::mouseUp (const MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    adapter.endGesture();
    repaint();
}

void OttKnob::mouseDoubleClick (const MouseEvent& e)
{
    if (isEnabled() && ! e.mods.isPopupMenu()) setFromUser (adapter.defaultValue());
}

void OttKnob::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (! isEnabled()) return;
    float delta = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
    if (w.isReversed) delta = -delta;
    if (std::abs (delta) < 1.0e-6f) return;
    int steps = 0;
    if (! w.isSmooth) steps = delta > 0.0f ? 1 : -1;     // one step per notch
    else
    {
        wheelAcc += delta;
        while (wheelAcc >= 0.12f)  { ++steps; wheelAcc -= 0.12f; }
        while (wheelAcc <= -0.12f) { --steps; wheelAcc += 0.12f; }
    }
    if (steps != 0) setFromUser (adapter.get() + steps);
}

void OttKnob::showEditor()
{
    editor = std::make_unique<TextEditor>();
    editor->setFont (CustomLookAndFeel::uiFont (12.0f, true));
    editor->setJustification (Justification::centred);
    editor->setColour (TextEditor::backgroundColourId, Colour (0xff0b0c0e));
    editor->setColour (TextEditor::textColourId, Colours::white);
    editor->setColour (TextEditor::outlineColourId, VirusColours::blue);
    editor->setColour (TextEditor::focusedOutlineColourId, VirusColours::blue);
    editor->setBounds (getLocalBounds().withTrimmedTop (boxH).withHeight (24).reduced (2, 0));
    editor->setText (adapter.text (adapter.get()), false);
    addAndMakeVisible (*editor);
    editor->selectAll();
    editor->grabKeyboardFocus();
    auto close = [safe = SafePointer<OttKnob> (this)] { MessageManager::callAsync ([safe] { if (safe != nullptr) safe->editor.reset(); }); };
    editor->onReturnKey = [this, close]
    {
        const auto text = editor->getText().trim();
        if (text.isNotEmpty())
        {
            // an exact match of the value text (list entries, "1/8"), else the number typed
            int best = INT_MIN;
            for (int v = adapter.minimum(); v <= adapter.maximum() && best == INT_MIN; ++v)
                if (adapter.text (v).equalsIgnoreCase (text)) best = v;
            if (best == INT_MIN)
            {
                const double typed = text.retainCharacters ("-+.0123456789").getDoubleValue();
                double bestDiff = 1.0e30;
                for (int v = adapter.minimum(); v <= adapter.maximum(); ++v)
                {
                    const double shownValue = adapter.text (v).retainCharacters ("-+.0123456789").getDoubleValue();
                    if (std::abs (shownValue - typed) < bestDiff) { bestDiff = std::abs (shownValue - typed); best = v; }
                }
            }
            if (best != INT_MIN) setFromUser (best);
        }
        close();
    };
    editor->onEscapeKey = close;
    editor->onFocusLost = close;
}

void OttKnob::paint (Graphics& g)
{
    const float side = (float) knobSize_;
    const auto knobArea = Rectangle<float> ((float) getWidth() * 0.5f - side * 0.5f, ((float) boxH - side) * 0.5f, side, side);
    const int mn = adapter.minimum(), mx = adapter.maximum();
    const float origin = (mn < 0 && mx > 0) ? (float) -mn / (float) (mx - mn) : 0.0f;
    drawKnob (g, knobArea, proportion(), origin, isEnabled());

    auto text = getLocalBounds().toFloat().withTrimmedTop ((float) boxH + 3.0f);
    g.setFont (CustomLookAndFeel::uiFont (10.0f, true));
    g.setColour (Colour (0xffaab3bb));
    g.drawText (title, text.removeFromTop (13.0f), Justification::centred, false);
    // Gary's FX describe values as "45  (45.0 %)": the part in brackets is the readable one
    auto value = adapter.text (adapter.get());
    if (value.containsChar ('(') && value.endsWithChar (')'))
        value = value.fromLastOccurrenceOf ("(", false, false).dropLastCharacters (1).trim();
    const auto va = text.removeFromTop (12.0f);
    g.setFont (CustomLookAndFeel::uiFont (9.5f, true));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawText (value, va.translated (0.0f, 1.0f), Justification::centred, false);
    g.setColour ((hovering || dragging) ? VirusColours::blue : Colour (0xffdfe3e6));
    g.drawText (value, va, Justification::centred, false);
}

void OttKnob::drawKnob (Graphics& g, Rectangle<float> area, float pos, float origin, bool enabled)
{
    // Gary's OTT knob, scaled from its 64 px proportions
    const float size = std::min (area.getWidth(), area.getHeight());
    const float k = size / 64.0f;
    const auto c = area.getCentre();
    const float R = size * 0.5f - 1.0f * k;
    const float angle = kStart + pos * (kEnd - kStart);
    const float originAngle = kStart + origin * (kEnd - kStart);

    // ticks
    g.setColour (Colour (0xff8d949b).withAlpha (0.8f));
    for (int i = 0; i < 21; ++i)
    {
        const float a = kStart + (kEnd - kStart) * (float) i / 20.0f;
        const float s = std::sin (a), co = -std::cos (a);
        g.drawLine (c.x + s * (R - 3.5f * k), c.y + co * (R - 3.5f * k), c.x + s * R, c.y + co * R, jmax (0.6f, 0.8f * k));
    }

    // recessed track and the glowing value arc
    const float ringR = R - 7.0f * k;
    Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, kStart, kEnd, true);
    g.setColour (Colour (0xff0a0c0e));
    g.strokePath (track, PathStrokeType (5.5f * k, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xff1a2633));
    g.strokePath (track, PathStrokeType (3.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
    if (std::abs (angle - originAngle) > 0.01f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, std::min (originAngle, angle), std::max (originAngle, angle), true);
        const float alpha = enabled ? 1.0f : 0.35f;
        g.setColour (VirusColours::blueGlow.withAlpha (0.18f * alpha));
        g.strokePath (value, PathStrokeType (9.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (VirusColours::blueGlow.withAlpha (0.35f * alpha));
        g.strokePath (value, PathStrokeType (5.5f * k, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (VirusColours::blue.withAlpha (alpha));
        g.strokePath (value, PathStrokeType (3.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xffc9e6ff).withAlpha (0.8f * alpha));
        g.strokePath (value, PathStrokeType (jmax (0.6f, 1.0f * k), PathStrokeType::curved, PathStrokeType::rounded));
    }

    // rubberised cap
    const float bodyR = ringR - 5.5f * k;
    const auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    Path bodyPath;
    bodyPath.addEllipse (body);
    DropShadow (Colours::black.withAlpha (0.8f), jmax (2, roundToInt (6.0f * k)), { 0, jmax (1, roundToInt (3.0f * k)) }).drawForPath (g, bodyPath);
    g.setGradientFill (ColourGradient (Colour (0xff6a6f75), body.getX(), body.getY(), Colour (0xff141618), body.getX(), body.getBottom(), false));
    g.fillEllipse (body);
    const auto cap = body.reduced (bodyR * 0.14f);
    g.setGradientFill (ColourGradient (Colour (0xff4a4f55), cap.getCentreX(), cap.getY(), Colour (0xff1f2225), cap.getCentreX(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.12f), cap.getCentreX(), cap.getY(),
                                       Colours::white.withAlpha (0.0f), cap.getCentreX(), cap.getCentreY(), false));
    g.fillEllipse (cap.reduced (1.0f));
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawEllipse (body, 0.8f);

    // indicator
    const float s = std::sin (angle), co = -std::cos (angle);
    const float p0 = bodyR * 0.35f, p1 = bodyR * 0.92f;
    g.setColour (Colour (0xffe6e9ec).withAlpha (enabled ? 1.0f : 0.5f));
    g.drawLine (c.x + s * p0, c.y + co * p0, c.x + s * p1, c.y + co * p1, jmax (1.2f, 1.8f * k));
}
