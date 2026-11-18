#include "SylenthEngine.h"
#include "Tables/Tables.h"
#include "SylenthDefaults.h"
#include <string>

namespace vc
{
using namespace sylenth;
using dsp::kPi;
using dsp::kTwoPi;

namespace sylenth
{
    static const char* kWaves[] = { "Sine", "Saw", "Triangle", "Pulse", "HPulse", "QPulse", "TriSaw", "Noise" };
    static const char* kVoices[] = { "Off", "1", "2", "3", "4", "5", "6", "7", "8" };
    static const char* kOct[] = { "-3", "-2", "-1", "0", "+1", "+2", "+3" };
    static const char* kNote[] = { "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7" };
    static const char* kOnOff[] = { "Off", "On" };
    static const char* kFType[] = { "Bypass", "Lowpass", "Bandpass", "Highpass" };
    static const char* kSlope[] = { "12 dB", "24 dB" };
    static const char* kDests[] = { "None", "Volume A", "Volume B", "Volume AB", "Pitch A", "Pitch B", "Pitch AB", "Phase A",
                                    "Phase B", "Phase AB", "Pan A", "Pan B", "Pan AB", "Cutoff A", "Cutoff B", "Cutoff AB", "Reso A",
                                    "Reso B", "Reso AB", "Phaser Freq", "Mix A", "Mix B", "Mix AB", "LFO1 Rate", "LFO1 Gain",
                                    "LFO2 Rate", "LFO2 Gain", "Dist Amount" };
    static const char* kSources[] = { "None", "Velocity", "ModWheel", "KeyTrack", "AmpEnv A", "AmpEnv B", "ModEnv 1", "ModEnv 2",
                                      "LFO 1", "LFO 2", "Aftertouch", "Step Velocity" };
    static const char* kLfoWaves[] = { "Sine", "Saw", "Ramp", "Triangle", "Pulse", "HPulse", "QPulse", "TriSaw", "Lorenz", "SmpHold", "Random" };
    static const char* kInput[] = { "Own part", "A and B", "None" };
    // tempo divisions, longest first: index 0 = Off, then 8/1D 8/1 8/1T 4/1D ... 1/256T (whole notes x dotted/straight/triplet)
    static const char* kSync[] = { "Off", "8/1D", "8/1", "8/1T", "4/1D", "4/1", "4/1T", "2/1D", "2/1", "2/1T", "1/1D", "1/1", "1/1T",
                                   "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T",
                                   "1/32D", "1/32", "1/32T", "1/64D", "1/64", "1/64T", "1/128D", "1/128", "1/128T", "1/256D", "1/256", "1/256T" };
    static double syncBeats (int index)       // quarter-note beats of a kSync entry
    {
        if (index <= 0) return 0.0;
        const int k = index - 1, base = k / 3, kind = k % 3;
        const double whole = 8.0 / std::pow (2.0, base);
        return whole * 4.0 * (kind == 0 ? 1.5 : kind == 1 ? 1.0 : 2.0 / 3.0);
    }
    static const char* kPortaMode[] = { "Normal", "Slide" };
    static const char* kBend[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18",
                                   "19", "20", "21", "22", "23", "24" };
    static const char* kPoly[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };

    struct Def { std::string id, name; float def; int steps; const char* const* items; };

