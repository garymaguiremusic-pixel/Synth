#include "PluginEditor.h"
#include "VirusCLayout.h"
#include "Tables/Tables.h"
#include "Tables/Waves.h"

#include <cstring>
#include <map>

using namespace juce;
namespace L = vc::layout;
using namespace vc::layout::off;

namespace
{
    constexpr int kWidth = VCClonePanel::kBaseWidth, kHeight = VCClonePanel::kBaseHeight;
    constexpr int kTopStripH = 40;
    constexpr int kTabsY = 46, kTabsH = 28;
    constexpr int kContentY = 80, kContentBottom = 584;
    constexpr int kLcdY = 590, kLcdH = 32;
    constexpr int kBottomBarY = 630;
    constexpr int kCellW = 66, kCellH = 76;

    const char* kTabNames[] = { "Oscillators", "Filters", "LFOs", "Modulation", "Common", "Effects", "Browser" };

    bool isListParam (int o)
    {
        const std::string_view t = L::kParams[o].toText;
        if (t.empty()) return false;
        static constexpr std::string_view numeric[] = { "signed", "pan", "unsignedZero", "unsignedOne", "oscShape",
                                                        "envSustainTime", "arpSwing", "bpm", "filterKeytrackBase", "ascii" };
        for (auto n : numeric) if (t == n) return false;
        return true;
    }

    bool isBipolar (int o)
    {
        const std::string_view t = L::kParams[o].toText;
        return t == "signed" || t == "pan" || t == "oscShape" || t == "envSustainTime";
    }

    String shortLabel (int o)
    {
        static const std::map<int, const char*> overrides = {
            { Osc1_Pulsewidth, "PW" }, { Osc2_Pulsewidth, "PW" }, { Osc1_Wave_Select, "Wave" }, { Osc2_Wave_Select, "Wave" },
            { Osc1_Shape_Velocity, "Shape Vel" }, { Osc2_Shape_Velocity, "Shape Vel" }, { Osc2_FM_Amount, "FM Amt" },
            { Osc2_Filt_Env_Amt, "F.Env>Pitch" }, { FM_Filt_Env_Amt, "F.Env>FM" }, { Osc_FM_Mode, "FM Mode" },
            { Fm_Amount_Velocity, "FM Vel" }, { Osc_Balance, "Balance" }, { Suboscillator_Volume, "Sub Vol" },
            { Suboscillator_Shape, "Sub Shape" }, { Osc_Mainvolume, "Osc Vol" }, { Ringmodulator_Volume, "Ring Mod" },
            { Osc_Init_Phase, "Init Phase" }, { PulseWidth_Velocity, "PW Vel" }, { Noise_Volume, "Volume" }, { Noise_Color, "Color" },
            { Cutoff, "Cutoff" }, { Cutoff2, "Cutoff" }, { Filter1_Resonance, "Resonance" }, { Filter2_Resonance, "Resonance" },
            { Filter1_Env_Amt, "Env Amt" }, { Filter2_Env_Amt, "Env Amt" }, { Filter1_Keyfollow, "Keyfollow" },
            { Filter2_Keyfollow, "Keyfollow" }, { Filter1_Mode, "Mode" }, { Filter2_Mode, "Mode" },
            { Filter1_Env_Polarity, "Env Polarity" }, { Filter2_Env_Polarity, "Env Polarity" }, { Filter2_Cutoff_Link, "Link" },
            { Flt1_EnvAmt_Velocity, "Env Vel" }, { Flt2_EnvAmt_Velocity, "Env Vel" }, { Resonance1_Velocity, "Reso Vel" },
            { Resonance2_Velocity, "Reso Vel" }, { Filter_Balance, "Balance" }, { Saturation_Curve, "Saturation" },
            { Filter_Routing, "Routing" }, { Filter_Keytrack_Base, "Key Base" },
            { Osc1_Lfo1_Amount, "Osc1" }, { Osc2_Lfo1_Amount, "Osc2" }, { PW_Lfo1_Amount, "PW" }, { Reso_Lfo1_Amount, "Reso" },
            { FiltGain_Lfo1_Amount, "Filt Gain" }, { LFO1_Assign_Dest, "Assign" }, { LFO1_Assign_Amount, "Amount" },
            { Shape_Lfo2_Amount, "Shape" }, { FM_Lfo2_Amount, "FM" }, { Cutoff1_Lfo2_Amount, "Cutoff 1" },
            { Cutoff2_Lfo2_Amount, "Cutoff 2" }, { Pan_Lfo2_Amount, "Pan" }, { LFO2_Assign_Dest, "Assign" },
            { LFO2_Assign_Amount, "Amount" }, { Osc_Lfo3_Amount, "Amount" }, { Lfo3_Destination, "Destination" },
            { Lfo3_Fade_In_Time, "Fade In" }, { Patch_Volume, "Volume" }, { Amp_Velocity, "Velocity" },
            { Panorama_Velocity, "Pan Vel" }, { Control_Smooth_Mode, "Smooth" }, { Bender_Range_Up, "Bend Up" },
            { Bender_Range_Down, "Bend Down" }, { Bender_Scale, "Bend Scale" }, { Unison_Mode, "Voices" },
            { Unison_Pan_Spread, "Pan Spread" }, { Unison_Lfo_Phase, "LFO Phase" }, { Arp_Pattern_Selct, "Pattern" },
            { Arp_Octave_Range, "Octaves" }, { Arp_Hold_Enable, "Hold" }, { Arp_Note_Length, "Length" },
            { Punch_Intensity, "Punch" }, { Portamento_Time, "Porta" }, { Channel_Volume, "Ch Volume" },
            { Clock_Tempo, "Tempo" }, { Category1, "Category 1" }, { Category2, "Category 2" },
            { Delay_Reverb_Mode, "Mode" }, { Effect_Send, "Send" }, { Delay_Time, "Time" }, { Delay_Feedback, "Feedback" },
            { Dly_Rate_Rev_Decay, "Rate/Decay" }, { Dly_Depth, "Depth/Size" }, { Reverb_Damping, "Damp/Shape" },
            { Delay_Color, "Color" }, { Delay_Clock, "Clock" }, { Chorus_Lfo_Shape, "Shape" },
            { LowEQ_Gain, "Low Gain" }, { LowEQ_Frequency, "Low Freq" }, { MidEQ_Gain, "Mid Gain" },
            { MidEQ_Frequency, "Mid Freq" }, { MidEQ_Q_Factor, "Mid Q" }, { HighEQ_Gain, "High Gain" },
            { HighEQ_Frequency, "High Freq" }, { Bass_Intensity, "Intensity" }, { Bass_Tune, "Tune" },
            { Distortion_Curve, "Curve" }, { Distortion_Intensity, "Intensity" }, { Panorama, "Panorama" },
        };
        if (auto it = overrides.find (o); it != overrides.end()) return it->second;

        String n = String (L::kParams[o].name).trim();
        for (auto prefix : { "Osc1 ", "Osc2 ", "Osc3 ", "Lfo1 ", "Lfo2 ", "Lfo3 ", "Filter Env ", "Amp Env ", "Chorus ",
                             "Phaser ", "Unison ", "Arp ", "Assign1 ", "Assign2 ", "Assign3 ", "Assign 4 ", "Assign 5 ", "Assign 6 " })
            if (n.startsWith (prefix)) { n = n.substring ((int) std::strlen (prefix)); break; }
        n = n.replace ("Destination", "Dest").replace ("Sustain Time", "Sus Time");
        return n;
    }

