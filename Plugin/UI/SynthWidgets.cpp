#include "SynthWidgets.h"
#include "../CustomLookAndFeel.h"

using namespace juce;

//==============================================================================
SegmentSelector::SegmentSelector (ParamAdapter& a) : adapter (a), items (a.items())
{
    setTooltip (a.name());
    shown = a.get();
}

bool SegmentSelector::drawWaveIcon (Graphics& g, const String& nameIn, Rectangle<float> r)
{
    const String n = nameIn.toLowerCase().removeCharacters (" _-");
    Path p;
    const float x0 = r.getX(), x1 = r.getRight(), w = r.getWidth(), cy = r.getCentreY(), h = r.getHeight() * 0.5f;
    auto line = [&] (std::initializer_list<Point<float>> pts)
    {
        bool first = true;
        for (auto pt : pts)
        {
            const Point<float> q (x0 + pt.x * w, cy - pt.y * h);
            if (first) p.startNewSubPath (q); else p.lineTo (q);
            first = false;
        }
    };
    if (n == "sine" || n == "sin")
    {
        for (int i = 0; i <= 24; ++i)
        {
            const float t = (float) i / 24.0f;
            const Point<float> q (x0 + t * w, cy - std::sin (t * MathConstants<float>::twoPi) * h);
            if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
    }
    else if (n == "saw" || n == "sawtooth") line ({ { 0, -1 }, { 0.5f, 1 }, { 0.5f, -1 }, { 1, 1 } });
    else if (n == "ramp") line ({ { 0, 1 }, { 0.5f, -1 }, { 0.5f, 1 }, { 1, -1 } });
    else if (n == "triangle" || n == "tri") line ({ { 0, 0 }, { 0.25f, 1 }, { 0.75f, -1 }, { 1, 0 } });
    else if (n == "pulse" || n == "square" || n == "sqr") line ({ { 0, -1 }, { 0, 1 }, { 0.5f, 1 }, { 0.5f, -1 }, { 1, -1 }, { 1, 1 } });
    else if (n == "hpulse") line ({ { 0, -1 }, { 0, 1 }, { 0.25f, 1 }, { 0.25f, -1 }, { 1, -1 }, { 1, 1 } });
    else if (n == "qpulse") line ({ { 0, -1 }, { 0, 1 }, { 0.12f, 1 }, { 0.12f, -1 }, { 1, -1 }, { 1, 1 } });
    else if (n == "trisaw") line ({ { 0, -1 }, { 0.6f, 1 }, { 1, -1 } });
    else if (n == "noise" || n == "random" || n == "s&g")
    {
        Random rng (7);
        for (int i = 0; i <= 14; ++i)
        {
            const Point<float> q (x0 + (float) i / 14.0f * w, cy - (rng.nextFloat() * 2.0f - 1.0f) * h);
            if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
    }
    else if (n == "smphold" || n == "s&h")
        line ({ { 0, 0.3f }, { 0.25f, 0.3f }, { 0.25f, -0.8f }, { 0.5f, -0.8f }, { 0.5f, 0.9f }, { 0.75f, 0.9f }, { 0.75f, -0.2f }, { 1, -0.2f } });
    else
        return false;
    g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
    (void) x1;
    return true;
}

static String shortName (const String& s)
{
    static const std::pair<const char*, const char*> abbrev[] = {
        { "LowPass", "LP" }, { "Lowpass", "LP" }, { "HighPass", "HP" }, { "Highpass", "HP" }, { "BandPass", "BP" }, { "Bandpass", "BP" },
        { "BandStop", "BS" }, { "Bypass", "Off" }, { "Analog 1P", "1P" }, { "Analog 2P", "2P" }, { "Analog 3P", "3P" }, { "Analog 4P", "4P" },
        { "Perfecto", "Perf" }, { "Infecto", "Infc" }, { "Scorpio", "Scrp" }, { "Shaper", "Shpr" }, { "Classic", "Clas" },
        { "AMSync", "AMSy" }, { "SawPWM", "PWM" }, { "HardFM", "HFM" }, { "Vowel", "Vowl" } };
    for (auto& [l, sh] : abbrev) if (s.equalsIgnoreCase (l)) return sh;
    return s;
}

void SegmentSelector::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    CustomLookAndFeel::drawInsetBox (g, b, 3.0f);
    const int n = jmax (1, items.size());
    const float cw = b.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = Rectangle<float> (b.getX() + cw * (float) i, b.getY(), cw, b.getHeight()).reduced (1.5f);
        const bool on = i == shown - adapter.minimum();
        if (on)
        {
            g.setGradientFill (ColourGradient (VirusColours::blue, 0, cell.getY(), VirusColours::blueGlow.darker (0.3f), 0, cell.getBottom(), false));
            g.fillRoundedRectangle (cell, 2.5f);
        }
        else if (i > 0)
        {
            g.setColour (VirusColours::bevelDark.withAlpha (0.7f));
            g.drawVerticalLine ((int) cell.getX() - 1, cell.getY() + 2, cell.getBottom() - 2);
        }
        g.setColour (on ? Colours::white : VirusColours::labelText);
        const auto icon = cell.reduced (jmax (3.0f, cell.getWidth() * 0.18f), cell.getHeight() * 0.28f);
        if (! drawWaveIcon (g, items[i], icon))
        {
            g.setFont (CustomLookAndFeel::uiFont (jmin (11.0f, cell.getHeight() * 0.7f), true));
            g.drawFittedText (shortName (items[i]), cell.toNearestInt(), Justification::centred, 1, 0.8f);
        }
    }
}

