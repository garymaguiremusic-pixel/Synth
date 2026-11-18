#include "PluginEditor.h"
#include "VirusCLayout.h"
#include "Tables/Tables.h"
#include "SpireLists.h"

#include <climits>
#include <cstring>

using namespace juce;
namespace L = vc::layout;
using namespace vc::layout::off;
namespace sp = vc::spire::idx;
using SpK = SpireAdapter::Kind;

namespace
{
    constexpr int kWidth = VCClonePanel::kBaseWidth, kHeight = VCClonePanel::kBaseHeight;

    // Spire value lists: the names Spire itself shows, per stored index (Engine/SpireLists.h, from vc_probe)
    template <size_t N> StringArray toList (const char* const (&items)[N])
    {
        StringArray out;
        for (auto* i : items) out.add (String (i).replace ("_", " "));
        return out;
    }
    namespace SL = vc::spire::lists;
    const StringArray kSpOscTypes = toList (SL::oscTypes);
    StringArray spWaves() { return toList (SL::waves); }
    const StringArray kSpUnison = toList (SL::unisonCounts);
    const StringArray kSpChords = toList (SL::unisonModes);
    const StringArray kSpFltTypes = toList (SL::filterTypes);
    const StringArray kSpFltModes = toList (SL::filterModes);
    const StringArray kSpVoice = toList (SL::voiceModes);
    const StringArray kSpPoly { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    const StringArray kSpBend { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24" };
    StringArray spLfoForms() { return toList (SL::lfoForms); }
    // in preset-file order (the order the stored values use)
    StringArray spTargets()
    {
        const auto names = toList (SL::targets);
        StringArray out;
        for (int i = 0; i < names.size(); ++i) out.add (names[vc::SpireEngine::targetFromStored (i)]);
        return out;
    }
    StringArray spSources()
    {
        const auto names = toList (SL::sources);
        StringArray out;
        for (int i = 0; i < names.size(); ++i) out.add (names[vc::SpireEngine::sourceFromStored (i)]);
        return out;
    }
    const StringArray kSpSync = toList (SL::arpSpeeds);

    bool isListParam (int o)
    {
        const std::string_view t = L::kParams[o].toText;
        if (t.empty()) return false;
        static constexpr std::string_view numeric[] = { "signed", "pan", "unsignedZero", "unsignedOne", "oscShape",
                                                        "envSustainTime", "arpSwing", "bpm", "filterKeytrackBase", "ascii" };
        for (auto n : numeric) if (t == n) return false;
        return true;
    }

    String shortVirusLabel (int o)
    {
        static const std::map<int, const char*> m = {
            { Osc1_Pulsewidth, "PW" }, { Osc2_Pulsewidth, "PW" }, { Osc1_Wave_Select, "Wave" }, { Osc2_Wave_Select, "Wave" },
            { Osc1_Shape_Velocity, "Shp Vel" }, { Osc2_Shape_Velocity, "Shp Vel" }, { Osc2_FM_Amount, "FM Amt" },
            { Osc2_Filt_Env_Amt, "FEnv>Pit" }, { FM_Filt_Env_Amt, "FEnv>FM" }, { Osc_FM_Mode, "FM Mode" }, { Fm_Amount_Velocity, "FM Vel" },
            { Osc_Balance, "Balance" }, { Suboscillator_Volume, "Sub Vol" }, { Suboscillator_Shape, "Sub Shp" }, { Osc_Mainvolume, "Osc Vol" },
            { Ringmodulator_Volume, "Ring" }, { Osc_Init_Phase, "Phase" }, { PulseWidth_Velocity, "PW Vel" }, { Noise_Volume, "Noise" },
            { Noise_Color, "Color" }, { Filter1_Resonance, "Reso" }, { Filter2_Resonance, "Reso" }, { Filter1_Env_Amt, "Env Amt" },
            { Filter2_Env_Amt, "Env Amt" }, { Filter1_Keyfollow, "Keyflw" }, { Filter2_Keyfollow, "Keyflw" }, { Filter1_Mode, "Mode" },
            { Filter2_Mode, "Mode" }, { Filter1_Env_Polarity, "Polarity" }, { Filter2_Env_Polarity, "Polarity" }, { Filter2_Cutoff_Link, "Link" },
            { Flt1_EnvAmt_Velocity, "Env Vel" }, { Flt2_EnvAmt_Velocity, "Env Vel" }, { Resonance1_Velocity, "Res Vel" },
            { Resonance2_Velocity, "Res Vel" }, { Filter_Balance, "Balance" }, { Saturation_Curve, "Saturat." }, { Filter_Routing, "Routing" },
            { Filter_Keytrack_Base, "Key Base" }, { Osc1_Lfo1_Amount, "Osc1" }, { Osc2_Lfo1_Amount, "Osc2" }, { PW_Lfo1_Amount, "PW" },
            { Reso_Lfo1_Amount, "Reso" }, { FiltGain_Lfo1_Amount, "Gain" }, { LFO1_Assign_Dest, "Assign" }, { LFO1_Assign_Amount, "Amount" },
            { Shape_Lfo2_Amount, "Shape" }, { FM_Lfo2_Amount, "FM" }, { Cutoff1_Lfo2_Amount, "Cut 1" }, { Cutoff2_Lfo2_Amount, "Cut 2" },
            { Pan_Lfo2_Amount, "Pan" }, { LFO2_Assign_Dest, "Assign" }, { LFO2_Assign_Amount, "Amount" }, { Osc_Lfo3_Amount, "Amount" },
            { Lfo3_Destination, "Dest" }, { Lfo3_Fade_In_Time, "Fade" }, { Patch_Volume, "Volume" }, { Amp_Velocity, "Velocity" },
            { Panorama_Velocity, "Pan Vel" }, { Control_Smooth_Mode, "Smooth" }, { Bender_Range_Up, "Bend Up" }, { Bender_Range_Down, "Bend Dn" },
            { Bender_Scale, "Bend Scl" }, { Unison_Mode, "Voices" }, { Unison_Pan_Spread, "Spread" }, { Unison_Lfo_Phase, "LFO Phs" },
            { Unison_Detune, "Detune" }, { Punch_Intensity, "Punch" }, { Portamento_Time, "Porta" }, { Channel_Volume, "Ch Vol" },
            { Clock_Tempo, "Tempo" }, { Panorama, "Pan" }, { Lfo1_Keytrigger, "KeyTrig" }, { Lfo2_Keytrigger, "KeyTrig" },
            { Lfo1_Env_Mode, "Env" }, { Lfo2_Env_Mode, "Env" }, { Lfo1_Symmetry, "Symm" }, { Lfo2_Symmetry, "Symm" },
            { Lfo1_Keyfollow, "Keyflw" }, { Lfo2_Keyfollow, "Keyflw" }, { Lfo3_Keyfollow, "Keyflw" },
        };
        if (auto it = m.find (o); it != m.end()) return it->second;
        String n = String (L::kParams[o].name).trim();
        for (auto prefix : { "Osc1 ", "Osc2 ", "Osc3 ", "Lfo1 ", "Lfo2 ", "Lfo3 ", "Filter Env ", "Amp Env ", "Assign1 ", "Assign2 ",
                             "Assign3 ", "Assign 4 ", "Assign 5 ", "Assign 6 ", "Unison " })
            if (n.startsWith (prefix)) { n = n.substring ((int) std::strlen (prefix)); break; }
        return n.replace ("Destination", "Dest").replace ("Sustain Time", "Sus Time").replace ("Amount", "Amt");
    }

    std::vector<int> regionOffsets (const char* id)
    {
        std::vector<int> out;
        for (const auto& r : L::kRegions)
            if (std::string_view (r.id) == id)
                for (int i = 0; i < r.count; ++i)
                    if (r.offsets[i] >= 0 && VCCloneAudioProcessor::isHostParameter (r.offsets[i])) out.push_back (r.offsets[i]);
        return out;
    }
}

//==============================================================================
std::unique_ptr<ParamAdapter> VCClonePanel::host (const char* id, String label)
{
    return std::make_unique<ApvtsAdapter> (*processor.apvts.getParameter (id), std::move (label));
}

std::unique_ptr<ParamAdapter> VCClonePanel::virus (int offset, String label)
{
    return std::make_unique<VirusAdapter> (*processor.getParam (offset), offset, label.isEmpty() ? shortVirusLabel (offset) : label,
                                           isListParam (offset));
}

namespace
{
    const char* kModeNames[] = { "VIRUS", "SYLENTH", "SPIRE" };
    constexpr int V = (int) vc::SynthMode::Virus, SY = (int) vc::SynthMode::Sylenth, SPm = (int) vc::SynthMode::Spire;
    const Colour kComboBlue { 0xff1f6bc4 }, kDim { 0xff8f99a3 }, kCaption { 0xffaab3bb };

    /** A waveform for the oscillator picture, from the wave's name. */
    float waveByName (const String& n, float p)
    {
        const auto s = n.toLowerCase();
        if (s.contains ("noise")) return Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        if (s.contains ("sine") || s.contains ("sin")) return std::sin (MathConstants<float>::twoPi * p);
        if (s.contains ("tri")) return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
        if (s.contains ("hpulse")) return p < 0.25f ? 1.0f : -1.0f;
        if (s.contains ("qpulse")) return p < 0.125f ? 1.0f : -1.0f;
        if (s.contains ("pulse") || s.contains ("square") || s.contains ("sq")) return p < 0.5f ? 1.0f : -1.0f;
        if (s.contains ("saw") || s.contains ("classic") || s.contains ("hyper")) return 1.0f - 2.0f * p;
        // wavetables and anything else: a soft harmonic shape
        return 0.6f * std::sin (MathConstants<float>::twoPi * p) + 0.3f * std::sin (3.0f * MathConstants<float>::twoPi * p);
    }
}

//==============================================================================
void VCClonePanel::TabButton::paintButton (Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool sel = getToggleState();
    const bool on = isOn ? isOn() : true;
    if (small)
    {
        // a numbered badge: the oscillator's on/off switch
        g.setColour (Colour (0xff15181b));
        g.fillRoundedRectangle (b, 4.0f);
        if (on)
        {
            g.setColour (VirusColours::blue.withAlpha (0.25f));
            g.drawRoundedRectangle (b.expanded (1.5f), 5.0f, 3.0f);
        }
        g.setColour (on ? VirusColours::blue : Colour (0xff3a3f45));
        g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
        g.setFont (CustomLookAndFeel::uiFont (b.getHeight() * 0.5f, true));
        g.setColour (on ? Colour (0xff7cc4ff) : kDim);
        g.drawText (getButtonText(), b, Justification::centred);
        return;
    }
    g.setColour (sel ? Colour (0xff15181b) : Colour (0xff1d2024));
    g.fillRoundedRectangle (b, 5.0f);
    if (sel)
    {
        g.setColour (VirusColours::blue.withAlpha (0.2f));
        g.drawRoundedRectangle (b.expanded (1.0f), 6.0f, 2.5f);
    }
    g.setColour (sel ? VirusColours::blue : (highlighted ? Colour (0xff3a3f45) : Colour (0xff0c0d0f)));
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
    const auto font = CustomLookAndFeel::uiFont (jmin (12.5f, b.getHeight() * 0.55f), true);
    const float tw = font.getStringWidthFloat (getButtonText());
    float x = b.getCentreX() - tw * 0.5f;
    if (isOn)
    {
        x += 8.0f;
        const auto dot = Rectangle<float> (x - 16.0f, b.getCentreY() - 4.0f, 8.0f, 8.0f);
        if (on) { g.setColour (VirusColours::blue.withAlpha (0.35f)); g.fillEllipse (dot.expanded (3.0f)); }
        g.setColour (on ? VirusColours::blue : Colour (0xff3a3f45));
        g.fillEllipse (dot);
    }
    g.setFont (font);
    g.setColour (sel ? Colour (0xff7cc4ff) : kDim);
    g.drawText (getButtonText(), Rectangle<float> (x, b.getY(), tw + 4.0f, b.getHeight()), Justification::centredLeft);
}

//==============================================================================
std::unique_ptr<ParamAdapter> VCClonePanel::fx (AudioProcessor& p, const char* id, String label)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (r->getParameterID() == id) return std::make_unique<ApvtsAdapter> (*r, std::move (label));
    return nullptr;
}

VCClonePanel::Control* VCClonePanel::makeControl (std::unique_ptr<ParamAdapter> a, bool forceKnob)
{
    if (! a) return nullptr;
    auto c = std::make_unique<Control>();
    auto* adapter = a.get();
    c->adapter = std::move (a);
    const auto list = adapter->items();
    const String fullName = adapter->name();
    auto touched = [this, adapter] { lcdName = adapter->name().toUpperCase(); lcdValue = adapter->text (adapter->get()); };

    if (adapter->isToggle())
    {
        c->kind = Control::Kind::Toggle;
        auto b = std::make_unique<TextButton> ("OFF");
        b->setClickingTogglesState (true);
        b->setTooltip (fullName);
        b->setColour (TextButton::textColourOnId, Colour (0xff06121e));
        b->setColour (TextButton::textColourOffId, kDim);
        b->onClick = [adapter, bp = b.get(), touched]
        {
            adapter->beginGesture(); adapter->set (bp->getToggleState() ? 1 : 0); adapter->endGesture(); touched();
        };
        c->comp = std::move (b);
    }
    else if (! list.isEmpty() && ! forceKnob)
    {
        c->kind = Control::Kind::Combo;
        auto box = std::make_unique<VirusCombo>();
        for (int i = 0; i < list.size(); ++i) box->addItem (list[i].isEmpty() ? String ("-") : list[i], i + 1);
        box->setTooltip (fullName);
        box->setColour (ComboBox::textColourId, Colours::white);
        box->setColour (ComboBox::arrowColourId, Colours::white);
        box->onChange = [adapter, bp = box.get(), touched]
        {
            const int id = bp->getSelectedId();
            if (id > 0) { adapter->beginGesture(); adapter->set (adapter->minimum() + id - 1); adapter->endGesture(); touched(); }
        };
        box->onTouched = touched;
        c->comp = std::move (box);
    }
    else
    {
        c->kind = Control::Kind::Knob;
        auto k = std::make_unique<VirusKnob> (VirusKnob::Size::Small, fullName.toLowerCase());
        k->setRange (adapter->minimum(), adapter->maximum(), 1.0);
        k->setDefaultValue (adapter->defaultValue());
        k->valueText = [adapter] (double v) { return adapter->text (roundToInt (v)); };
        k->onGestureBegin = [adapter] { adapter->beginGesture(); };
        k->onGestureEnd = [adapter] { adapter->endGesture(); };
        k->onValueChange = [adapter, kp = k.get(), touched] { adapter->set (roundToInt (kp->getValue())); touched(); };
        k->onTouched = touched;
        c->comp = std::move (k);
    }
    addChildComponent (*c->comp);
    controls.push_back (std::move (c));
    return controls.back().get();
}

VCClonePanel::Item& VCClonePanel::place (int mode, Component* comp, Rectangle<int> r, String caption)
{
    Item it;
    it.comp = comp; it.r = r; it.caption = std::move (caption); it.mode = mode; it.layer = curLayer;
    if (comp) comp->setBounds (r);
    items.push_back (it);
    return items.back();
}

VCClonePanel::Control* VCClonePanel::knob (int mode, std::unique_ptr<ParamAdapter> a, int x, int y, int w, int h)
{
    auto* c = makeControl (std::move (a), true);
    if (c) place (mode, c->comp.get(), { x, y, w, h }).control = c;
    return c;
}

VCClonePanel::Control* VCClonePanel::combo (int mode, std::unique_ptr<ParamAdapter> a, int x, int y, int w, String caption, int h)
{
    auto* c = makeControl (std::move (a));
    if (c) place (mode, c->comp.get(), { x, y, w, h }, std::move (caption)).control = c;
    return c;
}

VCClonePanel::Control* VCClonePanel::toggle (int mode, std::unique_ptr<ParamAdapter> a, int x, int y, int w, String caption, String onText)
{
    auto* c = makeControl (std::move (a));
    if (! c) return nullptr;
    c->comp->getProperties().set ("onText", onText);
    place (mode, c->comp.get(), { x, y, w, 24 }, std::move (caption)).control = c;
    return c;
}

VCClonePanel::Item& VCClonePanel::fixedBox (int mode, int x, int y, int w, String text, String caption)
{
    auto& it = place (mode, nullptr, { x, y, w, 24 }, std::move (caption));
    it.fixedText = String::fromUTF8 ("\xe2\x86\x92 ") + text;
    it.style = 1;
    return it;
}

VCClonePanel::Item& VCClonePanel::rowLabel (int mode, int x, int y, int w, String text, String sub)
{
    auto& it = place (mode, nullptr, { x, y, w, 30 }, std::move (sub));
    it.fixedText = std::move (text);
    it.style = 2;
    return it;
}

void VCClonePanel::separator (int mode, float x, float y, float w)
{
    separators.push_back ({ mode, curLayer, { x, y, w, 1.0f } });
}

int VCClonePanel::addTabs (int mode, Rectangle<int> area, StringArray names, std::vector<const char*> powerIds, bool small)
{
    auto g = std::make_unique<TabGroup>();
    g->mode = mode;
    const int gi = (int) tabGroups.size();
    const int n = names.size(), gap = small ? 0 : 6;
    const int w = (area.getWidth() - gap * (n - 1)) / jmax (1, n);
    for (int i = 0; i < n; ++i)
    {
        auto b = std::make_unique<TabButton> (names[i], small);
        auto* bp = b.get();
        if (i < (int) powerIds.size() && powerIds[(size_t) i] != nullptr)
        {
            auto* prm = processor.apvts.getParameter (powerIds[(size_t) i]);
            bp->isOn = [prm] { return prm->getValue() > 0.5f; };
            bp->togglePower = [prm] { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture(); };
            bp->setTooltip (small ? "Oscillator on / off" : "Click the dot to turn this oscillator on or off");
        }
        bp->onClick = [this, gi, i, bp]
        {
            if (bp->togglePower && (bp->small || bp->downX < 26)) { bp->togglePower(); bp->repaint(); return; }
            tabGroups[(size_t) gi]->selected = i;
            updateVisibility();
            repaint();
        };
        addChildComponent (*bp);
        place (mode, bp, small ? area : Rectangle<int> (area.getX() + i * (w + gap), area.getY(), w, area.getHeight()));
        g->buttons.push_back (std::move (b));
    }
    tabGroups.push_back (std::move (g));
    return gi;
}

void VCClonePanel::setGroup (int group, int index, size_t fromItem)
{
    for (size_t i = fromItem; i < items.size(); ++i) { items[i].group = group; items[i].index = index; }
}

VCClonePanel::Card& VCClonePanel::card (int mode, Rectangle<float> r, String title, const char* powerId)
{
    Card c;
    c.r = r; c.title = std::move (title); c.mode = mode; c.layer = curLayer;
    if (powerId != nullptr)
    {
        auto adapter = host (powerId, "On");
        auto pc = std::make_unique<Control>();
        pc->kind = Control::Kind::Toggle;
        auto* ad = adapter.get();
        pc->adapter = std::move (adapter);
        auto dot = std::make_unique<PowerDot>();
        dot->setTooltip (c.title + " on / off");
        dot->onClick = [ad, d = dot.get()] { ad->beginGesture(); ad->set (d->getToggleState() ? 1 : 0); ad->endGesture(); };
        pc->comp = std::move (dot);
        addChildComponent (*pc->comp);
        controls.push_back (std::move (pc));
        c.power = controls.back().get();
        place (mode, c.power->comp.get(), { (int) r.getX() + 8, (int) r.getY() + 6, 18, 18 }).control = c.power;
    }
    cards.push_back (c);
    return cards.back();
}

//==============================================================================
// Cards shared by every mode: amplifier extras, step sequencer, arpeggiator, delay, reverb, distortion
void VCClonePanel::buildShared()
{
    using AP = std::unique_ptr<ParamAdapter>;
    auto H = [this] (const char* id, const char* l) -> AP { return host (id, l); };

    // ENVELOPES: the global amp and filter envelopes (rows 1 and 2 in every mode), header switches
    curLayer = 0;
    {
        card (-1, { 806, 78, 374, 226 }, "ENVELOPES");
        toggle (-1, H (gid::useGlobalEnv, "Global ADSR"), 1006, 84, 52, {}, "GLOBAL");
        toggle (-1, H (gid::ampSync, "Amp Sync"), 1062, 84, 54, {}, "SYNC A");
        toggle (-1, H (gid::fltSync, "Filter Sync"), 1120, 84, 54, {}, "SYNC F");
        const char* amp[] = { gid::ampA, gid::ampD, gid::ampS, gid::ampR };
        const char* flt[] = { gid::fltA, gid::fltD, gid::fltS, gid::fltR };
        const char* adsr[] = { "a", "d", "s", "r" };
        rowLabel (-1, 816, 116, 52, "AMP", "volume");
        rowLabel (-1, 816, 165, 52, "FILTER", "cutoff");
        for (int i = 0; i < 4; ++i)
        {
            knob (-1, H (amp[i], adsr[i]), 870 + i * 38, 108, 38, 46);
            knob (-1, H (flt[i], adsr[i]), 870 + i * 38, 157, 38, 46);
        }
        knob (-1, H (gid::velocity, "vel"), 1022, 108, 50, 46);
        fixedBox (-1, 1074, 114, 96, "Volume", "send to");
        knob (-1, H (gid::envAmt, "amount"), 1022, 157, 50, 46);
        fixedBox (-1, 1074, 163, 96, "Cutoff", "send to");
        separator (-1, 816, 153, 354);
        separator (-1, 816, 202, 354);
        separator (-1, 816, 251, 354);
    }
    curLayer = -1;

    // AMPLIFIER: the global crossover (mono below) and phase lock
    card (-1, { 898, 314, 282, 104 }, "AMPLIFIER");
    knob (-1, H (gid::monoHz, "mono hz"), 1066, 350, 54, 52);
    knob (-1, H (gid::lockHz, "ph lock"), 1120, 350, 54, 52);

    // STEP SEQUENCER (ArpSampler): swing, key, scale, scale lock, the step grid; Randomise and Drag MIDI
    auto& seqState = processor.stepSequencer().state();
    auto Q = [&] (const char* id, const char* l) -> AP { return std::make_unique<ApvtsAdapter> (*seqState.getParameter (id), l); };
    card (-1, { 20, 428, 432, 178 }, "STEP SEQUENCER", gid::arpMode);
    rowLabel (-1, 30, 460, 30, "KEY", {}).style = 3;
    combo (-1, Q ("scaleKey", "Key"), 62, 464, 52, {}, 22);
    rowLabel (-1, 122, 460, 44, "SCALE", {}).style = 3;
    combo (-1, Q ("scale", "Scale"), 166, 464, 120, {}, 22);
    toggle (-1, Q ("scaleLock", "Scale Lock"), 294, 464, 100, {}, "SCALE LOCK");
    randomiseButton.setTooltip ("Randomise the steps (follows the key and scale when Scale Lock is on)");
    randomiseButton.onClick = [this] { processor.stepSequencer().randomise(); lcdName = "STEP SEQUENCER"; lcdValue = "RANDOMISED"; };
    addChildComponent (randomiseButton);
    place (-1, &randomiseButton, { 236, 434, 98, 22 });
    dragHandle = vendor::StepSequencer::createDragHandle ([this]
    {
        return processor.stepSequencer().exportMidi (processor.currentBpm.load(), processor.getLastHeldNotes());
    });
    addChildComponent (*dragHandle);
    place (-1, dragHandle.get(), { 340, 434, 102, 22 });
    stepGrid = processor.stepSequencer().createPanel();
    addChildComponent (*stepGrid);
    place (-1, stepGrid.get(), { 28, 494, 416, 106 });
    // the grid at 3/4 size, so every lane fits the card
    stepGrid->setBounds (0, 0, 555, 141);
    stepGrid->setTransform (AffineTransform::scale (0.75f).translated (28.0f, 494.0f));

    // ARPEGGIATOR: the sequencer's playback (pattern, speed, rate type)
    card (-1, { 20, 616, 432, 96 }, "ARPEGGIATOR");
    combo (-1, Q ("arpMode", "Mode"), 30, 654, 92, "mode");
    combo (-1, Q ("arpRate", "Speed"), 128, 654, 70, "speed");
    combo (-1, Q ("arpRateType", "Type"), 204, 654, 86, "type");
    knob (-1, Q ("arpSwing", "swing"), 294, 644, 50, 54);
    knob (-1, Q ("arpGate", "gate"), 344, 644, 50, 54);
    knob (-1, Q ("arpOctave", "octave"), 394, 644, 52, 54);

    // DELAY (Virus Delay)
    auto& dl = processor.delayProcessor();
    card (-1, { 462, 428, 290, 178 }, "DELAY", gid::delayOn);
    {
        const std::pair<const char*, const char*> k[] = { { "send", "send" }, { "feedback", "fbk" }, { "depth", "depth" }, { "color", "color" }, { "rate", "rate" } };
        for (int i = 0; i < 5; ++i) knob (-1, fx (dl, k[i].first, k[i].second), 470 + i * 55, 464, 54, 54);
        combo (-1, fx (dl, "mode", "Type"), 478, 548, 122, "type");
        combo (-1, fx (dl, "clock", "Clock"), 614, 548, 122, "time");
    }
    // REVERB (LeadVerb)
    auto& rv = processor.reverbProcessor();
    card (-1, { 762, 428, 186, 178 }, "REVERB", gid::reverbOn);
    {
        combo (-1, fx (rv, "mode", "Mode"), 868, 434, 74, {}, 20);
        const std::pair<const char*, const char*> k[] = { { "dryWet", "mix" }, { "decay", "decay" }, { "predelay", "pre-dly" },
                                                          { "damp", "damp" }, { "color", "color" }, { "wide", "wide" } };
        for (int i = 0; i < 6; ++i) knob (-1, fx (rv, k[i].first, k[i].second), 768 + (i % 3) * 58, i < 3 ? 464 : 534, 58, 54);
    }
    // DISTORTION (Virus saturation)
    card (-1, { 958, 428, 222, 178 }, "DISTORTION", gid::distOn);
    combo (-1, H (gid::satCurve, "Curve"), 976, 470, 186, "curve", 30);
    knob (-1, H (gid::satDrive, "drive"), 1002, 520, 70, 70);
    knob (-1, H (gid::satMix, "mix"), 1088, 526, 56, 58);

    card (-1, { 462, 616, 124, 96 }, "GLIDE");
    card (-1, { 596, 616, 232, 96 }, "PITCH");
    card (-1, { 838, 616, 124, 96 }, "VOICES");
}

//==============================================================================
void VCClonePanel::buildVirus()
{
    using AP = std::unique_ptr<ParamAdapter>;
    auto Vp = [this] (int o, const char* l) -> AP { return VCCloneAudioProcessor::isHostParameter (o) ? virus (o, l) : nullptr; };
    auto H = [this] (const char* id, const char* l) -> AP { return host (id, l); };
    auto oct = [&] (int o) -> AP { return std::make_unique<OctaveAdapter> (Vp (o, "octave"), 64); };
    auto cut = [] (AP a, const char* l) -> AP { return a ? std::make_unique<CutoffAdapter> (std::move (a), l) : nullptr; };

    // OSCILLATORS: three rows, a numbered on/off badge each; unison and sub / noise / ring underneath
    card (V, { 20, 78, 432, 340 }, "OSCILLATORS");
    struct Row { int semi, model; const char* modelId; int k3, k4, k5; const char *l3, *l4, *l5; };
    const Row rows[] = {
        { Osc1_Semitone, -1, gid::virusOscModel[0], Osc1_Shape, Osc1_Pulsewidth, Osc1_Wave_Select, "shape", "pw", "wave" },
        { Osc2_Semitone, -1, gid::virusOscModel[1], Osc2_Shape, Osc2_Pulsewidth, Osc2_Detune, "shape", "pw", "detune" },
        { Osc3_Semitone, Osc3_Mode, nullptr, Osc3_Detune, Osc3_Volume, -1, "detune", "level", "" },
    };
    for (int i = 0; i < 3; ++i)
    {
        const auto& r = rows[i];
        const int y = 116 + i * 70;
        addTabs (V, { 32, y + 6, 34, 34 }, { String (i + 1) }, { gid::oscOn[i] }, true);
        knob (V, oct (r.semi), 72, y, 50, 52);
        knob (V, Vp (r.semi, "semi"), 122, y, 50, 52);
        if (r.modelId) combo (V, H (r.modelId, "Model"), 176, y + 8, 110, "model");
        else combo (V, Vp (r.model, "Mode"), 176, y + 8, 110, "mode");
        knob (V, Vp (r.k3, r.l3), 290, y, 50, 52);
        knob (V, Vp (r.k4, r.l4), 340, y, 50, 52);
        if (r.k5 >= 0) knob (V, Vp (r.k5, r.l5), 390, y, 54, 52);
    }
    separator (V, 32, 330, 408);
    const std::pair<int, const char*> bottom[] = { { Suboscillator_Volume, "sub" }, { Noise_Volume, "noise" }, { Ringmodulator_Volume, "ring" },
                                                   { Unison_Mode, "voices" }, { Unison_Detune, "detune" }, { Unison_Pan_Spread, "spread" } };
    for (int i = 0; i < 6; ++i) knob (V, Vp (bottom[i].first, bottom[i].second), 28 + i * 48, 344, 48, 52);
    for (int i = 0; i < 3; ++i) toggle (V, H (gid::virusRetrig[i], "Retrig"), 322 + i * 40, 352, 36, "retrig " + String (i + 1), String (i + 1));

    // LFOS: three rows; each sends to the target in its dropdown
    curLayer = 0;
    card (V, { 462, 78, 334, 226 }, "LFOS");
    struct L { int shape, rate, amount, clock, dest; };
    const L lfos[] = { { Lfo1_Shape, Lfo1_Rate, LFO1_Assign_Amount, Lfo1_Clock, LFO1_Assign_Dest },
                       { Lfo2_Shape, Lfo2_Rate, LFO2_Assign_Amount, Lfo2_Clock, LFO2_Assign_Dest },
                       { Lfo3_Shape, Lfo3_Rate, Osc_Lfo3_Amount, Lfo3_Clock, Lfo3_Destination } };
    for (int i = 0; i < 3; ++i)
    {
        const int y = 114 + i * 62;
        rowLabel (V, 472, y + 8, 26, String (i + 1), {});
        combo (V, Vp (lfos[i].shape, "Shape"), 498, y + 6, 58, "shape");
        knob (V, Vp (lfos[i].rate, "rate"), 558, y, 44, 50);
        knob (V, Vp (lfos[i].amount, "amount"), 602, y, 48, 50);
        combo (V, Vp (lfos[i].clock, "Clock"), 652, y + 6, 46, "clock");
        combo (V, Vp (lfos[i].dest, "Send To"), 702, y + 6, 84, "send to");
        if (i < 2) separator (V, 472, (float) y + 58, 314);
    }

    // ENVELOPES rows 1-2 (global amp / filter), row 3: the filter envelope also bends osc 2 and FM
    rowLabel (V, 816, 214, 52, "F.ENV", "also to");
    knob (V, Vp (Osc2_Filt_Env_Amt, "pitch 2"), 874, 206, 56, 46);
    fixedBox (V, 934, 212, 86, "Pitch 2", "send to");
    knob (V, Vp (FM_Filt_Env_Amt, "fm"), 1022, 206, 50, 46);
    fixedBox (V, 1074, 212, 96, "FM", "send to");
    curLayer = -1;

    // FILTER: F1, F2 and the global filter on tabs
    card (V, { 462, 314, 426, 104 }, "FILTER", nullptr);
    const int fg = addTabs (V, { 696, 320, 184, 20 }, { "F1", "F2", "GLOBAL" }, {}, false);
    size_t from = items.size();
    combo (V, Vp (Filter1_Mode, "Type"), 474, 358, 104, "type");
    knob (V, cut (Vp (Cutoff, "cutoff"), "cutoff"), 584, 346, 62, 64);
    knob (V, Vp (Filter1_Resonance, "reso"), 650, 350, 46, 52);
    knob (V, Vp (Filter1_Env_Amt, "env"), 696, 350, 46, 52);
    knob (V, Vp (Filter1_Keyfollow, "keytrk"), 742, 350, 46, 52);
    knob (V, Vp (Filter_Balance, "balance"), 788, 350, 46, 52);
    combo (V, Vp (Filter_Routing, "Routing"), 836, 358, 46, "route");
    setGroup (fg, 0, from); from = items.size();
    combo (V, Vp (Filter2_Mode, "Type"), 474, 358, 104, "type");
    knob (V, cut (Vp (Cutoff2, "cutoff 2"), "cutoff 2"), 584, 346, 62, 64);
    knob (V, Vp (Filter2_Resonance, "reso"), 650, 350, 46, 52);
    knob (V, Vp (Filter2_Env_Amt, "env"), 696, 350, 46, 52);
    knob (V, Vp (Filter2_Keyfollow, "keytrk"), 742, 350, 46, 52);
    toggle (V, Vp (Filter2_Cutoff_Link, "Link"), 792, 358, 42, "link");
    setGroup (fg, 1, from);

    // AMPLIFIER (mode part)
    knob (V, Vp (Patch_Volume, "volume"), 904, 350, 54, 52);
    knob (V, Vp (Panorama, "pan"), 958, 350, 54, 52);
    knob (V, Vp (Amp_Velocity, "velocity"), 1012, 350, 54, 52);

    knob (V, Vp (Portamento_Time, "time"), 494, 646, 60, 56);
    knob (V, Vp (Bender_Range_Up, "bend up"), 606, 646, 54, 56);
    knob (V, Vp (Bender_Range_Down, "bend dn"), 662, 646, 54, 56);
    knob (V, Vp (Transpose, "transpose"), 718, 646, 60, 56);
    combo (V, Vp (Key_Mode, "Key Mode"), 852, 656, 96, "key mode");
}

void VCClonePanel::buildSylenth()
{
    using namespace vc::sylenth;
    using AP = std::unique_ptr<ParamAdapter>;
    auto S = [this] (int i, const char* l) -> AP { return std::make_unique<SylenthAdapter> (processor, i, l); };
    auto cut = [] (AP a, const char* l) -> AP { return std::make_unique<CutoffAdapter> (std::move (a), l); };

    card (SY, { 20, 78, 432, 340 }, "OSCILLATORS");
    const int og = addTabs (SY, { 32, 112, 408, 28 }, { "A1", "A2", "B1", "B2" }, { gid::oscOn[0], gid::oscOn[1], gid::oscOn[2], gid::oscOn[3] }, false);
    for (int k = 0; k < 4; ++k)
    {
        const size_t from = items.size();
        const int b = OscBase + k * OscStride;
        auto* w = makeControl (S (b + oWave, "Wave"));
        place (SY, w->comp.get(), { 32, 236, 96, 24 }, "wave").control = w;
        auto sc = std::make_unique<WaveScope>();
        auto* wa = w->adapter.get();
        sc->fn = [wa] (float p) { return waveByName (wa->text (wa->get()), p); };
        auto* vo = S (b + oVoices, "voices").release();
        std::unique_ptr<ParamAdapter> voicesHolder (vo);
        sc->layers = [vo] { return jlimit (1, 5, vo->get() + 1); };
        addChildComponent (*sc);
        place (SY, sc.get(), { 32, 148, 408, 70 });
        scopes.push_back (std::move (sc));
        knob (SY, std::move (voicesHolder), 132, 226, 52, 52);
        knob (SY, S (b + oDetune, "detune"), 184, 226, 52, 52);
        knob (SY, S (b + oOctave, "octave"), 236, 226, 52, 52);
        knob (SY, S (b + oNote, "note"), 288, 226, 52, 52);
        knob (SY, S (b + oFine, "fine"), 340, 226, 52, 52);
        toggle (SY, S (b + oKeytrack, "Keytrack"), 396, 234, 44, "keytrk");
        knob (SY, S (b + oVolume, "volume"), 28, 286, 52, 52);
        knob (SY, S (b + oPan, "pan"), 80, 286, 52, 52);
        knob (SY, S (b + oPhase, "phase"), 132, 286, 52, 52);
        knob (SY, S (b + oStereo, "stereo"), 184, 286, 52, 52);
        toggle (SY, S (b + oRetrig, "Retrig"), 244, 294, 50, "retrig");
        toggle (SY, S (b + oInvert, "Invert"), 300, 294, 50, "invert");
        setGroup (og, k, from);
    }
    separator (SY, 32, 350, 408);
    knob (SY, S (MixA, "mix A"), 28, 358, 52, 52);
    knob (SY, S (MixB, "mix B"), 80, 358, 52, 52);
    combo (SY, S (FiltAInput, "Filter A Input"), 140, 366, 92, "filter A in");
    combo (SY, S (FiltBInput, "Filter B Input"), 240, 366, 92, "filter B in");

    // LFOS: two rows and their second targets
    curLayer = 0;
    card (SY, { 462, 78, 334, 226 }, "LFOS");
    for (int l = 0; l < 2; ++l)
    {
        const int b = l == 0 ? Lfo1 : Lfo2, y = 116 + l * 64;
        rowLabel (SY, 472, y + 8, 26, String (l + 1), {});
        combo (SY, S (b, "Wave"), 498, y + 6, 58, "shape");
        knob (SY, S (b + 1, "rate"), 558, y, 44, 50);
        knob (SY, S (b + 6, "amount"), 602, y, 48, 50);
        combo (SY, S (b + 2, "Sync"), 652, y + 6, 46, "sync");
        combo (SY, S (b + 5, "Send To"), 702, y + 6, 84, "send to");
        separator (SY, 472, (float) y + 58, 314);
    }
    for (int l = 0; l < 2; ++l)
    {
        const int b = l == 0 ? Lfo1 : Lfo2, x = 500 + l * 144;
        combo (SY, S (b + 7, "Send To 2"), x, 254, 84, "LFO " + String (l + 1) + " also to");
        knob (SY, S (b + 8, "amt"), x + 86, 246, 46, 50);
    }

    // ENVELOPES rows 3-4: the two mod envelopes, each with a send-to target
    for (int e = 0; e < 2; ++e)
    {
        const int b = e == 0 ? Mod1 : Mod2, y = 206 + e * 48;
        rowLabel (SY, 816, y + 8, 52, "MOD " + String (e + 1), e == 0 ? "filter" : "free");
        for (int i = 0; i < 4; ++i) knob (SY, S (b + i, i == 0 ? "a" : i == 1 ? "d" : i == 2 ? "s" : "r"), 870 + i * 38, y, 38, 46);
        knob (SY, S (b + 5, "amount"), 1022, y, 50, 46);
        combo (SY, S (b + 4, "Send To"), 1074, y + 6, 96, "send to");
    }
    curLayer = -1;

    // FILTER: A, B, control
    card (SY, { 462, 314, 426, 104 }, "FILTER");
    const int fg = addTabs (SY, { 664, 320, 216, 20 }, { "A", "B", "CONTROL" }, {}, false);
    for (int k = 0; k < 2; ++k)
    {
        const size_t from = items.size();
        const int b = k == 0 ? FiltA : FiltB;
        combo (SY, S (b, "Type"), 474, 358, 100, "type");
        knob (SY, cut (S (b + 2, "cutoff"), "cutoff"), 580, 346, 62, 64);
        knob (SY, S (b + 3, "reso"), 646, 350, 50, 52);
        knob (SY, S (b + 4, "drive"), 696, 350, 50, 52);
        combo (SY, S (b + 1, "Slope"), 752, 358, 62, "slope");
        setGroup (fg, k, from);
    }
    {
        const size_t from = items.size();
        knob (SY, cut (S (FCtl, "cutoff"), "cutoff"), 480, 346, 62, 64);
        knob (SY, S (FCtl + 1, "reso"), 548, 350, 50, 52);
        knob (SY, S (FCtl + 2, "warm drive"), 600, 350, 60, 52);
        knob (SY, S (FCtl + 3, "keytrk"), 662, 350, 50, 52);
        setGroup (fg, 2, from);
    }

    knob (SY, S (MainVol, "volume"), 904, 350, 54, 52);
    toggle (SY, S (Legato, "Legato"), 962, 358, 50, "legato");
    toggle (SY, S (Mono, "Mono"), 1014, 358, 48, "mono");
    knob (SY, S (Porta, "time"), 494, 646, 60, 56);
    knob (SY, S (BendRange, "bend"), 610, 646, 56, 56);
    combo (SY, S (PortaMode, "Porta Mode"), 680, 656, 132, "glide mode");
    knob (SY, S (Polyphony, "poly"), 868, 646, 64, 56);
}

void VCClonePanel::buildSpire()
{
    using AP = std::unique_ptr<ParamAdapter>;
    const int M = SPm;
    auto P = [this] (int i, const char* l, SpK k, StringArray list = {}) -> AP { return std::make_unique<SpireAdapter> (processor, i, l, k, list); };
    auto cut = [] (AP a, const char* l) -> AP { return std::make_unique<CutoffAdapter> (std::move (a), l); };
    const auto waves = spWaves();
    const auto targets = spTargets();

    card (M, { 20, 78, 432, 340 }, "OSCILLATORS");
    const int og = addTabs (M, { 32, 112, 408, 28 }, { "OSC 1", "OSC 2", "OSC 3", "OSC 4" }, { gid::oscOn[0], gid::oscOn[1], gid::oscOn[2], gid::oscOn[3] }, false);
    const int mixIdx[4] = { sp::mix_osc1, sp::mix_osc2, sp::mix_osc3, sp::mix_osc4 };
    for (int k = 0; k < 4; ++k)
    {
        const size_t from = items.size();
        const int b = sp::osc1_type + k * 19;
        auto* t = makeControl (P (b, "Mode", SpK::List, kSpOscTypes));
        place (M, t->comp.get(), { 32, 236, 96, 24 }, "mode").control = t;
        auto sc = std::make_unique<WaveScope>();
        auto* ta = t->adapter.get();
        sc->fn = [ta] (float p) { return waveByName (ta->text (ta->get()), p); };
        addChildComponent (*sc);
        place (M, sc.get(), { 32, 148, 408, 70 });
        scopes.push_back (std::move (sc));
        knob (M, P (b + 4, "ctrl A", SpK::Knob1000), 132, 226, 52, 52);
        knob (M, P (b + 5, "ctrl B", SpK::Knob1000), 184, 226, 52, 52);
        knob (M, P (b + 1, "octave", SpK::Octave), 236, 226, 52, 52);
        knob (M, P (b + 2, "note", SpK::Semitone), 288, 226, 52, 52);
        knob (M, P (b + 3, "fine", SpK::Signed), 340, 226, 52, 52);
        knob (M, P (b + 8, "wt mix", SpK::Knob1000), 392, 226, 52, 52);
        knob (M, P (mixIdx[k], "level", SpK::Knob1000), 28, 286, 52, 52);
        knob (M, P (b + 14, "pan", SpK::Signed), 80, 286, 52, 52);
        knob (M, P (b + 18, "to flt", SpK::Knob1000), 132, 286, 52, 52);
        combo (M, P (b + 7, "Wave", SpK::List, waves), 190, 294, 120, "wavetable");
        separator (M, 32, 350, 408);
        combo (M, P (b + 11, "Unison", SpK::List, kSpUnison), 32, 368, 64, "unison");
        knob (M, P (b + 9, "detune", SpK::Knob1000), 100, 358, 52, 52);
        knob (M, P (b + 13, "density", SpK::Knob1000), 152, 358, 52, 52);
        knob (M, P (b + 10, "wide", SpK::Knob1000), 204, 358, 52, 52);
        combo (M, P (b + 12, "Chord", SpK::List, kSpChords), 262, 368, 100, "unison mode");
        setGroup (og, k, from);
    }

    // LFOS: four rows
    curLayer = 0;
    card (M, { 462, 78, 334, 226 }, "LFOS");
    for (int l = 0; l < 4; ++l)
    {
        const int b = sp::lfo1_rate + l * 16, y = 108 + l * 48;
        rowLabel (M, 472, y + 6, 26, String (l + 1), {});
        combo (M, P (b + 7, "Shape", SpK::List, spLfoForms()), 498, y + 4, 58, "shape", 22);
        knob (M, P (b, "rate", SpK::Knob1000), 558, y, 44, 44);
        knob (M, P (b + 11, "amount", SpK::Signed), 602, y, 48, 44);
        toggle (M, P (b + 3, "Sync", SpK::Flag), 654, y + 4, 42, "sync", "SYNC");
        combo (M, P (b + 10, "Send To", SpK::List, targets), 702, y + 4, 84, "send to", 22);
        if (l < 3) separator (M, 472, (float) y + 46, 314);
    }

    // ENVELOPES rows 3-4: ENV 1-2 or ENV 3-4 (tabs in the header)
    const int eg = addTabs (M, { 924, 84, 76, 20 }, { "1-2", "3-4" }, {}, false);
    for (int e = 0; e < 4; ++e)
    {
        const size_t from = items.size();
        const int b = sp::env1_att + e * 16, y = 206 + (e % 2) * 48;
        rowLabel (M, 816, y + 8, 52, "ENV " + String (e + 1), e == 0 ? "filter" : "free");
        const int off[4] = { 0, 1, 2, 5 };
        for (int i = 0; i < 4; ++i) knob (M, P (b + off[i], i == 0 ? "a" : i == 1 ? "d" : i == 2 ? "s" : "r", SpK::Knob1000), 870 + i * 38, y, 38, 46);
        knob (M, P (b + 11, "amount", SpK::Signed), 1022, y, 50, 46);
        combo (M, P (b + 10, "Send To", SpK::List, targets), 1074, y + 6, 96, "send to");
        setGroup (eg, e / 2, from);
    }
    curLayer = -1;

    // FILTER: F1, F2, routing
    card (M, { 462, 314, 426, 104 }, "FILTER");
    const int fg = addTabs (M, { 664, 320, 216, 20 }, { "F1", "F2", "ROUTING" }, {}, false);
    for (int k = 0; k < 2; ++k)
    {
        const size_t from = items.size();
        combo (M, P (k == 0 ? sp::flt1_type : sp::flt2_type, "Type", SpK::List, kSpFltTypes), 474, 358, 104, "type");
        knob (M, cut (P (k == 0 ? sp::flt1_cut : sp::flt2_cut, "cutoff", SpK::Knob1000), "cutoff"), 584, 346, 62, 64);
        knob (M, P (k == 0 ? sp::flt1_res : sp::flt2_res, "reso", SpK::Knob1000), 650, 350, 50, 52);
        combo (M, P (k == 0 ? sp::flt1_mode : sp::flt2_mode, "Mode", SpK::List, kSpFltModes), 706, 358, 84, "mode");
        setGroup (fg, k, from);
    }
    {
        const size_t from = items.size();
        toggle (M, P (sp::flt_rout, "Parallel", SpK::Flag), 480, 358, 66, "parallel");
        toggle (M, P (sp::flt_link, "Link", SpK::Flag), 554, 358, 50, "link");
        knob (M, P (sp::flt_balance, "balance", SpK::Knob1000), 612, 350, 56, 52);
        knob (M, P (sp::flt_keytrack, "keytrk", SpK::Signed), 670, 350, 56, 52);
        setGroup (fg, 2, from);
    }

    knob (M, P (sp::volume, "volume", SpK::Knob1000), 904, 350, 54, 52);
    knob (M, P (sp::velocity, "velocity", SpK::Knob1000), 958, 350, 54, 52);
    knob (M, P (sp::drift, "drift", SpK::Knob1000), 1012, 350, 54, 52);
    knob (M, P (sp::glide, "time", SpK::Knob1000), 494, 646, 60, 56);
    knob (M, P (sp::bend_up, "bend up", SpK::List, kSpBend), 610, 646, 56, 56);
    knob (M, P (sp::bend_dn, "bend dn", SpK::List, kSpBend), 668, 646, 56, 56);
    combo (M, P (sp::mode, "Voice Mode", SpK::List, kSpVoice), 846, 656, 62, "mode");
    knob (M, P (sp::poly, "poly", SpK::List, kSpPoly), 910, 646, 48, 56);
}

//==============================================================================
// The modulation matrix (Virus and Spire): one slot at a time, over the LFO and envelope cards
void VCClonePanel::buildMatrix()
{
    curLayer = 1;
    const Rectangle<float> area { 462, 78, 718, 226 };
    // Virus: the six assign slots
    {
        card (V, area, "MODULATION MATRIX");
        const char* regions[] = { "assignA", "assignB", "assignC", "assignD", "assignE", "assignF" };
        const int g = addTabs (V, { 476, 116, 360, 26 }, { "1", "2", "3", "4", "5", "6" }, {}, false);
        for (int s = 0; s < 6; ++s)
        {
            const size_t from = items.size();
            int x = 476;
            const int y = 166;
            for (int o : regionOffsets (regions[s]))
            {
                const auto a = virus (o);
                if (! a->items().isEmpty()) { combo (V, virus (o), x, y + 8, 150, shortVirusLabel (o).toLowerCase()); x += 158; }
                else { knob (V, virus (o, shortVirusLabel (o).toLowerCase()), x, y, 64, 60); x += 70; }
            }
            setGroup (g, s, from);
        }
    }
    // Spire: fifteen slots, two sources and two targets each
    {
        card (SPm, area, "MODULATION MATRIX");
        StringArray names;
        for (int s = 1; s <= 15; ++s) names.add (String (s));
        const int g = addTabs (SPm, { 476, 116, 690, 26 }, names, {}, false);
        using AP = std::unique_ptr<ParamAdapter>;
        auto P = [this] (int i, const char* l, SpK k, StringArray list = {}) -> AP { return std::make_unique<SpireAdapter> (processor, i, l, k, list); };
        const auto targets = spTargets(), sources = spSources();
        for (int s = 0; s < 15; ++s)
        {
            const size_t from = items.size();
            const int b = sp::slot1_Src1 + s * 10, y = 166;
            combo (SPm, P (b, "Source 1", SpK::List, sources), 476, y + 8, 118, "source 1");
            combo (SPm, P (b + 1, "Source 2", SpK::List, sources), 600, y + 8, 118, "source 2");
            combo (SPm, P (b + 3, "Target 1", SpK::List, targets), 730, y + 8, 130, "target 1");
            knob (SPm, P (b + 2, "amount 1", SpK::Signed), 862, y, 64, 60);
            combo (SPm, P (b + 5, "Target 2", SpK::List, targets), 934, y + 8, 130, "target 2");
            knob (SPm, P (b + 4, "amount 2", SpK::Signed), 1066, y, 64, 60);
            setGroup (g, s, from);
        }
    }
    curLayer = -1;
}

//==============================================================================
VCClonePanel::VCClonePanel (VCCloneAudioProcessor& p) : processor (p), browser (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    setSize (kWidth, kHeight);
    createBackground();

    // top bar
    modeButton.setTooltip ("Synth mode");
    modeButton.onClick = [this] { openModeMenu(); };
    nameButton.setTooltip ("Presets in the loaded bank");
    nameButton.onClick = [this] { openPresetMenu(); };
    initButton.setTooltip ("Init patch for the current synth mode");
    initButton.onClick = [this] { processor.loadInitPatch(); };
    prevButton.setTooltip ("Previous preset");
    prevButton.onClick = [this] { processor.setCurrentProgram (processor.getCurrentProgram() - 1); };
    nextButton.setTooltip ("Next preset");
    nextButton.onClick = [this] { processor.setCurrentProgram (processor.getCurrentProgram() + 1); };
    browserButton.setTooltip ("Banks for Virus, Sylenth and Spire (click again to go back)");
    browserButton.onClick = [this] { showBrowser (! browserShown); };
    loadButton.setTooltip ("Import a Virus (.mid/.syx), Sylenth (.fxb/.fxp) or Spire (.spf2) preset or bank; it opens in its synth mode. "
                           "You can also drag files onto the window");
    loadButton.onClick = [this] { openLoadDialog(); };
    exportButton.setTooltip ("Save the current preset or bank in the format of the synth mode that is loaded");
    exportButton.onClick = [this] { openExportMenu(); };
    for (auto* b : { &modeButton, &nameButton, &initButton, &prevButton, &nextButton, &browserButton, &loadButton, &exportButton })
        addAndMakeVisible (b);
    modeButton.setColour (TextButton::buttonColourId, VirusColours::blue);
    modeButton.setColour (TextButton::textColourOffId, Colour (0xff06121e));
    nameButton.setAlpha (0.0f);            // clickable; the name is drawn by paint()
    modeButton.setToggleState (true, dontSendNotification);
    voiceMeter.active = [this] { return processor.activeVoices.load(); };
    voiceMeter.maximum = [] { return 16; };
    addAndMakeVisible (voiceMeter);
    outputKnob = makeControl (host (gid::volume, "output"), true);
    outputKnob->comp->setVisible (true);

    buildShared();
    buildVirus();
    buildSylenth();
    buildSpire();
    buildMatrix();

    matrixButton.setTooltip ("Modulation matrix (Virus and Spire)");
    matrixButton.onClick = [this] { matrixShown = ! matrixShown; updateVisibility(); repaint(); };
    addChildComponent (matrixButton);

    addChildComponent (browser);

    resized();
    updateVisibility();
    timerCallback();
    startTimerHz (30);
}

VCClonePanel::~VCClonePanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int VCClonePanel::currentMode() const { return (int) processor.getMode(); }

bool VCClonePanel::itemVisible (const Item& it) const
{
    if (browserShown) return false;
    if (it.mode >= 0 && it.mode != currentMode()) return false;
    const int layer = matrixShown && currentMode() != SY ? 1 : 0;
    if (it.layer >= 0 && it.layer != layer) return false;
    if (it.group >= 0 && tabGroups[(size_t) it.group]->selected != it.index) return false;
    return true;
}

void VCClonePanel::updateVisibility()
{
    for (auto& it : items)
        if (it.comp) it.comp->setVisible (itemVisible (it));
    for (auto& g : tabGroups)
        for (int i = 0; i < (int) g->buttons.size(); ++i) g->buttons[(size_t) i]->setToggleState (i == g->selected, dontSendNotification);
    const bool mat = currentMode() != SY && ! browserShown;
    matrixButton.setVisible (mat);
    matrixButton.setButtonText (matrixShown ? String::fromUTF8 ("\xe2\x97\x82 LFO / ENV") : String::fromUTF8 ("MATRIX \xe2\x96\xb8"));
    matrixButton.setToggleState (! matrixShown, dontSendNotification);
    if (matrixShown) matrixButton.setBounds (1066, 84, 104, 20);
    else matrixButton.setBounds (700, 84, 86, 20);
    browser.setVisible (browserShown);
    browserButton.setToggleState (browserShown, dontSendNotification);
}

void VCClonePanel::showBrowser (bool show)
{
    browserShown = show;
    if (show) browser.refresh();
    updateVisibility();
    repaint();
}

void VCClonePanel::resized()
{
    modeButton.setBounds (186, 21, 62, 20);
    nameButton.setBounds (254, 15, 244, 32);
    initButton.setBounds (504, 21, 40, 20);
    prevButton.setBounds (552, 21, 22, 20);
    nextButton.setBounds (576, 21, 22, 20);
    voiceMeter.setBounds (614, 28, 104, 18);
    if (outputKnob) outputKnob->comp->setBounds (858, 4, 54, 56);
    browserButton.setBounds (989, 20, 64, 24);
    loadButton.setBounds (1063, 20, 54, 24);
    exportButton.setBounds (1125, 20, 54, 24);
    browser.setBounds (0, 64, kWidth, kHeight - 64);
}

//==============================================================================
void VCClonePanel::createBackground()
{
    background = Image (Image::RGB, kWidth * 2, kHeight * 2, true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (2.0f));
    g.setGradientFill (ColourGradient (Colour (0xff30353b), kWidth * 0.3f, 0.0f, Colour (0xff1a1c1f), kWidth * 0.3f, (float) kHeight * 0.9f, true));
    g.fillAll();
    // the top bar
    g.setGradientFill (ColourGradient (Colour (0xff34383d), 0.0f, 0.0f, Colour (0xff23262a), 0.0f, 64.0f, false));
    g.fillRect (0, 0, kWidth, 64);
    g.setColour (Colour (0xff0c0d0f));
    g.drawHorizontalLine (63, 0.0f, (float) kWidth);
    // the patch display, voices box and the parameter display
    g.setColour (Colour (0xff15181b));
    g.fillRoundedRectangle (174.0f, 10.0f, 432.0f, 42.0f, 8.0f);
    g.fillRoundedRectangle (606.0f + 6.0f, 10.0f, 112.0f, 42.0f, 8.0f);
    g.setColour (Colour (0xff0b0d0f));
    g.fillRoundedRectangle (730.0f, 10.0f, 120.0f, 42.0f, 8.0f);
}

void VCClonePanel::drawWordmark (Graphics& g, Rectangle<float> area)
{
    const float baseline = area.getBottom() - 8.0f;
    GlyphArrangement main;
    main.addLineOfText (Font (FontOptions (area.getHeight() * 0.62f, Font::bold | Font::italic)), "vc", area.getX(), baseline);
    Path mainPath; main.createPath (mainPath);
    const auto mb = mainPath.getBounds();
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillPath (mainPath, AffineTransform::translation (1.0f, 1.5f));
    ColourGradient chrome (Colour (0xffffffff), 0.0f, mb.getY(), Colour (0xff8c949c), 0.0f, mb.getBottom(), false);
    chrome.addColour (0.5, Colour (0xffd5dade)); chrome.addColour (0.55, Colour (0xffa4acb3));
    g.setGradientFill (chrome);
    g.fillPath (mainPath);
    GlyphArrangement second;
    second.addLineOfText (Font (FontOptions (area.getHeight() * 0.55f, Font::italic)), "clone", mb.getRight() + 2.0f, baseline);
    Path sp; second.createPath (sp);
    g.setColour (Colours::black.withAlpha (0.5f)); g.fillPath (sp, AffineTransform::translation (1.0f, 1.0f));
    g.setColour (VirusColours::blue); g.fillPath (sp);
}

void VCClonePanel::paint (Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());
    drawWordmark (g, { 20.0f, 6.0f, 150.0f, 52.0f });

