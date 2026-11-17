#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace lv;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LeadVerbAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Spire-style knobs (0..1000 or 0..100); the host shows the value plus the real unit.
    auto knob = [&layout] (const char* id, const char* name, float max, float def, String (*describe) (float))
    {
        auto attributes = AudioParameterFloatAttributes()
            .withStringFromValueFunction ([describe] (float v, int)
            {
                return describe != nullptr ? formatKnobValue (v) + "  (" + describe (v) + ")" : formatKnobValue (v);
            })
            .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (0.0f, max, 0.1f), def, attributes));   // steps of 0.1 (shift)
    };

    // Reverb section: Mode 1 (Spire) or Mode 2 (Massive)
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::section, 1 }, "Reverb Section", getSectionNames(), SectionSpire));

    // Mode 1 (Spire)
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode", getModeNames(), ModePlate1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::sync, 1 }, "Sync", false));
    knob (ParamIDs::predelay, "Predelay", 1000.0f, 200.0f, nullptr);   // unit depends on Sync
    knob (ParamIDs::damp,     "Damp",      100.0f,  15.0f, describeDamp);   // Spire's default
    knob (ParamIDs::decay,    "Decay",    1000.0f, 635.7f, nullptr);   // unit depends on Mode (Spire's default)
    knob (ParamIDs::modulation, "Modulation", 100.0f, 0.0f, describeModulation);
    knob (ParamIDs::wide,     "Wide",      100.0f,  50.0f, describeWide);   // double-click = 50, natural width
    knob (ParamIDs::color,    "Color",     100.0f,  65.0f, describeColor);   // Spire's default
    knob (ParamIDs::shelf,    "Hi Shelf",  100.0f,   0.0f, describeShelf);
    knob (ParamIDs::sidechain, "Sidechain", 100.0f,  0.0f, describeSidechain);
    knob (ParamIDs::dryWet,   "Dry/Wet",   100.0f,  50.0f, describeDryWet);
    knob (ParamIDs::drySource, "Dry Source", 100.0f, 0.0f, describeDrySource);
    knob (ParamIDs::lift,     "Lift",      100.0f,   0.0f, describeLift);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::externalSidechain, 1 }, "External Sidechain", false));

    // PingPong delay on the reverb, after everything else
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::delayOn, 1 }, "Delay On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::delayTime, 1 }, "Delay Time", getDelayNoteNames(), 2));   // 1/8
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::delayType, 1 }, "Delay Type", getDelayNoteTypeNames(), DelayDotted));
    knob (ParamIDs::delayFeedback, "Delay Feedback", 100.0f, 45.0f, describeDelayFeedback);
    knob (ParamIDs::delayMix,      "Delay Mix",      100.0f, 30.0f, describeDelayMix);

    // Mode 2 (Massive)
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::massiveType, 1 }, "Massive Type", getMassiveTypeNames(), MassiveReverb::Reverb));
    knob (ParamIDs::massiveDryWet,  "Massive Dry/Wet", 100.0f, 35.0f, describeMassiveDryWet);
    knob (ParamIDs::massiveSize,    "Massive Size",    100.0f, 50.0f, nullptr);   // unit depends on the type
    knob (ParamIDs::massiveDensity, "Massive Density", 100.0f, 50.0f, describeMassiveDensity);
    knob (ParamIDs::massiveColor,   "Massive Color",   100.0f, 50.0f, nullptr);   // unit depends on the type

    // Global
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::bpm, 1 }, "Internal BPM",
                                                       NormalisableRange<float> (40.0f, 240.0f, 0.1f), kDefaultBpm,
                                                       AudioParameterFloatAttributes().withLabel ("BPM")));
    return layout;
}

//==============================================================================
bool LeadVerbAudioProcessor::massiveSection() const noexcept
{
    return pSection->load() > 0.5f;
}

// Mode 1: Spire's dry/wet law (ReverbMath.h); Mode 2: its own Dry/Wet, an equal-power
// crossfade. In both, Dry Source raises the dry towards unity (at 100 the dry is always
// at full level, so Dry/Wet only sets how much reverb is added, like a send).
void LeadVerbAudioProcessor::mixGains (float& dry, float& wet) const noexcept
{
    // Dry Source (global) raises the dry towards unity in either section
    const float drySource = drySourceKnobToAmount (pDrySource->load());
    float crossfadeDry;
    if (massiveSection())
    {
        crossfadeDry = massiveDryGain (pMassiveDryWet->load());
        wet = massiveWetGain (pMassiveDryWet->load());
    }
    else
    {
        const float mix = dryWetToMix (pDryWet->load());
        crossfadeDry = mixDryGain (mix);
        wet = mixWetGain (mix);
    }
    dry = crossfadeDry + (1.0f - crossfadeDry) * drySource;
}

