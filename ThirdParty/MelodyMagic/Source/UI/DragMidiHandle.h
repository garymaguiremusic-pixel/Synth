#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    "Drag MIDI" grip. Dragging it renders the pattern to a .mid file and starts an
    external (OS-level) file drag, so the clip can be dropped on a DAW track.
    Right-click offers "Save MIDI file..." for hosts that don't accept drags.
*/
class DragMidiHandle : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    DragMidiHandle();

    /** Renders the pattern and returns the written file (empty on failure). */
    std::function<juce::File()> createFile;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override   { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override    { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void showMenu();

    bool hover = false, dragging = false, dragStarted = false;
    std::unique_ptr<juce::FileChooser> chooser;
};
