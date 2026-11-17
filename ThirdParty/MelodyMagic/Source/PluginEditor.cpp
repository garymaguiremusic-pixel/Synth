#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int kWidth = MelodyMagicPanel::kBaseWidth, kHeight = MelodyMagicPanel::kBaseHeight;
    constexpr int kTopStripH = 44;
    constexpr int kBottomBarY = 646;

    const Rectangle<float> patternArea  { 8.0f, 50.0f, 684.0f, 356.0f };
    const Rectangle<int>   libraryArea  { 8, 412, 684, 188 };
    const Rectangle<float> arpArea      { 700.0f, 50.0f, 292.0f, 112.0f };
    const Rectangle<float> scaleArea    { 700.0f, 168.0f, 292.0f, 112.0f };
    const Rectangle<float> generateArea { 700.0f, 286.0f, 292.0f, 216.0f };
    const Rectangle<float> grooveArea   { 700.0f, 508.0f, 292.0f, 92.0f };

    const int snapTicks[] = { 96, 48, 24, 12, 32, 16 };
    const char* snapNames[] = { "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" };
    const int lengthBars[] = { 1, 2, 4, 8 };

    StringArray helpLines()
    {
        return {
            "Hold a key or a chord: MelodyMagic plays the pattern with it. One key plays the pattern as a melody; a chord is arpeggiated.",
            "ARP MODE:  the order the held keys are played in: Up, Down, Up & Down, Down & Up. Each pattern note plays the next key.",
            "PATTERN:  click to add a note, drag to move, drag the right edge for length, right-click / double-click to delete, Alt = no snap.",
            "Notes you draw are locked to KEY + SCALE.  MOUSE WHEEL over a note (or Up / Down) moves it to the next scale note; Alt = a semitone.",
            "Clicking the pattern gives it the keyboard: Left / Right select notes, Delete removes, Ctrl+Z undo. Click the Library to browse with the arrows.",
            "Rows show the notes when you hold the KEY's root; other keys move the whole pattern. Drag in the lower strip for velocity.",
            "SCALE LOCK:  every note sent (live and Drag MIDI) moves to the nearest note of KEY + SCALE.",
            "NEW MELODY:  new notes, the pattern stays exactly as it is (chord tones of CHORDS on the beats, a repeating hook).",
            "NEW PATTERN:  a new rhythm, the notes stay in the same order.  LEARN FROM MIDI FILE: New Melody copies the file's style.",
            "MELODY mode (Arp):  plays the pattern's own notes from the Key's root; the key you hold picks the octave (C3-B3 = as written).",
            "MY MIDI (Library):  Import... adds .mid files (Key + Scale are set from the file), Save... stores the pattern.",
            "SHUFFLE FILE NOTES (Library):  keeps the rhythm and uses exactly the file's notes, in new places.",
            "GROOVE:  pick a groove preset; Q Start / Q End pull note starts / ends onto it.",
            "DRAG MIDI:  drag the blue grip onto a DAW track (the last chord you played, at least 4 bars).",
            "AUTO PLAY:  while the DAW plays, MelodyMagic plays with no MIDI note on its track (the Key's root in octave 3).",
            "TOP NOTES ONLY:  New Melody keeps the bottom notes and changes only the notes above them.",
            "With the DAW stopped, the pattern starts when you press a key. Knobs move in steps of 1 (drag or mouse wheel).",
            "",
            "Click anywhere to close."
        };
    }
}