    // patch name
    g.setFont (CustomLookAndFeel::uiFont (17.0f, true));
    g.setColour (Colour (0xffe8edf2));
    g.drawFittedText (processor.getPatchName().isEmpty() ? String ("Init") : processor.getPatchName(), nameButton.getBounds().withTrimmedLeft (4),
                      Justification::centredLeft, 1);
    // voices / CPU and the parameter display
    g.setFont (CustomLookAndFeel::uiFont (10.5f, true));
    g.setColour (Colour (0xffe8edf2));
    g.drawText ("VOICES " + String (processor.activeVoices.load()) + "/16", 622, 12, 96, 16, Justification::centredLeft);
    {
        const auto area = Rectangle<int> (736, 12, 108, 38);
        g.setColour (Colour (0xff7cc4ff));
        g.setFont (CustomLookAndFeel::uiFont (9.5f, true));
        const double now = Time::getMillisecondCounterHiRes();
        if (statusMessage.isNotEmpty() && now < statusUntil) g.drawFittedText (statusMessage, area, Justification::centred, 2);
        else if (lcdName.isNotEmpty())
        {
            g.drawFittedText (lcdName, area.withHeight (18), Justification::centred, 1);
            g.setColour (Colours::white);
            g.setFont (CustomLookAndFeel::uiFont (12.0f, true));
            g.drawFittedText (lcdValue, area.withTrimmedTop (18), Justification::centred, 1);
        }
        else
        {
            const auto& bank = processor.getBank();
            g.drawFittedText (bank.empty() ? String ("INIT") : String (processor.getCurrentProgram() + 1) + " / " + String ((int) bank.size()),
                              area, Justification::centred, 2);
        }
    }
    if (browserShown) return;

