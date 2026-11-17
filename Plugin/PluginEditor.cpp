#include "PluginEditor.h"
#include "VirusCLayout.h"
#include "Tables/Tables.h"

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
    constexpr int kTopH = 36, kTabsY = 40, kTabsH = 24, kContentY = 68;
    constexpr int kBottomY = 730;
    constexpr int kStripH = 104, kStripY = kBottomY - kStripH - 6;
    constexpr int kRowH = 88, kRowGap = 4, kHeaderH = 16, kCellH = 64;
    const char* kPageNames[] = { "Synth", "Mod", "Matrix", "Arp", "FX", "Browser" };
    const char* kModeNames[] = { "VIRUS", "SYLENTH", "SPIRE" };

    // Spire value lists (names from the Spire documentation; the order of Spire's wave list is approximate)
    const StringArray kSpOscTypes { "Classic", "Noise", "FM", "AMSync", "SawPWM", "HardFM", "Vowel" };
    StringArray spWaves()
    {
        StringArray w { "Sin", "Tri", "TriSine" };
        for (int i = 1; i <= 4; ++i) w.add ("Para " + String (i));
        for (int i = 1; i <= 4; ++i) w.add ("Bass " + String (i));
        for (int i = 1; i <= 4; ++i) w.add ("Piano A" + String (i));
        for (int i = 1; i <= 3; ++i) w.add ("Violin " + String (i));
        w.addArray ({ "Oboe", "Sitar", "Bayan 1", "Bayan 2", "Guitar 1", "Guitar 2", "Guitar 3", "Organ 1", "Organ 2", "Organ 3",
                      "Brass 1", "Brass 2" });
        for (int i = 1; i <= 6; ++i) w.add ("Vocal " + String (i));
        w.addArray ({ "Robo 1", "Robo 2", "Robo 3", "Synth 1", "Synth 2", "Synth 3", "Inter 1", "Inter 2", "FM 1", "FM 2", "FM 3",
                      "Alarm 1", "Alarm 2" });
        return w;
    }
    const StringArray kSpUnison { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    const StringArray kSpChords { "1 Oct", "2 Oct", "3 Oct", "Maj3", "Min3", "Maj7", "Dom7", "MinMaj7", "Min7", "Min9", "Maj9",
                                  "Dom9", "HalfDim7", "Dim7", "Sus2", "Sus4", "5th", "4th" };
    const StringArray kSpFltTypes { "Off", "Perfecto", "Acido", "Infecto", "Scorpio", "Combo", "Shaper" };
    const StringArray kSpFltModes { "Mode 1", "Mode 2", "Mode 3", "Mode 4", "Mode 5", "Mode 6" };
    const StringArray kSpVoice { "Mono 1", "Mono 2", "Mono 3", "Mono 4", "Poly 1", "Poly 2" };
    const StringArray kSpPoly { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    const StringArray kSpBend { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24" };
    const StringArray kSpCurve { "Lin", "Exp", "Pow" };
    StringArray spLfoForms() { StringArray f { "Sin", "Tri", "Sqr", "S&H", "S&G" }; f.addArray (spWaves()); return f; }
    StringArray spTargets() { StringArray t; t.add ("Off"); for (int i = 1; i < 230; ++i) t.add (i == 45 ? String ("Cutoff 1") : "ID " + String (i)); return t; }
    StringArray spSources() { return { "Off", "Osc1", "Osc2", "Osc3", "Osc4", "Env1", "Env2", "Env3", "Env4", "LFO1", "LFO2", "LFO3", "LFO4", "Velocity",
                                       "KeyFollow", "RandBi", "PitchWheel", "ModWheel", "Aftertouch", "Foot", "Breath", "Expression", "Sustain",
                                       "Stepper1", "Stepper2", "ArpStep", "Mod1", "Mod2", "Mod3", "Mod4", "RandUni", "Const10%", "Const50%" }; }
    const StringArray kSpSync { "16/1", "8/1", "4/1", "2/1", "1/1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "3/16", "1/6", "1/8",
                                "3/32", "1/12", "1/16", "3/64", "1/24", "1/32", "1/64" };

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

VCClonePanel::Control* VCClonePanel::makeControl (std::unique_ptr<ParamAdapter> a, bool forceKnob)
{
    auto c = std::make_unique<Control>();
    auto* adapter = a.get();
    c->adapter = std::move (a);
    const auto items = adapter->items();
    const String fullName = adapter->name();

    auto touched = [this, adapter] { lcdName = adapter->name().toUpperCase(); lcdValue = adapter->text (adapter->get()); };

    if (adapter->isToggle())
    {
        c->kind = Control::Kind::Toggle;
        auto b = std::make_unique<TextButton> (fullName);
        b->setClickingTogglesState (true);
        b->setTooltip (fullName);
        b->onClick = [adapter, bp = b.get(), touched]
        {
            adapter->beginGesture(); adapter->set (bp->getToggleState() ? 1 : 0); adapter->endGesture(); touched();
        };
        c->comp = std::move (b);
    }
    else if (! items.isEmpty() && ! forceKnob)
    {
        c->kind = Control::Kind::Combo;
        c->cells = 2;
        auto box = std::make_unique<VirusCombo>();
        for (int i = 0; i < items.size(); ++i) box->addItem (items[i].isEmpty() ? String ("-") : items[i], i + 1);
        box->setTooltip (fullName);
        box->onChange = [adapter, bp = box.get(), touched]
        {
            const int id = bp->getSelectedId();
            if (id > 0) { adapter->beginGesture(); adapter->set (adapter->minimum() + id - 1); adapter->endGesture(); touched(); }
        };
        box->onTouched = touched;
        auto lbl = std::make_unique<Label> (String(), fullName.toUpperCase());
        lbl->setFont (CustomLookAndFeel::uiFont (9.5f, true));
        lbl->setColour (Label::textColourId, VirusColours::labelText);
        lbl->setJustificationType (Justification::centredLeft);
        lbl->setInterceptsMouseClicks (false, false);
        lbl->setMinimumHorizontalScale (0.6f);
        addChildComponent (*lbl);
        c->label = std::move (lbl);
        c->comp = std::move (box);
    }
    else
    {
        c->kind = Control::Kind::Knob;
        auto k = std::make_unique<VirusKnob> (VirusKnob::Size::Small, fullName);
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

VCClonePanel::Panel& VCClonePanel::addPanel (String title, int page, int mode, int row,
                                             std::vector<std::unique_ptr<ParamAdapter>> adapters, int group, int groupIndex)
{
    Panel p;
    p.title = std::move (title);
    p.page = page; p.mode = mode; p.row = row; p.group = group; p.groupIndex = groupIndex;
    int cells = 0;
    for (auto& a : adapters)
    {
        auto* c = makeControl (std::move (a));
        cells += c->cells;
        p.controls.push_back (c);
    }
    p.cols = jmax (1, cells);
    panels.push_back (std::move (p));
    return panels.back();
}

int VCClonePanel::addGroup (int page, int mode, int row, StringArray labels)
{
    auto g = std::make_unique<Group>();
    g->page = page; g->mode = mode; g->row = row; g->labels = labels;
    const int gi = (int) groups.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto b = std::make_unique<TextButton> (labels[i]);
        b->setClickingTogglesState (false);
        b->onClick = [this, gi, i] { groups[(size_t) gi]->selected = i; updateVisibility(); repaint(); };
        addChildComponent (*b);
        g->buttons.push_back (std::move (b));
    }
    groups.push_back (std::move (g));
    return gi;
}

//==============================================================================
void VCClonePanel::buildVirus()
{
    constexpr int M = (int) vc::SynthMode::Virus;
    auto offs = [this] (std::vector<int> o) { std::vector<std::unique_ptr<ParamAdapter>> v; for (int x : o) v.push_back (virus (x)); return v; };
    // (oscillators, filters, LFO 1/2 are on the compact Synth page: buildSynthPages)
    addPanel ("Patch", PageMod, M, 0, offs ({ Panorama, Portamento_Time, Patch_Volume, Amp_Velocity, Punch_Intensity, Transpose, Key_Mode }));
    addPanel ("Unison", PageMod, M, 0, offs (regionOffsets ("unison")));
    addPanel ("Filter Velocity / Saturation", PageMod, M, 0, offs ({ Flt1_EnvAmt_Velocity, Flt2_EnvAmt_Velocity, Resonance1_Velocity,
                                                                              Resonance2_Velocity, Saturation_Curve, Filter_Keytrack_Base }));
    // MOD: LFOs, the patch's own envelopes (used when Global ADSR is off), common
    addPanel ("LFO 3", PageMod, M, 2, offs (regionOffsets ("lfoC")));
    addPanel ("LFO 1 (all)", PageMod, M, 1, offs (regionOffsets ("lfoA")));
    addPanel ("LFO 2 (all)", PageMod, M, 2, offs (regionOffsets ("lfoB")));
    addPanel ("Bender", PageMod, M, 2, offs ({ Bender_Range_Up, Bender_Range_Down, Bender_Scale }));
    addPanel ("Patch Filter Env", PageMod, M, 3, offs (regionOffsets ("envFilter")));
    addPanel ("Patch Amp Env", PageMod, M, 3, offs (regionOffsets ("envAmp")));
    addPanel ("Tempo", PageMod, M, 3, offs ({ Clock_Tempo, Channel_Volume, Panorama_Velocity }));
    // MATRIX
    addPanel ("Assign 1", PageMatrix, M, 0, offs (regionOffsets ("assignA")));
    addPanel ("Assign 2", PageMatrix, M, 0, offs (regionOffsets ("assignB")));
    addPanel ("Assign 3", PageMatrix, M, 1, offs (regionOffsets ("assignC")));
    addPanel ("Assign 4", PageMatrix, M, 2, offs (regionOffsets ("assignD")));
    addPanel ("Assign 5", PageMatrix, M, 2, offs (regionOffsets ("assignE")));
    addPanel ("Assign 6", PageMatrix, M, 3, offs (regionOffsets ("assignF")));
}

void VCClonePanel::buildSylenth()
{
    using namespace vc::sylenth;
    constexpr int M = (int) vc::SynthMode::Sylenth;
    auto syl = [this] (std::vector<std::pair<int, const char*>> list)
    {
        std::vector<std::unique_ptr<ParamAdapter>> v;
        for (auto& [i, l] : list) v.push_back (std::make_unique<SylenthAdapter> (processor, i, l));
        return v;
    };
    auto osc = [&] (int o)
    {
        const int b = OscBase + o * OscStride;
        return syl ({ { b + oWave, "Wave" }, { b + oVoices, "Voices" }, { b + oOctave, "Octave" }, { b + oNote, "Note" },
                      { b + oFine, "Fine" }, { b + oDetune, "Detune" }, { b + oStereo, "Stereo" }, { b + oVolume, "Volume" },
                      { b + oPan, "Pan" }, { b + oPhase, "Phase" }, { b + oRetrig, "Retrig" }, { b + oInvert, "Invert" },
                      { b + oKeytrack, "Keytrack" } });
    };
    auto filt = [&] (int b) { return syl ({ { b, "Type" }, { b + 1, "Slope" }, { b + 2, "Cutoff" }, { b + 3, "Reso" }, { b + 4, "Drive" },
                                             { b == FiltA ? FiltAInput : FiltBInput, "Input" } }); };
    // (oscillators, filters, LFOs and voicing are on the compact Synth page: buildSynthPages)
    (void) osc; (void) filt;
    auto modEnv = [&] (int b) { return syl ({ { b, "Attack" }, { b + 1, "Decay" }, { b + 2, "Sustain" }, { b + 3, "Release" },
                                               { b + 4, "Dest 1" }, { b + 5, "Amt 1" }, { b + 6, "Dest 2" }, { b + 7, "Amt 2" } }); };
    auto lfo = [&] (int b) { return syl ({ { b, "Wave" }, { b + 1, "Rate" }, { b + 2, "Sync" }, { b == Lfo1 ? Lfo1Free : Lfo2Free, "Free" },
                                            { b + 3, "Gain" }, { b + 4, "Offset" },
                                            { b + 5, "Dest 1" }, { b + 6, "Amt 1" }, { b + 7, "Dest 2" }, { b + 8, "Amt 2" } }); };
    addPanel ("Mod Env 1", PageMod, M, 0, modEnv (Mod1));
    addPanel ("Mod Env 2", PageMod, M, 1, modEnv (Mod2));
    (void) lfo;
    addPanel ("Patch Amp Env A", PageMod, M, 0, syl ({ { AmpA, "Attack" }, { AmpA + 1, "Decay" }, { AmpA + 2, "Sustain" }, { AmpA + 3, "Release" } }));
    addPanel ("Patch Amp Env B", PageMod, M, 1, syl ({ { AmpB, "Attack" }, { AmpB + 1, "Decay" }, { AmpB + 2, "Sustain" }, { AmpB + 3, "Release" } }));
    // the four misc modulation slots on the Matrix page
    const char* miscNames[] = { "Misc 1A", "Misc 1B", "Misc 2A", "Misc 2B" };
    for (int m = 0; m < 4; ++m)
    {
        const int b = Misc + m * 5;
        addPanel (miscNames[m], PageMatrix, M, m, syl ({ { b, "Source" }, { b + 1, "Dest 1" }, { b + 2, "Amt 1" }, { b + 3, "Dest 2" }, { b + 4, "Amt 2" } }));
    }
}

void VCClonePanel::buildSpire()
{
    constexpr int M = (int) vc::SynthMode::Spire;
    struct D { int idx; const char* label; SpK kind; StringArray list; };
    auto spr = [this] (std::vector<D> list)
    {
        std::vector<std::unique_ptr<ParamAdapter>> v;
        for (auto& d : list) v.push_back (std::make_unique<SpireAdapter> (processor, d.idx, d.label, d.kind, d.list));
        return v;
    };
    const auto waves = spWaves();
    const auto targets = spTargets();
    // (oscillators, filters, LFOs, mixer and voice are on the compact Synth page: buildSynthPages)
    (void) waves;
    const int eg = addGroup (PageMod, M, 0, { "E1", "E2", "E3", "E4" });
    for (int e = 0; e < 4; ++e)
    {
        const int b = sp::env1_att + e * 16;
        addPanel ("Envelope " + String (e + 1) + (e == 0 ? " (VCA)" : ""), PageMod, M, 0,
                  spr ({ { b, "Attack", SpK::Knob1000, {} }, { b + 1, "Decay", SpK::Knob1000, {} }, { b + 2, "Sustain", SpK::Knob1000, {} },
                         { b + 3, "Slope T", SpK::Knob1000, {} }, { b + 4, "Slope L", SpK::Knob1000, {} }, { b + 5, "Release", SpK::Knob1000, {} },
                         { b + 10, "Target 1", SpK::List, targets }, { b + 11, "Amt 1", SpK::Signed, {} }, { b + 13, "Target 2", SpK::List, targets },
                         { b + 14, "Amt 2", SpK::Signed, {} }, { b + 12, "Vel 1", SpK::Signed, {} } }),
                  eg, e);
    }
    const int mg = addGroup (PageMod, M, 1, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" });
    for (int s = 0; s < 15; ++s)
    {
        const int b = sp::slot1_Src1 + s * 10;
        addPanel ("Matrix Slot " + String (s + 1), PageMod, M, 1,
                  spr ({ { b, "Source 1", SpK::List, spSources() }, { b + 1, "Source 2", SpK::List, spSources() },
                         { b + 3, "Target 1", SpK::List, targets }, { b + 2, "Amt 1", SpK::Signed, {} },
                         { b + 5, "Target 2", SpK::List, targets }, { b + 4, "Amt 2", SpK::Signed, {} } }),
                  mg, s);
    }
}

void VCClonePanel::buildGlobal()
{
    auto v = [this] (std::vector<std::pair<const char*, const char*>> list)
    {
        std::vector<std::unique_ptr<ParamAdapter>> out;
        for (auto& [id, l] : list) out.push_back (host (id, l));
        return out;
    };
    for (int page : { (int) PageMod, (int) PageMatrix })
    {
        auto& a = addPanel ("Filter ADSR", page, -1, 9, v ({ { gid::fltA, "Attack" }, { gid::fltD, "Decay" }, { gid::fltS, "Sustain" },
                                                              { gid::fltR, "Release" }, { gid::fltSync, "Sync" } }));
        a.strip = true;
        auto& b = addPanel ("Amp ADSR", page, -1, 9, v ({ { gid::ampA, "Attack" }, { gid::ampD, "Decay" }, { gid::ampS, "Sustain" },
                                                           { gid::ampR, "Release" }, { gid::ampSync, "Sync" }, { gid::useGlobalEnv, "Global" } }));
        b.strip = true;
        auto& c = addPanel ("Global Filter", page, -1, 9, v ({ { gid::filterMode, "Type" }, { gid::cutoff, "Cutoff" }, { gid::reso, "Reso" },
                                                                { gid::envAmt, "Env Amt" }, { gid::keytrack, "Keytrack" }, { gid::velocity, "Velocity" } }));
        c.strip = true;
    }
    addPanel ("Saturation (Virus)", PageFx, -1, 0, v ({ { gid::satCurve, "Curve" }, { gid::satDrive, "Drive" }, { gid::satMix, "Mix" } }));
    addPanel ("Global FX", PageFx, -1, 0, v ({ { gid::delayOn, "Delay" }, { gid::reverbOn, "Reverb" }, { gid::volume, "Volume" } }));

    // Arp section: the selector and ArpSampler's arp / sequencer controls
    auto& seqState = processor.stepSequencer().state();
    auto seq = [&] (const char* id, const char* label) { return std::make_unique<ApvtsAdapter> (*seqState.getParameter (id), label); };
    std::vector<std::unique_ptr<ParamAdapter>> arp;
    arp.push_back (host (gid::arpMode, "Arp Section"));
    addPanel ("Arp", PageArp, -1, 0, std::move (arp));
    std::vector<std::unique_ptr<ParamAdapter>> s;
    for (auto [id, label] : std::initializer_list<std::pair<const char*, const char*>> {
             { "arpMode", "Mode" }, { "arpRate", "Rate" }, { "arpRateType", "Type" }, { "arpGate", "Gate" }, { "arpSwing", "Swing" },
             { "arpOctave", "Octaves" }, { "velMode", "Velocity" }, { "retrigger", "Retrig" }, { "seqLength", "Length" },
             { "seqEnd", "End" }, { "scaleKey", "Key" }, { "scaleLock", "Scale Lock" }, { "autoPlay", "Auto Play" } })
        s.push_back (seq (id, label));
    addPanel ("Step Sequencer (ArpSampler)", PageArp, -1, 0, std::move (s));
}


//==============================================================================
// Compact Synth page: columns of sections per mode
VCClonePanel::Control* VCClonePanel::makeSegment (std::unique_ptr<ParamAdapter> a)
{
    auto c = std::make_unique<Control>();
    auto* adapter = a.get();
    c->adapter = std::move (a);
    c->kind = Control::Kind::Segment;
    auto seg = std::make_unique<SegmentSelector> (*adapter);
    seg->onTouched = [this, adapter] { lcdName = adapter->name().toUpperCase(); lcdValue = adapter->text (adapter->get()); };
    c->comp = std::move (seg);
    addChildComponent (*c->comp);
    controls.push_back (std::move (c));
    return controls.back().get();
}

VCClonePanel::Section& VCClonePanel::addSection (int mode, String title, Rectangle<float> bounds, std::unique_ptr<ParamAdapter> header,
                                                 std::unique_ptr<ParamAdapter> hero, std::vector<std::unique_ptr<ParamAdapter>> body,
                                                 int group, int groupIndex)
{
    Section sec;
    sec.title = std::move (title);
    sec.mode = mode;
    sec.bounds = bounds;
    sec.group = group;
    sec.groupIndex = groupIndex;
    if (header) sec.header = makeSegment (std::move (header));
    if (hero) sec.hero = makeControl (std::move (hero), true);
    for (auto& a : body)
    {
        if (! a) continue;
        // short numeric lists (octave, voices, bend ...) are knobs that step through the list
        bool knob = false;
        const auto items = a->items();
        if (! items.isEmpty())
        {
            knob = true;
            for (auto& it : items) if (! it.trim().containsOnly ("0123456789+-. ")) { knob = false; break; }
        }
        sec.controls.push_back (makeControl (std::move (a), knob));
    }
    sections.push_back (std::move (sec));
    return sections.back();
}

VCClonePanel::Section& VCClonePanel::addEnvSection (int mode, String title, Rectangle<float> bounds,
                                                    std::vector<std::unique_ptr<ParamAdapter>> adsrAndMore)
{
    auto& sec = addSection (mode, std::move (title), bounds, nullptr, nullptr, std::move (adsrAndMore));
    auto& c = sec.controls;
    auto graph = std::make_unique<EnvelopeGraph> (*c[0]->adapter, *c[1]->adapter, *c[2]->adapter, *c[3]->adapter);
    graph->onTouched = [this] (ParamAdapter& a) { lcdName = a.name().toUpperCase(); lcdValue = a.text (a.get()); };
    addChildComponent (*graph);
    sec.env = graph.get();
    envGraphs.push_back (std::move (graph));
    return sec;
}

int VCClonePanel::addHeaderGroup (int mode, StringArray labels)
{
    const int gi = addGroup (PageSynth, mode, -1, labels);
    groups[(size_t) gi]->header = true;
    return gi;
}

bool VCClonePanel::sectionVisible (const Section& s) const
{
    if (currentPage != PageSynth || (s.mode >= 0 && s.mode != currentMode())) return false;
    if (s.tab >= 0 && s.tab != displayTab) return false;
    return s.group < 0 || groups[(size_t) s.group]->selected == s.groupIndex;
}

void VCClonePanel::buildSynthPages()
{
    // Rows: [Osc 1 | Amp Env | Osc 2] [Osc 3 | Filter Env | Osc 4] [Filters | display (Arp, Distortion, Delay, Reverb) | Global filter + Voice]
    //       [modulation along the bottom]
    constexpr float L = 6.0f, R = (float) kWidth - 6.0f, gap = 4.0f;
    constexpr float oscW = 444.0f, oscH = 116.0f, midH = 300.0f;
    constexpr float rowA = (float) kContentY, rowB = rowA + oscH + gap, rowC = rowB + oscH + gap, rowD = rowC + midH + gap;
    constexpr float botH = (float) kBottomY - 2.0f - rowD;
    constexpr float cX = L + oscW + gap, cW = R - oscW - gap - cX;
    constexpr float sideW = 280.0f, dispX = L + sideW + gap, dispW = R - sideW - gap - dispX;
    displayArea = { dispX, rowC, dispW, midH };
    const Rectangle<float> osc1 { L, rowA, oscW, oscH }, osc2 { R - oscW, rowA, oscW, oscH };
    const Rectangle<float> osc3 { L, rowB, oscW, oscH }, osc4 { R - oscW, rowB, oscW, oscH };
    const Rectangle<float> ampEnv { cX, rowA, cW, oscH }, fltEnv { cX, rowB, cW, oscH };
    const Rectangle<float> leftMid { L, rowC, sideW, midH }, rightMid { R - sideW, rowC, sideW, midH };
    using AP = std::unique_ptr<ParamAdapter>;
    using List = std::vector<AP>;
    auto stack = [&] (Rectangle<float> area, std::vector<float> weights)
    {
        std::vector<Rectangle<float>> r;
        float total = 0; for (auto w : weights) total += w;
        const float scale = (area.getHeight() - gap * (float) (weights.size() - 1)) / total;
        float y = area.getY();
        for (auto w : weights) { r.push_back ({ area.getX(), y, area.getWidth(), w * scale }); y += w * scale + gap; }
        return r;
    };
    auto bottom = [&] (int n)
    {
        std::vector<Rectangle<float>> r;
        const float w = (R - L - gap * (float) (n - 1)) / (float) n;
        for (int i = 0; i < n; ++i) r.push_back ({ L + (w + gap) * (float) i, rowD, w, botH });
        return r;
    };
    auto list = [] (auto&&... a) { List v; (v.push_back (std::move (a)), ...); return v; };
    auto cut = [] (AP a, const char* label = "Cutoff") -> AP { return a ? std::make_unique<CutoffAdapter> (std::move (a), label) : nullptr; };
    auto H = [this] (const char* id, const char* label) -> AP { return host (id, label); };

    // shared per mode: the global envelopes, the global filter and the voice block
    auto shared = [&] (int M, List voice)
    {
        addEnvSection (M, "Amp Env", ampEnv, list (H (gid::ampA, "A"), H (gid::ampD, "D"), H (gid::ampS, "S"), H (gid::ampR, "R"),
                                                   H (gid::ampSync, "Sync"), H (gid::useGlobalEnv, "Global")));
        addEnvSection (M, "Filter Env", fltEnv, list (H (gid::fltA, "A"), H (gid::fltD, "D"), H (gid::fltS, "S"), H (gid::fltR, "R"),
                                                      H (gid::fltSync, "Sync")));
        const auto r = stack (rightMid, { 1.0f, 1.0f });
        addSection (M, "Global Filter", r[0], H (gid::filterMode, "Type"), cut (H (gid::cutoff, "Cutoff")),
                    list (H (gid::reso, "Res"), H (gid::envAmt, "Env"), H (gid::keytrack, "Key"), H (gid::velocity, "Vel")));
        voice.insert (voice.begin(), H (gid::reverbOn, "Reverb"));
        voice.insert (voice.begin(), H (gid::delayOn, "Delay"));
        voice.insert (voice.begin(), H (gid::volume, "Volume"));
        addSection (M, "Voice / FX", r[1], nullptr, nullptr, std::move (voice));
    };

    // ---- the centre display: Arp, Distortion, Delay, Reverb (same in every mode) ------------
    {
        const auto inner = displayArea.reduced (6.0f).withTrimmedTop (24.0f);
        auto& arp = addSection (-1, "Step Sequencer", inner.withHeight (22.0f), H (gid::arpMode, "Arp"), nullptr, {});
        arp.tab = 0;
        auto& dist = addSection (-1, "Distortion (Virus Saturation)", inner.withSizeKeepingCentre (inner.getWidth() * 0.7f, 120.0f), nullptr, nullptr,
                                 list (H (gid::satCurve, "Curve"), H (gid::satDrive, "Drive"), H (gid::satMix, "Mix")));
        dist.tab = 1;
        const char* tabNames[] = { "ARP", "DISTORT", "DELAY", "REVERB" };
        for (int i = 0; i < 4; ++i)
        {
            auto& t = displayTabs[(size_t) i];
            t.setButtonText (tabNames[i]);
            t.onClick = [this, i] { displayTab = i; updateVisibility(); repaint(); };
            addChildComponent (t);
        }
    }

    // ---- Virus -------------------------------------------------------------------------------
    {
        constexpr int M = (int) vc::SynthMode::Virus;
        auto V = [this] (int o, const char* l = "") -> AP
        { return VCCloneAudioProcessor::isHostParameter (o) ? virus (o, l) : nullptr; };
        addSection (M, "Osc 1", osc1, nullptr, nullptr,
                    list (V (Osc1_Wave_Select, "Wave"), V (Osc1_Shape, "Shape"), V (Osc1_Pulsewidth), V (Osc1_Semitone, "Semi"),
                          V (Osc1_Keyfollow, "Key"), V (Osc1_Shape_Velocity)));
        addSection (M, "Osc 2", osc2, nullptr, nullptr,
                    list (V (Osc2_Wave_Select, "Wave"), V (Osc2_Shape, "Shape"), V (Osc2_Pulsewidth), V (Osc2_Semitone, "Semi"),
                          V (Osc2_Detune, "Detune"), V (Osc2_FM_Amount), V (Osc_FM_Mode), V (Osc2_Sync, "Sync"), V (Osc2_Filt_Env_Amt),
                          V (FM_Filt_Env_Amt), V (Osc2_Keyfollow, "Key"), V (Osc2_Shape_Velocity), V (Fm_Amount_Velocity)));
        addSection (M, "Osc 3", osc3, nullptr, nullptr,
                    list (V (Osc3_Mode, "Mode"), V (Osc3_Volume, "Level"), V (Osc3_Semitone, "Semi"), V (Osc3_Detune, "Detune"),
                          V (Osc_Balance), V (Osc_Mainvolume), V (Osc_Init_Phase)));
        addSection (M, "Sub / Noise / Ring", osc4, nullptr, nullptr,
                    list (V (Suboscillator_Volume), V (Suboscillator_Shape), V (Ringmodulator_Volume), V (Noise_Volume), V (Noise_Color),
                          V (PulseWidth_Velocity)));
        const auto f = stack (leftMid, { 1, 1 });
        addSection (M, "Filter 1", f[0], V (Filter1_Mode, "Mode"), cut (V (Cutoff)),
                    list (V (Filter1_Resonance, "Res"), V (Filter1_Env_Amt, "Env"), V (Filter1_Keyfollow, "Key"),
                          V (Filter1_Env_Polarity, "Pol"), V (Filter_Routing, "Routing"), V (Filter_Balance, "Balance")));
        addSection (M, "Filter 2", f[1], V (Filter2_Mode, "Mode"), cut (V (Cutoff2), "Cutoff 2"),
                    list (V (Filter2_Resonance, "Res"), V (Filter2_Env_Amt, "Env"), V (Filter2_Keyfollow, "Key"),
                          V (Filter2_Env_Polarity, "Pol"), V (Filter2_Cutoff_Link, "Link")));
        const auto b = bottom (4);
        addSection (M, "LFO 1", b[0], nullptr, nullptr,
                    list (V (Lfo1_Shape, "Shape"), V (Lfo1_Rate, "Rate"), V (Lfo1_Clock, "Clock"), V (Lfo1_Symmetry, "Symm"),
                          V (Lfo1_Keytrigger, "Key Trig"), V (Osc1_Lfo1_Amount, "Osc 1"), V (Osc2_Lfo1_Amount, "Osc 2")));
        addSection (M, "LFO 2", b[1], nullptr, nullptr,
                    list (V (Lfo2_Shape, "Shape"), V (Lfo2_Rate, "Rate"), V (Lfo2_Clock, "Clock"), V (Lfo2_Symmetry, "Symm"),
                          V (Lfo2_Keytrigger, "Key Trig"), V (Cutoff1_Lfo2_Amount, "Cut 1"), V (Cutoff2_Lfo2_Amount, "Cut 2")));
        addSection (M, "LFO 3", b[2], nullptr, nullptr,
                    list (V (Lfo3_Shape, "Shape"), V (Lfo3_Rate, "Rate"), V (Lfo3_Clock, "Clock"), V (Lfo3_Destination, "Dest"),
                          V (Osc_Lfo3_Amount, "Amount"), V (Lfo3_Fade_In_Time, "Fade")));
        addSection (M, "Unison", b[3], nullptr, nullptr,
                    list (V (Unison_Mode, "Voices"), V (Unison_Detune, "Detune"), V (Unison_Pan_Spread, "Spread"), V (Unison_Lfo_Phase, "LFO Phs"),
                          V (Punch_Intensity, "Punch")));
        shared (M, list (V (Patch_Volume, "Patch Vol"), V (Portamento_Time, "Glide"), V (Key_Mode, "Key Mode"), V (Amp_Velocity, "Velocity")));
    }

    // ---- Sylenth -----------------------------------------------------------------------------
    {
        using namespace vc::sylenth;
        constexpr int M = (int) vc::SynthMode::Sylenth;
        auto S = [this] (int i, const char* l) -> AP { return std::make_unique<SylenthAdapter> (processor, i, l); };
        const Rectangle<float> pos[4] = { osc1, osc2, osc3, osc4 };
        const char* names[] = { "Osc A1", "Osc A2", "Osc B1", "Osc B2" };
        for (int k = 0; k < 4; ++k)
        {
            const int b = OscBase + k * OscStride;
            addSection (M, names[k], pos[k], S (b + oWave, "Wave"), nullptr,
                        list (S (b + oVoices, "Voices"), S (b + oOctave, "Octave"), S (b + oNote, "Note"), S (b + oFine, "Fine"),
                              S (b + oVolume, "Volume"), S (b + oPhase, "Phase"), S (b + oDetune, "Detune"), S (b + oStereo, "Stereo"),
                              S (b + oPan, "Pan"), S (b + oInvert, "Inv"), S (b + oRetrig, "Retrig")));
        }
        const auto f = stack (leftMid, { 1, 1, 1 });
        for (int k = 0; k < 2; ++k)
        {
            const int b = k == 0 ? FiltA : FiltB;
            addSection (M, k == 0 ? "Filter A" : "Filter B", f[(size_t) k], S (b, "Type"), cut (S (b + 2, "Cutoff")),
                        list (S (b + 3, "Res"), S (b + 4, "Drive"), S (b + 1, "Slope"), S (k == 0 ? FiltAInput : FiltBInput, "Input")));
        }
        addSection (M, "Filter Control", f[2], nullptr, cut (S (FCtl, "Cutoff")),
                    list (S (FCtl + 1, "Res"), S (FCtl + 3, "Key"), S (FCtl + 2, "Warm"), S (MixA, "Mix A"), S (MixB, "Mix B")));
        const auto bt = bottom (4);
        const int eg = addHeaderGroup (M, { "1", "2" });
        for (int e = 0; e < 2; ++e)
        {
            const int b = e == 0 ? Mod1 : Mod2;
            addSection (M, "Mod Env", bt[0], nullptr, nullptr,
                        list (S (b, "A"), S (b + 1, "D"), S (b + 2, "S"), S (b + 3, "R"), S (b + 4, "Dest 1"), S (b + 5, "Amt 1"),
                              S (b + 6, "Dest 2"), S (b + 7, "Amt 2")), eg, e);
        }
        const int lg = addHeaderGroup (M, { "1", "2" });
        for (int l = 0; l < 2; ++l)
        {
            const int b = l == 0 ? Lfo1 : Lfo2;
            addSection (M, "LFO", bt[1], nullptr, nullptr,
                        list (S (b, "Wave"), S (b + 1, "Rate"), S (b + 2, "Sync"), S (l == 0 ? Lfo1Free : Lfo2Free, "Free"), S (b + 3, "Gain"),
                              S (b + 4, "Offset"), S (b + 5, "Dest 1"), S (b + 6, "Amt 1"), S (b + 7, "Dest 2"), S (b + 8, "Amt 2")), lg, l);
        }
        const int mg = addHeaderGroup (M, { "1A", "1B", "2A", "2B" });
        for (int m = 0; m < 4; ++m)
        {
            const int b = Misc + m * 5;
            addSection (M, "Misc", bt[2], nullptr, nullptr,
                        list (S (b, "Source"), S (b + 1, "Dest 1"), S (b + 2, "Amt 1"), S (b + 3, "Dest 2"), S (b + 4, "Amt 2")), mg, m);
        }
        const int ag = addHeaderGroup (M, { "A", "B" });
        for (int k = 0; k < 2; ++k)
        {
            const int b = k == 0 ? AmpA : AmpB;
            addSection (M, "Patch Amp Env", bt[3], nullptr, nullptr,
                        list (S (b, "A"), S (b + 1, "D"), S (b + 2, "S"), S (b + 3, "R"), S (Solo, "Solo")), ag, k);
        }
        shared (M, list (S (MainVol, "Main Vol"), S (Polyphony, "Voices"), S (BendRange, "Bend"), S (Porta, "Glide"),
                         S (PortaMode, "Porta"), S (Mono, "Mono"), S (Legato, "Legato")));
    }

    // ---- Spire -------------------------------------------------------------------------------
    {
        constexpr int M = (int) vc::SynthMode::Spire;
        auto P = [this] (int i, const char* l, SpK k, StringArray items = {}) -> AP
        { return std::make_unique<SpireAdapter> (processor, i, l, k, items); };
        const auto waves = spWaves();
        const Rectangle<float> pos[4] = { osc1, osc2, osc3, osc4 };
        const int mixIdx[4] = { sp::mix_osc1, sp::mix_osc2, sp::mix_osc3, sp::mix_osc4 };
        const int muteIdx[4] = { sp::osc1_mute, sp::osc2_mute, sp::osc3_mute, sp::osc4_mute };
        for (int k = 0; k < 4; ++k)
        {
            const int b = sp::osc1_type + k * 19;
            addSection (M, "Osc " + String (k + 1), pos[k], P (b, "Type", SpK::List, kSpOscTypes), nullptr,
                        list (P (b + 7, "Wave", SpK::List, waves), P (b + 1, "Octave", SpK::Octave), P (b + 2, "Note", SpK::Semitone),
                              P (b + 3, "Fine", SpK::Signed), P (b + 4, "Ctrl A", SpK::Knob1000), P (b + 5, "Ctrl B", SpK::Knob1000),
                              P (b + 8, "Wt Mix", SpK::Knob1000), P (mixIdx[k], "Level", SpK::Knob1000),
                              P (b + 11, "Voices", SpK::List, kSpUnison), P (b + 9, "Detune", SpK::Knob1000), P (b + 10, "Width", SpK::Knob1000),
                              P (b + 13, "Density", SpK::Knob1000), P (b + 12, "Chord", SpK::List, kSpChords), P (b + 14, "Pan", SpK::Signed),
                              P (b + 18, "To Flt", SpK::Knob1000), P (muteIdx[k], "Mute", SpK::Flag)));
        }
        const auto f = stack (leftMid, { 1, 1 });
        addSection (M, "Filter 1", f[0], P (sp::flt1_type, "Type", SpK::List, kSpFltTypes), cut (P (sp::flt1_cut, "Cutoff", SpK::Knob1000)),
                    list (P (sp::flt1_res, "Res", SpK::Knob1000), P (sp::flt1_mode, "Mode", SpK::List, kSpFltModes)));
        addSection (M, "Filter 2", f[1], P (sp::flt2_type, "Type", SpK::List, kSpFltTypes), cut (P (sp::flt2_cut, "Cutoff", SpK::Knob1000)),
                    list (P (sp::flt2_res, "Res", SpK::Knob1000), P (sp::flt2_mode, "Mode", SpK::List, kSpFltModes)));
        const auto bt = bottom (4);
        const auto targets = spTargets();
        const int eg = addHeaderGroup (M, { "1", "2", "3", "4" });
        for (int e = 0; e < 4; ++e)
        {
            const int b = sp::env1_att + e * 16;
            addSection (M, "Env", bt[0], nullptr, nullptr,
                        list (P (b, "A", SpK::Knob1000), P (b + 1, "D", SpK::Knob1000), P (b + 2, "S", SpK::Knob1000), P (b + 5, "R", SpK::Knob1000),
                              P (b + 10, "Target 1", SpK::List, targets), P (b + 11, "Amt 1", SpK::Signed),
                              P (b + 13, "Target 2", SpK::List, targets), P (b + 14, "Amt 2", SpK::Signed)), eg, e);
        }
        const int lg = addHeaderGroup (M, { "1", "2", "3", "4" });
        for (int l = 0; l < 4; ++l)
        {
            const int b = sp::lfo1_rate + l * 16;
            addSection (M, "LFO", bt[1], nullptr, nullptr,
                        list (P (b + 7, "Form", SpK::List, spLfoForms()), P (b, "Rate", SpK::Knob1000), P (b + 3, "Sync", SpK::Flag),
                              P (b + 2, "Symm", SpK::Signed), P (b + 8, "Amp", SpK::Knob1000), P (b + 10, "Target 1", SpK::List, targets),
                              P (b + 11, "Amt 1", SpK::Signed), P (b + 13, "Target 2", SpK::List, targets), P (b + 14, "Amt 2", SpK::Signed)), lg, l);
        }
        addSection (M, "Filter Routing", bt[2], nullptr, nullptr,
                    list (P (sp::flt_rout, "Parallel", SpK::Flag), P (sp::flt_link, "Link", SpK::Flag),
                          P (sp::flt_balance, "Balance", SpK::Knob1000), P (sp::flt_keytrack, "Key", SpK::Signed)));
        addSection (M, "Character", bt[3], nullptr, nullptr,
                    list (P (sp::drift, "Drift", SpK::Knob1000), P (sp::velocity, "Velocity", SpK::Knob1000),
                          P (sp::bend_up, "Bend Up", SpK::List, kSpBend), P (sp::bend_dn, "Bend Dn", SpK::List, kSpBend)));
        shared (M, list (P (sp::volume, "Spire Vol", SpK::Knob1000), P (sp::mode, "Mode", SpK::List, kSpVoice),
                         P (sp::poly, "Voices", SpK::List, kSpPoly), P (sp::glide, "Glide", SpK::Knob1000)));
    }
}

void VCClonePanel::layoutSections()
{
    constexpr float headerH = 18.0f;
    for (auto& sec : sections)
    {
        const auto b = sec.bounds;
        auto body = b.reduced (5.0f, 0.0f).withTrimmedTop (headerH + 3.0f).withTrimmedBottom (4.0f);
        // header: selector / group buttons at the right
        const float titleW = CustomLookAndFeel::uiFont (11.0f, true).getStringWidthFloat (sec.title.toUpperCase()) + 24.0f;
        const auto headerArea = Rectangle<float> (b.getX() + titleW, b.getY() + 1.5f, b.getWidth() - titleW - 4.0f, headerH - 3.0f);
        if (sec.header)
        {
            const int n = jmax (1, sec.header->adapter->items().size());
            const float w = jmin (headerArea.getWidth(), (float) n * 34.0f);
            sec.header->comp->setBounds (headerArea.withLeft (headerArea.getRight() - w).toNearestInt());
        }
        if (sec.group >= 0)
        {
            auto& g = *groups[(size_t) sec.group];
            const float w = jmin (headerArea.getWidth(), (float) g.labels.size() * 24.0f);
            g.area = headerArea.withLeft (headerArea.getRight() - w).toNearestInt();
        }
        if (sec.env)
        {
            if (body.getWidth() > body.getHeight() * 2.0f)     // wide block: graph left, controls right
                sec.env->setBounds (body.removeFromLeft (body.getWidth() * 0.55f).toNearestInt()), body.removeFromLeft (4.0f);
            else
            {
                const float rowH = jmin (50.0f, body.getHeight() * 0.36f);
                sec.env->setBounds (body.removeFromTop (body.getHeight() - rowH - 2.0f).toNearestInt());
            }
        }
        if (sec.hero)
        {
            const float side = jmin (body.getHeight(), 96.0f);
            sec.hero->comp->setBounds (body.removeFromLeft (side).withSizeKeepingCentre (side, side).toNearestInt());
            body.removeFromLeft (4.0f);
        }
        // grid: the largest cell that fits every control (combos take two cells)
        int cells = 0;
        for (auto* c : sec.controls) cells += c->kind == Control::Kind::Combo ? 2 : 1;
        float cell = 58.0f;
        int cols = 1;
        for (; cell > 30.0f; cell -= 1.0f)
        {
            cols = jmax (1, (int) (body.getWidth() / (cell * 0.95f)));
            int rows = 1, used = 0;
            for (auto* c : sec.controls)
            {
                const int w = c->kind == Control::Kind::Combo ? 2 : 1;
                if (used + w > cols) { ++rows; used = 0; }
                used += w;
            }
            if ((float) rows * cell <= body.getHeight()) break;
        }
        const float cw = body.getWidth() / (float) cols;
        int rows = 1, used = 0;
        for (auto* c : sec.controls)
        {
            const int w = c->kind == Control::Kind::Combo ? 2 : 1;
            if (used + w > cols) { ++rows; used = 0; }
            used += w;
        }
        const float ch = jmin (cell, 66.0f);
        float y = body.getY() + jmax (0.0f, (body.getHeight() - ch * (float) rows) * 0.5f);
        used = 0;
        for (auto* c : sec.controls)
        {
            const int w = c->kind == Control::Kind::Combo ? 2 : 1;
            if (used + w > cols) { used = 0; y += ch; }
            const auto r = Rectangle<float> (body.getX() + cw * (float) used, y, cw * (float) w, ch);
            used += w;
            switch (c->kind)
            {
                case Control::Kind::Knob: c->comp->setBounds (r.reduced (1.0f).toNearestInt()); break;
                case Control::Kind::Toggle: c->comp->setBounds (r.withSizeKeepingCentre (r.getWidth() - 6.0f, jmin (22.0f, r.getHeight() * 0.42f)).toNearestInt()); break;
                case Control::Kind::Combo:
                    c->label->setBounds (r.withTrimmedTop (r.getHeight() * 0.12f).withHeight (13.0f).reduced (3.0f, 0.0f).toNearestInt());
                    c->comp->setBounds (r.withTrimmedTop (r.getHeight() * 0.12f + 14.0f).withHeight (jmin (20.0f, r.getHeight() - 18.0f)).reduced (3.0f, 0.0f).toNearestInt());
                    break;
                case Control::Kind::Segment: c->comp->setBounds (r.reduced (2.0f, r.getHeight() * 0.3f).toNearestInt()); break;
            }
        }
        (void) cells;
    }
}

//==============================================================================
VCClonePanel::VCClonePanel (VCCloneAudioProcessor& p) : processor (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    setSize (kWidth, kHeight);
    createBackground();

    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (kModeNames[i]);
        b.setTooltip (String ("Synth mode: ") + kModeNames[i]);
        b.onClick = [this, i] { processor.setMode ((vc::SynthMode) i); };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < NumPages; ++i)
    {
        tabs[(size_t) i].setButtonText (kPageNames[i]);
        tabs[(size_t) i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[(size_t) i]);
    }
    loadButton.setTooltip ("Load a Virus (.mid/.syx), Sylenth (.fxb/.fxp) or Spire (.spf2) file - or drag one onto the window");
    loadButton.onClick = [this] { openLoadDialog(); };
    exportButton.setTooltip ("Export the current Virus patch or bank");
    exportButton.onClick = [this] { openExportMenu(); };
    initButton.setTooltip ("Init patch for the current synth mode");
    initButton.onClick = [this] { processor.loadInitPatch(); };
    prevButton.onClick = [this] { processor.setCurrentProgram (processor.getCurrentProgram() - 1); };
    nextButton.onClick = [this] { processor.setCurrentProgram (processor.getCurrentProgram() + 1); };
    presetBox.onChange = [this] { const int i = presetBox.getSelectedId() - 1; if (i >= 0 && i != processor.getCurrentProgram()) processor.setCurrentProgram (i); };
    for (Component* c : { (Component*) &loadButton, (Component*) &exportButton, (Component*) &initButton, (Component*) &prevButton,
                          (Component*) &nextButton, (Component*) &presetBox })
        addAndMakeVisible (c);

    buildVirus();
    buildSylenth();
    buildSpire();
    buildGlobal();
    buildSynthPages();

    // Gary's plugins, embedded
    delayPanel = vendor::createVirusDelayPanel (processor.delayProcessor());
    reverbPanel = vendor::createLeadVerbPanel (processor.reverbProcessor());
    stepGrid = processor.stepSequencer().createPanel();
    for (auto* c : { delayPanel.get(), reverbPanel.get(), stepGrid.get() }) addChildComponent (*c);

    patchList.setRowHeight (20);
    patchList.setColour (ListBox::backgroundColourId, Colour (0xff15181b));
    patchList.setColour (ListBox::outlineColourId, VirusColours::bevelDark);
    patchList.setOutlineThickness (1);
    addChildComponent (patchList);
    infoText.setMultiLine (true);
    infoText.setReadOnly (true);
    infoText.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 12.0f, Font::plain)));
    infoText.setColour (TextEditor::backgroundColourId, Colour (0xff15181b));
    infoText.setColour (TextEditor::textColourId, Colour (0xffdfe3e6));
    infoText.setColour (TextEditor::outlineColourId, VirusColours::bevelDark);
    addChildComponent (infoText);

    addAndMakeVisible (lcd);
    addAndMakeVisible (statusLcd);
    helpButton.onClick = [this] { helpOverlay.setVisible (true); helpOverlay.toFront (false); };
    addAndMakeVisible (helpButton);
    addChildComponent (helpOverlay);

    layoutAll();
    showPage (PageSynth);
    timerCallback();
    startTimerHz (30);
}

VCClonePanel::~VCClonePanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int VCClonePanel::currentMode() const { return (int) processor.getMode(); }

bool VCClonePanel::panelVisible (const Panel& p) const
{
    if (p.page != currentPage) return false;
    if (p.mode >= 0 && p.mode != currentMode()) return false;
    if (p.group >= 0 && groups[(size_t) p.group]->selected != p.groupIndex) return false;
    return true;
}

//==============================================================================
void VCClonePanel::layoutAll()
{
    // rows of panels per (page, mode); panels in a row share its width by their cell count.
    // A selector group takes a narrow column at the row's left; its alternatives share one slot.
    const float left = 8.0f, right = (float) kWidth - 8.0f, gap = 5.0f;
    for (int page = 0; page < NumPages; ++page)
        for (int mode = 0; mode < 3; ++mode)
            for (int row = 0; row < 10; ++row)
            {
                std::vector<Panel*> inRow;
                for (auto& p : panels)
                    if (p.page == page && p.row == row && (p.mode < 0 || p.mode == mode)) inRow.push_back (&p);
                if (inRow.empty()) continue;

                const bool strip = row == 9;
                const float y = strip ? (float) kStripY : (float) kContentY + row * (kRowH + kRowGap);
                float x = left;
                Group* grp = nullptr;
                for (auto& g : groups) if (g->page == page && g->row == row && (g->mode < 0 || g->mode == mode)) grp = g.get();
                if (grp != nullptr)
                {
                    const int bw = grp->labels.size() > 4 ? 30 : 28;
                    const int cols = grp->labels.size() > 4 ? 3 : 1;
                    grp->area = Rectangle<int> ((int) x, (int) y + 2, bw * cols, kRowH - 4);
                    x += (float) (bw * cols) + gap;
                }
                // slots: group alternatives share one slot
                std::vector<std::vector<Panel*>> slots;
                for (auto* p : inRow)
                {
                    bool placed = false;
                    if (p->group >= 0)
                        for (auto& s : slots) if (s.front()->group == p->group) { s.push_back (p); placed = true; break; }
                    if (! placed) slots.push_back ({ p });
                }
                float totalCells = 0;
                for (auto& s : slots) { int m = 0; for (auto* p : s) m = jmax (m, p->cols); totalCells += (float) m; }
                const float avail = right - x - gap * (float) (slots.size() - 1);
                for (auto& s : slots)
                {
                    int m = 0; for (auto* p : s) m = jmax (m, p->cols);
                    const float w = avail * (float) m / totalCells;
                    for (auto* p : s)
                    {
                        p->bounds = { x, y, w, (float) (strip ? kStripH : kRowH) };
                        // controls in a single row of cells, centred
                        const float cellW = jmin (66.0f, (w - 8.0f) / (float) jmax (1, p->cols));
                        float cx = x + (w - cellW * (float) p->cols) * 0.5f;
                        const float cy = y + kHeaderH + 3.0f + (strip ? 10.0f : 0.0f);
                        for (auto* c : p->controls)
                        {
                            const auto cell = Rectangle<float> (cx, cy, cellW * (float) c->cells, (float) kCellH);
                            cx += cellW * (float) c->cells;
                            switch (c->kind)
                            {
                                case Control::Kind::Knob: c->comp->setBounds (cell.reduced (2.0f, 1.0f).toNearestInt()); break;
                                case Control::Kind::Toggle: c->comp->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 6.0f, 22.0f).toNearestInt()); break;
                                case Control::Kind::Combo:
                                    c->label->setBounds (cell.withTrimmedTop (12.0f).withHeight (14.0f).reduced (3.0f, 0.0f).toNearestInt());
                                    c->comp->setBounds (cell.withTrimmedTop (28.0f).withHeight (22.0f).reduced (3.0f, 0.0f).toNearestInt());
                                    break;
                            }
                        }
                    }
                    x += w + gap;
                }
            }
    layoutSections();
    layoutEmbedded();
}