    std::vector<int> regionOffsets (const char* id)
    {
        std::vector<int> out;
        for (const auto& r : L::kRegions)
            if (std::string_view (r.id) == id)
                for (int i = 0; i < r.count; ++i)
                    if (r.offsets[i] >= 0 && VCCloneAudioProcessor::isHostParameter (r.offsets[i]))
                        out.push_back (r.offsets[i]);
        return out;
    }

    std::vector<int> without (std::vector<int> v, std::initializer_list<int> drop)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [&] (int o) { return std::find (drop.begin(), drop.end(), o) != drop.end(); }), v.end());
        return v;
    }
}

//==============================================================================
VCClonePanel::VCClonePanel (VCCloneAudioProcessor& p)
    : processor (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    setSize (kWidth, kHeight);
    createBackground();

    for (int i = 0; i < kNumTabs; ++i)
    {
        auto& t = tabs[(size_t) i];
        t.setButtonText (kTabNames[i]);
        t.onClick = [this, i] { showTab (i); };
        addAndMakeVisible (t);
    }

    loadButton.setTooltip ("Load a Virus A/B/C bank (.mid or .syx). You can also drag a file onto the window.");
    loadButton.onClick = [this] { openLoadDialog(); };
    exportButton.setTooltip ("Export the current patch or the whole bank as .syx or .mid");
    exportButton.onClick = [this] { openExportMenu(); };
    initButton.setTooltip ("Init patch: one saw, filter open, effects off");
    initButton.onClick = [this] { processor.loadInitPatch(); statusMessage = "Init patch"; statusUntil = Time::getMillisecondCounterHiRes() + 2000; };
    prevButton.setTooltip ("Previous patch");
    nextButton.setTooltip ("Next patch");
    prevButton.onClick = [this] { processor.setCurrentProgram (processor.getCurrentProgram() - 1); };
    nextButton.onClick = [this] { processor.setCurrentProgram (processor.getCurrentProgram() + 1); };
    presetBox.setTooltip ("Patches in the loaded bank");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != processor.getCurrentProgram()) processor.setCurrentProgram (idx);
    };
    for (Component* c : { (Component*) &loadButton, (Component*) &exportButton, (Component*) &initButton,
                          (Component*) &prevButton, (Component*) &nextButton, (Component*) &presetBox })
        addAndMakeVisible (c);

    buildPanels();

    patchList.setRowHeight (20);
    patchList.setColour (ListBox::backgroundColourId, Colour (0xff15181b));
    patchList.setColour (ListBox::outlineColourId, VirusColours::bevelDark);
    patchList.setOutlineThickness (1);
    addChildComponent (patchList);

    infoText.setMultiLine (true);
    infoText.setReadOnly (true);
    infoText.setScrollbarsShown (true);
    infoText.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 12.0f, Font::plain)));
    infoText.setColour (TextEditor::backgroundColourId, Colour (0xff15181b));
    infoText.setColour (TextEditor::textColourId, Colour (0xffdfe3e6));
    infoText.setColour (TextEditor::outlineColourId, VirusColours::bevelDark);
    addChildComponent (infoText);

    addAndMakeVisible (lcd);
    addAndMakeVisible (statusLcd);
    helpButton.setTooltip ("Help");
    helpButton.onClick = [this] { helpOverlay.setVisible (true); helpOverlay.toFront (false); };
    addAndMakeVisible (helpButton);
    addChildComponent (helpOverlay);

    shownValues.fill (-1);
    layoutPanels();
    showTab (0);
    timerCallback();
    startTimerHz (30);
}

