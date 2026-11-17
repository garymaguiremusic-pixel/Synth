#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "UI/VirusComponents.h"

//==============================================================================
/** The whole user interface, laid out at its base size. The editor scales it. */
class VCClonePanel : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer,
                     private juce::ListBoxModel
{
public:
    static constexpr int kBaseWidth = 1000, kBaseHeight = 660;

    explicit VCClonePanel (VCCloneAudioProcessor&);
    ~VCClonePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    enum class Preview { None, Osc1, Osc2, FiltEnv, AmpEnv, Lfo1, Lfo2, Lfo3 };

private:
    struct Item
    {
        int offset = -1;
        enum class Kind { Knob, Combo, Toggle, Preview } kind = Kind::Knob;
        int cells = 1;
        juce::Component* comp = nullptr;
    };
    struct Panel
    {
        juce::String title;
        int tab = 0, cols = 1;
        std::vector<Item> items;
        juce::Rectangle<float> bounds;
    };

    void timerCallback() override;
    void buildPanels();
    void addPanel (int tab, const juce::String& title, std::vector<int> offsets, int cols, Preview preview = Preview::None);
    void layoutPanels();
    void showTab (int);
    void createBackground();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float>);
    void refreshPresetList();
    void openLoadDialog();
    void openExportMenu();
    void loadFile (const juce::File&);
    void touched (int offset);
    float previewValue (Preview, float x) const;
    juce::String buildInfoText() const;

    // ListBoxModel (browser)
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    VCCloneAudioProcessor& processor;
    CustomLookAndFeel lnf;
    juce::Image background;

    static constexpr int kNumTabs = 7;
    std::array<juce::TextButton, kNumTabs> tabs;
    int currentTab = 0;

    juce::TextButton loadButton { "Load" }, exportButton { "Export" }, initButton { "Init" }, prevButton { "<" }, nextButton { ">" };
    juce::ComboBox presetBox;

    std::vector<Panel> panels;
    std::vector<std::unique_ptr<VirusKnob>> knobs;
    std::vector<std::unique_ptr<VirusCombo>> combos;
    std::vector<std::unique_ptr<juce::TextButton>> toggles;
    std::vector<std::unique_ptr<CurvePreview>> previews;
    std::vector<std::unique_ptr<juce::Label>> comboLabels;
    std::vector<std::unique_ptr<KnobAttachment>> knobAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<ToggleAttachment>> toggleAttachments;
    std::vector<std::pair<CurvePreview*, Preview>> previewKinds;

    // browser tab
    juce::ListBox patchList { "patches", this };
    juce::TextEditor infoText;

    LcdDisplay lcd, statusLcd;
    HelpButton helpButton;
    HelpOverlay helpOverlay;
    juce::TooltipWindow tooltipWindow { this, 500 };
    std::unique_ptr<juce::FileChooser> chooser;

    std::array<int, 256> shownValues {};
    int lcdOffset = -1;
    int lastBankVersion = -1;
    juce::String statusMessage;
    double statusUntil = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCClonePanel)
};

//==============================================================================
/** Resizable editor (fixed aspect ratio, 70 % .. 250 %); remembers its size. */
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
