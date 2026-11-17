#include "Global.h"
#include "Tables/Tables.h"

namespace vc
{
//==============================================================================
const char* SyncDivisions::name (int i)
{
    static const char* names[kCount] = { "1/64", "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T",
                                         "1/4", "1/4.", "1/2T", "1/2", "1/2.", "1/1", "1/1.", "2/1", "4/1", "8/1" };
    return i >= 0 && i < kCount ? names[i] : "?";
}

double SyncDivisions::beats (int i)
{
    static const double b[kCount] = { 1.0 / 16, 1.0 / 8, 1.0 / 6, 1.0 / 4, 3.0 / 8, 1.0 / 3, 1.0 / 2, 3.0 / 4, 2.0 / 3,
                                      1.0, 1.5, 4.0 / 3, 2.0, 3.0, 4.0, 6.0, 8.0, 16.0, 32.0 };
    return i >= 0 && i < kCount ? b[i] : 1.0;
}

//==============================================================================
void Adsr::setTimes (const AdsrTimes& t, double sr)
{
    attackInc = t.attack <= 0.0 ? 1.0 : 1.0 / (t.attack * sr);
    decayCoef = t.decay <= 0.0 ? 0.0 : std::exp (-6.9 / (t.decay * sr));
    releaseCoef = t.release <= 0.0 ? 0.0 : std::exp (-6.9 / (t.release * sr));
    sustain = std::clamp (t.sustain, 0.0, 1.0);
}

double Adsr::tick()
{
    switch (stage)
    {
        case Stage::Idle: return 0.0;
        case Stage::Attack:
            level += attackInc;
            if (level >= 1.0) { level = 1.0; stage = Stage::Decay; }
            break;
        case Stage::Decay:
            level = sustain + (level - sustain) * decayCoef;
            if (std::abs (level - sustain) < 1.0e-4) { level = sustain; stage = Stage::Sustain; }
            break;
        case Stage::Sustain:
            level = sustain;
            break;
        case Stage::Release:
            level *= releaseCoef;
            if (level < 1.0e-5) { level = 0.0; stage = Stage::Idle; }
            break;
    }
    return level;
}

//==============================================================================
void GlobalVoiceStage::start (int n, float v, const GlobalVoiceSettings& s, double sr, bool fromZero)
{
    note = n;
    vel = v;
    ampEnv.setTimes (s.amp, sr);
    filtEnv.setTimes (s.filter, sr);
    ampEnv.gateOn (fromZero);
    filtEnv.gateOn (fromZero);
    if (fromZero)
    {
        svfL.reset(); svfR.reset(); ladL.reset(); ladR.reset();
    }
    lastSr = 0;
    control (s, sr, 1);
}

void GlobalVoiceStage::control (const GlobalVoiceSettings& s, double sr, int)
{
    ampEnv.setTimes (s.amp, sr);
    filtEnv.setTimes (s.filter, sr);
    mode = std::clamp (s.filterMode, 0, 8);
    if (mode == 0)
        return;
    const double envAmt = s.envAmount * (1.0 + s.velocity * (vel - 1.0));
    double c = s.cutoff + envAmt * filtEnv.value() + (note - 60) * s.keytrack / tables::kCutoffSemisPerStep;
    c = std::clamp (c, -20.0, 150.0);
    const double hz = std::min (tables::cutoffHz (c), sr * 0.45);
    if (mode <= 4)
    {
        const double damp = tables::svfDamping (s.resonance);
        svfL.set (hz, damp, sr);
        svfR.set (hz, damp, sr);
    }
    else
    {
        const double k = tables::ladderK (s.resonance);
        ladL.set (hz, k, sr);
        ladR.set (hz, k, sr);
    }
}

void GlobalVoiceStage::filter (double& l, double& r)
{
    if (mode == 0)
        return;
    if (mode <= 4)
    {
        auto pick = [this] (dsp::Svf::Out o)
        {
            switch (mode) { case 1: return o.lp; case 2: return o.hp; case 3: return o.bp; default: return o.lp + o.hp; }
        };
        l = pick (svfL.tick (l));
        r = pick (svfR.tick (r));
    }
    else
    {
        l = ladL.tick (l, mode - 4);
        r = ladR.tick (r, mode - 4);
    }
}

//==============================================================================
void GlobalSaturation::prepare (double s)
{
    sr = s;
    reset();
}

void GlobalSaturation::reset()
{
    shL.reset(); shR.reset(); decL.reset(); decR.reset();
    prevL = prevR = 0.0;
    dryL.fill (0.0f); dryR.fill (0.0f); dryPos = 0;
    dcL.reset(); dcR.reset();
    dcL.setCutoff (8.0, sr * 2.0);
    dcR.setCutoff (8.0, sr * 2.0);
}

void GlobalSaturation::process (float* l, float* r, int n, int curve, double drive, double mix)
{
    if (curve <= 0 || mix <= 0.0)
        return;
    const double gain = 1.0 + drive * 8.0;
    const double makeup = 1.0 / std::sqrt (gain);
    const double osr = sr * 2.0;
    for (int i = 0; i < n; ++i)
    {
        // 2x: linear-interpolated upsample, shape, halfband decimate
        const double xl = l[i], xr = r[i];
        for (int k = 0; k < 2; ++k)
        {
            const double t = k == 0 ? 0.5 : 1.0;
            const double il = prevL + (xl - prevL) * t, ir = prevR + (xr - prevR) * t;
            double yl = shL.process (curve, il * gain, drive, 220.0, osr) * makeup;
            double yr = shR.process (curve, ir * gain, drive, 220.0, osr) * makeup;
            yl = dcL.hp (yl);
            yr = dcR.hp (yr);
            decL.push (yl);
            decR.push (yr);
        }
        prevL = xl;
        prevR = xr;
        const double wl = decL.read(), wr = decR.read();
        constexpr int lat = dsp::HalfbandDecimator::latencyOut();
        dryL[(size_t) dryPos] = (float) xl;
        dryR[(size_t) dryPos] = (float) xr;
        const int rd = (dryPos - lat + 16) & 15;
        dryPos = (dryPos + 1) & 15;
        l[i] = (float) (dryL[(size_t) rd] * (1.0 - mix) + wl * mix);
        r[i] = (float) (dryR[(size_t) rd] * (1.0 - mix) + wr * mix);
    }
}
} // namespace vc
