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
    if (led)
    {
        const auto d = b.withSizeKeepingCentre (8.0f, 8.0f);
        if (on) { g.setColour (VirusColours::blue.withAlpha (0.35f)); g.fillEllipse (d.expanded (3.0f)); }
        g.setColour (on ? VirusColours::blue : Colour (0xff3a3f45));
        g.fillEllipse (d);
        return;
    }
    if (small && isOn)
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
    if (icon) { icon (g, b, sel); return; }
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


namespace
{
    /** An envelope time knob: the value reads as a note value while its Sync switch is on. */
    class SyncTimeAdapter : public ApvtsAdapter
    {
    public:
        SyncTimeAdapter (RangedAudioParameter& p, RangedAudioParameter& sync, String label)
            : ApvtsAdapter (p, std::move (label)), syncParam (sync) {}
        String text (int v) const override
        {
            if (syncParam.getValue() > 0.5f)
                return vc::SyncDivisions::name (roundToInt ((float) v / 127.0f * (float) (vc::SyncDivisions::kCount - 1)));
            return ApvtsAdapter::text (v);
        }
    private:
        RangedAudioParameter& syncParam;
    };

    /** A list parameter stepped by a knob, showing the entry's name (delay clock, Spire unison...). */
    String capitalised (String s) { return s.isEmpty() ? s : s.substring (0, 1).toUpperCase() + s.substring (1); }
}

//==============================================================================
std::unique_ptr<ParamAdapter> VCClonePanel::envTime (const char* id, const char* syncId, String label)
{
    return std::make_unique<SyncTimeAdapter> (*processor.apvts.getParameter (id), *processor.apvts.getParameter (syncId), std::move (label));
}

ParamAdapter& VCClonePanel::keep (AP a)
{
    keptAdapters.push_back (std::move (a));
    return *keptAdapters.back();
}

VCClonePanel::Control* VCClonePanel::makeControl (AP a, bool forceKnob)
{
    if (! a) return nullptr;
    auto c = std::make_unique<Control>();
    auto* adapter = a.get();
    c->adapter = std::move (a);
    const auto list = adapter->items();
    const String fullName = adapter->name();
    auto touched = [this, adapter] { lcdName = adapter->name().toUpperCase(); lcdValue = adapter->text (adapter->get()); lcdUntil = Time::getMillisecondCounterHiRes() + 1500; };

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
        auto k = std::make_unique<OttKnob> (*adapter, capitalised (fullName));
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
    it.captionY = r.getBottom() + 3;
    if (comp) comp->setBounds (r);
    items.push_back (it);
    return items.back();
}

VCClonePanel::Cell VCClonePanel::K (AP a, int knob, int w) { Cell c; c.control = makeControl (std::move (a), true); c.w = w; c.knob = knob; return c; }
VCClonePanel::Cell VCClonePanel::D (AP a, int w, String cap)
{
    Cell c; c.control = makeControl (std::move (a)); c.w = w; c.caption = std::move (cap);
    c.knob = c.control != nullptr && c.control->kind == Control::Kind::Knob ? 26 : 0;
    return c;
}
VCClonePanel::Cell VCClonePanel::T (AP a, int w, String cap, String onText)
{
    Cell c;
    c.control = makeControl (std::move (a));
    if (c.control) c.control->comp->getProperties().set ("onText", onText);
    c.w = w; c.knob = 0; c.caption = std::move (cap);
    return c;
}

void VCClonePanel::row (int mode, int x, int y, int boxH, int gap, std::vector<Cell> cells)
{
    // every cell shares the box height: knobs are centred in it, dropdowns / switches too, and every
    // name sits on the same line under the box (knobs draw their own name and value)
    for (auto& c : cells)
    {
        Component* comp = c.comp != nullptr ? c.comp : (c.control != nullptr ? c.control->comp.get() : nullptr);
        if (comp != nullptr)
        {
            if (auto* k = dynamic_cast<OttKnob*> (comp))
            {
                k->setGeometry (c.knob, boxH);
                place (mode, comp, { x, y, c.w, boxH + 3 + 13 + 12 });
            }
            else
            {
                auto caption = c.caption;
                if (caption.isEmpty() && c.control != nullptr) caption = c.control->adapter->name();   // a switch in a knob's place
                const int h = c.control != nullptr && c.control->kind == Control::Kind::Toggle && c.caption.isEmpty() ? 22 : 26;
                auto& it = place (mode, comp, { x + (c.caption.isEmpty() && c.w > 64 ? (c.w - 64) / 2 : 0), y + (boxH - h) / 2,
                                                c.caption.isEmpty() && c.w > 64 ? 64 : c.w, h }, capitalised (caption));
                it.captionY = y + boxH + 3;
            }
        }
        x += c.w + gap;
    }
}

VCClonePanel::Control* VCClonePanel::headerCombo (int mode, AP a, Rectangle<int> r)
{
    auto* c = makeControl (std::move (a));
    if (c) place (mode, c->comp.get(), r);
    return c;
}

VCClonePanel::Control* VCClonePanel::smallToggle (int mode, AP a, Rectangle<int> r, String text)
{
    auto* c = makeControl (std::move (a));
    if (! c) return nullptr;
    c->comp->getProperties().set ("onText", text);
    c->comp->getProperties().set ("small", true);
    place (mode, c->comp.get(), r);
    return c;
}

VCClonePanel::Item& VCClonePanel::fixedBox (int mode, Rectangle<int> r, String text, String caption)
{
    auto& it = place (mode, nullptr, r, std::move (caption));
    it.fixedText = String::fromUTF8 ("\xe2\x86\x92 ") + text;
    it.style = 1;
    return it;
}

VCClonePanel::Item& VCClonePanel::heading (int mode, int x, int y, String text)
{
    auto& it = place (mode, nullptr, { x, y, 200, 12 });
    it.fixedText = std::move (text);
    it.style = 3;
    return it;
}

int VCClonePanel::addTabs (int mode, Rectangle<int> area, StringArray names, std::vector<const char*> powerIds, bool small)
{
    auto g = std::make_unique<TabGroup>();
    g->mode = mode;
    const int gi = (int) tabGroups.size();
    const int n = names.size(), gap = small ? 2 : 6;
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
            bp->setTooltip ("Click the dot to turn this on or off");
        }
        bp->onClick = [this, gi, i, bp]
        {
            if (bp->togglePower && bp->downX < 26) { bp->togglePower(); bp->repaint(); return; }
            tabGroups[(size_t) gi]->selected = i;
            updateVisibility();
            repaint();
        };
        addChildComponent (*bp);
        place (mode, bp, { area.getX() + i * (w + gap), area.getY(), w, area.getHeight() });
        g->buttons.push_back (std::move (b));
    }
    tabGroups.push_back (std::move (g));
    return gi;
}

void VCClonePanel::setGroup (size_t from, int group, int index)
{
    for (size_t i = from; i < items.size(); ++i) { items[i].group = group; items[i].index = index; }
}

void VCClonePanel::setGroup2 (size_t from, int group, int index)
{
    for (size_t i = from; i < items.size(); ++i) { items[i].group2 = group; items[i].index2 = index; }
}

VCClonePanel::Card& VCClonePanel::card (int mode, Rectangle<float> r, String title, const char* powerId)
{
    Card c;
    c.r = r; c.title = std::move (title); c.mode = mode; c.layer = curLayer;
    if (powerId != nullptr)
    {
        c.power = true;
        auto adapter = host (powerId, title);
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
        place (mode, controls.back()->comp.get(), { (int) r.getX() + 8, (int) r.getY() + 6, 18, 18 });
    }
    cards.push_back (c);
    return cards.back();
}

WaveScope* VCClonePanel::addScope (int mode, Rectangle<int> r, std::function<float (float)> fn, std::function<int()> layers)
{
    auto w = std::make_unique<WaveScope>();
    w->fn = std::move (fn);
    w->layers = std::move (layers);
    addChildComponent (*w);
    place (mode, w.get(), r);
    scopes.push_back (std::move (w));
    return scopes.back().get();
}

EnvelopeGraph* VCClonePanel::addEnvGraph (int mode, Rectangle<int> r, ParamAdapter& a, ParamAdapter& d, ParamAdapter& s, ParamAdapter& rel)
{
    auto g = std::make_unique<EnvelopeGraph> (a, d, s, rel);
    g->onTouched = [this] (ParamAdapter& p) { lcdName = p.name().toUpperCase(); lcdValue = p.text (p.get()); };
    addChildComponent (*g);
    place (mode, g.get(), r);
    envGraphs.push_back (std::move (g));
    return envGraphs.back().get();
}

