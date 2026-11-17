#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;

namespace
{
    constexpr int kWidth = LeadVerbPanel::kWidth, kHeight = LeadVerbPanel::kHeight;
    constexpr int kTopStripH = 34, kBottomBarY = 502;
    constexpr int kMassiveY = 264, kDelayY = 366;   // tops of the Mode 2 and PingPong strips

    // Centre panel: two large knobs, then a column of two small ones
    const Rectangle<int> largeSlot1 { 158, 64, 90, 90 };
    const Rectangle<int> largeSlot2 { 248, 64, 90, 90 };
    // (the column ends above the Tempo / Sync row, which runs under all of them)
    const Rectangle<int> smallTop    { 338, 58, 62, 70 };
    const Rectangle<int> smallBottom { 338, 127, 62, 70 };
}

//==============================================================================
LeadVerbPanel::LeadVerbPanel (LeadVerbAudioProcessor& p)
    : processor (p),
      tempoField (*p.apvts.getParameter (lv::ParamIDs::bpm))
{
    setSize (kWidth, kHeight);
    createBackground();

    auto& apvts = processor.apvts;

    // Dropdowns ------------------------------------------------------------------
    modeBox.addItemList (lv::getModeNames(), 1);
    modeBox.setTooltip ("Mode 1: Spire's reverb mode");
    modeBox.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (modeBox);
    comboAttachments.push_back (std::make_unique<ComboAttachment> (apvts, lv::ParamIDs::mode, modeBox));

    for (int i = 0; i < lv::numFactoryPresets; ++i)
        presetBox.addItem (String (i + 1).paddedLeft ('0', 2) + "  " + lv::factoryPresets[i].name, i + 1);
    presetBox.setTooltip ("Factory presets");
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != processor.getCurrentProgram())
            processor.setCurrentProgram (index);
        lastShownProgram = index;
    };
    addAndMakeVisible (presetBox);

    // Knobs ----------------------------------------------------------------------
    struct KnobSetup { VirusKnob& knob; const char* id; String (*describe) (float); };
    const KnobSetup knobs[] =
    {
        { predelayKnob, lv::ParamIDs::predelay, nullptr },
        { decayKnob,    lv::ParamIDs::decay,    nullptr },
        { modulationKnob, lv::ParamIDs::modulation, lv::describeModulation },
        { dampKnob,     lv::ParamIDs::damp,     nullptr },
        { wideKnob,     lv::ParamIDs::wide,     lv::describeWide },
        { colorKnob,    lv::ParamIDs::color,    nullptr },
        { shelfKnob,    lv::ParamIDs::shelf,    lv::describeShelf },
        { sidechainKnob, lv::ParamIDs::sidechain, lv::describeSidechain },
        { liftKnob,     lv::ParamIDs::lift,     lv::describeLift },
        { drySourceKnob, lv::ParamIDs::drySource, lv::describeDrySource },
        { dryWetKnob,   lv::ParamIDs::dryWet,   lv::describeDryWet },
        { massiveDryWetKnob,  lv::ParamIDs::massiveDryWet,  lv::describeMassiveDryWet },
        { massiveSizeKnob,    lv::ParamIDs::massiveSize,    nullptr },
        { massiveDensityKnob, lv::ParamIDs::massiveDensity, lv::describeMassiveDensity },
        { massiveColorKnob,   lv::ParamIDs::massiveColor,   nullptr },
        { delayFeedbackKnob, lv::ParamIDs::delayFeedback, lv::describeDelayFeedback },
        { delayMixKnob, lv::ParamIDs::delayMix, lv::describeDelayMix },
    };

    for (auto& k : knobs)
    {
        if (k.describe != nullptr)
            k.knob.describeValue = k.describe;
        knobAttachments.push_back (std::make_unique<KnobAttachment> (*apvts.getParameter (k.id), k.knob));
        addAndMakeVisible (k.knob);
    }
    predelayKnob.describeValue = [this] (float v) { return lv::describePredelay (v, syncOn(), processor.gui.bpm.load()); };
    decayKnob.onRightClick = [this] { showDecayEntry(); };
    lcdDecay.onRightClick  = [this] { showDecayEntry(); };
    decayKnob.describeValue    = [this] (float v) { return lv::describeDecay (v, currentModeIndex(), dampValue()); };
    dampKnob.describeValue     = [] (float v) { return lv::describeDamp (v); };
    colorKnob.describeValue    = [] (float v) { return lv::describeColor (v); };

    // Buttons ----------------------------------------------------------------------
    const std::tuple<TextButton*, const char*, const char*> buttons[] =
    {
        { &bypassButton, lv::ParamIDs::bypass, "Bypass the reverb (dry signal only)" },
        { &extButton,    lv::ParamIDs::externalSidechain,
          "External sidechain: duck the reverb from the plugin's sidechain input (route a kick or any track to it in your DAW). Off = duck from the plugin's own input" },
        { &syncButton,   lv::ParamIDs::sync,   "Predelay Sync: the Predelay knob steps through note values (1/64 - 16/1) at the host tempo" },
        { &delayOnButton, lv::ParamIDs::delayOn,
          "PingPong delay on the reverb: wide, moving, tempo-synced echoes after all the reverb processing, before Dry/Wet" },
    };
    for (auto& [button, id, tip] : buttons)
    {
        button->setClickingTogglesState (true);
        button->setTooltip (tip);
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (apvts, id, *button));
        addAndMakeVisible (*button);
    }

    // PingPong delay: note value box and Normal / Dotted / Triplet buttons
    delayTimeBox.addItemList (lv::getDelayNoteNames(), 1);
    delayTimeBox.setTooltip ("Delay time, synced to the tempo");
    delayTimeBox.setJustificationType (Justification::centred);
    addAndMakeVisible (delayTimeBox);
    comboAttachments.push_back (std::make_unique<ComboAttachment> (apvts, lv::ParamIDs::delayTime, delayTimeBox));

    const char* typeTips[] = { "Straight note values", "Dotted: 1.5 x the note", "Triplet: 2/3 of the note" };
    for (int i = 0; i < 3; ++i)
    {
        delayTypeButtons[i].setTooltip (typeTips[i]);
        delayTypeButtons[i].setConnectedEdges ((i > 0 ? Button::ConnectedOnLeft : 0) | (i < 2 ? Button::ConnectedOnRight : 0));
        delayTypeButtons[i].onClick = [this, i] { delayTypeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (delayTypeButtons[i]);
    }
    delayTypeAttachment = std::make_unique<ParameterAttachment> (*apvts.getParameter (lv::ParamIDs::delayType), [this] (float v)
    {
        for (int i = 0; i < 3; ++i)
            delayTypeButtons[i].setToggleState (i == roundToInt (v), dontSendNotification);
    });
    delayTypeAttachment->sendInitialUpdate();

    // Mode 2: Massive's type
    massiveTypeBox.addItemList (lv::getMassiveTypeNames(), 1);
    massiveTypeBox.setTooltip ("Mode 2: Massive's Reverb or Small Reverb");
    massiveTypeBox.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (massiveTypeBox);
    comboAttachments.push_back (std::make_unique<ComboAttachment> (apvts, lv::ParamIDs::massiveType, massiveTypeBox));
    massiveSizeKnob.describeValue  = [this] (float v) { return lv::describeMassiveSize (v, massiveType()); };
    massiveColorKnob.describeValue = [this] (float v) { return lv::describeMassiveColor (v, massiveType()); };
    addAndMakeVisible (lcdMassive);

    // Mode 1 / Mode 2 switch
    const char* sectionTips[] = { "Reverb Mode 1: Spire's reverb (the Mode 1 panels)", "Reverb Mode 2: Massive's reverb (the Mode 2 panel)" };
    for (int i = 0; i < 2; ++i)
    {
        sectionButtons[i].setTooltip (sectionTips[i]);
        sectionButtons[i].setConnectedEdges (i == 0 ? Button::ConnectedOnRight : Button::ConnectedOnLeft);
        sectionButtons[i].onClick = [this, i] { sectionAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (sectionButtons[i]);
    }
    sectionAttachment = std::make_unique<ParameterAttachment> (*apvts.getParameter (lv::ParamIDs::section), [this] (float v)
    {
        for (int i = 0; i < 2; ++i)
            sectionButtons[i].setToggleState (i == roundToInt (v), dontSendNotification);
    });
    sectionAttachment->sendInitialUpdate();

    randomButton.setTooltip ("Randomise the active section's knobs (Mode 1: also its Mode and Sync; Mode 2: also its type)");
    randomButton.onClick = [this] { processor.randomiseSettings(); };
    addAndMakeVisible (randomButton);

    // typed decay time: an LCD-coloured field over the Decay readout
    decayEntry.setJustification (Justification::centred);
    decayEntry.setFont (CustomLookAndFeel::uiFont (12.0f, true));
    decayEntry.setColour (TextEditor::backgroundColourId, VirusColours::lcdBottom);
    decayEntry.setColour (TextEditor::textColourId, Colours::white);
    decayEntry.setColour (TextEditor::highlightColourId, VirusColours::blue.withAlpha (0.6f));
    decayEntry.setColour (TextEditor::outlineColourId, VirusColours::blue);
    decayEntry.setColour (TextEditor::focusedOutlineColourId, Colours::white);
    decayEntry.setTooltip ("Type a decay time, e.g. 4.5, 4.5s or 4500ms, then press Enter (Esc cancels)");
    decayEntry.onReturnKey = [this] { commitDecayEntry(); };
    decayEntry.onEscapeKey = [this] { decayEntry.setVisible (false); };
    decayEntry.onFocusLost = [this] { decayEntry.setVisible (false); };
    addChildComponent (decayEntry);

    addAndMakeVisible (lcdPredelay);
    addAndMakeVisible (lcdDecay);
    addAndMakeVisible (lcdDelay);
    addAndMakeVisible (visualizer);
    addAndMakeVisible (tempoField);

    helpButton.setTooltip ("Help");
    helpButton.onClick = [this] { helpOverlay.setVisible (true); helpOverlay.toFront (false); };
    addAndMakeVisible (helpButton);
    addChildComponent (helpOverlay);

    lastTimerMs = Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimerHz (30);
}

LeadVerbPanel::~LeadVerbPanel()
{
    stopTimer();
}

//==============================================================================
void LeadVerbPanel::resized()
{
    // Mode 1 is one section across the top: mode + Dry/Wet | Predelay / Decay | Stereo / Tone
    mode1Panel  = { 8.0f,   40.0f, 544.0f, 218.0f };
    leftPanel   = { 8.0f,   40.0f, 138.0f, 218.0f };
    centrePanel = { 152.0f, 40.0f, 250.0f, 218.0f };
    rightPanel  = { 408.0f, 40.0f, 144.0f, 218.0f };
    // lower rows: the Global panel (both modes) beside the Mode 2 and PingPong strips
    globalPanel  = { 8.0f, (float) kMassiveY, 138.0f, 198.0f };
    massivePanel = { 152.0f, (float) kMassiveY, 400.0f, 96.0f };
    delayPanel   = { 152.0f, (float) kDelayY, 400.0f, 96.0f };

    // top strip: Mode 1 / Mode 2, Random, Bypass, presets
    sectionButtons[0].setBounds (158, 8, 56, 19);
    sectionButtons[1].setBounds (214, 8, 56, 19);
    randomButton.setBounds (280, 8, 56, 19);
    bypassButton.setBounds (342, 8, 56, 19);
    presetBox.setBounds (406, 7, 146, 21);

    // MODE 1 . SPIRE: the mode and Mode 1's Dry/Wet
    modeBox.setBounds (16, 80, 122, 21);
    dryWetKnob.setBounds (45, 140, 64, 72);

    // GLOBAL: Sidechain with its meter and Ext, then Dry Source
    const int gy = kMassiveY;
    sidechainKnob.setBounds (12, gy + 22, 64, 72);
    duckArea = { 80.0f, (float) gy + 40.0f, 58.0f, 19.0f };
    extButton.setBounds (80, gy + 65, 58, 19);
    drySourceKnob.setBounds (45, gy + 110, 64, 72);

    // PREDELAY / DECAY, with Modulation and Damp in a column
    predelayKnob.setBounds (largeSlot1);
    decayKnob.setBounds (largeSlot2);
    modulationKnob.setBounds (smallTop);
    dampKnob.setBounds (smallBottom);
    lcdPredelay.setBounds  (largeSlot1.getX(), 160, 89, 22);
    lcdDecay.setBounds     (largeSlot2.getX() + 1, 160, 89, 22);
    decayEntry.setBounds (lcdDecay.getBounds());

    statusArea = { 160.0f, 198.0f, 236.0f, 52.0f };
    tempoField.setBounds (162, 224, 112, 21);
    syncButton.setBounds (282, 224, 112, 21);

    // STEREO / TONE: 2 x 2 knobs
    wideKnob.setBounds  (414, 62, 64, 72);
    colorKnob.setBounds (482, 62, 64, 72);
    shelfKnob.setBounds (414, 160, 64, 72);
    liftKnob.setBounds  (482, 160, 64, 72);

    // MODE 2 . MASSIVE REVERB: type + readout, then the four knobs
    const int my = kMassiveY;
    massiveTypeBox.setBounds (160, my + 34, 120, 21);
    lcdMassive.setBounds (160, my + 62, 120, 22);
    massiveDryWetKnob.setBounds  (288, my + 21, 64, 72);
    massiveSizeKnob.setBounds    (352, my + 21, 64, 72);
    massiveDensityKnob.setBounds (416, my + 21, 64, 72);
    massiveColorKnob.setBounds   (480, my + 21, 64, 72);

    // PINGPONG DELAY: on, note value and readout, normal/dotted/triplet below, then the knobs
    const int dy = kDelayY;
    delayOnButton.setBounds (160, dy + 33, 48, 21);
    delayTimeBox.setBounds (214, dy + 33, 64, 21);
    lcdDelay.setBounds (286, dy + 32, 128, 22);
    for (int i = 0; i < 3; ++i)
        delayTypeButtons[i].setBounds (160 + i * 50, dy + 63, 50, 21);
    delayFeedbackKnob.setBounds (420, dy + 21, 64, 72);
    delayMixKnob.setBounds (484, dy + 21, 64, 72);

    // strip + bottom bar
    visualizer.setBounds (8, kDelayY + 101, 544, 32);
    helpButton.setBounds (kWidth - 26, kBottomBarY + 6, 20, 20);
    helpOverlay.setBounds (getLocalBounds());
}

void LeadVerbPanel::showDecayEntry()
{
    const float seconds = lv::decayKnobToSeconds (processor.apvts.getRawParameterValue (lv::ParamIDs::decay)->load(), currentModeIndex(), dampValue());
    decayEntry.setText (seconds >= lv::kEndlessSeconds ? String ("inf") : String (seconds, 2) + " s", dontSendNotification);
    decayEntry.setVisible (true);
    decayEntry.toFront (true);
    decayEntry.grabKeyboardFocus();
    decayEntry.selectAll();
}

void LeadVerbPanel::commitDecayEntry()
{
    const float seconds = lv::parseDecayText (decayEntry.getText());
    if (seconds > 0.0f)
        if (auto* param = processor.apvts.getParameter (lv::ParamIDs::decay))
        {
            // times outside what the mode can reach are clamped to its shortest / longest
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (lv::secondsToDecayKnob (seconds, currentModeIndex(), dampValue())));
            param->endChangeGesture();
        }
    decayEntry.setVisible (false);
}

