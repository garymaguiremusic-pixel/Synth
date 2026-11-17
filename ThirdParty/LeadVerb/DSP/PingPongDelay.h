#pragma once

#include "DspBuildingBlocks.h"

namespace lv
{

//==============================================================================
/**
    Tempo-synced ping-pong delay on the reverb output (the last stage of the wet
    path, just before the plugin's Dry/Wet).

    The (mono-summed) input goes into the left line; each line's output feeds the
    other, so the repeats bounce L, R, L, R ... exactly on the grid. To make it
    bigger than a plain ping-pong:

      - Movement: each side's read tap drifts slowly (smooth random wander, about
        +-1.5 ms) like a tape echo, so the repeats never sit still.
      - Depth: every pass goes through a short allpass diffuser, so each repeat is a
        little softer and more smeared than the last and they melt into a wash.
        Highs and lows also fade on each pass (like an analogue echo).
      - Width: the repeats are panned hard left and right, and a slow cross-stereo
        ensemble puts a moving, delayed ghost of each one on the far side (11-20 ms,
        like a Haas spread), so they fill the whole field without blurring the bounce.

    Mix crossfades from the incoming (reverb) signal to the echoes: the input stays
    at full level up to 50 % while the echoes rise, then fades out towards 100 %.
    Turning the delay off fades the echoes out over 30 ms and clears the lines.
*/
class PingPongDelay
{
public:
    static constexpr float kMaxDelaySeconds = 10.0f;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        const int maxLen = (int) std::ceil ((kMaxDelaySeconds + 0.01f) * fs) + 8;
        for (auto& line : lines)
            line.init (maxLen);

        constexpr float diffMs[2][2] = { { 7.3f, 11.9f }, { 8.9f, 13.1f } };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k)
            {
                diffusers[c][k].init ((int) (diffMs[c][k] * 0.001f * fs) + 4);
                diffusers[c][k].setLength ((int) (diffMs[c][k] * 0.001f * fs));
                diffusers[c][k].setGain (0.5f);
            }

        for (int c = 0; c < 2; ++c)
        {
            highCut[c].setCutoff (6500.0f, fs);
            lowCut[c].setCutoff (120.0f, fs);
            wander[c].prepare (c == 0 ? 0.31f : 0.43f, fs, (uint32_t) (4441 + 17 * c));
        }
        modDepth = 0.0015f * fs;

        ensemble.prepare (fs);
        ensemble.setAmount (0.4f);

        smoothing = 1.0f - std::exp (-1.0f / (0.03f * fs));
        fadeStep = 1.0f / (0.05f * fs);
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
            line.clear();
        for (auto& side : diffusers)
            for (auto& d : side)
                d.clear();
        for (int c = 0; c < 2; ++c)
        {
            highCut[c].clear();
            lowCut[c].clear();
        }
        ensemble.clear();
        ensemble.snap();
        current = next = target;
        fading = false;
        dryGain = dryTarget;
        wetGain = wetTarget;
        idle = ! on;
    }

    /** seconds: the synced delay time; feedback 0 .. 1 (already scaled); mix 0 .. 1. */
    void setParameters (bool enabled, float seconds, float feedbackAmount, float mix) noexcept
    {
        on = enabled;
        target = std::clamp (seconds * fs, modDepth + 4.0f, (float) lines[0].capacity() - 4.0f - modDepth);
        feedback = std::clamp (feedbackAmount, 0.0f, 0.95f);
        const float m = std::clamp (mix, 0.0f, 1.0f);
        dryTarget = on ? std::min (1.0f, 2.0f * (1.0f - m)) : 1.0f;
        wetTarget = on ? std::min (1.0f, 2.0f * m) : 0.0f;

        if (on && idle)
        {
            // starting fresh: jump straight to the time, no crossfade from a stale one
            current = next = target;
            fading = false;
            idle = false;
        }
    }

    /** Snap the smoothed mix to its target (after a reset, so nothing ramps in). */
    void snap() noexcept
    {
        dryGain = dryTarget;
        wetGain = wetTarget;
        if (! fading)
            current = next = target;
    }

    float getDelaySamples() const noexcept { return target; }

    /** In place, on the stereo wet signal. */
    void process (float* left, float* right, int n) noexcept
    {
        if (idle)
            return;

        for (int i = 0; i < n; ++i)
        {
            dryGain += (dryTarget - dryGain) * smoothing;
            wetGain += (wetTarget - wetGain) * smoothing;

            // tempo or note value changed: crossfade to the new read position (no pitch bend)
            if (! fading && std::abs (target - current) > 0.5f)
            {
                next = target;
                fade = 0.0f;
                fading = true;
            }

            float echo[2];
            for (int c = 0; c < 2; ++c)
            {
                const float mod = modDepth * wander[c].tick();   // centred: repeats stay on the grid
                float e = lines[c].readCubic (current + mod);
                if (fading)
                    e += (lines[c].readCubic (next + mod) - e) * fade;
                echo[c] = e;
            }
            if (fading)
            {
                fade += fadeStep;
                if (fade >= 1.0f)
                {
                    current = next;
                    fading = false;
                }
            }

            // each side's repeat feeds the other side: darker, thinner and more diffuse each pass
            float back[2];
            for (int c = 0; c < 2; ++c)
            {
                float x = lowCut[c].process (highCut[c].process (echo[1 - c]));
                x = diffusers[c][1].process (diffusers[c][0].process (x));
                back[c] = softClip (feedback * x);
            }

            const float input = on ? 0.7071f * (left[i] + right[i]) : 0.0f;
            lines[0].write (input + back[0]);
            lines[1].write (back[1]);

            // a moving ghost of every repeat on the far side: wide and deep, while the
            // repeat itself stays hard on its own side, so the bounce stays clear
            float l = echo[0], r = echo[1];
            ensemble.process (l, r);

            left[i]  = left[i]  * dryGain + l * wetGain;
            right[i] = right[i] * dryGain + r * wetGain;
        }

        // switched off and faded out: stop and clear, so turning it on starts clean
        if (! on && wetGain < 1.0e-4f)
        {
            wetGain = 0.0f;
            dryGain = 1.0f;
            reset();
            idle = true;
        }
    }

private:
    static inline float softClip (float x) noexcept
    {
        // transparent below about 0.5, then rounds off: high feedback can't run away
        const float a = std::abs (x);
        return a < 0.5f ? x : std::copysign (0.5f + (a - 0.5f) / (1.0f + 2.0f * (a - 0.5f)), x);
    }

    float fs = 44100.0f;
    DelayLine lines[2];
    Allpass diffusers[2][2];
    OnePoleLP highCut[2];
    OnePoleHP lowCut[2];
    Wander wander[2];
    Ensemble ensemble;

    bool on = false, idle = true, fading = false;
    float target = 4.0f, current = 4.0f, next = 4.0f, fade = 0.0f, fadeStep = 0.001f;
    float modDepth = 0.0f, feedback = 0.0f;
    float dryGain = 1.0f, wetGain = 0.0f, dryTarget = 1.0f, wetTarget = 0.0f, smoothing = 0.001f;
};

} // namespace lv
