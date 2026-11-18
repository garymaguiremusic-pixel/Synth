#include "PanelWidgets.h"
#include "../CustomLookAndFeel.h"
#include <juce_dsp/juce_dsp.h>

using namespace juce;

namespace
{
    void drawScreen (Graphics& g, Rectangle<float> b)
    {
        g.setGradientFill (ColourGradient (Colour (0xff07090b), 0.0f, b.getY(), Colour (0xff0d1013), 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) b.getBottom(), b.getX() + 6.0f, b.getRight() - 6.0f);
    }
    void drawGrid (Graphics& g, Rectangle<float> b, int nx, int ny)
    {
        for (int i = 1; i < nx; ++i)
        {
            g.setColour (VirusColours::blue.withAlpha (0.07f));
            g.drawVerticalLine ((int) (b.getX() + b.getWidth() * (float) i / (float) nx), b.getY() + 1.0f, b.getBottom() - 1.0f);
        }
        for (int j = 1; j < ny; ++j)
        {
            g.setColour (VirusColours::blue.withAlpha (j == ny / 2 ? 0.16f : 0.07f));
            g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) j / (float) ny), b.getX() + 1.0f, b.getRight() - 1.0f);
        }
    }
    void glowPath (Graphics& g, const Path& p, float width)
    {
        g.setColour (VirusColours::blueGlow.withAlpha (0.25f));
        g.strokePath (p, PathStrokeType (width * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (VirusColours::blue);
        g.strokePath (p, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

//==============================================================================
void OscDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawScreen (g, b);
    if (wave)
    {
        auto area = b.reduced (8.0f).withTrimmedTop (26.0f);
        Path p;
        for (int i = 0; i <= 120; ++i)
        {
            const float x = (float) i / 120.0f;
            float ph = x * 2.0f; ph -= std::floor (ph);
            const Point<float> pt (area.getX() + x * area.getWidth(), area.getCentreY() - jlimit (-1.0f, 1.0f, wave (ph)) * area.getHeight() * 0.45f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        glowPath (g, p, 1.6f);
    }
    const ParamAdapter* vals[3] = { oct, semi, fine };
    const String names[3] = { "OCT", "SEMI", fineLabel };
    const float w = b.getWidth() / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        const auto col = Rectangle<float> (b.getX() + w * (float) i, b.getY() + 4.0f, w, 24.0f);
        g.setFont (CustomLookAndFeel::uiFont (8.0f, true));
        g.setColour (Colour (0xff6f7880));
        g.drawText (names[i], col.withHeight (10.0f), Justification::centred);
        g.setFont (CustomLookAndFeel::uiFont (11.5f, true));
        g.setColour (Colour (0xff7cc4ff));
        g.drawText (vals[i] != nullptr ? vals[i]->text (vals[i]->get()) : String ("-"), col.withTrimmedTop (10.0f), Justification::centred);
    }
}

//==============================================================================
StepperButtons::StepperButtons (ParamAdapter& a) : adapter (a)
{
    for (auto* b : { &down, &up })
    {
        addAndMakeVisible (*b);
        b->onClick = [this, b]
        {
            adapter.beginGesture();
            adapter.set (jlimit (adapter.minimum(), adapter.maximum(), adapter.get() + (b == &up ? 1 : -1)));
            adapter.endGesture();
            if (onTouched) onTouched();
        };
    }
    down.setTooltip ("Octave down");
    up.setTooltip ("Octave up");
}

void StepperButtons::resized()
{
    auto r = getLocalBounds();
    down.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
    r.removeFromLeft (4);
    up.setBounds (r);
}

//==============================================================================
EqGraph::EqGraph (std::array<ParamAdapter*, 3> fr, std::array<ParamAdapter*, 3> qs, std::array<ParamAdapter*, 3> gs,
                  std::function<double (int)> freqHz, std::function<double (int)> qValue)
    : f (fr), q (qs), g (gs), toHz (std::move (freqHz)), toQ (std::move (qValue))
{
    setRepaintsOnMouseActivity (true);
}

float EqGraph::xForHz (double hz) const
{
    const auto b = getLocalBounds().toFloat().reduced (4.0f);
    return b.getX() + b.getWidth() * (float) (std::log (hz / 20.0) / std::log (1000.0));
}
double EqGraph::hzForX (float x) const
{
    const auto b = getLocalBounds().toFloat().reduced (4.0f);
    return 20.0 * std::pow (1000.0, jlimit (0.0, 1.0, (double) ((x - b.getX()) / b.getWidth())));
}
float EqGraph::yForDb (double db) const
{
    const auto b = getLocalBounds().toFloat().reduced (4.0f, 10.0f);
    return b.getCentreY() - (float) (db / 18.0) * b.getHeight() * 0.5f;
}

double EqGraph::responseDb (double hz) const
{
    constexpr double sr = 48000.0;
    double mag = 1.0;
    for (int b = 0; b < 3; ++b)
    {
        const float gain = Decibels::decibelsToGain ((float) g[(size_t) b]->get() / 10.0f);
        const float fr = (float) jlimit (20.0, sr * 0.45, toHz (f[(size_t) b]->get()));
        const float qq = (float) toQ (q[(size_t) b]->get());
        auto c = b == 0 ? dsp::IIR::Coefficients<float>::makeLowShelf (sr, fr, qq, gain)
               : b == 1 ? dsp::IIR::Coefficients<float>::makePeakFilter (sr, fr, qq, gain)
                        : dsp::IIR::Coefficients<float>::makeHighShelf (sr, fr, qq, gain);
        mag *= c->getMagnitudeForFrequency (hz, sr);
    }
    return Decibels::gainToDecibels (mag, -60.0);
}

Point<float> EqGraph::nodePos (int b) const
{
    const double hz = toHz (f[(size_t) b]->get());
    return { xForHz (hz), yForDb (g[(size_t) b]->get() / 10.0) };
}

void EqGraph::paint (Graphics& gr)
{
    auto b = getLocalBounds().toFloat();
    drawScreen (gr, b);
    drawGrid (gr, b, 8, 4);
    gr.setFont (CustomLookAndFeel::uiFont (7.5f, true));
    gr.setColour (Colour (0xff6f7880));
    for (auto [hz, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
        gr.drawText (t, Rectangle<float> (xForHz (hz) - 12.0f, b.getBottom() - 12.0f, 24.0f, 10.0f), Justification::centred);

    const bool on = enabled ? enabled() : true;
    Path curve, fill;
    const auto inner = b.reduced (4.0f);
    for (int i = 0; i <= 160; ++i)
    {
        const float x = inner.getX() + inner.getWidth() * (float) i / 160.0f;
        const float y = jlimit (b.getY() + 2.0f, b.getBottom() - 2.0f, yForDb (responseDb (hzForX (x))));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, yForDb (0.0)); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (inner.getRight(), yForDb (0.0));
    fill.closeSubPath();
    gr.setColour (VirusColours::blue.withAlpha (on ? 0.12f : 0.04f));
    gr.fillPath (fill);
    if (on) glowPath (gr, curve, 2.0f);
    else { gr.setColour (Colour (0xff3a4048)); gr.strokePath (curve, PathStrokeType (1.6f)); }
    for (int i = 0; i < 3; ++i)
    {
        const auto p = nodePos (i);
        const float r = i == selected ? 5.0f : 4.0f;
        gr.setColour (i == selected ? Colour (0xffeaf5ff) : Colour (0xff0d1013));
        gr.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        gr.setColour (on ? VirusColours::blue : Colour (0xff5d666e));
        gr.drawEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.6f);
    }
}

void EqGraph::mouseDown (const MouseEvent& e)
{
    dragging = -1;
    float best = 14.0f;
    for (int i = 0; i < 3; ++i)
        if (const float d = nodePos (i).getDistanceFrom (e.position); d < best) { best = d; dragging = i; }
    if (dragging < 0) return;
    selected = dragging;
    if (onSelect) onSelect (selected);
    f[(size_t) dragging]->beginGesture();
    g[(size_t) dragging]->beginGesture();
    repaint();
}

void EqGraph::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    auto* fa = f[(size_t) dragging];
    auto* ga = g[(size_t) dragging];
    const double hz = hzForX (e.position.x);
    int best = fa->minimum();
    double bestDiff = 1.0e30;
    for (int v = fa->minimum(); v <= fa->maximum(); ++v)
        if (const double d = std::abs (std::log (toHz (v) / hz)); d < bestDiff) { bestDiff = d; best = v; }
    fa->set (best);
    const auto b = getLocalBounds().toFloat().reduced (4.0f, 10.0f);
    const double db = (b.getCentreY() - e.position.y) / (b.getHeight() * 0.5f) * 18.0;
    ga->set (jlimit (ga->minimum(), ga->maximum(), roundToInt (db * 10.0)));
    repaint();
}

void EqGraph::mouseUp (const MouseEvent&)
{
    if (dragging < 0) return;
    f[(size_t) dragging]->endGesture();
    g[(size_t) dragging]->endGesture();
    dragging = -1;
}