    static std::vector<Def>& table()
    {
        static std::vector<Def> t = []
        {
            std::vector<Def> d ((size_t) NumParams);
            auto set = [&d] (int i, std::string id, std::string name, float def, int steps = 0, const char* const* items = nullptr)
            { d[(size_t) i] = { std::move (id), std::move (name), def, steps, items }; };
            const char* oscNames[] = { "A1", "A2", "B1", "B2" };
            for (int o = 0; o < 4; ++o)
            {
                const int b = OscBase + o * OscStride;
                const std::string pre = std::string ("syl_osc") + oscNames[o] + "_", nm = std::string ("Osc ") + oscNames[o] + " ";
                set (b + oWave, pre + "wave", nm + "Wave", 1.0f / 7.0f, 8, kWaves);
                set (b + oVoices, pre + "voices", nm + "Voices", o == 0 || o == 2 ? 1.0f / 8.0f : 0.0f, 9, kVoices);
                set (b + oDetune, pre + "detune", nm + "Detune", 0.25f);
                set (b + oStereo, pre + "stereo", nm + "Stereo", 0.5f);
                set (b + oOctave, pre + "octave", nm + "Octave", 0.5f, 7, kOct);
                set (b + oNote, pre + "note", nm + "Note", 0.5f, 15, kNote);
                set (b + oFine, pre + "fine", nm + "Fine", 0.5f);
                set (b + oPhase, pre + "phase", nm + "Phase", 0.0f);
                set (b + oRetrig, pre + "retrig", nm + "Retrig", 0.0f, 2, kOnOff);
                set (b + oInvert, pre + "invert", nm + "Invert", 0.0f, 2, kOnOff);
                set (b + oVolume, pre + "volume", nm + "Volume", 0.7f);
                set (b + oPan, pre + "pan", nm + "Pan", 0.5f);
                set (b + oKeytrack, pre + "keytrack", nm + "Keytrack", 1.0f, 2, kOnOff);
                set (b + oSpare, pre + "spare", nm + "Spare", 0.0f);
            }
            for (int f = 0; f < 2; ++f)
            {
                const int b = f == 0 ? FiltA : FiltB;
                const std::string pre = f == 0 ? "syl_filtA_" : "syl_filtB_", nm = f == 0 ? "Filter A " : "Filter B ";
                set (b + 0, pre + "type", nm + "Type", 1.0f / 3.0f, 4, kFType);
                set (b + 1, pre + "slope", nm + "Slope", 0.0f, 2, kSlope);
                set (b + 2, pre + "cutoff", nm + "Cutoff", 1.0f);
                set (b + 3, pre + "reso", nm + "Resonance", 0.0f);
                set (b + 4, pre + "drive", nm + "Drive", 0.0f);
                set (b + 5, pre + "keytrack", nm + "Keytrack", 0.0f);
            }
            set (FCtl + 0, "syl_fctl_cutoff", "Filter Ctl Cutoff", 0.5f);
            set (FCtl + 1, "syl_fctl_reso", "Filter Ctl Resonance", 0.0f);
            set (FCtl + 2, "syl_fctl_warm", "Filter Ctl Warm Drive", 0.0f, 2, kOnOff);
            set (FCtl + 3, "syl_fctl_keytrack", "Filter Ctl Keytrack", 0.0f);
            for (int e = 0; e < 2; ++e)
            {
                const int b = e == 0 ? AmpA : AmpB;
                const std::string pre = e == 0 ? "syl_ampA_" : "syl_ampB_", nm = e == 0 ? "Amp Env A " : "Amp Env B ";
                set (b + 0, pre + "attack", nm + "Attack", 0.0f);
                set (b + 1, pre + "decay", nm + "Decay", 0.3f);
                set (b + 2, pre + "sustain", nm + "Sustain", 1.0f);
                set (b + 3, pre + "release", nm + "Release", 0.15f);
            }
            for (int e = 0; e < 2; ++e)
            {
                const int b = e == 0 ? Mod1 : Mod2;
                const std::string pre = e == 0 ? "syl_mod1_" : "syl_mod2_", nm = e == 0 ? "Mod Env 1 " : "Mod Env 2 ";
                set (b + 0, pre + "attack", nm + "Attack", 0.0f);
                set (b + 1, pre + "decay", nm + "Decay", 0.3f);
                set (b + 2, pre + "sustain", nm + "Sustain", 0.0f);
                set (b + 3, pre + "release", nm + "Release", 0.15f);
                set (b + 4, pre + "dest1", nm + "Dest 1", 0.0f, DNumDests, kDests);
                set (b + 5, pre + "amt1", nm + "Amount 1", 0.5f);
                set (b + 6, pre + "dest2", nm + "Dest 2", 0.0f, DNumDests, kDests);
                set (b + 7, pre + "amt2", nm + "Amount 2", 0.5f);
            }
            for (int l = 0; l < 2; ++l)
            {
                const int b = l == 0 ? Lfo1 : Lfo2;
                const std::string pre = l == 0 ? "syl_lfo1_" : "syl_lfo2_", nm = l == 0 ? "LFO 1 " : "LFO 2 ";
                set (b + 0, pre + "wave", nm + "Wave", 0.0f, kNumLfoWaves, kLfoWaves);
                set (b + 1, pre + "rate", nm + "Rate", 0.5f);
                set (b + 2, pre + "sync", nm + "Sync", 0.0f, kNumSyncs, kSync);
                set (b + 3, pre + "gain", nm + "Gain", 1.0f);
                set (b + 4, pre + "offset", nm + "Offset", 0.5f);
                set (b + 5, pre + "dest1", nm + "Dest 1", 0.0f, DNumDests, kDests);
                set (b + 6, pre + "amt1", nm + "Amount 1", 0.5f);
                set (b + 7, pre + "dest2", nm + "Dest 2", 0.0f, DNumDests, kDests);
                set (b + 8, pre + "amt2", nm + "Amount 2", 0.5f);
            }
            set (MixA, "syl_mixA", "Mix A", 0.7f);
            set (MixB, "syl_mixB", "Mix B", 0.0f);
            set (Solo, "syl_solo", "Solo", 0.0f, 3, nullptr);
            set (Mono, "syl_mono", "Mono", 0.0f, 2, kOnOff);
            set (Legato, "syl_legato", "Legato", 0.0f, 2, kOnOff);
            set (Porta, "syl_porta", "Portamento", 0.0f);
            set (PortaMode, "syl_portamode", "Porta Mode", 0.0f, 2, kPortaMode);   // Normal, Slide
            set (Polyphony, "syl_poly", "Polyphony", 1.0f, 16, kPoly);
            set (BendRange, "syl_bend", "Bend Range", 1.0f / 23.0f, 24, kBend);
            set (MainVol, "syl_volume", "Main Volume", 0.7f);
            set (Lfo1Free, "syl_lfo1_free", "LFO 1 Free", 0.0f, 2, kOnOff);
            set (Lfo2Free, "syl_lfo2_free", "LFO 2 Free", 0.0f, 2, kOnOff);
            const char* miscNames[] = { "1A", "1B", "2A", "2B" };
            for (int m = 0; m < 4; ++m)
            {
                const int b = Misc + m * 5;
                const std::string pre = std::string ("syl_misc") + miscNames[m] + "_", nm = std::string ("Misc ") + miscNames[m] + " ";
                set (b + 0, pre + "source", nm + "Source", 0.0f, SNumSources, kSources);
                set (b + 1, pre + "dest1", nm + "Dest 1", 0.0f, DNumDests, kDests);
                set (b + 2, pre + "amt1", nm + "Amount 1", 0.5f);
                set (b + 3, pre + "dest2", nm + "Dest 2", 0.0f, DNumDests, kDests);
                set (b + 4, pre + "amt2", nm + "Amount 2", 0.5f);
            }
            set (FiltAInput, "syl_filtA_input", "Filter A Input", 0.0f, 3, kInput);
            set (FiltBInput, "syl_filtB_input", "Filter B Input", 0.0f, 3, kInput);
            set (Mod1Sync, "syl_mod1_sync", "Mod Env 1 Sync", 0.0f, 2, kOnOff);
            set (Mod2Sync, "syl_mod2_sync", "Mod Env 2 Sync", 0.0f, 2, kOnOff);
            return d;
        }();
        return t;
    }

    const ParamDef& def (int index)
    {
        static std::vector<ParamDef> defs = []
        {
            std::vector<ParamDef> v;
            for (auto& d : table()) v.push_back ({ d.id.c_str(), d.name.c_str(), d.def, d.steps });
            return v;
        }();
        return defs[(size_t) std::clamp (index, 0, NumParams - 1)];
    }

    const char* const* listItems (int index, int& count)
    {
        const auto& d = table()[(size_t) std::clamp (index, 0, NumParams - 1)];
        count = d.steps;
        static const char* kSolo[] = { "Off", "A", "B" };
        if (index == Solo) return kSolo;
        return d.items;
    }