void VCClonePanel::layoutEmbedded()
{
    const auto dSize = vendor::virusDelayPanelSize(), rSize = vendor::leadVerbPanelSize();
    if (currentPage == PageSynth)
    {
        // inside the centre display: one panel at a time, under the display tabs
        const auto inner = displayArea.reduced (6.0f).withTrimmedTop (24.0f);
        auto fit = [&] (Component& c, Point<int> size, Rectangle<float>& area)
        {
            const float s = jmin (inner.getWidth() / (float) size.x, inner.getHeight() / (float) size.y);
            area = Rectangle<float> (size.x * s, size.y * s).withCentre (inner.getCentre());
            c.setBounds (0, 0, size.x, size.y);
            c.setTransform (AffineTransform::scale (s).translated (area.getX(), area.getY()));
        };
        fit (*delayPanel, dSize, delayArea);
        fit (*reverbPanel, rSize, reverbArea);
        gridArea = inner.withTrimmedTop (26.0f);
        stepGrid->setTransform ({});
        stepGrid->setBounds (gridArea.reduced (4.0f).toNearestInt());
        return;
    }
    // FX page: the delay and reverb panels side by side under the saturation / FX row
    const float top = (float) kContentY + kRowH + kRowGap;
    const float availH = (float) kBottomY - top - 6.0f, availW = (float) kWidth - 16.0f - 6.0f;
    const float s = jmin (availW / (float) (dSize.x + rSize.x), availH / (float) jmax (dSize.y, rSize.y));
    delayArea = { 8.0f, top, dSize.x * s, dSize.y * s };
    reverbArea = { 8.0f + dSize.x * s + 6.0f, top, rSize.x * s, rSize.y * s };
    delayPanel->setBounds (0, 0, dSize.x, dSize.y);
    delayPanel->setTransform (AffineTransform::scale (s).translated (delayArea.getX(), delayArea.getY()));
    reverbPanel->setBounds (0, 0, rSize.x, rSize.y);
    reverbPanel->setTransform (AffineTransform::scale (s).translated (reverbArea.getX(), reverbArea.getY()));

    // Arp page: the step sequencer grid
    gridArea = { 8.0f, top, (float) kWidth - 16.0f, jmin (availH, 420.0f) };
    stepGrid->setTransform ({});
    stepGrid->setBounds (gridArea.reduced (8.0f, 8.0f).withTrimmedTop (14.0f).toNearestInt());
}