//==============================================================================
MelodyMagicPanel::MelodyMagicPanel (MelodyMagicProcessor& p)
    : processor (p), grid (p), libraryPanel (p), monitor (p),
      helpOverlay ("MelodyMagic - Quick Help", helpLines())
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    setSize (kWidth, kHeight);
    createBackground();

    // Top bar ---------------------------------------------------------------------
    undoButton.setTooltip ("Undo the last pattern change");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };
    for (auto* b : { &undoButton, &redoButton })
        addAndMakeVisible (b);
    attachToggle (bypassButton, mmp::ParamIDs::bypassed, "Bypass: MIDI passes through untouched");

    dragHandle.createFile = [this] { return processor.renderMidiFile(); };
    addAndMakeVisible (dragHandle);

    // Pattern ---------------------------------------------------------------------
    setupCombo (lengthBox, { "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, "Pattern length");
    StringArray snaps;
    for (auto* s : snapNames) snaps.add (s);
    setupCombo (snapBox, snaps, "Editing grid");
    snapBox.setSelectedItemIndex (2, dontSendNotification);
    lengthBox.onChange = [this]
    {
        if (updating) return;
        auto pt = processor.getPattern();
        mm::resizePattern (pt, lengthBars[jlimit (0, 3, lengthBox.getSelectedItemIndex())]);
        processor.setPattern (pt);
    };
    snapBox.onChange = [this] { grid.setSnapTicks (snapTicks[jlimit (0, 5, snapBox.getSelectedItemIndex())]); };
    grid.setSnapTicks (24);
    clearButton.setTooltip ("Remove every note");
    clearButton.onClick = [this]
    {
        auto pt = processor.getPattern();
        pt.notes.clear();
        processor.setPattern (pt);
        grid.clearSelection();
    };
    selectionLcd.setTooltip ("Selected note (as played from the key's root) and its velocity");
    grid.onSelectionChanged = [this] { updateSelectionInfo(); };
    grid.onFileDropped = [this] (const File& f) { importMidi (f); };
    for (auto* c : std::initializer_list<Component*> { &clearButton, &selectionLcd, &grid, &libraryPanel })
        addAndMakeVisible (c);

    // Arp -------------------------------------------------------------------------
    setupCombo (modeBox, mmp::getModeNames(), "The order the held keys are played in");
    setupCombo (speedBox, mmp::getSpeedNames(), "Pattern speed against the host tempo");
    attachCombo (modeBox, mmp::ParamIDs::arpMode);
    attachCombo (speedBox, mmp::ParamIDs::speed);
    attachToggle (autoPlayButton, mmp::ParamIDs::autoPlay,
                  "Auto Play: while the DAW plays, MelodyMagic plays without any MIDI note on its track,\n"
                  "as if the Key's root (octave 3) was held. A key you play takes over while you hold it.");
    attachKnob (randomVelKnob, mmp::ParamIDs::randomVel, [] (double v) { return "+/- " + String (roundToInt (v)); },
                "Random velocity, + or - this much");

    // Scale lock --------------------------------------------------------------------
    setupCombo (keyBox, mmp::getKeyNames(), "Key: the pattern's notes are shown in this key");
    setupCombo (scaleBox, mmp::getScaleNames(), "Scale (the same 16 scales as ArpSampler, plus Persian, Chromatic and Custom)");
    attachCombo (keyBox, mmp::ParamIDs::scaleKey);
    attachCombo (scaleBox, mmp::ParamIDs::scaleIndex);
    attachToggle (scaleLockButton, mmp::ParamIDs::scaleLock, "Scale Lock: every note MelodyMagic sends (live and Drag MIDI) moves to the nearest note of the key and scale");
    for (int i = 0; i < 12; ++i)
    {
        auto& k = customKeys[(size_t) i];
        k.setButtonText (mm::keyName (i));
        k.setClickingTogglesState (true);
        k.setTooltip ("Custom scale: " + String (mm::keyName (i)) + " in / out (custom scales ignore the key)");
        k.onClick = [this, i]
        {
            auto mask = processor.getCustomMask();
            mask = (std::uint16_t) (customKeys[(size_t) i].getToggleState() ? (mask | (1u << i)) : (mask & ~(1u << i)));
            processor.setCustomMask (mask);
        };
        addChildComponent (k);
    }

    // Melody Maker -------------------------------------------------------------------------
    setupCombo (progressionBox, mmp::getProgressionNames(),
                "The chords the melody is written over (scale degrees, one chord per bar).\n"
                "In A minor: 1 - 6 - 3 - 7 = Am F C G. Auto = the classic trance progression for the scale.");
    attachCombo (progressionBox, mmp::ParamIDs::genProgression);
    chordsLcd.setTooltip ("The chords in your Key + Scale");
    addAndMakeVisible (chordsLcd);
    attachKnob (densityKnob, mmp::ParamIDs::genDensity, [] (double v) { return String (roundToInt (v)) + " %"; },
                "How busy the melody's rhythm is");
    attachKnob (rangeKnob, mmp::ParamIDs::genRange, [] (double v) { return String (roundToInt (v)) + " st"; },
                "How far the melody spreads, in semitones");
    attachKnob (gateKnob, mmp::ParamIDs::genGate, [] (double v) { return String (roundToInt (v)) + " %"; },
                "Note length (100 % = legato)");
    newMelodyButton.setTooltip ("New notes. The pattern (rhythm) stays exactly as it is.\n"
                                "Notes on the beat are chord tones of CHORDS, always in the Key + Scale.");
    newPatternButton.setTooltip ("A new pattern (rhythm). The notes stay, in the same order.");
    fileStyleButton.setTooltip ("New Melody learns from the imported MIDI file: its intervals, favourite notes and range");
    attachToggle (topOnlyButton, mmp::ParamIDs::genTopOnly,
                  "Top Notes Only: New Melody keeps the bottom notes (the pattern's lowest note and the lower notes of chords)\n"
                  "and only changes the notes above them. On a pattern that is all one note, the beats stay as a pedal.");
    fileStyleButton.setClickingTogglesState (true);
    newMelodyButton.onClick = [this] { processor.newMelody(); grid.grabKeyboardFocus(); };
    newPatternButton.onClick = [this] { processor.newPattern(); grid.grabKeyboardFocus(); };
    fileStyleButton.onClick = [this] { processor.useFileStyle = fileStyleButton.getToggleState(); updateFromModel(); };
    for (auto* b : { &newMelodyButton, &newPatternButton, &fileStyleButton })
        addAndMakeVisible (b);

    // Groove ----------------------------------------------------------------------------
    grooveBox.setTooltip ("Groove preset: Q Start / Q End pull the notes onto it");
    grooveBox.setTextWhenNothingSelected ("No Groove");
    grooveBox.onChange = [this]
    {
        if (updating) return;
        const int id = grooveBox.getSelectedId();
        if (id <= 1 || grooveLibrary == nullptr)
        {
            processor.setGroove (std::nullopt, {});
            return;
        }
        const auto index = (size_t) (id - 2);
        if (index < grooveKeys.size())
            if (auto it = grooveLibrary->grooves.find (grooveKeys[index].toStdString()); it != grooveLibrary->grooves.end())
                processor.setGroove (it->second, grooveKeys[index]);
    };
    addAndMakeVisible (grooveBox);
    attachKnob (qStartKnob, mmp::ParamIDs::quantizeStart, [] (double v) { return String (roundToInt (v)) + " %"; },
                "How far note starts move onto the groove");
    attachKnob (qEndKnob, mmp::ParamIDs::quantizeEnd, [] (double v) { return String (roundToInt (v)) + " %"; },
                "How far note ends move onto the groove");
    attachToggle (qLinkButton, mmp::ParamIDs::quantizeLink, "Link: Q Start sets both starts and ends");

    // Monitor, help ------------------------------------------------------------------------
    addAndMakeVisible (monitor);
    helpButton.setTooltip ("Quick help");
    helpButton.onClick = [this] { helpOverlay.setVisible (true); helpOverlay.toFront (false); };
    addAndMakeVisible (helpButton);
    addChildComponent (helpOverlay);

    // only the pattern and the library lists take the keyboard
    for (auto* c : getChildren())
        if (c != &grid && c != &libraryPanel)
            keepFocusOnNotes (*c);

    processor.addChangeListener (this);
    updateFromModel();
    startTimerHz (8);
}

void MelodyMagicPanel::keepFocusOnNotes (Component& c)
{
    c.setWantsKeyboardFocus (false);
    c.setMouseClickGrabsKeyboardFocus (false);
    for (auto* child : c.getChildren())
        keepFocusOnNotes (*child);
}

void MelodyMagicPanel::rebuildGrooveMenu()
{
    const ScopedValueSetter<bool> svs (updating, true);
    grooveLibrary = processor.getLibrary();
    grooveKeys.clear();
    grooveBox.clear (dontSendNotification);

    auto* menu = grooveBox.getRootMenu();
    menu->addItem (1, "No Groove");
    if (grooveLibrary != nullptr && ! grooveLibrary->grooves.empty())
    {
        // one sub-menu per folder (MPC, SP1200, Swing...)
        std::map<String, PopupMenu> folders;
        for (auto& kv : grooveLibrary->grooves)
        {
            const auto key = String::fromUTF8 (kv.first.c_str());
            grooveKeys.push_back (key);
            auto folder = key.upToLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("/", false, false);
            auto name = key.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false);
            folders[folder.isEmpty() ? String ("Grooves") : folder].addItem ((int) grooveKeys.size() + 1, name);
        }
        for (auto& [folder, sub] : folders)
            menu->addSubMenu (folder, sub);
    }
    else
    {
        menu->addItem (-1, "(no grooves: set the Library folder)", false);
    }
}

