#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "UI/VirusComponents.h"

//==============================================================================
/**
    The whole interface, laid out at a fixed design size (kWidth x kHeight).
    The editor scales it as a unit, so the plugin window can be resized freely
    while every control keeps its place and proportions.
*/
class LeadVerbPanel : public juce::Component,
                      private juce::Timer
{
public:
    static constexpr int kWidth = 560, kHeight = 533;

    explicit LeadVerbPanel (LeadVerbAudioProcessor&);
    ~LeadVerbPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Bring every readout and the section dimming up to date now (normally the 30 Hz timer does). */
    void refreshNow() { timerCallback(); }

private:
    void timerCallback() override;
    void createBackground();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float> area);
    bool syncOn() const;
    int currentModeIndex() const;
    float dampValue() const;
    void showDecayEntry();
    void commitDecayEntry();
    bool massiveActive() const;
    int massiveType() const;
    void updateSectionDimming();

    LeadVerbAudioProcessor& processor;
    juce::Image background;   // rendered at 2x so it stays crisp when the window is enlarged

    // Dropdowns
    juce::ComboBox modeBox, presetBox;

    // Knobs
    VirusKnob predelayKnob  { VirusKnob::Size::Large, "Predelay" };
    VirusKnob decayKnob     { VirusKnob::Size::Large, "Decay" };
    VirusKnob modulationKnob { VirusKnob::Size::Small, "Modulation" };
    VirusKnob dampKnob      { VirusKnob::Size::Small, "Damp" };
    VirusKnob wideKnob      { VirusKnob::Size::Small, "Wide" };
    VirusKnob colorKnob     { VirusKnob::Size::Small, "Color" };
    VirusKnob shelfKnob     { VirusKnob::Size::Small, "Hi Shelf" };
    VirusKnob sidechainKnob { VirusKnob::Size::Small, "Sidechain" };
    VirusKnob liftKnob      { VirusKnob::Size::Small, "Lift" };
    VirusKnob dryWetKnob    { VirusKnob::Size::Small, "Dry/Wet" };
    VirusKnob drySourceKnob { VirusKnob::Size::Small, "Dry Source" };
    VirusKnob delayFeedbackKnob { VirusKnob::Size::Small, "Feedback" };
    VirusKnob delayMixKnob  { VirusKnob::Size::Small, "Delay Mix" };

    // Mode 2 (Massive)
    juce::ComboBox massiveTypeBox;
    VirusKnob massiveDryWetKnob  { VirusKnob::Size::Small, "Dry/Wet" };
    VirusKnob massiveSizeKnob    { VirusKnob::Size::Small, "Size" };
    VirusKnob massiveDensityKnob { VirusKnob::Size::Small, "Density" };
    VirusKnob massiveColorKnob   { VirusKnob::Size::Small, "Color" };
    LcdDisplay lcdMassive;

    // Mode 1 / Mode 2 switch
    juce::TextButton sectionButtons[2] { juce::TextButton { "Mode 1" }, juce::TextButton { "Mode 2" } };
    juce::TextButton bypassButton { "Bypass" }, randomButton { "Random" }, syncButton { "Sync" }, extButton { "Ext" };

    // PingPong delay
    juce::TextButton delayOnButton { "On" };
    juce::ComboBox delayTimeBox;
    juce::TextButton delayTypeButtons[3] { juce::TextButton { "Normal" }, juce::TextButton { "Dotted" }, juce::TextButton { "Triplet" } };

    LcdDisplay lcdPredelay, lcdDecay, lcdDelay;
    juce::TextEditor decayEntry;   // right-click Decay to type a time
    ReverbVisualizer visualizer;
    TempoField tempoField;
    HelpButton helpButton;
    HelpOverlay helpOverlay;
    juce::TooltipWindow tooltipWindow { this, 500 };

    // Attachments (declared after the controls so they are destroyed first)
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<KnobAttachment>> knobAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::ParameterAttachment> delayTypeAttachment, sectionAttachment;

    // Layout / state
    juce::Rectangle<float> mode1Panel, leftPanel, centrePanel, rightPanel, globalPanel, massivePanel, delayPanel, statusArea, duckArea;
    int shownSection = -1;
    float shownDuckDb = -1.0f;
    bool shownSidechainOn = false, shownExternal = false, shownKeyConnected = false;
    int lastShownProgram = -1;
    double lastTimerMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeadVerbPanel)
};

//==============================================================================
/** Resizable editor: keeps the panel's aspect ratio and scales it to fit. */
class LeadVerbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LeadVerbAudioProcessorEditor (LeadVerbAudioProcessor&);
    ~LeadVerbAudioProcessorEditor() override;

    void resized() override;

private:
    LeadVerbAudioProcessor& processor;
    CustomLookAndFeel lnf;   // declared before the panel so it outlives it
    LeadVerbPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeadVerbAudioProcessorEditor)
};
