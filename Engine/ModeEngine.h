#pragma once
// Common interface of the three synth modes (Virus, Sylenth, Spire).
// Each mode renders oscillators + its own filters + its own modulation; the global filter,
// the global Filter/Amp ADSR (GlobalVoiceSettings) and everything after the voices
// (saturation, delay, reverb) are shared. No JUCE dependency.

#include "Global.h"
#include <cstdint>
#include <string>
#include <vector>

namespace vc
{
class ModeEngine
{
public:
    virtual ~ModeEngine() = default;

    virtual SynthMode mode() const = 0;
    virtual void prepare (double hostRate, int maxBlock) = 0;
    virtual void reset() = 0;

    virtual void noteOn (int note, int velocity) = 0;
    virtual void noteOff (int note) = 0;
    virtual void pitchBend (int value14) = 0;          // 0..16383, 8192 = centre
    virtual void controller (int cc, int value) = 0;
    virtual void channelPressure (int) {}
    virtual void allNotesOff (bool hard) = 0;

    virtual void setTempo (double bpm) = 0;
    /** The shared global settings; the pointer stays valid while the engine lives. */
    virtual void setGlobal (const GlobalVoiceSettings* g) = 0;

    /** Renders n samples at the host rate (overwrites L/R). */
    virtual void process (float* L, float* R, int n) = 0;

    virtual int latencySamples() const = 0;
    virtual int activeVoiceCount() const = 0;
};

/** Steps of a patch's own arp / step sequencer, handed to the global step sequencer on load. */
struct PatchSteps
{
    bool present = false;          // the patch carries step data
    bool arpOn = false;
    int mode = 0;                  // ArpSampler/Spire arp mode list: Up, Down, UpDown, UpDown2, DownUp, DownUp2, Random, Ordered, Step, Chord
    int octaves = 1;               // 1..4
    int length = 16;               // steps that play
    int rateIndex = 3;             // 0 = 1/2, 1 = 1/4, 2 = 1/8, 3 = 1/16, 4 = 1/32
    int rateType = 0;              // 0 straight, 1 triplet, 2 dotted
    double gate = 0.75;            // 0.01 .. 2 of a step
    double swing = 0.0;            // -1 .. 1
    int velocityMode = 0;          // Key, Hold, Step, StepKey, StepHold
    struct Step { int transpose = 0; int velocity = 100; bool tie = false; };
    std::vector<Step> steps;       // up to 32
};
} // namespace vc
