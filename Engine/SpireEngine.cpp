#include "SpireEngine.h"
#include "Tables/Waves.h"
#include "Tables/Tables.h"

namespace vc
{
using namespace spire::idx;
using dsp::kPi;
using dsp::kTwoPi;

namespace
{
    constexpr int kOscStride = 19;       // type, oct, note, fine, ctrlA, ctrlB, phase, wave, wtmix, udet, uwid, ucnt, uoct, uden, pan, ana, inv, key, mixto
    constexpr int kEnvStride = 16, kLfoStride = 16;

    // Spire's sync divisions, longest first (index 16 = 1/16, the arp's default)
    constexpr double kSyncBeats[21] = { 64, 32, 16, 8, 4, 3, 8.0 / 3, 2, 1.5, 4.0 / 3, 1, 0.75, 2.0 / 3, 0.5, 0.375,
                                        1.0 / 3, 0.25, 0.1875, 1.0 / 6, 0.125, 0.0625 };

    // unison chord intervals (uoct list): 1..3 octaves, maj3, min3, maj7, dom7, min-maj7, min7, min9, maj9, dom9,
    // half-dim7, dim7, sus2, sus4, 5th, 4th (semitones of the chord notes the unison voices spread over)
    const std::vector<std::vector<int>> kChords = {
        { 0, 12 }, { 0, 12, 24 }, { 0, 12, 24, 36 }, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 11 }, { 0, 4, 7, 10 },
        { 0, 3, 7, 11 }, { 0, 3, 7, 10 }, { 0, 3, 7, 10, 14 }, { 0, 4, 7, 11, 14 }, { 0, 4, 7, 10, 14 },
        { 0, 3, 6, 10 }, { 0, 3, 6, 9 }, { 0, 2, 7 }, { 0, 5, 7 }, { 0, 7 }, { 0, 5 } };

    // GUESSED physical laws (to calibrate against Spire)
    double envSeconds (double v) { return 10.0 * v * v; }
    double cutoffHz (double v) { return 20.0 * std::pow (1000.0, std::clamp (v, 0.0, 1.0)); }
    double lfoHz (double v) { return 0.02 * std::pow (2500.0, std::clamp (v, 0.0, 1.0)); }
    double glideSeconds (double v) { return 2.0 * v * v; }

    // Mod targets: Spire's target IDs; only the ones confirmed so far are decoded.
    // The full list comes from the parameter probe (tools/vc_probe) - see README.
    enum Target { TNone, TCutoff1, TCutoff2, TReso1, TPitchAll, TVolume };
    Target decodeTarget (int id)
    {
        switch (id)
        {
            case 45: return TCutoff1;     // "Cutoff_1:45" (confirmed in the analysis)
            default: return TNone;
        }
    }

    double polyBlep (double t, double dt) { return dsp::polyBlep (t, dt); }
}

//==============================================================================
std::array<float, spire::kNumParams> SpireEngine::initParams()
{
    std::array<float, spire::kNumParams> a {};
    for (int i = 0; i < spire::kNumParams; ++i)
        a[(size_t) i] = spire::kParams[i].init;
    return a;
}

SpireEngine::SpireEngine()
{
    params = initParams();
    rng.seed (0x5E1);
    for (auto& v : voices)
        for (int c = 0; c < 2; ++c) { v.comb1[c].init (4096); v.comb2[c].init (4096); }
}

void SpireEngine::prepare (double hostRate, int maxBlock)
{
    sr = hostRate > 0 ? hostRate : 44100.0;
    bufL.assign ((size_t) std::max (64, maxBlock), 0.0);
    bufR = bufL;
    reset();
}

void SpireEngine::reset()
{
    for (auto& v : voices) { v.active = false; v.gate = false; v.gstage.kill(); }
    held.clear();
    ageCounter = 0;
    rng.seed (0x5E1);
}

