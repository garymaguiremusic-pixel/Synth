#pragma once
// Small DSP building blocks (no JUCE).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vc::dsp
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double clamp01 (double x) { return std::clamp (x, 0.0, 1.0); }
inline double midiToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }

// Deterministic RNG (xorshift64*), seedable for reproducible renders.
struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    void seed (uint64_t v) { s = v ? v : 0x9E3779B97F4A7C15ull; }
    uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 2685821657736338717ull; }
    double uniform() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }   // [0,1)
    double bipolar() { return uniform() * 2.0 - 1.0; }
};

// PolyBLEP residual for a step at fractional position t (0..1) relative to dt
inline double polyBlep (double t, double dt)
{
    if (t < dt)            { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt)      { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}
// PolyBLAMP (integrated BLEP), for slope discontinuities (triangle corners)
inline double polyBlamp (double t, double dt)
{
    if (t < dt)       { t = t / dt - 1.0; return -1.0 / 3.0 * t * t * t; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt + 1.0; return 1.0 / 3.0 * t * t * t; }
    return 0.0;
}

// One-pole smoother
struct Smoother
{
    double y = 0.0, a = 0.0;
    void setTime (double seconds, double sr) { a = seconds <= 0.0 ? 0.0 : std::exp (-1.0 / (seconds * sr)); }
    void reset (double v) { y = v; }
    double process (double x) { y = x + a * (y - x); return y; }
};

// TPT one-pole (LP/HP)
struct OnePole
{
    double s = 0.0, g = 0.0;
    void setCutoff (double hz, double sr) { const double w = std::tan (kPi * std::clamp (hz, 1.0, sr * 0.49) / sr); g = w / (1.0 + w); }
    double lp (double x) { const double v = (x - s) * g; const double y = v + s; s = y + v; return y; }
    double hp (double x) { return x - lp (x); }
    void reset() { s = 0.0; }
};

// TPT state-variable filter (Zavalishin), double precision.
struct Svf
{
    double ic1 = 0.0, ic2 = 0.0;
    double g = 0.0, k = 2.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    void set (double hz, double damping /* 2R */, double sr)
    {
        g = std::tan (kPi * std::clamp (hz, 5.0, sr * 0.48) / sr);
        k = damping;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    struct Out { double lp, bp, hp; };
    Out tick (double x)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        // soft-limit the integrators so high resonance can't blow up
        if (std::abs (ic1) > 8.0) ic1 = 8.0 * std::tanh (ic1 / 8.0);
        if (std::abs (ic2) > 8.0) ic2 = 8.0 * std::tanh (ic2 / 8.0);
        return { v2, v1, x - k * v1 - v2 };
    }
    void reset() { ic1 = ic2 = 0.0; }
};

// Zero-delay-feedback 4-pole ladder with 1..4 pole outputs and soft saturation.
struct Ladder
{
    double s[4] {};
    double G = 0.0, g = 0.0, k = 0.0;
    void set (double hz, double kRes, double sr)
    {
        const double w = std::tan (kPi * std::clamp (hz, 5.0, sr * 0.48) / sr);
        g = w / (1.0 + w);
        G = g * g * g * g;
        k = kRes;
    }
    // returns output for the selected number of poles (1..4)
    double tick (double x, int poles)
    {
        // zero-delay feedback: y4 = G*u + sigma, u = x - k*y4  =>  u = (x - k*sigma) / (1 + k*G)
        const double b = 1.0 - g;
        const double sigma = g * g * g * b * s[0] + g * g * b * s[1] + g * b * s[2] + b * s[3];
        const double u = (x - k * sigma) / (1.0 + k * G);
        const double us = std::tanh (u);          // input saturation (analog flavour)
        double y = us;
        double out[4];
        for (int i = 0; i < 4; ++i)
        {
            const double v = (y - s[i]) * g;
            const double lp = v + s[i];
            s[i] = lp + v;
            out[i] = lp;
            y = lp;
        }
        // resonance gain compensation (keeps passband level roughly constant)
        const double comp = 1.0 + 0.5 * k;
        return out[std::clamp (poles, 1, 4) - 1] * comp;
    }
    void reset() { for (auto& v : s) v = 0.0; }
};

// RBJ biquad (direct form 1, double)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double process (double x)
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
    void reset() { x1 = x2 = y1 = y2 = 0; }
    void setNorm (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    void peak (double f, double q, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * std::clamp (f, 10.0, sr * 0.45) / sr, al = std::sin (w) / (2.0 * q);
        setNorm (1 + al * A, -2 * std::cos (w), 1 - al * A, 1 + al / A, -2 * std::cos (w), 1 - al / A);
    }
    void lowShelf (double f, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * std::clamp (f, 10.0, sr * 0.45) / sr, c = std::cos (w);
        const double al = std::sin (w) / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
                 (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
    void highShelf (double f, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * std::clamp (f, 10.0, sr * 0.45) / sr, c = std::cos (w);
        const double al = std::sin (w) / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                 (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }
};

// Halfband decimator (2x -> 1x), 31-tap linear-phase FIR (windowed sinc), ~ -80 dB stopband
struct HalfbandDecimator
{
    static constexpr int kTaps = 31;
    std::array<double, kTaps> c {};
    std::array<double, kTaps * 2> buf {};
    int pos = 0;
    HalfbandDecimator()
    {
        const int M = kTaps - 1;
        double sum = 0;
        for (int n = 0; n < kTaps; ++n)
        {
            const double x = n - M / 2.0;
            const double sinc = x == 0.0 ? 1.0 : std::sin (kPi * x / 2.0) / (kPi * x / 2.0);
            const double a = 0.42 - 0.5 * std::cos (kTwoPi * n / M) + 0.08 * std::cos (2 * kTwoPi * n / M);  // Blackman
            c[(size_t) n] = 0.5 * sinc * a;
            sum += c[(size_t) n];
        }
        for (auto& v : c) v /= sum;
    }
    void push (double x) { buf[(size_t) pos] = buf[(size_t) (pos + kTaps)] = x; pos = (pos + 1) % kTaps; }
    // after pushing two samples, read one decimated output
    double read() const
    {
        double y = 0;
        for (int i = 0; i < kTaps; ++i) y += c[(size_t) i] * buf[(size_t) (pos + i)];
        return y;
    }
    void reset() { buf.fill (0.0); pos = 0; }
    static constexpr int latencyOut() { return (kTaps - 1) / 4; }   // in output samples
};

// Interpolated delay line
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // delay in samples (>= 1), cubic Hermite interpolation
    float read (double d) const
    {
        d = std::clamp (d, 1.0, (double) mask - 3.0);
        const double rp = (double) w - d;
        int i = (int) std::floor (rp);
        const float f = (float) (rp - i);
        auto at = [&] (int k) { return buf[(size_t) ((i + k) & mask)]; };
        const float xm1 = at (-1), x0 = at (0), x1 = at (1), x2 = at (2);
        const float c0 = x0, c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
};
} // namespace vc::dsp