//==============================================================================
// Card positions (base size 1200 x 732)
namespace Lay
{
    const Rectangle<float> osc { 20, 78, 300, 320 }, levels { 330, 78, 340, 92 }, unison { 330, 180, 160, 218 },
                           mix { 500, 180, 170, 218 }, filter { 680, 78, 250, 320 }, fx { 940, 78, 240, 320 },
                           play { 20, 408, 90, 304 }, modA { 120, 408, 400, 304 }, modB { 530, 408, 400, 304 },
                           output { 940, 408, 240, 304 };
}

//==============================================================================
// Parts shared by every mode: effects, sequencer / arp, output with EQ, modulation AMP / FILTER pages
void VCClonePanel::buildShared()
{
    auto H = [this] (const char* id, const char* l) -> AP { return host (id, l); };

    // ---- EFFECTS: DIST / DELAY / REVERB ----------------------------------------------------------
    card (-1, Lay::fx, "EFFECTS");
    const int fg = addTabs (-1, { 952, 116, 216, 24 }, { "DIST", "DELAY", "REVERB" }, { gid::distOn, gid::delayOn, gid::reverbOn });
    tabGroups[(size_t) fg]->selected = 1;
    auto& dl = processor.delayProcessor();
    auto& rv = processor.reverbProcessor();
    {
        size_t from = items.size();
        row (-1, 986, 154, 26, 0, { D (H (gid::satCurve, "Curve"), 148, "Curve") });
        row (-1, 984, 210, 40, 12, { K (H (gid::satDrive, "Drive"), 44, 76), K (H (gid::satMix, "Mix"), 40, 76) });
        setGroup (from, fg, 0);
        from = items.size();
        row (-1, 986, 154, 26, 0, { D (fx (dl, "mode", "Delay Type"), 148, "Type") });
        row (-1, 946, 210, 40, 0, { K (fx (dl, "send", "Mix"), 40, 76), K (fx (dl, "feedback", "Fbk"), 40, 76), K (fx (dl, "clock", "Time"), 40, 76) });
        row (-1, 946, 294, 40, 0, { K (fx (dl, "color", "Color"), 40, 76), K (fx (dl, "drySource", "Dry"), 40, 76), K (fx (dl, "depth", "Depth"), 40, 76) });
        setGroup (from, fg, 1);
        from = items.size();
        row (-1, 960, 154, 26, 12, { D (fx (rv, "mode", "Reverb Type"), 120, "Type"), T (fx (rv, "sync", "Pre-delay Sync"), 64, "Pre-dly sync", "SYNC") });
        row (-1, 946, 210, 40, 0, { K (fx (rv, "dryWet", "Mix"), 40, 76), K (fx (rv, "decay", "Decay"), 40, 76), K (fx (rv, "damp", "Damp"), 40, 76) });
        row (-1, 946, 294, 40, 0, { K (fx (rv, "color", "Color"), 40, 76), K (fx (rv, "drySource", "Dry"), 40, 76), K (fx (rv, "predelay", "Pre-dly"), 40, 76) });
        setGroup (from, fg, 2);
    }

    // ---- MOD B: SEQ / ARP / MATRIX (matrix pages are added per mode) -----------------------------
    auto& seqState = processor.stepSequencer().state();
    auto Q = [&] (const char* id, const char* l) -> AP { return std::make_unique<ApvtsAdapter> (*seqState.getParameter (id), l); };
    card (-1, Lay::modB, {});
    modBGroup = addTabs (-1, { 542, 674, 185, 22 }, { "SEQ", "ARP", "MATRIX" }, { gid::arpMode, gid::arpMode });
    {
        size_t from = items.size();
        row (-1, 540, 420, 34, 8, { D (Q ("scaleKey", "Key"), 52, "Key"), D (Q ("scale", "Scale"), 96, "Scale"), T (Q ("scaleLock", "Scale Lock"), 54, "Scale lock") });
        row (-1, 766, 420, 34, 0, { K (Q ("arpSwing", "Swing"), 34, 48), K (Q ("arpGate", "Gate"), 34, 48) });
        setGroup (from, modBGroup, 0);
        from = items.size();
        row (-1, 540, 420, 34, 8, { D (Q ("arpMode", "Arp Mode"), 96, "Mode"), D (Q ("arpRate", "Arp Speed"), 70, "Speed"),
                                    D (Q ("arpRateType", "Arp Type"), 80, "Type"), K (Q ("arpOctave", "Octave"), 34, 56) });
        setGroup (from, modBGroup, 1);
        noteGrid = processor.stepSequencer().createNoteGrid();
        addChildComponent (*noteGrid);
        auto& gridItem = place (-1, noteGrid.get(), { 542, 494, 376, 170 });
        gridItem.group = modBGroup; gridItem.indexMask = 0b011;      // SEQ and ARP pages
    }
    randomiseButton.setTooltip ("Randomise the steps (keeps to the key and scale when Scale Lock is on)");
    randomiseButton.onClick = [this] { processor.stepSequencer().randomise(); lcdName = "SEQUENCER"; lcdValue = "RANDOMISED"; };
    addChildComponent (randomiseButton);
    place (-1, &randomiseButton, { 730, 674, 84, 22 });
    dragHandle = vendor::StepSequencer::createDragHandle ([this]
    {
        return processor.stepSequencer().exportMidi (processor.currentBpm.load(), processor.getLastHeldNotes());
    });
    addChildComponent (*dragHandle);
    place (-1, dragHandle.get(), { 818, 674, 100, 22 });

    // ---- OUTPUT: EQ on top, knobs underneath -----------------------------------------------------
    card (-1, Lay::output, "OUTPUT");
    {
        auto* eqOnC = makeControl (H (gid::eqOn, "EQ"));
        eqOnC->comp->getProperties().set ("onText", "EQ");
        place (-1, eqOnC->comp.get(), { 1130, 414, 38, 22 });
        std::array<ParamAdapter*, 3> fA, qA, gA;
        for (int b = 0; b < 3; ++b)
        {
            fA[(size_t) b] = &keep (H (gid::eqFreq[b], "Freq"));
            qA[(size_t) b] = &keep (H (gid::eqQ[b], "Q"));
            gA[(size_t) b] = &keep (H (gid::eqGain[b], "Gain"));
        }
        auto graph = std::make_unique<EqGraph> (fA, qA, gA, [] (int v) { return VCCloneAudioProcessor::eqFreqHz (v); },
                                                [] (int v) { return VCCloneAudioProcessor::eqQValue (v); });
        eqGraph = graph.get();
        auto* eqOnParam = processor.apvts.getParameter (gid::eqOn);
        eqGraph->enabled = [eqOnParam] { return eqOnParam->getValue() > 0.5f; };
        addChildComponent (*graph);
        place (-1, eqGraph, { 952, 446, 216, 84 });
        widgets.push_back (std::move (graph));
        eqBandGroup = addTabs (-1, { 952, 536, 216, 20 }, { "LOW", "MID", "HIGH" }, {});
        tabGroups[(size_t) eqBandGroup]->selected = 1;
        for (int b = 0; b < 3; ++b)
        {
            auto& bt = *tabGroups[(size_t) eqBandGroup]->buttons[(size_t) b];
            bt.icon = [b] (Graphics& g, Rectangle<float> r, bool sel)
            {
                Path p;
                const float x0 = r.getCentreX() - 12.0f, x1 = r.getCentreX() + 12.0f, lo = r.getCentreY() + 3.5f, hi = r.getCentreY() - 3.5f;
                if (b == 0) { p.startNewSubPath (x0, hi); p.lineTo (x0 + 8.0f, hi); p.quadraticTo (x0 + 12.0f, lo, x0 + 16.0f, lo); p.lineTo (x1, lo); }
                else if (b == 1) { p.startNewSubPath (x0, lo); p.lineTo (x0 + 7.0f, lo); p.quadraticTo (r.getCentreX(), lo, r.getCentreX(), hi - 2.0f);
                                   p.quadraticTo (r.getCentreX(), lo, x1 - 7.0f, lo); p.lineTo (x1, lo); }
                else { p.startNewSubPath (x0, lo); p.lineTo (x0 + 8.0f, lo); p.quadraticTo (x0 + 12.0f, hi, x0 + 16.0f, hi); p.lineTo (x1, hi); }
                g.setColour (sel ? Colour (0xff7cc4ff) : Colour (0xffaab3bb));
                g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
            };
        }
        eqGraph->onSelect = [this] (int b) { tabGroups[(size_t) eqBandGroup]->selected = b; updateVisibility(); repaint(); };
        for (int b = 0; b < 3; ++b)
        {
            const size_t from = items.size();
            row (-1, 946, 564, 34, 0, { K (H (gid::eqFreq[b], "Freq"), 34, 76), K (H (gid::eqQ[b], b == 1 ? "Q" : "Slope"), 34, 76), K (H (gid::eqGain[b], "Gain"), 34, 76) });
            setGroup (from, eqBandGroup, b);
        }
    }
}