VCClonePanel::~VCClonePanel()
{
    stopTimer();
    knobAttachments.clear();
    comboAttachments.clear();
    toggleAttachments.clear();
    setLookAndFeel (nullptr);
}

//==============================================================================
void VCClonePanel::addPanel (int tab, const String& title, std::vector<int> offsets, int cols, Preview preview)
{
    Panel panel;
    panel.title = title;
    panel.tab = tab;
    panel.cols = cols;

    auto& apvts = processor.apvts;
    (void) apvts;
    for (int o : offsets)
    {
        auto* prm = processor.getParam (o);
        if (prm == nullptr) continue;
        const auto& d = L::kParams[o];
        Item item;
        item.offset = o;

        if (d.max - d.min == 1 && ! isListParam (o))
        {
            item.kind = Item::Kind::Toggle;
            auto b = std::make_unique<TextButton> (shortLabel (o));
            b->setTooltip (String (d.name).trim());
            toggleAttachments.push_back (std::make_unique<ToggleAttachment> (*prm, *b));
            item.comp = b.get();
            addChildComponent (*b);
            toggles.push_back (std::move (b));
        }
        else if (isListParam (o))
        {
            item.kind = Item::Kind::Combo;
            item.cells = 2;
            auto c = std::make_unique<VirusCombo>();
            for (int v = d.min; v <= d.max; ++v)
            {
                auto text = String (vc::valueToText (o, v));
                c->addItem (text.isEmpty() ? String ("-") : text, v - d.min + 1);
            }
            c->setTooltip (String (d.name).trim());
            c->onTouched = [this, o] { touched (o); };
            comboAttachments.push_back (std::make_unique<ComboAttachment> (*prm, *c, d.min));
            item.comp = c.get();
            addChildComponent (*c);

            auto lbl = std::make_unique<Label> (String(), shortLabel (o).toUpperCase());
            lbl->setFont (CustomLookAndFeel::uiFont (10.0f, true));
            lbl->setColour (Label::textColourId, VirusColours::labelText);
            lbl->setJustificationType (Justification::centredLeft);
            lbl->setInterceptsMouseClicks (false, false);
            lbl->setComponentID (String (o));
            addChildComponent (*lbl);
            c->getProperties().set ("labelIndex", (int) comboLabels.size());
            comboLabels.push_back (std::move (lbl));
            combos.push_back (std::move (c));
        }
        else
        {
            auto k = std::make_unique<VirusKnob> (VirusKnob::Size::Small, shortLabel (o));
            k->setBipolar (isBipolar (o));
            k->valueText = [o] (double v) { return String (vc::valueToText (o, roundToInt (v))); };
            k->onTouched = [this, o] { touched (o); };
            knobAttachments.push_back (std::make_unique<KnobAttachment> (*prm, *k));
            item.comp = k.get();
            addChildComponent (*k);
            knobs.push_back (std::move (k));
        }
        panel.items.push_back (item);
    }

    if (preview != Preview::None)
    {
        Item item;
        item.kind = Item::Kind::Preview;
        item.cells = 2;
        auto pv = std::make_unique<CurvePreview>();
        pv->setInterceptsMouseClicks (false, false);
        previewKinds.push_back ({ pv.get(), preview });
        pv->fn = [this, preview] (float x) { return previewValue (preview, x); };
        item.comp = pv.get();
        addChildComponent (*pv);
        previews.push_back (std::move (pv));
        panel.items.push_back (item);
    }
    panels.push_back (std::move (panel));
}