int SpireEngine::activeVoiceCount() const
{
    int n = 0;
    for (auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void SpireEngine::controller (int cc, int value)
{
    if (cc == 1) modWheel = value / 127.0;
    if (cc == 120 || cc == 123) allNotesOff (cc == 120);
}

void SpireEngine::allNotesOff (bool hard)
{
    for (auto& v : voices)
    {
        if (hard) { v.active = false; v.gstage.kill(); }
        else if (v.active) { v.gate = false; v.gstage.release(); for (auto& e : v.env) if (e.stage) e.stage = 4; }
    }
    held.clear();
}

//==============================================================================
void SpireEngine::noteOn (int note, int velocity)
{
    const float vel = std::clamp (velocity / 127.0f, 0.0f, 1.0f);
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);

    const int voiceMode = listIndex (p (spire::idx::mode));          // mono_1..4, poly_1..2
    if (voiceMode <= 3)
    {
        // mono: mono_1/3 retrigger, mono_2/4 legato
        const bool legato = (voiceMode == 1 || voiceMode == 3);
        Voice& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i) { voices[i].active = false; }
        startVoice (v, note, vel, legato && v.active && v.gate);
        return;
    }

    Voice* best = nullptr;
    for (auto& v : voices) if (! v.active) { best = &v; break; }
    if (! best)
    {
        for (auto& v : voices) if (! v.gate && (! best || v.age < best->age)) best = &v;
        if (! best) for (auto& v : voices) if (! best || v.age < best->age) best = &v;
    }
    startVoice (*best, note, vel, false);
}

void SpireEngine::noteOff (int note)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    const int voiceMode = listIndex (p (spire::idx::mode));
    if (voiceMode <= 3)
    {
        Voice& v = voices[0];
        if (! v.active || v.note != note) return;
        if (! held.empty()) { startVoice (v, held.back(), v.vel, voiceMode == 1 || voiceMode == 3); return; }
    }
    for (auto& v : voices)
        if (v.active && v.gate && v.note == note)
        {
            v.gate = false;
            v.gstage.release();
            for (auto& e : v.env) if (e.stage) e.stage = 4;
        }
}

void SpireEngine::startVoice (Voice& v, int note, float vel, bool legato)
{
    const bool wasActive = v.active;
    v.active = true;
    v.gate = true;
    v.note = note;
    v.vel = vel;
    v.age = ++ageCounter;
    v.targetNote = note;
    if (! legato) v.rng.seed (rng.next());
    const double glide = glideSeconds (p (spire::idx::glide));
    if (! wasActive || glide <= 0.0) v.curNote = note;

    if (! legato)
    {
        for (int o = 0; o < 4; ++o)
        {
            const int base = osc1_type + o * kOscStride;
            const int count = std::clamp (listIndex (p (base + 11)) + 1, 1, kMaxUnison);
            auto& osc = v.osc[(size_t) o];
            osc.count = count;
            const int phaseIdx = listIndex (p (base + 6));
            for (int u = 0; u < kMaxUnison; ++u)
            {
                auto& un = osc.u[(size_t) u];
                un.phase = phaseIdx > 0 ? phaseIdx / 999.0 : v.rng.uniform();   // 0 = free (random start)
                un.phase2 = un.phase;
                un.fb = 0;
            }
            osc.drift = v.rng.bipolar();
        }
        for (auto& e : v.env) { e.stage = 1; if (! wasActive) e.level = 0; }
        for (int l = 0; l < 4; ++l)
        {
            auto& lf = v.lfo[(size_t) l];
            const int base = lfo1_rate + l * kLfoStride;
            if (! flag (p (base + 4)))            // poly LFO: retrigger at the start phase
                lf.phase = p (base + 6);
            lf.fade = 0;
        }
        if (! wasActive)
            for (int c = 0; c < 2; ++c)
            {
                v.f1a[c].reset(); v.f1b[c].reset(); v.f2a[c].reset(); v.f2b[c].reset();
                v.lad1[c].reset(); v.lad2[c].reset(); v.comb1[c].clear(); v.comb2[c].clear();
            }
        if (global) v.gstage.start (note, vel, *global, sr, ! wasActive);
    }
    v.ctl = 0;
}

