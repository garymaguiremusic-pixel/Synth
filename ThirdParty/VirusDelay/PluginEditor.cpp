#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;

namespace
{
    constexpr int kWidth = VirusDelayPanel::kBaseWidth, kHeight = VirusDelayPanel::kBaseHeight;
    constexpr int kTopStripH = 40, kBottomBarY = 446;

    // Rows
    constexpr int kTabsY = 46, kTabsH = 28;
    constexpr float kPanelsY = 80.0f, kPanelsH = 226.0f;
    constexpr float kOutputY = 312.0f, kOutputH = 88.0f;

    // Centre panel slots
    const Rectangle<int> largeSlot1 { 200, 104, 116, 116 };
    const Rectangle<int> largeSlot2 { 332, 104, 116, 116 };
    const Rectangle<int> smallSlot  { 454, 128, 64, 84 };

    // Left panel rows
    const Rectangle<int> leftRow1 { 20, 118, 146, 24 };
    const Rectangle<int> leftRow2 { 20, 164, 146, 24 };
    const Rectangle<int> leftKnob { 54, 200, 78, 98 };
}

//==============================================================================
VirusDelayPanel::VirusDelayPanel (VirusDelayAudioProcessor& p)
    : processor (p),
      tempoField (*p.apvts.getParameter (vd::ParamIDs::bpm))
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    setSize (kWidth, kHeight);
    createBackground();

    auto& apvts = processor.apvts;

    // Mode tabs ---------------------------------------------------------------------
    const auto modeNames = vd::getModeNames();
    for (int i = 0; i < vd::NumDelayModes; ++i)
    {
        auto& tab = modeTabs[(size_t) i];
        tab.setButtonText (modeNames[i]);
        tab.setTooltip (modeNames[i]);
        tab.onClick = [this, i] { processor.setParameterValue (vd::ParamIDs::mode, (float) i); };
        addAndMakeVisible (tab);
    }

    // Dropdowns ------------------------------------------------------------------
    setupCombo (ratioBox,     vd::getClassicSubNames (vd::ClassicKind::PingPong), "Ping Pong ratio (left : right delay time)");
    setupCombo (patternBox,   vd::getClassicSubNames (vd::ClassicKind::Pattern),  "Pattern: the two alternating repeat lengths");
    setupCombo (clockBox,     vd::getClockNames(),     "Tempo sync division (Off = free Time)");
    setupCombo (clockLeftBox, vd::getTapeClockNames(), "Left tape delay time, synced to tempo");
    setupCombo (clockRightBox, vd::getTapeClockNames(), "Right tape delay time, synced to tempo");
    setupCombo (tapeClockBox, vd::getTapeClockNames(), "Tape delay time, synced to tempo");
    setupCombo (shapeBox,     vd::getShapeNames(),     "LFO waveform");

    const std::pair<ComboBox*, const char*> combos[] =
    {
        { &ratioBox, vd::ParamIDs::pingPongRatio }, { &patternBox, vd::ParamIDs::patternType },
        { &clockBox, vd::ParamIDs::clock }, { &clockLeftBox, vd::ParamIDs::clockLeft },
        { &clockRightBox, vd::ParamIDs::clockRight }, { &tapeClockBox, vd::ParamIDs::tapeClock },
        { &shapeBox, vd::ParamIDs::shape },
    };
    for (auto& [box, id] : combos)
        comboAttachments.push_back (std::make_unique<ComboAttachment> (apvts, id, *box));

    for (int i = 0; i < vd::numFactoryPresets; ++i)
        presetBox.addItem (String (i + 1).paddedLeft ('0', 2) + "  " + vd::factoryPresets[i].name, i + 1);
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
        { sendKnob,       vd::ParamIDs::send,       vd::describeSend },
        { feedbackKnob,   vd::ParamIDs::feedback,   vd::describeFeedback },
        { timeKnob,       vd::ParamIDs::time,       vd::describeTime },
        { colorKnob,      vd::ParamIDs::color,      vd::describeColor },
        { rateKnob,       vd::ParamIDs::rate,       vd::describeRate },
        { depthKnob,      vd::ParamIDs::depth,      vd::describeDepth },
        { centerKnob,     vd::ParamIDs::centerFreq, vd::describeCenter },
        { bandwidthKnob,  vd::ParamIDs::bandwidth,  vd::describeBandwidth },
        { modulationKnob, vd::ParamIDs::modulation, vd::describeModulation },
    };

    for (auto& k : knobs)
    {
        k.knob.describeValue = k.describe;
        knobAttachments.push_back (std::make_unique<KnobAttachment> (*apvts.getParameter (k.id), k.knob));
        addChildComponent (k.knob);
    }

    // Spire knobs
    const KnobSetup spireKnobs[] =
    {
        { spireFeedbackKnob, vd::ParamIDs::spireFeedback, vd::describeSpireFeedback },
        { spireRateKnob,     vd::ParamIDs::spireRate,     vd::describeRate },
        { spireModulateKnob, vd::ParamIDs::spireModulate, vd::describeSpireModulate },
        { spireColorKnob,    vd::ParamIDs::spireColor,    vd::describeSpireColor },
        { spireDryWetKnob,   vd::ParamIDs::spireDryWet,   vd::describeSpireDryWet },
        { spireDelayLKnob,   vd::ParamIDs::spireDelayL,   nullptr },
        { spireDelayRKnob,   vd::ParamIDs::spireDelayR,   nullptr },
        { spireWideKnob,     vd::ParamIDs::spireWide,     nullptr },
    };
    for (auto& k : spireKnobs)
    {
        if (k.describe != nullptr)
            k.knob.describeValue = k.describe;
        knobAttachments.push_back (std::make_unique<KnobAttachment> (*apvts.getParameter (k.id), k.knob));
        addChildComponent (k.knob);
    }
    spireDelayLKnob.describeValue = [this] (float v) { return spireDelayText (v); };
    spireDelayRKnob.describeValue = [this] (float v) { return spireDelayText (v); };
    spireWideKnob.describeValue   = [this] (float v) { return vd::describeSpireWide (v, spirePingPongOn()); };

    // Output section (all modes)
    const KnobSetup outputKnobs[] =
    {
        { drySourceKnob,  vd::ParamIDs::drySource,  vd::describeDrySource },
        { widthKnob,      vd::ParamIDs::width,      vd::describeWidth },
        { duckAmountKnob, vd::ParamIDs::duckAmount, vd::describeDuckAmount },
        { moveRateKnob,   vd::ParamIDs::moveRate,   vd::describeRate },
        { moveDepthKnob,  vd::ParamIDs::moveDepth,  vd::describeMoveDepth },
        { moveMixKnob,    vd::ParamIDs::moveMix,    vd::describeMoveMix },
    };
    for (auto& k : outputKnobs)
    {
        k.knob.describeValue = k.describe;
        knobAttachments.push_back (std::make_unique<KnobAttachment> (*apvts.getParameter (k.id), k.knob));
        addAndMakeVisible (k.knob);
    }
    addAndMakeVisible (movePreview);

    // Rate knobs are tempo-synced: hover shows the note value, the tooltip adds Hz.
    for (VirusKnob* k : { &rateKnob, &spireRateKnob, &moveRateKnob })
    {
        k->describeValue = [this] (float v) { return vd::describeRateAtTempo (v, processor.gui.bpm.load()); };
        k->displayValue  = [] (double v) { return vd::describeRate ((float) v); };
    }
    for (VirusKnob* k : { &spireDelayLKnob, &spireDelayRKnob })
        k->displayValue = [this] (double v)
        {
            return spireSyncOn() ? vd::getSpireDivisionNames()[vd::spireKnobToDivision ((float) v)]
                                 : vd::formatMs (vd::spireKnobToFreeMs ((float) v));
        };

    // Buttons ----------------------------------------------------------------------
    const std::tuple<TextButton*, const char*, const char*> buttons[] =
    {
        { &bypassButton,        vd::ParamIDs::bypass,        "Bypass the delay (dry signal only)" },
        { &spirePingPongButton, vd::ParamIDs::spirePingPong, "Ping-Pong: WIDE left of centre = cross feedback, right of centre = ping pong" },
        { &spireSyncButton,     vd::ParamIDs::spireSync,     "Sync: Delay L / R follow the host tempo in note divisions" },
        { &duckButton,          vd::ParamIDs::duckOn,        "Duck: push the repeats down while the dry source plays, let them back in the gaps" },
        { &moveHPButton,        vd::ParamIDs::moveHP,        "Movement: a high-pass filter sweeping up and down over the repeats" },
        { &movePhaserButton,    vd::ParamIDs::movePhaser,    "Phaser: adds a sweeping phaser layer to the repeats" },
        { &moveFlangerButton,   vd::ParamIDs::moveFlanger,   "Flanger: adds a jet-style flanger sweep to the repeats" },
    };
    for (auto& [button, id, tip] : buttons)
    {
        button->setClickingTogglesState (true);
        button->setTooltip (tip);
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (apvts, id, *button));
        addChildComponent (*button);
    }
    for (Component* c : { (Component*) &bypassButton, (Component*) &duckButton, (Component*) &moveHPButton,
                          (Component*) &movePhaserButton, (Component*) &moveFlangerButton })
        c->setVisible (true);

    randomButton.setTooltip ("Randomise: picks a random mode and random settings for it");
    randomButton.onClick = [this] { processor.randomise(); };
    addAndMakeVisible (randomButton);

    addAndMakeVisible (lcdLeft);
    addAndMakeVisible (lcdRight);
    addAndMakeVisible (lfoPreview);
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