    const int mode = currentMode();
    const int layer = matrixShown && mode != SY ? 1 : 0;
    for (auto& c : cards)
    {
        if ((c.mode >= 0 && c.mode != mode) || (c.layer >= 0 && c.layer != layer)) continue;
        const auto cb = c.r;
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (cb.translated (0.0f, 2.0f), 8.0f);
        g.setGradientFill (ColourGradient (Colour (0xff2c3035), 0.0f, cb.getY(), Colour (0xff23262a), 0.0f, cb.getBottom(), false));
        g.fillRoundedRectangle (cb, 8.0f);
        g.setColour (Colour (0xff0c0d0f));
        g.drawRoundedRectangle (cb.reduced (0.5f), 8.0f, 1.0f);
        // the title tab, top left
        const auto font = CustomLookAndFeel::uiFont (13.0f, true);
        const float pw = c.power ? 22.0f : 0.0f;
        const float tw = font.getStringWidthFloat (c.title) + 24.0f + pw;
        Path tab;
        tab.addRoundedRectangle (cb.getX(), cb.getY(), tw, 30.0f, 8.0f, 8.0f, true, false, false, true);
        g.setColour (Colour (0xff1a1d20));
        g.fillPath (tab);
        g.setFont (font);
        g.setColour (Colour (0xffe8edf2));
        g.drawText (c.title, Rectangle<float> (cb.getX() + 10.0f + pw, cb.getY(), tw, 30.0f), Justification::centredLeft);
    }
    g.setColour (Colour (0xff1a1d20));
    for (auto& s : separators)
        if ((s.mode < 0 || s.mode == mode) && (s.layer < 0 || s.layer == layer)) g.fillRect (s.r);

