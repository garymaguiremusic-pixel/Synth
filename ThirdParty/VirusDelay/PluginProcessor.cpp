#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace vd;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VirusDelayAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Every knob runs 0..100 (centre 50). describe == nullptr shows just the number.
    auto knob = [&layout] (const char* id, const char* name, float def, String (*describe) (float))
    {
        auto attributes = AudioParameterFloatAttributes()
            .withStringFromValueFunction ([describe] (float v, int)
                                          { return describe != nullptr ? formatKnobValue (v) + "  (" + describe (v) + ")" : formatKnobValue (v); })
            .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (0.0f, 100.0f), def, attributes));
    };

    auto choice = [&layout] (const char* id, const char* name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    auto toggle = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // Shared
    choice (ParamIDs::mode, "Mode", getModeNames(), ModeSimple);
    knob (ParamIDs::send,     "Send",     63.0f, describeSend);
    knob (ParamIDs::feedback, "Feedback", 38.0f, describeFeedback);

    // Classic
    choice (ParamIDs::pingPongRatio, "Ping Pong Ratio", getClassicSubNames (ClassicKind::PingPong), 0);
    choice (ParamIDs::patternType,   "Pattern",         getClassicSubNames (ClassicKind::Pattern), 0);
    knob (ParamIDs::time,  "Time",  75.0f, describeTime);
    choice (ParamIDs::clock, "Clock", getClockNames(), kClockOff);
    knob (ParamIDs::color, "Color", 50.0f, describeColor);
    knob (ParamIDs::rate,  "Rate",  20.0f, describeRate);     // 1/1
    knob (ParamIDs::depth, "Depth",  0.0f, describeDepth);
    choice (ParamIDs::shape, "Shape", getShapeNames(), ShapeSine);

    // Tape
    choice (ParamIDs::clockLeft,  "Tape Clock Left",  getTapeClockNames(), 3);
    choice (ParamIDs::clockRight, "Tape Clock Right", getTapeClockNames(), 4);
    choice (ParamIDs::tapeClock,  "Tape Clock",       getTapeClockNames(), 4);
    knob (ParamIDs::centerFreq, "Center Frequency", 50.0f, describeCenter);
    knob (ParamIDs::bandwidth,  "Bandwidth",        79.0f, describeBandwidth);
    knob (ParamIDs::modulation, "Modulation",       16.0f, describeModulation);

    // Spire
    knob (ParamIDs::spireDelayL,   "Spire Delay L",  50.0f, nullptr);                 // 1/4 when synced
    knob (ParamIDs::spireDelayR,   "Spire Delay R",  60.0f, nullptr);                 // 3/8 when synced
    toggle (ParamIDs::spirePingPong, "Spire Ping-Pong", false);
    toggle (ParamIDs::spireSync,     "Spire Sync",      true);
    knob (ParamIDs::spireRate,     "Spire Rate",     20.0f, describeRate);
    knob (ParamIDs::spireModulate, "Spire Modulate",  0.0f, describeSpireModulate);
    knob (ParamIDs::spireFeedback, "Spire F.Back",   40.0f, describeSpireFeedback);
    knob (ParamIDs::spireWide,     "Spire Wide",    100.0f, nullptr);
    knob (ParamIDs::spireColor,    "Spire Color",    50.0f, describeSpireColor);
    knob (ParamIDs::spireDryWet,   "Spire Dry/Wet",  30.0f, describeSpireDryWet);

    // Output section (all modes)
    knob (ParamIDs::drySource,  "Dry Source",   0.0f, describeDrySource);
    toggle (ParamIDs::duckOn,     "Duck",        false);
    knob (ParamIDs::duckAmount, "Duck Amount", 50.0f, describeDuckAmount);
    toggle (ParamIDs::moveHP,     "Move HP",     false);
    toggle (ParamIDs::movePhaser, "Move Phaser", false);
    toggle (ParamIDs::moveFlanger, "Move Flanger", false);
    knob (ParamIDs::moveRate,   "Move Rate",   27.0f, describeRate);     // 1/2
    knob (ParamIDs::moveDepth,  "Move Depth",  63.0f, describeMoveDepth);
    knob (ParamIDs::moveMix,    "Move Mix",   100.0f, describeMoveMix);
    knob (ParamIDs::width,      "Width",       50.0f, describeWidth);

    // Global
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::bpm, 1 }, "Internal BPM",
                                                       NormalisableRange<float> (40.0f, 240.0f, 0.1f), kDefaultBpm,
                                                       AudioParameterFloatAttributes().withLabel ("BPM")));
    return layout;
}

