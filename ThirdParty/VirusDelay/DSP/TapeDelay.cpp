#include "TapeDelay.h"

namespace vd
{

namespace
{
    constexpr double twoPi = 6.283185307179586;

    // Modulation depths at Modulation = 100 (milliseconds of head movement)
    constexpr float wowDepthMs     = 3.5f;
    constexpr float flutterDepthMs = 0.25f;
    constexpr float driftDepthMs   = 1.5f;
    constexpr float dopplerDepthMs = 10.0f;

    constexpr double wowHz = 0.6, flutterHz = 6.5;
}

void TapeDelay::prepare (double sampleRate)
{
    fs = sampleRate;

    const int maxSamples = (int) std::ceil ((kMaxDelayMs + dopplerDepthMs + 20.0f) * 0.001 * fs);
    for (auto& line : lines)
        line.prepare (maxSamples);

    for (int c = 0; c < 2; ++c)
    {
        band[c].prepare (fs);
        hfRollOff[c].setCutoff (6500.0f, fs);
        lowCut[c].setCutoff (35.0f, fs);
    }

    // Tape: the head approaches its target with a ~350 ms time constant and its
    // speed is smoothed over ~40 ms (over-damped, no overshoot), max 0.5x..1.5x.
    tapeGlide    = { 1.0 / (0.35 * fs), 1.0 - std::exp (-1.0 / (0.04 * fs)), 0.5 };
    // Doppler: a much faster, wider head -> dramatic pitch swoops.
    dopplerGlide = { 1.0 / (0.12 * fs), 1.0 - std::exp (-1.0 / (0.015 * fs)), 0.9 };

    gainSmoothing  = 1.0f - (float) std::exp (-1.0 / (0.02 * fs));
    driftSmoothing = 1.0f - (float) std::exp (-1.0 / (0.5 * fs));

    minDelay = DelayLine::kMinDelay + 1.0;
    maxDelay = kMaxDelayMs * 0.001 * fs;
    reset();
}

void TapeDelay::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        lines[c].clear();
        band[c].reset();
        hfRollOff[c].reset();
        lowCut[c].reset();
        velocity[c] = 0.0;
    }

    wowPhase = flutterPhase = sweepPhase = 0.0;
    drift = driftTarget = 0.0f;
    loopPeak = 0.0f;
    snapOnNextBlock = true;
}

void TapeDelay::setParameters (const Settings& s) noexcept
{
    for (int c = 0; c < 2; ++c)
        target[c] = std::clamp ((double) s.delayMs[c] * 0.001 * fs, minDelay, maxDelay);

    feedbackTarget = s.feedback;
    modulationTarget = std::clamp (s.modulation, 0.0f, 1.0f);
    doppler = s.doppler;

    for (auto& b : band)
        b.setBand (s.centreHz, s.bandwidthOctaves);

    if (snapOnNextBlock)
    {
        snapOnNextBlock = false;
        for (int c = 0; c < 2; ++c)
        {
            delay[c] = target[c];
            velocity[c] = 0.0;
        }
        feedback = feedbackTarget;
        modulation = modulationTarget;
        inputGain = inputGainTarget;
    }
}

void TapeDelay::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    const float fbStep  = (feedbackTarget - feedback) / (float) std::max (1, numSamples);
    const float modStep = (modulationTarget - modulation) / (float) std::max (1, numSamples);
    const Glide& glide = doppler ? dopplerGlide : tapeGlide;
    const float msToSamples = (float) (0.001 * fs);
    float peak = loopPeak;

    for (int i = 0; i < numSamples; ++i)
    {
        feedback += fbStep;
        modulation += modStep;
        inputGain += (inputGainTarget - inputGain) * gainSmoothing;

        // --- modulation offsets (samples) and doppler level ------------------------
        const float m = modulation * modulation;
        float offset[2], level[2] = { 1.0f, 1.0f };

        if (doppler)
        {
            sweepPhase += (0.25 + 1.0 * (double) modulation) / fs;
            if (sweepPhase >= 1.0) sweepPhase -= 1.0;

            const float s = (float) std::sin (twoPi * sweepPhase);
            const float depth = m * dopplerDepthMs * msToSamples;
            offset[0] = depth * (0.5f + 0.5f * s);
            offset[1] = depth * (0.5f - 0.5f * s);
            // a farther (longer delay) source is a little quieter
            level[0] = 1.0f - 0.3f * modulation * (0.5f + 0.5f * s);
            level[1] = 1.0f - 0.3f * modulation * (0.5f - 0.5f * s);
        }
        else
        {
            wowPhase += wowHz / fs;
            if (wowPhase >= 1.0) wowPhase -= 1.0;
            flutterPhase += flutterHz / fs;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;

            if (--driftCountdown <= 0)
            {
                driftCountdown = (int) (0.4 * fs);
                driftTarget = random.nextFloat() * 2.0f - 1.0f;
            }
            drift += (driftTarget - drift) * driftSmoothing;

            const float wow     = (float) std::sin (twoPi * wowPhase);
            const float flutter = (float) std::sin (twoPi * flutterPhase);
            const float ms = wowDepthMs * (0.5f + 0.5f * wow)
                           + flutterDepthMs * (0.5f + 0.5f * flutter)
                           + driftDepthMs * (0.5f + 0.5f * drift);
            offset[0] = offset[1] = m * ms * msToSamples;
        }

        const float in[2] = { inL[i] * inputGain, inR[i] * inputGain };
        float out[2];

        for (int c = 0; c < 2; ++c)
        {
            // Slew-limited, smoothed head speed -> continuous pitch glide.
            const double wantedVelocity = std::clamp ((target[c] - delay[c]) * glide.rate, -glide.maxVelocity, glide.maxVelocity);
            velocity[c] += (wantedVelocity - velocity[c]) * glide.velocitySmoothing;
            delay[c] = std::clamp (delay[c] + velocity[c], minDelay, maxDelay);

            const float y = lines[c].read ((float) delay[c] + offset[c]);
            out[c] = y * level[c];

            float w = in[c] + feedback * y;
            w = band[c].process (w);
            w = hfRollOff[c].lowpass (w);
            w = lowCut[c].highpass (w);

            peak = std::max (peak, std::abs (w));
            lines[c].push (safetyClip (saturate (w)));
        }

        outL[i] = out[0];
        outR[i] = out[1];
    }

    feedback = feedbackTarget;
    modulation = modulationTarget;
    loopPeak = peak;
}

} // namespace vd
