#pragma once
// The Browser: one column per synth mode (Virus, Sylenth, Spire). Each column keeps the banks the
// user loaded into it (saved in a settings file, so they are there every time the Browser opens),
// lists the selected bank's patches with a search box, and loads a patch on click (the synth
// switches to that mode).

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

class BrowserView : public juce::Component
{
public:
    explicit BrowserView (VCCloneAudioProcessor&);
    ~BrowserView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    /** Call when the processor's bank or program changed. */
    void refresh();
    /** A bank imported elsewhere (Import button, drag and drop) is added to its column too. */
    void addBank (const juce::File&);

private:
    struct Column;
    VCCloneAudioProcessor& processor;
    std::vector<std::unique_ptr<Column>> columns;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::FileChooser> chooser;
    void save();
    static int modeOfFile (const juce::File&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserView)
};
