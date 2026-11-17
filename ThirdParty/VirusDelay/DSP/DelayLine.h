#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace vd
{

//==============================================================================
/**
    Power-of-two circular buffer with 4-point cubic Hermite interpolated reads.

    The delay is measured in samples behind the *next* write position, so the
    typical per-sample pattern is: read() -> compute input -> push().
    Memory is only allocated in prepare().
*/
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;

        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
        maxDelay = (float) (size - 4);
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    inline void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Hermite read, delay in samples (fractional). Clamped to the safe range. */
    inline float read (float delaySamples) const noexcept
    {
        const float d = std::clamp (delaySamples, kMinDelay, maxDelay);
        const int   i = (int) d;
        const float t = d - (float) i;

        const float ym1 = at (i - 1);
        const float y0  = at (i);
        const float y1  = at (i + 1);
        const float y2  = at (i + 2);

        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    float getMaxDelay() const noexcept { return maxDelay; }

    // Hermite needs one newer neighbour, so the shortest legal delay is 2 samples.
    static constexpr float kMinDelay = 2.0f;

private:
    inline float at (int delay) const noexcept { return buffer[(size_t) ((writePos - delay) & mask)]; }

    std::vector<float> buffer;
    int mask = 0, writePos = 0;
    float maxDelay = 0.0f;
};

//==============================================================================
/** Simple one-pole low-pass (used for tape roll-off and DC blocking). */
struct OnePole
{
    void setCutoff (float hz, double sampleRate) noexcept
    {
        a = 1.0f - (float) std::exp (-2.0 * 3.14159265358979323846 * hz / sampleRate);
    }

    void reset() noexcept               { z = 0.0f; }
    inline float lowpass (float x) noexcept  { z += a * (x - z); return z; }
    inline float highpass (float x) noexcept { return x - lowpass (x); }

    float a = 1.0f, z = 0.0f;
};

/** Transparent below 0.9, smooth knee towards a hard ceiling of 1.5. Keeps
    feedback loops from ever running away. */
inline float safetyClip (float x) noexcept
{
    constexpr float knee = 0.9f, ceiling = 1.5f, range = ceiling - knee;
    const float a = std::abs (x);
    if (a <= knee)
        return x;
    return std::copysign (knee + range * std::tanh ((a - knee) / range), x);
}

} // namespace vd
