#pragma once

#include "DspBuildingBlocks.h"
#include "Filters.h"

namespace lv
{

//==============================================================================
/**
    Mode 2: a clean-room recreation of the two reverb master FX of NI Massive,
    "Reverb" (type 0) and "Small Reverb" / "Reverb Sm" (type 1), from the structure
    and constants in docs/massive_reverb_spec.md (recovered by static analysis of
    Massive; no NI code is used).

    Per sample, in 4 parallel lanes:

      stereo in -> pre-delay (Reverb: 20.2 / 19.8 ms + 3 samples; Small: none)
         lanes 0..3 <- D x (L, R, L, R) through a short (14 ms) allpass diffuser each,
                       lanes 0 / 1 also <- the other one's last output x the feedback
                       gain (cross-coupled, figure-of-eight)
         -> stage 1: modulated allpass per lane (126 / 106 / 89 / 75 ms at full size)
         -> stage 2: modulated allpass per lane (134 / 113 / 95 / 80 ms), opposite LFO
         -> 130.81 Hz high-pass and damping low-pass per lane (TPT SVFs)
         -> lanes 0 / 1 are fed back; all 4 lanes feed 4 long lines (359 / 548 /
            244 / 174 ms) in a Householder FDN
      -> wet out (L: lanes 0 + 2 and FDN 0 + 2; R: the odd ones)

    What comes from the analysis [OBSERVED]: the per-type constants, the delay
    times, the 0.5 Hz LFO with lanes 1/8 cycle apart and its parabolic shape, +m
    on stage 1 and -m on stage 2, the 3-sample offset on lanes 1-3, the lattice
    allpass form, cubic interpolation, the 130.81 Hz high-pass and the damping
    low-pass, lanes 0/1 fed back. What is an estimate, to be refined by
    calibration against Massive (tools/MassiveCalibrate.cpp) [UNKNOWN]: the knob
    curves, allpass gains, filter Q, how the long and short lines are routed and
    tapped, and the output mix. Those live in Internals.
*/
class MassiveReverb
{
public:
    enum Type { Reverb = 0, Small = 1 };

    /** Massive's three effect knobs, 0..1. */
    struct Knobs
    {
        float size = 0.5f, density = 0.5f, color = 0.5f;
    };

    /** Everything the analysis could not pin down, per type. Defaults are estimates. */
    struct Internals
    {
        float rtMin = 0.8f, rtMax = 7.0f;         // RT60 (s) at Size 0 and 100 (exponential between)
        float sizeMin = 0.3f, sizeMax = 1.0f;     // delay-length scale at Size 0 and 100
        float apGainMin = 0.15f, apGainMax = 0.8f;  // allpass gain at Density 0 and 100
        float apGain0Ratio = 0.9f;                // lane 0's allpass gain relative to the others
        float lfoShape = 16.0f;                   // k in m = (8 - k|p|) p 0.0005
        float hpQ = 0.7071f, lpQ = 0.7071f;
        float colorOctaves = 2.0f;                // Color moves the damping corner +-this many octaves
        float diffuserRatio = 1.0f;               // short-line diffuser gain relative to the stage allpasses
        float laneMix = 0.7f, fdnMix = 0.5f;
        float outputGain = 9.5f;
    };

    static Internals defaultInternals (int type);

    /** The RT60 a Size (0..1) asks for with the default internals, in seconds. */
    static float tailSeconds (float size, int type)
    {
        const auto in = defaultInternals (type);
        return in.rtMin * std::pow (in.rtMax / in.rtMin, std::clamp (size, 0.0f, 1.0f));
    }

    void prepare (double sampleRate, int type);
    void reset();

    void setKnobs (const Knobs&) noexcept;
    void setInternals (const Internals&) noexcept;
    const Internals& getInternals() const noexcept { return internals; }

    /** The RT60 the current Size asks for, in seconds. */
    float getTargetRt60() const noexcept { return targetRt60; }
    float getPredelayMs() const noexcept;

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    /** Per-type constants recovered from Massive [OBSERVED]. */
    struct TypeConstants
    {
        float predelayL, predelayR;   // seconds (+ 3 samples); unused when bypassPredelay
        bool bypassPredelay;
        float lowCutHz, dampHz;       // "A"
        float gainE, gainD;           // decay-gain ceiling, input gain
        float coeffB[4], coeffC[4];   // knob-curve polynomials (their use is not known yet; kept for calibration)
    };
    static const TypeConstants& constants (int type);

    static constexpr int kLanes = 4;

private:
    void updateCoefficients() noexcept;

    struct ModAllpass
    {
        DelayLine line;
        inline float process (float x, float delaySamples, float g)
        {
            const float yd = line.readCubic (delaySamples);
            const float v = x - g * yd;
            line.write (v);
            return yd + g * v;
        }
    };

    int type = Reverb;
    float fs = 48000.0f;
    Knobs knobs, smoothed;
    Internals internals;
    float targetRt60 = 2.0f, smoothing = 0.001f;

    DelayLine predelay[2];
    int predelaySamples[2] { 2, 2 };

    ModAllpass stage1[kLanes], stage2[kLanes];
    float base1[kLanes] {}, base2[kLanes] {};   // seconds at full size
    TptSvf highPass[kLanes], lowPass[kLanes];

    DelayLine longLine[kLanes];
    float longMax[kLanes] {};                   // samples at full size
    OnePoleLP longDamp[kLanes];
    float longGain[kLanes] {};

    ModAllpass shortAp[kLanes];
    float shortMax[kLanes] {};

    float lfoPhase = 0.0f, lfoStep = 0.0f;
    float fb0 = 0.0f, fb1 = 0.0f, fbGain = 0.0f, apGain = 0.5f, apGain0 = 0.45f, sizeScale = 1.0f;
    int coefCountdown = 0;
};

} // namespace lv