double SpireEngine::envTick (Voice& v, int e)
{
    auto& env = v.env[(size_t) e];
    const int base = env1_att + e * kEnvStride;
    const double att = envSeconds (p (base)), dec = envSeconds (p (base + 1)), sus = p (base + 2);
    const double slt = envSeconds (p (base + 3)), sll = p (base + 4), rel = envSeconds (p (base + 5));
    const double dt = 1.0 / sr;
    switch (env.stage)
    {
        case 0: return 0.0;
        case 1:   // attack (linear)
            env.level += att <= 0.0 ? 1.0 : dt / att;
            if (env.level >= 1.0) { env.level = 1.0; env.stage = 2; }
            break;
        case 2:   // decay to sustain
            env.level = sus + (env.level - sus) * (dec <= 0.0 ? 0.0 : std::exp (-5.0 * dt / dec));
            if (std::abs (env.level - sus) < 1e-4) { env.level = sus; env.stage = 3; }
            break;
        case 3:   // slope: from the sustain level towards the slope level
            if (slt > 0.0) env.level += (sll - env.level) * (1.0 - std::exp (-5.0 * dt / slt));
            break;
        case 4:   // release
            env.level *= rel <= 0.0 ? 0.0 : std::exp (-6.9 * dt / rel);
            if (env.level < 1e-5) { env.level = 0; env.stage = 0; }
            break;
    }
    return env.level;
}

//==============================================================================
void SpireEngine::controlVoice (Voice& v)
{
    // modulation from the envelopes' and LFOs' hard targets and the matrix (known targets only)
    v.modCut1 = v.modCut2 = v.modRes1 = v.modVol = 0;
    for (auto& m : v.modPitch) m = 0;
    auto apply = [&] (int targetId, double amount)
    {
        switch (decodeTarget (targetId))
        {
            case TCutoff1: v.modCut1 += amount; break;
            case TCutoff2: v.modCut2 += amount; break;
            case TReso1:   v.modRes1 += amount; break;
            case TPitchAll: for (auto& m : v.modPitch) m += amount * 24.0; break;
            case TVolume:  v.modVol += amount; break;
            default: break;
        }
    };
    for (int e = 0; e < 4; ++e)
    {
        const int base = env1_att + e * kEnvStride;
        const double lvl = v.env[(size_t) e].level;
        for (int t = 0; t < 2; ++t)
        {
            const int tgt = listIndex (p (base + 10 + t * 3));
            if (tgt == 0) continue;
            const double amt = (p (base + 11 + t * 3) - 0.5) * 2.0;
            const double velAmt = (p (base + 12 + t * 3) - 0.5) * 2.0;
            apply (tgt, lvl * amt * (1.0 + velAmt * (v.vel - 1.0)));
        }
    }
    for (int l = 0; l < 4; ++l)
    {
        const int base = lfo1_rate + l * kLfoStride;
        const double val = v.lfo[(size_t) l].value;
        for (int t = 0; t < 2; ++t)
        {
            const int tgt = listIndex (p (base + 10 + t * 3));
            if (tgt == 0) continue;
            apply (tgt, val * (p (base + 11 + t * 3) - 0.5) * 2.0);
        }
    }
}

