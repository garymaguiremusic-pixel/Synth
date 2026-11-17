#pragma once

#include "../Parameters.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace vd
{

//==============================================================================
/**
    Delay-time modulation LFO. Produces two bipolar outputs: A, and B which is
    a quarter cycle ahead (used by the second delay line for stereo movement).
    Outputs are lightly smoothed (~3 ms) so Square / Saw / S&H steps don't click
    the delay read head.
*/
class Lfo
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        smoothCoeff = 1.0f - (float) std::exp (-1.0 / (0.003 * fs));
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0;
        held = 0.0f;
        smoothA = smoothB = 0.0f;
    }

    void setShape (int newShape) noexcept { shape = newShape; }

    /** Fills n samples; the rate ramps linearly from rateStart to rateEnd (Hz). */
    void process (float* outA, float* outB, int n, float rateStart, float rateEnd) noexcept
    {
        const float rateStep = (rateEnd - rateStart) / (float) std::max (1, n);
        float rate = rateStart;

        for (int i = 0; i < n; ++i)
        {
            rate += rateStep;
            phase += rate / fs;

            if (phase >= 1.0)
            {
                phase -= std::floor (phase);
                held = random.nextFloat() * 2.0f - 1.0f;
            }

            float a, b;
            if (shape == ShapeSampleHold)
            {
                a = b = held;
            }
            else
            {
                a = shapeValue (shape, phase);
                double pb = phase + 0.25;
                b = shapeValue (shape, pb - std::floor (pb));
            }

            smoothA += (a - smoothA) * smoothCoeff;
            smoothB += (b - smoothB) * smoothCoeff;
            outA[i] = smoothA;
            outB[i] = smoothB;
        }
    }

    double getPhase() const noexcept { return phase; }

    /** Deterministic waveform value for a phase in [0, 1). For S&H this returns
        a fixed pseudo-random step pattern (used by the GUI preview). */
    static float shapeValue (int shape, double phase) noexcept
    {
        const float p = (float) phase;
        switch (shape)
        {
            case ShapeSine:     return std::sin (2.0f * 3.14159265f * p);
            case ShapeTriangle: return 1.0f - 4.0f * std::abs (p - 0.5f);
            case ShapeSaw:      return 2.0f * p - 1.0f;
            case ShapeSquare:   return p < 0.5f ? 1.0f : -1.0f;
            case ShapeSampleHold:
            {
                static constexpr float steps[] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, -0.5f, 0.75f, -0.1f };
                return steps[std::clamp ((int) (p * 8.0f), 0, 7)];
            }
            default:            return 0.0f;
        }
    }

private:
    double fs = 44100.0, phase = 0.0;
    float held = 0.0f, smoothA = 0.0f, smoothB = 0.0f, smoothCoeff = 0.01f;
    int shape = ShapeSine;
    juce::Random random { 0x5eed };
};

} // namespace vd
