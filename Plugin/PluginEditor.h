#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "UI/VirusComponents.h"
#include "UI/ParamAdapters.h"
#include "UI/SynthWidgets.h"
#include "UI/BrowserView.h"

//==============================================================================
/** The whole user interface at its base size (1200 x 732); the editor scales it.
    One window of cards (oscillators, LFOs, envelopes, filter, amplifier, step sequencer, delay, reverb,
    distortion, arpeggiator, glide, pitch, voices). Cards that differ per synth mode hold one set of
    controls per mode; tab groups (oscillator tabs, filter tabs) switch what a card shows. */
class VCClonePanel : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    static constexpr int kBaseWidth = 1200, kBaseHeight = 732;

    explicit VCClonePanel (VCCloneAudioProcessor&);
    ~VCClonePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    struct Control
    {
        std::unique_ptr<ParamAdapter> adapter;
        std::unique_ptr<juce::Component> comp;
        enum class Kind { Knob, Combo, Toggle } kind = Kind::Knob;
        int shown = INT_MIN;
    };
    /** Something placed on a card: a control, a caption under it, or a fixed "send to" box. */
    struct Item
    {
        juce::Component* comp = nullptr;
        Control* control = nullptr;
        juce::Rectangle<int> r;
        juce::String caption;        // drawn under combos / toggles / fixed boxes
        juce::String fixedText;      // a fixed target ("-> Volume"): drawn, no component
        int mode = -1;               // -1: every mode
        int group = -1, index = 0;   // shown only while the tab group selects index
        int layer = -1;              // 0: LFO / envelope cards, 1: the modulation matrix over them, -1: always
        int style = 0;               // 0 control, 1 fixed target box, 2 row label (fixedText + caption)
    };
    struct Card
    {
        juce::Rectangle<float> r;
        juce::String title;
        int mode = -1, layer = -1;
        Control* power = nullptr;
    };
    /** A row of tabs (oscillator pages, filter pages); each may carry an on/off dot. */
    class TabButton : public juce::Button
    {
    public:
        TabButton (juce::String text, bool small) : juce::Button (text), small (small) {}
        void paintButton (juce::Graphics&, bool, bool) override;
        std::function<bool()> isOn;      // the dot; null = no dot
        std::function<void()> togglePower;
        void mouseDown (const juce::MouseEvent& e) override { downX = e.x; juce::Button::mouseDown (e); }
        int downX = 99;
        bool small;
    };
    struct TabGroup
    {
        int mode = -1, selected = 0;
        std::vector<std::unique_ptr<TabButton>> buttons;
    };

    void timerCallback() override;
    Control* makeControl (std::unique_ptr<ParamAdapter>, bool forceKnob = false);
    Item& place (int mode, juce::Component*, juce::Rectangle<int>, juce::String caption = {});
    Control* knob (int mode, std::unique_ptr<ParamAdapter>, int x, int y, int w = 52, int h = 50);
    Control* combo (int mode, std::unique_ptr<ParamAdapter>, int x, int y, int w, juce::String caption, int h = 24);
    Control* toggle (int mode, std::unique_ptr<ParamAdapter>, int x, int y, int w, juce::String caption, juce::String onText = "ON");
    Item& fixedBox (int mode, int x, int y, int w, juce::String text, juce::String caption);
    Item& rowLabel (int mode, int x, int y, int w, juce::String text, juce::String sub);
    void separator (int mode, float x, float y, float w);
    int addTabs (int mode, juce::Rectangle<int> area, juce::StringArray names, std::vector<const char*> powerIds, bool small);
    void setGroup (int group, int index, size_t fromItem);   // items added since fromItem belong to the tab
    Card& card (int mode, juce::Rectangle<float> r, juce::String title, const char* powerId = nullptr);

    std::unique_ptr<ParamAdapter> host (const char* id, juce::String label = {});
    std::unique_ptr<ParamAdapter> virus (int offset, juce::String label = {});
    std::unique_ptr<ParamAdapter> fx (juce::AudioProcessor&, const char* id, juce::String label);

    void buildShared();
    void buildVirus();
    void buildSylenth();
    void buildSpire();
    void buildMatrix();
    void updateVisibility();
    bool itemVisible (const Item&) const;
    int currentMode() const;
    void createBackground();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float>);
    void openLoadDialog();
    void openExportMenu();
    void openModeMenu();
    void openPresetMenu();
    void loadFile (const juce::File&);
    void showBrowser (bool);

    VCCloneAudioProcessor& processor;
    CustomLookAndFeel lnf;
    juce::Image background;

    juce::TextButton modeButton, nameButton, initButton { "INIT" }, prevButton { "<" }, nextButton { ">" };
    juce::TextButton browserButton { "Browser" }, loadButton { "Import" }, exportButton { "Export" };
    juce::TextButton randomiseButton { "RANDOMISE" }, matrixButton { "MATRIX" };
    bool matrixShown = false;
    int curLayer = -1;
    juce::String lcdName, lcdValue;
    std::unique_ptr<juce::Component> dragHandle;
    std::unique_ptr<juce::Component> stepGrid;
    Control* outputKnob = nullptr;
    VoiceMeter voiceMeter;
    std::vector<std::unique_ptr<WaveScope>> scopes;
    BrowserView browser;
    bool browserShown = false;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Item> items;
    std::vector<Card> cards;
    std::vector<std::unique_ptr<TabGroup>> tabGroups;
    struct Sep { int mode, layer; juce::Rectangle<float> r; };
    std::vector<Sep> separators;

    juce::TooltipWindow tooltipWindow { this, 500 };
    std::unique_ptr<juce::FileChooser> chooser;
    int lastBankVersion = -1, lastMode = -1;
    juce::String statusMessage;
    double statusUntil = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCClonePanel)
};

//==============================================================================
class VCCloneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VCCloneAudioProcessorEditor (VCCloneAudioProcessor&);
    void resized() override;

private:
    VCCloneAudioProcessor& processor;
    VCClonePanel panel;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCCloneAudioProcessorEditor)
};
