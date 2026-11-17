#include "PatternGrid.h"
#include "../Shared/CustomLookAndFeel.h"

using namespace juce;

PatternGrid::PatternGrid (MelodyMagicProcessor& p) : proc (p)
{
    setTooltip ("Click: add a note (locked to the Key + Scale).  Drag: move.  Drag the right edge: length.  Alt: no snap, any semitone.\n"
                "Mouse wheel over a note: up / down to the next scale note (Alt: a semitone).\n"
                "Right-click or double-click: delete.  Drop a .mid file here to import it.\n"
                "Keys: Up / Down move the selected note, Left / Right select, Delete removes it.");
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    computeRange();
    startTimerHz (30);
}

//==============================================================================
void PatternGrid::computeRange()
{
    const auto& pt = proc.getPattern();
    int lo = -12, hi = 12;
    for (auto& n : pt.notes)
    {
        lo = jmin (lo, n.offset - 2);
        hi = jmax (hi, n.offset + 2);
    }
    bottom = jmax (-mm::kMaxOffset, lo);
    top = jmin (mm::kMaxOffset, hi);
    length = pt.lengthTicks();
}

void PatternGrid::modelChanged()
{
    if (drag == Drag::None)   // keep the rows still while a note is being dragged
        computeRange();
    else
        length = proc.getPattern().lengthTicks();
    if (selected >= (int) proc.getPattern().notes.size())
        selected = -1;
    repaint();
}

float PatternGrid::rowHeight() const noexcept      { return gridArea.getHeight() / (float) jmax (1, numRows()); }
float PatternGrid::xForTick (double tick) const noexcept
{
    return gridArea.getX() + (float) (tick / jmax (1, length)) * gridArea.getWidth();
}
double PatternGrid::tickForX (float x) const noexcept
{
    return (double) (x - gridArea.getX()) / jmax (1.0f, gridArea.getWidth()) * length;
}
int PatternGrid::snapToKey (int offset) const
{
    // the nearest note of the Key + Scale (ties go down)
    const auto scale = proc.currentScale();
    const int root = 60 + proc.currentKey();
    return jlimit (-mm::kMaxOffset, mm::kMaxOffset, scale.quantise (root + offset) - root);
}

int PatternGrid::offsetForY (float y) const noexcept
{
    return jlimit (bottom, top, top - (int) ((y - gridArea.getY()) / rowHeight()));
}

Rectangle<float> PatternGrid::noteRect (const mm::PatternNote& n) const
{
    const float rh = rowHeight();
    const float x0 = xForTick (n.start), x1 = jmax (x0 + 3.0f, xForTick (jmin (n.end, length)));
    return { x0, gridArea.getY() + rh * (float) (top - n.offset) + 0.5f, x1 - x0, jmax (2.0f, rh - 1.0f) };
}

int PatternGrid::noteAt (Point<float> pos) const
{
    const auto& notes = proc.getPattern().notes;
    for (int i = (int) notes.size() - 1; i >= 0; --i)
        if (noteRect (notes[(size_t) i]).expanded (1.0f, 1.0f).contains (pos))
            return i;
    return -1;
}