void VCClonePanel::updateVisibility()
{
    for (auto& p : panels)
    {
        const bool vis = panelVisible (p);
        for (auto* c : p.controls) { c->comp->setVisible (vis); if (c->label) c->label->setVisible (vis); }
    }
    for (auto& sec : sections)
    {
        const bool vis = sectionVisible (sec);
        for (auto* c : sec.controls) { c->comp->setVisible (vis); if (c->label) c->label->setVisible (vis); }
        if (sec.header) sec.header->comp->setVisible (vis);
        if (sec.hero) sec.hero->comp->setVisible (vis);
        if (sec.env) sec.env->setVisible (vis);
    }
    for (auto& g : groups)
    {
        const bool vis = g->page == currentPage && (g->mode < 0 || g->mode == currentMode());
        if (g->header)
        {
            const int n = g->labels.size(), bw = g->area.getWidth() / jmax (1, n);
            for (int i = 0; i < n; ++i)
            {
                auto& b = *g->buttons[(size_t) i];
                b.setVisible (vis);
                b.setBounds (g->area.getX() + i * bw, g->area.getY(), bw - 2, g->area.getHeight());
                b.setToggleState (i == g->selected, dontSendNotification);
            }
            continue;
        }
        const int cols = g->labels.size() > 4 ? 3 : 1;
        const int bw = g->area.getWidth() / cols, rows = (g->labels.size() + cols - 1) / cols;
        const int bh = jmin (22, g->area.getHeight() / jmax (1, rows));
        for (int i = 0; i < (int) g->buttons.size(); ++i)
        {
            auto& b = *g->buttons[(size_t) i];
            b.setVisible (vis);
            b.setBounds (g->area.getX() + (i % cols) * bw, g->area.getY() + (i / cols) * bh, bw - 2, bh - 2);
            b.setToggleState (i == g->selected, dontSendNotification);
        }
    }
    const int arpMode = roundToInt (processor.apvts.getRawParameterValue (gid::arpMode)->load());
    layoutEmbedded();
    const bool synth = currentPage == PageSynth;
    delayPanel->setVisible (currentPage == PageFx || (synth && displayTab == 2));
    reverbPanel->setVisible (currentPage == PageFx || (synth && displayTab == 3));
    stepGrid->setVisible (currentPage == PageArp || (synth && displayTab == 0));
    for (int i = 0; i < 4; ++i)
    {
        auto& t = displayTabs[(size_t) i];
        t.setVisible (synth);
        t.setToggleState (i == displayTab, dontSendNotification);
        const float w = 86.0f;
        t.setBounds (Rectangle<float> (displayArea.getX() + 8.0f + (float) i * (w + 3.0f), displayArea.getY() + 4.0f, w, 20.0f).toNearestInt());
    }
    (void) arpMode;
    patchList.setVisible (currentPage == PageBrowser);
    infoText.setVisible (currentPage == PageBrowser);
    for (int i = 0; i < 3; ++i) modeButtons[(size_t) i].setToggleState (i == currentMode(), dontSendNotification);
    for (int i = 0; i < NumPages; ++i) tabs[(size_t) i].setToggleState (i == currentPage, dontSendNotification);
}