bool LeadVerbPanel::syncOn() const
{
    return processor.apvts.getRawParameterValue (lv::ParamIDs::sync)->load() > 0.5f;
}

float LeadVerbPanel::dampValue() const
{
    return processor.apvts.getRawParameterValue (lv::ParamIDs::damp)->load();
}

bool LeadVerbPanel::massiveActive() const
{
    return processor.apvts.getRawParameterValue (lv::ParamIDs::section)->load() > 0.5f;
}

int LeadVerbPanel::massiveType() const
{
    return processor.apvts.getRawParameterValue (lv::ParamIDs::massiveType)->load() > 0.5f ? lv::MassiveReverb::Small : lv::MassiveReverb::Reverb;
}

/** The inactive section's controls are dimmed (but still editable). */
void LeadVerbPanel::updateSectionDimming()
{
    shownSection = massiveActive() ? 1 : 0;
    const float mode1Alpha = shownSection == 0 ? 1.0f : 0.38f, mode2Alpha = shownSection == 1 ? 1.0f : 0.38f;

    Component* mode1[] = { &modeBox, &dryWetKnob, &predelayKnob, &decayKnob,
                           &modulationKnob, &dampKnob, &lcdPredelay, &lcdDecay, &tempoField, &syncButton,
                           &wideKnob, &colorKnob, &shelfKnob, &liftKnob };
    Component* mode2[] = { &massiveTypeBox, &lcdMassive, &massiveDryWetKnob, &massiveSizeKnob, &massiveDensityKnob, &massiveColorKnob };
    for (auto* c : mode1) c->setAlpha (mode1Alpha);
    for (auto* c : mode2) c->setAlpha (mode2Alpha);
    repaint();
}