//==============================================================================
// Modulation A: the AMP and FILTER pages (global envelopes), shared by every mode
void VCClonePanel::addEnvPages (int m, int tabGroup)
{
    auto H = [this] (const char* id, const char* l) -> AP { return host (id, l); };
    for (int page = 0; page < 2; ++page)
    {
        const bool amp = page == 0;
        const char* sync = amp ? gid::ampSync : gid::fltSync;
        const size_t from = items.size();
        auto& a = keep (envTime (amp ? gid::ampA : gid::fltA, sync, "A"));
        auto& d = keep (envTime (amp ? gid::ampD : gid::fltD, sync, "D"));
        auto& s = keep (H (amp ? gid::ampS : gid::fltS, "S"));
        auto& r = keep (envTime (amp ? gid::ampR : gid::fltR, sync, "R"));
        addEnvGraph (m, { 132, 420, 254, 124 }, a, d, s, r);
        smallToggle (m, H (gid::useGlobalEnv, "Global ADSR"), { 286, 426, 48, 16 }, "GLOBAL");
        smallToggle (m, H (sync, "Sync"), { 338, 426, 42, 16 }, "SYNC");
        row (m, 128, 556, 38, 0, { K (envTime (amp ? gid::ampA : gid::fltA, sync, "Attack"), 38, 64), K (envTime (amp ? gid::ampD : gid::fltD, sync, "Decay"), 38, 64),
                                   K (H (amp ? gid::ampS : gid::fltS, "Sustain"), 38, 64), K (envTime (amp ? gid::ampR : gid::fltR, sync, "Release"), 38, 64) });
        fixedBox (m, { 396, 420, 112, 28 }, amp ? "Volume" : "Cutoff", "Send to");
        if (amp) row (m, 422, 480, 38, 0, { K (H (gid::velocity, "Vel"), 38, 60) });
        else row (m, 392, 480, 38, 0, { K (H (gid::envAmt, "Amount"), 38, 60), K (H (gid::keytrack, "Keytrk"), 38, 60) });
        setGroup (from, tabGroup, page);
    }
}

void VCClonePanel::addOscCommon (int m)
{
    card (m, Lay::osc, "OSCILLATOR");
    card (m, Lay::levels, {});
    card (m, Lay::unison, "UNISON");
    card (m, Lay::mix, "MIX");
    card (m, Lay::filter, "FILTER 1 & 2");
    card (m, Lay::play, "PLAY");
    card (m, Lay::modA, {});
}

/** The oscillator page's centre: model / mode dropdown, octave buttons, readout, wave dropdown. */
void VCClonePanel::oscCentre (int m, AP model, const String& modelCaption, ParamAdapter* octave, ParamAdapter* semi, ParamAdapter* fine,
                              const String& fineLabel, std::function<float (float)> wave, AP waveList, const String& waveCaption)
{
    if (model) row (m, 108, 152, 26, 0, { D (std::move (model), 124, modelCaption) });
    if (octave != nullptr)
    {
        auto st = std::make_unique<StepperButtons> (*octave);
        st->onTouched = [this, octave] { lcdName = "OCTAVE"; lcdValue = octave->text (octave->get()); };
        addChildComponent (*st);
        place (m, st.get(), { 128, 198, 64, 18 }, "Octave").captionY = 219;
        widgets.push_back (std::move (st));
    }
    auto disp = std::make_unique<OscDisplay>();
    disp->oct = octave; disp->semi = semi; disp->fine = fine; disp->fineLabel = fineLabel; disp->wave = std::move (wave);
    addChildComponent (*disp);
    place (m, disp.get(), { 108, 242, 124, 80 });
    displays.push_back (disp.get());
    widgets.push_back (std::move (disp));
    if (waveList) row (m, 108, 336, 26, 0, { D (std::move (waveList), 124, waveCaption) });
}

void VCClonePanel::levelLed (int m, int index, const char* powerId)
{
    auto* prm = processor.apvts.getParameter (powerId);
    auto b = std::make_unique<TabButton> (String(), true);
    b->isOn = [prm] { return prm->getValue() > 0.5f; };
    b->togglePower = [prm] { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture(); };
    b->led = true;
    b->setTooltip ("On / off");
    auto* bp = b.get();
    b->onClick = [bp] { bp->togglePower(); bp->repaint(); };
    addChildComponent (*b);
    place (m, b.get(), { 368 + index * 84, 154, 12, 12 });
    widgets.push_back (std::move (b));
}

