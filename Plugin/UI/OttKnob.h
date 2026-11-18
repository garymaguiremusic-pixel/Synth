#pragma once
// The knob from Gary's OTT Multiband plugin, for VC-Clone's controls:
//   look  - 21 ticks, recessed track, layered glowing value arc, rubberised cap with drop shadow,
//           white indicator; the name and the live value under the knob
//   feel  - vertical drag (200 px for the full range, shift = fine: 1500 px), incremental so
//           shift never jumps; wheel = one step per notch; double-click = default;
//           right-click (or ctrl/cmd-click) = type a value
// Drawn proportionally, so it looks the same at every size.

#include <juce_gui_basics/juce_gui_basics.h>
#include "ParamAdapters.h"

class OttKnob : public juce::Component, public juce::TooltipClient
{
public:
    OttKnob (ParamAdapter&, juce::String title);

    /** Knob diameter and the height of the knob's box (the knob is centred in it; text follows). */
    void setGeometry (int knobSize, int boxHeight) { knobSize_ = knobSize; boxH = boxHeight; repaint(); }
    void setTitle (const juce::String& t) { if (t != title) { title = t; repaint(); } }
    /** Refresh from the adapter (automation, patch loads). */
    void refresh();
    std::function<void()> onTouched;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hovering = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hovering = false; repaint(); }
    juce::String getTooltip() override;

    /** The OTT knob drawing, usable on its own (pos and origin 0..1). */
    static void drawKnob (juce::Graphics&, juce::Rectangle<float> area, float pos, float origin, bool enabled);

private:
    float proportion() const;
    void setFromUser (int v);
    void showEditor();

    ParamAdapter& adapter;
    juce::String title;
    int knobSize_ = 34, boxH = 44, shown = INT_MIN;
    bool hovering = false, dragging = false;
    float dragProportion = 0.0f, lastDragY = 0.0f, wheelAcc = 0.0f;
    std::unique_ptr<juce::TextEditor> editor;
};
