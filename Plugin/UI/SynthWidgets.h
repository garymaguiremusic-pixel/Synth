#pragma once
// Widgets of the compact Synth page: segmented selectors (waveforms, filter types) and
// envelope graphs with draggable points. Drawn in the Virus Delay style, no image assets.

#include <juce_gui_basics/juce_gui_basics.h>
#include "ParamAdapters.h"

//==============================================================================
/** A row of buttons for a short list parameter. Waveform names get a drawn icon.
    Click selects; the scroll wheel steps one entry. */
class SegmentSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SegmentSelector (ParamAdapter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setShown (int v) { if (v != shown) { shown = v; repaint(); } }
    std::function<void()> onTouched;

    /** Draws a small icon for a waveform name; false when the name is not a known shape. */
    static bool drawWaveIcon (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area);

private:
    void choose (int index);
    ParamAdapter& adapter;
    juce::StringArray items;
    int shown = 0;
};

//==============================================================================
/** ADSR graph bound to four controls. Drag the points: attack end (time), decay end
    (time and sustain level), release end (time). */
class EnvelopeGraph : public juce::Component
{
public:
    EnvelopeGraph (ParamAdapter& a, ParamAdapter& d, ParamAdapter& s, ParamAdapter& r);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
    std::function<void (ParamAdapter&)> onTouched;

private:
    struct Geo { juce::Rectangle<float> plot; juce::Point<float> start, peak, sus, rel, end; };
    Geo geometry() const;
    static float norm (const ParamAdapter& p) { const int span = p.maximum() - p.minimum(); return span > 0 ? (float) (p.get() - p.minimum()) / (float) span : 0.0f; }
    static void setNorm (ParamAdapter& p, float v) { p.set (p.minimum() + juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * (float) (p.maximum() - p.minimum()))); }

    ParamAdapter &att, &dec, &sus, &rel;
    int dragPoint = -1;
    float last[4] {};
};