VirusDelayPanel::~VirusDelayPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VirusDelayPanel::setupCombo (ComboBox& box, const StringArray& items, const String& tooltip)
{
    box.addItemList (items, 1);
    box.setTooltip (tooltip);
    box.setJustificationType (Justification::centredLeft);
    addChildComponent (box);
}

//==============================================================================
void VirusDelayPanel::resized()
{
    // top bar
    randomButton.setBounds (350, 10, 84, 22);
    bypassButton.setBounds (440, 10, 70, 22);
    presetBox.setBounds (564, 9, 148, 24);

    // mode tabs
    for (int i = 0; i < vd::NumDelayModes; ++i)
        modeTabs[(size_t) i].setBounds (8 + i * 101, kTabsY, 97, kTabsH);

    // the three mode panels
    leftPanel   = { 8.0f,   kPanelsY, 170.0f, kPanelsH };
    centrePanel = { 184.0f, kPanelsY, 340.0f, kPanelsH };
    rightPanel  = { 530.0f, kPanelsY, 182.0f, kPanelsH };

    ratioBox.setBounds (leftRow1);
    patternBox.setBounds (leftRow1);
    clockLeftBox.setBounds (leftRow1);
    clockRightBox.setBounds (leftRow2);
    tapeClockBox.setBounds (leftRow1);
    sendKnob.setBounds (leftKnob);
    spirePingPongButton.setBounds (leftRow1);
    spireSyncButton.setBounds (leftRow2);
    spireDryWetKnob.setBounds (leftKnob);

    lcdLeft.setBounds  (largeSlot1.getX(), 226, largeSlot1.getWidth(), 24);
    lcdRight.setBounds (largeSlot2.getX(), 226, largeSlot2.getWidth(), 24);

    statusArea     = { 196.0f, 258.0f, 316.0f, 44.0f };
    tempoField.setBounds (200, 278, 150, 22);
    saturationArea = { 362.0f, 278.0f, 150.0f, 22.0f };

    spireDelayLKnob.setBounds (largeSlot1);
    spireDelayRKnob.setBounds (largeSlot2);
    spireFeedbackKnob.setBounds (smallSlot);

    // right panel
    rateKnob.setBounds  (542, 102, 78, 92);
    depthKnob.setBounds (622, 102, 78, 92);
    modulationKnob.setBounds (582, 102, 78, 92);
    shapeBox.setBounds (544, 214, 156, 24);
    modTypeArea = { 544.0f, 214.0f, 156.0f, 24.0f };
    lfoPreview.setBounds (544, 244, 156, 54);

    spireRateKnob.setBounds     (542, 100, 78, 90);
    spireModulateKnob.setBounds (622, 100, 78, 90);
    spireWideKnob.setBounds     (542, 204, 78, 92);
    spireColorKnob.setBounds    (622, 204, 78, 92);

    // OUTPUT row (all modes): Dry Source | Width | Duck | Movement
    dryPanel   = { 8.0f,   kOutputY, 100.0f, kOutputH };
    widthPanel = { 114.0f, kOutputY, 100.0f, kOutputH };
    duckPanel  = { 220.0f, kOutputY, 160.0f, kOutputH };
    movePanel  = { 386.0f, kOutputY, 326.0f, kOutputH };
    drySourceKnob.setBounds (22, 334, 72, 62);
    widthKnob.setBounds (128, 334, 72, 62);
    duckButton.setBounds (230, 340, 72, 24);
    duckMeterArea = { 230.0f, 372.0f, 72.0f, 8.0f };
    duckAmountKnob.setBounds (304, 334, 70, 62);
    moveHPButton.setBounds (396, 333, 84, 20);
    movePhaserButton.setBounds (396, 355, 84, 20);
    moveFlangerButton.setBounds (396, 377, 84, 20);
    moveRateKnob.setBounds (486, 334, 60, 62);
    moveDepthKnob.setBounds (546, 334, 60, 62);
    moveMixKnob.setBounds (606, 334, 60, 62);
    movePreview.setBounds (668, 338, 36, 52);

    // strip + bottom bar
    visualizer.setBounds (8, 406, 704, 34);
    helpButton.setBounds (kWidth - 28, kBottomBarY + 5, 20, 20);
    helpOverlay.setBounds (getLocalBounds());

    layoutForMode();
}