//==============================================================================
VirusDelayAudioProcessor::VirusDelayAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VirusDelayState", createParameterLayout())
{
    pMode     = apvts.getRawParameterValue (ParamIDs::mode);
    pSend     = apvts.getRawParameterValue (ParamIDs::send);
    pTime     = apvts.getRawParameterValue (ParamIDs::time);
    pClock    = apvts.getRawParameterValue (ParamIDs::clock);
    pFeedback = apvts.getRawParameterValue (ParamIDs::feedback);
    pColor    = apvts.getRawParameterValue (ParamIDs::color);
    pRate     = apvts.getRawParameterValue (ParamIDs::rate);
    pDepth    = apvts.getRawParameterValue (ParamIDs::depth);
    pShape    = apvts.getRawParameterValue (ParamIDs::shape);
    pPingPongRatio = apvts.getRawParameterValue (ParamIDs::pingPongRatio);
    pPatternType   = apvts.getRawParameterValue (ParamIDs::patternType);
    pWidth         = apvts.getRawParameterValue (ParamIDs::width);
    pClockLeft   = apvts.getRawParameterValue (ParamIDs::clockLeft);
    pClockRight  = apvts.getRawParameterValue (ParamIDs::clockRight);
    pTapeClock   = apvts.getRawParameterValue (ParamIDs::tapeClock);
    pCenterFreq  = apvts.getRawParameterValue (ParamIDs::centerFreq);
    pBandwidth   = apvts.getRawParameterValue (ParamIDs::bandwidth);
    pModulation  = apvts.getRawParameterValue (ParamIDs::modulation);
    pSpireDelayL   = apvts.getRawParameterValue (ParamIDs::spireDelayL);
    pSpireDelayR   = apvts.getRawParameterValue (ParamIDs::spireDelayR);
    pSpirePingPong = apvts.getRawParameterValue (ParamIDs::spirePingPong);
    pSpireSync     = apvts.getRawParameterValue (ParamIDs::spireSync);
    pSpireRate     = apvts.getRawParameterValue (ParamIDs::spireRate);
    pSpireModulate = apvts.getRawParameterValue (ParamIDs::spireModulate);
    pSpireFeedback = apvts.getRawParameterValue (ParamIDs::spireFeedback);
    pSpireWide     = apvts.getRawParameterValue (ParamIDs::spireWide);
    pSpireColor    = apvts.getRawParameterValue (ParamIDs::spireColor);
    pSpireDryWet   = apvts.getRawParameterValue (ParamIDs::spireDryWet);
    pDrySource   = apvts.getRawParameterValue (ParamIDs::drySource);
    pDuckOn      = apvts.getRawParameterValue (ParamIDs::duckOn);
    pDuckAmount  = apvts.getRawParameterValue (ParamIDs::duckAmount);
    pMoveHP      = apvts.getRawParameterValue (ParamIDs::moveHP);
    pMovePhaser  = apvts.getRawParameterValue (ParamIDs::movePhaser);
    pMoveFlanger = apvts.getRawParameterValue (ParamIDs::moveFlanger);
    pMoveRate    = apvts.getRawParameterValue (ParamIDs::moveRate);
    pMoveDepth   = apvts.getRawParameterValue (ParamIDs::moveDepth);
    pMoveMix     = apvts.getRawParameterValue (ParamIDs::moveMix);
    pBypass   = apvts.getRawParameterValue (ParamIDs::bypass);
    pBpm      = apvts.getRawParameterValue (ParamIDs::bpm);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::bypass));
    randomiser.setSeedRandomly();

    // Start on the first factory preset so the preset box and the sound agree.
    setCurrentProgram (0);
}

