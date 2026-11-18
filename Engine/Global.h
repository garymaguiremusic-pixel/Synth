#pragma once
// Global sections shared by every synth mode (Virus, Sylenth, Spire):
//   - Filter ADSR and Amp ADSR (times in ms, or BPM-synced note values)
//   - Global filter: the Virus filter types (SVF LP/HP/BP/BS, analog 1-4 pole), per voice,
//     after the mode's own filters, driven by the Filter ADSR
//   - Saturation: the Virus saturation curves, on the summed output before the FX
// No JUCE dependency.

#include "Dsp.h"
#include "Fx.h"
#include <array>

namespace vc
{
enum class SynthMode { Virus = 0, Sylenth = 1, Spire = 2 };

/** One ADSR as set in the global section. Times are seconds (already converted from sync values). */
struct AdsrTimes
{
    double attack = 0.001, decay = 0.3, sustain = 1.0, release = 0.2;
};

/** Note divisions for the synced envelope times (quarter notes per value). */
struct SyncDivisions
{
    static constexpr int kCount = 19;
    static const char* name (int i);
    static double beats (int i);         // in quarter notes
};

/** Everything the voices need from the global section, rebuilt once per block. */
struct GlobalVoiceSettings
{
    AdsrTimes amp, filter;
    bool ampOverride = true;             // false: the mode's own amp envelope is used (patch value)
    int filterMode = 0;                  // 0 = off, 1..8 = LP, HP, BP, BS, Analog 1P .. 4P
    double cutoff = 127.0;               // 0..127 (Virus cutoff scale)
    double resonance = 0.0;              // 0..127
    double envAmount = 0.0;              // -64..+63 cutoff steps at full envelope
    double keytrack = 0.0;               // -1..+1 (1 = 100 %)
    double velocity = 0.0;               // -1..+1 velocity -> envelope amount
    std::array<bool, 4> oscOn { true, true, true, true };   // per-oscillator on/off (Virus: Osc 1, 2, 3, Sub/Noise/Ring)
    std::array<bool, 3> virusRetrig { false, false, false }; // Virus mode: restart Osc 1..3 at every note
    std::array<int, 2> virusOscModel { 0, 0 };               // Virus TI models for Osc 1/2: Classic, HyperSaw, Wavetable,
                                                             // Wave PWM, Grain Simple, Grain Complex, Formant Simple, Formant Complex
    double lockHz = 0.0;                 // Phase Lock: below this the unison stack is replaced by one coherent copy (0 = off)
};

/** Linkwitz-Riley 4th-order crossover (two Butterworth SVF stages per side), as in the Reese plugin. */
struct LR4
{
    dsp::Svf lp1, lp2, hp1, hp2;
    void set (double hz, double sr) { for (auto* f : { &lp1, &lp2, &hp1, &hp2 }) f->set (hz, 1.41421356, sr); }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    double low (double x) { return lp2.tick (lp1.tick (x).lp).lp; }
    double high (double x) { return hp2.tick (hp1.tick (x).hp).hp; }
};

/** Linear-attack / exponential-decay ADSR (the Virus envelope shape), with fixed times. */
class Adsr
{
public:
    void setTimes (const AdsrTimes& t, double sr);
    void gateOn (bool fromZero) { if (fromZero) level = 0.0; stage = Stage::Attack; }
    void gateOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void kill() { stage = Stage::Idle; level = 0.0; }
    double tick();
    bool active() const { return stage != Stage::Idle; }
    double value() const { return level; }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Stage::Idle;
    double level = 0.0, attackInc = 1.0, decayCoef = 0.0, releaseCoef = 0.0, sustain = 1.0;
};

/** Per-voice global stage: the global filter (stereo) and the global envelopes. */
class GlobalVoiceStage
{
public:
    void start (int note, float velocity, const GlobalVoiceSettings&, double sr, bool fromZero);
    void release() { ampEnv.gateOff(); filtEnv.gateOff(); }
    void kill() { ampEnv.kill(); filtEnv.kill(); }
    /** Call once per control block (coefficient update). */
    void control (const GlobalVoiceSettings&, double sr, int blockLen);
    /** Per sample: advances the envelopes. Returns the amp envelope value. */
    double tickEnvelopes() { filtEnv.tick(); return ampEnv.tick(); }
    double filterEnvelope() const { return filtEnv.value(); }
    double ampEnvelope() const { return ampEnv.value(); }
    bool ampActive() const { return ampEnv.active(); }
    /** The global filter on one stereo sample (no-op when off). */
    void filter (double& l, double& r);
    /** Phase Lock: high band of the full signal + low band of the coherent copy (lockL/R). */
    void phaseLock (double& l, double& r, double lockL, double lockR)
    {
        if (lockHz < 1.0) return;
        l = lockXL.high (l) + lockCL.low (lockL);
        r = lockXR.high (r) + lockCR.low (lockR);
    }
    bool lockOn() const { return lockHz >= 1.0; }

private:
    Adsr ampEnv, filtEnv;
    dsp::Svf svfL, svfR;
    dsp::Ladder ladL, ladR;
    LR4 lockXL, lockXR, lockCL, lockCR;
    double lockHz = 0, lastLockSr = 0;
    int mode = 0, note = 60;
    float vel = 1.0f;
    double lastSr = 0;
};

/** Saturation on the summed signal (Virus curves), 2x oversampled. */
class GlobalSaturation
{
public:
    void prepare (double sr);
    void reset();
    /** curve: Virus saturation list index (0 = Off). drive 0..1, mix 0..1. */
    void process (float* l, float* r, int n, int curve, double drive, double mix);

private:
    Shaper shL, shR;
    dsp::HalfbandDecimator decL, decR;
    double prevL = 0, prevR = 0, sr = 44100;
    dsp::OnePole dcL, dcR;
    // the dry path is delayed by the decimator's latency so dry/wet mixing doesn't comb
    std::array<float, 16> dryL {}, dryR {};
    int dryPos = 0;
};
} // namespace vc