int LeadVerbPanel::currentModeIndex() const
{
    return jlimit (0, lv::NumReverbModes - 1, (int) processor.apvts.getRawParameterValue (lv::ParamIDs::mode)->load());
}

//==============================================================================
void LeadVerbPanel::createBackground()
{
    background = Image (Image::RGB, kWidth * 2, kHeight * 2, true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (2.0f));

    g.setGradientFill (ColourGradient (Colour (0xff3b3e42), 0.0f, 0.0f, Colour (0xff1e2023), 0.0f, (float) kHeight, false));
    g.fillAll();

    // brushed metal: faint full-width lines plus random short streaks
    Random rng (0x7a11);
    for (int y = 0; y < kHeight; ++y)
    {
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (rng.nextFloat() * 0.035f));
        g.fillRect (0, y, kWidth, 1);
    }
    for (int i = 0; i < 1600; ++i)
    {
        const int x = rng.nextInt (kWidth), y = rng.nextInt (kHeight), len = 20 + rng.nextInt (140);
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.015f + rng.nextFloat() * 0.03f));
        g.fillRect (x, y, len, 1);
    }

    // soft vignette
    ColourGradient vignette (Colours::transparentBlack, kWidth * 0.5f, kHeight * 0.45f,
                             Colours::black.withAlpha (0.35f), 0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillAll();
}