void VCClonePanel::buildPanels()
{
    // 0 Oscillators
    addPanel (0, "Oscillator 1", regionOffsets ("oscA"), 5, Preview::Osc1);
    addPanel (0, "Oscillator 2", regionOffsets ("oscB"), 9, Preview::Osc2);
    addPanel (0, "Oscillator 3", regionOffsets ("oscC"), 3);
    addPanel (0, "Osc Common",   regionOffsets ("oscCommon"), 8);
    addPanel (0, "Noise",        regionOffsets ("noise"), 2);

    // 1 Filters + envelopes
    addPanel (1, "Filter 1",       regionOffsets ("filterA"), 5);
    addPanel (1, "Filter 2",       regionOffsets ("filterB"), 6);
    addPanel (1, "Filters Common", regionOffsets ("filterCommon"), 3);
    addPanel (1, "Filter Env",     regionOffsets ("envFilter"), 7, Preview::FiltEnv);
    addPanel (1, "Amp Env",        regionOffsets ("envAmp"), 7, Preview::AmpEnv);

    // 2 LFOs
    addPanel (2, "LFO 1", regionOffsets ("lfoA"), 7, Preview::Lfo1);
    addPanel (2, "LFO 2", regionOffsets ("lfoB"), 7, Preview::Lfo2);
    addPanel (2, "LFO 3", regionOffsets ("lfoC"), 14, Preview::Lfo3);

    // 3 Modulation matrix
    addPanel (3, "Assign 1", regionOffsets ("assignA"), 5);
    addPanel (3, "Assign 2", regionOffsets ("assignB"), 8);
    addPanel (3, "Assign 3", regionOffsets ("assignC"), 11);
    addPanel (3, "Assign 4", regionOffsets ("assignD"), 5);
    addPanel (3, "Assign 5", regionOffsets ("assignE"), 5);
    addPanel (3, "Assign 6", regionOffsets ("assignF"), 5);

    // 4 Common
    addPanel (4, "Patch",       { Panorama, Portamento_Time, Channel_Volume, Clock_Tempo, Punch_Intensity, Category1, Category2 }, 9);
    addPanel (4, "Amp",         without (regionOffsets ("amp"), { Second_Output_Balance }), 3);
    addPanel (4, "Common",      regionOffsets ("common"), 9);
    addPanel (4, "Unison",      regionOffsets ("unison"), 5);
    addPanel (4, "Arpeggiator", regionOffsets ("arp"), 9);

    // 5 Effects
    addPanel (5, "Delay / Reverb", regionOffsets ("delayReverb"), 6);
    addPanel (5, "Chorus",         regionOffsets ("chorus"), 7);
    addPanel (5, "Phaser",         regionOffsets ("phaser"), 8);
    addPanel (5, "Equalizer",      regionOffsets ("eq"), 7);
    addPanel (5, "Analog Boost",   regionOffsets ("bassBoost"), 2);
    addPanel (5, "Distortion",     regionOffsets ("distortion"), 3);
}

void VCClonePanel::layoutPanels()
{
    // shelf-pack the panels of each tab, then stretch every shelf to the full width
    for (int tab = 0; tab < kNumTabs; ++tab)
    {
        std::vector<Panel*> list;
        for (auto& pn : panels) if (pn.tab == tab) list.push_back (&pn);
        if (list.empty()) continue;

        auto rowsFor = [] (const Panel& pn)
        {
            int rows = 1, used = 0;
            for (auto& it : pn.items)
            {
                if (used + it.cells > pn.cols) { ++rows; used = 0; }
                used += it.cells;
            }
            return rows;
        };
        const float left = 8.0f, right = (float) kWidth - 8.0f, gap = 6.0f;
        float x = left, y = (float) kContentY + 2.0f, shelfH = 0.0f;
        std::vector<Panel*> shelf;
        auto finishShelf = [&]
        {
            if (shelf.empty()) return;
            float used = 0.0f;
            for (auto* pn : shelf) used += pn->bounds.getWidth();
            const float extra = (right - left - gap * (float) (shelf.size() - 1)) - used;
            float xx = left;
            for (auto* pn : shelf)
            {
                const float w = pn->bounds.getWidth() + extra * pn->bounds.getWidth() / used;
                pn->bounds = { xx, pn->bounds.getY(), w, shelfH };
                xx += w + gap;
            }
            shelf.clear();
        };
        for (auto* pn : list)
        {
            const float w = (float) (pn->cols * kCellW + 16);
            const float h = (float) (18 + 6 + rowsFor (*pn) * kCellH + 6);
            if (x + w > right + 0.5f && ! shelf.empty())
            {
                finishShelf();
                y += shelfH + gap;
                x = left;
                shelfH = 0.0f;
            }
            pn->bounds = { x, y, w, h };
            shelf.push_back (pn);
            x += w + gap;
            shelfH = std::max (shelfH, h);
        }
        finishShelf();

        // place the items: grid centred inside each panel
        for (auto* pn : list)
        {
            const float gridW = (float) (pn->cols * kCellW);
            const float gx = pn->bounds.getX() + (pn->bounds.getWidth() - gridW) * 0.5f;
            const float gy = pn->bounds.getY() + 18.0f + 6.0f;
            int row = 0, col = 0;
            for (auto& it : pn->items)
            {
                if (col + it.cells > pn->cols) { ++row; col = 0; }
                const auto cell = Rectangle<float> (gx + (float) (col * kCellW), gy + (float) (row * kCellH), (float) (it.cells * kCellW), (float) kCellH);
                col += it.cells;
                switch (it.kind)
                {
                    case Item::Kind::Knob:
                        it.comp->setBounds (cell.reduced (3.0f, 2.0f).toNearestInt());
                        break;
                    case Item::Kind::Toggle:
                        it.comp->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 8.0f, 24.0f).toNearestInt());
                        break;
                    case Item::Kind::Combo:
                    {
                        auto* c = static_cast<VirusCombo*> (it.comp);
                        c->setBounds (cell.withTrimmedTop (30.0f).withHeight (24.0f).reduced (5.0f, 0.0f).toNearestInt());
                        const int li = c->getProperties()["labelIndex"];
                        comboLabels[(size_t) li]->setBounds (cell.withTrimmedTop (10.0f).withHeight (16.0f).reduced (2.0f, 0.0f).toNearestInt());
                        break;
                    }
                    case Item::Kind::Preview:
                        it.comp->setBounds (cell.reduced (5.0f, 8.0f).toNearestInt());
                        break;
                }
            }
        }
    }
}