void SpireEngine::renderVoice (Voice& v, double* L, double* R, int n)
{
    const auto& waves = WaveBank::get();
    const double glide = glideSeconds (p (spire::idx::glide));
    const double bendSemis = bend >= 0 ? bend * listIndex (p (bend_up)) : bend * listIndex (p (bend_dn));
    const double masterGain = p (spire::idx::volume) * 2.0;
    const int rout = flag (p (flt_rout)) ? 1 : 0;
    const double balance = p (flt_balance);
    const double keytrack = (p (flt_keytrack) - 0.5) * 2.0;
    const double velSens = p (spire::idx::velocity);
    const bool ampOverride = global && global->ampOverride;

    for (int i = 0; i < n; ++i)
    {
        // control rate: every 32 samples
        if ((v.ctl++ & 31) == 0)
        {
            if (glide > 0) v.curNote = v.targetNote + (v.curNote - v.targetNote) * std::exp (-32.0 / (sr * glide * 0.3));
            else v.curNote = v.targetNote;
            // LFOs
            for (int l = 0; l < 4; ++l)
            {
                const int base = lfo1_rate + l * kLfoStride;
                auto& lf = v.lfo[(size_t) l];
                double hz = flag (p (base + 3)) ? bpm / 60.0 / kSyncBeats[std::clamp (listIndex (p (base)) % 21, 0, 20)] : lfoHz (p (base));
                const double prev = lf.phase;
                lf.phase += hz * 32.0 / sr;
                const bool wrap = lf.phase >= 1.0;
                lf.phase -= std::floor (lf.phase);
                const int form = std::min (listIndex (p (base + 7)), 51);
                const double ph = lf.phase;
                double val;
                switch (form)
                {
                    case 0: val = std::sin (kTwoPi * ph); break;
                    case 1: val = ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph; break;
                    case 2: val = ph < 0.5 ? 1 : -1; break;
                    case 3: if (wrap || prev == 0) lf.sh = v.rng.bipolar(); val = lf.sh; break;
                    case 4: if (wrap) lf.sh = v.rng.bipolar(); val = lf.value + (lf.sh - lf.value) * 0.1; break;
                    default: val = waves.sampleRaw (2 + (form - 5) % 62, ph); break;
                }
                const double fadeT = envSeconds (p (base + 9));
                lf.fade = fadeT > 0 ? std::min (1.0, lf.fade + 32.0 / (sr * fadeT)) : 1.0;
                lf.value = val * (p (base + 8) * 2.0) * lf.fade;
            }
            controlVoice (v);
            if (global) v.gstage.control (*global, sr, 32);

            // filters
            for (int f = 0; f < 2; ++f)
            {
                const int base = flt1_type + f * 4;
                const int type = listIndex (p (base));
                double cut = p (base + 2) + (f == 0 ? v.modCut1 : v.modCut2) + keytrack * (v.curNote - 60) / 127.0;
                if (f == 1 && flag (p (flt_link))) cut = p (flt1_cut) + (p (flt2_cut) - 0.5) + v.modCut1 + keytrack * (v.curNote - 60) / 127.0;
                const double hz = std::min (cutoffHz (cut), sr * 0.45);
                const double res = std::clamp (p (base + 3) + (f == 0 ? v.modRes1 : 0.0), 0.0, 1.0);
                for (int c = 0; c < 2; ++c)
                {
                    auto& a = f == 0 ? v.f1a[c] : v.f2a[c];
                    auto& b = f == 0 ? v.f1b[c] : v.f2b[c];
                    auto& lad = f == 0 ? v.lad1[c] : v.lad2[c];
                    switch (type)
                    {
                        case 1: a.set (hz, 2.0 * std::pow (1.0 - res, 1.5) + 0.02, sr); b.set (hz, 1.4, sr); break;        // Perfecto: 4-pole SVF
                        case 2: case 4: lad.set (hz, 4.1 * std::pow (res, 1.2), sr); break;                             // Acido / Scorpio: ladder
                        case 3: a.set (hz, 2.0 * std::pow (1.0 - res, 1.6) + 0.01, sr); break;                          // Infecto: 2-pole SVF
                        default: a.set (hz, 2.0 * std::pow (1.0 - res, 1.6) + 0.01, sr); break;
                    }
                }
            }
        }

        // envelopes
        const double env1 = envTick (v, 0);
        envTick (v, 1); envTick (v, 2); envTick (v, 3);
        const double gAmp = v.gstage.tickEnvelopes();
        const double amp = ampOverride ? gAmp : env1;

        // oscillators
        double inF1L = 0, inF1R = 0, inF2L = 0, inF2R = 0;
        for (int o = 0; o < 4; ++o)
        {
            if (flag (p (osc1_mute + o))) continue;
            const double level = p (mix_osc1 + o);
            if (level <= 0.0) continue;
            const int base = osc1_type + o * kOscStride;
            auto& osc = v.osc[(size_t) o];
            const int type = listIndex (p (base));
            const int oct = (int) (p (base + 1) * 8.99f) - 4;
            const int semi = (int) (p (base + 2) * 22.99f) - 11;
            const double fine = (p (base + 3) - 0.5) * 2.0;
            const double ctrlA = std::clamp ((double) p (base + 4), 0.0, 1.0);
            const double ctrlB = std::clamp ((double) p (base + 5), 0.0, 1.0);
            const int wave = listIndex (p (base + 7));
            const double wtmix = p (base + 8);
            const double udet = p (base + 9), uwid = p (base + 10), uden = p (base + 13);
            const int chord = listIndex (p (base + 12));
            const double pan = (p (base + 14) - 0.5) * 2.0;
            const double drift = p (base + 15) * 0.08 * osc.drift;
            const double inv = flag (p (base + 16)) ? -1.0 : 1.0;
            const bool keyOn = p (base + 17) > 0.5f;
            const double mixto = p (base + 18);
            const double noteBase = (keyOn ? v.curNote : 60.0) + oct * 12 + semi + fine + bendSemis + drift + v.modPitch[(size_t) o];
            const int count = osc.count;
            const auto& chordNotes = kChords[(size_t) std::clamp (chord, 0, (int) kChords.size() - 1)];
            const int waveIdx = wave <= 1 ? wave : 2 + (wave - 2) % 62;

            double sumL = 0, sumR = 0, norm = 0;
            for (int u = 0; u < count; ++u)
            {
                auto& un = osc.u[(size_t) u];
                const double pos = count > 1 ? (double) u / (count - 1) * 2.0 - 1.0 : 0.0;           // -1..1
                const double detSemis = pos * 0.6 * udet * udet;                                       // up to +-0.6 st
                const double chordSemis = chord > 0 && count > 1 ? chordNotes[(size_t) (u % (int) chordNotes.size())] : 0.0;
                const double hz = dsp::midiToHz (noteBase + detSemis + chordSemis);
                const double dt = std::min (hz / sr, 0.49);
                const double ph = un.phase;
                double s;
                switch (type)
                {
                    case 1:   // Noise (colour by Ctrl A)
                        s = v.rng.bipolar();
                        osc.noiseLp.setCutoff (200.0 * std::pow (100.0, ctrlA), sr);
                        s = osc.noiseLp.lp (s) * (1.0 + 2.0 * (1.0 - ctrlA));
                        break;
                    case 2:   // FM: sine carrier, modulator = selected wave at ratio
                    {
                        const double ratio = 1.0 + std::floor (ctrlB * 8.0);
                        un.phase2 += dt * ratio; un.phase2 -= std::floor (un.phase2);
                        const double m = waves.sampleRaw (waveIdx, un.phase2);
                        s = std::sin (kTwoPi * (ph + ctrlA * 1.5 * m));
                        break;
                    }
                    case 3:   // AMSync: hard-synced saw (Ctrl B = sync ratio) with AM (Ctrl A)
                    {
                        const double ratio = 1.0 + ctrlB * 7.0;
                        double sp = ph * ratio; sp -= std::floor (sp);
                        s = 2.0 * sp - 1.0;
                        s *= 1.0 - ctrlA * (0.5 - 0.5 * std::cos (kTwoPi * ph));
                        break;
                    }
                    case 4:   // SawPWM: two saws offset by Ctrl A
                    {
                        double p2 = ph + 0.5 * ctrlA; p2 -= std::floor (p2);
                        s = ((2.0 * ph - 1.0) - polyBlep (ph, dt)) - ((2.0 * p2 - 1.0) - polyBlep (p2, dt)) * (0.5 + 0.5 * ctrlB);
                        break;
                    }
                    case 5:   // HardFM: saw carrier, linear FM from a sine at ratio Ctrl B, depth Ctrl A
                    {
                        const double ratio = 0.5 + ctrlB * 7.5;
                        un.phase2 += dt * ratio; un.phase2 -= std::floor (un.phase2);
                        double cp = ph + ctrlA * 0.8 * std::sin (kTwoPi * un.phase2); cp -= std::floor (cp);
                        s = 2.0 * cp - 1.0;
                        break;
                    }
                    case 6:   // Vowel: saw with a formant resonance swept by Ctrl A
                    {
                        const double saw = (2.0 * ph - 1.0) - polyBlep (ph, dt);
                        const double f1 = 300.0 + ctrlA * 600.0, f2 = 900.0 + ctrlA * 1800.0;
                        s = saw * 0.4 + std::sin (kTwoPi * ph * (f1 / hz)) * 0.4 * (1.0 - ctrlB) + std::sin (kTwoPi * ph * (f2 / hz)) * 0.3 * ctrlB;
                        break;
                    }
                    default:  // Classic: saw -> pulse by Ctrl A, pulse width by Ctrl B
                    {
                        const double saw = (2.0 * ph - 1.0) - polyBlep (ph, dt);
                        const double duty = 0.5 - 0.45 * ctrlB;
                        double p2 = ph + (1.0 - duty); p2 -= std::floor (p2);
                        const double pulse = (ph < duty ? 1.0 : -1.0) + polyBlep (ph, dt) - polyBlep (p2, dt) - (2.0 * duty - 1.0);
                        s = saw * (1.0 - ctrlA) + pulse * ctrlA;
                        break;
                    }
                }
                if (wtmix > 0.0 && type != 1)
                    s = s * (1.0 - wtmix) + waves.sample (waveIdx, ph, dt) * wtmix;
                un.phase += dt;
                un.phase -= std::floor (un.phase);

                const double outer = std::abs (pos);
                const double g = count > 1 ? (1.0 - outer * (1.0 - uden)) : 1.0;
                const double up = std::clamp (pan + pos * uwid, -1.0, 1.0);
                sumL += s * g * std::cos ((up + 1.0) * kPi * 0.25);
                sumR += s * g * std::sin ((up + 1.0) * kPi * 0.25);
                norm += g * g;
            }
            const double scale = inv * level / std::sqrt (std::max (1.0, norm));
            sumL *= scale; sumR *= scale;
            if (rout == 0) { inF1L += sumL; inF1R += sumR; }
            else { inF1L += sumL * (1.0 - mixto); inF1R += sumR * (1.0 - mixto); inF2L += sumL * mixto; inF2R += sumR * mixto; }
        }

        // filters
        auto runFilter = [&] (int f, double x, int c) -> double
        {
            const int base = flt1_type + f * 4;
            const int type = listIndex (p (base));
            const int fmode = listIndex (p (base + 1));
            auto& a = f == 0 ? v.f1a[c] : v.f2a[c];
            auto& b = f == 0 ? v.f1b[c] : v.f2b[c];
            auto& lad = f == 0 ? v.lad1[c] : v.lad2[c];
            auto& comb = f == 0 ? v.comb1[c] : v.comb2[c];
            switch (type)
            {
                case 0: return x;
                case 1:   // Perfecto: LP4 BP4 HP4 Peak
                {
                    const auto o1 = a.tick (x);
                    switch (fmode)
                    {
                        case 1: return b.tick (o1.bp).bp * 2.0;
                        case 2: return b.tick (o1.hp).hp;
                        case 3: return x + o1.bp * 2.0;
                        default: return b.tick (o1.lp).lp;
                    }
                }
                case 2: return lad.tick (x, std::clamp (fmode + 1, 1, 4));                     // Acido: LP1..LP4
                case 3:   // Infecto: LP2 BP2 HP2 Notch, driven
                {
                    const auto o = a.tick (std::tanh (x * 1.5));
                    switch (fmode) { case 1: return o.bp; case 2: return o.hp; case 3: return o.lp + o.hp; default: return o.lp; }
                }
                case 4:   // Scorpio: Red/Black LP2/LP4 (ladder with drive), Black HP4 / BP4
                {
                    const double y = lad.tick (std::tanh (x * (fmode >= 2 ? 2.0 : 1.3)), fmode % 2 == 0 ? 2 : 4);
                    if (fmode == 4) return x - y;
                    if (fmode == 5) return lad.tick (x, 2) - y;
                    return y;
                }
                case 5:   // Combo: comb filter tuned by cutoff, +/- feedback
                {
                    const double hz = cutoffHz (p (base + 2));
                    const double d = std::clamp (sr / std::max (20.0, hz), 2.0, 4000.0);
                    const double fb = (fmode % 2 == 0 ? 1.0 : -1.0) * 0.95 * p (base + 3);
                    const double y = x + fb * comb.read (d);
                    comb.push ((float) y);
                    return y * 0.5;
                }
                case 6:   // Shaper: saturate / foldback, amount by cutoff
                {
                    const double drive = 1.0 + 10.0 * p (base + 2);
                    if (fmode == 1) { double z = x * drive; z = std::fmod (z + 1.0, 4.0); if (z < 0) z += 4.0; return (z < 2.0 ? z - 1.0 : 3.0 - z) * 0.7; }
                    return std::tanh (x * drive) / std::tanh (drive) ;
                }
                default: return x;
            }
        };
        double l, r;
        if (rout == 0)
        {
            l = runFilter (1, runFilter (0, inF1L, 0), 0);
            r = runFilter (1, runFilter (0, inF1R, 1), 1);
        }
        else
        {
            l = runFilter (0, inF1L, 0) * balance + runFilter (1, inF2L, 0) * (1.0 - balance);
            r = runFilter (0, inF1R, 1) * balance + runFilter (1, inF2R, 1) * (1.0 - balance);
        }

        v.gstage.filter (l, r);
        const double velGain = 1.0 - velSens * (1.0 - v.vel);
        const double g = amp * velGain * masterGain * 0.35 * (1.0 + v.modVol);
        L[i] += l * g;
        R[i] += r * g;

        const bool done = ampOverride ? ! v.gstage.ampActive() : v.env[0].stage == 0;
        if (done && ! v.gate) { v.active = false; return; }
    }
}

