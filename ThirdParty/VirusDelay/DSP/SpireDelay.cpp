#include "SpireDelay.h"

namespace vd
{

void SpireDelay::prepare (double sampleRate)
{
    fs = sampleRate;

    const int maxSamples = (int) std::ceil ((kSpireMaxDelayMs + 8.0f + 20.0f) * 0.001 * fs);
    for (auto& line : lines)
        line.prepare (maxSamples);

    for (auto& f : filters)
        f.prepare (fs);

    matrixSmoothing = 1.0f - (float) std::exp (-1.0 / (0.02 * fs));  // ~20 ms
    fadeIncrement   = (float) (1.0 / (0.04 * fs));                    // 40 ms crossfades
    reset();
}

void SpireDelay::reset()
{
    for (auto& line : lines)
        line.clear();
    for (auto& f : filters)
        f.reset();

    taps[0] = taps[1] = Tap{};
    loopPeak = 0.0f;
    snapOnNextBlock = true;
}

void SpireDelay::setParameters (const Settings& s) noexcept
{
    const float maxDelaySamples = (float) (kSpireMaxDelayMs * 0.001 * fs);
    const float minSamples = DelayLine::kMinDelay + 1.0f;

    for (int c = 0; c < 2; ++c)
        targetSamples[c] = std::clamp ((float) (s.delayMs[c] * 0.001 * fs), minSamples, maxDelaySamples);

    std::fill (std::begin (coeffTarget), std::end (coeffTarget), 0.0f);
    const float w = std::clamp (s.wide, -1.0f, 1.0f);

    if (! s.pingPong)
    {
        coeffTarget[inAL] = 1.0f;
        coeffTarget[inBR] = 1.0f;
        coeffTarget[fbAA] = s.feedback;
        coeffTarget[fbBB] = s.feedback;
        coeffTarget[width] = w;                       // negative = swapped
    }
    else if (w < 0.0f)                                // cross feedback
    {
        coeffTarget[inAL] = 1.0f;
        coeffTarget[inBR] = 1.0f;
        coeffTarget[fbBA] = s.feedback;
        coeffTarget[fbAB] = s.feedback;
        coeffTarget[width] = -w;
    }
    else                                              // ping pong
    {
        coeffTarget[inAL] = coeffTarget[inAR] = 0.7071f;   // equal-power mono sum
        coeffTarget[fbAB] = 1.0f;
        coeffTarget[fbBA] = s.feedback;
        coeffTarget[width] = w;
    }

    for (auto& f : filters)
        f.setColor (s.colorBipolar);

    depthTarget = (float) (s.modDepthMs * 0.001 * fs);

    if (snapOnNextBlock)
    {
        snapOnNextBlock = false;
        std::copy (std::begin (coeffTarget), std::end (coeffTarget), std::begin (coeff));
        for (int i = 0; i < 2; ++i)
        {
            taps[i].current = taps[i].next = targetSamples[i];
            taps[i].fading = false;
        }
        depthSamples = depthTarget;
        inputGain = inputGainTarget;
    }
}

inline float SpireDelay::readTap (const DelayLine& line, Tap& tap, float target, float mod) noexcept
{
    if (! tap.fading && std::abs (target - tap.current) > 0.5f)
    {
        tap.next = target;
        tap.fade = 0.0f;
        tap.fading = true;
    }

    if (! tap.fading)
        return line.read (tap.current + mod);

    const float a = line.read (tap.current + mod);
    const float b = line.read (tap.next + mod);
    const float y = a + tap.fade * (b - a);

    tap.fade += fadeIncrement;
    if (tap.fade >= 1.0f)
    {
        tap.current = tap.next;
        tap.fading = false;
    }
    return y;
}

void SpireDelay::process (const float* inL, const float* inR, float* outL, float* outR,
                          const float* lfoA, const float* lfoB, int numSamples) noexcept
{
    const float depthStep = (depthTarget - depthSamples) / (float) std::max (1, numSamples);
    float peak = loopPeak;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < numCoeffs; ++c)
            coeff[c] += (coeffTarget[c] - coeff[c]) * matrixSmoothing;

        inputGain += (inputGainTarget - inputGain) * matrixSmoothing;
        depthSamples += depthStep;

        const float a = readTap (lines[0], taps[0], targetSamples[0], depthSamples * (0.5f + 0.5f * lfoA[i]));
        const float b = readTap (lines[1], taps[1], targetSamples[1], depthSamples * (0.5f + 0.5f * lfoB[i]));

        const float xL = inL[i] * inputGain;
        const float xR = inR[i] * inputGain;

        float wA = coeff[inAL] * xL + coeff[inAR] * xR + coeff[fbAA] * a + coeff[fbBA] * b;
        float wB = coeff[inBL] * xL + coeff[inBR] * xR + coeff[fbAB] * a + coeff[fbBB] * b;

        wA = filters[0].process (wA);
        wB = filters[1].process (wB);
        peak = std::max (peak, std::max (std::abs (wA), std::abs (wB)));

        lines[0].push (safetyClip (wA));
        lines[1].push (safetyClip (wB));

        // Mid/side width: +1 normal, 0 mono, -1 swapped
        const float mid = 0.5f * (a + b), side = 0.5f * (a - b);
        outL[i] = mid + coeff[width] * side;
        outR[i] = mid - coeff[width] * side;
    }

    depthSamples = depthTarget;
    loopPeak = peak;
}

} // namespace vd