MelodyMagicPanel::~MelodyMagicPanel()
{
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void MelodyMagicPanel::setupCombo (ComboBox& box, const StringArray& items, const String& tooltip)
{
    box.addItemList (items, 1);
    box.setTooltip (tooltip);
    box.setSelectedItemIndex (0, dontSendNotification);
    addAndMakeVisible (box);
}

void MelodyMagicPanel::attachCombo (ComboBox& box, const char* paramId)
{
    comboAttachments.push_back (std::make_unique<ComboAttachment> (processor.apvts, paramId, box));
}

void MelodyMagicPanel::attachToggle (TextButton& b, const char* paramId, const String& tooltip)
{
    b.setClickingTogglesState (true);
    b.setTooltip (tooltip);
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (processor.apvts, paramId, b));
}

void MelodyMagicPanel::attachKnob (VirusKnob& k, const char* paramId, std::function<String (double)> describe, const String& tooltip)
{
    k.describeValue = std::move (describe);
    k.extraTooltip = tooltip;
    knobAttachments.push_back (std::make_unique<KnobAttachment> (*processor.apvts.getParameter (paramId), k));
    addAndMakeVisible (k);
}

void MelodyMagicPanel::importMidi (const File& f)
{
    String error;
    if (! processor.importMidiFile (f, error))
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Import MIDI", "Could not import " + f.getFileName() + "\n" + error);
}

