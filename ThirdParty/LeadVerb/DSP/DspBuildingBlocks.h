#pragma once

// Small, allocation-free (after prepare) DSP primitives used by the reverb engines.
// Deliberately framework-independent so the engine can be unit tested without JUCE.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lv
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline int nextPow2 (int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

//==============================================================================
// Circular delay line. read(d) returns the sample written d samples ago (d >= 1),
// so the usual pattern is: y = read (N); write (x);
class DelayLine
{
public:
    void init (int maxDelaySamples)
    {
        const int size = nextPow2 (std::max (4, maxDelaySamples + 4));
        buffer.assign ((size_t) size, 0.0f);
        mask     = size - 1;
        writePos = 0;
    }

    void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    inline void write (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    inline float read (int delay) const { return buffer[(size_t) ((writePos - delay) & mask)]; }

    // Linear interpolated fractional read, delay >= 1.
    inline float readFrac (float delay) const
    {
        const int   i = (int) delay;
        const float f = delay - (float) i;
        const float a = read (i);
        const float b = read (i + 1);
        return a + f * (b - a);
    }

    // 4-point cubic (Hermite) fractional read, delay >= 2. Used for modulated
    // lines: unlike linear interpolation it doesn't dull the highs as the tap moves.
    inline float readCubic (float delay) const
    {
        const int   i = (int) delay;
        const float t = delay - (float) i;
        const float ym1 = read (i - 1), y0 = read (i), y1 = read (i + 1), y2 = read (i + 2);
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    int capacity() const { return mask + 1 - 4; }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

//==============================================================================
// Schroeder allpass: H(z) = (z^-N - g) / (1 - g z^-N)
class Allpass
{
public:
    void init (int maxLen) { line.init (maxLen + 64); }
    void clear() { line.clear(); }
    void setLength (int samples) { len = std::max (1, std::min (samples, line.capacity())); }
    void setGain (float g) { gain = g; }

    inline float process (float x)
    {
        const float d = line.read (len);
        const float w = x + gain * d;
        line.write (w);
        return d - gain * w;
    }

    // Modulated variant: length is fractional and may move every sample.
    inline float processMod (float x, float lenSamples)
    {
        const float d = line.readCubic (std::max (2.0f, lenSamples));
        const float w = x + gain * d;
        line.write (w);
        return d - gain * w;
    }

    inline float tap (int d) const { return line.read (d); }
    inline float tapCubic (float d) const { return line.readCubic (d); }

private:
    DelayLine line;
    int   len  = 1;
    float gain = 0.5f;
};

//==============================================================================
struct OnePoleLP
{
    float a = 1.0f, z = 0.0f;

    void setCutoff (float hz, float sampleRate)
    {
        hz = std::min (hz, 0.49f * sampleRate);
        a  = 1.0f - std::exp (-kTwoPi * hz / sampleRate);
    }
    inline float process (float x) { z += a * (x - z); return z; }
    void clear() { z = 0.0f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float hz, float sampleRate) { lp.setCutoff (hz, sampleRate); }
    inline float process (float x) { return x - lp.process (x); }
    void clear() { lp.clear(); }
};

//==============================================================================
// Quadrature sine oscillator (rotating phasor), cheap enough to run per sample.
class QuadOsc
{
public:
    void setFrequency (float hz, float sr)
    {
        const float w = kTwoPi * hz / sr;
        cr = std::cos (w);
        sr_ = std::sin (w);
    }
    void reset (float phase = 0.0f) { c = std::cos (phase); s = std::sin (phase); counter = 0; }

    inline void tick()
    {
        const float nc = c * cr - s * sr_;
        const float ns = c * sr_ + s * cr;
        c = nc;
        s = ns;
        if (++counter >= 4096) // renormalise to stop amplitude drift
        {
            const float g = 1.0f / std::sqrt (c * c + s * s);
            c *= g;
            s *= g;
            counter = 0;
        }
    }

    float cosine() const { return c; }
    float sine() const { return s; }

private:
    float c = 1.0f, s = 0.0f, cr = 1.0f, sr_ = 0.0f;
    int counter = 0;
};

//==============================================================================
// Smooth random modulation ("wander"): glides along an S-curve from one
// random value to the next, at a roughly constant rate with some jitter. Unlike a
// sine LFO it never repeats, so modulated reverbs move without an audible cycle.
// Output is -1 .. +1.
class Wander
{
public:
    void prepare (float rateHz, float sampleRate, uint32_t seed)
    {
        state = seed * 2654435761u + 0x9e3779b9u;
        baseStep = rateHz / sampleRate;
        from = nextRandom();
        to = nextRandom();
        phase = 0.0f;
        step = baseStep * (0.7f + 0.6f * unit());
    }

    void reset (uint32_t seed) { prepare (baseStep, 1.0f, seed); }

    inline float tick()
    {
        phase += step;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            from = to;
            to = nextRandom();
            step = baseStep * (0.7f + 0.6f * unit());
        }
        // smoothstep: practically the same S-curve as a raised cosine, far cheaper,
        // which matters with a hundred or more of these running per sample
        const float w = phase * phase * (3.0f - 2.0f * phase);
        return from + (to - from) * w;
    }

private:
    inline uint32_t nextU32()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    inline float unit() { return (float) (nextU32() >> 8) * (1.0f / 16777216.0f); }
    inline float nextRandom() { return 2.0f * unit() - 1.0f; }

    uint32_t state = 1;
    float baseStep = 0.0f, step = 0.0f, phase = 0.0f, from = 0.0f, to = 0.0f;
};

//==============================================================================
// Two-tap crossfading delay-line pitch shifter, used for the shimmer feedback path.
class PitchShifter
{
public:
    void prepare (float sampleRate, float windowMs = 60.0f, float startPhase = 0.0f)
    {
        window = std::max (64.0f, windowMs * 0.001f * sampleRate);
        line.init ((int) window + 8);
        initialPhase = startPhase;
        phase = startPhase;
    }

    void setSemitones (float semis) { ratio = std::pow (2.0f, semis / 12.0f); }
    void clear() { line.clear(); phase = initialPhase; }

    inline float process (float x)
    {
        line.write (x);

        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);

        float p2 = phase + 0.5f;
        if (p2 >= 1.0f)
            p2 -= 1.0f;

        // sin^2 windows sum to exactly 1 for taps half a window apart
        const float g1 = std::sin (kPi * phase);
        const float g2 = std::sin (kPi * p2);

        return line.readCubic (2.0f + phase * window) * g1 * g1
             + line.readCubic (2.0f + p2 * window) * g2 * g2;
    }

private:
    DelayLine line;
    float window = 2048.0f, phase = 0.0f, initialPhase = 0.0f, ratio = 2.0f;
};

//==============================================================================
// "Depth": a slow cross-stereo ensemble on the reverb output. Two voices per side
// read the *opposite* side 11-20 ms back, each drifting by up to 2.5 ms with its
// own smooth random wander, and are blended in. The reverb's two sides are
// uncorrelated, so this adds a wide, swirling movement without the comb filter
// (metallic, flanged colour) a same-side chorus would put on the tail.
class Ensemble
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        const float msToSamples = 0.001f * fs;
        for (auto& line : lines)
            line.init ((int) (30.0f * msToSamples) + 8);

        constexpr float baseMs[2][2] = { { 11.3f, 17.9f }, { 13.7f, 19.3f } };
        constexpr float rates[2][2]  = { { 0.23f, 0.37f }, { 0.31f, 0.43f } };
        for (int c = 0; c < 2; ++c)
            for (int v = 0; v < 2; ++v)
            {
                base[c][v] = baseMs[c][v] * msToSamples;
                wander[c][v].prepare (rates[c][v], fs, (uint32_t) (907 + 41 * c + 7 * v));
            }
        depth = 2.5f * msToSamples;
        smoothing = 1.0f - std::exp (-1.0f / (0.05f * fs));
    }

    void clear()
    {
        for (auto& line : lines)
            line.clear();
        amount = target;
    }

    void setAmount (float a) { target = std::clamp (a, 0.0f, 1.0f); }
    void snap() { amount = target; }

    inline void process (float& left, float& right)
    {
        amount += (target - amount) * smoothing;
        lines[0].write (left);
        lines[1].write (right);

        float* io[2] = { &left, &right };
        for (int c = 0; c < 2; ++c)
        {
            const auto& other = lines[1 - c];
            const float voices = other.readCubic (base[c][0] + depth * (1.0f + wander[c][0].tick()))
                               + other.readCubic (base[c][1] + depth * (1.0f + wander[c][1].tick()));
            // the voices are uncorrelated with the direct signal, so this keeps the level about even
            *io[c] = *io[c] * (1.0f - 0.5f * amount) + voices * 0.6f * amount;
        }
    }

private:
    float fs = 44100.0f;
    DelayLine lines[2];
    Wander wander[2][2];
    float base[2][2] {}, depth = 0.0f;
    float amount = 0.0f, target = 0.0f, smoothing = 0.001f;
};