bool VirusDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

//==============================================================================
void VirusDelayAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    engine.prepare (sampleRate);
    tape.prepare (sampleRate);
    spire.prepare (sampleRate);
    movement.prepare (sampleRate);
    width.prepare (sampleRate);
    ducker.prepare (sampleRate);
    drySourceGain.reset (sampleRate, 0.03);
    drySourceGain.setCurrentAndTargetValue (drySourceToGain (pDrySource->load()));
    lfo.prepare (sampleRate);
    spireLfo.prepare (sampleRate);
    spireLfo.setShape (ShapeSine);

    sendSmoothed.reset (sampleRate, 0.02);
    wetGate.reset (sampleRate, 0.02);
    rateSmoothed.reset (sampleRate, 0.05);

    sendSmoothed.setCurrentAndTargetValue (sendToGain (pSend->load()));
    wetGate.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 0.0f : 1.0f);
    classicOutGain.reset (sampleRate, 0.03);
    tapeOutGain.reset (sampleRate, 0.03);
    rateSmoothed.setCurrentAndTargetValue (rateKnobToHz (pRate->load(), tempoBpm));
    spireRateSmoothed.reset (sampleRate, 0.05);
    spireRateSmoothed.setCurrentAndTargetValue (rateKnobToHz (pSpireRate->load(), tempoBpm));
    dryGain.reset (sampleRate, 0.03);
    spireWetGain.reset (sampleRate, 0.03);
    // Start every output gain at its final value so the delay is heard from the
    // very first sample (no fade-in) and nothing leaks through on load.
    const int startMode = juce::jlimit (0, NumDelayModes - 1, (int) pMode->load());
    const bool startBypassed = pBypass->load() > 0.5f;
    const float mix = spireDryWetToMix (pSpireDryWet->load());
    classicOutGain.setCurrentAndTargetValue (isClassicMode (startMode) ? 1.0f : 0.0f);
    tapeOutGain.setCurrentAndTargetValue (isTapeMode (startMode) ? 1.0f : 0.0f);
    dryGain.setCurrentAndTargetValue (isSpireMode (startMode) && ! startBypassed ? std::cos (mix * juce::MathConstants<float>::halfPi) : 1.0f);
    spireWetGain.setCurrentAndTargetValue (isSpireMode (startMode) ? std::sin (mix * juce::MathConstants<float>::halfPi) : 0.0f);
    peakHold = 0.0f;
}

void VirusDelayAudioProcessor::reset()
{
    engine.reset();
    tape.reset();
    spire.reset();
    movement.reset();
    width.reset();
    ducker.reset();
    lfo.reset();
    spireLfo.reset();
}

void VirusDelayAudioProcessor::updateTempo() noexcept
{
    bool fromHost = false;

    if (auto* ph = getPlayHead())
        if (auto position = ph->getPosition())
            if (auto bpm = position->getBpm())
                if (*bpm > 1.0)
                {
                    tempoBpm = *bpm;
                    fromHost = true;
                }

    if (! fromHost)
        tempoBpm = (double) pBpm->load();

    gui.bpm.store ((float) tempoBpm);
    gui.hostTempo.store (fromHost);
}

void VirusDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0 || buffer.getNumChannels() == 0)
        return;

    if (numIn == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    updateTempo();

    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < numSamples; start += kChunkSize)
    {
        const int n = std::min (kChunkSize, numSamples - start);
        processChunk (left + start, right != nullptr ? right + start : nullptr, n);
    }
}