void VCClonePanel::showPage (int page)
{
    currentPage = page;
    if (page == PageBrowser) infoText.setText (buildInfoText(), false);
    updateVisibility();
    repaint();
}

void VCClonePanel::resized()
{
    for (int i = 0; i < 3; ++i) modeButtons[(size_t) i].setBounds (176 + i * 84, 7, 80, 22);
    loadButton.setBounds (440, 7, 52, 22);
    exportButton.setBounds (496, 7, 58, 22);
    initButton.setBounds (558, 7, 42, 22);
    presetBox.setBounds (650, 5, 276, 26);
    prevButton.setBounds (930, 7, 28, 22);
    nextButton.setBounds (962, 7, 28, 22);
    const int w = (kWidth - 16) / NumPages;
    for (int i = 0; i < NumPages; ++i) tabs[(size_t) i].setBounds (8 + i * w, kTabsY, w - 4, kTabsH);
    patchList.setBounds (16, kContentY + 26, 440, kBottomY - kContentY - 36);
    infoText.setBounds (476, kContentY + 26, kWidth - 492, kBottomY - kContentY - 36);
    lcd.setBounds (8, kBottomY + 3, 560, 26);
    statusLcd.setBounds (574, kBottomY + 3, kWidth - 614, 26);
    helpButton.setBounds (kWidth - 32, kBottomY + 5, 22, 22);
    helpOverlay.setBounds (getLocalBounds());
}

