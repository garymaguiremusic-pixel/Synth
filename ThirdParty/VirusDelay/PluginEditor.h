#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "UI/VirusComponents.h"

//==============================================================================
/** The whole user interface, laid out at its base size (720 x 476). The editor
    below scales it to the window size. */
class VirusDelayPanel : public juce::Component,
                        private juce::Timer
{
public:
    static constexpr int kBaseWidth = 720, kBaseHeight = 476;

    explicit VirusDelayPanel (VirusDelayAudioProcessor&);
    ~VirusDelayPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupCombo (juce::ComboBox&, const juce::StringArray& items, const juce::String& tooltip);
    void layoutForMode();
    void updateActiveStates();
    void createBackground();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float> area);
    static void setActive (juce::Component&, bool active);
    juce::String spireDelayText (float knobValue) const;
    bool spireSyncOn() const;
    bool spirePingPongOn() const;

    VirusDelayAudioProcessor& processor;
    CustomLookAndFeel lnf;
    juce::Image background;

    // Mode tabs (one per delay mode) and dropdowns
    std::array<juce::TextButton, vd::NumDelayModes> modeTabs;
    juce::ComboBox ratioBox, patternBox, clockBox, clockLeftBox, clockRightBox, tapeClockBox, shapeBox, presetBox;

    // Knobs
    VirusKnob sendKnob       { VirusKnob::Size::Small, "Send" };
    VirusKnob feedbackKnob   { VirusKnob::Size::Large, "Feedback" };
    VirusKnob timeKnob       { VirusKnob::Size::Large, "Time" };
    VirusKnob colorKnob      { VirusKnob::Size::Small, "Color" };
    VirusKnob rateKnob       { VirusKnob::Size::Small, "Rate" };
    VirusKnob depthKnob      { VirusKnob::Size::Small, "Depth" };
    VirusKnob centerKnob     { VirusKnob::Size::Large, "Center Freq" };
    VirusKnob bandwidthKnob  { VirusKnob::Size::Small, "Bandwidth" };
    VirusKnob modulationKnob { VirusKnob::Size::Small, "Modulation" };
    juce::TextButton bypassButton { "Bypass" };

    // Spire Delay
    VirusKnob spireDelayLKnob   { VirusKnob::Size::Large, "Delay L" };
    VirusKnob spireDelayRKnob   { VirusKnob::Size::Large, "Delay R" };
    VirusKnob spireFeedbackKnob { VirusKnob::Size::Small, "F.Back" };
    VirusKnob spireRateKnob     { VirusKnob::Size::Small, "Rate" };
    VirusKnob spireModulateKnob { VirusKnob::Size::Small, "Modulate" };
    VirusKnob spireWideKnob     { VirusKnob::Size::Small, "Wide" };
    VirusKnob spireColorKnob    { VirusKnob::Size::Small, "Color" };
    VirusKnob spireDryWetKnob   { VirusKnob::Size::Small, "Dry/Wet" };
    juce::TextButton spirePingPongButton { "Ping-Pong" }, spireSyncButton { "Sync" };

    // Output section (all modes)
    VirusKnob drySourceKnob  { VirusKnob::Size::Small, "Level" };
    VirusKnob widthKnob      { VirusKnob::Size::Small, "Width" };
    VirusKnob duckAmountKnob { VirusKnob::Size::Small, "Amount" };
    VirusKnob moveRateKnob   { VirusKnob::Size::Small, "Rate" };
    VirusKnob moveDepthKnob  { VirusKnob::Size::Small, "Depth" };
    VirusKnob moveMixKnob    { VirusKnob::Size::Small, "Mix" };
    juce::TextButton duckButton { "Duck" }, moveHPButton { "Movement" }, movePhaserButton { "Phaser" }, moveFlangerButton { "Flanger" };
    juce::TextButton randomButton { "Randomise" };
    LfoPreview movePreview;

    LcdDisplay lcdLeft, lcdRight;
    LfoPreview lfoPreview;
    ModeVisualizer visualizer;
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

    // Layout / state
    juce::Rectangle<float> leftPanel, centrePanel, rightPanel, statusArea, saturationArea, modTypeArea;
    juce::Rectangle<float> dryPanel, widthPanel, duckPanel, movePanel, duckMeterArea;
    float duckReductionShown = 0.0f;
    int currentMode = -1;
    bool saturationLit = false;
    int lastShownProgram = -1;
    double lastTimerMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirusDelayPanel)
};

//==============================================================================
/** Resizable editor (fixed aspect ratio, 70 % .. 250 %); remembers its size. */
class VirusDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VirusDelayAudioProcessorEditor (VirusDelayAudioProcessor&);
    void resized() override;

private:
    VirusDelayAudioProcessor& processor;
    VirusDelayPanel panel;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirusDelayAudioProcessorEditor)
};
