#pragma once

#include "SpireReverb.h"
#include "MassiveReverb.h"
#include "Filters.h"
#include "Ducker.h"
#include "PingPongDelay.h"
#include "ReverbMath.h"

namespace lv
{

//==============================================================================
/**
    Wet path of LeadVerb:

      in -> predelay (+ Lift: an octave-up copy, into the reverb only)
         -> active reverb: one of Spire's seven modes (Mode 1, with its own
            Color and Wide) or one of Massive's two reverbs (Mode 2)
         -> Modulation (vibrato) -> Hi Shelf
         -> Sidechain ducking (keyed from the dry input, or an external key)
         -> PingPong delay (tempo synced, with its own mix) -> out

    Predelay, Lift, Modulation and Hi Shelf are Mode 1 controls; the processor sets
    them neutral in Mode 2, which keeps Massive's own pre-delay. Sidechain and the
    PingPong delay work in both.

    Every mode has its own engine instance. On a mode change the old engine stops
    receiving input and fades out over 150 ms while the new one starts, so a
    switch never clicks. The output is wet only; the processor does the
    dry/wet crossfade.
*/
class ReverbEngine
{
public:
    static constexpr int kMaxBlock = 512;

    struct Settings
    {
        int mode = ModePlate1;       // ReverbMode: a Spire mode or a Massive reverb
        // Mode 1, Spire's own 0..1 knobs, and Wide 0..2 (0..1 = Spire's range)
        float spireDecay = 0.6357f, spireDamp = 0.15f, spireColor = 0.5f;
        float width = 1.0f;
        // Mode 2, Massive's knobs 0..1
        float massiveSize = 0.5f, massiveDensity = 0.5f, massiveColor = 0.5f;
        float predelayMs = 0.0f;
        float shelfDb = 0.0f;        // Hi Shelf boost, 0 .. +12 dB
        float sidechain = 0.0f;      // ducking amount, 0 .. 1
        float lift = 0.0f;           // level of the octave-up copy fed into the reverb, 0 .. 1
        float vibratoCents = 0.0f;   // Modulation: vibrato depth on the reverb
        float vibratoHz = 5.0f;
        bool delayOn = false;        // PingPong delay after everything else
        float delaySeconds = 0.25f;  // synced time
        float delayFeedback = 0.4f;  // 0 .. 0.92
        float delayMix = 0.3f;       // 0 .. 1
    };

    void prepare (double sampleRate);
    void reset();

    void setParameters (const Settings&) noexcept;

    /** numSamples must be <= kMaxBlock. Outputs the wet signal only.
        keyL/keyR drive the sidechain ducking; nullptr uses the input itself. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                  const float* keyL = nullptr, const float* keyR = nullptr) noexcept;

    /** Largest sidechain gain reduction (dB, positive) since the last call. */
    float getAndResetDuckingDb() noexcept { return ducker.getAndResetReductionDb(); }
    int getActiveMode() const noexcept { return activeMode; }
    float getDelaySamples() const noexcept { return pingPong.getDelaySamples(); }
    /** Mode 2's current RT60 target and fixed pre-delay, for the display. */
    float getMassiveRt60 (int mode) const noexcept { return massive[mode == ModeMassiveSmall ? 1 : 0].getTargetRt60(); }
    float getMassivePredelayMs (int mode) const noexcept { return massive[mode == ModeMassiveSmall ? 1 : 0].getPredelayMs(); }

private:
    void runMode (int mode, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    void resetMode (int mode) noexcept;
    void setModeParameters (int mode) noexcept;
    float fs = 44100.0f;
    Settings settings;

    SpireReverb spire[kNumSpireModes];
    MassiveReverb massive[2];   // Reverb, Small Reverb

    // Predelay read position. A change crossfades from the old position to the new
    // one over 40 ms, so even a jump from 1/64 to 16/1 neither clicks nor bends pitch.
    DelayLine predelay[2];
    float predelayCurrent = 1.0f, predelayNext = 1.0f, predelayTarget = 1.0f;
    float predelayFade = 0.0f, predelayFadeStep = 0.001f;
    bool predelayFading = false;

    HighShelf shelf[2];
    Vibrato vibrato;

    // Lift: octave-up copy of the (predelayed) input, low cut, added to the reverb input
    PitchShifter liftShifter[2];
    OnePoleHP liftLowCut[2];
    float liftGain = 0.0f, liftSmoothing = 0.001f;
    Ducker ducker;
    PingPongDelay pingPong;

    int activeMode = ModePlate1, fadingMode = -1;
    float fadeGain = 0.0f, fadeStep = 0.001f;
    bool snapOnNextBlock = true;

    float preL[kMaxBlock] {}, preR[kMaxBlock] {};
    float tmpL[kMaxBlock] {}, tmpR[kMaxBlock] {};
    float silence[kMaxBlock] {};
};

} // namespace lv