void VirusDelayPanel::layoutForMode()
{
    const bool classic = vd::isClassicMode (currentMode);
    const bool tape    = vd::isTapeMode (currentMode);
    const bool spire   = vd::isSpireMode (currentMode);
    const bool clocked = currentMode == vd::ModeTapeClocked;

    for (Component* c : { (Component*) &spireDelayLKnob, (Component*) &spireDelayRKnob, (Component*) &spireFeedbackKnob,
                          (Component*) &spireRateKnob, (Component*) &spireModulateKnob, (Component*) &spireWideKnob,
                          (Component*) &spireColorKnob, (Component*) &spireDryWetKnob,
                          (Component*) &spirePingPongButton, (Component*) &spireSyncButton })
        c->setVisible (spire);

    sendKnob.setVisible (! spire);
    feedbackKnob.setVisible (! spire);
    lfoPreview.setVisible (! spire);

    // Simple: Clock only. Ping Pong / Pattern: their selector, then Clock.
    ratioBox.setVisible (currentMode == vd::ModePingPong);
    patternBox.setVisible (currentMode == vd::ModePattern);
    clockBox.setVisible (classic);
    clockBox.setBounds (currentMode == vd::ModeSimple ? leftRow1 : leftRow2);

    clockLeftBox.setVisible (clocked);
    clockRightBox.setVisible (clocked);
    tapeClockBox.setVisible (tape && ! clocked);

    timeKnob.setVisible (classic);
    colorKnob.setVisible (classic);
    rateKnob.setVisible (classic);
    depthKnob.setVisible (classic);
    shapeBox.setVisible (classic);

    centerKnob.setVisible (tape);
    bandwidthKnob.setVisible (tape);
    modulationKnob.setVisible (tape);

    if (classic)
    {
        timeKnob.setBounds (largeSlot1);
        feedbackKnob.setBounds (largeSlot2);
        colorKnob.setBounds (smallSlot);
    }
    else if (tape)
    {
        feedbackKnob.setBounds (largeSlot1);
        centerKnob.setBounds (largeSlot2);
        bandwidthKnob.setBounds (smallSlot);
    }

    for (int i = 0; i < vd::NumDelayModes; ++i)
        modeTabs[(size_t) i].setToggleState (i == currentMode, dontSendNotification);

    repaint();
}

