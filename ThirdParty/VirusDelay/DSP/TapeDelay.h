#pragma once

#include "DelayLine.h"
#include "ColorFilter.h"
#include "../Parameters.h"
#include <juce_core/juce_core.h>

namespace vd
{

//==============================================================================
/**
    Tape-style echo used by the Tape Clocked, Tape Free and Tape Doppler modes.

    Each channel has a single read head trailing the write head by `delay`
    samples. The delay never jumps: it is driven by a continuously variable,
    smoothed and slew-limited head speed, so changing the clock (or the host
    tempo) speeds the tape up or slows it down, which is heard as a pitch glide
    on the repeats. Read speed = 1 - d(delay)/dt.

    Tape Doppler uses a much faster, wider head response (bigger pitch swoops),
    and its Modulation moves the two heads in opposite directions with a level
    change, like a sound source moving past the listener. In the other tape
    modes Modulation is wow (slow), flutter (fast) and a slow random drift.

    Inside the feedback loop: the Center Frequency / Bandwidth band-pass, a
    gentle HF roll-off, a low cut (tape bass loss, also blocks DC) and tanh
    saturation on the record path.
*/
class TapeDelay
{
public:
    struct Settings
    {
        float delayMs[2] { 500.0f, 500.0f };
        float feedback = 0.0f;       // 0 .. 0.95
        float centreHz = 800.0f;
        float bandwidthOctaves = 10.0f;
        float modulation = 0.0f;     // 0 .. 1
        bool doppler = false;
    };

    void prepare (double sampleRate);
    void reset();

    void setParameters (const Settings&) noexcept;
    void setActive (bool shouldBeActive) noexcept { inputGainTarget = shouldBeActive ? 1.0f : 0.0f; }

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    float getCurrentDelayMs (int channel) const noexcept { return (float) (delay[channel] * 1000.0 / fs); }
    /** Relative read-head speed: 1 = normal pitch, < 1 = pitch falling, > 1 = rising. */
    float getSpeedRatio() const noexcept     { return (float) (1.0 - velocity[0]); }
    float getModPhase() const noexcept       { return (float) (doppler ? sweepPhase : wowPhase); }
    float getAndResetLoopPeak() noexcept     { const float p = loopPeak; loopPeak = 0.0f; return p; }

private:
    static inline float saturate (float x) noexcept
    {
        constexpr float drive = 1.4f;
        return std::tanh (x * drive) * (1.0f / drive);
    }

    struct Glide { double rate, velocitySmoothing, maxVelocity; };

    double fs = 44100.0;
    DelayLine lines[2];
    BandFilter band[2];
    OnePole hfRollOff[2], lowCut[2];

    double delay[2] {}, velocity[2] {}, target[2] {};
    Glide tapeGlide {}, dopplerGlide {};
    double minDelay = 3.0, maxDelay = 1000.0;

    // modulation oscillators
    double wowPhase = 0.0, flutterPhase = 0.0, sweepPhase = 0.0;
    float drift = 0.0f, driftTarget = 0.0f, driftSmoothing = 0.0f;
    int driftCountdown = 0;
    juce::Random random { 0x7a9e };

    float feedback = 0.0f, feedbackTarget = 0.0f;
    float modulation = 0.0f, modulationTarget = 0.0f;
    float inputGain = 1.0f, inputGainTarget = 1.0f, gainSmoothing = 0.001f;
    float loopPeak = 0.0f;
    bool doppler = false;
    bool snapOnNextBlock = true;
};

} // namespace vd