//==============================================================================
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
    g.setGradientFill (ColourGradient (Colours::transparentBlack, kWidth * 0.5f, kHeight * 0.45f, Colours::black.withAlpha (0.35f), 0.0f, 0.0f, true));
    g.fillAll();
}

void VCClonePanel::drawWordmark (Graphics& g, Rectangle<float> area)
{
    const float baseline = area.getBottom() - 8.0f;
    GlyphArrangement main;
    main.addLineOfText (Font (FontOptions (25.0f, Font::bold | Font::italic)), "vc", area.getX(), baseline);
    Path mainPath; main.createPath (mainPath);
    const auto mb = mainPath.getBounds();
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillPath (mainPath, AffineTransform::translation (1.0f, 1.5f));
    ColourGradient chrome (Colour (0xffffffff), 0.0f, mb.getY(), Colour (0xff8c949c), 0.0f, mb.getBottom(), false);
    chrome.addColour (0.5, Colour (0xffd5dade)); chrome.addColour (0.55, Colour (0xffa4acb3));
    g.setGradientFill (chrome);
    g.fillPath (mainPath);
    GlyphArrangement second;
    second.addLineOfText (Font (FontOptions (18.0f, Font::plain)), "clone", mb.getRight() + 5.0f, baseline);
    Path sp; second.createPath (sp);
    g.setColour (Colours::black.withAlpha (0.5f)); g.fillPath (sp, AffineTransform::translation (1.0f, 1.0f));
    g.setColour (VirusColours::blue); g.fillPath (sp);
    Path swoosh;
    const float sx = mb.getX() + 2.0f, sy = baseline + 3.5f, ex = sp.getBounds().getRight();
    swoosh.startNewSubPath (sx, sy);
    swoosh.quadraticTo ((sx + ex) * 0.5f, sy + 3.5f, ex + 6.0f, sy - 2.0f);
    g.setGradientFill (ColourGradient (VirusColours::blue.withAlpha (0.0f), sx, sy, VirusColours::blue, ex, sy, false));
    g.strokePath (swoosh, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

void VCClonePanel::paint (Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());
    auto top = Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kTopH);
    g.setGradientFill (ColourGradient (Colour (0xff4a4e53), 0.0f, 0.0f, Colour (0xff2b2e32), 0.0f, top.getBottom(), false));
    g.fillRect (top);
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kTopH - 1, 0.0f, (float) kWidth);
    drawWordmark (g, { 12.0f, 1.0f, 150.0f, (float) kTopH - 1.0f });
    CustomLookAndFeel::drawSmallLabel (g, "Preset", { 604.0f, 8.0f, 42.0f, 20.0f }, Justification::centredRight);
    CustomLookAndFeel::drawInsetBox (g, { 5.0f, (float) kTabsY - 2.0f, (float) kWidth - 10.0f, (float) kTabsH + 4.0f }, 4.0f);

    for (auto& p : panels)
        if (panelVisible (p)) CustomLookAndFeel::drawPanel (g, p.bounds, p.title);

    if (currentPage == PageSynth)
    {
        // the centre display: a recessed screen with the tab strip
        CustomLookAndFeel::drawPanel (g, displayArea, {});
        CustomLookAndFeel::drawInsetBox (g, displayArea.reduced (4.0f).withTrimmedTop (22.0f), 4.0f);

    }
    for (auto& sec : sections)
        if (sectionVisible (sec))
        {
            CustomLookAndFeel::drawPanel (g, sec.bounds, {});
            // title at the left of the header, then a blue dot
            const auto font = CustomLookAndFeel::uiFont (11.0f, true);
            const auto t = sec.title.toUpperCase();
            const float tw = font.getStringWidthFloat (t);
            const auto ta = Rectangle<float> (sec.bounds.getX() + 8.0f, sec.bounds.getY(), tw + 2.0f, 18.0f);
            g.setFont (font);
            g.setColour (Colours::black.withAlpha (0.45f));
            g.drawText (t, ta.translated (0.0f, 1.0f), Justification::centredLeft);
            g.setColour (VirusColours::headerText);
            g.drawText (t, ta, Justification::centredLeft);
            g.setColour (VirusColours::blue);
            g.fillEllipse (ta.getRight() + 4.0f, sec.bounds.getY() + 6.0f, 6.0f, 6.0f);
        }

    for (auto& gr : groups)
        if (! gr->header && gr->page == currentPage && (gr->mode < 0 || gr->mode == currentMode()))
            CustomLookAndFeel::drawInsetBox (g, gr->area.toFloat().expanded (2.0f), 3.0f);

    const int arpMode = roundToInt (processor.apvts.getRawParameterValue (gid::arpMode)->load());
    if (currentPage == PageFx || (currentPage == PageSynth && displayTab >= 2))
    {
        CustomLookAndFeel::drawInsetBox (g, (displayTab == 3 && currentPage == PageSynth ? reverbArea : delayArea).expanded (2.0f), 3.0f);
        if (currentPage == PageFx)
            CustomLookAndFeel::drawInsetBox (g, reverbArea.expanded (2.0f), 3.0f);
    }
    if (currentPage == PageArp)
    {
        CustomLookAndFeel::drawPanel (g, gridArea, "Step Sequencer");
    }
    if (currentPage == PageBrowser)
    {
        CustomLookAndFeel::drawPanel (g, { 8.0f, (float) kContentY + 2.0f, 456.0f, (float) (kBottomY - kContentY - 6) },
                                      processor.getBankName().isEmpty() ? String ("Patches") : "Patches - " + processor.getBankName());
        CustomLookAndFeel::drawPanel (g, { 468.0f, (float) kContentY + 2.0f, (float) kWidth - 476.0f, (float) (kBottomY - kContentY - 6) },
                                      "Patch Info / Warnings");
    }

    auto bar = Rectangle<float> (0.0f, (float) kBottomY, (float) kWidth, (float) (kHeight - kBottomY));
    g.setGradientFill (ColourGradient (Colour (0xff2e3135), 0.0f, bar.getY(), Colour (0xff1b1d20), 0.0f, bar.getBottom(), false));
    g.fillRect (bar);
    g.setColour (VirusColours::bevelDark);
    g.drawHorizontalLine (kBottomY - 1, 0.0f, (float) kWidth);
}