    for (auto& it : items)
    {
        if (! itemVisible (it)) continue;
        const auto r = it.r.toFloat();
        if (it.style == 1)
        {
            g.setColour (Colour (0xff15181b));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Colour (0xff2a3a4a));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (Colour (0xff9fd2ff));
            g.setFont (CustomLookAndFeel::uiFont (11.0f, true));
            g.drawFittedText (it.fixedText, it.r.reduced (8, 0), Justification::centredLeft, 1);
        }
        else if (it.style == 3)
        {
            g.setColour (kCaption);
            g.setFont (CustomLookAndFeel::uiFont (10.0f, true));
            g.drawText (it.fixedText, r, Justification::centredLeft);
        }
        else if (it.style == 2)
        {
            g.setColour (Colour (0xff7cc4ff));
            g.setFont (CustomLookAndFeel::uiFont (12.5f, true));
            if (it.caption.isEmpty())
            {
                g.setColour (VirusColours::blue);
                g.fillEllipse (r.getX(), r.getCentreY() - 4.0f, 8.0f, 8.0f);
                g.setColour (Colour (0xff7cc4ff));
                g.drawText (it.fixedText, r.withTrimmedLeft (11.0f), Justification::centredLeft);
                continue;
            }
            g.drawText (it.fixedText, r.withHeight (16.0f), Justification::centredLeft);
            g.setColour (kDim);
            g.setFont (CustomLookAndFeel::uiFont (9.5f, true));
            g.drawText (it.caption, r.withTrimmedTop (16.0f), Justification::centredLeft);
            continue;
        }
        if (it.caption.isNotEmpty())
        {
            g.setColour (kCaption);
            g.setFont (CustomLookAndFeel::uiFont (10.0f, true));
            g.drawText (it.caption, Rectangle<float> (r.getX() - 20.0f, r.getBottom() + 3.0f, r.getWidth() + 40.0f, 13.0f), Justification::centred);
        }
    }
    g.setFont (Font (FontOptions (28.0f, Font::bold | Font::italic)));
    g.setColour (Colour (0xff3a3f45));
    g.drawText ("vcclone", 990, 640, 190, 50, Justification::centredLeft);
}

