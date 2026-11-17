#include "MassiveReverb.h"

namespace lv
{
namespace
{
    // [OBSERVED] stage delay maxima (seconds): 0.12231 * 2^x + 0.00051, quarter-octave spacing
    constexpr float kStage1Seconds[MassiveReverb::kLanes] = { 0.12604f, 0.10606f, 0.08926f, 0.07514f };
    constexpr float kStage2Seconds[MassiveReverb::kLanes] = { 0.13428f, 0.11299f, 0.09509f, 0.08006f };
    // [OBSERVED] long lines: (0.12231 * 2^x * 0.021 * 1.1469 + c) seconds
    constexpr float kLongX[MassiveReverb::kLanes] = { 6.6667f, 7.3333f, 5.8f, 5.2f };
    constexpr float kLongC[MassiveReverb::kLanes] = { 0.06f, 0.0726f, 0.07986f, 0.066f };
    // [OBSERVED] 4 short lines of 14 ms (+3 samples); [INFERRED] used as modulated diffusers,
    // [UNKNOWN] their lengths within the 14 ms
    constexpr float kShortSeconds = 0.014f;
    constexpr float kShortTaps[MassiveReverb::kLanes] = { 1.0f, 0.83f, 0.67f, 0.53f };
    // [OBSERVED] LFO lane phase offsets, 1/8 cycle apart
    constexpr float kLanePhase[MassiveReverb::kLanes] = { 0.5f, 0.375f, 0.25f, 0.125f };

    const MassiveReverb::TypeConstants kTypes[2] =
    {
        // Reverb (type 0)
        { 0.0202f, 0.0198f, false, 130.81f, 3520.0f, 0.92f, 0.20f,
          { -1.0f / 6.0f, -0.5f, 4.0f, 1.0f }, { 1.0f / 6.0f, 0.5f, -4.0833f, 73.0f } },
        // Small Reverb (type 1)
        { 0.0f, 0.0f, true, 130.81f, 1174.7f, 0.68f, 0.16f,
          { -0.25f, -3.6667f, 4.0f, 1.0f }, { 1.0f / 6.0f, 3.5833f, -2.9167f, 55.0f } },
    };

    inline float wrapHalf (float p) { return p - std::floor (p + 0.5f); }   // -> [-0.5, 0.5)
}

const MassiveReverb::TypeConstants& MassiveReverb::constants (int t) { return kTypes[t == Small ? 1 : 0]; }

MassiveReverb::Internals MassiveReverb::defaultInternals (int t)
{
    Internals in;
    if (t == Small)
    {
        in.rtMin = 0.3f;
        in.rtMax = 2.2f;
        in.sizeMin = 0.12f;
        in.sizeMax = 0.45f;
        in.outputGain = 17.0f;
    }
    return in;
}

float MassiveReverb::getPredelayMs() const noexcept
{
    const auto& c = constants (type);
    return c.bypassPredelay ? 0.0f : 1000.0f * (0.5f * (float) (predelaySamples[0] + predelaySamples[1])) / fs;
}

void MassiveReverb::prepare (double sampleRate, int newType)
{
    type = newType == Small ? Small : Reverb;
    fs = (float) sampleRate;
    const auto& c = constants (type);
    internals = defaultInternals (type);

    // [OBSERVED] pre-delay: round(t * sr) + 3 samples (Reverb), 2 samples / bypassed (Small)
    predelaySamples[0] = c.bypassPredelay ? 2 : (int) std::lround (c.predelayL * fs) + 3;
    predelaySamples[1] = c.bypassPredelay ? 2 : (int) std::lround (c.predelayR * fs) + 3;
    for (int ch = 0; ch < 2; ++ch)
        predelay[ch].init (predelaySamples[ch] + 4);

    for (int k = 0; k < kLanes; ++k)
    {
        base1[k] = kStage1Seconds[k] - 0.00051f;   // leave the observed headroom for the modulation
        base2[k] = kStage2Seconds[k] - 0.00051f;
        stage1[k].line.init ((int) (kStage1Seconds[k] * fs + 1.0f) + 8);
        stage2[k].line.init ((int) (kStage2Seconds[k] * fs + 1.0f) + 8);

        longMax[k] = (0.12231f * std::pow (2.0f, kLongX[k]) * 0.021f * 1.1469f + kLongC[k]) * fs;
        longLine[k].init ((int) longMax[k] + 8);

        shortMax[k] = kShortSeconds * fs + 3.0f;
        shortAp[k].line.init ((int) shortMax[k] + 8);
    }

    lfoStep = 0.5f / fs;   // [OBSERVED] 0.5 Hz
    smoothing = 1.0f - std::exp (-32.0f / (0.05f * fs));   // per 32-sample update, ~50 ms
    reset();
}

void MassiveReverb::reset()
{
    for (auto& d : predelay) d.clear();
    for (int k = 0; k < kLanes; ++k)
    {
        stage1[k].line.clear();
        stage2[k].line.clear();
        highPass[k].reset();
        lowPass[k].reset();
        longLine[k].clear();
        longDamp[k].clear();
        shortAp[k].line.clear();
    }
    lfoPhase = 0.0f;
    fb0 = fb1 = 0.0f;
    smoothed = knobs;
    coefCountdown = 0;
    updateCoefficients();
}

void MassiveReverb::setKnobs (const Knobs& k) noexcept
{
    knobs.size = std::clamp (k.size, 0.0f, 1.0f);
    knobs.density = std::clamp (k.density, 0.0f, 1.0f);
    knobs.color = std::clamp (k.color, 0.0f, 1.0f);
}

void MassiveReverb::setInternals (const Internals& in) noexcept
{
    internals = in;
    coefCountdown = 0;
}

void MassiveReverb::updateCoefficients() noexcept
{
    const auto& c = constants (type);
    const auto& in = internals;

    smoothed.size += (knobs.size - smoothed.size) * smoothing;
    smoothed.density += (knobs.density - smoothed.density) * smoothing;
    smoothed.color += (knobs.color - smoothed.color) * smoothing;

    sizeScale = in.sizeMin + (in.sizeMax - in.sizeMin) * smoothed.size;
    targetRt60 = in.rtMin * std::pow (in.rtMax / std::max (0.01f, in.rtMin), smoothed.size);

    apGain = in.apGainMin + (in.apGainMax - in.apGainMin) * smoothed.density;
    apGain0 = apGain * in.apGain0Ratio;

    // decay: gain per trip through one lane, never above the observed ceiling E
    auto gainFor = [this, &c] (float seconds) { return std::min (c.gainE, std::pow (10.0f, -3.0f * seconds / std::max (0.05f, targetRt60))); };
    const float laneSeconds = 0.5f * (base1[0] + base2[0] + base1[1] + base2[1]) * sizeScale;
    fbGain = gainFor (laneSeconds);
    for (int k = 0; k < kLanes; ++k)
        longGain[k] = gainFor (longMax[k] * sizeScale / fs);

    // filters: 130.81 Hz high-pass, damping low-pass at A moved by Color
    const float lpHz = std::min (0.45f * fs, c.dampHz * std::pow (2.0f, (smoothed.color - 0.5f) * 2.0f * in.colorOctaves));
    for (int k = 0; k < kLanes; ++k)
    {
        highPass[k].set (c.lowCutHz, in.hpQ, fs);
        lowPass[k].set (lpHz, in.lpQ, fs);
        longDamp[k].setCutoff (std::min (0.45f * fs, lpHz * 1.5f), fs);
    }
}

void MassiveReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const auto& c = constants (type);
    const auto& in = internals;