//==============================================================================
void VCClonePanel::timerCallback()
{
    // follow values set elsewhere (patch loads, host automation, MIDI)
    bool changed = false;
    for (auto& cp : controls)
    {
        auto* c = cp.get();
        if (! c->comp->isVisible()) continue;
        {
            const int v = c->adapter->get();
            if (v == c->shown) continue;
            c->shown = v;
            changed = true;
            switch (c->kind)
            {
                case Control::Kind::Knob: static_cast<VirusKnob*> (c->comp.get())->setValue (v, dontSendNotification); c->comp->repaint(); break;
                case Control::Kind::Combo: static_cast<ComboBox*> (c->comp.get())->setSelectedId (v - c->adapter->minimum() + 1, dontSendNotification); break;
                case Control::Kind::Toggle: static_cast<Button*> (c->comp.get())->setToggleState (v != 0, dontSendNotification); break;
                case Control::Kind::Segment: static_cast<SegmentSelector*> (c->comp.get())->setShown (v); break;
            }
        }
    }

    if (changed)
        for (auto& gr : envGraphs) if (gr->isVisible()) gr->repaint();

    const int mode = currentMode();
    const int arpMode = roundToInt (processor.apvts.getRawParameterValue (gid::arpMode)->load());
    if (mode != lastMode || arpMode != lastArpMode)
    {
        lastMode = mode;
        lastArpMode = arpMode;
        for (auto& c : controls) c->shown = INT_MIN;
        updateVisibility();
        repaint();
    }

    if (lcdName.isNotEmpty()) lcd.setContent (lcdName, lcdValue);
    else lcd.setContent (processor.getPatchName().toUpperCase(), kModeNames[mode]);

    const double now = Time::getMillisecondCounterHiRes();
    if (statusMessage.isNotEmpty() && now < statusUntil) statusLcd.setContent (statusMessage, {});
    else statusLcd.setContent (String (kModeNames[mode]) + "   VOICES " + String (processor.activeVoices.load()),
                               processor.getBank().empty() ? String ("INIT") : String (processor.getCurrentProgram() + 1) + "/" + String ((int) processor.getBank().size()));

    if (lastBankVersion != processor.bankVersion.load())
    {
        lastBankVersion = processor.bankVersion.load();
        refreshPresetList();
        lcdName.clear();
        for (auto& c : controls) c->shown = INT_MIN;
        if (currentPage == PageBrowser) infoText.setText (buildInfoText(), false);
        repaint();
    }
}

