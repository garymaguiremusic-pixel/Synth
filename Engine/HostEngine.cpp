#include "HostEngine.h"
#include <cmath>

namespace vc
{
namespace
{
    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 30; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    }
}

HostEngine::HostEngine()
{
    histL.assign (kRing, 0.0f);
    histR.assign (kRing, 0.0f);
}

void HostEngine::prepare (double hostRate, int maxBlock)
{
    host = hostRate > 0 ? hostRate : 44100.0;
    ratio = tables::kInternalRate / host;
    passthrough = std::abs (host - tables::kInternalRate) < 0.5;

    // Low-pass cutoff at the lower of the two Nyquists (a little below), Kaiser beta 8.
    const double cutoff = std::min (1.0, 1.0 / ratio) * 0.92;
    const double beta = 8.0;
    table.assign ((size_t) (kPhases + 1) * kTaps, 0.0f);
    for (int ph = 0; ph <= kPhases; ++ph)
    {
        const double frac = (double) ph / kPhases;
        double sum = 0.0;
        for (int k = 0; k < kTaps; ++k)
        {
            const double x = (k - (kTaps / 2 - 1)) - frac;          // tap position relative to the read point
            const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (dsp::kPi * cutoff * x) / (dsp::kPi * cutoff * x);
            const double w = x / (kTaps / 2.0);
            const double win = std::abs (w) >= 1.0 ? 0.0 : besselI0 (beta * std::sqrt (1.0 - w * w)) / besselI0 (beta);
            const double v = cutoff * sinc * win;
            table[(size_t) ph * kTaps + (size_t) k] = (float) v;
            sum += v;
        }
        for (int k = 0; k < kTaps; ++k) table[(size_t) ph * kTaps + (size_t) k] = (float) (table[(size_t) ph * kTaps + (size_t) k] / sum);
    }
    tmpL.assign ((size_t) std::max (64, (int) std::ceil (maxBlock * ratio) + kTaps + 8), 0.0f);
    tmpR = tmpL;
    reset();
}

void HostEngine::reset()
{
    synth.reset();
    std::fill (histL.begin(), histL.end(), 0.0f);
    std::fill (histR.begin(), histR.end(), 0.0f);
    writePos = 0;
    produced = 0;
    readPos = 0.0;
}

int HostEngine::latencySamples() const
{
    const double internal = synth.latencyInternalSamples() + (passthrough ? 0 : kTaps / 2);
    return (int) std::lround (internal / ratio);
}

void HostEngine::generate (int n)
{
    if ((int) tmpL.size() < n) { tmpL.resize ((size_t) n); tmpR.resize ((size_t) n); }
    synth.renderInternal (tmpL.data(), tmpR.data(), n);
    for (int i = 0; i < n; ++i)
    {
        histL[(size_t) writePos] = tmpL[(size_t) i];
        histR[(size_t) writePos] = tmpR[(size_t) i];
        writePos = (writePos + 1) & (kRing - 1);
    }
    produced += n;
}

void HostEngine::process (float* L, float* R, int n)
{
    if (passthrough)
    {
        synth.renderInternal (L, R, n);
        return;
    }
    int i = 0;
    while (i < n)
    {
        // we need internal samples up to floor(readPos) + kTaps/2 + 1
        const long long need = (long long) std::floor (readPos) + kTaps / 2 + 1;
        if (produced < need)
        {
            const int chunk = (int) std::min<long long> (need - produced + 64, (long long) kRing / 2);
            generate (chunk);
            continue;
        }
        const long long ip = (long long) std::floor (readPos);
        const double frac = readPos - (double) ip;
        const int ph = (int) (frac * kPhases);
        const float pf = (float) (frac * kPhases - ph);
        const float* t0 = &table[(size_t) ph * kTaps];
        const float* t1 = &table[(size_t) (ph + 1) * kTaps];
        float l = 0.0f, r = 0.0f;
        // ring position of sample index (ip - (kTaps/2 - 1))
        const long long first = ip - (kTaps / 2 - 1);
        for (int k = 0; k < kTaps; ++k)
        {
            const long long idx = first + k;
            const float c = t0[k] + (t1[k] - t0[k]) * pf;
            if (idx < 0) continue;
            const int rp = (int) ((writePos - (produced - idx)) & (kRing - 1));
            l += c * histL[(size_t) rp];
            r += c * histR[(size_t) rp];
        }
        L[i] = l;
        R[i] = r;
        readPos += ratio;
        ++i;
    }
}
} // namespace vc
