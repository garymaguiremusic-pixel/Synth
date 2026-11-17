#pragma once

#include "../Parameters.h"
#include <cmath>

namespace vd
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

    float g = 0.0f, k = 1.414f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

//==============================================================================
/**
    The "Color" filter that lives inside the feedback loop.
    bipolar < 0 : 12 dB/oct low-pass sweeping 20 kHz -> 250 Hz (darker repeats)
    bipolar = 0 : exactly flat
    bipolar > 0 : 12 dB/oct high-pass sweeping 20 Hz -> 4 kHz (brighter/thinner repeats)

    Both filters run in series all the time and are faded in near the centre, so
    sweeping through the centre is click-free. The colour value is smoothed per
    sample; coefficients are recomputed every 16 samples.
*/
class ColorFilter
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        smoothCoeff = 1.0f - (float) std::exp (-1.0 / (0.02 * fs));
        reset();
    }

    void reset() noexcept
    {
        lp.reset();
        hp.reset();
        current = target;
        countdown = 0;
    }

    void setColor (float bipolar) noexcept { target = std::clamp (bipolar, -1.0f, 1.0f); }

    inline float process (float x) noexcept
    {
        current += (target - current) * smoothCoeff;

        if (--countdown <= 0)
            updateCoefficients();

        float lpOut, hpOut, unused;
        lp.tick (x, lpOut, unused);
        const float y1 = x + lpMix * (lpOut - x);
        hp.tick (y1, unused, hpOut);
        return y1 + hpMix * (hpOut - y1);
    }

private:
    void updateCoefficients() noexcept
    {
        countdown = 16;
        const float nyquistLimit = (float) (0.45 * fs);
        lp.set (std::min (colorLowpassHz (current), nyquistLimit), 0.707f, fs);
        hp.set (std::min (colorHighpassHz (current), nyquistLimit), 0.6f, fs);
        lpMix = std::clamp (-current / 0.1f, 0.0f, 1.0f);
        hpMix = std::clamp ( current / 0.1f, 0.0f, 1.0f);
    }

    TptSvf lp, hp;
    double fs = 44100.0;
    float target = 0.0f, current = 0.0f, smoothCoeff = 0.001f;
    float lpMix = 0.0f, hpMix = 0.0f;
    int countdown = 0;
};

//==============================================================================
/**
    Band-pass used in the tape feedback loop ("Center Frequency" / "Bandwidth").
    A 12 dB/oct high-pass at the lower edge and a 12 dB/oct low-pass at the
    upper edge, with the edges at centre / 2^(bw/2) and centre * 2^(bw/2).
    At 10 octaves the band covers the whole audio range (effectively flat).
    Both edges are Butterworth (no resonant peak), so the filter never adds
    gain and the feedback loop always decays.
*/
class BandFilter
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        smoothCoeff = 1.0f - (float) std::exp (-1.0 / (0.03 * fs));
        reset();
    }

    void reset() noexcept
    {
        hp.reset();
        lp.reset();
        logCentre = logCentreTarget;
        octaves = octavesTarget;
        countdown = 0;
    }

    void setBand (float centreHz, float bandwidthOctaves) noexcept
    {
        logCentreTarget = std::log2 (std::max (20.0f, centreHz));
        octavesTarget = std::clamp (bandwidthOctaves, 0.1f, 12.0f);
    }

    inline float process (float x) noexcept
    {
        logCentre += (logCentreTarget - logCentre) * smoothCoeff;
        octaves   += (octavesTarget - octaves) * smoothCoeff;

        if (--countdown <= 0)
            updateCoefficients();

        float lpOut, hpOut, unused;
        hp.tick (x, unused, hpOut);
        lp.tick (hpOut, lpOut, unused);
        return lpOut;
    }

private:
    void updateCoefficients() noexcept
    {
        countdown = 16;
        const float nyquistLimit = (float) (0.45 * fs);
        const float lowEdge  = std::clamp (std::exp2 (logCentre - 0.5f * octaves), 10.0f, nyquistLimit);
        const float highEdge = std::clamp (std::exp2 (logCentre + 0.5f * octaves), 20.0f, nyquistLimit);
        hp.set (lowEdge, 0.7071f, fs);
        lp.set (highEdge, 0.7071f, fs);
    }

    TptSvf hp, lp;
    double fs = 44100.0;
    float logCentre = 10.0f, logCentreTarget = 10.0f;
    float octaves = 10.0f, octavesTarget = 10.0f;
    float smoothCoeff = 0.001f;
    int countdown = 0;
};

} // namespace vd
