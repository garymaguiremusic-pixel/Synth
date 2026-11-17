#include "Waves.h"
#include "Tables.h"

#include <cmath>

namespace vc
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    // Deterministic placeholder spectra (see header). Families:
    //  2..17  : formant-like single peaks moving up the harmonic series
    //  18..33 : two peaks (vowel-ish)
    //  34..49 : odd/even weighted series with slopes (organ/reed-like)
    //  50..63 : sparse "metallic" sets
    void placeholderSpectrum (int w, std::array<float, WaveBank::kHarmonics>& m)
    {
        for (int h = 1; h <= WaveBank::kHarmonics; ++h)
        {
            double a = 0.0;
            if (w < 18)
            {
                const double c = 1.0 + (w - 2) * 1.7;
                const double bw = 1.5 + (w - 2) * 0.25;
                a = std::exp (-0.5 * std::pow ((h - c) / bw, 2.0)) + 0.6 / h;
            }
            else if (w < 34)
            {
                const int k = w - 18;
                const double c1 = 2.0 + k * 0.6, c2 = 6.0 + k * 2.2;
                a = std::exp (-0.5 * std::pow ((h - c1) / 1.4, 2.0)) + 0.7 * std::exp (-0.5 * std::pow ((h - c2) / 2.5, 2.0)) + 0.3 / h;
            }
            else if (w < 50)
            {
                const int k = w - 34;
                const double slope = 0.6 + k * 0.09;
                const double oddBias = (k % 2 == 0) ? 1.0 : 0.25;
                a = std::pow ((double) h, -slope) * ((h % 2 == 1) ? 1.0 : oddBias);
            }
            else
            {
                const int k = w - 50;
                const int step = 2 + (k % 5);
                a = ((h - 1) % step == 0 ? 1.0 : 0.0) * std::pow ((double) h, -0.5 - 0.05 * k);
                if (h == 1) a = 1.0;
            }
            m[(size_t) (h - 1)] = (float) a;
        }
    }
}

const WaveBank& WaveBank::get()
{
    static const WaveBank bank;
    return bank;
}

WaveBank::WaveBank()
{
    for (int w = 0; w < kNumWaves; ++w)
    {
        auto& m = mags[(size_t) w];
        auto& p = phases[(size_t) w];
        m.fill (0.0f);
        p.fill (0.0f);
        if (w == 0)
        {
            m[0] = 1.0f;
        }
        else if (w == 1)
        {
            // triangle: odd harmonics 1/h^2 with alternating sign (sine series)
            for (int h = 1; h <= kHarmonics; h += 2)
            {
                m[(size_t) (h - 1)] = (float) (8.0 / (kPi * kPi) / (h * h));
                p[(size_t) (h - 1)] = ((h / 2) % 2 == 0) ? 0.0f : (float) kPi;
            }
        }
        else
        {
#ifdef VC_MEASURED_WAVES
            for (int h = 0; h < kHarmonics; ++h) m[(size_t) h] = tables::measured::kWaveHarmonics[w][h];
#else
            placeholderSpectrum (w, m);
#endif
            for (int h = 0; h < kHarmonics; ++h)       // fixed pseudo-random phases (deterministic)
                p[(size_t) h] = (float) std::fmod ((h * h * 0.6180339887 + w * 0.37) * 2.0 * kPi, 2.0 * kPi);
        }
    }

    tables.assign ((size_t) kNumWaves * kMips * (kSize + 1), 0.0f);
    std::vector<double> tmp (kSize);
    for (int w = 0; w < kNumWaves; ++w)
    {
        for (int mip = 0; mip < kMips; ++mip)
        {
            const int maxH = kHarmonics >> mip;
            std::fill (tmp.begin(), tmp.end(), 0.0);
            for (int h = 1; h <= maxH; ++h)
            {
                const double a = mags[(size_t) w][(size_t) (h - 1)];
                if (a == 0.0) continue;
                const double ph = phases[(size_t) w][(size_t) (h - 1)];
                for (int i = 0; i < kSize; ++i)
                    tmp[(size_t) i] += a * std::sin (2.0 * kPi * h * i / kSize + ph);
            }
            // normalise every mip by the full-band peak so the level doesn't jump between mips
            if (mip == 0)
            {
                double peak = 1e-9;
                for (double v : tmp) peak = std::max (peak, std::abs (v));
                const double norm = 1.0 / peak;
                for (auto& a : mags[(size_t) w]) a = (float) (a * norm);
                for (auto& v : tmp) v *= norm;
            }
            float* t = tables.data() + ((size_t) w * kMips + (size_t) mip) * (kSize + 1);
            for (int i = 0; i < kSize; ++i) t[i] = (float) tmp[(size_t) i];
            t[kSize] = t[0];
        }
    }
}

float WaveBank::sample (int wave, double phase, double freqNorm) const
{
    wave = std::clamp (wave, 0, kNumWaves - 1);
    // choose the mip whose highest harmonic stays below ~0.45 * fs
    const double maxH = 0.45 / std::max (1e-7, std::abs (freqNorm));
    int mip = 0;
    while (mip < kMips - 1 && (kHarmonics >> mip) > maxH) ++mip;
    const float* t = table (wave, mip);
    const double pos = (phase - std::floor (phase)) * kSize;
    const int i = (int) pos;
    const float f = (float) (pos - i);
    return t[i] + (t[i + 1] - t[i]) * f;
}

float WaveBank::sampleRaw (int wave, double phase) const
{
    const float* t = table (std::clamp (wave, 0, kNumWaves - 1), 0);
    const double pos = (phase - std::floor (phase)) * kSize;
    const int i = (int) pos;
    const float f = (float) (pos - i);
    return t[i] + (t[i + 1] - t[i]) * f;
}
} // namespace vc
