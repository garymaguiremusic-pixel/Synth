#include "WetEffects.h"

namespace vd
{

namespace
{
    constexpr double twoPi = 6.283185307179586;
    inline float onePoleCoeff (double seconds, double fs) { return 1.0f - (float) std::exp (-1.0 / (seconds * fs)); }
}

//==============================================================================
// MovementFX
//==============================================================================
void MovementFX::prepare (double sampleRate)
{
    fs = sampleRate;
    mixSmoothing = onePoleCoeff (0.03, fs);
    paramSmoothing = onePoleCoeff (0.05, fs);
    for (auto& line : flangeLines)
        line.prepare ((int) std::ceil (0.01 * fs));   // 10 ms is plenty for a flanger
    reset();
}

void MovementFX::reset()
{
    phase = 0.0;
    countdown = 0;
    for (int c = 0; c < 2; ++c)
    {
        hp[c].reset();
        phaserFeedback[c] = 0.0f;
        flangeLines[c].clear();
        std::fill (std::begin (apState[c]), std::end (apState[c]), 0.0f);
    }
    snapOnNextBlock = true;
}

void MovementFX::setParameters (const Settings& s) noexcept
{
    settings = s;

    if (snapOnNextBlock)
    {
        snapOnNextBlock = false;
        hpMix = s.hpOn ? 1.0f : 0.0f;
        phaserMix = s.phaserOn ? 1.0f : 0.0f;
        flangerMix = s.flangerOn ? 1.0f : 0.0f;
        depth = s.depth;
        rate = s.rateHz;
        mix = s.mix;
    }
}

void MovementFX::updateCoefficients (int c, float lfo) noexcept
{
    const float sweep = depth * (0.5f + 0.5f * lfo);                 // 0..depth
    const float nyquistLimit = (float) (0.45 * fs);

    const float hpHz = std::min (30.0f * std::pow (100.0f, sweep), nyquistLimit);   // 30 Hz .. 3 kHz
    hp[c].set (hpHz, 0.9f, fs);

    const float apHz = std::min (200.0f * std::pow (20.0f, sweep), nyquistLimit);   // 200 Hz .. 4 kHz
    const float t = (float) std::tan (3.14159265358979323846 * apHz / fs);
    apCoeff[c] = (t - 1.0f) / (t + 1.0f);
}

void MovementFX::process (float* left, float* right, int numSamples) noexcept
{
    const float hpTarget = settings.hpOn ? 1.0f : 0.0f;
    const float phaserTarget = settings.phaserOn ? 1.0f : 0.0f;
    const float flangerTarget = settings.flangerOn ? 1.0f : 0.0f;

    // All off and faded out: nothing to do.
    if (! settings.hpOn && ! settings.phaserOn && ! settings.flangerOn
        && hpMix < 1.0e-4f && phaserMix < 1.0e-4f && flangerMix < 1.0e-4f)
    {
        hpMix = phaserMix = flangerMix = 0.0f;
        return;
    }

    const float msToSamples = (float) (0.001 * fs);

    float* channels[2] = { left, right };

    for (int i = 0; i < numSamples; ++i)
    {
        hpMix     += (hpTarget - hpMix) * mixSmoothing;
        phaserMix += (phaserTarget - phaserMix) * mixSmoothing;
        flangerMix += (flangerTarget - flangerMix) * mixSmoothing;
        depth     += (settings.depth - depth) * paramSmoothing;
        rate      += (settings.rateHz - rate) * paramSmoothing;
        mix       += (settings.mix - mix) * paramSmoothing;

        phase += rate / fs;
        if (phase >= 1.0)
            phase -= 1.0;

        if (--countdown <= 0)
        {
            countdown = 16;
            updateCoefficients (0, (float) std::sin (twoPi * phase));
            updateCoefficients (1, (float) std::sin (twoPi * (phase + 0.25)));
        }

        for (int c = 0; c < 2; ++c)
        {
            const float dry = channels[c][i];
            float x = dry;

            // Moving high-pass
            float lpOut, hpOut;
            hp[c].tick (x, lpOut, hpOut);
            x += hpMix * (hpOut - x);

            // Phaser layer: x + all-pass chain, equal-power mix (notches, average level kept)
            float ap = x + 0.25f * phaserFeedback[c];
            for (auto& z : apState[c])
            {
                const float y = apCoeff[c] * ap + z;
                z = ap - apCoeff[c] * y;
                ap = y;
            }
            phaserFeedback[c] = ap;
            const float phased = 0.7071f * (x + ap);
            x += phaserMix * (phased - x);

            // Flanger: short swept delay with feedback, equal-power mix
            const float lfo = (float) std::sin (twoPi * (phase + 0.25 * c));
            const float delayMs = 0.3f + 5.0f * depth * (0.5f + 0.5f * lfo);
            const float d = flangeLines[c].read (delayMs * msToSamples);
            flangeLines[c].push (safetyClip (x + 0.45f * d));
            const float flanged = 0.7071f * (x + d);
            x += flangerMix * (flanged - x);

            channels[c][i] = dry + mix * (x - dry);
        }
    }
}

//==============================================================================
// StereoWidth
//==============================================================================
void StereoWidth::prepare (double sampleRate)
{
    fs = sampleRate;
    smoothing = onePoleCoeff (0.03, fs);
    midDelay.prepare ((int) std::ceil (0.02 * fs));
    decorrelationTone.setCutoff (6000.0f, fs);
    reset();
}

void StereoWidth::reset()
{
    midDelay.clear();
    decorrelationTone.reset();
    snapOnNextBlock = true;
}

void StereoWidth::process (float* left, float* right, int numSamples) noexcept
{
    if (snapOnNextBlock)
    {
        snapOnNextBlock = false;
        width = target;
    }

    const float decorrelationDelay = (float) (0.011 * fs);

    for (int i = 0; i < numSamples; ++i)
    {
        width += (target - width) * smoothing;

        const float mid  = 0.5f * (left[i] + right[i]);
        float side = 0.5f * (left[i] - right[i]);

        // keep the decorrelation line running so turning Width up is instant
        const float delayedMid = decorrelationTone.lowpass (midDelay.read (decorrelationDelay));
        midDelay.push (mid);

        if (width <= 1.0f)
        {
            side *= width;
        }
        else
        {
            const float extra = width - 1.0f;          // 0..1
            side = side * (1.0f + 0.6f * extra) + 0.5f * extra * delayedMid;
        }

        left[i]  = mid + side;
        right[i] = mid - side;
    }
}

//==============================================================================
// Ducker
//==============================================================================
void Ducker::prepare (double sampleRate)
{
    fs = sampleRate;
    envAttack  = onePoleCoeff (0.005, fs);
    envRelease = onePoleCoeff (0.06, fs);
    gainDown   = onePoleCoeff (0.01, fs);
    gainUp     = onePoleCoeff (0.15, fs);
    amountSmoothing = onePoleCoeff (0.05, fs);
    reset();
}

void Ducker::reset()
{
    envelope = 0.0f;
    gain = 1.0f;
    amount = targetAmount;
    reductionDb = 0.0f;
}

void Ducker::process (const float* keyL, const float* keyR, float* left, float* right, int numSamples) noexcept
{
    if (targetAmount <= 0.0f && amount < 1.0e-4f && gain > 0.9999f)
    {
        amount = 0.0f;
        gain = 1.0f;
        reductionDb = 0.0f;
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        amount += (targetAmount - amount) * amountSmoothing;

        const float level = std::max (std::abs (keyL[i]), std::abs (keyR[i]));
        envelope += (level - envelope) * (level > envelope ? envAttack : envRelease);

        // -42 dBFS and below: no ducking; -12 dBFS and above: full depth.
        const float envDb = 20.0f * std::log10 (envelope + 1.0e-6f);
        const float drive = std::clamp ((envDb + 42.0f) / 30.0f, 0.0f, 1.0f);
        const float targetGain = std::pow (10.0f, -36.0f * amount * drive / 20.0f);

        gain += (targetGain - gain) * (targetGain < gain ? gainDown : gainUp);
        left[i] *= gain;
        right[i] *= gain;
    }

    reductionDb = 20.0f * std::log10 (std::max (gain, 1.0e-4f));
}

} // namespace vd