void VCClonePanel::refreshPresetList()
{
    presetBox.clear (dontSendNotification);
    const auto& bank = processor.getBank();
    static const char* typeTag[] = { "V", "Sy", "Sp" };
    for (int i = 0; i < (int) bank.size(); ++i)
        presetBox.addItem (String (i + 1).paddedLeft ('0', 3) + "  " + String (bank[(size_t) i].name) + "  [" + typeTag[(int) bank[(size_t) i].type] + "]", i + 1);
    if (bank.empty()) { presetBox.addItem ("---  " + processor.getPatchName(), 1); presetBox.setSelectedId (1, dontSendNotification); }
    else presetBox.setSelectedId (processor.getCurrentProgram() + 1, dontSendNotification);
    prevButton.setEnabled (bank.size() > 1);
    nextButton.setEnabled (bank.size() > 1);
    patchList.updateContent();
    patchList.selectRow (processor.getCurrentProgram(), true, true);
    patchList.repaint();
}

String VCClonePanel::buildInfoText() const
{
    String s;
    s << "Patch: " << processor.getPatchName() << "\n";
    s << "Synth mode: " << kModeNames[currentMode()] << "\n";
    if (processor.getBankFormat().isNotEmpty()) s << "File: " << processor.getBankName() << " (" << processor.getBankFormat() << ")\n";
    s << "\n";
    for (auto& n : processor.getPatchNotes()) s << "NOTE: " << n << "\n\n";
    if (currentMode() == 0)
    {
        const auto patch = processor.currentPatch();
        const auto ignored = vc::ignoredParametersInUse (patch);
        s << "IGNORED VIRUS PARAMETERS:\n";
        if (ignored.empty()) s << "  none\n";
        for (int o : ignored) s << "  " << String (L::kParams[o].name).trim() << "\n";
        s << "  (the Virus's own effects and arpeggiator are replaced by the global FX and arp sections)\n\n";
    }
    s << "IMPORT WARNINGS (" << (int) processor.getWarnings().size() << "):\n";
    int shown = 0;
    for (auto& w : processor.getWarnings())
    {
        if (++shown > 150) { s << "  ...\n"; break; }
        s << "  " << (w.patchIndex >= 0 ? "[" + String (w.patchIndex + 1).paddedLeft ('0', 3) + "] " : String()) << String (w.message) << "\n";
    }
    s << "\nVIRUS ENGINE TABLES:\n";
    for (auto& t : vc::tables::tableStatus())
        s << "  " << String (t.name).paddedRight (' ', 18) << (t.status == vc::tables::Status::Measured ? "measured" : "GUESSED") << "\n";
    return s;
}

