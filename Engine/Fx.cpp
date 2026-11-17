#include "Fx.h"
#include "Synth.h"
#include "VirusCLayout.h"

namespace vc
{
using namespace layout::off;

//==============================================================================
double Shaper::staticCurve (int curve, double x, double a)
{
    // `a` 0..1 scales pre-gain for the analog curves (guessed transfer functions)
    switch (curve)
    {
        case Light:     { const double g = 1.0 + a * 2.0;  return std::tanh (x * g * 0.5) / std::tanh (0.5 * g) * 0.9; }
        case Soft:      { const double g = 1.0 + a * 4.0;  return std::tanh (x * g) / std::tanh (g); }
        case Middle:    { const double g = 1.0 + a * 8.0;  const double y = x * g; return y / std::sqrt (1.0 + y * y) * std::sqrt (1.0 + g * g) / g; }
        case Hard:      { const double g = 1.0 + a * 16.0; return std::clamp (x * g, -1.0, 1.0); }
        case Digital:   { const double g = 1.0 + a * 24.0; double y = x * g; y = std::fmod (y + 1.0, 4.0); if (y < 0) y += 4.0; return y < 2.0 ? y - 1.0 : 3.0 - y; }   // fold
        case ShaperC:   { const double g = 1.0 + a * 6.0;  return std::sin (x * g * dsp::kPi * 0.5); }
        case Rectifier: { const double g = 1.0 + a * 2.0;  return std::abs (x * g) * 2.0 - 0.5 * g; }
        default:        return x;
    }
}

double Shaper::process (int curve, double x, double a, double noteHz, double sr)
{
    switch (curve)
    {
        case Off: return x;
        case BitReducer:
        {
            const double bits = 24.0 - a * 22.0;
            const double q = std::pow (2.0, bits - 1.0);
            return std::round (x * q) / q;
        }
        case RateReducer:
        case RateFollow:
        {
            const double target = curve == RateReducer ? sr * std::pow (2.0, -a * 9.0)
                                                       : noteHz * std::pow (2.0, (1.0 - a) * 6.0);
            const double step = std::max (1.0, sr / std::max (20.0, target));
            counter += 1.0;
            if (counter >= step) { counter -= step; hold = x; }
            return hold;
        }
        case LowPass: case LowFollow: case HighPass: case HighFollow:
        {
            const bool follow = curve == LowFollow || curve == HighFollow;
            const bool low = curve == LowPass || curve == LowFollow;
            const double hz = follow ? noteHz * std::pow (2.0, (low ? (1.0 - a) : a) * 7.0)
                                     : 20.0 * std::pow (1000.0, low ? (1.0 - a) : a);
            lp.setCutoff (hz, sr);
            return low ? lp.lp (x) : lp.hp (x);
        }
        default:
            return staticCurve (curve, x, a);
    }
}

//==============================================================================
FxChain::FxChain()
{
    chL.init (4096); chR.init (4096);
    dlL.init (65536); dlR.init (65536);
    pre.init (32768);
    static constexpr int lens[kLines] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
    for (int i = 0; i < kLines; ++i) rv[(size_t) i].init (lens[i] * 4);
    dlTimeL.setTime (0.08, sr); dlTimeR.setTime (0.08, sr);
    rng.seed (0x51ED);
}

void FxChain::reset()
{
    distL.reset(); distR.reset();
    for (auto* b : { &boostL, &boostR, &lowL, &lowR, &midL, &midR, &highL, &highR }) b->reset();
    chL.clear(); chR.clear(); dlL.clear(); dlR.clear(); pre.clear();
    for (auto& d : rv) d.clear();
    rvLp.fill (0.0);
    for (auto& a : apL) a.z = 0;
    for (auto& a : apR) a.z = 0;
    chFbL = chFbR = phFbL = phFbR = 0;
    dlTimeValid = false;
    rvPrev = 0.0;
    dlColL.reset(); dlColR.reset(); dlColHL.reset(); dlColHR.reset();
    rng.seed (0x51ED);
    chPhase = phPhase = dlPhase = 0;
}

namespace
{
    double lfoShapeValue (int shape, double ph, double& sh, double& prevPh, dsp::Rng& rng)
    {
        ph -= std::floor (ph);
        const bool wrapped = ph < prevPh;
        prevPh = ph;
        switch (shape)
        {
            case 0: return std::sin (dsp::kTwoPi * ph);
            case 1: return ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph;
            case 2: return 2.0 * ph - 1.0;
            case 3: return ph < 0.5 ? 1.0 : -1.0;
            default: if (wrapped) sh = rng.bipolar(); return sh;
        }
    }
}

void FxChain::process (float* L, float* R, int n, const ParamState& p, double bpm, const std::array<float, 256>& mod)
{
    auto pv = [&] (int o) { return (double) std::clamp (p.cur[(size_t) o] + mod[(size_t) o], (float) layout::kParams[o].min, (float) layout::kParams[o].max); };

    // --- distortion -----------------------------------------------------------------
    const int distCurve = distToCurve (p.i (Distortion_Curve));
    const double distAmt = pv (Distortion_Intensity) / 127.0;

    // --- EQ / boost (coefficients refreshed per block) --------------------------------
    const double boost = pv (Bass_Intensity) / 127.0;
    boostL.peak (tables::bassTuneHz ((int) pv (Bass_Tune)), 0.9, boost * 18.0, sr); boostR = boostL;
    const double lowDb = tables::eqGainDb ((int) pv (LowEQ_Gain)), midDb = tables::eqGainDb ((int) pv (MidEQ_Gain)), highDb = tables::eqGainDb ((int) pv (HighEQ_Gain));
    {
        auto keepState = [] (dsp::Biquad& dst, const dsp::Biquad& coeffs) { dst.b0 = coeffs.b0; dst.b1 = coeffs.b1; dst.b2 = coeffs.b2; dst.a1 = coeffs.a1; dst.a2 = coeffs.a2; };
        dsp::Biquad c;
        c.lowShelf (tables::eqLowHz ((int) pv (LowEQ_Frequency)), lowDb, sr);   keepState (lowL, c);  keepState (lowR, c);
        c.peak (tables::eqMidHz ((int) pv (MidEQ_Frequency)), tables::eqQ ((int) pv (MidEQ_Q_Factor)), midDb, sr); keepState (midL, c); keepState (midR, c);
        c.highShelf (tables::eqHighHz ((int) pv (HighEQ_Frequency)), highDb, sr); keepState (highL, c); keepState (highR, c);
        c.peak (tables::bassTuneHz ((int) pv (Bass_Tune)), 0.9, boost * 18.0, sr); keepState (boostL, c); keepState (boostR, c);
    }
    const bool eqOn = std::abs (lowDb) > 0.01 || std::abs (midDb) > 0.01 || std::abs (highDb) > 0.01;

    // --- chorus ---------------------------------------------------------------------
    const double chMix = pv (Chorus_Mix) / 127.0;
    const double chRate = tables::expMap (pv (Chorus_Rate) / 127.0, 0.02, 10.0);
    const double chDepthMs = pv (Chorus_Depth) / 127.0 * 6.0;
    const double chDelayMs = tables::chorusDelayMs (pv (Chorus_Delay));
    const double chFb = (pv (Chorus_Feedback) - 64.0) / 64.0 * 0.9;
    const int chShape = std::clamp (p.i (Chorus_Lfo_Shape), 0, 5);

    // --- phaser ---------------------------------------------------------------------
    const double phMix = pv (Phaser_Mix) / 127.0;
    const int phStages = std::clamp (p.i (Phaser_Mode), 1, 6) * 2;
    const double phRate = tables::expMap (pv (Phaser_Rate) / 127.0, 0.02, 10.0);
    const double phDepth = pv (Phaser_Depth) / 127.0;
    const double phFreq = tables::phaserFreqHz ((int) pv (Phaser_Frequency));
    const double phFb = (pv (Phaser_Feedback) - 64.0) / 64.0 * 0.9;
    const double phSpread = pv (Phaser_Spread) / 127.0;

    // --- delay / reverb -------------------------------------------------------------
    const int dMode = std::clamp (p.i (Delay_Reverb_Mode), 0, 26);
    const double send = tables::volumeGain (pv (Effect_Send));
    const bool isReverb = dMode >= 2 && dMode <= 4;
    const bool isDelay = dMode == 1 || dMode >= 5;
    double baseMs = tables::delayTimeMs (pv (Delay_Time));
    const int clk = p.i (Delay_Clock);
    if (clk > 0) baseMs = 60000.0 / std::max (1.0, bpm) * tables::divisionBeats (clk);
    baseMs = std::min (baseMs, 1400.0);
    double ratioL = 1.0, ratioR = 1.0;
    if (dMode >= 5 && dMode <= 8)
    {
        static constexpr double rl[4] = { 1.0, 1.0, 1.0, 1.0 }, rr[4] = { 0.5, 0.75, 0.25, 0.875 };   // 2:1, 4:3, 4:1, 8:7
        ratioL = rl[dMode - 5]; ratioR = rr[dMode - 5];
    }
    else if (dMode >= 9 && dMode <= 26)
    {
        static constexpr int pa[18][2] = { {1,1},{2,1},{3,1},{4,1},{5,1},{2,3},{2,5},{3,2},{3,3},{3,4},{3,5},{4,3},{4,5},{5,2},{5,3},{5,4},{5,5},{3,2} };
        ratioL = pa[dMode - 9][0] / 5.0; ratioR = pa[dMode - 9][1] / 5.0;
    }
    const double fb = pv (Delay_Feedback) / 127.0 * 0.98;
    const double dRate = tables::expMap (pv (Dly_Rate_Rev_Decay) / 127.0, 0.02, 10.0);
    const double dDepth = pv (Dly_Depth) / 127.0;
    const int dShape = std::clamp (p.i (Reverb_Damping), 0, 5);            // delay modes: byte 118 = LFO shape
    const double color = (pv (Delay_Color) - 64.0) / 64.0;
    const double colLpHz = color < 0 ? 20000.0 * std::pow (0.01, -color) : 20000.0;
    const double colHpHz = color > 0 ? 20.0 * std::pow (100.0, color) : 10.0;
    dlColL.setCutoff (colLpHz, sr); dlColR.g = dlColL.g;
    dlColHL.setCutoff (colHpHz, sr); dlColHR.g = dlColHL.g;

    const double rvDecay = tables::reverbDecaySec (p.i (Dly_Rate_Rev_Decay));
    const double rvDamp = pv (Reverb_Damping) / 127.0;
    const int rvSize = std::clamp (p.i (Dly_Depth), 0, 3);
    const double sizeScale = 0.45 + 0.25 * rvSize;
    const double preMs = baseMs * 0.25;
    const double rvFeedbackFromDelay = dMode == 3 ? fb * 0.5 : dMode == 4 ? fb * 0.8 : 0.0;

    double shPrevCh = 0, shCh = 0;
    for (int i = 0; i < n; ++i)
    {
        double l = L[i], r = R[i];

        if (distCurve != 0)
        {
            const double gain = 1.0 + distAmt * 6.0;
            l = distL.process (distCurve, l * gain, distAmt, 220.0, sr) / std::sqrt (gain);
            r = distR.process (distCurve, r * gain, distAmt, 220.0, sr) / std::sqrt (gain);
        }
        if (boost > 0.0) { l = boostL.process (l); r = boostR.process (r); }
        if (eqOn)
        {
            l = highL.process (midL.process (lowL.process (l)));
            r = highR.process (midR.process (lowR.process (r)));
        }

        if (chMix > 0.0)
        {
            chPhase += chRate / sr;
            if (chPhase >= 1.0) chPhase -= 1.0;
            const double mL = lfoShapeValue (chShape, chPhase, shCh, shPrevCh, rng);
            const double mR = lfoShapeValue (chShape == 4 || chShape == 5 ? 0 : chShape, chPhase + 0.25, shCh, shPrevCh, rng);
            const double dL = (chDelayMs + chDepthMs * (0.5 + 0.5 * mL)) * sr / 1000.0;
            const double dR = (chDelayMs + chDepthMs * (0.5 + 0.5 * mR)) * sr / 1000.0;
            const double wL = chL.read (dL), wR = chR.read (dR);
            chL.push ((float) (l + wL * chFb));
            chR.push ((float) (r + wR * chFb));
            l = l * (1.0 - chMix * 0.5) + wL * chMix;
            r = r * (1.0 - chMix * 0.5) + wR * chMix;
        }

        if (phMix > 0.0)
        {
            phPhase += phRate / sr;
            if (phPhase >= 1.0) phPhase -= 1.0;
            const double mL = std::sin (dsp::kTwoPi * phPhase);
            const double mR = std::sin (dsp::kTwoPi * (phPhase + 0.5 * phSpread));
            auto coef = [&] (double m)
            {
                const double f = std::clamp (phFreq * std::pow (2.0, m * phDepth * 3.0), 20.0, sr * 0.45);
                const double t = std::tan (dsp::kPi * f / sr);
                return (t - 1.0) / (t + 1.0);
            };
            const double aL = coef (mL), aR = coef (mR);
            double xl = l + phFbL * phFb, xr = r + phFbR * phFb;
            for (int s = 0; s < phStages; ++s) { xl = apL[(size_t) s].tick (xl, aL); xr = apR[(size_t) s].tick (xr, aR); }
            phFbL = xl; phFbR = xr;
            l = l * (1.0 - phMix * 0.5) + xl * phMix * 0.5;
            r = r * (1.0 - phMix * 0.5) + xr * phMix * 0.5;
        }

        if (send > 0.0 && isDelay)
        {
            dlPhase += dRate / sr;
            if (dlPhase >= 1.0) dlPhase -= 1.0;
            double prev = dlPhase - dRate / sr;
            const double m = lfoShapeValue (dShape, dlPhase, dlSh, prev, rng);
            const double modS = m * dDepth * 0.004 * sr;
            if (! dlTimeValid)
            {
                dlTimeL.reset (baseMs * ratioL * sr / 1000.0);
                dlTimeR.reset (baseMs * ratioR * sr / 1000.0);
                dlTimeValid = true;
            }
            const double tL = dlTimeL.process (baseMs * ratioL * sr / 1000.0);
            const double tR = dlTimeR.process (baseMs * ratioR * sr / 1000.0);
            double wl = dlL.read (tL + modS + 2.0), wr = dlR.read (tR - modS + 2.0);
            wl = dlColHL.hp (dlColL.lp (wl));
            wr = dlColHR.hp (dlColR.lp (wr));
            const bool pingPong = dMode >= 5 && dMode <= 8;
            if (pingPong)
            {
                dlL.push ((float) (0.5 * (l + r) * send + wr * fb));
                dlR.push ((float) (wl * fb));
            }
            else
            {
                dlL.push ((float) (l * send + std::tanh (wl * fb)));
                dlR.push ((float) (r * send + std::tanh (wr * fb)));
            }
            l += wl; r += wr;
        }
        else if (send > 0.0 && isReverb)
        {
            pre.push ((float) (0.5 * (l + r) * send + rvPrev * rvFeedbackFromDelay));
            const double in = pre.read (std::max (2.0, preMs * sr / 1000.0));
            // FDN: Householder mixing, damping in the loop
            std::array<double, kLines> o {};
            double sum = 0.0;
            static constexpr int lens[kLines] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
            for (int k = 0; k < kLines; ++k)
            {
                o[(size_t) k] = rv[(size_t) k].read (lens[k] * sizeScale * 1.6);
                sum += o[(size_t) k];
            }
            const double hh = sum * (2.0 / kLines);
            double outL = 0, outR = 0;
            for (int k = 0; k < kLines; ++k)
            {
                const double len = lens[k] * sizeScale * 1.6;
                const double g = std::pow (10.0, -3.0 * len / (rvDecay * sr));
                double v = (o[(size_t) k] - hh) * g;
                rvLp[(size_t) k] += (v - rvLp[(size_t) k]) * (1.0 - rvDamp * 0.85);
                v = rvLp[(size_t) k];
                rv[(size_t) k].push ((float) (in * ((k & 1) ? 1.0 : -1.0) + v));
                (k & 1 ? outR : outL) += o[(size_t) k];
            }
            outL = dlColHL.hp (dlColL.lp (outL * 0.35));
            outR = dlColHR.hp (dlColR.lp (outR * 0.35));
            rvPrev = 0.5 * (outL + outR);
            l += outL; r += outR;
        }

        L[i] = (float) l;
        R[i] = (float) r;
    }
}
} // namespace vc
