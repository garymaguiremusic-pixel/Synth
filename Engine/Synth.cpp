#include "Synth.h"
#include "Tables/Waves.h"
#include "VirusCLayout.h"

#include <cstring>

namespace vc
{
using namespace layout::off;
using dsp::kPi;
using dsp::kTwoPi;

namespace
{
    // modmatrixDest index -> byte offset (-1 = not a single-byte destination)
    constexpr int kDestOffset[123] = {
        -1, Patch_Volume, Channel_Volume, Panorama, Transpose, Portamento_Time,
        Osc1_Shape, Osc1_Pulsewidth, Osc1_Wave_Select, Osc1_Semitone, Osc1_Keyfollow,
        Osc2_Shape, Osc2_Pulsewidth, Osc2_Wave_Select, Osc2_Semitone, Osc2_Detune, Osc2_FM_Amount,
        Osc2_Filt_Env_Amt, FM_Filt_Env_Amt, Osc2_Keyfollow, Osc_Balance, Suboscillator_Volume,
        Osc_Mainvolume, Noise_Volume, Cutoff, Cutoff2, Filter1_Resonance, Filter2_Resonance,
        Filter1_Env_Amt, Filter2_Env_Amt, Filter1_Keyfollow, Filter2_Keyfollow, Filter_Balance,
        Filter_Env_Attack, Filter_Env_Decay, Filter_Env_Sustain, Filter_Env_Sustain_Time, Filter_Env_Release,
        Amp_Env_Attack, Amp_Env_Decay, Amp_Env_Sustain, Amp_Env_Sustain_Time, Amp_Env_Release,
        Lfo1_Rate, Lfo1_Symmetry, Osc1_Lfo1_Amount, Osc2_Lfo1_Amount, PW_Lfo1_Amount, Reso_Lfo1_Amount,
        FiltGain_Lfo1_Amount, Lfo2_Rate, Lfo2_Symmetry, Shape_Lfo2_Amount, FM_Lfo2_Amount,
        Cutoff1_Lfo2_Amount, Cutoff2_Lfo2_Amount, Pan_Lfo2_Amount, Lfo3_Rate, Osc_Lfo3_Amount,
        Unison_Detune, Unison_Pan_Spread, Unison_Lfo_Phase, Chorus_Mix, Chorus_Rate, Chorus_Depth,
        Chorus_Delay, Chorus_Feedback, Effect_Send, Delay_Time, Delay_Feedback, Dly_Rate_Rev_Decay,
        Dly_Depth, Osc1_Shape_Velocity, Osc2_Shape_Velocity, PulseWidth_Velocity, Fm_Amount_Velocity,
        Flt1_EnvAmt_Velocity, Flt2_EnvAmt_Velocity, Resonance1_Velocity, Resonance2_Velocity,
        Amp_Velocity, Panorama_Velocity, Assign1_Amount, Assign2_Amount1, Assign2_Amount2,
        Assign3_Amount1, Assign3_Amount2, Assign3_Amount3, Osc_Init_Phase, Punch_Intensity,
        Ringmodulator_Volume, Noise_Color, Delay_Color, Bass_Intensity, Bass_Tune, Distortion_Intensity,
        -1 /* RingmodMix (input) */, Osc3_Volume, Osc3_Semitone, Osc3_Detune, LFO1_Assign_Amount,
        LFO2_Assign_Amount, Phaser_Mix, Phaser_Rate, Phaser_Depth, Phaser_Frequency, Phaser_Feedback,
        Phaser_Spread, Dly_Rate_Rev_Decay, Reverb_Damping, Delay_Color, Delay_Time, Delay_Feedback,
        -1 /* SecBalance */, Arp_Note_Length, Arp_Swing, Arp_Pattern_Selct, MidEQ_Gain, MidEQ_Frequency,
        MidEQ_Q_Factor, Assign_4_Amount, Assign_5_Amount, Assign_6_Amount };