void VirusDelayAudioProcessor::processChunk (float* left, float* right, int n) noexcept
{
    const int   mode        = juce::jlimit (0, NumDelayModes - 1, (int) pMode->load());
    const int   classicType = classicTypeFor (mode, (int) pPingPongRatio->load(), (int) pPatternType->load());
    const int   clock       = juce::jlimit (0, 13, (int) pClock->load());
    const float feedback    = feedbackToGain (pFeedback->load());
    const bool  tapeMode    = isTapeMode (mode);
    const bool  spireMode   = isSpireMode (mode);
    const bool  classicOn   = isClassicMode (mode);
    const double quarterMs  = 60000.0 / std::max (1.0, tempoBpm);

    // --- Classic engine ----------------------------------------------------------
    float classicMs = clock != kClockOff ? (float) (quarterMs * clockDivisionInBeats (clock))
                                         : timeKnobToMs (pTime->load());
    classicMs = juce::jlimit (kMinDelayMs, kMaxDelayMs, classicMs);

    // All three engines always listen to the input, even while bypassed or not
    // selected, so switching on, un-bypassing or changing mode gives repeats
    // straight away (their buffers already hold what was just played). Mode,
    // and bypass only choose which engine is heard, with a 30 ms fade.
    if (classicOn)
        lastClassicType = classicType;   // other modes: keeps listening with the last routing

    engine.setActive (true);
    engine.setParameters (lastClassicType, classicMs, feedback, colorToBipolar (pColor->load()), depthToMs (pDepth->load()));
    classicOutGain.setTargetValue (classicOn ? 1.0f : 0.0f);

    // --- Tape engine -------------------------------------------------------------
    TapeDelay::Settings ts;
    if (mode == ModeTapeClocked)
    {
        ts.delayMs[0] = (float) (quarterMs * tapeClockInBeats ((int) pClockLeft->load()));
        ts.delayMs[1] = (float) (quarterMs * tapeClockInBeats ((int) pClockRight->load()));
    }
    else
    {
        ts.delayMs[0] = ts.delayMs[1] = (float) (quarterMs * tapeClockInBeats ((int) pTapeClock->load()));
    }
    for (auto& ms : ts.delayMs)
        ms = juce::jlimit (kMinDelayMs, kMaxDelayMs, ms);

    ts.feedback = feedback;
    ts.centreHz = centerToHz (pCenterFreq->load());
    ts.bandwidthOctaves = bandwidthToOctaves (pBandwidth->load());
    ts.modulation = modulationToAmount (pModulation->load());
    ts.doppler = mode == ModeTapeDoppler;

    tape.setActive (true);
    tapeOutGain.setTargetValue (tapeMode ? 1.0f : 0.0f);
    tape.setParameters (ts);

    // --- Spire engine ------------------------------------------------------------
    SpireDelay::Settings ss;
    const bool spireSync = pSpireSync->load() > 0.5f;
    const float spireKnobs[2] = { pSpireDelayL->load(), pSpireDelayR->load() };
    for (int c = 0; c < 2; ++c)
        ss.delayMs[c] = spireSync ? (float) std::min ((double) kSpireMaxDelayMs, quarterMs * spireDivisionInBeats (spireKnobToDivision (spireKnobs[c])))
                                  : spireKnobToFreeMs (spireKnobs[c]);
    ss.feedback     = spireFeedbackToGain (pSpireFeedback->load());
    ss.wide         = spireToBipolar (pSpireWide->load());
    ss.pingPong     = pSpirePingPong->load() > 0.5f;
    ss.colorBipolar = spireToBipolar (pSpireColor->load());
    ss.modDepthMs   = spireModulateToMs (pSpireModulate->load());

    spire.setActive (true);
    spire.setParameters (ss);

    // Spire uses a Dry/Wet crossfade (equal power) instead of the Send amount.
    const float mix = spireDryWetToMix (pSpireDryWet->load());
    const bool bypassed = pBypass->load() > 0.5f;
    dryGain.setTargetValue (spireMode && ! bypassed ? std::cos (mix * juce::MathConstants<float>::halfPi) : 1.0f);
    spireWetGain.setTargetValue (spireMode ? std::sin (mix * juce::MathConstants<float>::halfPi) : 0.0f);

    spireRateSmoothed.setTargetValue (rateKnobToHz (pSpireRate->load(), tempoBpm));
    const float spireRateStart = spireRateSmoothed.getCurrentValue();
    spireRateSmoothed.skip (n);
    spireLfo.process (spireLfoA, spireLfoB, n, spireRateStart, spireRateSmoothed.getCurrentValue());

    // --- LFO ---------------------------------------------------------------------
    lfo.setShape ((int) pShape->load());
    rateSmoothed.setTargetValue (rateKnobToHz (pRate->load(), tempoBpm));
    const float rateStart = rateSmoothed.getCurrentValue();
    rateSmoothed.skip (n);
    lfo.process (lfoA, lfoB, n, rateStart, rateSmoothed.getCurrentValue());

    // --- send (and bypass gate) ---------------------------------------------------
    sendSmoothed.setTargetValue (sendToGain (pSend->load()));
    wetGate.setTargetValue (pBypass->load() > 0.5f ? 0.0f : 1.0f);

    const float* inR = right != nullptr ? right : left;
    for (int i = 0; i < n; ++i)
    {
        gate[i] = wetGate.getNextValue();
        const float g = sendSmoothed.getNextValue();   // not gated by bypass: the delay keeps listening
        sendL[i] = left[i] * g;
        sendR[i] = inR[i] * g;
        spireInL[i] = left[i];
        spireInR[i] = inR[i];
    }

    engine.process (sendL, sendR, engL, engR, lfoA, lfoB, n);
    tape.process (sendL, sendR, tapL, tapR, n);
    spire.process (spireInL, spireInR, spL, spR, spireLfoA, spireLfoB, n);

    // --- wet bus: sum of the engines -> Movement -> Duck ---------------------------
    for (int i = 0; i < n; ++i)
    {
        const float classicGain = classicOutGain.getNextValue();
        const float tapeGain = tapeOutGain.getNextValue();
        const float spireGain = spireWetGain.getNextValue();
        wetL[i] = engL[i] * classicGain + tapL[i] * tapeGain + spL[i] * spireGain;
        wetR[i] = engR[i] * classicGain + tapR[i] * tapeGain + spR[i] * spireGain;
    }

    MovementFX::Settings ms;
    ms.hpOn     = pMoveHP->load() > 0.5f;
    ms.phaserOn = pMovePhaser->load() > 0.5f;
    ms.flangerOn = pMoveFlanger->load() > 0.5f;
    ms.rateHz   = rateKnobToHz (pMoveRate->load(), tempoBpm);
    ms.depth    = moveDepthToNorm (pMoveDepth->load());
    ms.mix      = norm100 (pMoveMix->load());
    movement.setParameters (ms);
    movement.process (wetL, wetR, n);

    width.setWidth (widthToFactor (pWidth->load()));
    width.process (wetL, wetR, n);

    // The key is the untouched input (still in left/right at this point).
    ducker.setParameters (pDuckOn->load() > 0.5f, duckAmountToNorm (pDuckAmount->load()));
    ducker.process (left, inR, wetL, wetR, n);

    // --- output: mode's dry path + Dry Source (untouched dry, all modes) + wet ------
    drySourceGain.setTargetValue (drySourceToGain (pDrySource->load()));
    for (int i = 0; i < n; ++i)
    {
        const float dry = dryGain.getNextValue() + drySourceGain.getNextValue() * gate[i];
        const float inLeft = left[i], inRight = inR[i];
        left[i] = inLeft * dry + wetL[i] * gate[i];
        if (right != nullptr)
            right[i] = inRight * dry + wetR[i] * gate[i];
    }

    // --- GUI meters ------------------------------------------------------------
    // Saturation meter: only the engine that is being heard.
    const float classicPeak = engine.getAndResetLoopPeak(), tapePeak = tape.getAndResetLoopPeak(), spirePeak = spire.getAndResetLoopPeak();
    const float peak = tapeMode ? tapePeak : (spireMode ? spirePeak : (classicOn ? classicPeak : 0.0f));
    peakHold = std::max (peak, peakHold * (float) std::exp (-(double) n / (0.3 * currentSampleRate)));

    gui.loopPeak.store (peakHold);
    gui.duckReductionDb.store (ducker.getReductionDb());
    gui.movePhase.store (movement.getPhase());
    gui.lfoPhase.store ((float) lfo.getPhase());
    gui.tapeDelayMsL.store (tape.getCurrentDelayMs (0));
    gui.tapeDelayMsR.store (tape.getCurrentDelayMs (1));
    gui.tapeSpeed.store (tape.getSpeedRatio());
    gui.tapeModPhase.store (tape.getModPhase());
    gui.baseDelayMs.store (classicMs);
    gui.spireDelayMsL.store (ss.delayMs[0]);
    gui.spireDelayMsR.store (ss.delayMs[1]);
}

