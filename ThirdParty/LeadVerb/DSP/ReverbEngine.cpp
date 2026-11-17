#include "ReverbEngine.h"

namespace lv
{

void ReverbEngine::prepare (double sampleRate)
{
    fs = (float) sampleRate;

    for (int m = 0; m < kNumSpireModes; ++m)
        spire[m].prepare (sampleRate, m);
    massive[0].prepare (sampleRate, MassiveReverb::Reverb);
    massive[1].prepare (sampleRate, MassiveReverb::Small);

    const int maxPredelay = (int) std::ceil ((kMaxPredelayMs + 10.0f) * 0.001f * fs);
    for (auto& d : predelay)
        d.init (maxPredelay);

    for (auto& filter : shelf)
        filter.prepare (sampleRate);
    ducker.prepare (sampleRate);
    vibrato.prepare (fs);
    pingPong.prepare (fs);

    // unequal windows so the two sides' grain cycles don't line up
    liftShifter[0].prepare (fs, 47.0f);
    liftShifter[1].prepare (fs, 61.0f, 0.41f);
    for (int c = 0; c < 2; ++c)
    {
        liftShifter[c].setSemitones (12.0f);
        liftLowCut[c].setCutoff (150.0f, fs);
    }
    liftSmoothing = 1.0f - std::exp (-1.0f / (0.03f * fs));

    predelayFadeStep  = 1.0f / (0.04f * fs);                       // 40 ms crossfade
    fadeStep          = 1.0f / (0.15f * fs);

    reset();
}

void ReverbEngine::reset()
{
    for (auto& mode : spire)
        mode.reset();
    for (auto& m : massive)
        m.reset();

    for (auto& d : predelay)
        d.clear();
    for (auto& sh : shelf)
        sh.reset();
    ducker.reset();
    vibrato.clear();
    pingPong.reset();
    for (int c = 0; c < 2; ++c)
    {
        liftShifter[c].clear();
        liftLowCut[c].clear();
    }

    fadingMode = -1;
    fadeGain = 0.0f;
    snapOnNextBlock = true;
}

void ReverbEngine::setModeParameters (int mode) noexcept
{
    if (isSpireMode (mode))
    {
        SpireReverb::Knobs k;
        k.decay = settings.spireDecay;
        k.damp = settings.spireDamp;
        k.color = settings.spireColor;
        k.wide = settings.width;
        spire[mode].setParameters (k);
    }
    else if (isMassiveMode (mode))
    {
        MassiveReverb::Knobs k;
        k.size = settings.massiveSize;
        k.density = settings.massiveDensity;
        k.color = settings.massiveColor;
        massive[mode == ModeMassiveSmall ? 1 : 0].setKnobs (k);
    }
}

void ReverbEngine::setParameters (const Settings& s) noexcept
{
    settings = s;
    settings.mode = std::clamp (s.mode, 0, NumEngineModes - 1);

    if (settings.mode != activeMode)
    {
        // Let the previous mode ring out briefly; a third switch during the fade
        // simply cuts the oldest one.
        if (fadingMode >= 0 && fadingMode != settings.mode)
        {
            resetMode (fadingMode);
        }

        fadingMode = snapOnNextBlock ? -1 : activeMode;
        fadeGain = 1.0f;
        activeMode = settings.mode;
    }

    setModeParameters (activeMode);

    // The line is written before it is read, so a read offset of 1 is zero delay.
    predelayTarget = std::clamp (settings.predelayMs * 0.001f * fs + 1.0f, 1.0f, (float) predelay[0].capacity() - 2.0f);

    for (auto& sh : shelf)
        sh.setGainDb (settings.shelfDb);
    ducker.setAmount (settings.sidechain);
    vibrato.set (settings.vibratoCents, settings.vibratoHz);
    pingPong.setParameters (settings.delayOn, settings.delaySeconds, settings.delayFeedback, settings.delayMix);

    if (snapOnNextBlock)
    {
        snapOnNextBlock = false;
        // start every smoothed setting at its value, so the first notes after a
        // reset already get the full vibrato and Lift
        predelayCurrent = predelayNext = predelayTarget;
        predelayFading = false;
        liftGain = settings.lift;
        vibrato.snap();
        if (isSpireMode (activeMode))
            spire[activeMode].snap();
        else
            massive[activeMode == ModeMassiveSmall ? 1 : 0].reset();   // starts at its knobs
        pingPong.snap();
    }
}

void ReverbEngine::resetMode (int mode) noexcept
{
    if (isSpireMode (mode))
        spire[mode].reset();
    else if (isMassiveMode (mode))
        massive[mode == ModeMassiveSmall ? 1 : 0].reset();
}

void ReverbEngine::runMode (int mode, const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    if (isSpireMode (mode))
        spire[mode].process (inL, inR, outL, outR, n);
    else if (isMassiveMode (mode))
        massive[mode == ModeMassiveSmall ? 1 : 0].process (inL, inR, outL, outR, n);
    else
    {
        std::fill (outL, outL + n, 0.0f);
        std::fill (outR, outR + n, 0.0f);
    }
}

void ReverbEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                            const float* keyL, const float* keyR) noexcept
{
    const int n = std::min (numSamples, kMaxBlock);

    // --- predelay ------------------------------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        predelay[0].write (inL[i]);
        predelay[1].write (inR[i]);

        if (! predelayFading && std::abs (predelayTarget - predelayCurrent) > 0.5f)
        {
            predelayNext = predelayTarget;
            predelayFade = 0.0f;
            predelayFading = true;
        }

        if (! predelayFading)
        {
            preL[i] = predelay[0].readFrac (predelayCurrent);
            preR[i] = predelay[1].readFrac (predelayCurrent);
            continue;
        }

        const float a = predelayFade;
        preL[i] = predelay[0].readFrac (predelayCurrent) * (1.0f - a) + predelay[0].readFrac (predelayNext) * a;
        preR[i] = predelay[1].readFrac (predelayCurrent) * (1.0f - a) + predelay[1].readFrac (predelayNext) * a;

        predelayFade += predelayFadeStep;
        if (predelayFade >= 1.0f)
        {
            predelayCurrent = predelayNext;
            predelayFading = false;
        }
    }

    // --- Lift: octave-up copy into the reverb only -------------------------------------
    const float liftTarget = settings.lift;
    if (liftTarget > 0.0f || liftGain > 1.0e-5f)
    {
        for (int i = 0; i < n; ++i)
        {
            liftGain += (liftTarget - liftGain) * liftSmoothing;
            preL[i] += liftGain * liftLowCut[0].process (liftShifter[0].process (preL[i]));
            preR[i] += liftGain * liftLowCut[1].process (liftShifter[1].process (preR[i]));
        }
    }
    else
    {
        liftGain = 0.0f;
    }

    // --- reverb -----------------------------------------------------------------
    runMode (activeMode, preL, preR, outL, outR, n);

    if (fadingMode >= 0)
    {
        runMode (fadingMode, silence, silence, tmpL, tmpR, n);
        for (int i = 0; i < n; ++i)
        {
            outL[i] += tmpL[i] * fadeGain;
            outR[i] += tmpR[i] * fadeGain;
            fadeGain = std::max (0.0f, fadeGain - fadeStep);
        }

        if (fadeGain <= 0.0f)
        {
            const int finished = fadingMode;
            fadingMode = -1;
            resetMode (finished);
        }
    }

    // --- Modulation + Hi Shelf ------------------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        vibrato.process (outL[i], outR[i]);
        outL[i] = shelf[0].process (outL[i]);
        outR[i] = shelf[1].process (outR[i]);
    }

    // --- Sidechain: the dry input (or the external key) ducks the reverb ---------------
    ducker.process (keyL != nullptr ? keyL : inL, keyR != nullptr ? keyR : inR, outL, outR, n);

    // --- PingPong delay: last, just before the plugin's Dry/Wet -------------------------
    pingPong.process (outL, outR, n);
}

} // namespace lv
