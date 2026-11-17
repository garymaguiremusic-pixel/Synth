#pragma once
// Sylenth mode: a clean-room engine with the publicly documented Sylenth1 architecture.
// No LennarDigital code or data is used (the plugin is copy-protected and was not unpacked).
//
//   Part A and Part B, two oscillators each (A1 A2 B1 B2):
//     wave (Sine, Saw, Triangle, Pulse, HPulse, QPulse, TriSaw, Noise), unison voices 0..8
//     (0 = oscillator off), detune, stereo, octave, note, fine, phase, retrigger, invert, volume, pan
//   Filter A / Filter B (Bypass, Lowpass, Bandpass, Highpass; 12 or 24 dB; cutoff, resonance, drive,
//     keytrack) + Filter Control (cutoff / resonance offsets, warm drive, keytrack)
//   Amp envelopes A/B (or the global Amp ADSR), Mod envelopes 1/2 and LFOs 1/2 with two destinations each
//   Mix A/B, mono/legato, portamento, bend range
//
// Parameters are this engine's own (normalised 0..1, see kSyParams). Importing Sylenth presets needs
// Sylenth's VST parameter order, which the parameter probe (tools/vc_probe) produces; the 244 raw
// values of an imported preset are kept so it can be re-mapped once that list exists.

#include "ModeEngine.h"
#include <array>

namespace vc
{
namespace sylenth
{
    enum Dest { DNone, DCutoffA, DCutoffB, DCutoffAB, DResoAB, DPitchA, DPitchB, DPitchAB, DVolumeA, DVolumeB, DVolumeAB,
                DPanAB, DMixAB, DDetuneAB, DPhaseAB, DNumDests };

    struct ParamDef { const char* id; const char* name; float def; int steps; /* 0 = continuous, else list size */ };

    // Parameter indices
    enum P
    {
        // oscillators: 4 x 14 (A1, A2, B1, B2)
        OscBase = 0, OscStride = 14,
        // per oscillator offsets
        oWave = 0, oVoices, oDetune, oStereo, oOctave, oNote, oFine, oPhase, oRetrig, oInvert, oVolume, oPan, oKeytrack, oSpare,
        FiltA = OscBase + 4 * OscStride,    // type, slope, cutoff, reso, drive, keytrack
        FiltB = FiltA + 6,
        FCtl = FiltB + 6,                   // cutoff, reso, warm drive, keytrack
        AmpA = FCtl + 4,                    // A D S R
        AmpB = AmpA + 4,
        Mod1 = AmpB + 4,                    // A D S R dest1 amt1 dest2 amt2
        Mod2 = Mod1 + 8,
        Lfo1 = Mod2 + 8,                    // wave rate sync gain offset dest1 amt1 dest2 amt2
        Lfo2 = Lfo1 + 9,
        MixA = Lfo2 + 9, MixB, Solo, Mono, Legato, Porta, PortaMode, Polyphony, BendRange, MainVol,
        NumParams
    };

    const ParamDef& def (int index);
    const char* const* listItems (int index, int& count);   // for list parameters
}

class SylenthEngine : public ModeEngine
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxUnison = 8;
    static constexpr int kNumParams = sylenth::NumParams;

    SylenthEngine();
    SynthMode mode() const override { return SynthMode::Sylenth; }
    void prepare (double hostRate, int maxBlock) override;
    void reset() override;
    void noteOn (int note, int velocity) override;
    void noteOff (int note) override;
    void pitchBend (int v14) override { bend = (v14 - 8192) / 8192.0; }
    void controller (int cc, int value) override { if (cc == 120 || cc == 123) allNotesOff (cc == 120); (void) value; }
    void allNotesOff (bool hard) override;
    void setTempo (double b) override { bpm = b > 0 ? b : 120.0; }
    void setGlobal (const GlobalVoiceSettings* g) override { global = g; }
    void process (float* L, float* R, int n) override;
    int latencySamples() const override { return 0; }
    int activeVoiceCount() const override;

    void setParam (int i, float v) { if (i >= 0 && i < kNumParams) params[(size_t) i] = std::clamp (v, 0.0f, 1.0f); }
    float getParam (int i) const { return i >= 0 && i < kNumParams ? params[(size_t) i] : 0.0f; }
    void setAllParams (const std::array<float, kNumParams>& a) { params = a; }
    const std::array<float, kNumParams>& getAllParams() const { return params; }
    static std::array<float, kNumParams> initParams();
    static void extractEnvelopes (const std::array<float, kNumParams>&, AdsrTimes& amp, AdsrTimes& filter);

    static int listValue (float v, int count) { return std::clamp ((int) (v * (count - 1) + 0.5f), 0, count - 1); }

private:
    struct Osc { std::array<double, kMaxUnison> phase {}; dsp::Rng rng; };
    struct Env { int stage = 0; double level = 0; };
    struct Voice
    {
        bool active = false, gate = false;
        int note = 60; float vel = 1.0f; uint64_t age = 0;
        double curNote = 60, targetNote = 60;
        std::array<Osc, 4> osc;
        std::array<Env, 4> env;       // amp A, amp B, mod 1, mod 2
        std::array<double, 2> lfoPhase {}, lfoVal {}, lfoSh {};
        dsp::Svf fA[2][2], fB[2][2];  // [stage][channel]
        GlobalVoiceStage gstage;
        int ctl = 0;
        std::array<double, sylenth::DNumDests> mod {};
    };

    void startVoice (Voice&, int note, float vel, bool legato);
    void renderVoice (Voice&, double* L, double* R, int n);
    double envTick (Env&, int base);
    float p (int i) const { return params[(size_t) i]; }

    std::array<float, kNumParams> params {};
    std::array<Voice, kMaxVoices> voices;
    std::vector<double> bufL, bufR;
    double sr = 44100, bpm = 120, bend = 0;
    const GlobalVoiceSettings* global = nullptr;
    uint64_t ageCounter = 0;
    std::vector<int> held;
    dsp::Rng rng;
};
} // namespace vc
