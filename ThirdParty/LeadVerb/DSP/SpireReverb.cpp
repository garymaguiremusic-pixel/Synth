#include "SpireReverb.h"

namespace lv
{
using namespace spire;

void SpireReverb::Biquad::setPeaking (double sampleRate, double hz, double q, double gainDb)
{
    // RBJ peaking EQ, computed in double and stored as float
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
    const double alpha = std::sin (w) / (2.0 * q);
    const double a0 = 1.0 + alpha / A;
    b0 = (float) ((1.0 + alpha * A) / a0);
    b1 = (float) (-2.0 * std::cos (w) / a0);
    b2 = (float) ((1.0 - alpha * A) / a0);
    a1 = (float) (-2.0 * std::cos (w) / a0);
    a2 = (float) ((1.0 - alpha / A) / a0);
}

void SpireReverb::prepare (double sampleRate, int newMode)
{
    mode = std::clamp (newMode, 0, 6);
    const auto& m = kModes[mode];

    fs = (float) sampleRate;
    invFs = 1.0f / fs;
    ratio = fs / kRefRate;
    K = ratio * (float) m.trr;

    // smoothing: v += (target - v) * sin(pi * Fc / fs), Fc = 24 Hz (240 Hz after a big jump)
    slowCoef = (float) std::sin (3.14159265358979323846 * 24.0 / sampleRate);
    fastCoef = (float) std::sin (3.14159265358979323846 * 240.0 / sampleRate);

    for (int k = 0; k < 4; ++k)
        lfo[k].rate = m.lfoHz[k] * invFs;

    // Size every line for the longest delay it can reach in this mode.
    const float maxMt = m.sizeMt[0] + std::max (0.0f, m.sizeMt[1]);
    float mtLen = 0.0f;
    for (int k = 0; k < m.numMt; ++k)
        mtLen = std::max (mtLen, ratio * m.mt[k].delay * maxMt);
    mt.init ((int) mtLen + 8);

    const float maxDiff = m.sizeDiff[0] + std::max (0.0f, m.sizeDiff[1]);
    for (int k = 0; k < 6; ++k)
        df[k].init ((int) (K * m.df[k].delay * maxDiff) + 8);

    // LFO values span about -0.5 .. 1.5
    auto wgLen = [this] (const Wg& g, float size, float depthB) { return (int) (ratio * g.delay * size + 1.5f * (g.depthA + depthB)) + 16; };
    for (int k = 0; k < 2; ++k)
    {
        apL[k].init ((int) (K * m.apL[k].delay) + 8);
        apR[k].init ((int) (K * m.apR[k].delay) + 8);
        wgL[k].init (wgLen (m.wgL[k], m.inside[k], k == 1 ? m.wgL[1].depthB : 0.0f));
        wgR[k].init (wgLen (m.wgR[k], m.inside[k], 0.0f));
    }
    for (int k = 0; k < 7; ++k)
    {
        clL[k].init ((int) (K * m.clL[k].delay) + 8);
        clR[k].init ((int) (K * m.clR[k].delay) + 8);
    }

    for (int k = 0; k < 2; ++k)
        notch[k].setPeaking (sampleRate, m.notch[k].hz, notchQ (m.notch[k].q), m.notch[k].gainDb);

    reset();
}

void SpireReverb::reset()
{
    for (auto& l : lfo)
        l.reset();
    mt.clear();
    for (auto& l : df) l.clear();
    for (auto& l : apL) l.clear();
    for (auto& l : apR) l.clear();
    for (auto& l : wgL) l.clear();
    for (auto& l : wgR) l.clear();
    for (auto& l : clL) l.clear();
    for (auto& l : clR) l.clear();
    for (auto& n : notch) n.clear();
    colorLp = colorHp = dampL = dampR = fbR = 0.0f;
}

void SpireReverb::setParameters (const Knobs& k) noexcept
{
    decay.set (std::clamp (k.decay, 0.0f, 1.0f));
    damp.set (std::clamp (k.damp, 0.0f, 1.0f));
    color.set (std::clamp (k.color, 0.0f, 1.0f));
    wide.set (std::clamp (k.wide, 0.0f, 2.0f));
}

void SpireReverb::snap() noexcept
{
    decay.snap();
    damp.snap();
    color.snap();
    wide.snap();
}

void SpireReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    const auto& m = kModes[mode];
    const bool taps = m.useTl != 0 && m.usePd != 0;
    const float invTrr = 1.0f / (float) m.trr;
    const float hpLowest = onePoleCoef (noteHz (23.8f), invFs), lp20k = onePoleCoef (20000.0f, invFs);

