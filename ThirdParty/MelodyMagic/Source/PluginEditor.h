#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Shared/CustomLookAndFeel.h"
#include "Shared/VirusComponents.h"
#include "UI/DragMidiHandle.h"
#include "UI/LibraryPanel.h"
#include "UI/MidiMonitor.h"
#include "UI/PatternGrid.h"

//==============================================================================
/** The whole user interface, laid out at its base size (1000 x 680). The editor
    below scales it to the window size. */
class MelodyMagicPanel : public juce::Component,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    static constexpr int kBaseWidth = 1000, kBaseHeight = 680;

    explicit MelodyMagicPanel (MelodyMagicProcessor&);
    ~MelodyMagicPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void setupCombo (juce::ComboBox&, const juce::StringArray& items, const juce::String& tooltip);
    void attachCombo (juce::ComboBox&, const char* paramId);
    void attachToggle (juce::TextButton&, const char* paramId, const juce::String& tooltip);
    void attachKnob (VirusKnob&, const char* paramId, std::function<juce::String (double)> describe, const juce::String& tooltip);
    void updateFromModel();
    void updateSelectionInfo();
    void updateCustomKeys();
    void importMidi (const juce::File&);
    void rebuildGrooveMenu();
    void keepFocusOnNotes (juce::Component&);
    void createBackground();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float> area);

    MelodyMagicProcessor& processor;
    CustomLookAndFeel lnf;
    juce::Image background;

    // Top bar
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, bypassButton { "Bypass" };
    DragMidiHandle dragHandle;

    // Pattern
    juce::ComboBox lengthBox, snapBox;
    LcdDisplay selectionLcd;
    juce::TextButton clearButton { "Clear" };
    PatternGrid grid;
    LibraryPanel libraryPanel;

    // Arp
    juce::ComboBox modeBox, speedBox;
    VirusKnob randomVelKnob { VirusKnob::Size::Small, "Rnd Vel" };
    juce::TextButton autoPlayButton { "Auto Play" };

    // Scale lock
    juce::ComboBox keyBox, scaleBox;
    juce::TextButton scaleLockButton { "Scale Lock" };
    std::array<juce::TextButton, 12> customKeys;

    // Melody Maker
    juce::ComboBox progressionBox;
    LcdDisplay chordsLcd;
    VirusKnob densityKnob { VirusKnob::Size::Small, "Density" };
    VirusKnob rangeKnob   { VirusKnob::Size::Small, "Range" };
    VirusKnob gateKnob    { VirusKnob::Size::Small, "Gate" };
    juce::TextButton newMelodyButton { "New Melody" }, newPatternButton { "New Pattern" }, fileStyleButton { "Learn From File" },
                     topOnlyButton { "Top Notes Only" };

    // Groove
    juce::ComboBox grooveBox;
    std::vector<juce::String> grooveKeys;
    std::shared_ptr<const mm::Library> grooveLibrary;
    VirusKnob qStartKnob { VirusKnob::Size::Small, "Q Start" };
    VirusKnob qEndKnob   { VirusKnob::Size::Small, "Q End" };
    juce::TextButton qLinkButton { "Link" };

    MidiMonitor monitor;
    HelpButton helpButton;
    HelpOverlay helpOverlay;
    juce::TooltipWindow tooltipWindow { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;

    // Attachments (declared after the controls so they are destroyed first)
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<KnobAttachment>> knobAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    int lastScaleIndex = -1, lastKey = -1, lastLock = -1;
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyMagicPanel)
};

//==============================================================================
/** Resizable editor (fixed aspect ratio, 60 % .. 200 %); remembers its size. */
class MelodyMagicEditor : public juce::AudioProcessorEditor
{
public:
    explicit MelodyMagicEditor (MelodyMagicProcessor&);
    void resized() override;

private:
    MelodyMagicProcessor& processor;
    MelodyMagicPanel panel;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyMagicEditor)
};
