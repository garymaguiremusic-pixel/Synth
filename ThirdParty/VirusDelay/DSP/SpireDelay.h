#pragma once

#include "DelayLine.h"
#include "ColorFilter.h"
#include "../Parameters.h"

namespace vd
{

//==============================================================================
/**
    Stereo delay for the "Spire Delay" mode.

    Two lines (L = first, R = second) with independent times. The routing depends
    on the Ping-Pong button and the Wide knob (w = -1 .. +1, centre = 0):

      Ping-Pong off : each channel feeds back into itself.
                      w > 0 normal stereo, w < 0 L/R swapped, w = 0 mono.
      Ping-Pong on  : w < 0 cross feedback (stereo in, each line feeds the other),
                      w > 0 ping pong (mono in -> L -> R -> L ...).
                      |w| sets the stereo width, centre is mono.

    The Color filter (left = hi cut, right = low cut) sits inside the loop, and
    the LFO ("Rate" / "Modulate") moves both read taps. Time changes use a 40 ms
    two-tap crossfade. Buffer is 32 s so 16/1 works at 120 BPM.
*/
class SpireDelay
{
public:
    struct Settings
    {
        float delayMs[2] { 500.0f, 500.0f };
        float feedback = 0.0f;      // 0 .. 0.95
        float wide = 1.0f;          // -1 .. +1
        bool pingPong = false;
        float colorBipolar = 0.0f;  // -1 hi cut .. +1 low cut
        float modDepthMs = 0.0f;
    };

    void prepare (double sampleRate);
    void reset();

    void setParameters (const Settings&) noexcept;
    void setActive (bool shouldBeActive) noexcept { inputGainTarget = shouldBeActive ? 1.0f : 0.0f; }

    void process (const float* inL, const float* inR, float* outL, float* outR,
                  const float* lfoA, const float* lfoB, int numSamples) noexcept;

    float getAndResetLoopPeak() noexcept { const float p = loopPeak; loopPeak = 0.0f; return p; }

private:
    struct Tap
    {
        float current = 0.0f, next = 0.0f, fade = 0.0f;
        bool fading = false;
    };

    inline float readTap (const DelayLine& line, Tap& tap, float target, float mod) noexcept;

    enum Coeff { inAL, inAR, inBL, inBR, fbAA, fbBA, fbAB, fbBB, width, numCoeffs };

    double fs = 44100.0;
    DelayLine lines[2];
    Tap taps[2];
    ColorFilter filters[2];

    float targetSamples[2] {};
    float coeff[numCoeffs] {}, coeffTarget[numCoeffs] {};
    float matrixSmoothing = 0.001f, fadeIncrement = 0.001f;

    float inputGain = 1.0f, inputGainTarget = 1.0f;
    float depthSamples = 0.0f, depthTarget = 0.0f;
    float loopPeak = 0.0f;
    bool snapOnNextBlock = true;
};

} // namespace vd