//==============================================================================
int VirusDelayAudioProcessor::getNumPrograms()
{
    return numFactoryPresets;
}

const juce::String VirusDelayAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numFactoryPresets) ? juce::String (factoryPresets[index].name) : juce::String();
}

void VirusDelayAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numFactoryPresets))
        return;

    currentProgram.store (index);
    const auto& p = factoryPresets[index];

    auto set = [this] (const char* id, float value) { setParameterValue (id, value); };

    set (ParamIDs::mode,     (float) p.mode);
    set (ParamIDs::send,     p.send);
    {
        const int type = findClassicType (p.classicType);
        const auto kind = getClassicType (type).kind;
        if (kind == ClassicKind::PingPong) set (ParamIDs::pingPongRatio, (float) (type - firstClassicTypeOf (kind)));
        if (kind == ClassicKind::Pattern)  set (ParamIDs::patternType,   (float) (type - firstClassicTypeOf (kind)));
    }
    set (ParamIDs::clockLeft,   (float) p.clockLeft);
    set (ParamIDs::clockRight,  (float) p.clockRight);
    set (ParamIDs::tapeClock,   (float) p.tapeClock);
    set (ParamIDs::centerFreq,  p.centerFreq);
    set (ParamIDs::bandwidth,   p.bandwidth);
    set (ParamIDs::modulation,  p.modulation);
    set (ParamIDs::spireDelayL,   p.spireDelayL);
    set (ParamIDs::spireDelayR,   p.spireDelayR);
    set (ParamIDs::spirePingPong, p.spirePingPong ? 1.0f : 0.0f);
    set (ParamIDs::spireSync,     p.spireSync ? 1.0f : 0.0f);
    set (ParamIDs::spireRate,     p.spireRate);
    set (ParamIDs::spireModulate, p.spireModulate);
    set (ParamIDs::spireFeedback, p.spireFeedback);
    set (ParamIDs::spireWide,     p.spireWide);
    set (ParamIDs::spireColor,    p.spireColor);
    set (ParamIDs::spireDryWet,   p.spireDryWet);
    set (ParamIDs::time,     p.time);
    set (ParamIDs::clock,    (float) p.clock);
    set (ParamIDs::feedback, p.feedback);
    set (ParamIDs::color,    p.color);
    set (ParamIDs::rate,     p.rate);
    set (ParamIDs::depth,    p.depth);
    set (ParamIDs::shape,    (float) p.shape);
}