void VCClonePanel::showTab (int t)
{
    currentTab = t;
    for (int i = 0; i < kNumTabs; ++i)
        tabs[(size_t) i].setToggleState (i == t, dontSendNotification);
    for (auto& pn : panels)
        for (auto& it : pn.items)
        {
            it.comp->setVisible (pn.tab == t);
            if (it.kind == Item::Kind::Combo)
            {
                const int li = it.comp->getProperties()["labelIndex"];
                comboLabels[(size_t) li]->setVisible (pn.tab == t);
            }
        }
    const bool browser = t == kNumTabs - 1;
    patchList.setVisible (browser);
    infoText.setVisible (browser);
    if (browser) infoText.setText (buildInfoText(), false);
    repaint();
}

//==============================================================================
void VCClonePanel::resized()
{
    for (int i = 0; i < kNumTabs; ++i)
    {
        const int w = (kWidth - 16) / kNumTabs;
        tabs[(size_t) i].setBounds (8 + i * w, kTabsY, w - 6, kTabsH);
    }
    loadButton.setBounds (318, 9, 60, 24);
    exportButton.setBounds (384, 9, 64, 24);
    initButton.setBounds (454, 9, 50, 24);
    presetBox.setBounds (612, 7, 300, 28);
    prevButton.setBounds (918, 9, 32, 24);
    nextButton.setBounds (954, 9, 32, 24);

    patchList.setBounds (16, kContentY + 30, 440, kContentBottom - kContentY - 40);
    infoText.setBounds (476, kContentY + 30, kWidth - 492, kContentBottom - kContentY - 40);

    lcd.setBounds (8, kLcdY, 600, kLcdH);
    statusLcd.setBounds (616, kLcdY, kWidth - 624, kLcdH);
    helpButton.setBounds (kWidth - 34, kBottomBarY + 5, 22, 22);
    helpOverlay.setBounds (getLocalBounds());
}

void VCClonePanel::createBackground()
{
    background = Image (Image::RGB, kWidth * 2, kHeight * 2, true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (2.0f));

    g.setGradientFill (ColourGradient (Colour (0xff3b3e42), 0.0f, 0.0f, Colour (0xff1e2023), 0.0f, (float) kHeight, false));
    g.fillAll();

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

void VCClonePanel::drawWordmark (Graphics& g, Rectangle<float> area)
{
    // Original stylised lowercase wordmark: chrome "vc", blue "clone", and a swoosh.
    const float baseline = area.getBottom() - 7.0f;

    GlyphArrangement main;
    main.addLineOfText (Font (FontOptions (27.0f, Font::bold | Font::italic)), "vc", area.getX(), baseline);
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
    second.addLineOfText (Font (FontOptions (19.0f, Font::plain)), "clone", mainBounds.getRight() + 6.0f, baseline);
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

void VCClonePanel::paint (Graphics& g)
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

    drawWordmark (g, { 16.0f, 2.0f, 260.0f, (float) kTopStripH - 2.0f });
    CustomLookAndFeel::drawSmallLabel (g, "Preset", { 556.0f, 10.0f, 50.0f, 22.0f }, Justification::centredRight);

    // tab groove
    CustomLookAndFeel::drawInsetBox (g, { 5.0f, (float) kTabsY - 3.0f, (float) kWidth - 10.0f, (float) kTabsH + 6.0f }, 4.0f);

    // panels of the current tab
    for (auto& pn : panels)
        if (pn.tab == currentTab)
            CustomLookAndFeel::drawPanel (g, pn.bounds, pn.title);

    if (currentTab == kNumTabs - 1)
    {
        CustomLookAndFeel::drawPanel (g, { 8.0f, (float) kContentY + 2.0f, 456.0f, (float) (kContentBottom - kContentY - 4) },
                                      processor.getBankName().isEmpty() ? String ("Patches") : "Patches - " + processor.getBankName());
        CustomLookAndFeel::drawPanel (g, { 468.0f, (float) kContentY + 2.0f, (float) kWidth - 476.0f, (float) (kContentBottom - kContentY - 4) },
                                      "Patch Info / Warnings / Ignored Parameters");
    }

    // bottom bar
    auto bar = Rectangle<float> (0.0f, (float) kBottomBarY, (float) kWidth, (float) (kHeight - kBottomBarY));
    g.setGradientFill (ColourGradient (Colour (0xff2e3135), 0.0f, bar.getY(), Colour (0xff1b1d20), 0.0f, bar.getBottom(), false));
    g.fillRect (bar);
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kBottomBarY - 1, 0.0f, (float) kWidth);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (kBottomBarY, 0.0f, (float) kWidth);

    auto tab = Rectangle<float> (8.0f, bar.getY() + 6.0f, 60.0f, 18.0f);
    g.setGradientFill (ColourGradient (VirusColours::headerTop, 0.0f, tab.getY(), VirusColours::headerBottom, 0.0f, tab.getBottom(), false));
    g.fillRoundedRectangle (tab, 3.0f);
    g.setColour (VirusColours::bevelDark);
    g.drawRoundedRectangle (tab, 3.0f, 1.0f);
    g.setColour (VirusColours::headerText);
    g.setFont (CustomLookAndFeel::uiFont (10.5f, true));
    g.drawText ("SINGLE", tab, Justification::centred);

    auto inset = bar.reduced (90.0f, 6.0f);
    CustomLookAndFeel::drawInsetBox (g, inset, 3.0f);
    g.setFont (CustomLookAndFeel::uiFont (13.0f, true));
    g.setColour (Colour (0xffc8cdd2));
    g.drawText ("VC-CLONE", inset, Justification::centred);
}