//==============================================================================
void VCClonePanel::buildVirus()
{
    const int M = V;
    auto Vp = [this] (int o, const char* l) -> AP { return VCCloneAudioProcessor::isHostParameter (o) ? virus (o, l) : nullptr; };
    auto H = [this] (const char* id, const char* l) -> AP { return host (id, l); };
    auto oct = [&] (int o) -> AP { return std::make_unique<OctaveAdapter> (Vp (o, "Octave"), 64); };
    auto cut = [] (AP a, const char* l) -> AP { return a ? std::make_unique<CutoffAdapter> (std::move (a), l) : nullptr; };
    addOscCommon (M);

    // ---- OSCILLATOR: 1, 2, 3, SUB --------------------------------------------------------------
    oscGroup[M] = addTabs (M, { 32, 116, 276, 24 }, { "1", "2", "3", "SUB" }, { gid::oscOn[0], gid::oscOn[1], gid::oscOn[2], gid::oscOn[3] });
    for (int k = 0; k < 4; ++k)
    {
        const size_t from = items.size();
        if (k < 2)
        {
            const int semiO = k == 0 ? Osc1_Semitone : Osc2_Semitone;
            auto* model = &keep (H (gid::virusOscModel[k], "Model"));
            auto shape = K (Vp (k == 0 ? Osc1_Shape : Osc2_Shape, "Shape"), 38, 74);
            auto pw = K (Vp (k == 0 ? Osc1_Pulsewidth : Osc2_Pulsewidth, "PW"), 38, 74);
            for (auto [cellPtr, hyper, plain] : { std::tuple<Cell*, const char*, const char*> { &shape, "Density", "Shape" },
                                                  std::tuple<Cell*, const char*, const char*> { &pw, "Spread", "PW" } })
                if (cellPtr->control)
                    retitles.push_back ({ static_cast<OttKnob*> (cellPtr->control->comp.get()),
                                          [model, hyper, plain] { return String (model->text (model->get()).containsIgnoreCase ("hyper") ? hyper : plain); } });
            row (M, 24, 152, 38, 0, { K (Vp (semiO, "Semi"), 38, 74) });
            row (M, 24, 232, 38, 0, { shape });
            row (M, 24, 312, 38, 0, { pw });
            if (k == 0)
            {
                row (M, 242, 152, 38, 0, { K (Vp (Osc1_Keyfollow, "Keytrk"), 38, 74) });
                row (M, 242, 232, 38, 0, { K (Vp (Osc1_Shape_Velocity, "Shp vel"), 38, 74) });
                row (M, 242, 312, 38, 0, { K (Vp (PulseWidth_Velocity, "PW vel"), 38, 74) });
            }
            else
            {
                row (M, 242, 152, 38, 0, { K (Vp (Osc2_Detune, "Detune"), 38, 74) });
                row (M, 242, 232, 38, 0, { K (Vp (Osc2_FM_Amount, "FM"), 38, 74) });
                row (M, 242, 312, 38, 0, { K (Vp (Osc2_Keyfollow, "Keytrk"), 38, 74) });
            }
            auto* waveSel = &keep (Vp (k == 0 ? Osc1_Wave_Select : Osc2_Wave_Select, "Wave"));
            oscCentre (M, H (gid::virusOscModel[k], "Model"), "Model", &keep (oct (semiO)), &keep (Vp (semiO, "Semi")),
                       k == 1 ? &keep (Vp (Osc2_Detune, "Detune")) : nullptr, k == 1 ? "DET" : "FINE",
                       [model, waveSel] (float p) { const auto mt = model->text (model->get()); return waveByName (mt.containsIgnoreCase ("classic") ? waveSel->text (waveSel->get()) : mt, p); },
                       Vp (k == 0 ? Osc1_Wave_Select : Osc2_Wave_Select, "Wave"), "Wave");
        }
        else if (k == 2)
        {
            auto* mode = &keep (Vp (Osc3_Mode, "Mode"));
            row (M, 24, 152, 38, 0, { K (Vp (Osc3_Semitone, "Semi"), 38, 74) });
            row (M, 24, 232, 38, 0, { K (Vp (Osc3_Detune, "Detune"), 38, 74) });
            row (M, 24, 312, 38, 0, { K (Vp (Osc_Init_Phase, "Phase"), 38, 74) });
            row (M, 242, 152, 38, 0, { K (Vp (Osc_Balance, "Balance"), 38, 74) });
            row (M, 242, 232, 38, 0, { K (Vp (Osc_Mainvolume, "Osc vol"), 38, 74) });
            row (M, 242, 312, 38, 0, { K (Vp (Osc_FM_Mode, "FM mode"), 38, 74) });
            oscCentre (M, Vp (Osc3_Mode, "Mode"), "Mode", &keep (oct (Osc3_Semitone)), &keep (Vp (Osc3_Semitone, "Semi")),
                       &keep (Vp (Osc3_Detune, "Detune")), "DET", [mode] (float p) { return waveByName (mode->text (mode->get()), p); }, nullptr, {});
        }
        else
        {
            auto* shape = &keep (Vp (Suboscillator_Shape, "Shape"));
            row (M, 24, 152, 38, 0, { K (Vp (Noise_Volume, "Noise"), 38, 74) });
            row (M, 24, 232, 38, 0, { K (Vp (Noise_Color, "Color"), 38, 74) });
            row (M, 242, 152, 38, 0, { K (Vp (Ringmodulator_Volume, "Ring"), 38, 74) });
            oscCentre (M, Vp (Suboscillator_Shape, "Sub Shape"), "Sub shape", nullptr, nullptr, nullptr, {},
                       [shape] (float p) { return waveByName (shape->get() > 0 ? "triangle" : "square", p); }, nullptr, {});
        }
        setGroup (from, oscGroup[M], k);
    }
    // levels strip
    row (M, 334, 86, 38, 4, { K (Vp (Osc_Mainvolume, "Osc vol"), 38, 80), K (Vp (Osc_Balance, "Balance"), 38, 80),
                              K (Vp (Osc3_Volume, "Osc 3"), 38, 80), K (Vp (Suboscillator_Volume, "Sub"), 38, 80) });
    levelLed (M, 2, gid::oscOn[2]);
    levelLed (M, 3, gid::oscOn[3]);

    // ---- UNISON / MIX ---------------------------------------------------------------------------
    row (M, 338, 220, 40, 4, { K (Vp (Unison_Detune, "Detune"), 40, 70), K (Vp (Unison_Pan_Spread, "Spread"), 40, 70) });
    row (M, 348, 302, 26, 0, { D (Vp (Unison_Mode, "Unison"), 122, "Voices") });
    row (M, 338, 350, 34, 4, { K (Vp (Unison_Lfo_Phase, "LFO phase"), 30, 70), K (Vp (Punch_Intensity, "Punch"), 30, 70) });
    row (M, 506, 220, 40, 0, { K (Vp (Panorama, "Pan"), 40, 76) });
    row (M, 506, 306, 40, 0, { K (Vp (Filter_Balance, "Flt bal"), 40, 76) });
    row (M, 586, 306, 40, 0, { K (Vp (Ringmodulator_Volume, "Ring"), 40, 76) });
    for (int k = 0; k < 3; ++k)
    {
        const size_t from = items.size();
        smallToggle (M, H (gid::virusRetrig[k], "Retrig"), { 596, 220, 64, 22 }, "RETRIG");
        if (k == 1) smallToggle (M, Vp (Osc2_Sync, "Sync"), { 596, 248, 64, 22 }, "SYNC");
        setGroup (from, oscGroup[M], k);
    }

    // ---- FILTER 1 & 2 ----------------------------------------------------------------------------
    smallToggle (M, Vp (Filter2_Cutoff_Link, "Link"), { 888, 84, 48, 22 }, "LINK");
    row (M, 692, 118, 26, 10, { D (Vp (Filter1_Mode, "Filter 1"), 108, "Filter 1"), D (Vp (Filter_Routing, "Routing"), 108, "Routing") });
    row (M, 684, 174, 40, 0, { K (cut (Vp (Cutoff, "Cut 1"), "Cut 1"), 40, 80), K (Vp (Filter1_Resonance, "Res 1"), 40, 80), K (Vp (Filter1_Keyfollow, "Keytrk"), 40, 80) });
    row (M, 684, 262, 40, 0, { K (cut (Vp (Cutoff2, "Cut 2"), "Cut 2"), 40, 80), K (Vp (Filter2_Resonance, "Res 2"), 40, 80), K (Vp (Filter_Balance, "Balance"), 40, 80) });
    row (M, 692, 346, 26, 10, { D (Vp (Filter2_Mode, "Filter 2"), 108, "Filter 2"), D (Vp (Filter1_Env_Polarity, "Env pol"), 108, "Env polarity") });

    // ---- PLAY --------------------------------------------------------------------------------------
    row (M, 28, 448, 40, 0, { K (Vp (Portamento_Time, "Glide"), 40, 74) });
    row (M, 28, 532, 34, 0, { K (Vp (Bender_Range_Up, "Bend up"), 34, 74) });
    row (M, 28, 612, 34, 0, { K (Vp (Bender_Range_Down, "Bend down"), 34, 74) });

    // ---- MOD A: AMP, FILTER, LFO 1-3 --------------------------------------------------------------
    const int mg = addTabs (M, { 132, 674, 374, 22 }, { "AMP", "FILTER", "LFO 1", "LFO 2", "LFO 3" }, {});
    addEnvPages (M, mg);
    struct Lf { int shape, rate, amount, clock, dest, k1, k2; const char *l1, *l2; };
    const Lf lfos[] = { { Lfo1_Shape, Lfo1_Rate, LFO1_Assign_Amount, Lfo1_Clock, LFO1_Assign_Dest, Osc1_Lfo1_Amount, Osc2_Lfo1_Amount, "Osc 1", "Osc 2" },
                        { Lfo2_Shape, Lfo2_Rate, LFO2_Assign_Amount, Lfo2_Clock, LFO2_Assign_Dest, Cutoff1_Lfo2_Amount, Cutoff2_Lfo2_Amount, "Cut 1", "Cut 2" },
                        { Lfo3_Shape, Lfo3_Rate, Osc_Lfo3_Amount, Lfo3_Clock, Lfo3_Destination, Lfo3_Fade_In_Time, Lfo3_Keyfollow, "Fade", "Keyflw" } };
    for (int i = 0; i < 3; ++i)
    {
        const size_t from = items.size();
        auto* shapeA = &keep (Vp (lfos[i].shape, "Shape"));
        addScope (M, { 132, 420, 254, 124 }, [shapeA] (float p) { return waveByName (shapeA->text (shapeA->get()), p); });
        row (M, 128, 556, 38, 2, { D (Vp (lfos[i].shape, "Shape"), 70, "Shape"), K (Vp (lfos[i].rate, "Rate"), 38, 60),
                                   K (Vp (lfos[i].amount, "Amount"), 38, 60), D (Vp (lfos[i].clock, "Clock"), 62, "Clock") });
        row (M, 396, 420, 28, 0, { D (Vp (lfos[i].dest, "Send To"), 112, "Send to") });
        row (M, 392, 480, 38, 0, { K (Vp (lfos[i].k1, lfos[i].l1), 38, 60), K (Vp (lfos[i].k2, lfos[i].l2), 38, 60) });
        setGroup (from, mg, 2 + i);
    }

    // ---- MOD B: MATRIX (the six assign slots) ------------------------------------------------------
    {
        const char* regions[] = { "assignA", "assignB", "assignC", "assignD", "assignE", "assignF" };
        const size_t tabsFrom = items.size();
        const int g = addTabs (M, { 542, 420, 376, 22 }, { "1", "2", "3", "4", "5", "6" }, {});
        setGroup (tabsFrom, modBGroup, 2);
        for (int s = 0; s < 6; ++s)
        {
            const size_t from = items.size();
            std::vector<Cell> lists, knobs;
            for (int o : regionOffsets (regions[s]))
            {
                auto a = virus (o, capitalised (shortVirusLabel (o).toLowerCase()));
                if (! a->items().isEmpty()) lists.push_back (D (std::move (a), 120, capitalised (shortVirusLabel (o).toLowerCase())));
                else knobs.push_back (K (std::move (a), 38, 64));
            }
            row (M, 542, 462, 26, 8, std::move (lists));
            row (M, 542, 528, 38, 4, std::move (knobs));
            setGroup (from, g, s);
            setGroup2 (from, modBGroup, 2);
        }
    }
}