void LeadVerbPanel::drawWordmark (Graphics& g, Rectangle<float> area)
{
    // Stylised lowercase wordmark: chrome "lead", blue "verb", and a swoosh.
    const float baseline = area.getBottom() - 7.0f;

    GlyphArrangement main;
    main.addLineOfText (Font (FontOptions (27.0f, Font::bold | Font::italic)), "lead", area.getX(), baseline);
    Path mainPath;
    main.createPath (mainPath);
    const auto mainBounds = mainPath.getBounds();

    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillPath (mainPath, AffineTransform::translation (1.0f, 1.5f));
    ColourGradient chrome (Colour (0xffffffff), 0.0f, mainBounds.getY(), Colour (0xff8c949c), 0.0f, mainBounds.getBottom(), false);
    chrome.addColour (0.5, Colour (0xffd5dade));
    chrome.addColour (0.55, Colour (0xffa4acb3));
    g.setGradientFill (chrome);
    g.fillPath (mainPath);

    GlyphArrangement second;
    second.addLineOfText (Font (FontOptions (19.0f, Font::plain)), "verb", mainBounds.getRight() + 6.0f, baseline);
    Path secondPath;
    second.createPath (secondPath);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillPath (secondPath, AffineTransform::translation (1.0f, 1.0f));
    g.setColour (VirusColours::blue);
    g.fillPath (secondPath);

    // swoosh under the wordmark
    Path swoosh;
    const float sx = mainBounds.getX() + 2.0f, sy = baseline + 3.5f;
    const float ex = secondPath.getBounds().getRight();
    swoosh.startNewSubPath (sx, sy);
    swoosh.quadraticTo ((sx + ex) * 0.5f, sy + 3.5f, ex + 6.0f, sy - 2.0f);
    g.setGradientFill (ColourGradient (VirusColours::blue.withAlpha (0.0f), sx, sy, VirusColours::blue, ex, sy, false));
    g.strokePath (swoosh, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

void LeadVerbPanel::paint (Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());

    // --- top strip ----------------------------------------------------------------
    auto top = Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kTopStripH);
    g.setGradientFill (ColourGradient (Colour (0xff4a4e53), 0.0f, 0.0f, Colour (0xff2b2e32), 0.0f, top.getBottom(), false));
    g.fillRect (top);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine (0, 0.0f, (float) kWidth);
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kTopStripH - 1, 0.0f, (float) kWidth);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (kTopStripH, 0.0f, (float) kWidth);

    drawWordmark (g, { 14.0f, 0.0f, 260.0f, (float) kTopStripH });

    // --- panels ---------------------------------------------------------------------
    CustomLookAndFeel::drawPanel (g, mode1Panel, "Mode 1 - Spire");
    // dividers between its three areas
    for (float x : { 149.0f, 405.0f })
    {
        g.setColour (VirusColours::bevelDark);
        g.drawVerticalLine ((int) x, 66.0f, 248.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawVerticalLine ((int) x + 1, 66.0f, 248.0f);
    }
    CustomLookAndFeel::drawPanel (g, globalPanel, "Global");
    CustomLookAndFeel::drawPanel (g, massivePanel, "Mode 2 - Massive Reverb");
    CustomLookAndFeel::drawPanel (g, delayPanel,  "PingPong Delay");

    const float dy = (float) kDelayY;
    CustomLookAndFeel::drawSmallLabel (g, "Delay", { 160.0f, dy + 20.0f, 48.0f, 12.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Time",  { 214.0f, dy + 20.0f, 64.0f, 12.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Echo time", { 286.0f, dy + 20.0f, 128.0f, 12.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Type", { 160.0f, (float) kMassiveY + 20.0f, 120.0f, 12.0f }, Justification::centredLeft);

    CustomLookAndFeel::drawSmallLabel (g, "Mode",       { 16.0f, 66.0f, 122.0f, 12.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Ducking", { 80.0f, duckArea.getY() - 14.0f, 58.0f, 12.0f }, Justification::centredLeft);

    // Global panel: divider between Sidechain and Dry Source
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kMassiveY + 104, 16.0f, 138.0f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine (kMassiveY + 105, 16.0f, 138.0f);

    // right panel: divider between the two rows of knobs
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (148, 416.0f, 544.0f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine (149, 416.0f, 544.0f);

    // centre status row: separator, tempo and the predelay sync button
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine ((int) statusArea.getY(), statusArea.getX(), statusArea.getRight());
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) statusArea.getY() + 1, statusArea.getX(), statusArea.getRight());

    CustomLookAndFeel::drawSmallLabel (g, "Tempo",  { 162.0f, 208.0f, 112.0f, 12.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Predelay Sync", { 282.0f, 208.0f, 112.0f, 12.0f }, Justification::centredLeft);

    // sidechain gain-reduction meter: a blue LCD bar growing with the ducking depth
    CustomLookAndFeel::drawInsetBox (g, duckArea);
    g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
    if (shownSidechainOn && shownExternal && ! shownKeyConnected)
    {
        g.setColour (VirusColours::labelText);
        g.drawText ("NO INPUT", duckArea, Justification::centred);
    }
    else if (shownSidechainOn)
    {
        const float fraction = jlimit (0.0f, 1.0f, shownDuckDb / lv::Ducker::kMaxReductionDb);
        if (fraction > 0.0f)
        {
            auto bar = duckArea.reduced (2.0f);
            g.setGradientFill (ColourGradient (VirusColours::lcdTop, 0.0f, bar.getY(), VirusColours::lcdBottom, 0.0f, bar.getBottom(), false));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fraction), 2.0f);
        }
        g.setColour (Colours::white);
        g.drawText (shownDuckDb < 0.05f ? String ("0.0 dB") : "-" + String (shownDuckDb, 1) + " dB", duckArea, Justification::centred);
    }
    else
    {
        g.setColour (VirusColours::labelText.withAlpha (0.45f));
        g.drawText ("OFF", duckArea, Justification::centred);
    }

    // the inactive reverb section sits in shadow
    g.setColour (Colours::black.withAlpha (0.42f));
    if (shownSection == 1)
        g.fillRoundedRectangle (mode1Panel, 5.0f);
    else if (shownSection == 0)
        g.fillRoundedRectangle (massivePanel, 5.0f);

    // --- bottom bar -----------------------------------------------------------------
    auto bar = Rectangle<float> (0.0f, (float) kBottomBarY, (float) kWidth, (float) (kHeight - kBottomBarY));
    g.setGradientFill (ColourGradient (Colour (0xff17191b), 0.0f, bar.getY(), Colour (0xff0c0d0e), 0.0f, bar.getBottom(), false));
    g.fillRect (bar);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (kBottomBarY, 0.0f, (float) kWidth);

    // "REVERB" title tab at the left
    auto tab = Rectangle<float> (8.0f, bar.getY() + 7.0f, 56.0f, 18.0f);
    g.setGradientFill (ColourGradient (VirusColours::headerTop, 0.0f, tab.getY(), VirusColours::headerBottom, 0.0f, tab.getBottom(), false));
    g.fillRoundedRectangle (tab, 3.0f);
    g.setColour (VirusColours::bevelDark);
    g.drawRoundedRectangle (tab, 3.0f, 1.0f);
    g.setColour (VirusColours::headerText);
    g.setFont (CustomLookAndFeel::uiFont (10.5f, true));
    g.drawText ("REVERB", tab, Justification::centred);

    // centred product name with a thin groove
    auto inset = bar.reduced (70.0f, 7.0f);
    CustomLookAndFeel::drawInsetBox (g, inset, 3.0f);
    g.setFont (CustomLookAndFeel::uiFont (13.0f, true));
    g.setColour (Colour (0xffc8cdd2));
    g.drawText ("LEAD VERB", inset, Justification::centred);
}

//==============================================================================
void LeadVerbPanel::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jlimit (0.0, 0.1, (now - lastTimerMs) * 0.001);
    lastTimerMs = now;

    auto& apvts = processor.apvts;
    auto& gui = processor.gui;
    auto raw = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int mode = currentModeIndex();
    const bool sync = syncOn();

    // double-click Decay: Spire Plate1 returns to the capture's 2.95 s (Decay 310), every
    // other mode to Spire's default (635.7)
    decayKnob.setDefaultValue (mode == lv::ModePlate1 ? 310.0 : 635.7);

    if (shownSection != (massiveActive() ? 1 : 0))
        updateSectionDimming();

    // Show the knob's current value (not the smoothed audio value) so the readouts react instantly.
    const float predelayMs = sync ? lv::syncedPredelayMs (raw (lv::ParamIDs::predelay), gui.bpm.load())
                                  : lv::predelayKnobToMs (raw (lv::ParamIDs::predelay));
    const float decaySeconds = lv::decayKnobToSeconds (raw (lv::ParamIDs::decay), mode, raw (lv::ParamIDs::damp));

    lcdPredelay.setContent ("PREDELAY", lv::shortPredelay (raw (lv::ParamIDs::predelay), sync));
    lcdDecay.setContent ("DECAY", lv::shortDecay (decaySeconds));

    const int delayNote = (int) raw (lv::ParamIDs::delayTime), delayType = (int) raw (lv::ParamIDs::delayType);
    const float delayMs = 1000.0f * lv::syncedDelaySeconds (delayNote, delayType, gui.bpm.load());
    lcdDelay.setContent (lv::shortDelayNote (delayNote, delayType),
                         delayMs < 1000.0f ? String (roundToInt (delayMs)) + "ms" : lv::shortSeconds (delayMs * 0.001f));
    lcdDelay.setDimmed (raw (lv::ParamIDs::delayOn) < 0.5f);

    // Mode 2 readout: Massive's tail length and its fixed pre-delay
    const int type = massiveType();
    const float massiveTail = lv::massiveTailSeconds (raw (lv::ParamIDs::massiveSize), type);
    lcdMassive.setContent (type == lv::MassiveReverb::Small ? "SMALL" : "REVERB", lv::shortSeconds (massiveTail));

    ReverbVisualizer::State vs;
    if (massiveActive())
    {
        vs.mode         = type == lv::MassiveReverb::Small ? lv::ModeMassiveSmall : lv::ModeMassiveReverb;
        vs.predelayMs   = gui.massivePredelayMs.load();
        vs.decaySeconds = massiveTail;
        vs.width        = 1.0f;
    }
    else
    {
        vs.mode         = mode;
        vs.predelayMs   = predelayMs;
        vs.decaySeconds = std::min (decaySeconds, 99.0f);
        vs.width        = lv::wideKnobToWidth (raw (lv::ParamIDs::wide));
    }
    visualizer.update (vs, dt);

    tempoField.setTempo (gui.bpm.load(), gui.hostTempo.load());

    const bool sidechainOn = raw (lv::ParamIDs::sidechain) >= 0.05f;
    const float duckDb = std::round (gui.duckingDb.load() * 10.0f) / 10.0f;
    const bool external = raw (lv::ParamIDs::externalSidechain) > 0.5f;
    const bool keyConnected = gui.externalKeyConnected.load();
    if (sidechainOn != shownSidechainOn || std::abs (duckDb - shownDuckDb) > 0.05f
        || external != shownExternal || keyConnected != shownKeyConnected)
    {
        shownSidechainOn = sidechainOn;
        shownExternal = external;
        shownKeyConnected = keyConnected;
        shownDuckDb = duckDb;
        repaint (duckArea.toNearestInt().expanded (2));
    }

    const int program = processor.getCurrentProgram();
    if (program != lastShownProgram)
    {
        lastShownProgram = program;
        presetBox.setSelectedId (program + 1, dontSendNotification);
    }
}

//==============================================================================
// Editor: scales the panel to the window, keeping the aspect ratio
//==============================================================================
LeadVerbAudioProcessorEditor::LeadVerbAudioProcessorEditor (LeadVerbAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    // Read the saved size first: setting the limits below resizes the (still empty)
    // window, and resized() would overwrite the saved scale.
    const float scale = jlimit (0.75f, 2.5f, processor.uiScale.load());

    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);

    constexpr int w = LeadVerbPanel::kWidth, h = LeadVerbPanel::kHeight;
    setResizable (true, true);
    setResizeLimits (w * 3 / 4, h * 3 / 4, w * 5 / 2, h * 5 / 2);
    getConstrainer()->setFixedAspectRatio ((double) w / (double) h);

    // reopen at the size the user left it
    setSize (roundToInt ((float) w * scale), roundToInt ((float) h * scale));
}

LeadVerbAudioProcessorEditor::~LeadVerbAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void LeadVerbAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) LeadVerbPanel::kWidth;
    panel.setBounds (0, 0, LeadVerbPanel::kWidth, LeadVerbPanel::kHeight);
    panel.setTransform (AffineTransform::scale (scale));
    processor.uiScale.store (scale);
}