//==============================================================================
void VCClonePanel::timerCallback()
{
    for (auto& cp : controls)
    {
        auto* c = cp.get();
        if (! c->comp->isVisible()) continue;
        const int v = c->adapter->get();
        if (v == c->shown) continue;
        c->shown = v;
        switch (c->kind)
        {
            case Control::Kind::Knob: static_cast<VirusKnob*> (c->comp.get())->setValue (v, dontSendNotification); c->comp->repaint(); break;
            case Control::Kind::Combo: static_cast<ComboBox*> (c->comp.get())->setSelectedId (v - c->adapter->minimum() + 1, dontSendNotification); break;
            case Control::Kind::Toggle:
            {
                auto* b = static_cast<Button*> (c->comp.get());
                b->setToggleState (v != 0, dontSendNotification);
                const auto on = b->getProperties()["onText"].toString();
                if (on.isNotEmpty()) b->setButtonText (on == "ON" ? String (v != 0 ? "ON" : "OFF") : on);
                break;
            }
        }
    }
    for (auto& s : scopes) if (s->isVisible()) s->repaint();

    const int mode = currentMode();
    if (mode != lastMode)
    {
        lastMode = mode;
        modeButton.setButtonText (String (kModeNames[mode]) + String::fromUTF8 (" \xe2\x96\xbe"));
        for (auto& c : controls) c->shown = INT_MIN;
        updateVisibility();
        repaint();
    }
    nameButton.setButtonText (processor.getPatchName().isEmpty() ? String ("Init") : processor.getPatchName());
    voiceMeter.repaint();
    repaint (174, 8, 680, 48);

    if (lastBankVersion != processor.bankVersion.load())
    {
        lastBankVersion = processor.bankVersion.load();
        const bool many = processor.getBank().size() > 1;
        prevButton.setEnabled (many);
        nextButton.setEnabled (many);
        lcdName.clear();
        for (auto& c : controls) c->shown = INT_MIN;
        browser.refresh();
        repaint();
    }
}