void VCClonePanel::buildSylenth()
{
    using namespace vc::sylenth;
    const int M = SY;
    auto S = [this] (int i, const char* l) -> AP { return std::make_unique<SylenthAdapter> (processor, i, l); };
    auto cut = [] (AP a, const char* l) -> AP { return std::make_unique<CutoffAdapter> (std::move (a), l); };
    addOscCommon (M);

    oscGroup[M] = addTabs (M, { 32, 116, 276, 24 }, { "A1", "A2", "B1", "B2" }, { gid::oscOn[0], gid::oscOn[1], gid::oscOn[2], gid::oscOn[3] });
    for (int k = 0; k < 4; ++k)
    {
        const int b = OscBase + k * OscStride;
        const size_t from = items.size();
        row (M, 24, 152, 38, 0, { K (S (b + oNote, "Note"), 38, 74) });
        row (M, 24, 232, 38, 0, { K (S (b + oPhase, "Phase"), 38, 74) });
        row (M, 24, 312, 38, 0, { K (S (b + oKeytrack, "Keytrack"), 38, 74) });
        row (M, 242, 152, 38, 0, { K (S (b + oFine, "Fine"), 38, 74) });
        row (M, 242, 232, 38, 0, { K (S (b + oStereo, "Stereo"), 38, 74) });
        row (M, 242, 312, 38, 0, { K (S (b + oVolume, "Volume"), 38, 74) });
        auto* wave = &keep (S (b + oWave, "Wave"));
        oscCentre (M, nullptr, {}, &keep (S (b + oOctave, "Octave")), &keep (S (b + oNote, "Note")), &keep (S (b + oFine, "Fine")), "FINE",
                   [wave] (float p) { return waveByName (wave->text (wave->get()), p); }, S (b + oWave, "Wave"), "Wave");
        // unison and mix follow the selected oscillator
        row (M, 338, 220, 40, 4, { K (S (b + oDetune, "Detune"), 40, 70), K (S (b + oStereo, "Stereo"), 40, 70) });
        row (M, 348, 302, 26, 0, { D (S (b + oVoices, "Voices"), 122, "Voices") });
        row (M, 506, 220, 40, 0, { K (S (k < 2 ? MixA : MixB, k < 2 ? "Mix A" : "Mix B"), 40, 76) });
        row (M, 506, 306, 40, 0, { K (S (b + oPan, "Pan"), 40, 76) });
        row (M, 586, 306, 40, 0, { K (S (k < 2 ? FiltAInput : FiltBInput, "Filter in"), 40, 76) });
        smallToggle (M, S (b + oRetrig, "Retrig"), { 596, 220, 64, 22 }, "RETRIG");
        smallToggle (M, S (b + oInvert, "Invert"), { 596, 248, 64, 22 }, "INV");
        setGroup (from, oscGroup[M], k);
    }
    row (M, 334, 86, 38, 4, { K (S (OscBase + oVolume, "A1"), 38, 80), K (S (OscBase + OscStride + oVolume, "A2"), 38, 80),
                              K (S (OscBase + 2 * OscStride + oVolume, "B1"), 38, 80), K (S (OscBase + 3 * OscStride + oVolume, "B2"), 38, 80) });
    for (int k = 0; k < 4; ++k) levelLed (M, k, gid::oscOn[k]);

    row (M, 692, 118, 26, 10, { D (S (FiltA, "Filter A"), 108, "Filter A"), D (S (FiltB, "Filter B"), 108, "Filter B") });
    row (M, 684, 174, 40, 0, { K (cut (S (FCtl, "Cutoff"), "Cutoff"), 40, 80), K (S (FCtl + 1, "Reso"), 40, 80), K (S (FCtl + 2, "Warm"), 40, 80) });
    row (M, 684, 262, 40, 0, { K (cut (S (FiltA + 2, "Cut A"), "Cut A"), 40, 80), K (cut (S (FiltB + 2, "Cut B"), "Cut B"), 40, 80), K (S (FCtl + 3, "Keytrk"), 40, 80) });
    row (M, 692, 346, 26, 10, { D (S (FiltA + 1, "Slope A"), 108, "Slope A"), D (S (FiltB + 1, "Slope B"), 108, "Slope B") });

    row (M, 28, 448, 40, 0, { K (S (Porta, "Glide"), 40, 74) });
    row (M, 28, 532, 34, 0, { K (S (BendRange, "Bend"), 34, 74) });
    row (M, 28, 612, 34, 0, { K (S (Mono, "Mono"), 34, 74) });

    const int mg = addTabs (M, { 132, 674, 374, 22 }, { "AMP", "FILTER", "LFO 1", "LFO 2", "MOD 1", "MOD 2" }, {});
    addEnvPages (M, mg);
    for (int l = 0; l < 2; ++l)
    {
        const int b = l == 0 ? Lfo1 : Lfo2;
        const size_t from = items.size();
        auto* shapeA = &keep (S (b, "Shape"));
        addScope (M, { 132, 420, 254, 124 }, [shapeA] (float p) { return waveByName (shapeA->text (shapeA->get()), p); });
        row (M, 128, 556, 38, 2, { D (S (b, "Shape"), 70, "Shape"), K (S (b + 1, "Rate"), 38, 60), K (S (b + 3, "Gain"), 38, 60), D (S (b + 2, "Sync"), 62, "Sync") });
        row (M, 396, 420, 28, 0, { D (S (b + 5, "Send To"), 112, "Send to") });
        row (M, 392, 480, 38, 0, { K (S (b + 6, "Amount"), 38, 60), K (S (b + 8, "Amount 2"), 38, 60) });
        row (M, 396, 574, 28, 0, { D (S (b + 7, "Also To"), 112, "Also to") });
        setGroup (from, mg, 2 + l);
    }
    for (int e = 0; e < 2; ++e)
    {
        const int b = e == 0 ? Mod1 : Mod2;
        const size_t from = items.size();
        auto& a = keep (S (b, "A")); auto& d = keep (S (b + 1, "D")); auto& s = keep (S (b + 2, "S")); auto& r = keep (S (b + 3, "R"));
        addEnvGraph (M, { 132, 420, 254, 124 }, a, d, s, r);
        smallToggle (M, S (e == 0 ? Mod1Sync : Mod2Sync, "Sync"), { 338, 426, 42, 16 }, "SYNC");
        row (M, 128, 556, 38, 0, { K (S (b, "Attack"), 38, 64), K (S (b + 1, "Decay"), 38, 64), K (S (b + 2, "Sustain"), 38, 64), K (S (b + 3, "Release"), 38, 64) });
        row (M, 396, 420, 28, 0, { D (S (b + 4, "Send To"), 112, "Send to") });
        row (M, 392, 480, 38, 0, { K (S (b + 5, "Amount"), 38, 60), K (S (b + 7, "Amount 2"), 38, 60) });
        row (M, 396, 574, 28, 0, { D (S (b + 6, "Also To"), 112, "Also to") });
        setGroup (from, mg, 4 + e);
    }

    // MATRIX: Sylenth's four misc modulation slots
    {
        const size_t tabsFrom = items.size();
        const int g = addTabs (M, { 542, 420, 376, 22 }, { "1A", "1B", "2A", "2B" }, {});
        setGroup (tabsFrom, modBGroup, 2);
        for (int s = 0; s < 4; ++s)
        {
            const int b = Misc + s * 5;
            const size_t from = items.size();
            row (M, 542, 462, 26, 8, { D (S (b, "Source"), 180, "Source") });
            row (M, 542, 520, 38, 8, { D (S (b + 1, "Dest 1"), 180, "Send to"), K (S (b + 2, "Amount"), 38, 64) });
            row (M, 542, 590, 38, 8, { D (S (b + 3, "Dest 2"), 180, "Also to"), K (S (b + 4, "Amount 2"), 38, 64) });
            setGroup (from, g, s);
            setGroup2 (from, modBGroup, 2);
        }
    }
}