    // source index -> controller number for the controller sources (modmatrixSource list)
    constexpr int kSourceCC[28] = { -1, -1, -1, 1, 2, 3, 4, 6, 8, 9, 11, 12, 13, 14, 15, 16, 64, 65, 66,
                                    -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    struct Slot { int src, dst, amt; };
    constexpr Slot kSlots[] = {
        { Assign1_Source, Assign1_Destination, Assign1_Amount },
        { Assign2_Source, Assign2_Destination1, Assign2_Amount1 },
        { Assign2_Source, Assign2_Destination2, Assign2_Amount2 },
        { Assign3_Source, Assign3_Destination1, Assign3_Amount1 },
        { Assign3_Source, Assign3_Destination2, Assign3_Amount2 },
        { Assign3_Source, Assign3_Destination3, Assign3_Amount3 },
        { Assign_4_Source, Assign_4_Destination, Assign_4_Amount },
        { Assign_5_Source, Assign_5_Destination, Assign_5_Amount },
        { Assign_6_Source, Assign_6_Destination, Assign_6_Amount },
    };

    inline float clampParam (int o, double v)
    {
        return (float) std::clamp (v, (double) layout::kParams[o].min, (double) layout::kParams[o].max);
    }

    bool isContinuous (int o)
    {
        const auto& d = layout::kParams[o];
        const std::string_view t = d.toText;
        return (d.max - d.min) > 16 && (t.empty() || t == "signed" || t == "pan" || t == "oscShape" || t == "unsignedZero");
    }
}

//==============================================================================
float Lfo::advance (double seconds, int shape, float sym, bool envMode, dsp::Rng& rng)
{
    if (envMode && oneShotDone)
        return value;
    const double prev = phase;
    phase += rateHz * seconds;
    bool wrapped = false;
    if (phase >= 1.0)
    {
        wrapped = true;
        if (envMode) { phase = 1.0; oneShotDone = true; }
        else phase -= std::floor (phase);
    }
    (void) prev;

    // symmetry: warp the phase so the first half-cycle gets shorter/longer
    const double s = std::clamp (0.5 + 0.48 * sym, 0.02, 0.98);
    const double ph = phase < s ? 0.5 * phase / s : 0.5 + 0.5 * (phase - s) / (1.0 - s);

    switch (shape)
    {
        case 0: value = (float) std::sin (kTwoPi * ph); break;
        case 1: value = (float) (ph < 0.25 ? 4 * ph : ph < 0.75 ? 2 - 4 * ph : 4 * ph - 4); break;
        case 2: value = (float) (1.0 - 2.0 * ph); break;
        case 3: value = ph < 0.5 ? 1.0f : -1.0f; break;
        case 4:
            if (wrapped) sh = (float) rng.bipolar();
            value = sh; break;
        case 5:
            if (wrapped) { shPrev = sgCur; sgCur = (float) rng.bipolar(); }
            value = shPrev + (sgCur - shPrev) * (float) ph; break;
        default:
            value = WaveBank::get().sampleRaw (std::clamp (shape - 6 + 2, 2, 63), ph); break;
    }
    if (fadeTime > 0.0)
    {
        fadeElapsed += seconds;
        value *= (float) std::min (1.0, fadeElapsed / fadeTime);
    }
    return value;
}

//==============================================================================
void Envelope::setup (double a, double d, double s, int sustainTime, double r, double sr)
{
    attackInc = a <= 0.0 ? 1.0 : 1.0 / (a * sr);
    // decay/release: exponential, time = to reach -60 dB-ish (6.9 time constants ~ 1/1000)
    decayCoef = d <= 0.0 ? 0.0 : std::exp (-6.9 / (d * sr));
    releaseCoef = r <= 0.0 ? 0.0 : std::exp (-6.9 / (r * sr));
    sustain = std::clamp (s, 0.0, 1.0);
    // sustain slope: dB per second -> gain per sample
    slopeMulPerSample = std::pow (10.0, tables::sustainSlopeDbPerSec (sustainTime) / (20.0 * sr));
}

double Envelope::tick()
{
    switch (stage)
    {
        case Stage::Idle: return 0.0;
        case Stage::Attack:
            level += attackInc;
            if (level >= 1.0) { level = 1.0; stage = Stage::Decay; }
            break;
        case Stage::Decay:
            level = sustain + (level - sustain) * decayCoef;
            if (std::abs (level - sustain) < 1.0e-4) { level = sustain; stage = Stage::Sustain; }
            break;
        case Stage::Sustain:
            if (slopeMulPerSample != 1.0)
            {
                level = std::min (1.0, std::max (level, 1.0e-6) * slopeMulPerSample);
                if (level < 1.0e-5 && slopeMulPerSample < 1.0) { level = 0.0; stage = Stage::Idle; }
            }
            break;
        case Stage::Release:
            level *= releaseCoef;
            if (level < 1.0e-5) { level = 0.0; stage = Stage::Idle; }
            break;
    }
    return level;
}

//==============================================================================
void Voice::start (Synth& s, int n, float vel, int uIndex, int uCount, double glideFrom, bool retrigger, uint64_t a)
{
    const bool wasActive = active;
    active = true;
    gate = true;
    note = n;
    velocity = vel;
    unisonIndex = uIndex;
    unisonCount = uCount;
    age = a;
    targetNote = n;
    curNote = glideFrom >= 0.0 ? glideFrom : (double) n;
    randomSrc = (float) s.rng.bipolar();
    rng.seed (s.rng.next());
    timeSinceOn = 0.0;

    const auto& p = s.p;
    const int initPhase = p.i (Osc_Init_Phase);
    if (initPhase > 0 || ! wasActive)
    {
        const double ip = initPhase > 0 ? (initPhase - 1) / 126.0 : rng.uniform();
        ph1 = ip; ph2 = initPhase > 0 ? ip : rng.uniform(); ph3 = initPhase > 0 ? ip : rng.uniform();
        subPh = ph1 * 0.5; subOdd = false;
    }

    // LFO key trigger: 0 = free, otherwise restart at the given phase
    const int ktr[3] = { p.i (Lfo1_Keytrigger), p.i (Lfo2_Keytrigger), 0 };
    for (int i = 0; i < 3; ++i)
    {
        const double uniPhase = unisonCount > 1 ? (p[Unison_Lfo_Phase] - 64.0) / 64.0 * (double) unisonIndex / (unisonCount - 1) : 0.0;
        if (i < 2 ? ktr[i] > 0 : true)
            lfo[i].retrigger ((i < 2 ? (ktr[i] - 1) / 126.0 : 0.0) + uniPhase);
        else if (! wasActive)
            lfo[i].retrigger (rng.uniform() + uniPhase);
        lfo[i].oneShotDone = false;
    }
    lfo[2].fadeTime = tables::lfoFadeSec (p.i (Lfo3_Fade_In_Time));
    lfo[2].fadeElapsed = 0.0;

    if (! wasActive)
    {
        f1a.reset(); f1b.reset(); f2a.reset(); f2b.reset(); ladder.reset();
        noiseLp.reset(); noiseHp.reset(); satShaper.reset();
    }
    ampEnv.gateOn (! wasActive);
    filtEnv.gateOn (! wasActive);
    (void) retrigger;
    punchEnv = 1.0;
    punchCoef = std::exp (-1.0 / (0.006 * kVoiceRate));
    ctlCounter = 0;
    firstCtl = true;
}

void Voice::legatoTo (int n, double glideFromCurrent)
{
    note = n;
    targetNote = n;
    if (glideFromCurrent < 0.0) curNote = n;
}

void Voice::release()
{
    gate = false;
    ampEnv.gateOff();
    filtEnv.gateOff();
}

void Voice::controlTick (Synth& s, int blockLen)
{
    const auto& p = s.p;
    const double dtBlock = blockLen / kVoiceRate;
    prevCtl = ctl;

    // --- glide -------------------------------------------------------------------
    const double porta = tables::portamentoSec ((int) std::lround (std::clamp (p[Portamento_Time] + mod[Portamento_Time], 0.0f, 127.0f)));
    if (porta <= 0.0) curNote = targetNote;
    else curNote = targetNote + (curNote - targetNote) * std::exp (-dtBlock / (porta * 0.3));

    // --- LFOs ---------------------------------------------------------------------
    auto pvRaw = [&] (int o) { return (double) clampParam (o, p[o] + mod[(size_t) o]); };
    const double bpm = s.tempoBpm();
    const int lfoRateOff[3] = { Lfo1_Rate, Lfo2_Rate, Lfo3_Rate };
    const int lfoShapeOff[3] = { Lfo1_Shape, Lfo2_Shape, Lfo3_Shape };
    const int lfoModeOff[3] = { Lfo1_Mode, Lfo2_Mode, Lfo3_Mode };
    const int lfoClockOff[3] = { Lfo1_Clock, Lfo2_Clock, Lfo3_Clock };
    const int lfoKfOff[3] = { Lfo1_Keyfollow, Lfo2_Keyfollow, Lfo3_Keyfollow };
    for (int i = 0; i < 3; ++i)
    {
        if (p.i (lfoModeOff[i]) == 1)
        {
            lfoVal[i] = s.monoLfoVal[i];
            continue;
        }
        const int clk = p.i (lfoClockOff[i]);
        double hz = clk > 0 ? bpm / 60.0 / tables::divisionBeats (clk) : tables::lfoRateHz (pvRaw (lfoRateOff[i]));
        hz *= std::pow (2.0, (curNote - 60.0) / 12.0 * pvRaw (lfoKfOff[i]) / 127.0);
        lfo[i].rateHz = hz;
        const float sym = i < 2 ? (float) ((pvRaw (i == 0 ? Lfo1_Symmetry : Lfo2_Symmetry) - 64.0) / 64.0) : 0.0f;
        const bool envMode = i < 2 && p.i (i == 0 ? Lfo1_Env_Mode : Lfo2_Env_Mode) != 0;
        lfoVal[i] = lfo[i].advance (dtBlock, p.i (lfoShapeOff[i]), sym, envMode, rng);
    }

    // --- envelopes (re-setup each block so modulation of times works) -------------
    ampEnv.setup (tables::attackTimeSec (pvRaw (Amp_Env_Attack)), tables::decayTimeSec (pvRaw (Amp_Env_Decay)),
                  pvRaw (Amp_Env_Sustain) / 127.0, (int) std::lround (pvRaw (Amp_Env_Sustain_Time)),
                  tables::releaseTimeSec (pvRaw (Amp_Env_Release)), kVoiceRate);
    filtEnv.setup (tables::attackTimeSec (pvRaw (Filter_Env_Attack)), tables::decayTimeSec (pvRaw (Filter_Env_Decay)),
                   pvRaw (Filter_Env_Sustain) / 127.0, (int) std::lround (pvRaw (Filter_Env_Sustain_Time)),
                   tables::releaseTimeSec (pvRaw (Filter_Env_Release)), kVoiceRate);

    // --- mod matrix: sources -------------------------------------------------------
    const double vel = velocity;
    auto source = [&] (int src) -> double
    {
        switch (src)
        {
            case 0: return 0.0;
            case 1: return s.src.pitchBend;
            case 2: return s.src.chanPressure;
            case 19: return ampEnv.level;
            case 20: return filtEnv.level;
            case 21: return lfoVal[0];
            case 22: return lfoVal[1];
            case 23: return lfoVal[2];
            case 24: return vel;
            case 25: return vel;        // note-off velocity: not tracked separately (uses note-on)
            case 26: return (curNote - 64.0) / 64.0;
            case 27: return randomSrc;
            default:
                if (src > 0 && src < 28 && kSourceCC[src] >= 0)
                    return s.src.cc[(size_t) kSourceCC[src]];
                return 0.0;
        }
    };

    std::array<float, 256> next {};
    auto addMod = [&] (int destIdx, double amount)
    {
        if (destIdx <= 0 || destIdx >= 123) return;
        const int o = kDestOffset[destIdx];
        if (o >= 0) next[(size_t) o] += (float) amount;
    };
    for (const auto& sl : kSlots)
    {
        const int srcIdx = p.i (sl.src);
        if (srcIdx == 0) continue;
        const double amt = (pvRaw (sl.amt) - 64.0) / 64.0;
        addMod (p.i (sl.dst), amt * source (srcIdx) * 127.0);
    }
    addMod (p.i (LFO1_Assign_Dest), (pvRaw (LFO1_Assign_Amount) - 64.0) / 64.0 * lfoVal[0] * 127.0);
    addMod (p.i (LFO2_Assign_Dest), (pvRaw (LFO2_Assign_Amount) - 64.0) / 64.0 * lfoVal[1] * 127.0);
    mod = next;

    auto pv = [&] (int o) { return (double) clampParam (o, p[o] + mod[(size_t) o]); };
    auto sgn = [&] (int o) { return (pv (o) - 64.0) / 64.0; };
    auto velMod = [&] (int o) { return sgn (o) * vel * 127.0; };   // velocity -> parameter (guess)

    // --- pitch ----------------------------------------------------------------------
    const double bendRange = s.src.pitchBend >= 0 ? tables::benderSemis (p.i (Bender_Range_Up))
                                                  : -tables::benderSemis (p.i (Bender_Range_Down));
    double bendSemis = s.src.pitchBend * bendRange;
    if (p.i (Bender_Scale) == 0)  // linear: frequency moves linearly with the wheel
    {
        const double pb = s.src.pitchBend;
        const double full = bendRange * (pb >= 0 ? 1.0 : -1.0);      // semitones at full deflection
        bendSemis = 12.0 * std::log2 (std::max (0.01, 1.0 + (std::pow (2.0, full / 12.0) - 1.0) * std::abs (pb)));
    }
    const double transpose = pv (Transpose) - 64.0;
    double uniDetune = 0.0, uniPan = 0.0;
    if (unisonCount > 1)
    {
        const double pos = (double) unisonIndex / (unisonCount - 1) * 2.0 - 1.0;
        uniDetune = pos * 0.5 * tables::unisonDetuneCents (pv (Unison_Detune)) / 100.0;
        uniPan = pos * pv (Unison_Pan_Spread) / 127.0;
    }
    const double common = transpose + bendSemis + uniDetune;
    const double n1 = 60.0 + (curNote - 60.0) * tables::oscKeyfollow (pv (Osc1_Keyfollow)) + (pv (Osc1_Semitone) - 64.0)
                    + tables::lfoPitchSemis (sgn (Osc1_Lfo1_Amount)) * lfoVal[0];
    const double fenvSemis = tables::envPitchSemis (sgn (Osc2_Filt_Env_Amt)) * filtEnv.level;
    const double n2 = 60.0 + (curNote - 60.0) * tables::oscKeyfollow (pv (Osc2_Keyfollow)) + (pv (Osc2_Semitone) - 64.0)
                    + tables::osc2DetuneCents (pv (Osc2_Detune)) / 100.0 + fenvSemis
                    + tables::lfoPitchSemis (sgn (Osc2_Lfo1_Amount)) * lfoVal[0];
    // LFO3 -> osc pitch / PW
    const int l3dest = p.i (Lfo3_Destination);
    const double l3 = lfoVal[2] * pv (Osc_Lfo3_Amount) / 127.0;
    const double l3semis = l3 * l3 * (l3 < 0 ? -1 : 1) * 12.0;
    const double n1f = n1 + ((l3dest == 0 || l3dest == 1) ? l3semis : 0.0);
    const double n2f = n2 + ((l3dest == 1 || l3dest == 2) ? l3semis : 0.0);
    ctl.f1 = dsp::midiToHz (n1f + common);
    ctl.f2 = dsp::midiToHz (n2f + common);

    ctl.osc3Mode = p.i (Osc3_Mode);
    const double n3 = (ctl.osc3Mode == 1 ? n2f : curNote + (pv (Osc3_Semitone) - 64.0)) + tables::osc3DetuneCents (pv (Osc3_Detune)) / 100.0;
    ctl.f3 = dsp::midiToHz (n3 + common);

    // --- shapes / PW ------------------------------------------------------------------
    ctl.shape1 = std::clamp (pv (Osc1_Shape) + velMod (Osc1_Shape_Velocity) * 0.5, 0.0, 127.0);
    ctl.shape2 = std::clamp (pv (Osc2_Shape) + velMod (Osc2_Shape_Velocity) * 0.5 + sgn (Shape_Lfo2_Amount) * lfoVal[1] * 64.0, 0.0, 127.0);
    const double pwLfo = sgn (PW_Lfo1_Amount) * lfoVal[0] * 127.0 + velMod (PulseWidth_Velocity) * 0.5;
    const double l3pw = l3 * 127.0;
    ctl.duty1 = tables::pulseDuty (pv (Osc1_Pulsewidth) + pwLfo + ((l3dest == 3 || l3dest == 4) ? l3pw : 0.0));
    ctl.duty2 = tables::pulseDuty (pv (Osc2_Pulsewidth) + pwLfo + ((l3dest == 4 || l3dest == 5) ? l3pw : 0.0));
    ctl.wave1 = std::clamp ((int) std::lround (pv (Osc1_Wave_Select)), 0, 63);
    ctl.wave2 = std::clamp ((int) std::lround (pv (Osc2_Wave_Select)), 0, 63);
    ctl.sync = p.i (Osc2_Sync) != 0;
    ctl.fmMode = p.i (Osc_FM_Mode);
    ctl.fm = tables::fmIndex (std::clamp (pv (Osc2_FM_Amount) + sgn (FM_Filt_Env_Amt) * filtEnv.level * 127.0
                                          + sgn (FM_Lfo2_Amount) * lfoVal[1] * 127.0 + velMod (Fm_Amount_Velocity) * 0.5, 0.0, 127.0));

    // --- mixer --------------------------------------------------------------------------
    const double bal = pv (Osc_Balance) / 127.0;
    ctl.g1 = std::min (1.0, 2.0 * (1.0 - bal));
    ctl.g2 = std::min (1.0, 2.0 * bal);
    ctl.gSub = tables::volumeGain (pv (Suboscillator_Volume));
    ctl.subShape = p.i (Suboscillator_Shape);
    ctl.gOsc3 = ctl.osc3Mode == 0 ? 0.0 : tables::volumeGain (pv (Osc3_Volume));
    ctl.gNoise = tables::volumeGain (pv (Noise_Volume));
    ctl.gRing = tables::volumeGain (pv (Ringmodulator_Volume));
    ctl.gMain = tables::mainVolumeGain (pv (Osc_Mainvolume));
    ctl.drive = tables::mainVolumeDrive (pv (Osc_Mainvolume));
    ctl.noiseColor = sgn (Noise_Color);

    // --- filters ------------------------------------------------------------------------
    const double fe = filtEnv.level;
    const int base = p.i (Filter_Keytrack_Base);
    auto cutoffUnits = [&] (int cutOff, int envAmt, int pol, int kf, int envVel, int lfoAmt)
    {
        const double polSign = p.i (pol) ? 1.0 : -1.0;
        const double envAmount = std::clamp (pv (envAmt) + velMod (envVel), 0.0, 127.0);
        return pv (cutOff) + polSign * envAmount * fe
             + (curNote - base) * sgn (kf) / tables::kCutoffSemisPerStep
             + sgn (lfoAmt) * lfoVal[1] * 64.0;
    };
    double c1 = cutoffUnits (Cutoff, Filter1_Env_Amt, Filter1_Env_Polarity, Filter1_Keyfollow, Flt1_EnvAmt_Velocity, Cutoff1_Lfo2_Amount);
    double c2;
    if (p.i (Filter2_Cutoff_Link))
        c2 = c1 + (pv (Cutoff2) - 64.0) + (sgn (Cutoff2_Lfo2_Amount) - sgn (Cutoff1_Lfo2_Amount)) * lfoVal[1] * 64.0;
    else
        c2 = cutoffUnits (Cutoff2, Filter2_Env_Amt, Filter2_Env_Polarity, Filter2_Keyfollow, Flt2_EnvAmt_Velocity, Cutoff2_Lfo2_Amount);
    ctl.cut1 = tables::cutoffHz (std::clamp (c1, -20.0, 150.0));
    ctl.cut2 = tables::cutoffHz (std::clamp (c2, -20.0, 150.0));
    const double r1 = std::clamp (pv (Filter1_Resonance) + velMod (Resonance1_Velocity) * 0.5 + sgn (Reso_Lfo1_Amount) * lfoVal[0] * 64.0, 0.0, 127.0);
    const double r2 = std::clamp (pv (Filter2_Resonance) + velMod (Resonance2_Velocity) * 0.5, 0.0, 127.0);
    ctl.damp1 = tables::svfDamping (r1);
    ctl.damp2 = tables::svfDamping (r2);
    ctl.ladderK = tables::ladderK (r1);
    ctl.filtGain = std::max (0.0, 1.0 + sgn (FiltGain_Lfo1_Amount) * lfoVal[0]);
    ctl.f1Mode = std::clamp (p.i (Filter1_Mode), 0, 7);
    ctl.f2Mode = std::clamp (p.i (Filter2_Mode), 0, 3);
    ctl.routing = std::clamp (p.i (Filter_Routing), 0, 3);
    ctl.balance = pv (Filter_Balance) / 127.0;
    ctl.satCurve = std::clamp (p.i (Saturation_Curve), 0, 14);
    ctl.satAmount = std::clamp ((pv (Osc_Mainvolume) - 32.0) / 95.0, 0.0, 1.0);

    // --- amp / pan ------------------------------------------------------------------------
    const double velGain = tables::ampVelocityGain (sgn (Amp_Velocity), vel);
    const double vol = tables::patchVolumeGain (pv (Patch_Volume)) * tables::volumeGain (pv (Channel_Volume))
                     * (s.src.cc[11] > 0.0f ? s.src.cc[11] : 1.0f);
    ctl.level = vol * velGain / std::sqrt ((double) unisonCount) * std::pow (10.0, tables::kOutputTrimDb / 20.0);
    double pan = sgn (Panorama) + sgn (Pan_Lfo2_Amount) * lfoVal[1] + uniPan + sgn (Panorama_Velocity) * (vel - 0.5);
    pan = std::clamp (pan, -1.0, 1.0);
    ctl.panL = std::cos ((pan + 1.0) * kPi * 0.25);
    ctl.panR = std::sin ((pan + 1.0) * kPi * 0.25);

    if (firstCtl) { prevCtl = ctl; firstCtl = false; }
}

void Voice::render (Synth& s, double* L, double* R, int numSamples)
{
    if (! active) return;

    const auto& waves = WaveBank::get();
    const int ctlLen = kControlBlock * kOversample;
    const double sr = kVoiceRate;
    const double punchAmt = s.p[Punch_Intensity] / 127.0;

    for (int i = 0; i < numSamples; ++i)
    {
        if (ctlCounter == 0)
            controlTick (s, ctlLen);
        const double t = (double) ctlCounter / ctlLen;
        ctlCounter = (ctlCounter + 1) % ctlLen;
        auto lerp = [t] (double a, double b) { return a + (b - a) * t; };
        auto lerpLog = [t] (double a, double b) { return a * std::pow (b / a, t); };

        const double f1 = lerpLog (prevCtl.f1, ctl.f1);
        double f2 = lerpLog (prevCtl.f2, ctl.f2);
        const double dt1 = f1 / sr;

        // --- osc 1 --------------------------------------------------------------
        auto oscValue = [&] (double ph, double dt, double shape, double duty, int wave) -> double
        {
            // 0..64: wave -> saw, 64..127: saw -> pulse
            const double saw = (2.0 * ph - 1.0) - dsp::polyBlep (ph, std::abs (dt));
            if (shape <= 64.0)
            {
                const double a = shape / 64.0;
                const double w = a < 1.0 ? waves.sample (wave, ph, dt) : 0.0;
                return (1.0 - a) * w + a * saw;
            }
            double ph2 = ph + (1.0 - duty);
            ph2 -= std::floor (ph2);
            double pulse = (ph < duty ? 1.0 : -1.0) + dsp::polyBlep (ph, std::abs (dt)) - dsp::polyBlep (ph2, std::abs (dt));
            pulse -= (2.0 * duty - 1.0);
            const double b = (shape - 64.0) / 63.0;
            return (1.0 - b) * saw + b * pulse;
        };

        const double sh1 = lerp (prevCtl.shape1, ctl.shape1), sh2 = lerp (prevCtl.shape2, ctl.shape2);
        const double o1 = oscValue (ph1, dt1, sh1, lerp (prevCtl.duty1, ctl.duty1), ctl.wave1);

        // --- sub osc (one octave below osc1) --------------------------------------
        double sub = 0.0;
        if (ctl.gSub > 0.0)
        {
            const double sp = subPh;
            if (ctl.subShape == 0)
            {
                double sp2 = sp + 0.5; sp2 -= std::floor (sp2);
                sub = (sp < 0.5 ? 1.0 : -1.0) + dsp::polyBlep (sp, dt1 * 0.5) - dsp::polyBlep (sp2, dt1 * 0.5);
            }
            else
                sub = sp < 0.5 ? 4.0 * sp - 1.0 : 3.0 - 4.0 * sp;
        }

        // --- noise --------------------------------------------------------------------
        double noise = 0.0;
        if (ctl.gNoise > 0.0 || ctl.fmMode == 3)
        {
            const double w = rng.bipolar();
            const double c = ctl.noiseColor;
            noiseLp.setCutoff (c < 0 ? 20000.0 * std::pow (0.01, -c) : 20000.0, sr);
            noiseHp.setCutoff (c > 0 ? 20.0 * std::pow (200.0, c) : 5.0, sr);
            noise = noiseHp.hp (noiseLp.lp (w));
        }

        // --- FM ------------------------------------------------------------------------
        if (ctl.fm > 0.0)
        {
            double m = 0.0;
            switch (ctl.fmMode)
            {
                case 0: m = ph1 < 0.5 ? 2.0 * ph1 : 2.0 - 2.0 * ph1; break;                 // pos-tri 0..1
                case 1: m = ph1 < 0.5 ? 4.0 * ph1 - 1.0 : 3.0 - 4.0 * ph1; break;           // tri -1..1
                case 2: m = o1; break;
                case 3: m = noise; break;
                default: m = 0.0; break;                                                    // external inputs: no-op
            }
            f2 = f2 + m * lerp (prevCtl.fm, ctl.fm) * f1;
        }
        f2 = std::clamp (f2, -0.45 * sr, 0.45 * sr);
        const double dt2 = f2 / sr;
        const double o2 = oscValue (ph2, dt2, sh2, lerp (prevCtl.duty2, ctl.duty2), ctl.wave2);

        // --- osc 3 ------------------------------------------------------------------------
        double o3 = 0.0;
        const double dt3 = ctl.f3 / sr;
        if (ctl.osc3Mode != 0 && ctl.gOsc3 > 0.0)
        {
            switch (ctl.osc3Mode)
            {
                case 1: o3 = oscValue (ph3, dt3, sh2, ctl.duty2, ctl.wave2); break;
                case 2: o3 = (2.0 * ph3 - 1.0) - dsp::polyBlep (ph3, dt3); break;
                case 3: { double q = ph3 + 0.5; q -= std::floor (q); o3 = (ph3 < 0.5 ? 1.0 : -1.0) + dsp::polyBlep (ph3, dt3) - dsp::polyBlep (q, dt3); break; }
                case 4: o3 = std::sin (kTwoPi * ph3); break;
                case 5: o3 = ph3 < 0.5 ? 4.0 * ph3 - 1.0 : 3.0 - 4.0 * ph3; break;
                default: o3 = waves.sample (std::clamp (ctl.osc3Mode - 6 + 2, 2, 63), ph3, dt3); break;
            }
        }

        // --- advance phases (with hard sync) ----------------------------------------------
        ph1 += dt1;
        bool wrapped1 = false;
        double wrapFrac = 0.0;
        if (ph1 >= 1.0)
        {
            ph1 -= 1.0;
            wrapped1 = true;
            wrapFrac = dt1 > 0 ? ph1 / dt1 : 0.0;   // fraction of the sample since the wrap
            subOdd = ! subOdd;
        }
        subPh = 0.5 * ph1 + (subOdd ? 0.5 : 0.0);
        ph2 += dt2;
        ph2 -= std::floor (ph2);
        if (ctl.sync && wrapped1)
        {
            ph2 = wrapFrac * dt2;
            ph2 -= std::floor (ph2);
        }
        ph3 += dt3;
        ph3 -= std::floor (ph3);

        // --- mix -----------------------------------------------------------------------------
        const double g1 = lerp (prevCtl.g1, ctl.g1), g2 = lerp (prevCtl.g2, ctl.g2);
        const double gMain = lerp (prevCtl.gMain, ctl.gMain);
        const double ring = o1 * o2;
        const double pathA = g1 * o1 + ctl.gSub * sub;
        const double pathB = g2 * o2 + ctl.gOsc3 * o3;
        const double common = ctl.gNoise * noise + ctl.gRing * ring;

        // --- filters ---------------------------------------------------------------------------
        const double c1 = lerpLog (prevCtl.cut1, ctl.cut1), c2 = lerpLog (prevCtl.cut2, ctl.cut2);
        const bool serial6 = ctl.routing == 1;
        if ((i & 3) == 0)
        {
            f1a.set (c1, ctl.damp1, sr);
            f1b.set (c1, serial6 ? 1.4 : ctl.damp1, sr);
            f2a.set (c2, ctl.damp2, sr);
            f2b.set (c2, 1.4, sr);
            ladder.set (c1, ctl.ladderK, sr);
        }
        auto runF1 = [&] (double x)
        {
            if (ctl.f1Mode >= 4)
                return ladder.tick (x, ctl.f1Mode - 3);
            auto pick = [&] (dsp::Svf::Out o)
            {
                switch (ctl.f1Mode) { case 0: return o.lp; case 1: return o.hp; case 2: return o.bp; default: return o.lp + o.hp; }
            };
            double y = pick (f1a.tick (x));
            if (serial6) y = pick (f1b.tick (y));
            return y;
        };
        auto runF2 = [&] (double x)
        {
            const auto o = f2a.tick (x);
            switch (ctl.f2Mode) { case 0: return o.lp; case 1: return o.hp; case 2: return o.bp; default: return o.lp + o.hp; }
        };
        const double satHz = dsp::midiToHz (curNote);

        double out = 0.0;
        const double bal = ctl.balance;
        switch (ctl.routing)
        {
            case 0: case 1:   // serial: F1 -> saturation -> F2; balance cross-fades F1 out vs F2 out
            {
                const double in = (pathA + pathB) * gMain + common;
                double y1 = runF1 (in);
                if (ctl.satCurve != 0)   // Osc Volume above 64 drives the saturation (level compensated)
                    y1 = satShaper.process (ctl.satCurve, y1 * ctl.drive, ctl.satAmount, satHz, sr) / ctl.drive;
                const double y2 = runF2 (y1);
                out = y1 * (1.0 - bal) + y2 * bal;
                break;
            }
            case 2:           // parallel
            {
                const double in = (pathA + pathB) * gMain + common;
                double y = runF1 (in) * (1.0 - bal) + runF2 (in) * bal;
                out = satShaper.process (ctl.satCurve, y, ctl.satAmount, satHz, sr) * 1.4;
                break;
            }
            default:          // split: osc1 path -> F1, osc2 path -> F2
            {
                const double y1 = runF1 (pathA * gMain + common * 0.5);
                const double y2 = runF2 (pathB * gMain + common * 0.5);
                out = satShaper.process (ctl.satCurve, y1 * (1.0 - bal) * 2.0 + y2 * bal * 2.0, ctl.satAmount, satHz, sr) * 0.7;
                break;
            }
        }
        out *= lerp (prevCtl.filtGain, ctl.filtGain);

        // --- amp -------------------------------------------------------------------------------
        filtEnv.tick();
        double amp = ampEnv.tick();
        if (punchAmt > 0.0)
        {
            amp *= 1.0 + punchAmt * 1.5 * punchEnv;
            punchEnv *= punchCoef;
        }
        const double level = lerp (prevCtl.level, ctl.level) * amp * 0.5;
        const double v = out * level;
        L[i] += v * lerp (prevCtl.panL, ctl.panL);
        R[i] += v * lerp (prevCtl.panR, ctl.panR);
        timeSinceOn += 1.0 / sr;

        if (! ampEnv.active())
        {
            active = false;
            gate = false;
            break;
        }
    }
}

//==============================================================================
Synth::Synth()
{
    for (int o = 0; o < 256; ++o)
        p.continuous[(size_t) o] = isContinuous (o);
    for (auto& v : monoLfoDisplay) v = 0.0;
    seed (1);
    setPatch (nullptr);
}

void Synth::seed (uint64_t s)
{
    seedValue = s;
    rng.seed (s * 0x2545F4914F6CDD1Dull + 1);
}

void Synth::reset()
{
    allNotesOff (true);
    ageCounter = 0;
    lastNoteForGlide = -1.0;
    noteVelocity.fill (0);
    arpNotes.clear();
    arpPhase = 0.0; arpStep = 0; arpPlaying = -1; arpDir = 1; arpNoteRemaining = 0.0; arpWasActive = false; arpFresh = true;
    for (int i = 0; i < 3; ++i) { monoLfo[i] = Lfo(); monoLfoVal[i] = 0.0; }
    p.cur = p.target;
    decL.reset(); decR.reset();
    bufL.fill (0.0); bufR.fill (0.0);
    ctlPos = 0;
    fx.reset();
    seed (seedValue);
}

void Synth::setPatch (const uint8_t* r)
{
    for (int o = 0; o < 256; ++o)
    {
        const uint8_t v = r ? r[o] : (uint8_t) layout::kParams[o].def;
        raw[(size_t) o] = v;
        p.target[(size_t) o] = p.cur[(size_t) o] = clampParam (o, v);
    }
    // controller sources start from the values stored in the patch
    const int ctlBytes[] = { 1, 2, 3, 4, 6, 8, 9, 11, 12, 13, 14, 15, 16, 64, 65, 66 };
    for (int c : ctlBytes) src.cc[(size_t) c] = raw[(size_t) c] / 127.0f;
    if (raw[(size_t) Expression] == 0) src.cc[11] = 1.0f;
}

void Synth::setParam (int o, int value)
{
    if (o < 0 || o >= 256) return;
    raw[(size_t) o] = (uint8_t) std::clamp (value, 0, 127);
    p.target[(size_t) o] = clampParam (o, value);
    if (! p.continuous[(size_t) o]) p.cur[(size_t) o] = p.target[(size_t) o];
    applyParamEffects (o);
}

void Synth::applyParamEffects (int o)
{
    if (o == Key_Mode || o == Unison_Mode || o == Arp_Mode)
        allNotesOff (false);
}

int Synth::getParam (int o) const { return o >= 0 && o < 256 ? raw[(size_t) o] : 0; }

double Synth::tempoBpm() const { return hostBpm > 0.0 ? hostBpm : tables::clockBpm (p.i (Clock_Tempo)); }

int Synth::unisonCount() const
{
    const int u = p.i (Unison_Mode);
    return u == 0 ? 1 : u == 1 ? 2 : std::clamp (u + 1, 1, 16);
}

bool Synth::isMono() const { const int k = p.i (Key_Mode); return k >= 1 && k <= 4; }

int Synth::activeVoiceCount() const
{
    int n = 0;
    for (auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

Voice* Synth::allocVoice()
{
    for (auto& v : voices) if (! v.active) return &v;
    Voice* best = nullptr;
    for (auto& v : voices)        // oldest releasing voice first
        if (v.isReleasing() && (best == nullptr || v.age < best->age)) best = &v;
    if (best) return best;
    for (auto& v : voices)
        if (best == nullptr || v.age < best->age) best = &v;
    best->kill();
    return best;
}

void Synth::noteOn (int note, int velocity)
{
    if (note < 0 || note > 127) return;
    if (velocity <= 0) { noteOff (note); return; }
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    heldNotes.push_back (note);
    noteVelocity[(size_t) note] = velocity;

    if (p.i (Arp_Mode) != 0)
    {
        if (p.i (Arp_Hold_Enable) && heldNotes.size() == 1)
            arpLatched.clear();
        if (std::find (arpLatched.begin(), arpLatched.end(), note) == arpLatched.end())
            arpLatched.push_back (note);
        return;
    }
    startNote (note, velocity, false);
}

void Synth::noteOff (int note)
{
    if (note < 0 || note > 127) return;
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    if (p.i (Arp_Mode) != 0)
    {
        if (! p.i (Arp_Hold_Enable))
            arpLatched.erase (std::remove (arpLatched.begin(), arpLatched.end(), note), arpLatched.end());
        return;
    }
    stopNote (note, false);
}

void Synth::startNote (int note, int velocity, bool)
{
    const float vel = velocity / 127.0f;
    const int uni = unisonCount();
    const int keyMode = p.i (Key_Mode);
    const double porta = p[Portamento_Time];

    if (keyMode == 5)   // Hold: new key after all were released releases the held ones
    {
        if (heldNotes.size() == 1)
        {
            for (auto& v : voices) if (v.active && v.gate) v.release();
            holdNotes.clear();
        }
        holdNotes.push_back (note);
    }

    if (keyMode >= 1 && keyMode <= 4)
    {
        // mono: 1 = multi-trigger + porta, 2 = legato + porta, 3 = multi-trigger, porta only legato, 4 = legato, porta only legato
        const bool legatoPlaying = currentMonoNote >= 0 && std::any_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.active && v.gate; });
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        monoStack.push_back (note);
        const bool retrigger = keyMode == 1 || keyMode == 3 || ! legatoPlaying;
        const bool glide = porta > 0 && (keyMode <= 2 || legatoPlaying);
        const double from = glide && lastNoteForGlide >= 0 ? lastNoteForGlide : -1.0;

        if (! retrigger)
        {
            for (auto& v : voices) if (v.active && v.gate) v.legatoTo (note, glide ? 1.0 : -1.0);
        }
        else
        {
            int u = 0;
            for (auto& v : voices)
                if (v.active && v.gate && u < uni)
                    v.start (*this, note, vel, u++, uni, from, true, ++ageCounter);
            for (; u < uni; ++u)
                allocVoice()->start (*this, note, vel, u, uni, from, true, ++ageCounter);
        }
        currentMonoNote = note;
        lastNoteForGlide = note;
        return;
    }

    const double from = porta > 0 && lastNoteForGlide >= 0 ? lastNoteForGlide : -1.0;
    for (int u = 0; u < uni; ++u)
        allocVoice()->start (*this, note, vel, u, uni, from, true, ++ageCounter);
    lastNoteForGlide = note;
}

void Synth::stopNote (int note, bool)
{
    const int keyMode = p.i (Key_Mode);
    if (keyMode == 5) return;   // Hold

    if (keyMode >= 1 && keyMode <= 4)
    {
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        if (note != currentMonoNote) return;
        if (! monoStack.empty())
        {
            const int back = monoStack.back();
            const bool retrigger = keyMode == 1 || keyMode == 3;
            const bool glide = p[Portamento_Time] > 0;
            if (retrigger)
            {
                monoStack.pop_back();
                startNote (back, noteVelocity[(size_t) back], false);
            }
            else
            {
                for (auto& v : voices) if (v.active && v.gate) v.legatoTo (back, glide ? 1.0 : -1.0);
                currentMonoNote = back;
                lastNoteForGlide = back;
            }
            return;
        }
        for (auto& v : voices) if (v.active && v.gate) v.release();
        currentMonoNote = -1;
        return;
    }

    for (auto& v : voices)
        if (v.active && v.gate && v.note == note) v.release();
}

void Synth::allNotesOff (bool hard)
{
    for (auto& v : voices)
    {
        if (hard) v.kill();
        else if (v.active) v.release();
    }
    heldNotes.clear(); monoStack.clear(); holdNotes.clear(); arpLatched.clear();
    currentMonoNote = -1;
    arpPlaying = -1;
}

void Synth::pitchBend (int v14) { src.pitchBend = (float) std::clamp ((v14 - 8192) / 8192.0, -1.0, 1.0); }
void Synth::channelPressure (int v) { src.chanPressure = v / 127.0f; }

void Synth::controller (int cc, int value)
{
    if (cc < 0 || cc > 127) return;
    src.cc[(size_t) cc] = value / 127.0f;
    if (cc == 120 || cc == 123) { allNotesOff (cc == 120); return; }
    if (cc == 64 && value < 64) { /* sustain pedal release: nothing special in v1 */ }
    // Page A parameter CCs (Virus maps CC n to page A index n)
    for (int i = 0; i < layout::kControllerMapSize; ++i)
    {
        const auto& m = layout::kControllerMap[i];
        if (m.type == layout::CtlType::CC && m.number == cc)
        {
            setParam (m.offset, value);
            break;
        }
    }
}

//==============================================================================
void Synth::controlTick()
{
    // parameter smoothing (~1 ms) on continuous parameters
    const double a = std::exp (-(double) kControlBlock / (0.001 * tables::kInternalRate));
    for (int o = 0; o < 256; ++o)
    {
        auto& c = p.cur[(size_t) o];
        const float t = p.target[(size_t) o];
        if (c != t)
        {
            c = (float) (t + (c - t) * a);
            if (std::abs (c - t) < 0.01f) c = t;
        }
    }

    // mono LFOs (shared by voices whose LFO mode is Mono)
    const double dt = kControlBlock / tables::kInternalRate;
    const int rateOff[3] = { Lfo1_Rate, Lfo2_Rate, Lfo3_Rate }, shapeOff[3] = { Lfo1_Shape, Lfo2_Shape, Lfo3_Shape };
    const int clkOff[3] = { Lfo1_Clock, Lfo2_Clock, Lfo3_Clock };
    for (int i = 0; i < 3; ++i)
    {
        const int clk = p.i (clkOff[i]);
        monoLfo[i].rateHz = clk > 0 ? tempoBpm() / 60.0 / tables::divisionBeats (clk) : tables::lfoRateHz (p[rateOff[i]]);
        const float sym = i < 2 ? (p[i == 0 ? Lfo1_Symmetry : Lfo2_Symmetry] - 64.0f) / 64.0f : 0.0f;
        monoLfoVal[i] = monoLfo[i].advance (dt, p.i (shapeOff[i]), sym, false, rng);
        monoLfoDisplay[i] = monoLfo[i].phase;
    }
}

void Synth::arpTick (int numSamples)
{
    const int mode = p.i (Arp_Mode);
    if (mode == 0)
    {
        if (arpWasActive)
        {
            if (arpPlaying >= 0) stopNote (arpPlaying, true);
            arpPlaying = -1;
            arpWasActive = false;
        }
        return;
    }
    arpWasActive = true;

    const double bpm = tempoBpm();
    const int clk = std::max (1, p.i (Arp_Clock));
    const double stepBeats = tables::divisionBeats (clk);
    const double stepSamples = std::max (1.0, stepBeats * 60.0 / bpm * tables::kInternalRate);
    const double swing = (p[Arp_Swing] / 127.0) * 0.25;       // up to 75 % swing (guess)
    const double lenFrac = std::clamp (0.5 + (p[Arp_Note_Length] - 64.0) / 128.0, 0.05, 1.0);

    if (arpPlaying >= 0)
    {
        arpNoteRemaining -= numSamples;
        if (arpNoteRemaining <= 0.0) { stopNote (arpPlaying, true); arpPlaying = -1; }
    }

    if (arpLatched.empty())
    {
        arpStep = 0;
        arpPhase = 0.0;
        arpFresh = true;
        return;
    }

    if (arpFresh)
    {
        arpFresh = false;
        arpPhase = 0.0;
    }
    else
    {
        arpPhase += numSamples;
        const double thisStep = stepSamples * ((arpStep % 2 == 1) ? (1.0 + swing) : (1.0 - swing));
        if (arpPhase < thisStep) return;
        arpPhase -= thisStep;
    }

    // build the note list
    std::vector<int> base = arpLatched;
    if (mode != 4) std::sort (base.begin(), base.end());
    const int octaves = std::clamp (p.i (Arp_Octave_Range), 0, 3) + 1;
    arpNotes.clear();
    for (int o = 0; o < octaves; ++o)
        for (int n : base) if (n + 12 * o < 128) arpNotes.push_back (n + 12 * o);
    if (arpNotes.empty()) return;

    if (arpPlaying >= 0) { stopNote (arpPlaying, true); arpPlaying = -1; }
    const int count = (int) arpNotes.size();
    int idx = 0;
    switch (mode)
    {
        case 1: idx = arpStep % count; break;                                     // up
        case 2: idx = count - 1 - arpStep % count; break;                         // down
        case 3: { const int period = std::max (1, 2 * count - 2); const int k = arpStep % period; idx = k < count ? k : period - k; break; }
        case 4: idx = arpStep % count; break;                                     // as played
        case 5: idx = (int) (rng.uniform() * count) % count; break;              // random
        case 6:                                                                   // chord
        {
            for (int n : arpNotes) startNote (n, noteVelocity[(size_t) (n % 128)] > 0 ? noteVelocity[(size_t) (n % 128)] : 100, true);
            arpPlaying = -1;
            arpStep++;
            return;
        }
        default: break;
    }
    const int n = arpNotes[(size_t) idx];
    const int baseNote = arpLatched[(size_t) (idx % (int) arpLatched.size())];
    startNote (n, std::max (1, noteVelocity[(size_t) baseNote]), true);
    arpPlaying = n;
    arpNoteRemaining = stepSamples * lenFrac;
    arpStep++;
}

void Synth::renderInternal (float* L, float* R, int numSamples)
{
    int done = 0;
    while (done < numSamples)
    {
        if (ctlPos == 0)
        {
            controlTick();
            arpTick (kControlBlock);
        }
        const int n = std::min (numSamples - done, kControlBlock - ctlPos);
        const int os = n * kOversample;
        std::fill (bufL.begin(), bufL.begin() + os, 0.0);
        std::fill (bufR.begin(), bufR.begin() + os, 0.0);
        for (auto& v : voices)
            if (v.active) v.render (*this, bufL.data(), bufR.data(), os);
        for (int i = 0; i < n; ++i)
        {
            decL.push (bufL[(size_t) (2 * i)]); decL.push (bufL[(size_t) (2 * i + 1)]);
            decR.push (bufR[(size_t) (2 * i)]); decR.push (bufR[(size_t) (2 * i + 1)]);
            L[done + i] = (float) decL.read();
            R[done + i] = (float) decR.read();
        }
        done += n;
        ctlPos = (ctlPos + n) % kControlBlock;
    }

    std::array<float, 256> globalMod {};
    fx.process (L, R, numSamples, p, tempoBpm(), globalMod);
}
} // namespace vc
