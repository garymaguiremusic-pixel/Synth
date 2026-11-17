#pragma once
// The whole VC-Clone instrument without the JUCE-hosted parts (arp / step sequencer, delay, reverb):
// three synth modes (Virus, Sylenth, Spire), one active at a time, plus the global sections that every
// mode shares - Filter ADSR, Amp ADSR (both BPM-syncable), the global filter and the saturation.
// No JUCE dependency.

#include "HostEngine.h"
#include "ModeEngine.h"
#include "SpireEngine.h"
#include "SylenthEngine.h"
#include "vc/PresetIO.h"
#include <memory>

namespace vc
{
/** The Virus engine as a mode: HostEngine (46.875 kHz internal, resampled) with its own FX and arp off. */
class VirusModeEngine : public ModeEngine
{
public:
    VirusModeEngine()
    {
        host.synth.setInternalFx (false);
        host.synth.setInternalArp (false);
        host.synth.setPatch (makeInitPatch().raw.data());
    }
    SynthMode mode() const override { return SynthMode::Virus; }
    void prepare (double r, int b) override { host.prepare (r, b); }
    void reset() override { host.reset(); }
    void noteOn (int n, int v) override { host.synth.noteOn (n, v); }
    void noteOff (int n) override { host.synth.noteOff (n); }
    void pitchBend (int v) override { host.synth.pitchBend (v); }
    void controller (int cc, int v) override { host.synth.controller (cc, v); }
    void channelPressure (int v) override { host.synth.channelPressure (v); }
    void allNotesOff (bool hard) override { host.synth.allNotesOff (hard); }
    void setTempo (double bpm) override { host.synth.setHostTempo (bpm); }
    void setGlobal (const GlobalVoiceSettings* g) override { host.synth.setGlobal (g); }
    void process (float* L, float* R, int n) override { host.process (L, R, n); }
    int latencySamples() const override { return host.latencySamples(); }
    int activeVoiceCount() const override { return host.synth.activeVoiceCount(); }
    Synth& synth() { return host.synth; }

private:
    HostEngine host;
};

/** User-facing values of the global section (what the knobs say). */
struct GlobalParams
{
    struct Env
    {
        double attackMs = 1, decayMs = 300, sustain = 1.0, releaseMs = 200;   // free times
        bool sync = false;
        int attackDiv = 0, decayDiv = 9, releaseDiv = 6;                         // SyncDivisions indices
    };
    Env amp, filter;
    bool useGlobalEnvelopes = true;      // off: each mode uses its patch's own envelopes

    int filterMode = 0;                  // 0 off, 1..8 Virus filter types
    double cutoff = 127, resonance = 0, envAmount = 0, keytrack = 0, velocity = 0;

    int saturationCurve = 0;             // Virus saturation list (0 = off)
    double saturationDrive = 0.3, saturationMix = 1.0;
    double volume = 1.0;
    double monoHz = 0.0;                 // Mono: below this the output is summed to mono (0 = off)
    double lockHz = 0.0;                 // Phase Lock (see GlobalVoiceSettings)
    std::array<bool, 4> oscOn { true, true, true, true };
    std::array<bool, 3> virusRetrig { false, false, false };
};

class Instrument
{
public:
    Instrument();

    void prepare (double hostRate, int maxBlock);
    void reset();

    void setMode (SynthMode);
    SynthMode getMode() const { return modeIndex; }
    ModeEngine& current() { return *engines[(size_t) modeIndex]; }
    VirusModeEngine& virus() { return *virusEngine; }
    SylenthEngine& sylenth() { return *sylenthEngine; }
    SpireEngine& spire() { return *spireEngine; }

    GlobalParams params;
    void setTempo (double bpm);

    void noteOn (int n, int v) { current().noteOn (n, v); }
    void noteOff (int n) { current().noteOff (n); }
    void pitchBend (int v) { current().pitchBend (v); }
    void controller (int cc, int v) { current().controller (cc, v); }
    void channelPressure (int v) { current().channelPressure (v); }
    void allNotesOff (bool hard) { current().allNotesOff (hard); }

    /** Renders n host-rate samples: the active mode, then the saturation. */
    void process (float* L, float* R, int n);

    int latencySamples() { return current().latencySamples(); }
    int activeVoiceCount() { return current().activeVoiceCount(); }

    /** Converts the knob values into the per-voice settings at the current tempo. */
    static GlobalVoiceSettings makeVoiceSettings (const GlobalParams&, double bpm);
    static double envTimeSeconds (double ms, bool sync, int div, double bpm);

private:
    std::unique_ptr<VirusModeEngine> virusEngine;
    std::unique_ptr<SylenthEngine> sylenthEngine;
    std::unique_ptr<SpireEngine> spireEngine;
    std::array<ModeEngine*, 3> engines {};
    SynthMode modeIndex = SynthMode::Virus;
    LR4 monoL, monoR;
    double monoSetHz = -1, sampleRate = 44100;
    GlobalVoiceSettings voiceSettings;
    GlobalSaturation saturation;
    double bpm = 120.0;
};

inline Instrument::Instrument()
    : virusEngine (std::make_unique<VirusModeEngine>()),
      sylenthEngine (std::make_unique<SylenthEngine>()),
      spireEngine (std::make_unique<SpireEngine>())
{
    engines = { virusEngine.get(), sylenthEngine.get(), spireEngine.get() };
    voiceSettings = makeVoiceSettings (params, bpm);
    for (auto* e : engines) e->setGlobal (&voiceSettings);
}
} // namespace vc