void VCClonePanel::openModeMenu()
{
    PopupMenu m;
    for (int i = 0; i < 3; ++i) m.addItem (i + 1, kModeNames[i], true, i == currentMode());
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (modeButton), [this] (int r)
    {
        if (r > 0) processor.setMode ((vc::SynthMode) (r - 1));
    });
}

void VCClonePanel::openPresetMenu()
{
    const auto& bank = processor.getBank();
    if (bank.empty()) { showBrowser (true); return; }
    PopupMenu m;
    for (int i = 0; i < (int) bank.size(); ++i)
        m.addItem (i + 1, String (i + 1).paddedLeft ('0', 3) + "  " + String (bank[(size_t) i].name), true, i == processor.getCurrentProgram());
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (nameButton).withMaximumNumColumns (4), [this] (int r)
    {
        if (r > 0) processor.setCurrentProgram (r - 1);
    });
}

bool VCClonePanel::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        for (auto ext : { ".mid", ".midi", ".syx", ".fxb", ".fxp", ".spf2", ".repatch" })
            if (f.endsWithIgnoreCase (ext)) return true;
    return false;
}

void VCClonePanel::filesDropped (const StringArray& files, int, int)
{
    for (auto& f : files) if (isInterestedInFileDrag ({ f })) { loadFile (File (f)); break; }
}