//==============================================================================
float VCClonePanel::previewValue (Preview kind, float x) const
{
    auto val = [this] (int o) { auto* prm = processor.getParam (o); return prm ? prm->convertFrom0to1 (prm->getValue()) : 0.0f; };
    const auto& waves = vc::WaveBank::get();
    switch (kind)
    {
        case Preview::Osc1:
        case Preview::Osc2:
        {
            const bool one = kind == Preview::Osc1;
            const float shape = val (one ? Osc1_Shape : Osc2_Shape);
            const int wave = (int) val (one ? Osc1_Wave_Select : Osc2_Wave_Select);
            const double duty = vc::tables::pulseDuty (val (one ? Osc1_Pulsewidth : Osc2_Pulsewidth));
            const double ph = std::fmod (x * 2.0, 1.0);
            const double saw = 2.0 * ph - 1.0;
            if (shape <= 64.0f)
            {
                const double a = shape / 64.0;
                return (float) ((1.0 - a) * waves.sampleRaw (wave, ph) + a * saw) * 0.9f;
            }
            const double b = (shape - 64.0) / 63.0;
            return (float) ((1.0 - b) * saw + b * (ph < duty ? 1.0 : -1.0)) * 0.9f;
        }
        case Preview::FiltEnv:
        case Preview::AmpEnv:
        {
            const bool amp = kind == Preview::AmpEnv;
            const double a = vc::tables::envTimeSec (val (amp ? Amp_Env_Attack : Filter_Env_Attack));
            const double d = vc::tables::envTimeSec (val (amp ? Amp_Env_Decay : Filter_Env_Decay));
            const double s = val (amp ? Amp_Env_Sustain : Filter_Env_Sustain) / 127.0;
            const double slope = vc::tables::sustainSlopeSec ((int) val (amp ? Amp_Env_Sustain_Time : Filter_Env_Sustain_Time));
            const double r = vc::tables::envTimeSec (val (amp ? Amp_Env_Release : Filter_Env_Release));
            // log-ish time axis: attack, decay, 1 s of sustain, release, each squeezed into a quarter
            auto squash = [] (double t) { return std::log1p (t * 20.0) / std::log1p (20.0 * 20.0); };
            const double qa = std::max (0.02, squash (a) * 0.3), qd = std::max (0.02, squash (d) * 0.3), qr = std::max (0.02, squash (r) * 0.3);
            const double total = qa + qd + 0.3 + qr;
            double t = x * total, level;
            if (t < qa) level = t / qa;
            else if ((t -= qa) < qd) level = s + (1.0 - s) * std::exp (-5.0 * t / qd);
            else if ((t -= qd) < 0.3) level = std::clamp (s + (slope == 0.0 ? 0.0 : (t / 0.3) * 1.0 / slope), 0.0, 1.0);
            else
            {
                t -= 0.3;
                const double held = std::clamp (s + (slope == 0.0 ? 0.0 : 1.0 / slope), 0.0, 1.0);
                level = held * std::exp (-5.0 * t / qr);
            }
            return (float) (level * 1.8 - 0.9);
        }
        case Preview::Lfo1:
        case Preview::Lfo2:
        case Preview::Lfo3:
        {
            const int shapeOff = kind == Preview::Lfo1 ? Lfo1_Shape : kind == Preview::Lfo2 ? Lfo2_Shape : Lfo3_Shape;
            const int shape = (int) val (shapeOff);
            float sym = 0.0f;
            if (kind != Preview::Lfo3) sym = (val (kind == Preview::Lfo1 ? Lfo1_Symmetry : Lfo2_Symmetry) - 64.0f) / 64.0f;
            double phase = std::fmod (x * 2.0, 1.0);
            const double s = std::clamp (0.5 + 0.48 * sym, 0.02, 0.98);
            const double ph = phase < s ? 0.5 * phase / s : 0.5 + 0.5 * (phase - s) / (1.0 - s);
            static const float sh[8] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.2f, -0.5f, 0.7f, -0.1f };
            const int step = (int) (x * 8.0f) & 7;
            switch (shape)
            {
                case 0: return (float) std::sin (MathConstants<double>::twoPi * ph) * 0.9f;
                case 1: return (float) (ph < 0.25 ? 4 * ph : ph < 0.75 ? 2 - 4 * ph : 4 * ph - 4) * 0.9f;
                case 2: return (float) (1.0 - 2.0 * ph) * 0.9f;
                case 3: return ph < 0.5 ? 0.9f : -0.9f;
                case 4: return sh[step] * 0.9f;
                case 5: { const float f = x * 8.0f - (float) (int) (x * 8.0f); return (sh[(step + 7) & 7] + (sh[step] - sh[(step + 7) & 7]) * f) * 0.9f; }
                default: return waves.sampleRaw (std::clamp (shape - 4, 2, 63), ph) * 0.9f;
            }
        }
        default: return 0.0f;
    }
}