    // GUESSED physical laws (to be calibrated against Sylenth with the harness)
    static double envSeconds (double v) { return 0.0005 + 12.0 * v * v * v; }
    static double cutoffHz (double v) { return 20.0 * std::pow (1000.0, std::clamp (v, 0.0, 1.0)); }
    static double lfoHz (double v) { return 0.04 * std::pow (1000.0, std::clamp (v, 0.0, 1.0)); }
}

//==============================================================================
std::array<float, SylenthEngine::kNumParams> SylenthEngine::initParams()
{
    std::array<float, kNumParams> a {};
    for (int i = 0; i < kNumParams; ++i) a[(size_t) i] = sylenth::def (i).def;
    return a;
}

SylenthEngine::SylenthEngine()
{
    params = initParams();
    rng.seed (0x51E);
}

void SylenthEngine::prepare (double hostRate, int maxBlock)
{
    sr = hostRate > 0 ? hostRate : 44100.0;
    bufL.assign ((size_t) std::max (64, maxBlock), 0.0);
    bufR = bufL;
    reset();
}

void SylenthEngine::reset()
{
    for (auto& v : voices) { v.active = false; v.gate = false; v.gstage.kill(); }
    held.clear();
    ageCounter = 0;
    rng.seed (0x51E);
}

int SylenthEngine::activeVoiceCount() const
{
    int n = 0;
    for (auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void SylenthEngine::allNotesOff (bool hard)
{
    for (auto& v : voices)
    {
        if (hard) { v.active = false; v.gstage.kill(); }
        else if (v.active) { v.gate = false; v.gstage.release(); for (auto& e : v.env) if (e.stage) e.stage = 4; }
    }
    held.clear();
}

void SylenthEngine::noteOn (int note, int velocity)
{
    const float vel = std::clamp (velocity / 127.0f, 0.0f, 1.0f);
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);
    if (p (Mono) > 0.5f)
    {
        Voice& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i) voices[i].active = false;
        startVoice (v, note, vel, p (Legato) > 0.5f && v.active && v.gate);
        return;
    }
    const int poly = listValue (p (Polyphony), 16) + 1;
    int activeCount = 0;
    for (auto& v : voices) activeCount += v.active && v.gate ? 1 : 0;
    Voice* best = nullptr;
    if (activeCount < poly)
        for (auto& v : voices) if (! v.active) { best = &v; break; }
    if (! best)
    {
        for (auto& v : voices) if (v.active && ! v.gate && (! best || v.age < best->age)) best = &v;
        if (! best) for (auto& v : voices) if (! best || v.age < best->age) best = &v;
    }
    startVoice (*best, note, vel, false);
}

void SylenthEngine::noteOff (int note)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    if (p (Mono) > 0.5f)
    {
        Voice& v = voices[0];
        if (! v.active || v.note != note) return;
        if (! held.empty()) { startVoice (v, held.back(), v.vel, p (Legato) > 0.5f); return; }
    }
    for (auto& v : voices)
        if (v.active && v.gate && v.note == note)
        {
            v.gate = false;
            v.gstage.release();
            for (auto& e : v.env) if (e.stage) e.stage = 4;
        }
}

void SylenthEngine::startVoice (Voice& v, int note, float vel, bool legato)
{
    const bool wasActive = v.active;
    v.active = true;
    v.gate = true;
    v.note = note;
    v.vel = vel;
    v.age = ++ageCounter;
    v.targetNote = note;
    const bool slideOnly = listValue (p (PortaMode), 2) == 1;      // Slide: glide only between overlapping notes
    if (! wasActive || p (Porta) <= 0.0f || (slideOnly && held.size() < 2)) v.curNote = note;
    if (legato) return;

    for (int o = 0; o < 4; ++o)
    {
        const int b = OscBase + o * OscStride;
        auto& osc = v.osc[(size_t) o];
        if (! wasActive) osc.rng.seed (rng.next());
        const bool lockPhase = global && global->lockHz >= 1.0;
        const bool retrig = p (b + oRetrig) > 0.5f || lockPhase;
        for (auto& ph : osc.phase)
            if (retrig || ! wasActive) ph = retrig ? p (b + oPhase) : osc.rng.uniform();
    }
    for (auto& e : v.env) { e.stage = 1; if (! wasActive) e.level = 0; }
    if (! wasActive)
        for (int s = 0; s < 2; ++s) for (int c = 0; c < 2; ++c) { v.fA[s][c].reset(); v.fB[s][c].reset(); }
    if (global) v.gstage.start (note, vel, *global, sr, ! wasActive);
    v.ctl = 0;
}

double SylenthEngine::envTick (Env& e, int base)
{
    // synced mod envelopes: the knob picks a note value from 1/256 to 4 bars (13 doublings)
    const bool synced = (base == Mod1 && p (Mod1Sync) > 0.5f) || (base == Mod2 && p (Mod2Sync) > 0.5f);
    auto t = [&] (double v) { return synced ? std::max (0.0005, 16.0 * std::pow (2.0, std::round (v * 12.0) - 12.0) * 60.0 / bpm) : envSeconds (v); };
    const double att = t (p (base)), dec = t (p (base + 1)), sus = p (base + 2), rel = t (p (base + 3));
    const double dt = 1.0 / sr;
    switch (e.stage)
    {
        case 0: return 0.0;
        case 1: e.level += dt / att; if (e.level >= 1.0) { e.level = 1.0; e.stage = 2; } break;
        case 2: e.level = sus + (e.level - sus) * std::exp (-5.0 * dt / dec); if (std::abs (e.level - sus) < 1e-4) { e.level = sus; e.stage = 3; } break;
        case 3: e.level = sus; break;
        case 4: e.level *= std::exp (-6.9 * dt / rel); if (e.level < 1e-5) { e.level = 0; e.stage = 0; } break;
    }
    return e.level;
}