//==============================================================================
// Vibrato for the "Modulation" knob: both sides read a short delay whose length
// swings with a sine LFO, which bends the pitch up and down. The depth is set in
// cents (peak pitch deviation): for a delay swinging by +-D seconds at f Hz the
// pitch deviates by 2*pi*f*D, so D = (2^(cents/1200) - 1) / (2*pi*f). The right
// side's LFO runs 90 degrees behind the left, so the wobble also swirls in stereo.
class Vibrato
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& line : lines)
            line.init ((int) (0.006f * fs) + 8);   // up to ~60 cents at 4.5 Hz
        smoothing = 1.0f - std::exp (-1.0f / (0.05f * fs));
        clear();
    }

    void clear()
    {
        for (auto& line : lines)
            line.clear();
        depth = depthTarget;
        osc.reset();
    }

    void snap() { depth = depthTarget; }

    void set (float cents, float rateHz)
    {
        const float ratio = std::pow (2.0f, std::max (0.0f, cents) / 1200.0f);
        depthTarget = std::min (0.0025f * fs, (ratio - 1.0f) / (kTwoPi * rateHz) * fs);
        osc.setFrequency (rateHz, fs);
    }

    inline void process (float& left, float& right)
    {
        depth += (depthTarget - depth) * smoothing;
        osc.tick();
        lines[0].write (left);
        lines[1].write (right);
        // centre the swing around a fixed delay so it never reads ahead of the write
        left  = lines[0].readCubic (2.0f + depth * (1.0f + osc.sine()));
        right = lines[1].readCubic (2.0f + depth * (1.0f + osc.cosine()));
    }

private:
    float fs = 44100.0f;
    DelayLine lines[2];
    QuadOsc osc;
    float depth = 0.0f, depthTarget = 0.0f, smoothing = 0.001f;
};

//==============================================================================
// Feedback gain for a loop of the given length so that it decays 60 dB in rt60 seconds.
inline float decayGainForLoop (float loopSamples, float rt60Seconds, float sampleRate)
{
    return std::pow (10.0f, -3.0f * loopSamples / (std::max (0.05f, rt60Seconds) * sampleRate));
}

} // namespace lv