void VirusDelayAudioProcessor::setParameterValue (const char* id, float value)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

void VirusDelayAudioProcessor::randomise()
{
    // Message thread only (called from the editor's Randomise button).
    auto& r = randomiser;
    auto knob   = [&r] (float lo, float hi) { return (float) juce::roundToInt (lo + r.nextFloat() * (hi - lo)); };
    auto pick   = [&r] (int count) { return (float) r.nextInt (count); };
    auto chance = [&r] (float probability) { return r.nextFloat() < probability ? 1.0f : 0.0f; };

    // Any mode. Ranges are kept musical: feedback can't run away.
    const int mode = r.nextInt (NumDelayModes);
    setParameterValue (ParamIDs::mode, (float) mode);

    if (isClassicMode (mode))
    {
        setParameterValue (ParamIDs::pingPongRatio, pick (countClassicTypesOf (ClassicKind::PingPong)));
        setParameterValue (ParamIDs::patternType,   pick (countClassicTypesOf (ClassicKind::Pattern)));
        setParameterValue (ParamIDs::clock,    r.nextFloat() < 0.3f ? (float) kClockOff : (float) (3 + r.nextInt (11)));  // Off or 1/16 .. 1/1
        setParameterValue (ParamIDs::send,     knob (45, 80));
        setParameterValue (ParamIDs::time,     knob (40, 85));
        setParameterValue (ParamIDs::feedback, knob (10, 70));
        setParameterValue (ParamIDs::color,    knob (25, 75));
        setParameterValue (ParamIDs::rate,     knob (0, 100));
        setParameterValue (ParamIDs::depth,    r.nextFloat() < 0.4f ? 0.0f : knob (5, 45));
        setParameterValue (ParamIDs::shape,    pick (NumLfoShapes));
    }
    else if (isTapeMode (mode))
    {
        setParameterValue (ParamIDs::clockLeft,  pick (6));
        setParameterValue (ParamIDs::clockRight, pick (6));
        setParameterValue (ParamIDs::tapeClock,  pick (6));
        setParameterValue (ParamIDs::send,       knob (45, 80));
        setParameterValue (ParamIDs::feedback,   knob (20, 75));
        setParameterValue (ParamIDs::centerFreq, knob (25, 80));
        setParameterValue (ParamIDs::bandwidth,  knob (35, 100));
        setParameterValue (ParamIDs::modulation, knob (0, 60));
    }
    else // Spire
    {
        setParameterValue (ParamIDs::spireDelayL,   knob (20, 80));
        setParameterValue (ParamIDs::spireDelayR,   knob (20, 80));
        setParameterValue (ParamIDs::spirePingPong, chance (0.5f));
        setParameterValue (ParamIDs::spireSync,     chance (0.8f));
        setParameterValue (ParamIDs::spireRate,     knob (0, 100));
        setParameterValue (ParamIDs::spireModulate, r.nextFloat() < 0.4f ? 0.0f : knob (5, 40));
        setParameterValue (ParamIDs::spireFeedback, knob (15, 75));
        setParameterValue (ParamIDs::spireWide,     knob (0, 100));
        setParameterValue (ParamIDs::spireColor,    knob (25, 75));
        setParameterValue (ParamIDs::spireDryWet,   knob (20, 55));
    }

    // Movement Rate / Depth (the on/off buttons, Duck and Dry Source are left alone).
    setParameterValue (ParamIDs::moveRate,  knob (0, 100));
    setParameterValue (ParamIDs::moveDepth, knob (20, 90));
}

//==============================================================================
void VirusDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);
    state.setProperty ("editorWidth", gui.editorWidth.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VirusDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram.store (juce::jlimit (0, numFactoryPresets - 1, (int) state.getProperty ("program", 0)));
            gui.editorWidth.store ((int) state.getProperty ("editorWidth", 0));
            apvts.replaceState (state);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* VirusDelayAudioProcessor::createEditor()
{
    return new VirusDelayAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VirusDelayAudioProcessor();
}
