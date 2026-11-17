#pragma once

#include "ColorFilter.h"
#include "DelayLine.h"
#include "../Parameters.h"

namespace vd
{

//==============================================================================
/**
    Effects on the summed delay repeats (all modes):

      Movement: a high-pass whose cutoff is swept by a sine LFO (30 Hz up to
                3 kHz at full Depth). Left and right are a quarter cycle apart.
      Phaser  : a 6-stage all-pass phaser (200 Hz .. 4 kHz sweep at full Depth,
                light feedback), mixed with the repeats to add a phaser layer.
      Flanger : a short modulated delay (0.3 ms .. 5.3 ms at full Depth) with
                feedback, mixed with the repeats for a jet-style comb sweep.

    All three share Rate, Depth and Mix (dry repeats vs. the effected repeats)
    and fade in/out smoothly when switched.
*/
class MovementFX
{
public:
    struct Settings
    {
        bool hpOn = false, phaserOn = false, flangerOn = false;
        float rateHz = 0.5f;
        float depth = 0.6f;    // 0..1
        float mix = 1.0f;      // 0 = untouched repeats, 1 = fully effected
    };

    void prepare (double sampleRate);
    void reset();
    void setParameters (const Settings&) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

    float getPhase() const noexcept { return (float) phase; }

private:
    void updateCoefficients (int channel, float lfo) noexcept;

    static constexpr int numStages = 6;

    double fs = 44100.0, phase = 0.0;
    Settings settings;
    float hpMix = 0.0f, phaserMix = 0.0f, flangerMix = 0.0f, mixSmoothing = 0.001f;
    float depth = 0.6f, rate = 0.5f, mix = 1.0f, paramSmoothing = 0.001f;
    int countdown = 0;
    bool snapOnNextBlock = true;   // start at the current settings instead of fading in after a reset

    TptSvf hp[2];
    float apCoeff[2] {};
    float apState[2][numStages] {};
    float phaserFeedback[2] {};

    DelayLine flangeLines[2];
};

//==============================================================================
/**
    Stereo width of the repeats (all modes), factor 0..2 (knob 0..100):
      0 .. 1 : mid/side, the side signal scaled from 0 (mono) to 1 (as is)
      1 .. 2 : the side is boosted (up to +60 %) and a decorrelated copy of the
               mid (an 11 ms delayed, slightly darkened copy) is added to the
               side, so even mono repeats spread out. Everything added sits in
               the side signal, so the mono sum (L + R) never changes.
*/
class StereoWidth
{
public:
    void prepare (double sampleRate);
    void reset();
    void setWidth (float factor) noexcept { target = std::clamp (factor, 0.0f, 2.0f); }
    void process (float* left, float* right, int numSamples) noexcept;

private:
    double fs = 44100.0;
    float width = 1.0f, target = 1.0f, smoothing = 0.001f;
    bool snapOnNextBlock = true;
    DelayLine midDelay;
    OnePole decorrelationTone;
};

//==============================================================================
/**
    Ducks the repeats while the dry source is playing, then lets them swell back
    into the gaps. The key is the dry input level (5 ms attack, 60 ms release);
    Amount sets how deep the repeats are pushed down (up to -36 dB when the
    source is loud). Gain changes are smoothed: 10 ms down, 150 ms back up.
*/
class Ducker
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParameters (bool enabled, float amount01) noexcept { targetAmount = enabled ? amount01 : 0.0f; }

    /** keyL/keyR = dry input, left/right = repeats (processed in place). */
    void process (const float* keyL, const float* keyR, float* left, float* right, int numSamples) noexcept;

    float getReductionDb() const noexcept { return reductionDb; }

private:
    double fs = 44100.0;
    float envelope = 0.0f, envAttack = 0.0f, envRelease = 0.0f;
    float gain = 1.0f, gainDown = 0.0f, gainUp = 0.0f;
    float amount = 0.0f, targetAmount = 0.0f, amountSmoothing = 0.001f;
    float reductionDb = 0.0f;
};

} // namespace vd
