#pragma once

#include <algorithm>
#include <cmath>

namespace lv
{

//==============================================================================
/** Topology-preserving-transform state variable filter (Zavalishin). */
struct TptSvf
{
    void set (float cutoffHz, float q, double sampleRate) noexcept
    {
        g  = (float) std::tan (3.14159265358979323846 * cutoffHz / sampleRate);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    inline void tick (float v0, float& lp, float& hp) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        hp = v0 - k * v1 - v2;
    }

    /** Low-pass output only. */
    inline float tick2 (float v0) noexcept
    {
        float lp, hp;
        tick (v0, lp, hp);
        return lp;
    }

    float g = 0.0f, k = 1.414f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

//==============================================================================
/**
    "Hi Shelf" boost on the reverb output: RBJ high shelf at 4 kHz, 0 .. +12 dB.
    The gain is smoothed per sample and the coefficients are recomputed every
    16 samples, so turning the knob never zippers.
*/
class HighShelf
{
public:
    static constexpr float kFrequencyHz = 4000.0f;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        smoothCoeff = 1.0f - (float) std::exp (-1.0 / (0.02 * fs));
        reset();
    }

    void reset() noexcept
    {
        s1 = s2 = 0.0f;
        current = target;
        countdown = 0;
    }

    void setGainDb (float db) noexcept { target = std::clamp (db, 0.0f, 24.0f); }

    inline float process (float x) noexcept
    {
        current += (target - current) * smoothCoeff;

        if (--countdown <= 0)
            updateCoefficients();

        const float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

private:
    void updateCoefficients() noexcept
    {
        countdown = 16;

        // shelf slope S = 1
        const double A = std::pow (10.0, current / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * std::min ((double) kFrequencyHz, 0.45 * fs) / fs;
        const double cw = std::cos (w0);
        const double alpha = std::sin (w0) / 2.0 * std::sqrt (2.0);
        const double sqrtA2alpha = 2.0 * std::sqrt (A) * alpha;

        const double a0 = (A + 1.0) - (A - 1.0) * cw + sqrtA2alpha;
        b0 = (float) (A * ((A + 1.0) + (A - 1.0) * cw + sqrtA2alpha) / a0);
        b1 = (float) (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0);
        b2 = (float) (A * ((A + 1.0) + (A - 1.0) * cw - sqrtA2alpha) / a0);
        a1 = (float) (2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0);
        a2 = (float) (((A + 1.0) - (A - 1.0) * cw - sqrtA2alpha) / a0);
    }

    double fs = 44100.0;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
    float target = 0.0f, current = 0.0f, smoothCoeff = 0.001f;
    int countdown = 0;
};

} // namespace lv