//==============================================================================
void MelodyMagicPanel::changeListenerCallback (ChangeBroadcaster*)
{
    updateFromModel();
}

void MelodyMagicPanel::updateFromModel()
{
    const ScopedValueSetter<bool> svs (updating, true);
    const auto& pt = processor.getPattern();

    int lengthIndex = 0;
    for (int i = 0; i < 4; ++i)
        if (pt.bars == lengthBars[i])
            lengthIndex = i;
    lengthBox.setSelectedItemIndex (lengthIndex, dontSendNotification);

    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());

    const int fileNotes = processor.numFileNotes();

    grid.modelChanged();
    updateSelectionInfo();
    libraryPanel.refresh();

    if (processor.getLibrary() != grooveLibrary)
        rebuildGrooveMenu();
    int grooveId = 1;
    for (size_t i = 0; i < grooveKeys.size(); ++i)
        if (grooveKeys[i] == processor.getGrooveName())
            grooveId = (int) i + 2;
    grooveBox.setSelectedId (grooveId, dontSendNotification);

    const bool hasFile = fileNotes > 0;
    fileStyleButton.setEnabled (hasFile);
    fileStyleButton.setToggleState (processor.fileStyleActive(), dontSendNotification);
    chordsLcd.setContent (processor.describeChords(), {});

    updateCustomKeys();
    repaint (patternArea.toNearestInt());
    repaint (generateArea.toNearestInt());
}

