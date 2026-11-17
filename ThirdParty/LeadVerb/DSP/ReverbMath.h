#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include "SpireModes.h"
#include "SpireRtGrid.h"
#include "SpireRtTable.h"

//==============================================================================
// Knob -> real unit mappings (Predelay and Decay 0..1000 like Spire; every
// other knob 0..100). Knobs move in steps of 1, or 0.1 with shift. shared by the DSP, the
// processor and the editor. Kept free of JUCE so the DSP can be tested alone.
//==============================================================================
namespace lv
{
/** Reverb Mode 1 (Spire) or Reverb Mode 2 (Massive). */
enum ReverbSection : int { SectionSpire = 0, SectionMassive = 1 };

/** The reverb the engine runs: one of Spire's seven modes (Mode 1), or one of
    Massive's two reverbs (Mode 2). */
enum ReverbMode : int
{
    // Mode 1: Spire's seven modes, replicated from its own algorithm and settings (SpireReverb.h)
    ModePlate1 = 0,
    ModePlate2,
    ModeHall,
    ModeSparkles,
    ModeAmbience,
    ModeDmension,
    ModeAura,
    // Mode 2: Massive's master-FX reverbs (MassiveReverb.h)
    ModeMassiveReverb,
    ModeMassiveSmall,
    NumEngineModes
};

constexpr int kNumSpireModes = 7;
constexpr int NumReverbModes = kNumSpireModes;   // choices of the Mode 1 "Mode" parameter
inline bool isSpireMode (int mode) { return mode >= 0 && mode < kNumSpireModes; }
inline bool isMassiveMode (int mode) { return mode == ModeMassiveReverb || mode == ModeMassiveSmall; }

constexpr float kMaxFreePredelayMs = 500.0f;
constexpr float kMaxPredelayMs     = 32000.0f;  // longest synced predelay (16/1 at 120 BPM)
constexpr float kDefaultBpm        = 120.0f;

inline float norm1000 (float v)        { return std::clamp (v, 0.0f, 1000.0f) / 1000.0f; }
inline float norm100 (float v)         { return std::clamp (v, 0.0f, 100.0f) / 100.0f; }

//==============================================================================
// Predelay: free 0..500 ms (squared curve for fine control of short times), or
// with SYNC on one of 21 note divisions, 1/64 .. 16/1 (1/1 = one 4/4 bar).
inline float predelayKnobToMs (float v) { const float n = norm1000 (v); return kMaxFreePredelayMs * n * n; }

constexpr int kNumPredelayDivisions = 21;

inline double predelayDivisionInBeats (int index)
{
    static constexpr double beats[] = { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 6.0, 3.0 / 16.0, 1.0 / 4.0, 1.0 / 3.0,
                                        3.0 / 8.0, 1.0 / 2.0, 2.0 / 3.0, 3.0 / 4.0, 1.0, 4.0 / 3.0, 1.5,
                                        2.0, 8.0 / 3.0, 3.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
    return beats[std::clamp (index, 0, (int) std::size (beats) - 1)];
}

inline int predelayKnobToDivision (float v)
{
    return (int) std::lround (norm1000 (v) * (float) (kNumPredelayDivisions - 1));
}

inline float syncedPredelayMs (float knob, double bpm)
{
    const double ms = 60000.0 / std::max (1.0, bpm) * predelayDivisionInBeats (predelayKnobToDivision (knob));
    return (float) std::min ((double) kMaxPredelayMs, ms);
}

//==============================================================================
// PingPong delay: synced note values 1/32 .. 1/1, each normal, dotted (x 1.5) or
// triplet (x 2/3). 1/4 = one beat.
constexpr int kNumDelayNotes = 6;
enum DelayNoteType : int { DelayNormal = 0, DelayDotted, DelayTriplet, NumDelayNoteTypes };

inline double delayNoteInBeats (int note, int type)
{
    static constexpr double beats[kNumDelayNotes] = { 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0, 2.0, 4.0 };   // 1/32 .. 1/1
    const double base = beats[std::clamp (note, 0, kNumDelayNotes - 1)];
    switch (type)
    {
        case DelayDotted:  return base * 1.5;
        case DelayTriplet: return base * 2.0 / 3.0;
        default:           return base;
    }
}

inline float syncedDelaySeconds (int note, int type, double bpm)
{
    return (float) std::min (9.9, 60.0 / std::max (1.0, bpm) * delayNoteInBeats (note, type));
}

// Delay Feedback (0..100): 0 = one repeat .. 100 = very long trail (0.92 per pass).
inline float delayFeedbackKnobToGain (float v) { return 0.92f * norm100 (v); }
inline float delayMixKnobToMix (float v)       { return norm100 (v); }

//==============================================================================
// Decay (Mode 1)
/** RT60 of a Spire mode, from the measured table (SpireRtTable.h), interpolated
    on a log scale. decay and damp are Spire's 0..1 knobs. 999 or more = endless. */
inline float spireRt60 (int mode, float decay, float damp)
{
    using namespace spire;
    if (tankFeedback (mode, decay) >= 0.9999f)
        return 999.0f;   // tank gain 1: the low end never dies away
    const float x = std::clamp (decay, 0.0f, 1.0f) * (float) (kRtDecaySteps - 1);
    const int i = std::min ((int) x, kRtDecaySteps - 2);
    const float fx = x - (float) i;
    const float dp = std::clamp (damp, 0.0f, 1.0f);
    int j = 0;
    while (j < kRtDampSteps - 2 && dp > kRtDampGrid[j + 1])
        ++j;
    const float fy = (dp - kRtDampGrid[j]) / (kRtDampGrid[j + 1] - kRtDampGrid[j]);
    const auto& t = kRt60Table[std::clamp (mode, 0, kNumSpireModes - 1)];
    auto lg = [] (float v) { return std::log (std::max (v, 1.0e-3f)); };
    const float a = lg (t[j][i]) + (lg (t[j][i + 1]) - lg (t[j][i])) * fx;
    const float b = lg (t[j + 1][i]) + (lg (t[j + 1][i + 1]) - lg (t[j + 1][i])) * fx;
    return std::exp (a + (b - a) * fy);
}

constexpr float kEndlessSeconds = 900.0f;   // a Spire mode at or above this reads as endless

/** Decay knob (0..1000) -> RT60 in seconds of a Spire mode, which also depends on
    Damp (0..100). */
inline float decayKnobToSeconds (float v, int mode, float dampKnob = 15.0f)
{
    return spireRt60 (mode, norm1000 (v), norm100 (dampKnob));
}

/** Inverse of decayKnobToSeconds: the knob value (0..1000) for an RT60 in seconds,
    clamped to what the mode can reach. */
inline float secondsToDecayKnob (float seconds, int mode, float dampKnob = 15.0f)
{
    // the measured time rises with the knob: bisect
    float lo = 0.0f, hi = 1000.0f;
    for (int k = 0; k < 40; ++k)
    {
        const float mid = 0.5f * (lo + hi);
        (decayKnobToSeconds (mid, mode, dampKnob) < seconds ? lo : hi) = mid;
    }
    return std::round (0.5f * (lo + hi) * 10.0f) / 10.0f;   // 0.1 steps, like the knob
}

// Modulation (0..100): vibrato on the reverb. Depth grows with the square of the
// knob (fine control at the bottom) up to about +-45 cents; the rate rises gently
// from 4.5 Hz to 6 Hz, like a singer's vibrato getting more intense.
constexpr float kMaxVibratoCents = 45.0f;
inline float modulationKnobToCents (float v) { const float n = norm100 (v); return kMaxVibratoCents * n * n; }
inline float modulationKnobToRateHz (float v) { return 4.5f + 1.5f * norm100 (v); }

// Wide (0..100): side gain of the wet signal, 0 = mono, 50 = natural, 100 = double width.
inline float wideKnobToWidth (float v)  { return 2.0f * norm100 (v); }

// Dry/Wet (0..100), Spire's law: the knob goes through Spire's mix curve to m; the
// dry stays at full level up to m = 0.7 while the wet rises (1.43 m), then the wet
// is at full level and the dry fades out (3.333 (1 - m)).
inline float dryWetToMix (float v)      { return spire::mixCurve (norm100 (v)); }
inline float mixDryGain (float m)       { return m < 0.7f ? 1.0f : (1.0f - m) * 3.3333f; }
inline float mixWetGain (float m)       { return m < 0.7f ? 1.43f * m : 1.0f; }

// Hi Shelf (0..100): boost of the 4 kHz high shelf on the reverb, 0 .. +12 dB.
constexpr float kMaxShelfDb = 12.0f;
inline float shelfKnobToDb (float v)    { return kMaxShelfDb * norm100 (v); }

// Lift (0..100): level of the octave-up copy fed into the reverb only, 0 .. 1.
inline float liftKnobToGain (float v)   { return norm100 (v); }

// Dry Source (0..100): untouched dry signal brought back on top of the Dry/Wet
// crossfade. 0 = the crossfade alone; 100 = full dry whatever Dry/Wet says.
inline float drySourceKnobToAmount (float v) { return norm100 (v); }

// Sidechain (0..100): how hard the dry input ducks the reverb.
inline float sidechainKnobToAmount (float v) { return norm100 (v); }

//==============================================================================
// Mode 2 (Massive): Dry/Wet, Size, Density and Color, all 0..100.
// Dry/Wet is an equal-power crossfade (Massive's own law is not known yet).
inline float massiveDryGain (float v) { return std::cos (norm100 (v) * 1.5707963f); }
inline float massiveWetGain (float v) { return std::sin (norm100 (v) * 1.5707963f); }

} // namespace lv