void SylenthEngine::renderVoice (Voice& v, double* L, double* R, int n)
{
    // portamento: Sylenth's display reads 3000 ms * v^5 (93.75 ms at the centre)
    const double portaT = 3.0 * std::pow ((double) p (Porta), 5.0);
    const double bendSemis = bend * (listValue (p (BendRange), 24) + 1);
    const bool ampOverride = global && global->ampOverride;

    for (int i = 0; i < n; ++i)
    {
        if ((v.ctl++ & 31) == 0)
        {
            v.curNote = portaT > 0.0005 ? v.targetNote + (v.curNote - v.targetNote) * std::exp (-32.0 / (sr * portaT * 0.3)) : v.targetNote;
            v.mod.fill (0.0);
            auto add = [&] (int destParam, int amtParam, double src)
            {
                const int d = listValue (p (destParam), DNumDests);
                if (d > 0) v.mod[(size_t) d] += src * (p (amtParam) - 0.5) * 2.0;
            };
            for (int m = 0; m < 2; ++m)
            {
                const int b = m == 0 ? Mod1 : Mod2;
                add (b + 4, b + 5, v.env[(size_t) (2 + m)].level);
                add (b + 6, b + 7, v.env[(size_t) (2 + m)].level);
            }
            for (int l = 0; l < 2; ++l)
            {
                const int b = l == 0 ? Lfo1 : Lfo2;
                const int sync = listValue (p (b + 2), kNumSyncs);
                double hz = sync > 0 ? bpm / 60.0 / syncBeats (sync) : lfoHz (p (b + 1));
                hz *= std::pow (2.0, 4.0 * v.lfoRateMod[(size_t) l]);
                const bool free = p (l == 0 ? Lfo1Free : Lfo2Free) > 0.5f;
                double& ph = free ? freeLfoPhase[(size_t) l] : v.lfoPhase[(size_t) l];
                const double prev = ph;
                if (! free) { ph += hz * 32.0 / sr; ph -= std::floor (ph); }
                const int w = listValue (p (b), kNumLfoWaves);
                double val;
                switch (w)
                {
                    case 0: val = std::sin (kTwoPi * ph); break;
                    case 1: val = 1.0 - 2.0 * ph; break;                                  // Saw (falling)
                    case 2: val = 2.0 * ph - 1.0; break;                                  // Ramp (rising)
                    case 3: val = ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph; break;
                    case 4: val = ph < 0.5 ? 1 : -1; break;
                    case 5: val = ph < 0.25 ? 1 : -1; break;
                    case 6: val = ph < 0.125 ? 1 : -1; break;
                    case 7: val = ph < 0.5 ? 4 * ph - 1 : 1.0 - 2.0 * (ph - 0.5) * 2.0; break;
                    case 8: val = std::clamp (lorenz[0] / 20.0, -1.0, 1.0); break;      // chaotic
                    case 9: if (ph < prev) v.lfoSh[(size_t) l] = rng.bipolar(); val = v.lfoSh[(size_t) l]; break;
                    default:                                                              // smoothed random
                        if (ph < prev) v.lfoSh[(size_t) l] = rng.bipolar();
                        v.lfoVal[(size_t) l] += (v.lfoSh[(size_t) l] - v.lfoVal[(size_t) l]) * std::min (1.0, hz * 32.0 / sr * 4.0);
                        val = v.lfoVal[(size_t) l];
                        break;
                }
                if (w <= 9) v.lfoVal[(size_t) l] = val;
                const double gain = std::clamp (p (b + 3) + v.lfoGainMod[(size_t) l], 0.0, 1.0);
                val = val * gain + (p (b + 4) - 0.5) * 2.0;
                add (b + 5, b + 6, val);
                add (b + 7, b + 8, val);
            }
            // misc slots: velocity, mod wheel, key, envelopes, LFOs, aftertouch as sources
            for (int m = 0; m < 4; ++m)
            {
                const int b = Misc + m * 5;
                double src = 0;
                switch (listValue (p (b), SNumSources))
                {
                    case SVelocity: case SStepVelocity: src = v.vel; break;
                    case SModWheel: src = modWheel; break;
                    case SKeyTrack: src = (v.curNote - 60.0) / 60.0; break;
                    case SAmpEnvA: src = v.env[0].level; break;
                    case SAmpEnvB: src = v.env[1].level; break;
                    case SModEnv1: src = v.env[2].level; break;
                    case SModEnv2: src = v.env[3].level; break;
                    case SLfo1: src = v.lfoVal[0] * p (Lfo1 + 3); break;
                    case SLfo2: src = v.lfoVal[1] * p (Lfo2 + 3); break;
                    case SAftertouch: src = aftertouch; break;
                    default: continue;
                }
                add (b + 1, b + 2, src);
                add (b + 3, b + 4, src);
            }
            v.lfoRateMod = { v.mod[DLfo1Rate], v.mod[DLfo2Rate] };
            v.lfoGainMod = { v.mod[DLfo1Gain], v.mod[DLfo2Gain] };
            if (global)
            {
                v.gstage.control (*global, sr, 32);
                if (global->lockHz >= 1.0)
                    for (auto& a : v.lockX) for (auto& x : a) x.set (global->lockHz, sr);
                if (global->lockHz >= 1.0)
                    for (auto& a : v.lockC) for (auto& x : a) x.set (global->lockHz, sr);
            }

            // Filter Control is the master: its cutoff / resonance offset both filters (centre = no change)
            const double fctlCut = (p (FCtl) - 0.5), fctlRes = p (FCtl + 1), fctlKey = p (FCtl + 3);
            for (int f = 0; f < 2; ++f)
            {
                const int b = f == 0 ? FiltA : FiltB;
                double cut = p (b + 2) + fctlCut + v.mod[DCutoffAB] + (f == 0 ? v.mod[DCutoffA] : v.mod[DCutoffB])
                           + (p (b + 5) + fctlKey) * (v.curNote - 60) / 120.0;
                const double res = std::clamp (p (b + 3) + fctlRes + v.mod[DResoAB] + (f == 0 ? v.mod[DResoA] : v.mod[DResoB]), 0.0, 1.0);
                const double hz = std::min (cutoffHz (cut), sr * 0.45);
                const double damp = 2.0 * std::pow (1.0 - res, 1.6) + 0.02;
                for (int s = 0; s < 2; ++s)
                    for (int c = 0; c < 2; ++c)
                        (f == 0 ? v.fA[s][c] : v.fB[s][c]).set (hz, s == 0 ? damp : 1.4, sr);
            }
        }

        const double ampA = envTick (v.env[0], AmpA), ampB = envTick (v.env[1], AmpB);
        envTick (v.env[2], Mod1);
        envTick (v.env[3], Mod2);
        const double gAmp = v.gstage.tickEnvelopes();

        double partL[2] {}, partR[2] {}, lockL[2] {}, lockR[2] {};
        const bool lock = v.gstage.lockOn();
        for (int o = 0; o < 4; ++o)
        {
            const int b = OscBase + o * OscStride;
            const int count = listValue (p (b + oVoices), 9);
            if (count == 0 || (global && ! global->oscOn[(size_t) o])) continue;
            const int part = o / 2;
            auto& osc = v.osc[(size_t) o];
            const int wave = listValue (p (b + oWave), 8);
            const double detune = p (b + oDetune) * p (b + oDetune) * 0.8;   // semitones of spread
            const double stereo = p (b + oStereo);
            const double baseNote = (p (b + oKeytrack) > 0.5f ? v.curNote : 60.0) + 12.0 * (listValue (p (b + oOctave), 7) - 3)
                                  + (listValue (p (b + oNote), 15) - 7) + (p (b + oFine) - 0.5) * 2.0 + bendSemis
                                  + (part == 0 ? v.mod[DPitchA] : v.mod[DPitchB]) * 12.0 + v.mod[DPitchAB] * 12.0;
            const double vol = std::clamp (p (b + oVolume) + v.mod[DVolumeAB] + (part == 0 ? v.mod[DVolumeA] : v.mod[DVolumeB]), 0.0, 1.0);
            const double pan = std::clamp ((p (b + oPan) - 0.5) * 2.0 + v.mod[DPanAB] + (part == 0 ? v.mod[DPanA] : v.mod[DPanB]), -1.0, 1.0);
            const double inv = p (b + oInvert) > 0.5f ? -1.0 : 1.0;
            double sl = 0, srr = 0, cl = 0, cr = 0;
            const int centre = (count - 1) / 2;
            for (int u = 0; u < count; ++u)
            {
                const double pos = count > 1 ? (double) u / (count - 1) * 2.0 - 1.0 : 0.0;
                const double hz = dsp::midiToHz (baseNote + pos * detune);
                const double dt = std::min (hz / sr, 0.49);
                double& ph = osc.phase[(size_t) u];
                double s;
                auto pulse = [&] (double duty)
                {
                    double p2 = ph + (1.0 - duty); p2 -= std::floor (p2);
                    return (ph < duty ? 1.0 : -1.0) + dsp::polyBlep (ph, dt) - dsp::polyBlep (p2, dt) - (2.0 * duty - 1.0);
                };
                switch (wave)
                {
                    case 0: s = std::sin (kTwoPi * ph); break;
                    case 1: s = (2.0 * ph - 1.0) - dsp::polyBlep (ph, dt); break;
                    case 2: s = ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph; break;
                    case 3: s = pulse (0.5); break;
                    case 4: s = pulse (0.25); break;
                    case 5: s = pulse (0.125); break;
                    case 6: s = 0.5 * ((ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph) + (2.0 * ph - 1.0) - dsp::polyBlep (ph, dt)); break;
                    default: s = osc.rng.bipolar(); break;
                }
                ph += dt;
                ph -= std::floor (ph);
                const double up = std::clamp (pan + pos * stereo, -1.0, 1.0);
                sl += s * std::cos ((up + 1.0) * kPi * 0.25);
                srr += s * std::sin ((up + 1.0) * kPi * 0.25);
                if (u == centre) { cl = s * std::cos ((pan + 1.0) * kPi * 0.25); cr = s * std::sin ((pan + 1.0) * kPi * 0.25); }
            }
            const double g = inv * vol / std::sqrt ((double) count);
            partL[part] += sl * g;
            partR[part] += srr * g;
            lockL[part] += cl * inv * vol;
            lockR[part] += cr * inv * vol;
        }

        // Phase Lock: below the crossover each part plays only its oscillators' centre voice (no beating)
        if (lock)
            for (int pt = 0; pt < 2; ++pt)
            {
                partL[pt] = v.lockX[pt][0].high (partL[pt]) + v.lockC[pt][0].low (lockL[pt]);
                partR[pt] = v.lockX[pt][1].high (partR[pt]) + v.lockC[pt][1].low (lockR[pt]);
            }

        // filters A and B (one per part), 12 or 24 dB, drive
        auto runFilter = [&] (int f, double x, int c)
        {
            const int b = f == 0 ? FiltA : FiltB;
            const int type = listValue (p (b), 4);
            if (type == 0) return x;
            const double drive = 1.0 + p (b + 4) * 8.0 + (p (FCtl + 2) > 0.5f ? 1.5 : 0.0);
            if (drive > 1.01) x = std::tanh (x * drive) / std::sqrt (drive);
            auto& s1 = f == 0 ? v.fA[0][c] : v.fB[0][c];
            auto& s2 = f == 0 ? v.fA[1][c] : v.fB[1][c];
            auto pick = [type] (dsp::Svf::Out o) { return type == 1 ? o.lp : type == 2 ? o.bp : o.hp; };
            double y = pick (s1.tick (x));
            if (listValue (p (b + 1), 2) == 1) y = pick (s2.tick (y));
            return y;
        };
        const int solo = listValue (p (Solo), 3);
        // modulation adds to the knob, in knob units (amount 10 = the knob's full range)
        const double mixA = solo == 2 ? 0.0 : std::clamp (p (MixA) + v.mod[DMixA] + v.mod[DMixAB], 0.0, 1.0);
        const double mixB = solo == 1 ? 0.0 : std::clamp (p (MixB) + v.mod[DMixB] + v.mod[DMixAB], 0.0, 1.0);
        const double envA = ampOverride ? gAmp : ampA, envB = ampOverride ? gAmp : ampB;
        double l = runFilter (0, partL[0], 0) * mixA * envA + runFilter (1, partL[1], 0) * mixB * envB;
        double r = runFilter (0, partR[0], 1) * mixA * envA + runFilter (1, partR[1], 1) * mixB * envB;

        v.gstage.filter (l, r);
        const double out = p (MainVol) * 0.5 * (0.3 + 0.7 * v.vel);
        L[i] += l * out;
        R[i] += r * out;

        const bool done = ampOverride ? ! v.gstage.ampActive() : (v.env[0].stage == 0 && v.env[1].stage == 0);
        if (done && ! v.gate) { v.active = false; return; }
    }
}

