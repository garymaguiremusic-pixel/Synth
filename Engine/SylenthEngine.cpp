#include "SylenthEngine.h"
#include "Tables/Tables.h"
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
    static const char* kDests[] = { "None", "Cutoff A", "Cutoff B", "Cutoff AB", "Reso AB", "Pitch A", "Pitch B", "Pitch AB",
                                    "Volume A", "Volume B", "Volume AB", "Pan AB", "Mix AB", "Detune AB", "Phase AB" };
    static const char* kLfoWaves[] = { "Sine", "Saw", "Triangle", "Pulse", "HPulse", "QPulse", "TriSaw", "Noise" };
    static const char* kSync[] = { "Off", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1", "2/1", "1/8T", "1/16T", "1/4T", "1/8D", "1/16D", "1/4D" };
    static const char* kPortaMode[] = { "Normal", "Slide" };
    static const char* kPoly[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    static constexpr double kSyncBeats[] = { 0, 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8, 1.0 / 3, 1.0 / 6, 2.0 / 3, 0.75, 0.375, 1.5 };

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
            set (FCtl + 0, "syl_fctl_cutoff", "Filter Ctl Cutoff", 1.0f);
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
                set (b + 0, pre + "wave", nm + "Wave", 0.0f, 8, kLfoWaves);
                set (b + 1, pre + "rate", nm + "Rate", 0.5f);
                set (b + 2, pre + "sync", nm + "Sync", 0.0f, 15, kSync);
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
            set (PortaMode, "syl_portamode", "Porta Mode", 0.0f, 2, kPortaMode);
            set (Polyphony, "syl_poly", "Polyphony", 1.0f, 16, kPoly);
            set (BendRange, "syl_bend", "Bend Range", 2.0f / 24.0f);
            set (MainVol, "syl_volume", "Main Volume", 0.7f);
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
    if (! wasActive || p (Porta) <= 0.0f) v.curNote = note;
    if (legato) return;

    for (int o = 0; o < 4; ++o)
    {
        const int b = OscBase + o * OscStride;
        auto& osc = v.osc[(size_t) o];
        if (! wasActive) osc.rng.seed (rng.next());
        const bool retrig = p (b + oRetrig) > 0.5f;
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
    const double att = envSeconds (p (base)), dec = envSeconds (p (base + 1)), sus = p (base + 2), rel = envSeconds (p (base + 3));
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
    const double portaT = 2.0 * p (Porta) * p (Porta);
    const double bendSemis = bend * std::round (p (BendRange) * 24.0);
    const bool ampOverride = global && global->ampOverride;

    for (int i = 0; i < n; ++i)
    {
        if ((v.ctl++ & 31) == 0)
        {
            v.curNote = portaT > 0 ? v.targetNote + (v.curNote - v.targetNote) * std::exp (-32.0 / (sr * portaT * 0.3)) : v.targetNote;
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
                const int sync = listValue (p (b + 2), 15);
                const double hz = sync > 0 ? bpm / 60.0 / kSyncBeats[sync] : lfoHz (p (b + 1));
                double& ph = v.lfoPhase[(size_t) l];
                const double prev = ph;
                ph += hz * 32.0 / sr;
                ph -= std::floor (ph);
                const int w = listValue (p (b), 8);
                double val;
                switch (w)
                {
                    case 0: val = std::sin (kTwoPi * ph); break;
                    case 1: val = 1.0 - 2.0 * ph; break;
                    case 2: val = ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph; break;
                    case 3: val = ph < 0.5 ? 1 : -1; break;
                    case 4: val = ph < 0.25 ? 1 : -1; break;
                    case 5: val = ph < 0.125 ? 1 : -1; break;
                    case 6: val = ph < 0.5 ? 4 * ph - 1 : 1.0 - 2.0 * (ph - 0.5) * 2.0; break;
                    default: if (ph < prev) v.lfoSh[(size_t) l] = rng.bipolar(); val = v.lfoSh[(size_t) l]; break;
                }
                val = val * p (b + 3) + (p (b + 4) - 0.5) * 2.0;
                v.lfoVal[(size_t) l] = val;
                add (b + 5, b + 6, val);
                add (b + 7, b + 8, val);
            }
            if (global) v.gstage.control (*global, sr, 32);

            const double fctlCut = (p (FCtl) - 1.0), fctlRes = p (FCtl + 1), fctlKey = p (FCtl + 3);
            for (int f = 0; f < 2; ++f)
            {
                const int b = f == 0 ? FiltA : FiltB;
                double cut = p (b + 2) + fctlCut + v.mod[DCutoffAB] * 0.5 + (f == 0 ? v.mod[DCutoffA] : v.mod[DCutoffB]) * 0.5
                           + (p (b + 5) + fctlKey) * (v.curNote - 60) / 120.0;
                const double res = std::clamp (p (b + 3) + fctlRes + v.mod[DResoAB] * 0.5, 0.0, 1.0);
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

        double partL[2] {}, partR[2] {};
        for (int o = 0; o < 4; ++o)
        {
            const int b = OscBase + o * OscStride;
            const int count = listValue (p (b + oVoices), 9);
            if (count == 0) continue;
            const int part = o / 2;
            auto& osc = v.osc[(size_t) o];
            const int wave = listValue (p (b + oWave), 8);
            const double detune = p (b + oDetune) * p (b + oDetune) * 0.8 + v.mod[DDetuneAB] * 0.2;   // semitones of spread
            const double stereo = p (b + oStereo);
            const double baseNote = (p (b + oKeytrack) > 0.5f ? v.curNote : 60.0) + 12.0 * (listValue (p (b + oOctave), 7) - 3)
                                  + (listValue (p (b + oNote), 15) - 7) + (p (b + oFine) - 0.5) * 2.0 + bendSemis
                                  + (part == 0 ? v.mod[DPitchA] : v.mod[DPitchB]) * 12.0 + v.mod[DPitchAB] * 12.0;
            const double vol = p (b + oVolume) * (1.0 + v.mod[DVolumeAB] + (part == 0 ? v.mod[DVolumeA] : v.mod[DVolumeB]));
            const double pan = std::clamp ((p (b + oPan) - 0.5) * 2.0 + v.mod[DPanAB], -1.0, 1.0);
            const double inv = p (b + oInvert) > 0.5f ? -1.0 : 1.0;
            double sl = 0, srr = 0;
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
            }
            const double g = inv * vol / std::sqrt ((double) count);
            partL[part] += sl * g;
            partR[part] += srr * g;
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
        const double mixA = solo == 2 ? 0.0 : p (MixA) * (1.0 - std::max (0.0, v.mod[DMixAB]));
        const double mixB = solo == 1 ? 0.0 : p (MixB) * (1.0 + std::min (0.0, v.mod[DMixAB]));
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
    for (auto& v : voices) if (v.active) renderVoice (v, bufL.data(), bufR.data(), n);
    for (int i = 0; i < n; ++i) { L[i] = (float) bufL[(size_t) i]; R[i] = (float) bufR[(size_t) i]; }
}

void SylenthEngine::extractEnvelopes (const std::array<float, kNumParams>& a, AdsrTimes& amp, AdsrTimes& filter)
{
    auto pv = [&] (int i) { return (double) a[(size_t) i]; };
    amp = { envSeconds (pv (AmpA)), envSeconds (pv (AmpA + 1)), pv (AmpA + 2), envSeconds (pv (AmpA + 3)) };
    filter = { envSeconds (pv (Mod1)), envSeconds (pv (Mod1 + 1)), pv (Mod1 + 2), envSeconds (pv (Mod1 + 3)) };
}
} // namespace vc
