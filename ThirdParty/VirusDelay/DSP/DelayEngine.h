#pragma once

#include "DelayLine.h"
#include "ColorFilter.h"
#include "../Parameters.h"

namespace vd
{

//==============================================================================
/**
    Digital delay engine for the Classic types (Simple, Ping Pong and Pattern).

    Two delay lines (A and B) are connected through a smoothed routing matrix:

      Simple   : A = left, B = right, each feeding back into itself.
      PingPong : mono input -> A (left) -> B (right) -> back into A. The L/R
                 times follow the chosen ratio, repeats bounce between sides.
      Pattern  : same A -> B -> A chain but with the two pattern lengths, so the
                 repeats alternate between the two lengths (e.g. 3,4,3,4 ...).
                 Both lines are mixed to both sides with a soft spread.

    Because the matrix is smoothed, switching between these modes is click-free.
    Delay time changes are handled with a 40 ms two-tap crossfade (no pitch
    artefacts), while the LFO modulates the tap position continuously.
*/
class DelayEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Call once per block (audio thread). */
    void setParameters (int classicType, float baseDelayMs, float feedback, float colorBipolar, float modDepthMs) noexcept;

    /** An inactive engine receives no new input; its tail decays naturally. */
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

    enum Coeff { inAL, inAR, inBL, inBR, fbAA, fbBA, fbAB, fbBB, outLA, outLB, outRA, outRB, numCoeffs };

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