void SylenthEngine::process (float* L, float* R, int n)
{
    if ((int) bufL.size() < n) { bufL.resize ((size_t) n); bufR.resize ((size_t) n); }
    std::fill (bufL.begin(), bufL.begin() + n, 0.0);
    std::fill (bufR.begin(), bufR.begin() + n, 0.0);
    // free-running LFOs and the Lorenz source advance once per block for every voice
    for (int l = 0; l < 2; ++l)
    {
        const int b = l == 0 ? Lfo1 : Lfo2;
        const int sync = listValue (p (b + 2), kNumSyncs);
        const double hz = sync > 0 ? bpm / 60.0 / syncBeats (sync) : lfoHz (p (b + 1));
        freeLfoPhase[(size_t) l] += hz * n / sr;
        freeLfoPhase[(size_t) l] -= std::floor (freeLfoPhase[(size_t) l]);
    }
    {
        const double dt = std::min (0.01, 2.0 * n / sr);
        auto& z = lorenz;
        const double dx = 10.0 * (z[1] - z[0]), dy = z[0] * (28.0 - z[2]) - z[1], dz = z[0] * z[1] - 8.0 / 3.0 * z[2];
        z[0] += dx * dt; z[1] += dy * dt; z[2] += dz * dt;
    }
    for (auto& v : voices) if (v.active) renderVoice (v, bufL.data(), bufR.data(), n);
    for (int i = 0; i < n; ++i) { L[i] = (float) bufL[(size_t) i]; R[i] = (float) bufR[(size_t) i]; }
}