//==============================================================================
void VCClonePanel::touched (int offset)
{
    lcdOffset = offset;
    processor.lastTouched = offset;
}

void VCClonePanel::timerCallback()
{
    // follow parameter changes (mouse, host automation, MIDI CC) on the LCD
    for (int o = 0; o < 256; ++o)
        if (auto* prm = processor.getParam (o))
        {
            const int v = roundToInt (prm->convertFrom0to1 (prm->getValue()));
            if (shownValues[(size_t) o] != v)
            {
                if (shownValues[(size_t) o] >= 0 && lastBankVersion == processor.bankVersion.load()) lcdOffset = o;
                shownValues[(size_t) o] = v;
                for (auto& [pv, kind] : previewKinds) pv->repaint();
            }
        }

    if (lcdOffset >= 0)
    {
        auto* prm = processor.getParam (lcdOffset);
        const int v = prm ? roundToInt (prm->convertFrom0to1 (prm->getValue())) : 0;
        lcd.setContent (String (L::kParams[lcdOffset].name).trim().toUpperCase(), String (vc::valueToText (lcdOffset, v)));
    }
    else
    {
        const auto patch = processor.currentPatch();
        lcd.setContent (String (patch.name()).toUpperCase(), String (vc::valueToText (Category1, patch.value (Category1))));
    }

    const double now = Time::getMillisecondCounterHiRes();
    if (statusMessage.isNotEmpty() && now < statusUntil)
        statusLcd.setContent (statusMessage, {});
    else
    {
        int clamped = 0, checksum = 0;
        for (auto& w : processor.getWarnings())
        {
            clamped += w.kind == vc::ImportWarning::Kind::Clamped ? 1 : 0;
            checksum += w.kind == vc::ImportWarning::Kind::Checksum ? 1 : 0;
        }
        String s = "VOICES " + String (processor.activeVoices.load());
        if (clamped + checksum > 0) s << "   WARN " << (clamped + checksum);
        statusLcd.setContent (s, processor.getBank().empty() ? String ("INIT") : String (processor.getCurrentProgram() + 1) + "/" + String ((int) processor.getBank().size()));
    }

    if (lastBankVersion != processor.bankVersion.load())
    {
        lastBankVersion = processor.bankVersion.load();
        refreshPresetList();
        lcdOffset = -1;
        if (currentTab == kNumTabs - 1) infoText.setText (buildInfoText(), false);
        repaint();
    }
}

void VCClonePanel::refreshPresetList()
{
    presetBox.clear (dontSendNotification);
    const auto& bank = processor.getBank();
    for (int i = 0; i < (int) bank.size(); ++i)
        presetBox.addItem (String (i + 1).paddedLeft ('0', 3) + "  " + bank[(size_t) i].name(), i + 1);
    if (bank.empty())
    {
        presetBox.addItem ("---  " + String (processor.currentPatch().name()), 1);
        presetBox.setSelectedId (1, dontSendNotification);
    }
    else
        presetBox.setSelectedId (processor.getCurrentProgram() + 1, dontSendNotification);
    prevButton.setEnabled (bank.size() > 1);
    nextButton.setEnabled (bank.size() > 1);
    patchList.updateContent();
    patchList.selectRow (processor.getCurrentProgram(), true, true);
    patchList.repaint();
}

String VCClonePanel::buildInfoText() const
{
    String s;
    const auto patch = processor.currentPatch();
    s << "Current patch: " << patch.name() << "\n";
    s << "Category: " << vc::valueToText (Category1, patch.value (Category1)) << " / " << vc::valueToText (Category2, patch.value (Category2)) << "\n";
    s << "Version byte: " << patch.version() << (patch.version() == vc::kVirusCVersion ? " (Virus C)" : "") << "\n\n";

    const auto ignored = vc::ignoredParametersInUse (patch);
    s << "IGNORED PARAMETERS (not implemented in v1):\n";
    if (ignored.empty()) s << "  none used by this patch\n";
    for (int o : ignored) s << "  " << String (L::kParams[o].name).trim() << " = " << vc::valueToText (o, patch.value (o)) << "\n";
    s << "  (input, vocoder, input follower and second output are always ignored)\n\n";

    std::vector<int> oor;
    for (int o = 0; o < 256; ++o) if (patch.isOutOfRange (o)) oor.push_back (o);
    s << "OUT-OF-RANGE BYTES IN THIS PATCH (clamped, raw byte kept for export):\n";
    if (oor.empty()) s << "  none\n";
    for (int o : oor) s << "  " << String (L::kParams[o].name).trim() << " raw " << (int) patch.raw[(size_t) o] << " -> " << patch.value (o) << "\n";

    s << "\nBANK IMPORT WARNINGS (" << (int) processor.getWarnings().size() << "):\n";
    int shown = 0;
    for (auto& w : processor.getWarnings())
    {
        if (++shown > 200) { s << "  ...\n"; break; }
        s << "  " << (w.patchIndex >= 0 ? "[" + String (w.patchIndex + 1).paddedLeft ('0', 3) + "] " : String()) << String (w.message) << "\n";
    }

    s << "\nENGINE TABLES (guessed until calibrated against Osirus):\n";
    for (auto& t : vc::tables::tableStatus())
        s << "  " << String (t.name).paddedRight (' ', 18) << (t.status == vc::tables::Status::Measured ? "measured" : "GUESSED") << "\n";
    return s;
}