void VirusDelayPanel::setActive (Component& c, bool active)
{
    if (c.isEnabled() != active)
    {
        c.setEnabled (active);
        c.setAlpha (active ? 1.0f : 0.4f);
    }
}

bool VirusDelayPanel::spireSyncOn() const
{
    return processor.apvts.getRawParameterValue (vd::ParamIDs::spireSync)->load() > 0.5f;
}

bool VirusDelayPanel::spirePingPongOn() const
{
    return processor.apvts.getRawParameterValue (vd::ParamIDs::spirePingPong)->load() > 0.5f;
}

String VirusDelayPanel::spireDelayText (float knobValue) const
{
    return vd::describeSpireDelay (knobValue, spireSyncOn(), processor.gui.bpm.load());
}

void VirusDelayPanel::updateActiveStates()
{
    // Only the Time knob is ever inactive: in Simple / Ping Pong / Pattern while Clock is on.
    const bool timeActive = ! vd::isClassicMode (currentMode)
                         || vd::isTimeActive ((int) processor.apvts.getRawParameterValue (vd::ParamIDs::clock)->load());
    setActive (timeKnob, timeActive);
    lcdLeft.setDimmed (vd::isClassicMode (currentMode) && ! timeActive);
    lcdRight.setDimmed (false);
    timeKnob.inactiveHint = "Inactive while CLOCK is on: the time follows the tempo";
}