    for (int s = 0; s < n; ++s)
    {
        const float l0 = lfo[0].tick(), l1 = lfo[1].tick(), l2 = lfo[2].tick(), l3 = lfo[3].tick();

        const float D = decayCurve (decay.tick (slowCoef, fastCoef));
        const float dampKnob = damp.tick (slowCoef, fastCoef);
        const float c = color.tick (slowCoef, fastCoef);
        const float wd = wide.tick (slowCoef, fastCoef);

        // --- 1, 2: mono in, Color tilt ----------------------------------------------------
        float cLp, cHp;
        if (c < 0.49f)      { cLp = onePoleCoef (noteHz (c * 238.0f), invFs); cHp = hpLowest; }
        else if (c < 0.51f) { cLp = lp20k; cHp = hpLowest; }
        else                { cLp = lp20k; cHp = onePoleCoef (noteHz ((c - 0.5f) * 190.4f + 23.8f), invFs); }

        colorLp += (0.5f * (inL[s] + inR[s]) - colorLp) * cLp;
        colorHp += (colorLp - colorHp) * cHp;
        const float x = colorLp - colorHp;

        // (3: pre-delay is the engine's)

        const float fb = m.decTail[0] + D * m.decTail[1];
        const float mtScale = m.sizeMt[0] + D * m.sizeMt[1];
        const float dfScale = m.sizeDiff[0] + D * m.sizeDiff[1];

        // --- 4: early multitap --------------------------------------------------------------
        float in = x;
        if (m.useMt != 0)
        {
            float sum = x;
            for (int k = 0; k < m.numMt; ++k)
            {
                // (fs/REF*trr) * dly * mtScale / trr: trr cancels, but the truncation is Spire's
                const int d = (int) (invTrr * mtScale * K * m.mt[k].delay);
                sum += m.mt[k].gain * mt.read (std::min (d, mt.size - 1));
            }
            mt.write (x);
            in = sum;
        }

        // --- 5: input diffusers -----------------------------------------------------------
        if (m.useDf != 0)
            for (int k = 0; k < m.numDf; ++k)
                in = df[k].allpass (in, std::min ((int) (K * m.df[k].delay * dfScale), df[k].size - 1), m.df[k].gain);

        // --- 6: tank ----------------------------------------------------------------------
        const float cDamp = onePoleCoef (noteHz ((1.0f - dampCurve (dampKnob)) * 119.0f), invFs);
        const float sz0 = m.inside[0], sz1 = m.inside[1];
        auto wgRead = [] (const Line& line, float delay)
        {
            return line.hermite (std::clamp (delay, 2.0f, (float) line.size - 3.0f));
        };

        // left half
        const float a = in + fb * fbR;
        const float n0 = apL[0].allpass (a, (int) (K * m.apL[0].delay), m.apL[0].gain);
        const float d1 = wgRead (wgL[0], ratio * m.wgL[0].delay * sz0 + l2 * m.wgL[0].depthA);
        wgL[0].write (n0);
        dampL += (d1 - dampL) * cDamp;
        const float n1 = dampL;
        const float n2 = apL[1].allpass (n1 * fb, (int) (K * m.apL[1].delay), m.apL[1].gain);
        const float defaultL = 0.6f * (n1 + n2);
        const float d2 = wgRead (wgL[1], ratio * m.wgL[1].delay * sz1 + l1 * m.wgL[1].depthA - l2 * m.wgL[1].depthB);
        wgL[1].write (n2);

        // right half
        const float b = in + fb * d2;
        const float n3 = apR[0].allpass (b, (int) (K * m.apR[0].delay), m.apR[0].gain);
        const float d3 = wgRead (wgR[0], ratio * m.wgR[0].delay * sz0 + l0 * m.wgR[0].depthA);
        wgR[0].write (n3);
        dampR += (d3 - dampR) * cDamp;
        const float n4 = dampR;
        const float n5 = apR[1].allpass (n4 * fb, (int) (K * m.apR[1].delay), m.apR[1].gain);
        const float defaultR = 0.6f * (n4 + n5);
        const float d4 = wgRead (wgR[1], ratio * m.wgR[1].delay * sz1 + l3 * m.wgR[1].depthA);
        wgR[1].write (n5);
        fbR = d4;

        // --- 7: output taps -------------------------------------------------------------------
        float wetL = defaultL, wetR = defaultR;
        if (taps)
        {
            const float nodes[6] = { n0, n1, n2, n3, n4, n5 };
            wetL = wetR = 0.0f;
            for (int k = 0; k < 7; ++k)
            {
                const float tl = clL[k].read ((int) (K * m.clL[k].delay));
                clL[k].write (nodes[std::clamp (m.clL[k].node, 0, 5)]);
                wetL += m.clL[k].gain * tl;
                const float tr = clR[k].read ((int) (K * m.clR[k].delay));
                clR[k].write (nodes[std::clamp (m.clR[k].node, 0, 5)]);
                wetR += m.clR[k].gain * tr;
            }
        }

        // --- 8: fixed peaking EQs ---------------------------------------------------------------
        wetL = notch[1].process (notch[0].process (wetL, 0), 0);
        wetR = notch[1].process (notch[0].process (wetR, 1), 1);

        // --- 9: width -------------------------------------------------------------------------
        const float w = (wd + 1.0f) * 0.5f;
        outL[s] = w * wetL + (1.0f - w) * wetR;
        outR[s] = w * wetR + (1.0f - w) * wetL;
    }
}

} // namespace lv
