#pragma once
// Runs the Synth at its fixed internal 44.1 kHz rate and converts to the host rate
// with a windowed-sinc (Kaiser, 32 taps, 512-phase table) fractional resampler.

#include "Synth.h"
#include <vector>

namespace vc
{
class HostEngine
{
public:
    HostEngine();
    void prepare (double hostRate, int maxBlock);
    void reset();
    /** Renders n host-rate samples. */
    void process (float* L, float* R, int n);
    /** Latency in host samples (decimator + resampler). */
    int latencySamples() const;
    double hostRate() const { return host; }

    Synth synth;

private:
    static constexpr int kTaps = 32, kPhases = 512;
    double host = 44100.0, ratio = 1.0;      // internal samples per host sample
    bool passthrough = true;
    std::vector<float> table;                // [kPhases + 1][kTaps]
    std::vector<float> histL, histR;         // internal-rate history ring (size kRing)
    static constexpr int kRing = 4096;
    int writePos = 0;
    long long produced = 0;                  // internal samples generated
    double readPos = 0.0;                    // absolute internal position of the next output
    std::vector<float> tmpL, tmpR;
    void generate (int n);
};
} // namespace vc