void SylenthEngine::extractEnvelopes (const std::array<float, kNumParams>& a, AdsrTimes& amp, AdsrTimes& filter)
{
    auto pv = [&] (int i) { return (double) a[(size_t) i]; };
    amp = { envSeconds (pv (AmpA)), envSeconds (pv (AmpA + 1)), pv (AmpA + 2), envSeconds (pv (AmpA + 3)) };
    filter = { envSeconds (pv (Mod1)), envSeconds (pv (Mod1 + 1)), pv (Mod1 + 2), envSeconds (pv (Mod1 + 3)) };
}

//==============================================================================
// Sylenth1 presets: 244 values in Sylenth's VST parameter order. The order, the lists and the display
// laws below were read from the user's own Sylenth1 through the plain VST2 API (tools/vc_probe): names,
// list entries and value->text curves. Nothing was unpacked or disassembled.
namespace
{
    namespace sy
    {
        enum
        {
            AmpA = 0, AmpB = 4,                      // Attack Decay Release Sustain
            ArpGate = 8, ArpMode, ArpOctave, ArpTime, ArpVelo, ArpWrap,
            FilterA = 42,                            // Cutoff Drive Input Reso Type dB
            FilterB = 48,
            FCtlCutoff = 54, FCtlKey, FCtlReso, FCtlWarm,
            Lfo1 = 58, Lfo2 = 63,                    // Free Gain Offset Rate Wave
            MainVolume = 68, MixA, MixB,
            ModEnv1 = 71, ModEnv2 = 75,              // Attack Decay Release Sustain
            MonoLegato = 80,
            OscA1 = 81, OscStride = 12,              // Detune Fine Invert Note Octave Pan Phase Retrigger Stereo Voices Volume Waveform
            BendRange = 138, Polyphony, PortaMode, PortaTime,
            Solo = 147, Sync,
            StepHold = 149, StepTranspose = 165, StepVelocity = 181,
            ModEnv1Amt = 197, ModEnv2Amt = 199, Lfo1Amt = 201, Lfo2Amt = 203,
            Misc1AAmt = 205, Misc1ASource = 207, Misc1BAmt = 208, Misc1BSource = 210,
            Misc2AAmt = 211, Misc2ASource = 213, Misc2BAmt = 214, Misc2BSource = 216,
            ArpOn = 217,
            ModEnv1Dest = 225, ModEnv2Dest = 227, Lfo1Dest = 229, Lfo2Dest = 231,
            Misc1ADest = 233, Misc1BDest = 235, Misc2ADest = 237, Misc2BDest = 239,
            PartSelect = 241, ArpSwing = 243,
            NumValues = 244
        };

        // the tempo-synced LFO rate display at v = k/32 (k = 0..32), as quarter-note beats
        constexpr double kLfoRateBeats[33] = {
            48, 32, 24, 64.0 / 3, 16, 12, 8, 6, 16.0 / 3, 4, 3, 8.0 / 3, 2, 1.5, 4.0 / 3, 1, 2.0 / 3, 0.5, 0.375, 1.0 / 3,
            0.25, 0.1875, 1.0 / 6, 0.125, 0.09375, 1.0 / 12, 0.0625, 1.0 / 24, 1.0 / 32, 0.0234375, 1.0 / 48, 1.0 / 64, 1.0 / 96 };
        // the arp time display at v = k/32
        constexpr const char* kArpTime[33] = {
            "1/1", "1/1", "1/2D", "1/1T", "1/1T", "1/2", "1/2", "1/4D", "1/4D", "1/2T", "1/2T", "1/4", "1/8D", "1/8D", "1/4T", "1/4T", "1/8",
            "1/8", "1/16D", "1/8T", "1/8T", "1/16", "1/16", "1/32D", "1/32D", "1/16T", "1/32", "1/32", "1/64D", "1/64D", "1/32T", "1/32T", "1/64" };
    }

    int idx (float v, int count) { return std::clamp ((int) std::lround (v * (count - 1)), 0, count - 1); }
    float list (int index, int count) { return count > 1 ? (float) index / (float) (count - 1) : 0.0f; }

    int syncIndexFromRate (float v)
    {
        // log-interpolate the displayed period, then snap to the nearest division
        const double k = std::clamp ((double) v, 0.0, 1.0) * 32.0;
        const int k0 = std::min (31, (int) k);
        const double t = k - k0;
        const double beats = std::exp (std::log (sy::kLfoRateBeats[k0]) * (1 - t) + std::log (sy::kLfoRateBeats[k0 + 1]) * t);
        int best = 1;
        for (int i = 1; i < kNumSyncs; ++i)
            if (std::abs (std::log (syncBeats (i) / beats)) < std::abs (std::log (syncBeats (best) / beats))) best = i;
        return best;
    }
}