int VCClonePanel::getNumRows() { return (int) processor.getBank().size(); }

void VCClonePanel::paintListBoxItem (int row, Graphics& g, int width, int height, bool selected)
{
    const auto& bank = processor.getBank();
    if (row < 0 || row >= (int) bank.size()) return;
    if (selected) { g.setColour (Colour (0xff2a6fb8)); g.fillRect (0, 0, width, height); }
    const auto& p = bank[(size_t) row];
    static const char* typeName[] = { "Virus", "Sylenth", "Spire" };
    g.setFont (CustomLookAndFeel::uiFont (12.5f, selected));
    g.setColour (selected ? Colours::white : Colour (0xffdfe3e6));
    g.drawText (String (row + 1).paddedLeft ('0', 3), 6, 0, 34, height, Justification::centredLeft);
    g.drawText (String (p.name), 44, 0, 190, height, Justification::centredLeft);
    g.setColour (selected ? Colours::white : VirusColours::labelText);
    g.drawText (typeName[(int) p.type], 240, 0, 70, height, Justification::centredLeft);
    g.drawText (String (p.type == vc::PatchType::Virus ? vc::valueToText (Category1, p.virus.value (Category1)) : p.category), 314, 0, 110, height, Justification::centredLeft);
}

void VCClonePanel::listBoxItemClicked (int row, const MouseEvent&) { processor.setCurrentProgram (row); }

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
    if (err.isNotEmpty()) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Could not load " + f.getFileName(), err);
    else { statusMessage = "LOADED " + String ((int) processor.getBank().size()) + " PATCHES"; statusUntil = Time::getMillisecondCounterHiRes() + 2500; }
}

void VCClonePanel::openLoadDialog()
{
    chooser = std::make_unique<FileChooser> ("Load Virus / Sylenth / Spire presets", File(), "*.mid;*.midi;*.syx;*.fxb;*.fxp;*.spf2;*.repatch");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
}

void VCClonePanel::openExportMenu()
{
    PopupMenu m;
    m.addItem (1, "Export Virus patch (.syx)", currentMode() == 0);
    m.addItem (2, "Export Virus patch (.mid)", currentMode() == 0);
    m.addItem (3, "Export Virus patches of the bank (.syx)");
    m.addItem (4, "Export Virus patches of the bank (.mid)");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (exportButton), [this] (int r)
    {
        if (r == 0) return;
        const bool mid = r == 2 || r == 4, bank = r >= 3;
        const String ext = mid ? ".mid" : ".syx";
        chooser = std::make_unique<FileChooser> ("Export", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("VC-Clone" + ext), "*" + ext);
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this, bank, ext] (const FileChooser& fc)
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
    if (sizeRestored) processor.editorWidth.store (getWidth());
}
