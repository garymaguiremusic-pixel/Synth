#pragma once
// VC-Clone engine: a clean-room Virus C style single-part synth.
// No JUCE dependency. Internal rate is fixed at 46.875 kHz (the Virus C rate); voices run 2x oversampled
// (88.2 kHz) and are decimated before the FX chain. HostEngine (Resampler.h)
// converts to the host rate.

#include "Dsp.h"
#include "Fx.h"
#include "Global.h"
#include "Tables/Tables.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace vc
{
constexpr int kMaxVoices = 32;
constexpr int kOversample = 2;
constexpr int kControlBlock = 16;                       // internal samples per control tick
constexpr double kVoiceRate = tables::kInternalRate * kOversample;

/** Parameter store: target values (what the patch says) and smoothed current values. */
struct ParamState
{
    std::array<float, 256> target {}, cur {};
    std::array<bool, 256> continuous {};
    float operator[] (int o) const { return cur[(size_t) o]; }
    int i (int o) const { return (int) std::lround (cur[(size_t) o]); }
};

/** Part-level modulation sources shared by all voices. */
struct PartSources
{
    float pitchBend = 0.0f;       // -1..1
    float chanPressure = 0.0f;    // 0..1
    std::array<float, 128> cc {}; // 0..1
};

struct Lfo
{
    double phase = 0.0, rateHz = 1.0;
    float value = 0.0f, sh = 0.0f, shPrev = 0.0f, sgCur = 0.0f;
    bool oneShotDone = false;
    double fadeTime = 0.0, fadeElapsed = 0.0;
    void retrigger (double startPhase) { phase = startPhase - std::floor (startPhase); oneShotDone = false; fadeElapsed = 0.0; }
    /** Advances by `seconds`, returns -1..1. */
    float advance (double seconds, int shape, float symmetry /* -1..1 */, bool envMode, dsp::Rng& rng);
};

struct Envelope
{
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Stage::Idle;
    double level = 0.0;
    double attackInc = 1.0, decayCoef = 0.0, releaseCoef = 0.0, sustain = 1.0, slopeMulPerSample = 1.0;
    void setup (double a, double d, double s, int sustainTime, double r, double sr);
    void gateOn (bool retriggerFromZero) { if (retriggerFromZero) level = 0.0; stage = Stage::Attack; }
    void gateOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    double tick();
    bool active() const { return stage != Stage::Idle; }
};

class Synth;

class Voice
{
public:
    void start (Synth&, int note, float velocity, int unisonIndex, int unisonCount, double glideFromNote, bool retrigger, uint64_t age);
    void release();
    void kill() { active = false; }
    void legatoTo (int note, double glideFromCurrent);
    /** Renders numSamples at the voice rate (88.2 kHz), adding into L/R. */
    void render (Synth&, double* L, double* R, int numSamples);

    bool active = false, gate = false;
    int note = -1, unisonIndex = 0, unisonCount = 1;
    uint64_t age = 0;
    bool isReleasing() const { return active && ! gate; }

private:
    void controlTick (Synth&, int blockLen);

    float velocity = 1.0f, randomSrc = 0.0f;
    double curNote = 60.0, targetNote = 60.0, glideCoef = 0.0;
    double ph1 = 0, ph2 = 0, ph3 = 0, subPh = 0; bool subOdd = false;
    Envelope ampEnv, filtEnv;
    Lfo lfo[3];
    double lfoVal[3] {};
    dsp::Svf f1a, f1b, f2a, f2b;
    dsp::Ladder ladder;
    dsp::OnePole noiseLp, noiseHp;
    Shaper satShaper;
    GlobalVoiceStage gstage;      // global filter (used when Synth::global is set)
    double punchEnv = 0.0, punchCoef = 0.0;
    dsp::Rng rng;
    double timeSinceOn = 0.0;

    // per-control-block derived values (start/end for interpolation)
    struct Ctl
    {
        double f1 = 0, f2 = 0, f3 = 0;               // Hz
        double shape1 = 64, shape2 = 64, duty1 = .5, duty2 = .5;
        int wave1 = 0, wave2 = 0, osc3Mode = 0;
        double fm = 0; int fmMode = 0;
        double g1 = 1, g2 = 0, gSub = 0, gOsc3 = 0, gNoise = 0, gRing = 0, gMain = 1, drive = 1;
        double noiseColor = 0;
        double cut1 = 1000, cut2 = 1000, damp1 = 2, damp2 = 2, ladderK = 0;
        double filtGain = 1;
        double panL = 0.707, panR = 0.707, level = 1;
        int f1Mode = 0, f2Mode = 0, routing = 0, satCurve = 0, subShape = 0;
        double satAmount = 0, balance = 0.5;
        bool sync = false;
    } ctl, prevCtl;
    std::array<float, 256> mod {};
    int ctlCounter = 0;
    bool firstCtl = true;
};

class Synth
{
public:
    Synth();

    void setPatch (const uint8_t* raw256);
    void setParam (int offset, int value);           // 0..127 raw value (clamped to the param range)
    int getParam (int offset) const;
    const std::array<uint8_t, 256>& getRaw() const { return raw; }

    void noteOn (int note, int velocity);
    void noteOff (int note);
    void pitchBend (int value14);                    // 0..16383, 8192 = centre
    void controller (int cc, int value);
    void channelPressure (int value);
    void allNotesOff (bool hard = false);
    void setHostTempo (double bpm) { hostBpm = bpm; }

    /** Modes integration: shared global section (null = stand-alone Virus behaviour), and whether
        the Virus's own effects and arpeggiator run (VC-Clone uses its global FX and arp instead). */
    void setGlobal (const GlobalVoiceSettings* g) { global = g; }
    void setInternalFx (bool on) { internalFx = on; }
    void setInternalArp (bool on) { internalArp = on; if (! on) arpLatched.clear(); }
    const GlobalVoiceSettings* global = nullptr;
    void seed (uint64_t s);
    /** Back to a clean start (no voices, FX tails, LFO phases or glide history; RNG re-seeded). */
    void reset();

    /** Renders at the internal rate (tables::kInternalRate). */
    void renderInternal (float* L, float* R, int numSamples);

    int activeVoiceCount() const;
    double tempoBpm() const;
    double lfoDisplayPhase (int i) const { return monoLfoDisplay[i]; }
    int latencyInternalSamples() const { return dsp::HalfbandDecimator::latencyOut(); }

    // accessed by Voice
    ParamState p;
    PartSources src;
    dsp::Rng rng;
    Lfo monoLfo[3];
    double monoLfoVal[3] {};
    std::atomic<double> monoLfoDisplay[3] {};
    double lastNoteForGlide = -1.0;

private:
    friend class Voice;
    void startNote (int note, int velocity, bool fromArp);
    void stopNote (int note, bool fromArp);
    Voice* allocVoice();
    int unisonCount() const;
    bool isMono() const;
    void controlTick();
    void arpTick (int numSamples);
    void applyParamEffects (int offset);

    std::array<uint8_t, 256> raw {};
    uint64_t seedValue = 1;
    std::array<Voice, kMaxVoices> voices;
    uint64_t ageCounter = 0;
    double hostBpm = 0.0;

    std::vector<int> heldNotes;      // physical keys (in order)
    std::vector<int> monoStack;
    std::vector<int> holdNotes;      // Hold key mode
    int currentMonoNote = -1;
    std::array<int, 128> noteVelocity {};

    // arpeggiator
    std::vector<int> arpNotes;
    std::vector<int> arpLatched;
    double arpPhase = 0.0;
    int arpStep = 0, arpPlaying = -1, arpDir = 1;
    double arpNoteRemaining = 0.0;
    bool arpWasActive = false, arpFresh = true;

    std::array<double, kControlBlock * kOversample> bufL {}, bufR {};
    dsp::HalfbandDecimator decL, decR;
    int ctlPos = 0;

    FxChain fx;
    bool internalFx = true, internalArp = true;
};
} // namespace vc