//==============================================================================
void VirusDelayPanel::createBackground()
{
    // Rendered at 2x so it stays sharp when the window is enlarged.
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

void VirusDelayPanel::drawWordmark (Graphics& g, Rectangle<float> area)
{
    // Original stylised lowercase wordmark: chrome "virus", blue "delay", and a swoosh.
    const float baseline = area.getBottom() - 7.0f;

    GlyphArrangement main;
    main.addLineOfText (Font (FontOptions (27.0f, Font::bold | Font::italic)), "virus", area.getX(), baseline);
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
    second.addLineOfText (Font (FontOptions (19.0f, Font::plain)), "delay", mainBounds.getRight() + 6.0f, baseline);
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

void VirusDelayPanel::paint (Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());

    // --- top bar --------------------------------------------------------------------
    auto top = Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kTopStripH);
    g.setGradientFill (ColourGradient (Colour (0xff4a4e53), 0.0f, 0.0f, Colour (0xff2b2e32), 0.0f, top.getBottom(), false));
    g.fillRect (top);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine (0, 0.0f, (float) kWidth);
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kTopStripH - 1, 0.0f, (float) kWidth);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (kTopStripH, 0.0f, (float) kWidth);

    drawWordmark (g, { 16.0f, 2.0f, 260.0f, (float) kTopStripH - 2.0f });
    CustomLookAndFeel::drawSmallLabel (g, "Preset", { 512.0f, 10.0f, 46.0f, 22.0f }, Justification::centredRight);

    // --- mode tabs groove -------------------------------------------------------------
    CustomLookAndFeel::drawInsetBox (g, { 5.0f, (float) kTabsY - 3.0f, (float) kWidth - 10.0f, (float) kTabsH + 6.0f }, 4.0f);

    // --- the selected mode's panels --------------------------------------------------------
    const bool classic = vd::isClassicMode (currentMode);
    const bool spire   = vd::isSpireMode (currentMode);
    const bool clocked = currentMode == vd::ModeTapeClocked;

    CustomLookAndFeel::drawPanel (g, leftPanel,   vd::getModeNames()[jmax (0, currentMode)]);
    CustomLookAndFeel::drawPanel (g, centrePanel, classic ? "Time / Feedback" : (spire ? "Delay L / Delay R" : "Feedback / Filter"));
    CustomLookAndFeel::drawPanel (g, rightPanel,  spire ? "Modulation / Stereo" : "Modulation");

    String row1, row2;
    switch (currentMode)
    {
        case vd::ModeSimple:      row1 = "Clock"; break;
        case vd::ModePingPong:    row1 = "Ratio";         row2 = "Clock"; break;
        case vd::ModePattern:     row1 = "Pattern";       row2 = "Clock"; break;
        case vd::ModeTapeClocked: row1 = "Clock Left";    row2 = "Clock Right"; break;
        case vd::ModeSpire:       row1 = "Feedback Mode"; row2 = "Tempo Sync"; break;
        default:                  row1 = "Clock"; break;
    }
    CustomLookAndFeel::drawSmallLabel (g, row1, { 20.0f, 104.0f, 146.0f, 12.0f }, Justification::centredLeft);
    if (row2.isNotEmpty())
        CustomLookAndFeel::drawSmallLabel (g, row2, { 20.0f, 150.0f, 146.0f, 12.0f }, Justification::centredLeft);
    ignoreUnused (clocked);

    if (spire)
    {
        // knobs carry their own labels; just a divider between the two rows
        g.setColour (VirusColours::bevelDark);
        g.drawHorizontalLine (198, 544.0f, 700.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine (199, 544.0f, 700.0f);
    }
    else if (classic)
    {
        CustomLookAndFeel::drawSmallLabel (g, "Shape", { 544.0f, 200.0f, 156.0f, 12.0f }, Justification::centredLeft);
    }
    else
    {
        CustomLookAndFeel::drawSmallLabel (g, "Type", { 544.0f, 200.0f, 156.0f, 12.0f }, Justification::centredLeft);
        CustomLookAndFeel::drawInsetBox (g, modTypeArea);
        g.setFont (CustomLookAndFeel::uiFont (12.5f, true));
        g.setColour (Colours::white);
        g.drawText (currentMode == vd::ModeTapeDoppler ? "Doppler" : "Wow & Flutter",
                    modTypeArea.reduced (6.0f, 0.0f), Justification::centredLeft);
    }

    // centre status row: separator, tempo and saturation indicator
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine ((int) statusArea.getY(), statusArea.getX(), statusArea.getRight());
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) statusArea.getY() + 1, statusArea.getX(), statusArea.getRight());

    CustomLookAndFeel::drawSmallLabel (g, "Tempo", { 200.0f, 264.0f, 150.0f, 12.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Loop",  saturationArea.withY (264.0f).withHeight (12.0f), Justification::centredLeft);

    CustomLookAndFeel::drawInsetBox (g, saturationArea);
    g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
    if (saturationLit)
    {
        g.setColour (VirusColours::red.withAlpha (0.25f));
        g.fillRoundedRectangle (saturationArea.reduced (2.0f), 2.0f);
        g.setColour (VirusColours::red);
    }
    else
    {
        g.setColour (Colour (0xff5a2a28));
    }
    g.drawText ("SATURATION", saturationArea, Justification::centred);

    // --- output row (all modes) -------------------------------------------------------
    CustomLookAndFeel::drawPanel (g, dryPanel,   "Dry Source");
    CustomLookAndFeel::drawPanel (g, widthPanel, "Width");
    CustomLookAndFeel::drawPanel (g, duckPanel,  "Duck");
    CustomLookAndFeel::drawPanel (g, movePanel,  "Movement");

    // duck gain-reduction meter (fills from the left as the repeats are pushed down)
    CustomLookAndFeel::drawInsetBox (g, duckMeterArea, 2.0f);
    const float reduction = jlimit (0.0f, 1.0f, -duckReductionShown / 36.0f);
    if (reduction > 0.001f)
    {
        g.setColour (VirusColours::blue);
        g.fillRoundedRectangle (duckMeterArea.reduced (1.5f).withWidth ((duckMeterArea.getWidth() - 3.0f) * reduction), 1.5f);
    }

    // --- bottom bar -----------------------------------------------------------------
    auto bar = Rectangle<float> (0.0f, (float) kBottomBarY, (float) kWidth, (float) (kHeight - kBottomBarY));
    g.setGradientFill (ColourGradient (Colour (0xff17191b), 0.0f, bar.getY(), Colour (0xff0c0d0e), 0.0f, bar.getBottom(), false));
    g.fillRect (bar);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (kBottomBarY, 0.0f, (float) kWidth);

    // "DELAY" title tab at the left
    auto tab = Rectangle<float> (8.0f, bar.getY() + 6.0f, 56.0f, 18.0f);
    g.setGradientFill (ColourGradient (VirusColours::headerTop, 0.0f, tab.getY(), VirusColours::headerBottom, 0.0f, tab.getBottom(), false));
    g.fillRoundedRectangle (tab, 3.0f);
    g.setColour (VirusColours::bevelDark);
    g.drawRoundedRectangle (tab, 3.0f, 1.0f);
    g.setColour (VirusColours::headerText);
    g.setFont (CustomLookAndFeel::uiFont (10.5f, true));
    g.drawText ("DELAY", tab, Justification::centred);

    // centred product name with a thin groove
    auto inset = bar.reduced (90.0f, 6.0f);
    CustomLookAndFeel::drawInsetBox (g, inset, 3.0f);
    g.setFont (CustomLookAndFeel::uiFont (13.0f, true));
    g.setColour (Colour (0xffc8cdd2));
    g.drawText ("VIRUS DELAY", inset, Justification::centred);
}

//==============================================================================
void VirusDelayPanel::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jlimit (0.0, 0.1, (now - lastTimerMs) * 0.001);
    lastTimerMs = now;

    auto& apvts = processor.apvts;
    auto& gui = processor.gui;
    auto raw = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int mode = jlimit (0, vd::NumDelayModes - 1, (int) raw (vd::ParamIDs::mode));
    if (mode != currentMode)
    {
        currentMode = mode;
        layoutForMode();
    }
    updateActiveStates();

    const bool tape = vd::isTapeMode (mode);
    const float fbValue = raw (vd::ParamIDs::feedback);

    if (vd::isSpireMode (mode))
    {
        auto shortDelay = [this] (float v)
        {
            if (spireSyncOn())
                return vd::getSpireDivisionNames()[vd::spireKnobToDivision (v)];
            const float ms = vd::spireKnobToFreeMs (v);
            return ms < 1000.0f ? String (roundToInt (ms)) + "ms" : String (ms / 1000.0f, 2) + "s";
        };
        lcdLeft.setContent ("DELAY L", shortDelay (raw (vd::ParamIDs::spireDelayL)));
        lcdRight.setContent ("DELAY R", shortDelay (raw (vd::ParamIDs::spireDelayR)));
    }
    else if (tape)
    {
        lcdLeft.setContent ("FEEDBACK", vd::formatKnobValue (fbValue));
        lcdRight.setContent ("CENTER", vd::formatKnobValue (raw (vd::ParamIDs::centerFreq)));
        lfoPreview.setState (mode == vd::ModeTapeDoppler ? LfoPreview::Kind::Doppler : LfoPreview::Kind::TapeWowFlutter,
                             0, gui.tapeModPhase.load(), raw (vd::ParamIDs::modulation) / 100.0f);
    }
    else
    {
        lcdLeft.setContent ("TIME", vd::formatKnobValue (raw (vd::ParamIDs::time)));
        lcdRight.setContent ("FEEDBACK", vd::formatKnobValue (fbValue));
        lfoPreview.setState (LfoPreview::Kind::Lfo, (int) raw (vd::ParamIDs::shape),
                             gui.lfoPhase.load(), raw (vd::ParamIDs::depth) / 100.0f);
    }

    ModeVisualizer::State vs;
    vs.mode         = mode;
    vs.classicType  = vd::classicTypeFor (mode, (int) raw (vd::ParamIDs::pingPongRatio), (int) raw (vd::ParamIDs::patternType));
    vs.baseDelayMs  = gui.baseDelayMs.load();
    vs.feedback     = vd::feedbackToGain (fbValue);
    vs.tapeDelayMsL = gui.tapeDelayMsL.load();
    vs.tapeDelayMsR = gui.tapeDelayMsR.load();
    vs.tapeSpeed    = gui.tapeSpeed.load();
    auto spireMs = [&] (float knob)
    {
        return spireSyncOn() ? (float) std::min ((double) vd::kSpireMaxDelayMs, 60000.0 / std::max (1.0f, gui.bpm.load())
                                                     * vd::spireDivisionInBeats (vd::spireKnobToDivision (knob)))
                             : vd::spireKnobToFreeMs (knob);
    };
    vs.spireDelayMsL = spireMs (raw (vd::ParamIDs::spireDelayL));
    vs.spireDelayMsR = spireMs (raw (vd::ParamIDs::spireDelayR));
    vs.spireFeedback = vd::spireFeedbackToGain (raw (vd::ParamIDs::spireFeedback));
    vs.spireWide     = vd::spireToBipolar (raw (vd::ParamIDs::spireWide));
    vs.spirePingPong = spirePingPongOn();

    const auto tapeClocks = vd::getTapeClockNames();
    if (mode == vd::ModeTapeClocked)
        vs.clockText = "L " + tapeClocks[(int) raw (vd::ParamIDs::clockLeft)] + "  R " + tapeClocks[(int) raw (vd::ParamIDs::clockRight)];
    else
        vs.clockText = tapeClocks[(int) raw (vd::ParamIDs::tapeClock)];
    visualizer.update (vs, dt);

    tempoField.setTempo (gui.bpm.load(), gui.hostTempo.load());

    // output row: movement preview and duck meter
    const bool moving = raw (vd::ParamIDs::moveHP) > 0.5f || raw (vd::ParamIDs::movePhaser) > 0.5f
                     || raw (vd::ParamIDs::moveFlanger) > 0.5f;
    movePreview.setState (LfoPreview::Kind::Lfo, vd::ShapeSine, moving ? gui.movePhase.load() : 0.0f,
                          raw (vd::ParamIDs::moveDepth) / 100.0f);
    movePreview.setAlpha (moving ? 1.0f : 0.4f);

    const float reductionDb = gui.duckReductionDb.load();
    if (std::abs (reductionDb - duckReductionShown) > 0.1f)
    {
        duckReductionShown = reductionDb;
        repaint (duckMeterArea.toNearestInt().expanded (2));
    }

    const bool lit = gui.loopPeak.load() > 0.9f;
    if (lit != saturationLit)
    {
        saturationLit = lit;
        repaint (saturationArea.toNearestInt().expanded (2));
    }

    const int program = processor.getCurrentProgram();
    if (program != lastShownProgram)
    {
        lastShownProgram = program;
        presetBox.setSelectedId (program + 1, dontSendNotification);
    }
}

//==============================================================================
// Resizable editor: the panel is laid out at its base size and scaled as a whole.
//==============================================================================
VirusDelayAudioProcessorEditor::VirusDelayAudioProcessorEditor (VirusDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    // Read the remembered size first: setting up the resize limits below briefly
    // resizes the window, and that must not overwrite the saved size.
    const int savedWidth = processor.gui.editorWidth.load();

    addAndMakeVisible (panel);

    setResizable (true, true);
    setResizeLimits (kWidth * 7 / 10, kHeight * 7 / 10, kWidth * 5 / 2, kHeight * 5 / 2);
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kHeight);

    const int width = savedWidth > 0 ? jlimit (kWidth * 7 / 10, kWidth * 5 / 2, savedWidth) : kWidth;
    setSize (width, roundToInt (width * (double) kHeight / (double) kWidth));
    sizeRestored = true;
}

void VirusDelayAudioProcessorEditor::resized()
{
    panel.setBounds (0, 0, kWidth, kHeight);
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) kWidth));

    if (sizeRestored)
        processor.gui.editorWidth.store (getWidth());
}