void MelodyMagicPanel::updateSelectionInfo()
{
    const auto& pt = processor.getPattern();
    const int sel = grid.getSelected();
    if (sel >= 0 && sel < (int) pt.notes.size())
    {
        const auto& n = pt.notes[(size_t) sel];
        const String name (mm::keyName (processor.currentKey() + n.offset));
        selectionLcd.setContent (name + "  " + (n.offset > 0 ? "+" : "") + String (n.offset) + " st", "V" + String (n.velocity));
        selectionLcd.setDimmed (false);
    }
    else
    {
        selectionLcd.setContent (String ((int) pt.notes.size()) + " notes", {});
        selectionLcd.setDimmed (true);
    }
}

void MelodyMagicPanel::updateCustomKeys()
{
    const bool custom = processor.currentScaleIndex() == mm::Custom;
    const auto mask = processor.getCustomMask();
    for (int i = 0; i < 12; ++i)
    {
        customKeys[(size_t) i].setVisible (custom);
        customKeys[(size_t) i].setToggleState (((mask >> i) & 1u) != 0, dontSendNotification);
    }
}

void MelodyMagicPanel::timerCallback()
{
    // the key / scale / lock can change from the host: the grid labels follow them
    const int scale = processor.currentScaleIndex(), key = processor.currentKey(), lock = processor.scaleLockOn() ? 1 : 0;
    if (scale != lastScaleIndex || key != lastKey || lock != lastLock)
    {
        lastScaleIndex = scale;
        lastKey = key;
        lastLock = lock;
        updateCustomKeys();
        updateSelectionInfo();
        chordsLcd.setContent (processor.describeChords(), {});
        grid.repaint();
        repaint (scaleArea.toNearestInt());
    }
    if (processor.isLibraryLoading())
        libraryPanel.repaint();
}

//==============================================================================
void MelodyMagicPanel::resized()
{
    // top bar
    undoButton.setBounds (330, 9, 64, 26);
    redoButton.setBounds (400, 9, 64, 26);
    bypassButton.setBounds (480, 9, 76, 26);
    dragHandle.setBounds (790, 7, 194, 30);

    // pattern
    lengthBox.setBounds (66, 72, 84, 22);
    snapBox.setBounds (198, 72, 72, 22);
    selectionLcd.setBounds (470, 72, 156, 22);
    clearButton.setBounds (636, 72, 48, 22);
    grid.setBounds (16, 100, 668, 298);
    libraryPanel.setBounds (libraryArea);

    // arp
    modeBox.setBounds (752, 74, 140, 22);
    speedBox.setBounds (752, 104, 140, 22);
    autoPlayButton.setBounds (708, 134, 184, 22);
    randomVelKnob.setBounds (906, 70, 72, 84);

    // scale lock
    keyBox.setBounds (744, 192, 62, 22);
    scaleBox.setBounds (858, 192, 126, 22);
    scaleLockButton.setBounds (708, 220, 276, 26);
    for (int i = 0; i < 12; ++i)
        customKeys[(size_t) i].setBounds (708 + i * 23, 252, 22, 20);

    // melody maker
    progressionBox.setBounds (764, 310, 220, 22);
    chordsLcd.setBounds (708, 338, 276, 22);
    densityKnob.setBounds (716, 364, 64, 64);
    rangeKnob.setBounds (810, 364, 64, 64);
    gateKnob.setBounds (904, 364, 64, 64);
    newMelodyButton.setBounds (708, 434, 136, 26);
    newPatternButton.setBounds (848, 434, 136, 26);
    fileStyleButton.setBounds (708, 466, 136, 24);
    topOnlyButton.setBounds (848, 466, 136, 24);

    // groove
    grooveBox.setBounds (708, 532, 170, 22);
    qLinkButton.setBounds (708, 564, 70, 24);
    qStartKnob.setBounds (868, 526, 60, 66);
    qEndKnob.setBounds (926, 526, 60, 66);

    monitor.setBounds (8, 606, 984, 34);
    helpButton.setBounds (kWidth - 34, kBottomBarY + 6, 22, 22);
    helpOverlay.setBounds (getLocalBounds());
}

