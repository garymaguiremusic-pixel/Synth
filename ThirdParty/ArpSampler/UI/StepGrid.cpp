#include "StepGrid.h"
#include "Shared/CustomLookAndFeel.h"

using namespace juce;
using as::arp::Pattern;

//==============================================================================
StepGrid::StepGrid (as::arp::PatternModel& m, RangedAudioParameter& lengthParam, RangedAudioParameter& endParam)
    : model (m),
      lengthAttachment (lengthParam, [this] (float v) { length = roundToInt (v) == 0 ? 16 : 32; repaint(); }),
      endAttachment (endParam, [this] (float v) { endStep = roundToInt (v); repaint(); })
{
    pattern = model.get();
    model.addChangeListener (this);
    lengthAttachment.sendInitialUpdate();
    endAttachment.sendInitialUpdate();
}

StepGrid::~StepGrid()
{
    model.removeChangeListener (this);
}

void StepGrid::changeListenerCallback (ChangeBroadcaster*)
{
    pattern = model.get();
    repaint();
}

void StepGrid::setCurrentStep (int step)
{
    if (step != currentStep)
    {
        currentStep = step;
        repaint();
    }
}

//==============================================================================
StepGrid::Layout StepGrid::getLayout() const
{
    Layout l;
    auto b = getLocalBounds().toFloat();
    l.labels = b.removeFromLeft (74.0f);
    l.gridX = b.getX();
    l.stepWidth = b.getWidth() / (float) numVisibleSteps();

    l.header = b.removeFromTop (15.0f);
    b.removeFromTop (3.0f);
    l.tie = b.removeFromBottom (18.0f);
    b.removeFromBottom (3.0f);
    l.accent = b.removeFromBottom (18.0f);
    b.removeFromBottom (3.0f);
    l.reverse = b.removeFromBottom (18.0f);
    b.removeFromBottom (3.0f);
    l.ratchet = b.removeFromBottom (18.0f);
    b.removeFromBottom (3.0f);
    l.transpose = b.removeFromBottom (24.0f);
    b.removeFromBottom (3.0f);
    l.velocity = b.removeFromBottom (22.0f);
    b.removeFromBottom (3.0f);
    l.volume = b;
    return l;
}

Rectangle<float> StepGrid::cell (Rectangle<float> lane, int step) const
{
    const auto l = getLayout();
    return { l.gridX + step * l.stepWidth, lane.getY(), l.stepWidth, lane.getHeight() };
}

int StepGrid::stepAt (float x) const
{
    const auto l = getLayout();
    return jlimit (0, numVisibleSteps() - 1, (int) std::floor ((x - l.gridX) / l.stepWidth));
}

StepGrid::Lane StepGrid::laneAt (Point<float> p) const
{
    const auto l = getLayout();
    if (p.x < l.gridX)               return Lane::None;
    if (l.volume.contains (p))       return Lane::Volume;
    if (l.velocity.contains (p))     return Lane::Velocity;
    if (l.transpose.contains (p))    return Lane::Transpose;
    if (l.tie.contains (p))          return Lane::Tie;
    if (l.ratchet.contains (p))      return Lane::Ratchet;
    if (l.reverse.contains (p))      return Lane::Reverse;
    if (l.accent.contains (p))       return Lane::Accent;
    if (l.header.contains (p))       return Lane::Header;
    return Lane::None;
}

int StepGrid::valueFromY (float y, Rectangle<float> laneArea, int minimum)
{
    const auto lane = laneArea.reduced (0.0f, 3.0f);
    return jlimit (minimum, 127, roundToInt ((lane.getBottom() - y) / lane.getHeight() * 127.0f));
}

int StepGrid::applyQuantise (int transpose) const
{
    transpose = jlimit (-24, 24, transpose);
    return quantiseTranspose ? jlimit (-24, 24, quantiseTranspose (transpose)) : transpose;
}