void SegmentSelector::choose (int index)
{
    index = jlimit (0, jmax (0, items.size() - 1), index);
    adapter.beginGesture();
    adapter.set (adapter.minimum() + index);
    adapter.endGesture();
    shown = adapter.get();
    repaint();
    if (onTouched) onTouched();
}

void SegmentSelector::mouseDown (const MouseEvent& e)
{
    const int n = jmax (1, items.size());
    choose ((int) ((float) e.x / (float) jmax (1, getWidth()) * (float) n));
}

void SegmentSelector::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    const float d = w.deltaY != 0 ? w.deltaY : -w.deltaX;
    if (d == 0) return;
    choose (shown - adapter.minimum() + (d > 0 ? 1 : -1));
}

//==============================================================================
EnvelopeGraph::EnvelopeGraph (ParamAdapter& a, ParamAdapter& d, ParamAdapter& s, ParamAdapter& r)
    : att (a), dec (d), sus (s), rel (r) {}

EnvelopeGraph::Geo EnvelopeGraph::geometry() const
{
    Geo geo;
    geo.plot = getLocalBounds().toFloat().reduced (8.0f, 8.0f);
    const auto& p = geo.plot;
    const float seg = p.getWidth() * 0.27f;               // longest attack / decay / release
    const float a = 4.0f + norm (att) * seg, d = 4.0f + norm (dec) * seg, r = 4.0f + norm (rel) * seg;
    const float sLevel = p.getBottom() - norm (sus) * p.getHeight();
    geo.start = { p.getX(), p.getBottom() };
    geo.peak = { p.getX() + a, p.getY() };
    geo.sus = { geo.peak.x + d, sLevel };
    geo.rel = { jmax (geo.sus.x + 10.0f, p.getRight() - r - 4.0f), sLevel };
    geo.end = { geo.rel.x + r, p.getBottom() };
    return geo;
}

void EnvelopeGraph::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    CustomLookAndFeel::drawInsetBox (g, b, 4.0f);
    const auto geo = geometry();
    // grid
    g.setColour (Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 6; ++i)
        g.drawVerticalLine ((int) (geo.plot.getX() + geo.plot.getWidth() * (float) i / 6.0f), geo.plot.getY(), geo.plot.getBottom());
    g.drawHorizontalLine ((int) geo.plot.getCentreY(), geo.plot.getX(), geo.plot.getRight());

    Path curve, fill;
    curve.startNewSubPath (geo.start);
    curve.lineTo (geo.peak);
    curve.quadraticTo ({ geo.peak.x + (geo.sus.x - geo.peak.x) * 0.25f, geo.sus.y }, geo.sus);
    curve.lineTo (geo.rel);
    curve.quadraticTo ({ geo.rel.x + (geo.end.x - geo.rel.x) * 0.25f, geo.end.y }, geo.end);
    fill = curve;
    fill.lineTo (geo.start);
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (VirusColours::blue.withAlpha (0.32f), 0, geo.plot.getY(), VirusColours::blue.withAlpha (0.04f), 0, geo.plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (VirusColours::blue);
    g.strokePath (curve, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    for (auto pt : { geo.peak, geo.sus, geo.end })
    {
        g.setColour (VirusColours::bevelDark);
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pt));
        g.setColour (VirusColours::blue.brighter (0.4f));
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (pt));
    }
}

void EnvelopeGraph::mouseDown (const MouseEvent& e)
{
    const auto geo = geometry();
    const Point<float> pts[3] = { geo.peak, geo.sus, geo.end };
    dragPoint = -1;
    float best = 18.0f;
    for (int i = 0; i < 3; ++i)
        if (pts[i].getDistanceFrom (e.position) < best) { best = pts[i].getDistanceFrom (e.position); dragPoint = i; }
    if (dragPoint < 0)   // nearest by x
        dragPoint = e.position.x < geo.peak.x + 4 ? 0 : e.position.x < geo.rel.x ? 1 : 2;
    last[0] = norm (att); last[1] = norm (dec); last[2] = norm (sus); last[3] = norm (rel);
    for (auto* p : { &att, &dec, &sus, &rel }) p->beginGesture();
}

void EnvelopeGraph::mouseDrag (const MouseEvent& e)
{
    const auto geo = geometry();
    const float seg = geo.plot.getWidth() * 0.27f;
    const float dx = (float) e.getDistanceFromDragStartX() / seg, dy = (float) -e.getDistanceFromDragStartY() / geo.plot.getHeight();
    switch (dragPoint)
    {
        case 0: setNorm (att, last[0] + dx); if (onTouched) onTouched (att); break;
        case 1: setNorm (dec, last[1] + dx); setNorm (sus, last[2] + dy); if (onTouched) onTouched (std::abs (dx) > std::abs (dy) ? dec : sus); break;
        case 2: setNorm (rel, last[3] + dx); if (onTouched) onTouched (rel); break;
        default: break;
    }
    repaint();
}

void EnvelopeGraph::mouseUp (const MouseEvent&)
{
    for (auto* p : { &att, &dec, &sus, &rel }) p->endGesture();
    dragPoint = -1;
}

void EnvelopeGraph::refresh() { repaint(); }