//==============================================================================
void MelodyMagicPanel::paint (Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());

    // top bar
    auto top = Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kTopStripH);
    g.setGradientFill (ColourGradient (Colour (0xff4a4e53), 0.0f, 0.0f, Colour (0xff2b2e32), 0.0f, top.getBottom(), false));
    g.fillRect (top);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine (0, 0.0f, (float) kWidth);
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kTopStripH - 1, 0.0f, (float) kWidth);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (kTopStripH, 0.0f, (float) kWidth);
    drawWordmark (g, { 16.0f, 2.0f, 280.0f, (float) kTopStripH - 2.0f });
    CustomLookAndFeel::drawSmallLabel (g, "Pattern", { 700.0f, 11.0f, 84.0f, 22.0f }, Justification::centredRight);

    // pattern panel
    const auto& pt = processor.getPattern();
    String title = "Pattern  |  " + String (pt.name);
    if (processor.numFileNotes() > 0 && processor.getFileName().isNotEmpty())
        title << "     (file notes: " << processor.getFileName() << ")";
    CustomLookAndFeel::drawPanel (g, patternArea, title);
    CustomLookAndFeel::drawSmallLabel (g, "Length", { 16.0f, 72.0f, 48.0f, 22.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Snap", { 160.0f, 72.0f, 36.0f, 22.0f }, Justification::centredLeft);

    // right column
    CustomLookAndFeel::drawPanel (g, arpArea, "Arp");
    CustomLookAndFeel::drawSmallLabel (g, "Mode", { 708.0f, 74.0f, 42.0f, 22.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Speed", { 708.0f, 104.0f, 42.0f, 22.0f }, Justification::centredLeft);

    CustomLookAndFeel::drawPanel (g, scaleArea, "Scale Lock");
    CustomLookAndFeel::drawSmallLabel (g, "Key", { 708.0f, 192.0f, 34.0f, 22.0f }, Justification::centredLeft);
    CustomLookAndFeel::drawSmallLabel (g, "Scale", { 816.0f, 192.0f, 40.0f, 22.0f }, Justification::centredLeft);
    if (processor.currentScaleIndex() != mm::Custom)
    {
        g.setFont (CustomLookAndFeel::uiFont (10.0f));
        g.setColour (VirusColours::labelText.withAlpha (0.75f));
        g.drawFittedText ("Locks every note sent, live and Drag MIDI, to the key",
                          Rectangle<int> (708, 250, 276, 22), Justification::centred, 1, 0.8f);
    }

    const String style = processor.fileStyleActive() ? "Style: " + processor.getFileName().upToLastOccurrenceOf (".", false, true)
                                                     : String ("Style: classic trance");
    CustomLookAndFeel::drawPanel (g, generateArea, "Melody Maker");
    CustomLookAndFeel::drawSmallLabel (g, "Chords", { 708.0f, 310.0f, 54.0f, 22.0f }, Justification::centredLeft);
    g.setFont (CustomLookAndFeel::uiFont (10.0f));
    g.setColour (processor.fileStyleActive() ? VirusColours::blue : VirusColours::labelText.withAlpha (0.75f));
    g.drawFittedText (style, Rectangle<int> (708, 491, 276, 10), Justification::centred, 1, 0.8f);

    CustomLookAndFeel::drawPanel (g, grooveArea, "Groove");

    // bottom bar
    auto bottom = Rectangle<float> (0.0f, (float) kBottomBarY, (float) kWidth, (float) (kHeight - kBottomBarY));
    g.setGradientFill (ColourGradient (Colour (0xff2b2e32), 0.0f, bottom.getY(), Colour (0xff1c1e21), 0.0f, bottom.getBottom(), false));
    g.fillRect (bottom);
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kBottomBarY, 0.0f, (float) kWidth);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (kBottomBarY + 1, 0.0f, (float) kWidth);

    auto tag = Rectangle<float> (12.0f, (float) kBottomBarY + 6.0f, 96.0f, 22.0f);
    g.setGradientFill (ColourGradient (Colour (0xff6b7076), 0.0f, tag.getY(), Colour (0xff3c4045), 0.0f, tag.getBottom(), false));
    g.fillRoundedRectangle (tag, 3.0f);
    g.setColour (VirusColours::bevelDark);
    g.drawRoundedRectangle (tag, 3.0f, 1.0f);
    g.setFont (CustomLookAndFeel::uiFont (11.5f, true));
    g.setColour (Colour (0xffdfe3e6));
    g.drawText ("ARP", tag, Justification::centred);

    auto titleBox = Rectangle<float> (180.0f, (float) kBottomBarY + 6.0f, (float) kWidth - 360.0f, 22.0f);
    CustomLookAndFeel::drawInsetBox (g, titleBox, 3.0f);
    g.setFont (CustomLookAndFeel::uiFont (13.0f, true));
    g.setColour (Colour (0xffe3e6e8));
    g.drawText ("MELODYMAGIC", titleBox, Justification::centred);
}

//==============================================================================
void MelodyMagicPanel::createBackground()
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
    for (int i = 0; i < 2400; ++i)
    {
        const int x = rng.nextInt (kWidth), y = rng.nextInt (kHeight), len = 20 + rng.nextInt (140);
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.015f + rng.nextFloat() * 0.03f));
        g.fillRect (x, y, len, 1);
    }

    ColourGradient vignette (Colours::transparentBlack, kWidth * 0.5f, kHeight * 0.45f,
                             Colours::black.withAlpha (0.35f), 0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillAll();
}

