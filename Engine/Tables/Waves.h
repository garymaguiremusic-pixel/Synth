#pragma once
// Single-cycle waves for Osc Wave Select / LFO shapes / Osc3 modes.
//   0 = sine, 1 = triangle, 2..63 = "Wave 3".."Wave 64" (spectral waves).
// The spectral waves are synthesised additively from 64-harmonic magnitude tables.
// GUESS: the harmonic content below is a deterministic placeholder family; the
// calibration step measures each Osirus wave (single-cycle FFT) and writes
// kWaveHarmonics into Measured.h (VC_MEASURED_WAVES), which then replaces it.
// Tables are band-limited per octave (mip-maps) for alias-free playback.

#include <array>
#include <vector>

namespace vc
{
class WaveBank
{
public:
    static constexpr int kNumWaves = 64;
    static constexpr int kHarmonics = 64;
    static constexpr int kSize = 2048;
    static constexpr int kMips = 7;      // max harmonics 64, 32, 16, 8, 4, 2, 1

    static const WaveBank& get();

    /** Band-limited lookup. phase 0..1, freqNorm = f / sampleRate. */
    float sample (int wave, double phase, double freqNorm) const;
    /** Full-bandwidth (64-harmonic) value, used by LFOs. */
    float sampleRaw (int wave, double phase) const;

    /** Harmonic magnitudes actually used for a wave (for reports / tests). */
    const std::array<float, kHarmonics>& harmonics (int wave) const { return mags[(size_t) wave]; }

private:
    WaveBank();
    std::array<std::array<float, kHarmonics>, kNumWaves> mags {};
    std::array<std::array<float, kHarmonics>, kNumWaves> phases {};
    std::vector<float> tables;   // [wave][mip][kSize + 1]
    const float* table (int wave, int mip) const { return tables.data() + ((size_t) wave * kMips + (size_t) mip) * (kSize + 1); }
};
} // namespace vc
