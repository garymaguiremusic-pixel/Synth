#include "Instrument.h"

namespace vc
{
void Instrument::prepare (double hostRate, int maxBlock)
{
    for (auto* e : engines) e->prepare (hostRate, maxBlock);
    saturation.prepare (hostRate);
    sampleRate = hostRate > 0 ? hostRate : 44100.0;
    monoSetHz = -1;
}

void Instrument::reset()
{
    for (auto* e : engines) e->reset();
    saturation.reset();
    monoL.reset(); monoR.reset();
}

void Instrument::setMode (SynthMode m)
{
    if (m == modeIndex) return;
    current().allNotesOff (true);
    modeIndex = m;
    current().reset();
}

void Instrument::setTempo (double b)
{
    bpm = b > 0 ? b : 120.0;
    for (auto* e : engines) e->setTempo (bpm);
}

double Instrument::envTimeSeconds (double ms, bool sync, int div, double b)
{
    if (sync)
        return SyncDivisions::beats (div) * 60.0 / std::max (1.0, b);
    return std::max (0.0, ms) / 1000.0;
}

GlobalVoiceSettings Instrument::makeVoiceSettings (const GlobalParams& p, double b)
{
    GlobalVoiceSettings s;
    auto conv = [b] (const GlobalParams::Env& e)
    {
        AdsrTimes t;
        t.attack = envTimeSeconds (e.attackMs, e.sync, e.attackDiv, b);
        t.decay = envTimeSeconds (e.decayMs, e.sync, e.decayDiv, b);
        t.sustain = std::clamp (e.sustain, 0.0, 1.0);
        t.release = envTimeSeconds (e.releaseMs, e.sync, e.releaseDiv, b);
        return t;
    };
    s.amp = conv (p.amp);
    s.filter = conv (p.filter);
    s.ampOverride = p.useGlobalEnvelopes;
    s.filterMode = p.filterMode;
    s.cutoff = p.cutoff;
    s.resonance = p.resonance;
    s.envAmount = p.envAmount;
    s.keytrack = p.keytrack;
    s.velocity = p.velocity;
    s.oscOn = p.oscOn;
    s.virusRetrig = p.virusRetrig;
    s.lockHz = p.lockHz;
    return s;
}

void Instrument::process (float* L, float* R, int n)
{
    voiceSettings = makeVoiceSettings (params, bpm);   // engines hold a pointer to it
    current().process (L, R, n);
    // Mono: below the crossover the output is summed to mono (as the Reese plugin's Mono Bass)
    if (params.monoHz >= 1.0)
    {
        if (std::abs (params.monoHz - monoSetHz) > 0.01) { monoL.set (params.monoHz, sampleRate); monoR.set (params.monoHz, sampleRate); monoSetHz = params.monoHz; }
        for (int i = 0; i < n; ++i)
        {
            const double low = 0.5 * (monoL.low (L[i]) + monoR.low (R[i]));
            L[i] = (float) (monoL.high (L[i]) + low);
            R[i] = (float) (monoR.high (R[i]) + low);
        }
    }
    else if (monoSetHz >= 0) { monoL.reset(); monoR.reset(); monoSetHz = -1; }
    saturation.process (L, R, n, params.saturationCurve, params.saturationDrive, params.saturationMix);
    if (params.volume != 1.0)
        for (int i = 0; i < n; ++i) { L[i] *= (float) params.volume; R[i] *= (float) params.volume; }
}
} // namespace vc
