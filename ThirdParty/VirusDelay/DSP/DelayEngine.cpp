#include "DelayEngine.h"

namespace vd
{

void DelayEngine::prepare (double sampleRate)
{
    fs = sampleRate;

    const int maxSamples = (int) std::ceil ((kMaxDelayMs + kMaxModDepthMs + 10.0f) * 0.001 * fs);
    for (auto& line : lines)
        line.prepare (maxSamples);

    for (auto& f : filters)
        f.prepare (fs);

    matrixSmoothing = 1.0f - (float) std::exp (-1.0 / (0.02 * fs));  // ~20 ms
    fadeIncrement   = (float) (1.0 / (0.04 * fs));                    // 40 ms crossfades
    reset();
}

void DelayEngine::reset()
{
    for (auto& line : lines)
        line.clear();
    for (auto& f : filters)
        f.reset();

    taps[0] = taps[1] = Tap{};
    loopPeak = 0.0f;
    snapOnNextBlock = true;
}

void DelayEngine::setParameters (int classicType, float baseDelayMs, float feedback,
                                 float colorBipolar, float modDepthMs) noexcept
{
    float ratioA, ratioB;
    getLineRatios (classicType, ratioA, ratioB);

    const float maxDelaySamples = (float) (kMaxDelayMs * 0.001 * fs);
    const float baseSamples = (float) (baseDelayMs * 0.001 * fs);
    const float minSamples = DelayLine::kMinDelay + 1.0f;

    targetSamples[0] = std::clamp (baseSamples * ratioA, minSamples, maxDelaySamples);
    targetSamples[1] = std::clamp (baseSamples * ratioB, minSamples, maxDelaySamples);

    std::fill (std::begin (coeffTarget), std::end (coeffTarget), 0.0f);

    if (isPingPong (classicType) || isPattern (classicType))
    {
        coeffTarget[inAL] = coeffTarget[inAR] = 0.7071f;   // equal-power mono sum into A
        coeffTarget[fbAB] = 1.0f;                        // A -> B always (second half of the bounce)
        coeffTarget[fbBA] = feedback;                    // B -> A closes the loop

        if (isPingPong (classicType))
        {
            coeffTarget[outLA] = 1.0f;
            coeffTarget[outRB] = 1.0f;
        }
        else
        {
            coeffTarget[outLA] = 0.85f; coeffTarget[outLB] = 0.5f;
            coeffTarget[outRA] = 0.5f;  coeffTarget[outRB] = 0.85f;
        }
    }
    else // Simple
    {
        coeffTarget[inAL]  = 1.0f;
        coeffTarget[inBR]  = 1.0f;
        coeffTarget[fbAA]  = feedback;
        coeffTarget[fbBB]  = feedback;
        coeffTarget[outLA] = 1.0f;
        coeffTarget[outRB] = 1.0f;
    }

    for (auto& f : filters)
        f.setColor (colorBipolar);

    depthTarget = (float) (modDepthMs * 0.001 * fs);

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

inline float DelayEngine::readTap (const DelayLine& line, Tap& tap, float target, float mod) noexcept
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

void DelayEngine::process (const float* inL, const float* inR, float* outL, float* outR,
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

        // Unipolar modulation keeps the tap at or above the base time.
        const float modA = depthSamples * (0.5f + 0.5f * lfoA[i]);
        const float modB = depthSamples * (0.5f + 0.5f * lfoB[i]);

        const float a = readTap (lines[0], taps[0], targetSamples[0], modA);
        const float b = readTap (lines[1], taps[1], targetSamples[1], modB);

        const float xL = inL[i] * inputGain;
        const float xR = inR[i] * inputGain;

        float wA = coeff[inAL] * xL + coeff[inAR] * xR + coeff[fbAA] * a + coeff[fbBA] * b;
        float wB = coeff[inBL] * xL + coeff[inBR] * xR + coeff[fbAB] * a + coeff[fbBB] * b;

        wA = filters[0].process (wA);
        wB = filters[1].process (wB);

        peak = std::max (peak, std::max (std::abs (wA), std::abs (wB)));

        lines[0].push (safetyClip (wA));
        lines[1].push (safetyClip (wB));

        outL[i] = coeff[outLA] * a + coeff[outLB] * b;
        outR[i] = coeff[outRA] * a + coeff[outRB] * b;
    }

    depthSamples = depthTarget;
    loopPeak = peak;
}

} // namespace vd
