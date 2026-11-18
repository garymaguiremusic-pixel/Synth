#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "UI/VirusComponents.h"
#include "UI/ParamAdapters.h"
#include "UI/SynthWidgets.h"

//==============================================================================
/** The whole user interface at its base size (1200 x 732); the editor scales it. */
class VCClonePanel : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer,
                     private juce::ListBoxModel
{
public:
    static constexpr int kBaseWidth = 1200, kBaseHeight = 732;

    explicit VCClonePanel (VCCloneAudioProcessor&);
    ~VCClonePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    enum Page { PageSynth, PageMod, PageMatrix, PageArp, PageFx, PageBrowser, NumPages };

private:
    // a control bound to an adapter
    struct Control
    {
        std::unique_ptr<ParamAdapter> adapter;
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;      // dropdown caption
        enum class Kind { Knob, Combo, Toggle, Segment } kind = Kind::Knob;
        int cells = 1;
        int shown = INT_MIN;
    };
    struct Panel
    {
        juce::String title;
        int page = PageSynth, mode = -1;          // mode -1 = every mode
        int row = 0, group = -1, groupIndex = 0;
        int cols = 1;
        std::vector<Control*> controls;
        juce::Rectangle<float> bounds;
        bool strip = false;                       // the global strip under the synth pages
    };
    /** A block of the compact Synth page: header (title, optional selector), optional envelope graph,
        optional big knob, and a grid of controls that shrinks to fit. */
    struct Section
    {
        juce::String title;
        int mode = 0;
        juce::Rectangle<float> bounds;
        Control* header = nullptr;                // segmented selector in the header
        Control* power = nullptr;                 // on/off button after the title (oscillators)
        Control* hero = nullptr;                  // big knob (cutoff)
        std::vector<Control*> controls;
        EnvelopeGraph* env = nullptr;
        int group = -1, groupIndex = 0;           // alternatives share one block (LFO 1..4)
        int tab = -1;                             // shown only on this tab of the centre display
        float headerH = 18.0f;                    // 40 for a tab bar
        bool segTop = false;                      // header selector as the first row of the body
        WaveScope* scope = nullptr;               // waveform picture at the left of the body
        std::vector<juce::Component*> bottomComps;   // bottom of the body: first wide, the rest stacked at the right
        float bottomH = 0.0f;
        int subGroup = -1, subGroupIndex = 0;     // a second selector (matrix slot) shown as a button row in the body
    };
    struct Group
    {
        bool header = false;                      // buttons in a Synth-page section header
        bool tabBar = false;                      // full-width tabs, each with an on/off dot
        std::vector<Control*> powers;
        int page = 0, mode = -1, row = 0, selected = 0;
        juce::StringArray labels;
        std::vector<std::unique_ptr<juce::TextButton>> buttons;
        juce::Rectangle<int> area;
    };

    void timerCallback() override;
    Control* makeControl (std::unique_ptr<ParamAdapter>, bool forceKnob = false, bool powerDot = false);
    Control* makeSegment (std::unique_ptr<ParamAdapter>);
    Section& addSection (int mode, juce::String title, juce::Rectangle<float> bounds, std::unique_ptr<ParamAdapter> header,
                         std::unique_ptr<ParamAdapter> hero, std::vector<std::unique_ptr<ParamAdapter>> body,
                         int group = -1, int groupIndex = 0);
    Section& addEnvSection (int mode, juce::String title, juce::Rectangle<float> bounds, std::vector<std::unique_ptr<ParamAdapter>> adsrAndMore);
    int addHeaderGroup (int mode, juce::StringArray labels);
    int addTabBar (int mode, juce::StringArray labels, std::vector<std::unique_ptr<ParamAdapter>> powers);
    WaveScope* addScope (std::function<float (float)> fn, std::function<int()> layers = {});
    void buildSynthPages();
    void layoutSections();
    bool sectionVisible (const Section&) const;
    Panel& addPanel (juce::String title, int page, int mode, int row, std::vector<std::unique_ptr<ParamAdapter>> adapters,
                     int group = -1, int groupIndex = 0);
    int addGroup (int page, int mode, int row, juce::StringArray labels);
    std::unique_ptr<ParamAdapter> host (const char* id, juce::String label = {});
    std::unique_ptr<ParamAdapter> virus (int offset, juce::String label = {});
    void buildVirus();
    void buildSylenth();
    void buildSpire();
    void buildGlobal();
    void layoutAll();
    void updateVisibility();
    void showPage (int);
    bool panelVisible (const Panel&) const;
    int currentMode() const;
    void createBackground();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float>);
    void refreshPresetList();
    void openLoadDialog();
    void openExportMenu();
    void loadFile (const juce::File&);
    juce::String buildInfoText() const;
    void layoutEmbedded();

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    VCCloneAudioProcessor& processor;
    CustomLookAndFeel lnf;
    juce::Image background;

    std::array<juce::TextButton, 3> modeButtons;
    std::array<juce::TextButton, NumPages> tabs;
    int currentPage = PageSynth;
    juce::TextButton loadButton { "Load" }, exportButton { "Export" }, initButton { "Init" }, prevButton { "<" }, nextButton { ">" };
    juce::ComboBox presetBox;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Panel> panels;
    std::vector<std::unique_ptr<Group>> groups;
    std::vector<Section> sections;
    enum DisplayTab { TabArp, TabMod, TabDistort, TabDelay, TabReverb, NumDisplayTabs };
    std::array<juce::TextButton, NumDisplayTabs> displayTabs;
    std::array<juce::TextButton, 2> modSubButtons;    // MOD tab: Matrix / More
    int displayTab = TabArp, modSub = 0;
    juce::TextButton randomiseButton { "Randomise" }, browserButton { "Browser" };
    std::unique_ptr<juce::Component> dragHandle;
    Control* fxOn[2] {};                          // Delay / Reverb on buttons in the display
    juce::Rectangle<float> displayArea;
    std::vector<std::unique_ptr<EnvelopeGraph>> envGraphs;
    std::vector<std::unique_ptr<WaveScope>> scopes;
    VoiceMeter voiceMeter;
    std::array<Control*, 3> voiceModeCombo {};    // per synth mode: polyphony / voice mode

    // embedded panels from Gary's plugins
    std::unique_ptr<juce::Component> delayPanel, reverbPanel, stepGrid;
    juce::Rectangle<float> delayArea, reverbArea, gridArea;

    juce::ListBox patchList { "patches", this };
    juce::TextEditor infoText;
    LcdDisplay lcd, statusLcd, soundsetLcd;
    juce::Rectangle<int> lcdTop;
    HelpButton helpButton;
    HelpOverlay helpOverlay;
    juce::TooltipWindow tooltipWindow { this, 500 };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String lcdName, lcdValue;
    int lastBankVersion = -1, lastMode = -1, lastArpMode = -1;
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