//==============================================================================
void StepGrid::paint (Graphics& g)
{
    const auto l = getLayout();
    const int playLength = as::arp::effectiveLength (length, endStep);

    // lane labels
    CustomLookAndFeel::drawSmallLabel (g, "Volume", l.labels.withY (l.volume.getY()).withHeight (l.volume.getHeight()), Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Velocity", l.labels.withY (l.velocity.getY()).withHeight (l.velocity.getHeight()), Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Transpose", l.labels.withY (l.transpose.getY()).withHeight (l.transpose.getHeight()), Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Ratchet", l.labels.withY (l.ratchet.getY()).withHeight (l.ratchet.getHeight()), Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Reverse", l.labels.withY (l.reverse.getY()).withHeight (l.reverse.getHeight()), Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Accent", l.labels.withY (l.accent.getY()).withHeight (l.accent.getHeight()), Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Tie", l.labels.withY (l.tie.getY()).withHeight (l.tie.getHeight()), Justification::centredLeft);

    // lane backgrounds
    CustomLookAndFeel::drawInsetBox (g, l.volume);
    CustomLookAndFeel::drawInsetBox (g, l.velocity);
    CustomLookAndFeel::drawInsetBox (g, l.transpose);
    CustomLookAndFeel::drawInsetBox (g, l.ratchet);
    CustomLookAndFeel::drawInsetBox (g, l.reverse);
    CustomLookAndFeel::drawInsetBox (g, l.tie);

    // guide lines in the volume lane at 25/50/75 %
    g.setColour (Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = l.volume.getBottom() - l.volume.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine (roundToInt (y), l.volume.getX() + 2.0f, l.volume.getRight() - 2.0f);
    }

    for (int i = 0; i < numVisibleSteps(); ++i)
    {
        const auto& s = pattern.steps[(size_t) i];
        const bool active = i < playLength;
        const bool playing = i == currentStep;
        const float alpha = active ? 1.0f : 0.3f;

        // beat separators every 4 steps
        if (i > 0)
        {
            const float x = l.gridX + i * l.stepWidth;
            g.setColour (Colours::black.withAlpha (i % 4 == 0 ? 0.7f : 0.3f));
            g.drawVerticalLine (roundToInt (x), l.volume.getY() + 2.0f, l.volume.getBottom() - 2.0f);
            g.drawVerticalLine (roundToInt (x), l.velocity.getY() + 2.0f, l.velocity.getBottom() - 2.0f);
        }

        // header: number + LED
        {
            auto h = cell (l.header, i).reduced (1.0f, 0.0f);
            if (playing)
            {
                g.setColour (VirusColours::red.withAlpha (0.9f));
                g.fillRoundedRectangle (h.reduced (2.0f, 2.0f), 2.0f);
            }
            g.setColour ((playing ? Colours::white : (i % 4 == 0 ? VirusColours::headerText : VirusColours::labelText)).withMultipliedAlpha (alpha));
            g.setFont (CustomLookAndFeel::uiFont (10.0f, i % 4 == 0 || playing));
            g.drawText (String (i + 1), h, Justification::centred);
        }

        // volume bar
        {
            auto c = cell (l.volume, i).reduced (l.stepWidth > 30.0f ? 4.0f : 2.0f, 3.0f);
            if (! s.isRest())
            {
                auto bar = c.withTop (c.getBottom() - c.getHeight() * (float) s.volume / 127.0f);
                const auto top = (playing ? Colour (0xff8fd0ff) : VirusColours::blue).withMultipliedAlpha (alpha);
                const auto bottom = (playing ? VirusColours::blue : VirusColours::lcdBottom).withMultipliedAlpha (alpha);
                g.setGradientFill (ColourGradient (top, 0.0f, bar.getY(), bottom, 0.0f, bar.getBottom(), false));
                g.fillRoundedRectangle (bar, 1.5f);
                g.setColour (Colours::white.withAlpha (0.5f * alpha));
                g.drawHorizontalLine (roundToInt (bar.getY()), bar.getX() + 1.0f, bar.getRight() - 1.0f);
            }
            else
            {
                // rest marker
                g.setColour (Colours::white.withAlpha (0.25f * alpha));
                g.fillRect (c.withTop (c.getBottom() - 2.0f).reduced (c.getWidth() * 0.25f, 0.0f));
            }

            if (playing)
            {
                g.setColour (Colours::white.withAlpha (0.06f));
                g.fillRect (cell (l.volume, i).reduced (1.0f, 2.0f));
            }
        }

        // velocity bar (thin, lighter; dimmed on rests)
        {
            auto c = cell (l.velocity, i).reduced (l.stepWidth > 30.0f ? 8.0f : 4.0f, 3.0f);
            auto bar = c.withTop (c.getBottom() - c.getHeight() * (float) jlimit (0, 127, s.velocity) / 127.0f);
            g.setColour (Colour (0xff8fd0ff).withMultipliedAlpha (alpha * (s.isRest() ? 0.25f : (playing ? 1.0f : 0.75f))));
            g.fillRoundedRectangle (bar, 1.0f);
        }

        // transpose value
        {
            auto c = cell (l.transpose, i);
            const String text = s.transpose == 0 ? String ("0") : (s.transpose > 0 ? "+" : "") + String (s.transpose);
            g.setFont (CustomLookAndFeel::uiFont (l.stepWidth > 30.0f ? 12.0f : 10.0f, true));
            g.setColour ((s.transpose == 0 ? VirusColours::labelText.withAlpha (0.45f) : Colours::white).withMultipliedAlpha (alpha));
            g.drawText (text, c, Justification::centred);
            if (i > 0)
            {
                g.setColour (Colours::black.withAlpha (0.5f));
                g.drawVerticalLine (roundToInt (c.getX()), c.getY() + 3.0f, c.getBottom() - 3.0f);
            }
        }

        // ratchet: one tick per hit
        {
            auto c = cell (l.ratchet, i);
            const int hits = jlimit (1, as::arp::kMaxRatchet, s.ratchet);
            const float tickW = jmin (4.0f, (c.getWidth() - 8.0f) / 7.0f);
            const float gap = tickW * 0.8f;
            const float totalW = hits * tickW + (hits - 1) * gap;
            float x = c.getCentreX() - totalW * 0.5f;
            g.setColour (hits > 1 ? (playing ? Colour (0xff8fd0ff) : VirusColours::blue).withMultipliedAlpha (alpha)
                                  : Colours::white.withAlpha (0.15f * alpha));
            for (int h = 0; h < hits; ++h, x += tickW + gap)
                g.fillRoundedRectangle (x, c.getY() + 5.0f, tickW, c.getHeight() - 10.0f, 1.0f);

            if (i > 0)
            {
                g.setColour (Colours::black.withAlpha (0.5f));
                g.drawVerticalLine (roundToInt (c.getX()), c.getY() + 3.0f, c.getBottom() - 3.0f);
            }
        }

        // reverse: a backwards "play" triangle with a bar
        {
            auto c = cell (l.reverse, i);
            const float size = jmin (c.getHeight() - 8.0f, c.getWidth() - 6.0f);
            auto r = c.withSizeKeepingCentre (size, size);
            Path icon;
            icon.addTriangle (r.getRight() - size * 0.15f, r.getY(), r.getX() + size * 0.25f, r.getCentreY(), r.getRight() - size * 0.15f, r.getBottom());
            icon.addRectangle (r.getX(), r.getY(), size * 0.16f, size);

            if (s.reverse)
            {
                g.setColour (VirusColours::blueGlow.withAlpha (0.35f * alpha));
                g.strokePath (icon, PathStrokeType (3.0f));
                g.setColour (VirusColours::blue.withMultipliedAlpha (alpha));
            }
            else
            {
                g.setColour (Colours::white.withAlpha (0.12f * alpha));
            }
            g.fillPath (icon);

            if (i > 0)
            {
                g.setColour (Colours::black.withAlpha (0.5f));
                g.drawVerticalLine (roundToInt (c.getX()), c.getY() + 3.0f, c.getBottom() - 3.0f);
            }
        }

        // accent: a bold ">" mark (lit = accented)
        {
            auto c = cell (l.accent, i);
            const float size = jmin (c.getHeight() - 6.0f, c.getWidth() - 6.0f);
            auto r = c.withSizeKeepingCentre (size * 0.8f, size);
            Path mark;
            mark.startNewSubPath (r.getX(), r.getY());
            mark.lineTo (r.getRight(), r.getCentreY());
            mark.lineTo (r.getX(), r.getBottom());

            if (s.accent)
            {
                g.setColour (VirusColours::red.withAlpha (0.35f * alpha));
                g.strokePath (mark, PathStrokeType (5.0f, PathStrokeType::mitered, PathStrokeType::rounded));
                g.setColour (Colour (0xffff6a4d).withMultipliedAlpha (alpha));
            }
            else
            {
                g.setColour (Colours::white.withAlpha (0.12f * alpha));
            }
            g.strokePath (mark, PathStrokeType (2.2f, PathStrokeType::mitered, PathStrokeType::rounded));

            if (i > 0)
            {
                g.setColour (Colours::black.withAlpha (0.5f));
                g.drawVerticalLine (roundToInt (c.getX()), c.getY() + 3.0f, c.getBottom() - 3.0f);
            }
        }

        // tie arrow
        {
            auto c = cell (l.tie, i);
            const float size = jmin (c.getHeight() - 8.0f, c.getWidth() - 6.0f);
            auto r = c.withSizeKeepingCentre (size, size);
            Path arrow;
            arrow.addTriangle (r.getX() + size * 0.2f, r.getY(), r.getRight(), r.getCentreY(), r.getX() + size * 0.2f, r.getBottom());
            arrow.addRectangle (r.getX() - size * 0.15f, r.getCentreY() - size * 0.12f, size * 0.45f, size * 0.24f);

            if (s.tie)
            {
                g.setColour (VirusColours::blueGlow.withAlpha (0.35f * alpha));
                g.strokePath (arrow, PathStrokeType (3.0f));
                g.setColour (VirusColours::blue.withMultipliedAlpha (alpha));
            }
            else
            {
                g.setColour (Colours::white.withAlpha (0.12f * alpha));
            }
            g.fillPath (arrow);

            if (i > 0)
            {
                g.setColour (Colours::black.withAlpha (0.5f));
                g.drawVerticalLine (roundToInt (c.getX()), c.getY() + 3.0f, c.getBottom() - 3.0f);
            }
        }
    }

    // End marker
    if (endStep > 0 && endStep < numVisibleSteps())
    {
        const float x = l.gridX + endStep * l.stepWidth;
        g.setColour (VirusColours::red.withAlpha (0.8f));
        g.drawLine (x, l.header.getY(), x, l.tie.getBottom(), 2.0f);
    }

    // hover outline
    if (hoverStep >= 0 && hoverLane != Lane::None && hoverLane != Lane::Header)
    {
        const auto lane = hoverLane == Lane::Volume ? l.volume
                        : hoverLane == Lane::Velocity ? l.velocity
                        : hoverLane == Lane::Transpose ? l.transpose
                        : hoverLane == Lane::Ratchet ? l.ratchet
                        : hoverLane == Lane::Reverse ? l.reverse
                        : hoverLane == Lane::Accent ? l.accent : l.tie;
        g.setColour (VirusColours::blue.withAlpha (0.4f));
        g.drawRect (cell (lane, hoverStep).reduced (1.0f), 1.0f);
    }

    // a short message (what was imported)
    if (status.isNotEmpty() && dropHint.isEmpty())
    {
        g.setFont (CustomLookAndFeel::uiFont (12.5f, true));
        const float w = jmin (getWidth() - l.labels.getWidth() - 20.0f, GlyphArrangement::getStringWidth (g.getCurrentFont(), status) + 28.0f);
        auto box = Rectangle<float> (w, 24.0f).withCentre ({ l.gridX + (getWidth() - l.gridX) * 0.5f, l.velocity.getCentreY() });
        g.setColour (VirusColours::background.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (VirusColours::blue);
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (Colours::white);
        g.drawText (status, box, Justification::centred, true);
    }

    // a file is being dragged over: show what dropping it will do
    if (dropHint.isNotEmpty())
    {
        auto area = getLocalBounds().toFloat().withTrimmedLeft (l.labels.getWidth()).reduced (2.0f);
        g.setColour (VirusColours::background.withAlpha (0.82f));
        g.fillRoundedRectangle (area, 6.0f);
        Path outline;
        outline.addRoundedRectangle (area.reduced (1.0f), 6.0f);
        const float dashes[] = { 8.0f, 6.0f };
        PathStrokeType (2.0f).createDashedStroke (outline, outline, dashes, 2);
        g.setColour (VirusColours::blue);
        g.fillPath (outline);

        const bool isMidi = dropHint.endsWithIgnoreCase (".mid") || dropHint.endsWithIgnoreCase (".midi");
        auto text = area.withSizeKeepingCentre (area.getWidth(), 64.0f);
        g.setFont (CustomLookAndFeel::uiFont (17.0f, true));
        g.setColour (Colours::white);
        g.drawText ("Drop to import  " + dropHint, text.removeFromTop (28.0f), Justification::centred, true);
        g.setFont (CustomLookAndFeel::uiFont (12.5f));
        g.setColour (VirusColours::labelText);
        g.drawText (isMidi ? "MIDI notes only (CC, pitch bend and other messages are ignored) - fills the steps from bar 1"
                           : "Detects the notes of a single-note line (bass, acid, lead) and fills the steps",
                    text, Justification::centred, true);
    }
}

bool StepGrid::isInterestedInFileDrag (const StringArray& files)
{
    return files.size() == 1 && canImport && canImport (File (files[0]));
}

void StepGrid::fileDragEnter (const StringArray& files, int, int)
{
    setDropHint (File (files[0]).getFileName());
}

void StepGrid::fileDragExit (const StringArray&)
{
    setDropHint ({});
}

void StepGrid::filesDropped (const StringArray& files, int, int)
{
    setDropHint ({});
    if (onFileDropped && files.size() == 1)
        onFileDropped (File (files[0]));
}

void StepGrid::showStatus (const String& message)
{
    status = message;
    repaint();
    if (message.isNotEmpty())
        startTimer (4000);
}

void StepGrid::timerCallback()
{
    stopTimer();
    status.clear();
    repaint();
}

void StepGrid::setDropHint (const String& fileName)
{
    if (fileName != dropHint)
    {
        dropHint = fileName;
        repaint();
    }
}

//==============================================================================
void StepGrid::mouseMove (const MouseEvent& e)
{
    const auto lane = laneAt (e.position);
    const int step = lane == Lane::None ? -1 : stepAt (e.position.x);
    if (lane != hoverLane || step != hoverStep)
    {
        hoverLane = lane;
        hoverStep = step;
        setMouseCursor (lane == Lane::Transpose ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
        repaint();
    }
}

void StepGrid::mouseDown (const MouseEvent& e)
{
    dragLane = laneAt (e.position);
    if (dragLane == Lane::None || dragLane == Lane::Header)
        return;

    dragStep = stepAt (e.position.x);
    auto& s = pattern.steps[(size_t) dragStep];
    model.pushUndoPoint();

    switch (dragLane)
    {
        case Lane::Volume:
            if (e.mods.isPopupMenu() || e.mods.isAltDown())
            {
                s.volume = s.volume > 0 ? 0 : 127;
                if (s.velocity <= 0)
                    s.velocity = 100;
                dragLane = Lane::None;          // a toggle, not a drag
            }
            else
            {
                s.volume = valueFromY (e.position.y, getLayout().volume, 0);
            }
            break;

        case Lane::Velocity:
            s.velocity = valueFromY (e.position.y, getLayout().velocity, 1);
            break;

        case Lane::Transpose:
            dragStartTranspose = s.transpose;
            dragStartY = e.position.y;
            break;

        case Lane::Tie:
            tiePaintValue = ! s.tie;
            s.tie = tiePaintValue;
            break;

        case Lane::Reverse:
            reversePaintValue = ! s.reverse;
            s.reverse = reversePaintValue;
            break;

        case Lane::Accent:
            accentPaintValue = ! s.accent;
            s.accent = accentPaintValue;
            break;

        case Lane::Ratchet:
        {
            const int n = as::arp::kMaxRatchet;
            const int current = jlimit (1, n, s.ratchet);
            ratchetPaintValue = e.mods.isShiftDown() ? (current == 1 ? n : current - 1) : (current % n) + 1;
            s.ratchet = ratchetPaintValue;
            break;
        }

        case Lane::None:
        case Lane::Header:
            break;
    }

    model.set (pattern, false);
}

void StepGrid::mouseDrag (const MouseEvent& e)
{
    if (dragLane == Lane::None || dragLane == Lane::Header)
        return;

    switch (dragLane)
    {
        case Lane::Volume:
            pattern.steps[(size_t) stepAt (e.position.x)].volume = valueFromY (e.position.y, getLayout().volume, 0);
            break;

        case Lane::Velocity:
            pattern.steps[(size_t) stepAt (e.position.x)].velocity = valueFromY (e.position.y, getLayout().velocity, 1);
            break;

        case Lane::Transpose:
        {
            const float pixelsPerSemitone = e.mods.isShiftDown() ? 14.0f : 6.0f;
            const int delta = roundToInt ((dragStartY - e.position.y) / pixelsPerSemitone);
            pattern.steps[(size_t) dragStep].transpose = applyQuantise (dragStartTranspose + delta);
            break;
        }

        case Lane::Tie:
            pattern.steps[(size_t) stepAt (e.position.x)].tie = tiePaintValue;
            break;

        case Lane::Ratchet:
            pattern.steps[(size_t) stepAt (e.position.x)].ratchet = ratchetPaintValue;
            break;

        case Lane::Reverse:
            pattern.steps[(size_t) stepAt (e.position.x)].reverse = reversePaintValue;
            break;

        case Lane::Accent:
            pattern.steps[(size_t) stepAt (e.position.x)].accent = accentPaintValue;
            break;

        case Lane::None:
        case Lane::Header:
            break;
    }

    model.set (pattern, false);
}

void StepGrid::mouseUp (const MouseEvent&)
{
    dragLane = Lane::None;
    dragStep = -1;
}

void StepGrid::mouseDoubleClick (const MouseEvent& e)
{
    const auto lane = laneAt (e.position);
    const int step = stepAt (e.position.x);

    if (lane == Lane::Transpose)
        pattern.steps[(size_t) step].transpose = applyQuantise (0);
    else if (lane == Lane::Volume)
        pattern.steps[(size_t) step].volume = 127;
    else if (lane == Lane::Velocity)
        pattern.steps[(size_t) step].velocity = 100;
    else if (lane == Lane::Ratchet)
        pattern.steps[(size_t) step].ratchet = 1;
    else
        return;

    model.set (pattern, true);
}

void StepGrid::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    const auto lane = laneAt (e.position);
    if (lane != Lane::Transpose && lane != Lane::Ratchet)
        return;

    const float amount = std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX;
    if (std::abs (amount) < 1.0e-4f)
        return;

    const int dir = (amount > 0.0f) != wheel.isReversed ? 1 : -1;
    auto& s = pattern.steps[(size_t) stepAt (e.position.x)];

    if (lane == Lane::Ratchet)
    {
        s.ratchet = jlimit (1, as::arp::kMaxRatchet, s.ratchet + dir);
        model.set (pattern, true);
        return;
    }
    int next = jlimit (-24, 24, s.transpose + dir);

    // with scale lock, keep moving until the value lands on a new scale note
    while (applyQuantise (next) == s.transpose && next > -24 && next < 24)
        next += dir;

    s.transpose = applyQuantise (next);
    model.set (pattern, true);
}