std::array<float, SylenthEngine::kNumParams> SylenthEngine::fromSylenthPreset (const std::vector<float>& rawIn, PatchSteps* steps)
{
    std::vector<float> raw (rawIn);
    raw.resize (sy::NumValues, 0.0f);
    auto r = [&] (int i) { return std::clamp (raw[(size_t) i], 0.0f, 1.0f); };
    auto a = initParams();
    auto set = [&] (int i, float v) { a[(size_t) i] = std::clamp (v, 0.0f, 1.0f); };

    // oscillators A1 A2 B1 B2: same lists and ranges as this engine
    for (int o = 0; o < 4; ++o)
    {
        const int s = sy::OscA1 + o * sy::OscStride, b = OscBase + o * OscStride;
        set (b + oDetune, r (s + 0));
        set (b + oFine, r (s + 1));
        set (b + oInvert, r (s + 2) > 0.5f ? 1.0f : 0.0f);
        set (b + oNote, list (idx (r (s + 3), 15), 15));
        set (b + oOctave, list (idx (r (s + 4), 7), 7));
        set (b + oPan, r (s + 5));
        set (b + oPhase, r (s + 6));
        set (b + oRetrig, r (s + 7) > 0.5f ? 1.0f : 0.0f);
        set (b + oStereo, r (s + 8));
        set (b + oVoices, list (idx (r (s + 9), 9), 9));
        set (b + oVolume, r (s + 10));
        set (b + oWave, list (idx (r (s + 11), 8), 8));
        set (b + oKeytrack, 1.0f);
    }
    // filters: Sylenth's dB switch is 24 (0) / 12 (1); this engine's slope list is 12 dB, 24 dB
    for (int f = 0; f < 2; ++f)
    {
        const int s = f == 0 ? sy::FilterA : sy::FilterB, b = f == 0 ? FiltA : FiltB;
        set (b + 0, list (idx (r (s + 4), 4), 4));
        set (b + 1, r (s + 5) > 0.5f ? 0.0f : 1.0f);
        set (b + 2, r (s + 0));
        set (b + 3, r (s + 3));
        set (b + 4, r (s + 1));
        set (b + 5, 0.0f);
        set (f == 0 ? FiltAInput : FiltBInput, list (idx (r (s + 2), 3), 3));
    }
    set (FCtl + 0, r (sy::FCtlCutoff));
    set (FCtl + 1, r (sy::FCtlReso));
    set (FCtl + 2, r (sy::FCtlWarm) > 0.5f ? 1.0f : 0.0f);
    set (FCtl + 3, r (sy::FCtlKey));

    // envelopes: Sylenth stores Attack Decay Release Sustain
    auto env = [&] (int s, int b) { set (b + 0, r (s + 0)); set (b + 1, r (s + 1)); set (b + 2, r (s + 3)); set (b + 3, r (s + 2)); };
    env (sy::AmpA, AmpA);
    env (sy::AmpB, AmpB);
    env (sy::ModEnv1, Mod1);
    env (sy::ModEnv2, Mod2);

    // modulation destinations: Sylenth's list has 28 entries on a 1/32 grid
    auto dest = [&] (int s) { return list (std::clamp ((int) std::lround (r (s) * 32.0f), 0, DNumDests - 1), DNumDests); };
    set (Mod1 + 4, dest (sy::ModEnv1Dest));     set (Mod1 + 5, r (sy::ModEnv1Amt));
    set (Mod1 + 6, dest (sy::ModEnv1Dest + 1)); set (Mod1 + 7, r (sy::ModEnv1Amt + 1));
    set (Mod2 + 4, dest (sy::ModEnv2Dest));     set (Mod2 + 5, r (sy::ModEnv2Amt));
    set (Mod2 + 6, dest (sy::ModEnv2Dest + 1)); set (Mod2 + 7, r (sy::ModEnv2Amt + 1));

    // LFOs: with Sylenth's Sync on the rate is a tempo division
    const bool hostSync = r (sy::Sync) > 0.5f;
    for (int l = 0; l < 2; ++l)
    {
        const int s = l == 0 ? sy::Lfo1 : sy::Lfo2, b = l == 0 ? Lfo1 : Lfo2;
        const int ds = l == 0 ? sy::Lfo1Dest : sy::Lfo2Dest, as = l == 0 ? sy::Lfo1Amt : sy::Lfo2Amt;
        set (l == 0 ? Lfo1Free : Lfo2Free, r (s + 0) > 0.5f ? 1.0f : 0.0f);
        set (b + 3, r (s + 1));
        set (b + 4, r (s + 2));
        set (b + 1, r (s + 3));
        set (b + 2, hostSync ? list (syncIndexFromRate (r (s + 3)), kNumSyncs) : 0.0f);
        set (b + 0, list (idx (r (s + 4), kNumLfoWaves), kNumLfoWaves));
        set (b + 5, dest (ds));     set (b + 6, r (as));
        set (b + 7, dest (ds + 1)); set (b + 8, r (as + 1));
    }
    // misc slots 1A 1B 2A 2B
    const int miscSrc[4] = { sy::Misc1ASource, sy::Misc1BSource, sy::Misc2ASource, sy::Misc2BSource };
    const int miscAmt[4] = { sy::Misc1AAmt, sy::Misc1BAmt, sy::Misc2AAmt, sy::Misc2BAmt };
    const int miscDst[4] = { sy::Misc1ADest, sy::Misc1BDest, sy::Misc2ADest, sy::Misc2BDest };
    for (int m = 0; m < 4; ++m)
    {
        const int b = Misc + m * 5;
        set (b + 0, list (idx (r (miscSrc[m]), SNumSources), SNumSources));
        set (b + 1, dest (miscDst[m]));     set (b + 2, r (miscAmt[m]));
        set (b + 3, dest (miscDst[m] + 1)); set (b + 4, r (miscAmt[m] + 1));
    }

    set (MixA, r (sy::MixA));
    set (MixB, r (sy::MixB));
    set (MainVol, r (sy::MainVolume));
    set (Solo, r (sy::Solo) > 0.5f ? list (1 + idx (r (sy::PartSelect), 2), 3) : 0.0f);
    const int poly = idx (r (sy::Polyphony), 17);            // 0..16
    const bool mono = r (sy::MonoLegato) > 0.5f || poly <= 1;
    set (Mono, mono ? 1.0f : 0.0f);
    set (Legato, r (sy::MonoLegato) > 0.5f ? 1.0f : 0.0f);
    set (Polyphony, list (std::clamp (poly, 1, 16) - 1, 16));
    set (Porta, r (sy::PortaTime));
    set (PortaMode, idx (r (sy::PortaMode), 2) == 0 ? 1.0f : 0.0f);   // Sylenth: Slide (0) / Normal (1)
    set (BendRange, list (idx (r (sy::BendRange), 24), 24));

    if (steps)
    {
        auto& st = *steps;
        st = {};
        st.arpOn = r (sy::ArpOn) > 0.5f;
        st.present = true;          // every Sylenth preset carries its 16 arp steps: always load them
        st.mode = idx (r (sy::ArpMode), 10);
        st.octaves = 1 + idx (r (sy::ArpOctave), 4);
        const std::string t = sy::kArpTime[std::clamp ((int) std::lround (r (sy::ArpTime) * 32.0f), 0, 32)];
        const int denom = std::atoi (t.c_str() + 2);
        const int rates[5] = { 2, 4, 8, 16, 32 };
        st.rateIndex = 0;
        for (int k = 0; k < 5; ++k) if (std::abs (rates[k] - denom) < std::abs (rates[st.rateIndex] - denom)) st.rateIndex = k;
        st.rateType = t.back() == 'T' ? 1 : t.back() == 'D' ? 2 : 0;
        st.gate = std::max (0.01, (double) r (sy::ArpGate));
        st.swing = r (sy::ArpSwing);
        st.velocityMode = idx (r (sy::ArpVelo), 5);
        const int wrap = idx (r (sy::ArpWrap), 17);
        st.length = wrap == 0 ? 16 : wrap;
        for (int k = 0; k < 16; ++k)
        {
            PatchSteps::Step s;
            s.transpose = idx (r (sy::StepTranspose + k), 49) - 24;
            s.velocity = (int) std::lround (r (sy::StepVelocity + k) * 127.0f);
            s.tie = r (sy::StepHold + k) > 0.5f;
            st.steps.push_back (s);
        }
    }
    return a;
}