void VCClonePanel::buildSpire()
{
    const int M = SPm;
    auto P = [this] (int i, const char* l, SpK k, StringArray list = {}) -> AP { return std::make_unique<SpireAdapter> (processor, i, l, k, list); };
    auto cut = [] (AP a, const char* l) -> AP { return std::make_unique<CutoffAdapter> (std::move (a), l); };
    const auto waves = spWaves();
    const auto targets = spTargets();
    addOscCommon (M);

    oscGroup[M] = addTabs (M, { 32, 116, 276, 24 }, { "1", "2", "3", "4" }, { gid::oscOn[0], gid::oscOn[1], gid::oscOn[2], gid::oscOn[3] });
    const int mixIdx[4] = { sp::mix_osc1, sp::mix_osc2, sp::mix_osc3, sp::mix_osc4 };
    for (int k = 0; k < 4; ++k)
    {
        const int b = sp::osc1_type + k * 19;
        const size_t from = items.size();
        row (M, 24, 152, 38, 0, { K (P (b + 2, "Note", SpK::Semitone), 38, 74) });
        row (M, 24, 232, 38, 0, { K (P (b + 4, "Ctrl A", SpK::Knob1000), 38, 74) });
        row (M, 24, 312, 38, 0, { K (P (b + 8, "Wt mix", SpK::Knob1000), 38, 74) });
        row (M, 242, 152, 38, 0, { K (P (b + 3, "Fine", SpK::Signed), 38, 74) });
        row (M, 242, 232, 38, 0, { K (P (b + 5, "Ctrl B", SpK::Knob1000), 38, 74) });
        row (M, 242, 312, 38, 0, { K (P (b + 18, "To flt", SpK::Knob1000), 38, 74) });
        auto* type = &keep (P (b, "Mode", SpK::List, kSpOscTypes));
        oscCentre (M, P (b, "Mode", SpK::List, kSpOscTypes), "Mode", &keep (P (b + 1, "Octave", SpK::Octave)), &keep (P (b + 2, "Note", SpK::Semitone)),
                   &keep (P (b + 3, "Fine", SpK::Signed)), "FINE", [type] (float p) { return waveByName (type->text (type->get()), p); },
                   P (b + 7, "Wave", SpK::List, waves), "Wave");
        row (M, 338, 220, 40, 4, { K (P (b + 9, "Detune", SpK::Knob1000), 40, 70), K (P (b + 13, "Density", SpK::Knob1000), 40, 70) });
        row (M, 348, 302, 26, 0, { D (P (b + 12, "Unison Mode", SpK::List, kSpChords), 122, "Unison mode") });
        row (M, 348, 350, 26, 0, { D (P (b + 11, "Voices", SpK::List, kSpUnison), 122, "Voices") });
        row (M, 506, 220, 40, 0, { K (P (b + 10, "Wide", SpK::Knob1000), 40, 76) });
        row (M, 506, 306, 40, 0, { K (P (b + 14, "Pan", SpK::Signed), 40, 76) });
        row (M, 586, 306, 40, 0, { K (P (b + 18, "Filter in", SpK::Knob1000), 40, 76) });
        setGroup (from, oscGroup[M], k);
    }
    row (M, 334, 86, 38, 4, { K (P (mixIdx[0], "Osc 1", SpK::Knob1000), 38, 80), K (P (mixIdx[1], "Osc 2", SpK::Knob1000), 38, 80),
                              K (P (mixIdx[2], "Osc 3", SpK::Knob1000), 38, 80), K (P (mixIdx[3], "Osc 4", SpK::Knob1000), 38, 80) });
    for (int k = 0; k < 4; ++k) levelLed (M, k, gid::oscOn[k]);

    smallToggle (M, P (sp::flt_rout, "Parallel", SpK::Flag), { 836, 84, 48, 22 }, "PAR");
    smallToggle (M, P (sp::flt_link, "Link", SpK::Flag), { 888, 84, 48, 22 }, "LINK");
    row (M, 692, 118, 26, 10, { D (P (sp::flt1_type, "Filter 1", SpK::List, kSpFltTypes), 108, "Filter 1"), D (P (sp::flt1_mode, "Mode 1", SpK::List, kSpFltModes), 108, "Mode 1") });
    row (M, 684, 174, 40, 0, { K (cut (P (sp::flt1_cut, "Cut 1", SpK::Knob1000), "Cut 1"), 40, 80), K (P (sp::flt1_res, "Res 1", SpK::Knob1000), 40, 80),
                               K (P (sp::flt_keytrack, "Keytrk", SpK::Signed), 40, 80) });
    row (M, 684, 262, 40, 0, { K (cut (P (sp::flt2_cut, "Cut 2", SpK::Knob1000), "Cut 2"), 40, 80), K (P (sp::flt2_res, "Res 2", SpK::Knob1000), 40, 80),
                               K (P (sp::flt_balance, "Balance", SpK::Knob1000), 40, 80) });
    row (M, 692, 346, 26, 10, { D (P (sp::flt2_type, "Filter 2", SpK::List, kSpFltTypes), 108, "Filter 2"), D (P (sp::flt2_mode, "Mode 2", SpK::List, kSpFltModes), 108, "Mode 2") });

    row (M, 28, 448, 40, 0, { K (P (sp::glide, "Glide", SpK::Knob1000), 40, 74) });
    row (M, 34, 534, 26, 0, { D (P (sp::bend_up, "Bend Up", SpK::List, kSpBend), 62, "Bend up") });
    row (M, 34, 586, 26, 0, { D (P (sp::bend_dn, "Bend Down", SpK::List, kSpBend), 62, "Bend down") });
    row (M, 28, 634, 30, 0, { K (P (sp::drift, "Drift", SpK::Knob1000), 30, 74) });

    const int mg = addTabs (M, { 132, 674, 374, 22 }, { "AMP", "FILTER", "LFO", "ENV" }, {});
    addEnvPages (M, mg);
    const size_t selFrom = items.size();
    const int lsel = addTabs (M, { 292, 426, 88, 16 }, { "1", "2", "3", "4" }, {}, true);
    setGroup (selFrom, mg, 2);
    const size_t selFrom2 = items.size();
    const int esel = addTabs (M, { 292, 426, 88, 16 }, { "1", "2", "3", "4" }, {}, true);
    setGroup (selFrom2, mg, 3);
    for (int l = 0; l < 4; ++l)
    {
        const int b = sp::lfo1_rate + l * 16;
        const size_t from = items.size();
        auto* shapeA = &keep (P (b + 7, "Shape", SpK::List, spLfoForms()));
        addScope (M, { 132, 420, 254, 124 }, [shapeA] (float p) { return waveByName (shapeA->text (shapeA->get()), p); });
        row (M, 128, 556, 38, 2, { D (P (b + 7, "Shape", SpK::List, spLfoForms()), 70, "Shape"), K (P (b, "Rate", SpK::Knob1000), 38, 60),
                                   K (P (b + 8, "Amp", SpK::Knob1000), 38, 60), T (P (b + 3, "Sync", SpK::Flag), 62, "Sync", "SYNC") });
        row (M, 396, 420, 28, 0, { D (P (b + 10, "Send To", SpK::List, targets), 112, "Send to") });
        row (M, 392, 480, 38, 0, { K (P (b + 11, "Amount", SpK::Signed), 38, 60), K (P (b + 14, "Amount 2", SpK::Signed), 38, 60) });
        row (M, 396, 574, 28, 0, { D (P (b + 13, "Also To", SpK::List, targets), 112, "Also to") });
        setGroup (from, mg, 2); setGroup2 (from, lsel, l);
    }
    for (int e = 0; e < 4; ++e)
    {
        const int b = sp::env1_att + e * 16;
        const size_t from = items.size();
        auto& a = keep (P (b, "A", SpK::Knob1000)); auto& d = keep (P (b + 1, "D", SpK::Knob1000));
        auto& s = keep (P (b + 2, "S", SpK::Knob1000)); auto& r = keep (P (b + 5, "R", SpK::Knob1000));
        addEnvGraph (M, { 132, 420, 254, 124 }, a, d, s, r);
        row (M, 128, 556, 38, 0, { K (P (b, "Attack", SpK::Knob1000), 38, 64), K (P (b + 1, "Decay", SpK::Knob1000), 38, 64),
                                   K (P (b + 2, "Sustain", SpK::Knob1000), 38, 64), K (P (b + 5, "Release", SpK::Knob1000), 38, 64) });
        row (M, 396, 420, 28, 0, { D (P (b + 10, "Send To", SpK::List, targets), 112, "Send to") });
        row (M, 392, 480, 38, 0, { K (P (b + 11, "Amount", SpK::Signed), 38, 60), K (P (b + 14, "Amount 2", SpK::Signed), 38, 60) });
        row (M, 396, 574, 28, 0, { D (P (b + 13, "Also To", SpK::List, targets), 112, "Also to") });
        setGroup (from, mg, 3); setGroup2 (from, esel, e);
    }

    // MATRIX: fifteen slots
    {
        StringArray names;
        for (int s = 1; s <= 15; ++s) names.add (String (s));
        const size_t tabsFrom = items.size();
        const int g = addTabs (M, { 542, 420, 376, 22 }, names, {}, true);
        setGroup (tabsFrom, modBGroup, 2);
        const auto sources = spSources();
        for (int s = 0; s < 15; ++s)
        {
            const int b = sp::slot1_Src1 + s * 10;
            const size_t from = items.size();
            row (M, 542, 462, 26, 8, { D (P (b, "Source 1", SpK::List, sources), 180, "Source 1"), D (P (b + 1, "Source 2", SpK::List, sources), 180, "Source 2") });
            row (M, 542, 520, 38, 8, { D (P (b + 3, "Target 1", SpK::List, targets), 180, "Target 1"), K (P (b + 2, "Amount 1", SpK::Signed), 38, 64) });
            row (M, 542, 590, 38, 8, { D (P (b + 5, "Target 2", SpK::List, targets), 180, "Target 2"), K (P (b + 4, "Amount 2", SpK::Signed), 38, 64) });
            setGroup (from, g, s);
            setGroup2 (from, modBGroup, 2);
        }
    }
}