int PatternGrid::velocityNoteAt (float x) const
{
    const auto& notes = proc.getPattern().notes;
    int best = -1;
    float bestDist = 6.0f;
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const float d = std::abs (xForTick (notes[(size_t) i].start) + 2.0f - x);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

//==============================================================================
void PatternGrid::resized()
{
    auto b = getLocalBounds().toFloat();
    velArea = b.removeFromBottom (40.0f);
    b.removeFromBottom (4.0f);
    rulerArea = b.removeFromTop (14.0f);
    labelArea = b.removeFromLeft (40.0f);
    gridArea = b;
    rulerArea = rulerArea.withTrimmedLeft (40.0f);
    velArea = velArea.withTrimmedLeft (40.0f);
}

void PatternGrid::paint (Graphics& g)
{
    const auto& pt = proc.getPattern();
    const auto scale = proc.currentScale();
    const int key = proc.currentKey();

    CustomLookAndFeel::drawInsetBox (g, gridArea.expanded (2.0f), 3.0f);
    CustomLookAndFeel::drawInsetBox (g, velArea.expanded (2.0f), 3.0f);

    // rows: in-scale notes lighter, the key's root highlighted
    const float rh = rowHeight();
    for (int o = top; o >= bottom; --o)
    {
        const auto r = Rectangle<float> (gridArea.getX(), gridArea.getY() + rh * (float) (top - o), gridArea.getWidth(), rh);
        const bool root = mm::floorMod (o, 12) == 0;
        const bool inScale = scale.inScale (key + o);
        g.setColour (root ? Colour (0xff1f2d3b) : (inScale ? Colour (0xff1d2024) : Colour (0xff131517)));
        g.fillRect (r);
        g.setColour (Colours::black.withAlpha (root ? 0.0f : 0.45f));
        g.drawHorizontalLine ((int) r.getBottom(), r.getX(), r.getRight());

        if (rh >= 7.0f || root)
        {
            String label (mm::keyName (key + o));
            if (root && o != 0)
                label << (o > 0 ? " +" : " ") << String (o / 12);
            g.setFont (CustomLookAndFeel::uiFont (jlimit (7.0f, 10.0f, rh * 0.9f), root));
            g.setColour (root ? VirusColours::blue : (inScale ? VirusColours::labelText : VirusColours::labelText.withAlpha (0.4f)));
            g.drawText (label, Rectangle<float> (labelArea.getX(), r.getY() - 2.0f, labelArea.getWidth() - 4.0f, rh + 4.0f), Justification::centredRight);
        }
        if (root)
        {
            g.setColour (Colour (0xff3a4a5a));
            g.drawHorizontalLine ((int) r.getBottom(), r.getX(), r.getRight());
        }
    }

    // time grid
    for (int t = 0; t <= length; t += snap)
    {
        const float x = xForTick (t);
        const bool bar = t % mm::kTicksPerBar == 0, beat = t % mm::kPhrasePpq == 0;
        g.setColour (bar ? Colour (0xff59636d) : (beat ? Colour (0xff353c43) : Colour (0xff24292e)));
        g.drawVerticalLine ((int) x, gridArea.getY(), gridArea.getBottom());
        g.setColour (bar ? Colour (0xff59636d).withAlpha (0.8f) : Colour (0xff2b3035));
        g.drawVerticalLine ((int) x, velArea.getY(), velArea.getBottom());
        if (bar && t < length)
        {
            g.setFont (CustomLookAndFeel::uiFont (10.0f, true));
            g.setColour (VirusColours::labelText);
            g.drawText (String (t / mm::kTicksPerBar + 1), Rectangle<float> (x + 3.0f, rulerArea.getY(), 30.0f, rulerArea.getHeight()), Justification::centredLeft);
        }
    }

    // notes
    for (int i = 0; i < (int) pt.notes.size(); ++i)
    {
        const auto& n = pt.notes[(size_t) i];
        const auto r = noteRect (n);
        const bool sel = i == selected;
        const float v = (float) n.velocity / 127.0f;
        const auto c1 = VirusColours::blue.withMultipliedBrightness (0.55f + 0.45f * v);
        const auto c2 = VirusColours::blueGlow.withMultipliedBrightness (0.45f + 0.4f * v);
        g.setGradientFill (ColourGradient (sel ? c1.brighter (0.35f) : c1, 0.0f, r.getY(), sel ? c2.brighter (0.35f) : c2, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (sel ? Colours::white : Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 2.0f, sel ? 1.4f : 1.0f);

        const float x = xForTick (n.start);
        const float h = (velArea.getHeight() - 4.0f) * v;
        g.setColour (sel ? Colours::white : VirusColours::blue);
        g.fillRect (Rectangle<float> (x + 1.0f, velArea.getBottom() - 2.0f - h, 3.0f, h));
    }

    if (playhead >= 0.0)
    {
        const float x = xForTick (std::fmod (playhead, (double) jmax (1, length)));
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawVerticalLine ((int) x, gridArea.getY(), gridArea.getBottom());
        g.setColour (VirusColours::blue.withAlpha (0.25f));
        g.fillRect (Rectangle<float> (x - 2.0f, gridArea.getY(), 4.0f, gridArea.getHeight()));
    }

    if (pt.notes.empty())
    {
        g.setFont (CustomLookAndFeel::uiFont (12.0f, true));
        g.setColour (VirusColours::labelText.withAlpha (0.7f));
        g.drawText ("Click to add notes, press New Pattern, pick one in the Library, or drop a MIDI file here",
                    gridArea, Justification::centred);
    }
    if (hasKeyboardFocus (false))
    {
        g.setColour (VirusColours::blue.withAlpha (0.7f));
        g.drawRoundedRectangle (gridArea.expanded (2.0f), 3.0f, 1.5f);
    }
    if (dropHighlight)
    {
        g.setColour (VirusColours::blue.withAlpha (0.25f));
        g.fillRect (gridArea);
        g.setColour (VirusColours::blue);
        g.drawRect (gridArea, 2.0f);
        g.setFont (CustomLookAndFeel::uiFont (14.0f, true));
        g.setColour (Colours::white);
        g.drawText ("Drop to import the MIDI file", gridArea, Justification::centred);
    }
}

void PatternGrid::timerCallback()
{
    const double pos = proc.playPosition.load();
    if (std::abs (pos - playhead) > 0.01)
    {
        playhead = pos;
        repaint();
    }
}

//==============================================================================
void PatternGrid::ensureEditStarted()
{
    if (! editStarted)
    {
        proc.beginEdit();
        editStarted = true;
    }
}

void PatternGrid::commit (mm::Pattern& p)
{
    proc.updatePattern (p);
    repaint();
}

int PatternGrid::selectAfterSort (mm::Pattern& p, const mm::PatternNote& n)
{
    p.sort();
    for (int k = 0; k < (int) p.notes.size(); ++k)
        if (p.notes[(size_t) k] == n)
            return k;
    return -1;
}

void PatternGrid::mouseMove (const MouseEvent& e)
{
    const int i = noteAt (e.position);
    if (i >= 0 && std::abs (e.position.x - noteRect (proc.getPattern().notes[(size_t) i]).getRight()) < 5.0f)
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
    else if (i >= 0)
        setMouseCursor (MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (MouseCursor::NormalCursor);
}

void PatternGrid::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    editStarted = false;
    drag = Drag::None;
    auto p = proc.getPattern();

    if (velArea.contains (e.position))
    {
        const int i = velocityNoteAt (e.position.x);
        if (i >= 0)
        {
            drag = Drag::Velocity;
            selected = i;
            setVelocityFromY (i, e.position.y);
            if (onSelectionChanged) onSelectionChanged();
        }
        return;
    }
    if (! gridArea.contains (e.position))
        return;

    const int i = noteAt (e.position);
    if (e.mods.isPopupMenu())
    {
        if (i >= 0)
        {
            ensureEditStarted();
            p.notes.erase (p.notes.begin() + i);
            selected = -1;
            commit (p);
            if (onSelectionChanged) onSelectionChanged();
        }
        return;
    }

    if (i >= 0)
    {
        selected = i;
        dragOriginal = p.notes[(size_t) i];
        dragStartTick = tickForX (e.position.x);
        dragStartOffset = offsetForY (e.position.y);
        drag = std::abs (e.position.x - noteRect (dragOriginal).getRight()) < 5.0f ? Drag::Resize : Drag::Move;
    }
    else
    {
        ensureEditStarted();
        const double t = tickForX (e.position.x);
        mm::PatternNote n;
        n.start = jlimit (0, jmax (0, length - 1), e.mods.isAltDown() ? (int) t : (int) (t / snap) * snap);
        n.end = jmax (n.start + 1, jmin (length, n.start + snap));
        n.offset = e.mods.isAltDown() ? offsetForY (e.position.y) : snapToKey (offsetForY (e.position.y));
        n.velocity = selected >= 0 && selected < (int) p.notes.size() ? p.notes[(size_t) selected].velocity : 100;
        p.notes.push_back (n);
        selected = selectAfterSort (p, n);
        dragOriginal = n;
        dragStartTick = t;
        drag = Drag::Resize;
        commit (p);
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void PatternGrid::mouseDrag (const MouseEvent& e)
{
    if (drag == Drag::Velocity)
    {
        const int i = velocityNoteAt (e.position.x);
        if (i >= 0)
            setVelocityFromY (i, e.position.y);
        return;
    }
    if (selected < 0 || (drag != Drag::Move && drag != Drag::Resize))
        return;

    auto p = proc.getPattern();
    if (selected >= (int) p.notes.size())
        return;
    auto n = dragOriginal;
    const bool fine = e.mods.isAltDown();
    const double t = tickForX (e.position.x);

    if (drag == Drag::Move)
    {
        double delta = t - dragStartTick;
        delta = fine ? std::round (delta) : std::round (delta / snap) * snap;
        const int len = dragOriginal.end - dragOriginal.start;
        n.start = jlimit (0, jmax (0, length - len), dragOriginal.start + (int) delta);
        n.end = n.start + len;
        n.offset = jlimit (-mm::kMaxOffset, mm::kMaxOffset, dragOriginal.offset + offsetForY (e.position.y) - dragStartOffset);
        if (! fine)
            n.offset = snapToKey (n.offset);
    }
    else
    {
        const int end = fine ? (int) std::round (t) : (int) std::ceil (t / snap - 1.0e-6) * snap;
        n.end = jlimit (n.start + 1, length, end);
    }

    if (n == p.notes[(size_t) selected])
        return;
    ensureEditStarted();
    p.notes[(size_t) selected] = n;
    commit (p);
}

void PatternGrid::mouseUp (const MouseEvent&)
{
    if ((drag == Drag::Move || drag == Drag::Resize) && editStarted)
    {
        auto p = proc.getPattern();
        if (selected >= 0 && selected < (int) p.notes.size())
        {
            const auto n = p.notes[(size_t) selected];
            selected = selectAfterSort (p, n);
            commit (p);
        }
    }
    drag = Drag::None;
    editStarted = false;
    computeRange();
    repaint();
}

void PatternGrid::mouseDoubleClick (const MouseEvent& e)
{
    const int i = noteAt (e.position);
    if (i < 0 || e.mods.isPopupMenu())
        return;
    auto p = proc.getPattern();
    proc.beginEdit();
    p.notes.erase (p.notes.begin() + i);
    selected = -1;
    commit (p);
    if (onSelectionChanged) onSelectionChanged();
}

void PatternGrid::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    const int i = noteAt (e.position);
    if (i < 0)
        return;
    const float dir = (wheel.isReversed ? -1.0f : 1.0f) * (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX);
    if (std::abs (dir) < 1.0e-6f)
        return;
    selected = i;
    moveSelected (dir > 0.0f ? 1 : -1, e.mods.isAltDown());
}

void PatternGrid::moveSelected (int direction, bool chromatic)
{
    auto p = proc.getPattern();
    if (selected < 0 || selected >= (int) p.notes.size())
        return;
    const auto scale = proc.currentScale();
    auto n = p.notes[(size_t) selected];
    n.offset = mm::stepOffset (n.offset, direction, chromatic ? nullptr : &scale, proc.currentKey());
    if (n == p.notes[(size_t) selected])
        return;
    proc.beginEdit();
    p.notes[(size_t) selected] = n;
    selected = selectAfterSort (p, n);
    commit (p);
    computeRange();
    if (onSelectionChanged) onSelectionChanged();
}

bool PatternGrid::keyPressed (const KeyPress& key)
{
    const int count = (int) proc.getPattern().notes.size();
    const auto code = key.getKeyCode();
    const bool cmd = key.getModifiers().isCommandDown();

    if (cmd && code == 'Z')
    {
        key.getModifiers().isShiftDown() ? proc.redo() : proc.undo();
        return true;
    }
    if (cmd && code == 'Y')
    {
        proc.redo();
        return true;
    }
    if (code == KeyPress::upKey || code == KeyPress::downKey)
    {
        moveSelected (code == KeyPress::upKey ? 1 : -1, key.getModifiers().isAltDown());
        return true;
    }
    if (code == KeyPress::leftKey || code == KeyPress::rightKey)
    {
        if (count > 0)
        {
            selected = selected < 0 ? 0 : jlimit (0, count - 1, selected + (code == KeyPress::rightKey ? 1 : -1));
            repaint();
            if (onSelectionChanged) onSelectionChanged();
        }
        return true;
    }
    if ((code == KeyPress::deleteKey || code == KeyPress::backspaceKey) && selected >= 0 && selected < count)
    {
        auto p = proc.getPattern();
        proc.beginEdit();
        p.notes.erase (p.notes.begin() + selected);
        selected = jmin (selected, (int) p.notes.size() - 1);
        commit (p);
        if (onSelectionChanged) onSelectionChanged();
        return true;
    }
    return false;
}

void PatternGrid::setVelocityFromY (int index, float y)
{
    auto p = proc.getPattern();
    if (index < 0 || index >= (int) p.notes.size())
        return;
    const int vel = jlimit (1, 127, roundToInt ((velArea.getBottom() - 2.0f - y) / (velArea.getHeight() - 4.0f) * 127.0f));
    if (p.notes[(size_t) index].velocity == vel)
        return;
    ensureEditStarted();
    p.notes[(size_t) index].velocity = vel;
    proc.updatePattern (p);
    repaint();
    if (onSelectionChanged) onSelectionChanged();
}

//==============================================================================
bool PatternGrid::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void PatternGrid::filesDropped (const StringArray& files, int, int)
{
    dropHighlight = false;
    repaint();
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
        {
            if (onFileDropped)
                onFileDropped (File (f));
            return;
        }
}