//==============================================================================
LeadVerbAudioProcessor::LeadVerbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "LeadVerbState", createParameterLayout())
{
    pMode     = apvts.getRawParameterValue (ParamIDs::mode);
    pSync     = apvts.getRawParameterValue (ParamIDs::sync);
    pPredelay = apvts.getRawParameterValue (ParamIDs::predelay);
    pDamp     = apvts.getRawParameterValue (ParamIDs::damp);
    pWide     = apvts.getRawParameterValue (ParamIDs::wide);
    pDecay    = apvts.getRawParameterValue (ParamIDs::decay);
    pColor    = apvts.getRawParameterValue (ParamIDs::color);
    pDryWet   = apvts.getRawParameterValue (ParamIDs::dryWet);
    pShelf    = apvts.getRawParameterValue (ParamIDs::shelf);
    pSidechain = apvts.getRawParameterValue (ParamIDs::sidechain);
    pExternalSidechain = apvts.getRawParameterValue (ParamIDs::externalSidechain);
    pLift      = apvts.getRawParameterValue (ParamIDs::lift);
    pDrySource = apvts.getRawParameterValue (ParamIDs::drySource);
    pModulation = apvts.getRawParameterValue (ParamIDs::modulation);
    pDelayOn       = apvts.getRawParameterValue (ParamIDs::delayOn);
    pDelayTime     = apvts.getRawParameterValue (ParamIDs::delayTime);
    pDelayType     = apvts.getRawParameterValue (ParamIDs::delayType);
    pDelayFeedback = apvts.getRawParameterValue (ParamIDs::delayFeedback);
    pDelayMix      = apvts.getRawParameterValue (ParamIDs::delayMix);
    pSection       = apvts.getRawParameterValue (ParamIDs::section);
    pMassiveType   = apvts.getRawParameterValue (ParamIDs::massiveType);
    pMassiveDryWet = apvts.getRawParameterValue (ParamIDs::massiveDryWet);
    pMassiveSize   = apvts.getRawParameterValue (ParamIDs::massiveSize);
    pMassiveDensity = apvts.getRawParameterValue (ParamIDs::massiveDensity);
    pMassiveColor  = apvts.getRawParameterValue (ParamIDs::massiveColor);
    pBypass   = apvts.getRawParameterValue (ParamIDs::bypass);
    pBpm      = apvts.getRawParameterValue (ParamIDs::bpm);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::bypass));

    // Start on the first factory preset so the preset box and the sound agree.
    setCurrentProgram (0);
}

bool LeadVerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono())
        return false;

    // optional external sidechain: off, mono or stereo
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
    }
    return true;
}

//==============================================================================
void LeadVerbAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);

    wetGate.reset (sampleRate, 0.02);
    dryGain.reset (sampleRate, 0.03);
    wetGain.reset (sampleRate, 0.03);

    // Start the gains at their final values so nothing leaks through on load.
    const bool bypassed = pBypass->load() > 0.5f;
    float dry = 1.0f, wet = 0.0f;
    mixGains (dry, wet);
    wetGate.setCurrentAndTargetValue (bypassed ? 0.0f : 1.0f);
    dryGain.setCurrentAndTargetValue (bypassed ? 1.0f : dry);
    wetGain.setCurrentAndTargetValue (wet);
    duckHold = 0.0f;
}

void LeadVerbAudioProcessor::reset()
{
    engine.reset();
}

void LeadVerbAudioProcessor::updateTempo() noexcept
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

void LeadVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    // The buffer holds the main input, then the sidechain input; the main output
    // is channels 0 and 1. Read each chunk of the sidechain before channel 1 can
    // be overwritten (a mono main input is copied into channel 1).
    const int mainIn = getMainBusNumInputChannels();
    int scFirst = -1, scChannels = 0;
    if (auto* sc = getBus (true, 1); sc != nullptr && sc->isEnabled() && sc->getNumberOfChannels() > 0)
    {
        scFirst = getChannelIndexInProcessBlockBuffer (true, 1, 0);
        scChannels = sc->getNumberOfChannels();
    }

    const bool external = pExternalSidechain->load() > 0.5f;
    gui.externalKeyConnected.store (scChannels > 0);

    updateTempo();

    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    for (int start = 0; start < numSamples; start += kChunkSize)
    {
        const int n = std::min (kChunkSize, numSamples - start);

        const float* key = nullptr;   // nullptr = duck from the main input
        if (external)
        {
            if (scChannels > 0)
            {
                const float* scL = buffer.getReadPointer (scFirst) + start;
                const float* scR = buffer.getReadPointer (scFirst + (scChannels > 1 ? 1 : 0)) + start;
                std::copy (scL, scL + n, keyL);
                std::copy (scR, scR + n, keyR);
            }
            else
            {
                // External chosen but nothing routed in: nothing ducks the reverb.
                std::fill (keyL, keyL + n, 0.0f);
                std::fill (keyR, keyR + n, 0.0f);
            }
            key = keyL;
        }

        if (mainIn == 1)
            std::copy (left + start, left + start + n, right + start);

        processChunk (left + start, right + start, n, key != nullptr);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

ReverbEngine::Settings LeadVerbAudioProcessor::currentSettings() const noexcept
{
    const bool sync = pSync->load() > 0.5f;
    ReverbEngine::Settings s;

    // Global: the sidechain ducking and the PingPong delay work in both sections
    s.sidechain     = sidechainKnobToAmount (pSidechain->load());
    s.delayOn       = pDelayOn->load() > 0.5f;
    s.delaySeconds  = syncedDelaySeconds ((int) pDelayTime->load(), (int) pDelayType->load(), tempoBpm);
    s.delayFeedback = delayFeedbackKnobToGain (pDelayFeedback->load());
    s.delayMix      = delayMixKnobToMix (pDelayMix->load());

    if (massiveSection())
    {
        // Mode 2: Massive's reverb with its own pre-delay; Mode 1's extras stay neutral
        s.mode           = (int) pMassiveType->load() == MassiveReverb::Small ? ModeMassiveSmall : ModeMassiveReverb;
        s.massiveSize    = norm100 (pMassiveSize->load());
        s.massiveDensity = norm100 (pMassiveDensity->load());
        s.massiveColor   = norm100 (pMassiveColor->load());
        return s;
    }

    const int mode = juce::jlimit (0, NumReverbModes - 1, (int) pMode->load());
    s.mode         = mode;
    s.predelayMs   = sync ? syncedPredelayMs (pPredelay->load(), tempoBpm) : predelayKnobToMs (pPredelay->load());
    s.spireDecay   = norm1000 (pDecay->load());
    s.spireDamp    = norm100 (pDamp->load());
    s.spireColor   = norm100 (pColor->load());
    s.width        = wideKnobToWidth (pWide->load());
    s.shelfDb      = shelfKnobToDb (pShelf->load());
    s.lift         = liftKnobToGain (pLift->load());
    s.vibratoCents = modulationKnobToCents (pModulation->load());
    s.vibratoHz    = modulationKnobToRateHz (pModulation->load());
    return s;
}

void LeadVerbAudioProcessor::processChunk (float* left, float* right, int n, bool externalKey) noexcept
{
    const auto s = currentSettings();
    engine.setParameters (s);

    // The active section's Dry/Wet; bypass fades the wet path out and the dry to unity.
    float dryTarget = 1.0f, wetTarget = 0.0f;
    mixGains (dryTarget, wetTarget);
    const bool bypassed = pBypass->load() > 0.5f;
    wetGate.setTargetValue (bypassed ? 0.0f : 1.0f);
    dryGain.setTargetValue (bypassed ? 1.0f : dryTarget);
    wetGain.setTargetValue (wetTarget);

    const float* inR = right != nullptr ? right : left;
    for (int i = 0; i < n; ++i)
    {
        gate[i] = wetGate.getNextValue();
        wetInL[i] = left[i] * gate[i];
        wetInR[i] = inR[i] * gate[i];
    }

    engine.process (wetInL, wetInR, wetL, wetR, n, externalKey ? keyL : nullptr, externalKey ? keyR : nullptr);

    for (int i = 0; i < n; ++i)
    {
        const float dry = dryGain.getNextValue();
        const float wet = wetGain.getNextValue() * gate[i];
        left[i] = left[i] * dry + wetL[i] * wet;
        if (right != nullptr)
            right[i] = right[i] * dry + wetR[i] * wet;
    }

    // --- GUI meters ------------------------------------------------------------
    // hold the deepest ducking briefly so the meter is readable
    const float duck = engine.getAndResetDuckingDb();
    duckHold = std::max (duck, duckHold - (float) n / (float) currentSampleRate * 60.0f);
    gui.duckingDb.store (duckHold);
    gui.predelayMs.store (s.predelayMs);
    gui.decaySeconds.store (isSpireMode (s.mode) ? decayKnobToSeconds (pDecay->load(), s.mode, pDamp->load()) : engine.getMassiveRt60 (s.mode));
    gui.massiveRt60.store (engine.getMassiveRt60 (s.mode));
    gui.massivePredelayMs.store (engine.getMassivePredelayMs (s.mode));
    gui.delaySeconds.store (s.delaySeconds);
}

//==============================================================================
int LeadVerbAudioProcessor::getNumPrograms()
{
    return numFactoryPresets;
}

const juce::String LeadVerbAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numFactoryPresets) ? juce::String (factoryPresets[index].name) : juce::String();
}

void LeadVerbAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numFactoryPresets))
        return;

    currentProgram.store (index);
    const auto& p = factoryPresets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };

    set (ParamIDs::section,  (float) p.section);
    set (ParamIDs::mode,     (float) p.mode);
    set (ParamIDs::sync,     p.sync ? 1.0f : 0.0f);
    set (ParamIDs::predelay, p.predelay);
    set (ParamIDs::damp,     p.damp);
    set (ParamIDs::wide,     p.wide);
    set (ParamIDs::decay,    p.decay);
    set (ParamIDs::color,    p.color);
    set (ParamIDs::dryWet,   p.dryWet);
    set (ParamIDs::shelf,    p.shelf);
    set (ParamIDs::sidechain, p.sidechain);
    set (ParamIDs::lift,     p.lift);
    set (ParamIDs::drySource, p.drySource);
    set (ParamIDs::modulation, p.modulation);
    set (ParamIDs::delayOn,  p.delayOn ? 1.0f : 0.0f);
    set (ParamIDs::delayTime, (float) p.delayTime);
    set (ParamIDs::delayType, (float) p.delayType);
    set (ParamIDs::delayFeedback, p.delayFeedback);
    set (ParamIDs::delayMix, p.delayMix);
    set (ParamIDs::massiveType, (float) p.massiveType);
    set (ParamIDs::massiveDryWet, p.massiveDryWet);
    set (ParamIDs::massiveSize, p.massiveSize);
    set (ParamIDs::massiveDensity, p.massiveDensity);
    set (ParamIDs::massiveColor, p.massiveColor);
}

//==============================================================================
void LeadVerbAudioProcessor::randomiseSettings()
{
    auto& rng = juce::Random::getSystemRandom();

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };

    auto randomKnob = [&] (const char* id)
    {
        const auto range = apvts.getParameter (id)->getNormalisableRange();
        set (id, (float) juce::roundToInt (range.start + rng.nextFloat() * (range.end - range.start)));
    };

    // the global knobs (Sidechain, Dry Source) take part in either section
    for (auto* id : { ParamIDs::sidechain, ParamIDs::drySource })
        randomKnob (id);

    if (massiveSection())
    {
        for (auto* id : { ParamIDs::massiveDryWet, ParamIDs::massiveSize, ParamIDs::massiveDensity, ParamIDs::massiveColor })
            randomKnob (id);
        set (ParamIDs::massiveType, (float) rng.nextInt (2));
        return;
    }

    for (auto* id : { ParamIDs::predelay, ParamIDs::decay, ParamIDs::modulation, ParamIDs::damp, ParamIDs::wide, ParamIDs::color,
                      ParamIDs::shelf, ParamIDs::lift, ParamIDs::dryWet })
        randomKnob (id);
    set (ParamIDs::mode, (float) rng.nextInt (NumReverbModes));
    set (ParamIDs::sync, rng.nextBool() ? 1.0f : 0.0f);
}

//==============================================================================
void LeadVerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LeadVerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram.store (juce::jlimit (0, numFactoryPresets - 1, (int) state.getProperty ("program", 0)));
            uiScale.store (juce::jlimit (0.75f, 2.5f, (float) state.getProperty ("uiScale", 1.0f)));
            apvts.replaceState (state);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* LeadVerbAudioProcessor::createEditor()
{
    return new LeadVerbAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LeadVerbAudioProcessor();
}