void VCClonePanel::buildMatrix() {}

//==============================================================================
VCClonePanel::VCClonePanel (VCCloneAudioProcessor& p) : processor (p), browser (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    createBackground();

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
    modeButton.setToggleState (true, dontSendNotification);
    nameButton.setAlpha (0.0f);            // clickable; the name is drawn by paint()
    voiceMeter.active = [this] { return processor.activeVoices.load(); };
    voiceMeter.maximum = [] { return 16; };
    addAndMakeVisible (voiceMeter);
    outputKnob = makeControl (host (gid::volume, "Output"), true);
    static_cast<OttKnob*> (outputKnob->comp.get())->setGeometry (30, 34);
    outputKnob->comp->setVisible (true);

    // oscillator copy / paste (the selected oscillator's page)
    copyButton.setTooltip ("Copy the selected oscillator's settings");
    pasteButton.setTooltip ("Paste them onto the selected oscillator");
    copyButton.onClick = [this] { copyOsc(); };
    pasteButton.onClick = [this] { pasteOsc(); };

    buildShared();
    buildVirus();
    buildSylenth();
    buildSpire();
    for (int m = 0; m < 3; ++m)
    {
        place (m, &copyButton, { 196, 84, 46, 22 });
        place (m, &pasteButton, { 246, 84, 54, 22 });
    }
    addChildComponent (copyButton);
    addChildComponent (pasteButton);

    // top bar: the mode's voices and voice mode
    {
        using namespace vc::sylenth;
        auto Vp = [this] (int o, const char* l) -> AP { return VCCloneAudioProcessor::isHostParameter (o) ? virus (o, l) : nullptr; };
        auto S = [this] (int i, const char* l) -> AP { return std::make_unique<SylenthAdapter> (processor, i, l); };
        auto P = [this] (int i, const char* l, SpK k, StringArray list = {}) -> AP { return std::make_unique<SpireAdapter> (processor, i, l, k, list); };
        headerCombo (V, Vp (Unison_Mode, "Voices"), { 730, 30, 58, 22 });
        headerCombo (V, Vp (Key_Mode, "Mode"), { 794, 30, 70, 22 });
        headerCombo (SY, S (Polyphony, "Voices"), { 730, 30, 58, 22 });
        headerCombo (SY, S (PortaMode, "Mode"), { 794, 30, 70, 22 });
        headerCombo (SPm, P (sp::poly, "Voices", SpK::List, kSpPoly), { 730, 30, 58, 22 });
        headerCombo (SPm, P (sp::mode, "Mode", SpK::List, kSpVoice), { 794, 30, 70, 22 });
        // output knobs: velocity, patch volume (per mode), mono, phase lock
        auto H = [this] (const char* id, const char* l) -> AP { return host (id, l); };
        row (V, 946, 640, 28, 0, { K (Vp (Amp_Velocity, "Velocity"), 28, 57), K (Vp (Patch_Volume, "Patch"), 28, 57) });
        row (SY, 946, 640, 28, 0, { K (H (gid::velocity, "Velocity"), 28, 57), K (S (MainVol, "Patch"), 28, 57) });
        row (SPm, 946, 640, 28, 0, { K (P (sp::velocity, "Velocity", SpK::Knob1000), 28, 57), K (P (sp::volume, "Patch", SpK::Knob1000), 28, 57) });
        row (-1, 1060, 640, 28, 0, { K (H (gid::monoHz, "Mono Hz"), 28, 57), K (H (gid::lockHz, "Ph lock"), 28, 57) });
    }
    addChildComponent (browser);

    setSize (kWidth, kHeight);
    resized();
    updateVisibility();
    timerCallback();
    startTimerHz (30);
}

void VCClonePanel::copyOsc()
{
    const int m = currentMode();
    const int g = oscGroup[m], sel = tabGroups[(size_t) g]->selected;
    oscClipboard.clear();
    for (auto& it : items)
        if (it.mode == m && it.group == g && it.index == sel && it.comp != nullptr)
            for (auto& c : controls) if (c->comp.get() == it.comp) oscClipboard.push_back (c->adapter->get());
    oscClipboardMode = m;
    lcdName = "OSCILLATOR"; lcdValue = "COPIED";
}

void VCClonePanel::pasteOsc()
{
    const int m = currentMode();
    if (oscClipboardMode != m || oscClipboard.empty()) return;
    const int g = oscGroup[m], sel = tabGroups[(size_t) g]->selected;
    std::vector<ParamAdapter*> targets;
    for (auto& it : items)
        if (it.mode == m && it.group == g && it.index == sel && it.comp != nullptr)
            for (auto& c : controls) if (c->comp.get() == it.comp) targets.push_back (c->adapter.get());
    if (targets.size() != oscClipboard.size()) { lcdName = "OSCILLATOR"; lcdValue = "CAN'T PASTE HERE"; return; }
    for (size_t i = 0; i < targets.size(); ++i) { targets[i]->beginGesture(); targets[i]->set (oscClipboard[i]); targets[i]->endGesture(); }
    lcdName = "OSCILLATOR"; lcdValue = "PASTED";
}

VCClonePanel::~VCClonePanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int VCClonePanel::currentMode() const { return (int) processor.getMode(); }
int VCClonePanel::currentLayer() const { return 0; }

bool VCClonePanel::itemVisible (const Item& it) const
{
    if (browserShown) return false;
    if (it.mode >= 0 && it.mode != currentMode()) return false;
    if (it.group >= 0)
    {
        const int sel = tabGroups[(size_t) it.group]->selected;
        if (it.indexMask != 0 ? ((it.indexMask >> sel) & 1) == 0 : sel != it.index) return false;
    }
    if (it.group2 >= 0 && tabGroups[(size_t) it.group2]->selected != it.index2) return false;
    return true;
}

void VCClonePanel::updateVisibility()
{
    // a component placed for several modes (copy / paste) is visible if any of its items is
    std::map<Component*, bool> vis;
    for (auto& it : items)
        if (it.comp) vis[it.comp] = vis[it.comp] || itemVisible (it);
    for (auto& [c, v] : vis) c->setVisible (v);
    for (auto& g : tabGroups)
        for (int i = 0; i < (int) g->buttons.size(); ++i) g->buttons[(size_t) i]->setToggleState (i == g->selected, dontSendNotification);
    if (eqGraph) eqGraph->selected = tabGroups[(size_t) eqBandGroup]->selected;
    browser.setVisible (browserShown);
    browserButton.setToggleState (browserShown, dontSendNotification);
    for (auto& c : controls) c->shown = INT_MIN;
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
    voiceMeter.setBounds (622, 30, 96, 16);
    if (outputKnob) outputKnob->comp->setBounds (884, 2, 60, 62);
    browserButton.setBounds (989, 20, 64, 24);
    loadButton.setBounds (1063, 20, 54, 24);
    exportButton.setBounds (1125, 20, 54, 24);
    browser.setBounds (0, 64, kWidth, kHeight - 64);
}

