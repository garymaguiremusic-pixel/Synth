#pragma once
// Saturation / distortion shapers and the part FX chain (internal rate, stereo).
// Order (to be verified by calibration): Distortion -> Analog Boost -> EQ ->
// Chorus -> Phaser -> Delay/Reverb (send).

#include "Dsp.h"
#include <array>

namespace vc
{
struct ParamState;

/** Shared curve set: index = saturation curve list (Off, Light, Soft, Middle, Hard, Digital,
    Shaper, Rectifier, BitReducer, RateReducer, Rate+Flw, LowPass, Low+Flw, HighPass, High+Flw). */
struct Shaper
{
    enum Curve { Off, Light, Soft, Middle, Hard, Digital, ShaperC, Rectifier, BitReducer, RateReducer,
                 RateFollow, LowPass, LowFollow, HighPass, HighFollow, NumCurves };

    /** amount 0..1 (curve intensity), noteHz used by the "follow" curves. */
    double process (int curve, double x, double amount, double noteHz, double sr);
    void reset() { hold = 0; counter = 0; lp.reset(); }

    static double staticCurve (int curve, double x, double amount);

private:
    double hold = 0, counter = 0;
    dsp::OnePole lp;
};

/** Maps the 12-entry distortion list to the shared curve index. */
inline int distToCurve (int d)
{
    static constexpr int map[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 13 };
    return d >= 0 && d < 12 ? map[d] : 0;
}

class FxChain
{
public:
    FxChain();
    void reset();
    void process (float* L, float* R, int n, const ParamState&, double bpm, const std::array<float, 256>& mod);

private:
    double sr = 46875.0;          // = tables::kInternalRate
    Shaper distL, distR;
    dsp::Biquad boostL, boostR, lowL, lowR, midL, midR, highL, highR;
    int eqCounter = 0;

    // chorus
    dsp::DelayLine chL, chR;
    double chPhase = 0.0;
    double chFbL = 0, chFbR = 0;

    // phaser
    struct Allpass { double z = 0; double tick (double x, double a) { const double y = -a * x + z; z = x + a * y; return y; } };
    std::array<Allpass, 12> apL, apR;
    double phPhase = 0.0, phFbL = 0, phFbR = 0;

    // delay
    dsp::DelayLine dlL, dlR;
    double dlPhase = 0.0, dlSh = 0.0;
    dsp::OnePole dlColL, dlColR, dlColHL, dlColHR;
    dsp::Smoother dlTimeL, dlTimeR;
    bool dlTimeValid = false;      // smoothers start at the target after a reset

    // reverb (8-line FDN)
    static constexpr int kLines = 8;
    std::array<dsp::DelayLine, kLines> rv;
    std::array<double, kLines> rvLp {};
    dsp::DelayLine pre;
    double rvPrev = 0.0;
    dsp::Rng rng;
};
} // namespace vc