std::vector<float> SylenthEngine::toSylenthPreset (const std::array<float, kNumParams>& a, const std::vector<float>& base)
{
    // the inverse of fromSylenthPreset: starts from the imported values (or Sylenth's defaults) so
    // everything this engine doesn't model (effects, arp settings ...) keeps its value
    std::vector<float> raw (base.size() == sy::NumValues ? base : std::vector<float> (std::begin (kSylenthDefaults), std::end (kSylenthDefaults)));
    auto pv = [&] (int i) { return std::clamp (a[(size_t) i], 0.0f, 1.0f); };
    auto set = [&] (int i, float v) { raw[(size_t) i] = std::clamp (v, 0.0f, 1.0f); };
    auto relist = [] (float v, int from, int to) { return list (std::clamp ((int) std::lround (v * (from - 1)), 0, to - 1), to); };
    for (int o = 0; o < 4; ++o)
    {
        const int s = sy::OscA1 + o * sy::OscStride, b = OscBase + o * OscStride;
        set (s + 0, pv (b + oDetune));  set (s + 1, pv (b + oFine));    set (s + 2, pv (b + oInvert));
        set (s + 3, relist (pv (b + oNote), 15, 15));  set (s + 4, relist (pv (b + oOctave), 7, 7));
        set (s + 5, pv (b + oPan));     set (s + 6, pv (b + oPhase));   set (s + 7, pv (b + oRetrig));
        set (s + 8, pv (b + oStereo));  set (s + 9, relist (pv (b + oVoices), 9, 9));
        set (s + 10, pv (b + oVolume)); set (s + 11, relist (pv (b + oWave), 8, 8));
    }
    for (int f = 0; f < 2; ++f)
    {
        const int s = f == 0 ? sy::FilterA : sy::FilterB, b = f == 0 ? FiltA : FiltB;
        set (s + 0, pv (b + 2)); set (s + 1, pv (b + 4)); set (s + 2, relist (pv (f == 0 ? FiltAInput : FiltBInput), 3, 3));
        set (s + 3, pv (b + 3)); set (s + 4, relist (pv (b), 4, 4)); set (s + 5, pv (b + 1) > 0.5f ? 0.0f : 1.0f);
    }
    set (sy::FCtlCutoff, pv (FCtl)); set (sy::FCtlReso, pv (FCtl + 1)); set (sy::FCtlWarm, pv (FCtl + 2)); set (sy::FCtlKey, pv (FCtl + 3));
    auto env = [&] (int s, int b) { set (s + 0, pv (b)); set (s + 1, pv (b + 1)); set (s + 3, pv (b + 2)); set (s + 2, pv (b + 3)); };
    env (sy::AmpA, AmpA); env (sy::AmpB, AmpB); env (sy::ModEnv1, Mod1); env (sy::ModEnv2, Mod2);
    auto dest = [&] (int i) { return (float) std::clamp ((int) std::lround (pv (i) * (DNumDests - 1)), 0, DNumDests - 1) / 32.0f; };
    set (sy::ModEnv1Dest, dest (Mod1 + 4)); set (sy::ModEnv1Amt, pv (Mod1 + 5)); set (sy::ModEnv1Dest + 1, dest (Mod1 + 6)); set (sy::ModEnv1Amt + 1, pv (Mod1 + 7));
    set (sy::ModEnv2Dest, dest (Mod2 + 4)); set (sy::ModEnv2Amt, pv (Mod2 + 5)); set (sy::ModEnv2Dest + 1, dest (Mod2 + 6)); set (sy::ModEnv2Amt + 1, pv (Mod2 + 7));
    for (int l = 0; l < 2; ++l)
    {
        const int s = l == 0 ? sy::Lfo1 : sy::Lfo2, b = l == 0 ? Lfo1 : Lfo2;
        const int ds = l == 0 ? sy::Lfo1Dest : sy::Lfo2Dest, as = l == 0 ? sy::Lfo1Amt : sy::Lfo2Amt;
        set (s + 0, pv (l == 0 ? Lfo1Free : Lfo2Free)); set (s + 1, pv (b + 3)); set (s + 2, pv (b + 4)); set (s + 3, pv (b + 1));
        // a tempo division: the rate knob position that shows it (Sylenth's Sync on)
        const int sync = listValue (pv (b + 2), kNumSyncs);
        if (sync > 0)
        {
            int best = 0;
            for (int k = 1; k < 33; ++k)
                if (std::abs (std::log (sy::kLfoRateBeats[k] / syncBeats (sync))) < std::abs (std::log (sy::kLfoRateBeats[best] / syncBeats (sync)))) best = k;
            set (s + 3, (float) best / 32.0f);
        }
        set (s + 4, relist (pv (b), kNumLfoWaves, kNumLfoWaves));
        set (ds, dest (b + 5)); set (as, pv (b + 6)); set (ds + 1, dest (b + 7)); set (as + 1, pv (b + 8));
    }
    const int miscSrc[4] = { sy::Misc1ASource, sy::Misc1BSource, sy::Misc2ASource, sy::Misc2BSource };
    const int miscAmt[4] = { sy::Misc1AAmt, sy::Misc1BAmt, sy::Misc2AAmt, sy::Misc2BAmt };
    const int miscDst[4] = { sy::Misc1ADest, sy::Misc1BDest, sy::Misc2ADest, sy::Misc2BDest };
    for (int m = 0; m < 4; ++m)
    {
        const int b = Misc + m * 5;
        set (miscSrc[m], relist (pv (b), SNumSources, SNumSources));
        set (miscDst[m], dest (b + 1)); set (miscAmt[m], pv (b + 2)); set (miscDst[m] + 1, dest (b + 3)); set (miscAmt[m] + 1, pv (b + 4));
    }
    set (sy::Sync, listValue (pv (Lfo1 + 2), kNumSyncs) > 0 || listValue (pv (Lfo2 + 2), kNumSyncs) > 0 ? 1.0f : 0.0f);
    set (sy::MixA, pv (MixA)); set (sy::MixB, pv (MixB)); set (sy::MainVolume, pv (MainVol));
    set (sy::MonoLegato, pv (Legato) > 0.5f || pv (Mono) > 0.5f ? 1.0f : 0.0f);
    set (sy::Polyphony, list (listValue (pv (Polyphony), 16) + 1, 17));
    set (sy::PortaTime, pv (Porta));
    set (sy::PortaMode, listValue (pv (PortaMode), 2) == 1 ? 0.0f : 1.0f);
    set (sy::BendRange, pv (BendRange));
    const int solo = listValue (pv (Solo), 3);
    set (sy::Solo, solo > 0 ? 1.0f : 0.0f);
    if (solo > 0) set (sy::PartSelect, solo == 2 ? 1.0f : 0.0f);
    return raw;
}
} // namespace vc