//==============================================================================
void VCClonePanel::createBackground()
{
    background = Image (Image::RGB, kWidth * 2, kHeight * 2, true);
    {
        Graphics g (background);
        g.addTransform (AffineTransform::scale (2.0f));
        g.setGradientFill (ColourGradient (Colour (0xff30353b), kWidth * 0.3f, 0.0f, Colour (0xff1a1c1f), kWidth * 0.3f, (float) kHeight * 0.9f, true));
        g.fillAll();
        g.setGradientFill (ColourGradient (Colour (0xff34383d), 0.0f, 0.0f, Colour (0xff23262a), 0.0f, 64.0f, false));
        g.fillRect (0, 0, kWidth, 64);
        g.setColour (Colour (0xff0c0d0f));
        g.drawHorizontalLine (63, 0.0f, (float) kWidth);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (64, 0.0f, (float) kWidth);
        auto inset = [&] (Rectangle<float> r, Colour c)
        {
            g.setColour (Colours::white.withAlpha (0.06f));
            g.drawRoundedRectangle (r.translated (0.0f, 1.0f), 8.0f, 1.0f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        };
        inset ({ 174.0f, 10.0f, 432.0f, 42.0f }, Colour (0xff15181b));
        inset ({ 612.0f, 10.0f, 112.0f, 42.0f }, Colour (0xff15181b));
    }
    // fine surface texture for the cards
    noise = Image (Image::ARGB, 256, 256, true);
    Random rng (0x5eed);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x)
        {
            const float v = rng.nextFloat();
            noise.setPixelAt (x, y, (v > 0.5f ? Colours::white : Colours::black).withAlpha (std::abs (v - 0.5f) * 0.07f));
        }
}

void VCClonePanel::drawCard (Graphics& g, const Card& c)
{
    const auto cb = c.r;
    // shadow, body, texture, bevels
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (Colours::black.withAlpha (0.09f));
        g.fillRoundedRectangle (cb.translated (0.0f, (float) i).expanded ((float) i * 0.5f), 8.0f + (float) i * 0.5f);
    }
    ColourGradient body (Colour (0xff30343a), 0.0f, cb.getY(), Colour (0xff202326), 0.0f, cb.getBottom(), false);
    body.addColour (0.4, Colour (0xff24272b));
    g.setGradientFill (body);
    g.fillRoundedRectangle (cb, 8.0f);
    {
        Graphics::ScopedSaveState s (g);
        Path clip; clip.addRoundedRectangle (cb, 8.0f);
        g.reduceClipRegion (clip);
        g.setTiledImageFill (noise, 0, 0, 1.0f);
        g.fillRect (cb);
    }
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) cb.getY() + 1, cb.getX() + 8.0f, cb.getRight() - 8.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawHorizontalLine ((int) cb.getBottom() - 2, cb.getX() + 8.0f, cb.getRight() - 8.0f);
    g.setColour (Colour (0xff0a0b0d));
    g.drawRoundedRectangle (cb.reduced (0.5f), 8.0f, 1.0f);
    if (c.title.isEmpty()) return;
    // the title tab, top left
    const auto font = CustomLookAndFeel::uiFont (13.0f, true);
    const float pw = c.power ? 22.0f : 0.0f;
    const float tw = font.getStringWidthFloat (c.title) + 26.0f + pw;
    Path tab;
    tab.addRoundedRectangle (cb.getX(), cb.getY(), tw, 30.0f, 8.0f, 8.0f, true, false, false, true);
    g.setGradientFill (ColourGradient (Colour (0xff1d2024), 0.0f, cb.getY(), Colour (0xff16181b), 0.0f, cb.getY() + 30.0f, false));
    g.fillPath (tab);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawLine (cb.getX() + 2.0f, cb.getY() + 30.5f, cb.getX() + tw - 8.0f, cb.getY() + 30.5f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawLine (cb.getX() + tw + 0.5f, cb.getY() + 4.0f, cb.getX() + tw + 0.5f, cb.getY() + 22.0f, 1.0f);
    g.setFont (font);
    g.setColour (Colours::black);
    g.drawText (c.title, Rectangle<float> (cb.getX() + 11.0f + pw, cb.getY() + 1.0f, tw, 30.0f), Justification::centredLeft);
    g.setColour (Colour (0xffe8edf2));
    g.drawText (c.title, Rectangle<float> (cb.getX() + 11.0f + pw, cb.getY(), tw, 30.0f), Justification::centredLeft);
}

void VCClonePanel::paint (Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());
    drawWordmark (g, { 20.0f, 6.0f, 150.0f, 52.0f });

    g.setFont (CustomLookAndFeel::uiFont (17.0f, true));
    g.setColour (Colour (0xffe8edf2));
    g.drawFittedText (processor.getPatchName().isEmpty() ? String ("Init") : processor.getPatchName(), nameButton.getBounds().withTrimmedLeft (4),
                      Justification::centredLeft, 1);
    g.setFont (CustomLookAndFeel::uiFont (10.5f, true));
    {
        const double now = Time::getMillisecondCounterHiRes();
        const bool status = statusMessage.isNotEmpty() && now < statusUntil;
        const bool touched = lcdName.isNotEmpty() && now < lcdUntil;
        g.setColour (status || touched ? Colour (0xff7cc4ff) : Colour (0xffe8edf2));
        g.drawFittedText (status ? statusMessage : touched ? lcdName + "  " + lcdValue : "VOICES " + String (processor.activeVoices.load()) + "/16",
                          { 622, 12, 98, 16 }, Justification::centredLeft, 1, 0.7f);
    }
    g.setFont (CustomLookAndFeel::uiFont (8.5f, true));
    g.setColour (Colour (0xff6f7880));
    g.drawText ("VOICES", 731, 14, 60, 12, Justification::centredLeft);
    g.drawText ("MODE", 795, 14, 60, 12, Justification::centredLeft);
    if (browserShown) return;

    const int mode = currentMode(), layer = currentLayer();
    for (auto& c : cards)
        if ((c.mode < 0 || c.mode == mode) && (c.layer < 0 || c.layer == layer)) drawCard (g, c);

    // the divider in the Output section
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRect (952, 632, 216, 1);
    g.setColour (Colours::white.withAlpha (0.04f));
    g.fillRect (952, 633, 216, 1);

    for (auto& it : items)
    {
        if (! itemVisible (it)) continue;
        const auto r = it.r.toFloat();
        if (it.style == 1)
        {
            g.setColour (Colour (0xff15181b));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (Colour (0xff2a3a4a));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
            g.setColour (Colour (0xff9fd2ff));
            g.setFont (CustomLookAndFeel::uiFont (12.0f, true));
            g.drawFittedText (it.fixedText, it.r.reduced (10, 0), Justification::centredLeft, 1);
        }
        else if (it.style == 3)
        {
            g.setColour (Colour (0xff6f7880));
            g.setFont (CustomLookAndFeel::uiFont (9.5f, true));
            g.drawFittedText (it.fixedText, it.r, Justification::topLeft, 4, 1.0f);
            continue;
        }
        if (it.caption.isNotEmpty())
        {
            g.setColour (kCaption);
            g.setFont (CustomLookAndFeel::uiFont (10.0f, true));
            g.drawText (it.caption, Rectangle<float> (r.getX() - 20.0f, (float) it.captionY, r.getWidth() + 40.0f, 13.0f), Justification::centred);
        }
    }
}

//==============================================================================
void VCClonePanel::timerCallback()
{
    processor.stepSequencer().syncScaleUnits();
    for (auto& cp : controls)
    {
        auto* c = cp.get();
        if (! c->comp->isVisible()) continue;
        const int v = c->adapter->get();
        if (c->kind == Control::Kind::Knob) { static_cast<OttKnob*> (c->comp.get())->refresh(); continue; }
        if (v == c->shown) continue;
        c->shown = v;
        switch (c->kind)
        {
            case Control::Kind::Combo: static_cast<ComboBox*> (c->comp.get())->setSelectedId (v - c->adapter->minimum() + 1, dontSendNotification); break;
            case Control::Kind::Toggle:
            {
                auto* b = static_cast<Button*> (c->comp.get());
                b->setToggleState (v != 0, dontSendNotification);
                const auto on = b->getProperties()["onText"].toString();
                if (on.isNotEmpty()) b->setButtonText (on == "ON" ? String (v != 0 ? "ON" : "OFF") : on);
                break;
            }
            default: break;
        }
    }
    for (auto& r : retitles) if (r.knob->isVisible()) r.knob->setTitle (r.title());
    for (auto& s : scopes) if (s->isVisible()) s->repaint();
    for (auto& e : envGraphs) if (e->isVisible()) e->refresh();
    for (auto* d : displays) if (d->isVisible()) d->repaint();
    if (eqGraph && eqGraph->isVisible()) eqGraph->repaint();

    const int mode = currentMode();
    if (mode != lastMode)
    {
        lastMode = mode;
        modeButton.setButtonText (String (kModeNames[mode]) + String::fromUTF8 (" \xe2\x96\xbe"));
        updateVisibility();
        repaint();
    }
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
