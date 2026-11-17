#include "MidiMonitor.h"
#include "../Shared/CustomLookAndFeel.h"

using namespace juce;

MidiMonitor::MidiMonitor (MelodyMagicProcessor& p) : proc (p)
{
    setTooltip ("MIDI monitor: the chord the arpeggiator is using (left) and the notes MelodyMagic sends out (right)");
    startTimerHz (30);
}

void MidiMonitor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    MonitorEvent buffer[256];
    int n;
    bool changed = false;
    while ((n = proc.readMonitor (buffer, 256)) > 0)
    {
        changed = true;
        for (int i = 0; i < n; ++i)
        {
            const auto& e = buffer[i];
            if (e.on)
            {
                notes.push_back ({ e.note, e.vel, e.channel, now, -1.0 });
                lastText = mmp::noteName (e.note) + "  vel " + String (e.vel) + "  ch " + String (e.channel);
                ++sentCount;
            }
            else
            {
                for (auto it = notes.rbegin(); it != notes.rend(); ++it)
                    if (it->note == e.note && it->channel == e.channel && it->off < 0.0)
                    {
                        it->off = now;
                        break;
                    }
            }
        }
    }

    const auto before = notes.size();
    notes.erase (std::remove_if (notes.begin(), notes.end(), [now] (const Shown& s)
    {
        return (s.off >= 0.0 && now - s.off > kWindowMs) || (s.off < 0.0 && now - s.on > 60000.0);
    }), notes.end());
    if (notes.size() > 400)
        notes.erase (notes.begin(), notes.begin() + (std::ptrdiff_t) (notes.size() - 400));

    const auto held = proc.getHeldChord();
    if (held != chord)
    {
        chord = held;
        changed = true;
    }
    if (changed || before != notes.size() || ! notes.empty())
        repaint();
}

void MidiMonitor::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    CustomLookAndFeel::drawInsetBox (g, b, 3.0f);
    b = b.reduced (6.0f, 3.0f);

    // chord readout
    auto left = b.removeFromLeft (250.0f);
    g.setFont (CustomLookAndFeel::uiFont (10.0f, true));
    g.setColour (VirusColours::labelText);
    g.drawText ("CHORD", left.removeFromTop (left.getHeight() * 0.5f), Justification::centredLeft);
    StringArray names;
    for (int note : chord)
        names.add (mmp::noteName (note));
    g.setFont (CustomLookAndFeel::uiFont (12.0f, true));
    g.setColour (chord.empty() ? VirusColours::labelText.withAlpha (0.5f) : Colours::white);
    const bool autoPlay = proc.apvts.getRawParameterValue (mmp::ParamIDs::autoPlay)->load() > 0.5f;
    const String idle = autoPlay ? "Auto Play: " + mmp::noteName (60 + proc.currentKey()) + " (while the DAW plays)"
                                 : String ("hold some notes, or turn on Auto Play");
    g.drawFittedText (chord.empty() ? idle : names.joinIntoString (" "), left.toNearestInt(), Justification::centredLeft, 1, 0.7f);

    auto right = b.removeFromRight (200.0f);
    g.setFont (CustomLookAndFeel::uiFont (10.0f, true));
    g.setColour (VirusColours::labelText);
    g.drawText ("OUT", right.removeFromTop (right.getHeight() * 0.5f), Justification::centredLeft);
    g.setFont (CustomLookAndFeel::uiFont (11.5f, true));
    g.setColour (lastText.isEmpty() ? VirusColours::labelText.withAlpha (0.5f) : VirusColours::blue);
    g.drawFittedText (lastText.isEmpty() ? String ("-") : lastText, right.toNearestInt(), Justification::centredLeft, 1, 0.7f);

    // scrolling roll
    auto roll = b.reduced (8.0f, 0.0f);
    g.setColour (Colour (0xff101214));
    g.fillRoundedRectangle (roll, 2.0f);
    if (notes.empty())
        return;

    int lo = 127, hi = 0;
    for (auto& s : notes) { lo = jmin (lo, s.note); hi = jmax (hi, s.note); }
    lo -= 2; hi += 2;
    const float noteH = jmax (1.5f, roll.getHeight() / (float) (hi - lo + 1));
    const double now = Time::getMillisecondCounterHiRes();

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (roll.toNearestInt());
    for (auto& s : notes)
    {
        const float x0 = roll.getRight() - (float) ((now - s.on) / kWindowMs) * roll.getWidth();
        const float x1 = roll.getRight() - (float) ((s.off >= 0.0 ? now - s.off : 0.0) / kWindowMs) * roll.getWidth();
        const float y = roll.getBottom() - (float) (s.note - lo + 1) * noteH;
        g.setColour (VirusColours::blue.withAlpha (0.35f + 0.65f * (float) s.vel / 127.0f));
        g.fillRect (Rectangle<float> (x0, y, jmax (2.0f, x1 - x0), jmax (1.5f, noteH - 0.5f)));
    }
}
