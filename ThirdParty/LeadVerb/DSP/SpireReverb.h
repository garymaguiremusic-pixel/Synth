#pragma once

#include "SpireModes.h"
#include <cstdint>
#include <vector>

namespace lv
{

//==============================================================================
/**
    Replica of Spire's reverb, built from the specification recovered from Spire's
    binary (docs/spire_reverb_spec.md). All seven of Spire's modes are one algorithm,
    Jon Dattorro's 1997 figure-of-eight plate, run with each mode's settings
    (SpireModes.h):

      mono in -> Color (1-pole LP then HP) -> early multitap -> input diffusers
              -> tank (two halves: allpass, modulated delay, damping, allpass, delay;
                 the decay gain applied twice per half; 4 random LFOs)
              -> 7 output taps per side (or 0.6 x two tank nodes) -> 2 peaking EQs
              -> width -> out (wet only)

    The quirks of the original are kept on purpose: unipolar-ish random LFOs that
    all start from the same seed, the 0.6 output scale, the double decay multiply,
    integer-truncated delays and the wrap-around table lookups.

    Not done here (LeadVerb does them around this class): the pre-delay (the
    engine's own, which lies before the color filter in Spire but commutes with
    it), and the dry/wet mix (the processor, with Spire's mix curve).

    One instance runs one mode, so the engine can crossfade between modes. All
    memory is allocated in prepare().
*/
class SpireReverb
{
public:
    /** knobs: decay, damp, color 0..1 (Spire's own ranges); wide 0..2, where
        0..1 is Spire's range (mono .. natural) and above 1 widens further. */
    struct Knobs
    {
        float decay = 0.6357f, damp = 0.15f, color = 0.65f, wide = 1.0f;
    };

    void prepare (double sampleRate, int mode);
    void reset();

    void setParameters (const Knobs&) noexcept;
    /** Jump the smoothed knobs to their targets (after a reset). */
    void snap() noexcept;

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    int getMode() const noexcept { return mode; }

private:
    // Circular buffer with Spire's indexing: read-before-write, wrapped indices.
    struct Line
    {
        std::vector<float> buf;
        int size = 1, w = 0;

        void init (int n) { size = std::max (4, n); buf.assign ((size_t) size, 0.0f); w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

        inline float read (int d) const
        {
            int i = w - d;
            if (i < 0) i += size;
            return buf[(size_t) i];
        }
        inline void write (float x)
        {
            buf[(size_t) w] = x;
            if (++w >= size) w = 0;
        }
        // 4-point Hermite (Catmull-Rom) at position w - delay, exactly as Spire writes it
        inline float hermite (float delay) const
        {
            float p = (float) w - delay;
            if (p < 0.0f) p += (float) size;
            const int i = (int) p;
            const float t = p - (float) i;
            const int im1 = i - 1 < 0 ? size - 1 : i - 1;
            const int i1 = i + 1 >= size ? 0 : i + 1;
            const int i2 = i + 2 >= size ? 0 : i + 2;
            const float ym1 = buf[(size_t) im1], y0 = buf[(size_t) i], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2];
            return ((((ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2) + (1.5f * (y0 - y1) + 0.5f * (y2 - ym1)) * t) * t
                     + 0.5f * (y1 - ym1)) * t) + y0;
        }
        // Allpass A(D, g): y = g*x + buf[w-D]; buf[w] = x - g*y
        inline float allpass (float x, int d, float g)
        {
            const float y = g * x + read (d);
            write (x - g * y);
            return y;
        }
    };

    struct RandomLfo
    {
        float rate = 0.0f, phase = 0.0f, start = 0.0f, slope = 0.0f, value = 0.0f;
        uint32_t a = 0x67452301u, b = 0xEFCDAB89u;

        void reset() { phase = start = slope = value = 0.0f; a = 0x67452301u; b = 0xEFCDAB89u; }
        inline float tick()
        {
            phase += rate;
            if (phase > 1.0f)
            {
                phase -= 1.0f;
                const uint32_t r = b;
                a ^= b;
                b = a + r;
                start = value;
                slope = ((float) (int32_t) r * 4.656612873077393e-10f + 0.5f) - start;   // 2^-31
            }
            value = phase * slope + start;
            return value;
        }
    };

    struct Smoothed
    {
        float value = 0.0f, target = 0.0f;
        bool fast = false;
        void set (float t) { if (std::abs (t - value) >= 0.5f) fast = true; target = t; }
        void snap() { value = target; fast = false; }
        inline float tick (float slowCoef, float fastCoef)
        {
            const float diff = target - value;
            if (std::abs (diff) < 0.001f) { value = target; fast = false; }
            else value += diff * (fast ? fastCoef : slowCoef);
            return value;
        }
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1[2] {}, x2[2] {}, y1[2] {}, y2[2] {};
        void setPeaking (double fs, double hz, double q, double gainDb);
        void clear() { for (int c = 0; c < 2; ++c) x1[c] = x2[c] = y1[c] = y2[c] = 0.0f; }
        inline float process (float x, int c)
        {
            const float y = b0 * x + b1 * x1[c] + b2 * x2[c] - a1 * y1[c] - a2 * y2[c];
            x2[c] = x1[c]; x1[c] = x;
            y2[c] = y1[c]; y1[c] = y;
            return y;
        }
    };

    int mode = 0;
    float fs = 44100.0f, invFs = 1.0f / 44100.0f, K = 1.0f, ratio = 1.0f;   // K = fs/REF*trr, ratio = fs/REF

    Smoothed decay, damp, color, wide;
    float slowCoef = 0.0f, fastCoef = 0.0f;

    RandomLfo lfo[4];
    Line mt, df[6], apL[2], wgL[2], apR[2], wgR[2], clL[7], clR[7];
    float colorLp = 0.0f, colorHp = 0.0f, dampL = 0.0f, dampR = 0.0f, fbR = 0.0f;
    Biquad notch[2];
};

} // namespace lv