void SpireEngine::process (float* L, float* R, int n)
{
    if ((int) bufL.size() < n) { bufL.resize ((size_t) n); bufR.resize ((size_t) n); }
    std::fill (bufL.begin(), bufL.begin() + n, 0.0);
    std::fill (bufR.begin(), bufR.begin() + n, 0.0);
    for (auto& v : voices)
        if (v.active) renderVoice (v, bufL.data(), bufR.data(), n);
    for (int i = 0; i < n; ++i) { L[i] = (float) bufL[(size_t) i]; R[i] = (float) bufR[(size_t) i]; }
}

//==============================================================================
PatchSteps SpireEngine::extractSteps (const std::array<float, spire::kNumParams>& a)
{
    PatchSteps s;
    auto pv = [&] (int i) { return a[(size_t) i]; };
    s.present = true;
    s.arpOn = flag (pv (arp_on));
    s.mode = std::clamp (listIndex (pv (arp_mode)), 0, 9);
    s.octaves = std::clamp (listIndex (pv (arp_octave)) + 1, 1, 4);
    s.length = std::clamp (listIndex (pv (arp_wrap)), 1, 16);
    s.gate = std::clamp ((double) pv (arp_length) * 1.5, 0.05, 1.5);
    s.swing = (pv (arp_swing) - 0.5) * 2.0;
    s.velocityMode = std::clamp (listIndex (pv (arp_velocity)), 0, 4);

    // Spire's division -> ArpSampler rate (1/2 1/4 1/8 1/16 1/32) and type (straight, triplet, dotted)
    const double beats = kSyncBeats[std::clamp (listIndex (pv (arp_speed)), 0, 20)];
    struct R { double beats; int idx, type; };
    static const R rates[] = { { 2, 0, 0 }, { 1, 1, 0 }, { 0.5, 2, 0 }, { 0.25, 3, 0 }, { 0.125, 4, 0 },
                               { 4.0 / 3, 0, 1 }, { 2.0 / 3, 1, 1 }, { 1.0 / 3, 2, 1 }, { 1.0 / 6, 3, 1 }, { 1.0 / 12, 4, 1 },
                               { 3, 0, 2 }, { 1.5, 1, 2 }, { 0.75, 2, 2 }, { 0.375, 3, 2 }, { 0.1875, 4, 2 } };
    double bestErr = 1e9;
    for (auto& r : rates)
        if (std::abs (std::log (r.beats / beats)) < bestErr) { bestErr = std::abs (std::log (r.beats / beats)); s.rateIndex = r.idx; s.rateType = r.type; }

    for (int k = 0; k < 16; ++k)
    {
        PatchSteps::Step st;
        st.transpose = (int) std::lround ((pv (arp_note1 + k) - 0.5) * 48.0);          // GUESS: +-24 semitones
        st.velocity = std::clamp ((int) std::lround (pv (arp_vel1 + k) * 127.0), 0, 127);
        st.tie = flag (pv (arp_hold1 + k));
        s.steps.push_back (st);
    }
    return s;
}

void SpireEngine::extractEnvelopes (const std::array<float, spire::kNumParams>& a, AdsrTimes& amp, AdsrTimes& filter)
{
    auto pv = [&] (int i) { return (double) a[(size_t) i]; };
    amp = { envSeconds (pv (env1_att)), envSeconds (pv (env1_dec)), pv (env1_sus), envSeconds (pv (env1_rel)) };
    filter = { envSeconds (pv (env2_att)), envSeconds (pv (env2_dec)), pv (env2_sus), envSeconds (pv (env2_rel)) };
}
} // namespace vc