    for (int s = 0; s < n; ++s)
    {
        if (--coefCountdown <= 0)
        {
            coefCountdown = 32;
            updateCoefficients();
        }

        // 1: pre-delay (or the direct input)
        predelay[0].write (inL[s]);
        predelay[1].write (inR[s]);
        // (written first, so read(n + 1) is n samples back)
        const float l = c.bypassPredelay ? inL[s] : predelay[0].read (predelaySamples[0] + 1);
        const float r = c.bypassPredelay ? inR[s] : predelay[1].read (predelaySamples[1] + 1);

        // 3: LFO, 0.5 Hz, lanes 1/8 cycle apart: m = (8 - k|p|) p 0.0005 (seconds)
        lfoPhase = wrapHalf (lfoPhase + lfoStep);

        float lane[kLanes];
        for (int k = 0; k < kLanes; ++k)
        {
            const float p = wrapHalf (lfoPhase + kLanePhase[k]);
            const float m = (8.0f - in.lfoShape * std::abs (p)) * p * 0.0005f;
            const float offset = k == 0 ? 0.0f : 3.0f;
            const float g = k == 0 ? apGain0 : apGain;

            // 2: lane input through its short diffuser; lanes 0 / 1 cross-coupled by the feedback
            const float capS = (float) shortAp[k].line.capacity() - 4.0f;
            const float posS = std::clamp (shortMax[k] * kShortTaps[k] * std::sqrt (sizeScale) + 0.1f * m * fs, 3.0f, capS);
            float xk = shortAp[k].process (c.gainD * (k % 2 == 0 ? l : r), posS, apGain * in.diffuserRatio);
            if (k == 0) xk += fbGain * fb1;
            if (k == 1) xk += fbGain * fb0;

            const float cap1 = (float) stage1[k].line.capacity() - 4.0f, cap2 = (float) stage2[k].line.capacity() - 4.0f;
            const float pos1 = std::clamp ((m + base1[k]) * sizeScale * fs + offset, 3.0f, cap1);
            const float pos2 = std::clamp ((-m + base2[k]) * sizeScale * fs + offset, 3.0f, cap2);

            float y = stage1[k].process (xk, pos1, g);      // 3: stage 1, +m
            y = stage2[k].process (y, pos2, g);               // 4: stage 2, -m

            float lp, hp;                                     // 5: filters
            highPass[k].tick (y, lp, hp);
            lane[k] = lowPass[k].tick2 (hp);
        }
        fb0 = lane[0];                                        // 6: feedback state
        fb1 = lane[1];

        // long lines: 4x4 Householder FDN fed by the lanes [UNKNOWN routing]
        float tap[kLanes], sum = 0.0f;
        for (int k = 0; k < kLanes; ++k)
        {
            tap[k] = longLine[k].readFrac (std::max (2.0f, longMax[k] * sizeScale));
            sum += tap[k];
        }
        const float h = 0.5f * sum;
        for (int k = 0; k < kLanes; ++k)
            longLine[k].write (0.5f * lane[k] + longGain[k] * longDamp[k].process (tap[k] - h));

        outL[s] = in.outputGain * (in.laneMix * (lane[0] + lane[2]) + in.fdnMix * (tap[0] + tap[2]));
        outR[s] = in.outputGain * (in.laneMix * (lane[1] + lane[3]) + in.fdnMix * (tap[1] + tap[3]));
    }
}

} // namespace lv
