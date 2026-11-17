#pragma once

#include <algorithm>
#include <cmath>

namespace lv
{

//==============================================================================
/**
    Sidechain ducking for the wet signal ("Sidechain" knob).

    The dry input is the key: a fast peak follower measures its level, and
    everything above -45 dBFS pushes the wet signal down dB for dB, up to 40 dB
    at full amount. The gain reduction itself clamps down in 10 ms and lets go
    over 250 ms, so the reverb is held back while the source plays and swells
    up around it in the gaps - the pumping reverb used in EDM. Releasing in dB
    makes the swell take the same time however loud the source was.
*/
class Ducker
{
public:
    static constexpr float kThresholdDb   = -45.0f;
    static constexpr float kMaxReductionDb = 40.0f;

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        detectAttack  = 1.0f - std::exp (-1.0f / (0.001f * fs));   // level detector
        detectRelease = 1.0f - std::exp (-1.0f / (0.03f * fs));    // holds across waveform cycles
        attack  = 1.0f - std::exp (-1.0f / (0.01f * fs));          // duck in 10 ms
        release = 1.0f - std::exp (-1.0f / (0.25f * fs));          // swell back over 250 ms
        smoothing = 1.0f - std::exp (-1.0f / (0.02f * fs));
        reset();
    }

    void reset() noexcept
    {
        envelope = 0.0f;
        reductionDb = 0.0f;
        amount = amountTarget;
        maxReductionDb = 0.0f;
    }

    /** 0 = off, 1 = full ducking. */
    void setAmount (float a) noexcept { amountTarget = std::clamp (a, 0.0f, 1.0f); }

    void process (const float* keyL, const float* keyR, float* wetL, float* wetR, int numSamples) noexcept
    {
        float maxRed = maxReductionDb;

        for (int i = 0; i < numSamples; ++i)
        {
            const float level = std::max (std::abs (keyL[i]), std::abs (keyR[i]));
            envelope += (level - envelope) * (level > envelope ? detectAttack : detectRelease);
            amount += (amountTarget - amount) * smoothing;

            const float envDb = 20.0f * std::log10 (envelope + 1.0e-9f);
            const float targetDb = amount * std::clamp (envDb - kThresholdDb, 0.0f, kMaxReductionDb);
            reductionDb += (targetDb - reductionDb) * (targetDb > reductionDb ? attack : release);

            if (reductionDb < 1.0e-3f)
                continue;

            const float gain = std::pow (10.0f, -0.05f * reductionDb);

            wetL[i] *= gain;
            wetR[i] *= gain;
            maxRed = std::max (maxRed, reductionDb);
        }

        maxReductionDb = maxRed;
    }

    /** Largest gain reduction (dB, positive) since the last call. */
    float getAndResetReductionDb() noexcept { const float r = maxReductionDb; maxReductionDb = 0.0f; return r; }

private:
    float fs = 44100.0f;
    float detectAttack = 0.1f, detectRelease = 0.001f;
    float attack = 0.01f, release = 0.0001f, smoothing = 0.001f;
    float envelope = 0.0f, reductionDb = 0.0f, amount = 0.0f, amountTarget = 0.0f;
    float maxReductionDb = 0.0f;
};

} // namespace lv
