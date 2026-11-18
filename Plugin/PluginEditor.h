#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "UI/VirusComponents.h"
#include "UI/ParamAdapters.h"
#include "UI/SynthWidgets.h"
#include "UI/BrowserView.h"
#include "UI/OttKnob.h"
#include "UI/PanelWidgets.h"

//==============================================================================
/** The whole user interface at its base size (1200 x 732); the editor scales it.
    Top row: Oscillator, Filter, Modulation (each with tabs). Middle row: Sequencer, Delay, Reverb,
    Distortion. Bottom: the Play bar (output, glide / bend, voices, arp). Cards whose contents differ
    per synth mode hold one set of controls per mode. The modulation matrix (Virus, Spire) opens over
    the Filter and Modulation cards. */
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
    /** Something placed on the panel: a component, a caption under it, or drawn text / boxes. */
    struct Item
    {
        juce::Component* comp = nullptr;
        juce::Rectangle<int> r;
        juce::String caption, fixedText;
        int captionY = 0;
        int mode = -1, layer = -1;           // layer 0: normal cards, 1: the matrix over them
        int group = -1, index = 0;           // shown while tab group 'group' selects 'index'
        int indexMask = 0;                   // non-zero: shown for every tab whose bit is set
        int group2 = -1, index2 = 0;         // and a second tab group (osc MAIN / MORE, Spire LFO / ENV number)
        int style = 0;                       // 0 component, 1 fixed "send to" box, 3 small heading text
    };
    struct Card
    {
        juce::Rectangle<float> r;
        juce::String title;
        int mode = -1, layer = -1;
        bool power = false;
    };
    class TabButton : public juce::Button
    {
    public:
        TabButton (juce::String text, bool small) : juce::Button (text), small (small) {}
        void paintButton (juce::Graphics&, bool, bool) override;
        void mouseDown (const juce::MouseEvent& e) override { downX = e.x; juce::Button::mouseDown (e); }
        std::function<bool()> isOn;          // the on/off dot; null = no dot
        std::function<void()> togglePower;
        std::function<void (juce::Graphics&, juce::Rectangle<float>, bool)> icon;   // drawn instead of the text
        bool small, led = false;
        int downX = 99;
    };
    struct TabGroup
    {
        int mode = -1, selected = 0;
        std::vector<std::unique_ptr<TabButton>> buttons;
    };
    /** One cell of a row: a knob (draws its own name and value) or a dropdown / switch with a caption. */
    struct Cell
    {
        Control* control = nullptr;
        juce::Component* comp = nullptr;
        int w = 58, knob = 34;
        juce::String caption;
    };
    using AP = std::unique_ptr<ParamAdapter>;

    void timerCallback() override;
    Control* makeControl (AP, bool forceKnob = false);
    Item& place (int mode, juce::Component*, juce::Rectangle<int>, juce::String caption = {});
    void row (int mode, int x, int y, int boxH, int gap, std::vector<Cell>);
    Cell K (AP, int knob = 34, int w = 58);
    Cell D (AP, int w, juce::String caption);
    Cell T (AP, int w, juce::String caption, juce::String onText = "ON");
    Control* headerCombo (int mode, AP, juce::Rectangle<int>);
    Control* smallToggle (int mode, AP, juce::Rectangle<int>, juce::String text);
    Item& fixedBox (int mode, juce::Rectangle<int>, juce::String text, juce::String caption);
    Item& heading (int mode, int x, int y, juce::String text);
    int addTabs (int mode, juce::Rectangle<int>, juce::StringArray names, std::vector<const char*> powerIds, bool small = false);
    void setGroup (size_t fromItem, int group, int index);
    void setGroup2 (size_t fromItem, int group, int index);
    Card& card (int mode, juce::Rectangle<float>, juce::String title, const char* powerId = nullptr);
    WaveScope* addScope (int mode, juce::Rectangle<int>, std::function<float (float)>, std::function<int()> layers = {});
    EnvelopeGraph* addEnvGraph (int mode, juce::Rectangle<int>, ParamAdapter& a, ParamAdapter& d, ParamAdapter& s, ParamAdapter& r);
    ParamAdapter& keep (AP);

    AP host (const char* id, juce::String label = {});
    AP virus (int offset, juce::String label = {});
    AP fx (juce::AudioProcessor&, const char* id, juce::String label);
    AP envTime (const char* id, const char* syncId, juce::String label);

    void buildShared();
    void buildVirus();
    void buildSylenth();
    void buildSpire();
    void buildMatrix();
    void addEnvPages (int mode, int tabGroup);
    void addOscCommon (int mode);
    void oscCentre (int mode, AP model, const juce::String& modelCaption, ParamAdapter* octave, ParamAdapter* semi, ParamAdapter* fine,
                    const juce::String& fineLabel, std::function<float (float)> wave, AP waveList, const juce::String& waveCaption);
    void levelLed (int mode, int index, const char* powerId);
    void copyOsc();
    void pasteOsc();
    void updateVisibility();
    bool itemVisible (const Item&) const;
    int currentMode() const;
    int currentLayer() const;
    void createBackground();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float>);
    void drawCard (juce::Graphics&, const Card&);
    void openLoadDialog();
    void openExportMenu();
    void openModeMenu();
    void openPresetMenu();
    void loadFile (const juce::File&);
    void showBrowser (bool);

    VCCloneAudioProcessor& processor;
    CustomLookAndFeel lnf;
    juce::Image background, noise;

    juce::TextButton modeButton, nameButton, initButton { "INIT" }, prevButton { "<" }, nextButton { ">" };
    juce::TextButton browserButton { "Browser" }, loadButton { "Import" }, exportButton { "Export" };
    juce::TextButton randomiseButton { "RANDOMISE" }, matrixButton { "MATRIX" }, matrixBack { "BACK" };
    std::unique_ptr<juce::Component> dragHandle, noteGrid;
    Control* outputKnob = nullptr;
    VoiceMeter voiceMeter;
    BrowserView browser;
    bool browserShown = false, matrixShown = false;
    int curLayer = -1;
    juce::String lcdName, lcdValue;
    double lcdUntil = 0.0;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<std::unique_ptr<ParamAdapter>> keptAdapters;
    std::vector<Item> items;
    std::vector<Card> cards;
    std::vector<std::unique_ptr<TabGroup>> tabGroups;
    std::vector<std::unique_ptr<WaveScope>> scopes;
    std::vector<std::unique_ptr<EnvelopeGraph>> envGraphs;
    std::vector<std::unique_ptr<juce::Component>> widgets;
    std::vector<OscDisplay*> displays;
    EqGraph* eqGraph = nullptr;
    int modBGroup = -1, eqBandGroup = -1, oscGroup[3] { -1, -1, -1 };
    juce::TextButton copyButton { "COPY" }, pasteButton { "PASTE" };
    std::vector<int> oscClipboard;
    int oscClipboardMode = -1;
    struct Retitle { OttKnob* knob; std::function<juce::String()> title; };
    std::vector<Retitle> retitles;            // knobs whose name follows another value (Virus HyperSaw)

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