int VCClonePanel::getNumRows() { return (int) processor.getBank().size(); }

void VCClonePanel::paintListBoxItem (int row, Graphics& g, int width, int height, bool selected)
{
    const auto& bank = processor.getBank();
    if (row < 0 || row >= (int) bank.size()) return;
    if (selected)
    {
        g.setColour (Colour (0xff2a6fb8));
        g.fillRect (0, 0, width, height);
    }
    const auto& p = bank[(size_t) row];
    g.setFont (CustomLookAndFeel::uiFont (12.5f, selected));
    g.setColour (selected ? Colours::white : Colour (0xffdfe3e6));
    g.drawText (String (row + 1).paddedLeft ('0', 3), 6, 0, 34, height, Justification::centredLeft);
    g.drawText (p.name(), 44, 0, 150, height, Justification::centredLeft);
    g.setColour (selected ? Colours::white : VirusColours::labelText);
    g.drawText (String (vc::valueToText (Category1, p.value (Category1))), 200, 0, 110, height, Justification::centredLeft);
    bool warn = ! p.checksumOk;
    for (int o = 0; o < 256 && ! warn; ++o) warn = p.isOutOfRange (o);
    if (warn)
    {
        g.setColour (Colour (0xffffb340));
        g.drawText ("!", width - 20, 0, 14, height, Justification::centred);
    }
}

void VCClonePanel::listBoxItemClicked (int row, const MouseEvent&)
{
    processor.setCurrentProgram (row);
}

//==============================================================================
bool VCClonePanel::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".syx") || f.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void VCClonePanel::filesDropped (const StringArray& files, int, int)
{
    for (auto& f : files)
        if (isInterestedInFileDrag ({ f })) { loadFile (File (f)); break; }
}

void VCClonePanel::loadFile (const File& f)
{
    const auto err = processor.loadBankFile (f);
    if (err.isNotEmpty())
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Could not load " + f.getFileName(), err);
    else
    {
        statusMessage = "LOADED " + String ((int) processor.getBank().size()) + " PATCHES";
        statusUntil = Time::getMillisecondCounterHiRes() + 2500;
    }
}

void VCClonePanel::openLoadDialog()
{
    chooser = std::make_unique<FileChooser> ("Load Virus bank", File(), "*.mid;*.midi;*.syx");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
}

void VCClonePanel::openExportMenu()
{
    PopupMenu m;
    m.addItem (1, "Export current patch (.syx)");
    m.addItem (2, "Export current patch (.mid)");
    m.addItem (3, "Export bank (.syx)", ! processor.getBank().empty());
    m.addItem (4, "Export bank (.mid)", ! processor.getBank().empty());
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (exportButton), [this] (int r)
    {
        if (r == 0) return;
        const bool mid = r == 2 || r == 4, bank = r >= 3;
        const String ext = mid ? ".mid" : ".syx";
        const String name = bank ? File (processor.getBankName()).getFileNameWithoutExtension() + "_export"
                                 : String (processor.currentPatch().name()).trim().replaceCharacter (' ', '_');
        chooser = std::make_unique<FileChooser> ("Export", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (name + ext), "*" + ext);
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                              [this, bank, ext] (const FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == File()) return;
                                  f = f.withFileExtension (ext);
                                  const bool ok = bank ? processor.exportBank (f) : processor.exportCurrent (f);
                                  statusMessage = ok ? "EXPORTED " + f.getFileName().toUpperCase() : String ("EXPORT FAILED");
                                  statusUntil = Time::getMillisecondCounterHiRes() + 2500;
                              });
    });
}

//==============================================================================
VCCloneAudioProcessorEditor::VCCloneAudioProcessorEditor (VCCloneAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    const int savedWidth = processor.editorWidth.load();
    addAndMakeVisible (panel);

    setResizable (true, true);
    setResizeLimits (kWidth * 7 / 10, kHeight * 7 / 10, kWidth * 5 / 2, kHeight * 5 / 2);
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kHeight);

    const int width = savedWidth > 0 ? jlimit (kWidth * 7 / 10, kWidth * 5 / 2, savedWidth) : kWidth;
    setSize (width, roundToInt (width * (double) kHeight / (double) kWidth));
    sizeRestored = true;
}

void VCCloneAudioProcessorEditor::resized()
{
    panel.setBounds (0, 0, kWidth, kHeight);
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) kWidth));
    if (sizeRestored)
        processor.editorWidth.store (getWidth());
}
