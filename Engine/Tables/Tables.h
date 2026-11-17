#pragma once
// Every 0..127 -> physical value mapping used by the engine lives here.
//
// STATUS: every table below starts as a documented GUESS. The calibration tooling
// (Calib/) measures Osirus and writes Engine/Tables/Measured.h with fitted 128-entry
// tables; when a measured table exists it replaces the guess (see VC_MEASURED_* in
// Measured.h). `tableStatus()` reports which is which, and vc_render --tables prints it.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#if __has_include("Measured.h")
 #include "Measured.h"
#endif

namespace vc::tables
{
constexpr double kInternalRate = 46875.0;   // measured: Osirus runs the Virus C DSP at 46875 Hz

enum class Status { Guessed, Measured };
struct TableInfo { const char* name; Status status; const char* note; };

inline double expMap (double v01, double lo, double hi) { return lo * std::pow (hi / lo, v01); }
inline double signedUnit (int v) { return (v - 64) / 64.0; }        // 0..127 -> -1 .. +0.984

// -------------------------------------------------------------------------------------
// Filter cutoff: value (0..127, fractional after modulation) -> Hz.
// Guess: exponential, 0 -> ~20 Hz, 127 -> ~20 kHz (about 10 octaves over 127 steps).
inline double cutoffHz (double v)
{
#ifdef VC_MEASURED_CUTOFF
    return measured::interp (measured::kCutoffHz, v);
#else
    return 20.0 * std::pow (2.0, v * (9.97 / 127.0));
#endif
}
/** One cutoff step in semitones (used for keyfollow). */
constexpr double kCutoffSemisPerStep = 9.97 * 12.0 / 127.0;

// Resonance 0..127 -> SVF damping (2R, 2 = no resonance, ~0 = self oscillation)
inline double svfDamping (double v)
{
    const double x = std::clamp (v / 127.0, 0.0, 1.0);
#ifdef VC_MEASURED_RESO_SVF
    return measured::interp (measured::kResoSvf, v);
#else
    return 2.0 * std::pow (1.0 - x, 1.6) + 0.004;
#endif
}
// Resonance 0..127 -> ladder feedback k (self-oscillation from ~4.0)
inline double ladderK (double v)
{
    const double x = std::clamp (v / 127.0, 0.0, 1.0);
#ifdef VC_MEASURED_RESO_LADDER
    return measured::interp (measured::kResoLadder, v);
#else
    return 4.3 * std::pow (x, 1.3);
#endif
}

// -------------------------------------------------------------------------------------
// Envelope segment times (seconds) for Attack / Decay / Release 0..127.
// Guess: 0 = instant (1 sample), exponential up to ~20 s.
inline double envTimeSec (double v)
{
#ifdef VC_MEASURED_ENV_TIME
    return measured::interp (measured::kEnvTime, v);
#else
    if (v <= 0.0) return 0.0;
    return expMap (v / 127.0, 0.0012, 20.0);
#endif
}
inline double attackTimeSec (double v)
{
#ifdef VC_MEASURED_ATTACK
    return measured::interp (measured::kAttack, v);
#else
    return envTimeSec (v);
#endif
}
inline double decayTimeSec (double v)
{
#ifdef VC_MEASURED_DECAY
    return measured::interp (measured::kDecay, v);
#else
    return envTimeSec (v);
#endif
}
inline double releaseTimeSec (double v)
{
#ifdef VC_MEASURED_RELEASE
    return measured::interp (measured::kRelease, v);
#else
    return envTimeSec (v);
#endif
}
// Sustain time (slope) 0..127. MEASURED on Osirus (env/amp_sustain_time probes):
// 64 = hold; below 64 the level falls, above 64 it rises (to full level), linearly in dB.
// The rate grows ~5.2x per 16 steps away from 64: 1.7 dB/s at 48, 8.9 at 32, 47 at 16, near-instant at 0
// (rises: ~1.4 dB/s at 80, ~7 at 96). Note: Osirus's value list labels 0 "Infinite" and 64 "Fall";
// the audio shows 64 is the flat setting.
// Returns dB per second, signed (negative = falling), 0 = hold.
inline double sustainSlopeDbPerSec (int v)
{
    if (v == 64) return 0.0;
    const double d = std::abs (v - 64);
    const double rate = 1.6 * std::pow (5.2, (d - 16.0) / 16.0);
    return v < 64 ? -rate : rate;
}

// -------------------------------------------------------------------------------------
// LFO rate 0..127 -> Hz. Guess: 0.02 Hz .. ~60 Hz exponential.
inline double lfoRateHz (double v)
{
#ifdef VC_MEASURED_LFO_RATE
    return measured::interp (measured::kLfoRate, v);
#else
    return expMap (std::clamp (v, 0.0, 127.0) / 127.0, 0.02, 60.0);
#endif
}
// LFO3 fade-in 0..127 -> seconds
inline double lfoFadeSec (int v) { return v <= 0 ? 0.0 : expMap (v / 127.0, 0.02, 10.0); }

// -------------------------------------------------------------------------------------
// Pitch
inline double semitoneOffset (int v) { return (double) (v - 64); }
// Keyfollow: 64 = 0 %, 96 = 100 %, 127 ~ 197 %, 0 = -200 %
inline double oscKeyfollow (double v) { return (v - 64.0) / 32.0; }
// Osc2 detune 0..127 -> cents (guess: up to ~ +50 cents, fine at the bottom)
inline double osc2DetuneCents (double v)
{
#ifdef VC_MEASURED_DETUNE
    return measured::interp (measured::kDetuneCents, v);
#else
    return 50.0 * std::pow (std::clamp (v, 0.0, 127.0) / 127.0, 2.0);
#endif
}
// Osc3 detune: same law as osc2 (guess)
inline double osc3DetuneCents (double v) { return osc2DetuneCents (v); }
// Unison detune 0..127 -> max spread in cents between outer voices (guess)
inline double unisonDetuneCents (double v) { return 60.0 * std::pow (std::clamp (v, 0.0, 127.0) / 127.0, 2.0); }
// Bipolar modulation depth for pitch destinations (LFO -> osc): +-64 -> +-semitones
inline double lfoPitchSemis (double amountSigned /* -1..1 */)
{
    return (amountSigned < 0 ? -1.0 : 1.0) * amountSigned * amountSigned * 24.0;
}
// Filter-env -> osc2 pitch: full amount = +-48 semitones (guess)
inline double envPitchSemis (double amountSigned) { return amountSigned * std::abs (amountSigned) * 48.0; }
// Portamento 0..127 -> glide time constant (seconds per octave-ish; guess)
inline double portamentoSec (int v) { return v <= 0 ? 0.0 : expMap (v / 127.0, 0.003, 5.0); }
// Bender range 0..127 (64 = 0) -> semitones
inline double benderSemis (int v) { return (double) (v - 64); }

// -------------------------------------------------------------------------------------
// Levels
inline double volumeGain (double v)             // 0..127 -> 0..1 (squared law guess)
{
    const double x = std::clamp (v, 0.0, 127.0) / 127.0;
    return x * x;
}
// Osc Volume: MEASURED -9.6 dB at 32 against 64, and no level change above 64 (it drives the saturation).
inline double mainVolumeGain (double v)
{
    const double x = std::clamp (v, 0.0, 127.0);
    return x >= 64.0 ? 1.0 : std::pow (x / 64.0, 1.6);
}
// Drive into the saturation stage from Osc Volume above 64 (the output is compensated, see Synth.cpp)
inline double mainVolumeDrive (double v)
{
    const double x = std::clamp (v, 0.0, 127.0);
    return x <= 64.0 ? 1.0 : 1.0 + (x - 64.0) / 63.0 * 3.0;
}
// Patch Volume: MEASURED (dB relative to 100): 32 -22.7, 64 -9.1, 96 -1.0, 100 0, 127 +6.8; 0 = silent.
inline double patchVolumeGain (double v)
{
    const double x = std::clamp (v, 0.0, 127.0);
    if (x <= 0.0) return 0.0;
    static constexpr double xs[] = { 0.0, 32.0, 64.0, 96.0, 100.0, 127.0 };
    static constexpr double db[] = { -36.3, -22.7, -9.1, -1.0, 0.0, 6.8 };
    int i = 0;
    while (i < 4 && x > xs[i + 1]) ++i;
    const double t = (x - xs[i]) / (xs[i + 1] - xs[i]);
    return std::pow (10.0, (db[i] + (db[i + 1] - db[i]) * t) / 20.0);
}
// Amp Velocity (signed amount -1..1, velocity 0..1). MEASURED at velocity 40: 64 = no effect; +1 = -18.4 dB,
// +0.5 = -8.5 dB, -1 = -7.9 dB, -0.5 = -4 dB, i.e. gain = vel^(1.8 a) for a > 0 and vel^(0.78 |a|) for a < 0.
// (Only one velocity was probed: the negative side may scale with velocity differently.)
inline double ampVelocityGain (double amount, double vel)
{
    vel = std::clamp (vel, 0.01, 1.0);
    return amount >= 0.0 ? std::pow (vel, 1.8 * amount) : std::pow (vel, 0.78 * -amount);
}
// Overall output level: MEASURED init-patch saw at -25.7 dBFS RMS on Osirus (VC-Clone was 11.8 dB louder).
constexpr double kOutputTrimDb = -11.8;

// FM index for Osc2 FM Amount 0..127 (linear FM, guess)
inline double fmIndex (double v) { const double x = std::clamp (v, 0.0, 127.0) / 127.0; return 6.0 * x * x; }

// Pulse width 0..127 -> duty (0 = 50 %, 127 ~ 1 %)
inline double pulseDuty (double v) { return 0.5 - 0.49 * std::clamp (v, 0.0, 127.0) / 127.0; }

// Clock tempo 0..127 -> BPM (value list: 63 .. 190)
inline double clockBpm (int v) { return 63.0 + std::clamp (v, 0, 127); }

// Music division index (value list "musicDivision") -> length in quarter notes
inline double divisionBeats (int idx)
{
    static constexpr double kBeats[22] = {
        0.0,        // Off
        1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1.0, 2.0,               // 1/64 .. 1/2
        3.0 / 16, 3.0 / 8, 3.0 / 4, 3.0 / 2,                          // 3/64 .. 3/8
        1.0 / 6, 1.0 / 3, 2.0 / 3, 4.0 / 3, 8.0 / 3, 3.0,             // 1/24 .. 3/4
        4.0, 8.0, 16.0, 32.0, 64.0 };                                 // 1/1 .. 16/1
    return idx >= 0 && idx < 22 ? kBeats[idx] : 0.0;
}

// FX ----------------------------------------------------------------------------------
inline double delayTimeMs (double v)  { return expMap (std::clamp (v, 0.0, 127.0) / 127.0, 1.0, 693.0); }   // guess
inline double chorusDelayMs (double v){ return 0.5 + 25.0 * std::clamp (v, 0.0, 127.0) / 127.0; }
inline double eqLowHz (int v)         { return expMap (v / 127.0, 32.0, 458.0); }
inline double eqMidHz (int v)         { return expMap (v / 127.0, 19.0, 24000.0); }
inline double eqHighHz (int v)        { return expMap (v / 127.0, 1800.0, 24000.0); }
inline double eqGainDb (int v)        { return (v - 64) * (16.0 / 64.0); }
inline double eqQ (int v)             { return expMap (v / 127.0, 0.28, 15.0); }
inline double bassTuneHz (int v)      { return expMap (v / 127.0, 20.0, 400.0); }
inline double reverbDecaySec (int v)  { return expMap (v / 127.0, 0.2, 20.0); }
inline double phaserFreqHz (int v)    { return expMap (v / 127.0, 40.0, 10000.0); }

inline std::array<TableInfo, 16> tableStatus()
{
    auto st = [] (bool measured) { return measured ? Status::Measured : Status::Guessed; };
    return {{
        { "cutoffHz",        st (
#ifdef VC_MEASURED_CUTOFF
            true
#else
            false
#endif
            ), "Filter cutoff 0..127 -> Hz" },
        { "svfResonance",    st (
#ifdef VC_MEASURED_RESO_SVF
            true
#else
            false
#endif
            ), "SVF resonance" },
        { "ladderResonance", st (
#ifdef VC_MEASURED_RESO_LADDER
            true
#else
            false
#endif
            ), "Analog filter resonance / self-oscillation" },
        { "envTime",         st (
#if defined (VC_MEASURED_ENV_TIME) || (defined (VC_MEASURED_ATTACK) && defined (VC_MEASURED_DECAY) && defined (VC_MEASURED_RELEASE))
            true
#else
            false
#endif
            ), "Attack/Decay/Release times" },
        { "sustainTime",     Status::Measured, "Sustain slope (dB/s law from the Osirus probes)" },
        { "lfoRate",         st (
#ifdef VC_MEASURED_LFO_RATE
            true
#else
            false
#endif
            ), "LFO rate 0..127 -> Hz" },
        { "osc2Detune",      st (
#ifdef VC_MEASURED_DETUNE
            true
#else
            false
#endif
            ), "Osc2 detune cents" },
        { "unisonDetune",    Status::Guessed, "Unison detune spread" },
        { "portamento",      Status::Guessed, "Portamento time" },
        { "levels",          Status::Measured, "Patch/osc volume, amp velocity, output trim (Osirus probes)" },
        { "fmIndex",         Status::Guessed, "Osc2 FM amount" },
        { "pulseWidth",      Status::Guessed, "Pulse width law" },
        { "waves",           st (
#ifdef VC_MEASURED_WAVES
            true
#else
            false
#endif
            ), "Spectral waves 3..64 harmonic content" },
        { "saturation",      Status::Guessed, "Saturation / distortion transfer curves" },
        { "fxTimes",         Status::Guessed, "Chorus/phaser/delay/reverb timing" },
        { "eq",              Status::Guessed, "EQ / bass boost frequencies and gains" },
    }};
}
} // namespace vc::tables