void MelodyMagicPanel::drawWordmark (Graphics& g, Rectangle<float> area)
{
    // Chrome "melody", blue "magic", and a swoosh (same treatment as Virus Delay).
    const float baseline = area.getBottom() - 8.0f;

    GlyphArrangement main;
    main.addLineOfText (Font (FontOptions (27.0f, Font::bold | Font::italic)), "melody", area.getX(), baseline);
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
    second.addLineOfText (Font (FontOptions (19.0f, Font::plain)), "magic", mainBounds.getRight() + 6.0f, baseline);
    Path secondPath;
    second.createPath (secondPath);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillPath (secondPath, AffineTransform::translation (1.0f, 1.0f));
    g.setColour (VirusColours::blue);
    g.fillPath (secondPath);

    Path swoosh;
    const float sx = mainBounds.getX() + 2.0f, sy = baseline + 3.5f;
    const float ex = secondPath.getBounds().getRight();
    swoosh.startNewSubPath (sx, sy);
    swoosh.quadraticTo ((sx + ex) * 0.5f, sy + 3.5f, ex + 6.0f, sy - 2.0f);
    g.setGradientFill (ColourGradient (VirusColours::blue.withAlpha (0.0f), sx, sy, VirusColours::blue, ex, sy, false));
    g.strokePath (swoosh, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
// Resizable editor: the panel is laid out at its base size and scaled as a whole.
//==============================================================================
MelodyMagicEditor::MelodyMagicEditor (MelodyMagicProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    // Read the remembered size first: setting the resize limits below briefly
    // resizes the window, and that must not overwrite the saved size.
    const int savedWidth = processor.editorWidth.load();

    addAndMakeVisible (panel);

    setResizable (true, true);
    setResizeLimits (kWidth * 6 / 10, kHeight * 6 / 10, kWidth * 2, kHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kHeight);

    const int width = savedWidth > 0 ? jlimit (kWidth * 6 / 10, kWidth * 2, savedWidth) : kWidth;
    setSize (width, roundToInt (width * (double) kHeight / (double) kWidth));
    sizeRestored = true;
}

void MelodyMagicEditor::resized()
{
    panel.setBounds (0, 0, kWidth, kHeight);
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) kWidth));

    if (sizeRestored)
        processor.editorWidth.store (getWidth());
}
