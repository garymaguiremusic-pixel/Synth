#pragma once
// Spire mode: a clean-room engine with Spire's parameter set (725 normalised values, the layout
// of Spire's own presets: reference/spire_param_map.csv). No Reveal Sound code or data tables are
// used; every behaviour here is written from the documented feature set and is to be calibrated
// by measuring the real plugin, like the Virus mode.
//
// Voice: 4 oscillators (Classic, Noise, FM, AMSync, SawPWM, HardFM, Vowel; 1..9 unison voices,
// detune / width / density / chord) -> 2 filters (Perfecto, Acido, Infecto, Scorpio, Combo,
// Shaper; serial or parallel, balance, keytrack) -> global filter -> amp (Env 1 = VCA, or the
// global Amp ADSR). 4 envelopes and 4 LFOs; their modulation targets use Spire's target IDs,
// which are decoded where the ID is known (see SpireTargets in the .cpp).

#include "ModeEngine.h"
#include "SpireLayout.h"
#include <array>
#include <atomic>

namespace vc
{
class SpireEngine : public ModeEngine
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxUnison = 9;

    SpireEngine();
    SynthMode mode() const override { return SynthMode::Spire; }
    void prepare (double hostRate, int maxBlock) override;
    void reset() override;
    void noteOn (int note, int velocity) override;
    void noteOff (int note) override;
    void pitchBend (int v14) override { bend = (v14 - 8192) / 8192.0; }
    void controller (int cc, int value) override;
    void channelPressure (int v) override { aftertouch = v / 127.0; }
    void allNotesOff (bool hard) override;
    void setTempo (double b) override { bpm = b > 0 ? b : 120.0; }
    void setGlobal (const GlobalVoiceSettings* g) override { global = g; }
    void process (float* L, float* R, int n) override;
    int latencySamples() const override { return 0; }
    int activeVoiceCount() const override;

    /** Normalised 0..1 parameter values, exactly as stored in a Spire preset. */
    void setParam (int index, float v) { if (index >= 0 && index < spire::kNumParams) params[(size_t) index] = std::clamp (v, 0.0f, 1.0f); }
    float getParam (int index) const { return index >= 0 && index < spire::kNumParams ? params[(size_t) index] : 0.0f; }
    void setAllParams (const std::array<float, spire::kNumParams>& p) { params = p; }
    const std::array<float, spire::kNumParams>& getAllParams() const { return params; }
    static std::array<float, spire::kNumParams> initParams();

    /** The patch's arp settings and steps, for the global step sequencer. */
    static PatchSteps extractSteps (const std::array<float, spire::kNumParams>&);
    /** The patch's Env1 (amp) and Env2 as global ADSR times. */
    static void extractEnvelopes (const std::array<float, spire::kNumParams>&, AdsrTimes& amp, AdsrTimes& filter);

    // value decoders (Spire's storage conventions)
    static int listIndex (float v) { return (int) (v * 999.99f); }
    static bool flag (float v) { return (int) (v * 1.99f) != 0; }

public:
    static constexpr int kNumTargets = 230;

private:
    struct Unison { double phase = 0, phase2 = 0, detune = 0, pan = 0, gain = 1, fb = 0; };
    struct Osc { std::array<Unison, kMaxUnison> u; int count = 1; double drift = 0; dsp::OnePole noiseLp; };
    struct Env { int stage = 0; double level = 0; };
    struct VoiceLfo { double phase = 0, value = 0, sh = 0, fade = 0; };

    struct Voice
    {
        bool active = false, gate = false;
        int note = 60; float vel = 1.0f; uint64_t age = 0;
        double curNote = 60, targetNote = 60;
        std::array<Osc, 4> osc;
        std::array<Env, 4> env;
        std::array<VoiceLfo, 4> lfo;
        dsp::Svf f1a[2], f1b[2], f2a[2], f2b[2];
        dsp::Ladder lad1[2], lad2[2];
        dsp::DelayLine comb1[2], comb2[2];
        GlobalVoiceStage gstage;
        int ctl = 0;
        double modCut1 = 0, modCut2 = 0, modRes1 = 0, modPitch[4] {}, modVol = 0, modCtrlA[4] {}, modCtrlB[4] {}, modWt[4] {};
        std::array<double, kNumTargets> mod {};    // summed modulation per Spire target (SpireLists.h: targets)
        double randBi = 0, randUni = 0;            // per-note random sources
        dsp::Rng rng;
    };

    void startVoice (Voice&, int note, float vel, bool legato);
    void renderVoice (Voice&, double* L, double* R, int n);
    void controlVoice (Voice&);
    double envTick (Voice&, int e);
    float p (int i) const { return params[(size_t) i]; }

    std::array<float, spire::kNumParams> params {};
    std::array<Voice, kMaxVoices> voices;
    std::vector<double> bufL, bufR;
    double sr = 44100, bpm = 120, bend = 0, modWheel = 0, aftertouch = 0, foot = 0, breath = 0, expression = 0, sustainPedal = 0;
    const GlobalVoiceSettings* global = nullptr;
    uint64_t ageCounter = 0;
    std::vector<int> held;
    dsp::Rng rng;
};
} // namespace vc