void VCClonePanel::loadFile (const File& f)
{
    const auto err = processor.loadBankFile (f);
    if (err.isNotEmpty()) { AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Could not load " + f.getFileName(), err); return; }
    browser.addBank (f);
    statusMessage = "LOADED " + String ((int) processor.getBank().size()) + " PATCHES";
    statusUntil = Time::getMillisecondCounterHiRes() + 2500;
}

void VCClonePanel::openLoadDialog()
{
    chooser = std::make_unique<FileChooser> ("Load Virus / Sylenth / Spire presets", File(), "*.mid;*.midi;*.syx;*.fxb;*.fxp;*.spf2;*.repatch");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
}

void VCClonePanel::openExportMenu()
{
    // offers only the formats of the synth mode that is loaded
    PopupMenu m;
    struct Entry { String label, ext; bool bank; };
    std::vector<Entry> entries;
    switch (processor.getMode())
    {
        case vc::SynthMode::Sylenth:
            entries = { { "Save Sylenth preset (.fxp)", ".fxp", false }, { "Save Sylenth soundbank (.fxb)", ".fxb", true } };
            break;
        case vc::SynthMode::Spire:
            entries = { { "Save Spire preset (.spf2)", ".spf2", false } };
            break;
        default:
            entries = { { "Save Virus preset (.syx)", ".syx", false }, { "Save Virus preset (.mid)", ".mid", false },
                        { "Save Virus soundbank (.syx)", ".syx", true }, { "Save Virus soundbank (.mid)", ".mid", true } };
            break;
    }
    for (size_t i = 0; i < entries.size(); ++i) m.addItem ((int) i + 1, entries[i].label);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (exportButton), [this, entries] (int r)
    {
        if (r <= 0) return;
        const auto it = entries[(size_t) r - 1];
        const String base = File::createLegalFileName (processor.getPatchName().isEmpty() ? String ("VC-Clone") : processor.getPatchName());
        chooser = std::make_unique<FileChooser> ("Export", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (base + it.ext), "*" + it.ext);
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this, it] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == File()) return;
            f = f.withFileExtension (it.ext);
            const bool ok = it.bank ? processor.exportBank (f) : processor.exportCurrent (f);
            statusMessage = ok ? "SAVED " + f.getFileName().toUpperCase() : String ("EXPORT FAILED");
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
    if (sizeRestored) processor.editorWidth.store (getWidth());
}
