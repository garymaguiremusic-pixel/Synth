#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "VirusCLayout.h"
#include "Tables/Tables.h"

using namespace juce;
using namespace vc::layout::off;

//==============================================================================
String VCCloneAudioProcessor::paramID (int offset) { return String (vc::layout::kParams[offset].id); }

bool VCCloneAudioProcessor::isHostParameter (int o)
{
    const auto& d = vc::layout::kParams[o];
    if (! d.isPublic) return false;
    if (o == Version || (o >= 240 && o <= 249)) return false;
    return d.max > d.min;
}

double VCCloneAudioProcessor::envKnobToMs (double v)
{
    if (v <= 0.0) return 0.0;
    return 0.5 * std::pow (60000.0 / 0.5, std::clamp (v, 0.0, 127.0) / 127.0);
}

double VCCloneAudioProcessor::msToEnvKnob (double ms)
{
    if (ms <= 0.5) return ms <= 0.01 ? 0.0 : 1.0;
    return std::clamp (127.0 * std::log (ms / 0.5) / std::log (60000.0 / 0.5), 0.0, 127.0);
}

namespace
{
    String envText (int v, bool sync)
    {
        if (sync) return vc::SyncDivisions::name (roundToInt (v / 127.0 * (vc::SyncDivisions::kCount - 1)));
        const double ms = VCCloneAudioProcessor::envKnobToMs (v);
        return ms < 1000.0 ? String (ms, ms < 10 ? 1 : 0) + " ms" : String (ms / 1000.0, 2) + " s";
    }
    int syncIndex (float v) { return roundToInt (v / 127.0f * (vc::SyncDivisions::kCount - 1)); }
}

AudioProcessorValueTreeState::ParameterLayout VCCloneAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // global section
    auto intP = [&] (const char* id, const char* name, int lo, int hi, int def, std::function<String (int)> text = {})
    {
        auto a = AudioParameterIntAttributes();
        if (text) a = a.withStringFromValueFunction ([text] (int v, int) { return text (v); });
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def, a));
    };
    auto choiceP = [&] (const char* id, const char* name, StringArray items, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto boolP = [&] (const char* id, const char* name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto signedText = [] (int v) { return v == 64 ? String ("0") : (v > 64 ? "+" : "") + String (v - 64); };

    choiceP (gid::mode, "Synth Mode", { "Virus", "Sylenth", "Spire" }, 0);
    boolP (gid::useGlobalEnv, "Global ADSR", true);
    intP (gid::ampA, "Amp Attack", 0, 127, 0, [] (int v) { return envText (v, false); });
    intP (gid::ampD, "Amp Decay", 0, 127, 70, [] (int v) { return envText (v, false); });
    intP (gid::ampS, "Amp Sustain", 0, 127, 127);
    intP (gid::ampR, "Amp Release", 0, 127, 50, [] (int v) { return envText (v, false); });
    boolP (gid::ampSync, "Amp ADSR Sync", false);
    intP (gid::fltA, "Filter Attack", 0, 127, 0, [] (int v) { return envText (v, false); });
    intP (gid::fltD, "Filter Decay", 0, 127, 70, [] (int v) { return envText (v, false); });
    intP (gid::fltS, "Filter Sustain", 0, 127, 0);
    intP (gid::fltR, "Filter Release", 0, 127, 50, [] (int v) { return envText (v, false); });
    boolP (gid::fltSync, "Filter ADSR Sync", false);
    choiceP (gid::filterMode, "Global Filter", { "Off", "LowPass", "HighPass", "BandPass", "BandStop", "Analog 1P", "Analog 2P", "Analog 3P", "Analog 4P" }, 0);
    intP (gid::cutoff, "Global Cutoff", 0, 127, 127);
    intP (gid::reso, "Global Resonance", 0, 127, 0);
    intP (gid::envAmt, "Global Env Amount", 0, 127, 64, signedText);
    intP (gid::keytrack, "Global Keytrack", 0, 127, 64, signedText);
    intP (gid::velocity, "Global Env Velocity", 0, 127, 64, signedText);
    {
        StringArray curves;
        if (auto* vl = vc::layout::findValueList ("satCurve")) for (int i = 0; i < vl->count; ++i) curves.add (vl->items[i]);
        choiceP (gid::satCurve, "Saturation", curves, 0);
    }
    intP (gid::satDrive, "Saturation Drive", 0, 127, 40);
    intP (gid::satMix, "Saturation Mix", 0, 127, 127);
    choiceP (gid::arpMode, "Arp Section", { "Off", "Arp (MelodyMagic)", "Step Seq (ArpSampler)" }, 0);
    boolP (gid::delayOn, "Delay On", false);
    boolP (gid::reverbOn, "Reverb On", false);
    intP (gid::volume, "Master Volume", 0, 127, 100);

    // Virus patch bytes
    const auto init = vc::makeInitPatch();
    for (int o = 0; o < 256; ++o)
    {
        if (! isHostParameter (o)) continue;
        const auto& d = vc::layout::kParams[o];
        const int def = std::clamp ((int) init.raw[(size_t) o], d.min, d.max);
        auto attrs = AudioParameterIntAttributes()
                         .withStringFromValueFunction ([o] (int v, int) { return String (vc::valueToText (o, v)); })
                         .withValueFromStringFunction ([o] (const String& text)
                         {
                             const auto& dd = vc::layout::kParams[o];
                             for (int v = dd.min; v <= dd.max; ++v)
                                 if (text.trim().equalsIgnoreCase (String (vc::valueToText (o, v)))) return v;
                             return text.getIntValue();
                         });
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { paramID (o), 1 }, String (d.name).trim(), d.min, d.max, def, attrs));
    }
    return layout;
}

VCCloneAudioProcessor::VCCloneAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VCClone", createLayout())
{
    for (int o = 0; o < 256; ++o)
        params[(size_t) o] = isHostParameter (o) ? apvts.getParameter (paramID (o)) : nullptr;

    const auto init = vc::makeInitPatch();
    patchRaw = init.raw;
    pendingPatch = init.raw;
    patchPending = true;
    lastPushed.fill (-1);

    const auto syl = vc::SylenthEngine::initParams();
    for (size_t i = 0; i < syl.size(); ++i) sylenthValues[i].store (syl[i]);
    const auto sp = vc::SpireEngine::initParams();
    for (size_t i = 0; i < sp.size(); ++i) spireValues[i].store (sp[i]);

    delay = vendor::createVirusDelay();
    reverb = vendor::createLeadVerb();
    melody = vendor::createMelodyMagic();
    patchName = "Init";
}

VCCloneAudioProcessor::~VCCloneAudioProcessor() { cancelPendingUpdate(); }

bool VCCloneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void VCCloneAudioProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    inst.prepare (sr, block);
    stepSeq.prepare (sr);
    for (auto* p : { delay.get(), reverb.get(), melody.get() })
    {
        p->setRateAndBufferSizeDetails (sr, block);
        p->prepareToPlay (sr, block);
    }
    scratch.setSize (2, block);
    lastLatency = -1;
}

void VCCloneAudioProcessor::reset()
{
    inst.reset();
    stepSeq.reset();
    for (auto* p : { delay.get(), reverb.get(), melody.get() }) p->reset();
}

vc::SynthMode VCCloneAudioProcessor::getMode() const
{
    return (vc::SynthMode) roundToInt (apvts.getRawParameterValue (gid::mode)->load());
}

void VCCloneAudioProcessor::setParamValue (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void VCCloneAudioProcessor::setMode (vc::SynthMode m) { setParamValue (gid::mode, (float) (int) m); }

//==============================================================================
void VCCloneAudioProcessor::handleSysex (const uint8_t* d, int size)
{
    if (size < 8 || d[0] != 0xF0 || d[1] != 0x00 || d[2] != 0x20 || d[3] != 0x33 || d[4] != 0x01)
        return;
    const uint8_t cmd = d[6];
    auto& synth = inst.virus().synth();
    if (cmd == 0x10 && size >= 9 + 256 + 2)
    {
        std::array<uint8_t, 256> raw {};
        std::copy (d + 9, d + 9 + 256, raw.begin());
        synth.setPatch (raw.data());
        for (int o = 0; o < 256; ++o)
            lastPushed[(size_t) o] = std::clamp ((int) raw[(size_t) o], vc::layout::kParams[o].min, vc::layout::kParams[o].max);
        int s1, n1, s2, n2;
        ccFifo.prepareToWrite (256, s1, n1, s2, n2);
        if (n1 + n2 == 256)
        {
            for (int i = 0; i < n1; ++i) ccQueue[(size_t) (s1 + i)] = { 1000 + i, raw[(size_t) i] };
            for (int i = 0; i < n2; ++i) ccQueue[(size_t) (s2 + i)] = { 1000 + n1 + i, raw[(size_t) (n1 + i)] };
            ccFifo.finishedWrite (256);
        }
        triggerAsyncUpdate();
    }
    else if ((cmd == 0x70 || cmd == 0x71) && size == 11)
    {
        const int offset = (cmd == 0x71 ? 128 : 0) + d[8];
        synth.setParam (offset, d[9]);
        lastPushed[(size_t) offset] = std::clamp ((int) d[9], vc::layout::kParams[offset].min, vc::layout::kParams[offset].max);
        int s1, n1, s2, n2;
        ccFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 == 1) { ccQueue[(size_t) s1] = { offset, d[9] }; ccFifo.finishedWrite (1); }
        triggerAsyncUpdate();
    }
}

void VCCloneAudioProcessor::updateGlobalParams()
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto& g = inst.params;
    auto env = [&] (vc::GlobalParams::Env& e, const char* a, const char* d, const char* s, const char* r, const char* sync)
    {
        e.sync = raw (sync) > 0.5f;
        e.attackMs = envKnobToMs (raw (a)); e.decayMs = envKnobToMs (raw (d)); e.releaseMs = envKnobToMs (raw (r));
        e.sustain = raw (s) / 127.0;
        e.attackDiv = syncIndex (raw (a)); e.decayDiv = syncIndex (raw (d)); e.releaseDiv = syncIndex (raw (r));
    };
    env (g.amp, gid::ampA, gid::ampD, gid::ampS, gid::ampR, gid::ampSync);
    env (g.filter, gid::fltA, gid::fltD, gid::fltS, gid::fltR, gid::fltSync);
    g.useGlobalEnvelopes = raw (gid::useGlobalEnv) > 0.5f;
    g.filterMode = roundToInt (raw (gid::filterMode));
    g.cutoff = raw (gid::cutoff);
    g.resonance = raw (gid::reso);
    g.envAmount = raw (gid::envAmt) - 64.0;
    g.keytrack = (raw (gid::keytrack) - 64.0) / 64.0;
    g.velocity = (raw (gid::velocity) - 64.0) / 64.0;
    g.saturationCurve = roundToInt (raw (gid::satCurve));
    g.saturationDrive = raw (gid::satDrive) / 127.0;
    g.saturationMix = raw (gid::satMix) / 127.0;
    g.volume = std::pow (raw (gid::volume) / 100.0, 2.0);
}

void VCCloneAudioProcessor::syncModeEngines()
{
    if (sylenthDirty.exchange (false))
    {
        std::array<float, vc::SylenthEngine::kNumParams> a {};
        for (size_t i = 0; i < a.size(); ++i) a[i] = sylenthValues[i].load();
        inst.sylenth().setAllParams (a);
    }
    if (spireDirty.exchange (false))
    {
        std::array<float, vc::spire::kNumParams> a {};
        for (size_t i = 0; i < a.size(); ++i) a[i] = spireValues[i].load();
        inst.spire().setAllParams (a);
    }
}

void VCCloneAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    auto& synth = inst.virus().synth();
    const int numSamples = buffer.getNumSamples();

    if (patchPending.load())
    {
        const SpinLock::ScopedTryLockType lock (patchLock);
        if (lock.isLocked())
        {
            synth.allNotesOff (true);
            synth.setPatch (pendingPatch.data());
            for (int o = 0; o < 256; ++o)
                lastPushed[(size_t) o] = std::clamp ((int) pendingPatch[(size_t) o], vc::layout::kParams[o].min, vc::layout::kParams[o].max);
            patchPending = false;
        }
    }
    for (int o = 0; o < 256; ++o)
    {
        auto* prm = params[(size_t) o];
        if (prm == nullptr) continue;
        const int v = roundToInt (prm->convertFrom0to1 (prm->getValue()));
        if (v != lastPushed[(size_t) o]) { lastPushed[(size_t) o] = v; synth.setParam (o, v); }
    }
    syncModeEngines();

    const auto mode = getMode();
    if (mode != inst.getMode()) inst.setMode (mode);
    updateGlobalParams();

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    currentBpm = bpm;
    inst.setTempo (bpm);

    // --- arp section ------------------------------------------------------------------
    const int arpMode = roundToInt (apvts.getRawParameterValue (gid::arpMode)->load());
    if (arpMode == 1)
    {
        melody->setPlayHead (getPlayHead());
        scratch.setSize (2, numSamples, false, false, true);
        scratch.clear();
        melody->processBlock (scratch, midi);          // MelodyMagic replaces the keys with its notes
    }
    else if (arpMode == 2)
        stepSeq.process (midi, numSamples, getPlayHead(), bpm);

    // --- synth ------------------------------------------------------------------------
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    HeapBlock<float> tmpR;
    if (R == nullptr) { tmpR.allocate ((size_t) numSamples, true); R = tmpR.get(); }

    int pos = 0;
    auto renderTo = [&] (int end) { if (end > pos) { inst.process (L + pos, R + pos, end - pos); pos = end; } };
    for (const auto meta : midi)
    {
        renderTo (std::clamp (meta.samplePosition, 0, numSamples));
        const auto m = meta.getMessage();
        if (m.isNoteOn()) inst.noteOn (m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff()) inst.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel()) inst.pitchBend (m.getPitchWheelValue());
        else if (m.isChannelPressure()) inst.channelPressure (m.getChannelPressureValue());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) inst.allNotesOff (m.isAllSoundOff());
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), val = m.getControllerValue();
            inst.controller (cc, val);
            if (mode == vc::SynthMode::Virus)
                for (int i = 0; i < vc::layout::kControllerMapSize; ++i)
                {
                    const auto& e = vc::layout::kControllerMap[i];
                    if (e.type == vc::layout::CtlType::CC && e.number == cc && params[(size_t) e.offset] != nullptr)
                    {
                        lastPushed[(size_t) e.offset] = std::clamp (val, vc::layout::kParams[e.offset].min, vc::layout::kParams[e.offset].max);
                        int s1, n1, s2, n2;
                        ccFifo.prepareToWrite (1, s1, n1, s2, n2);
                        if (n1 == 1) { ccQueue[(size_t) s1] = { e.offset, val }; ccFifo.finishedWrite (1); }
                        triggerAsyncUpdate();
                        break;
                    }
                }
        }
        else if (m.isSysEx())
        {
            uint8_t buf[600];
            const int n = m.getSysExDataSize();
            if (n + 2 <= (int) sizeof (buf))
            {
                buf[0] = 0xF0;
                std::memcpy (buf + 1, m.getSysExData(), (size_t) n);
                buf[n + 1] = 0xF7;
                handleSysex (buf, n + 2);
            }
        }
    }
    renderTo (numSamples);
    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);

    // --- global FX: Virus Delay, then LeadVerb (only stereo buffers) ----------------------
    if (buffer.getNumChannels() >= 2)
    {
        if (apvts.getRawParameterValue (gid::delayOn)->load() > 0.5f)
        {
            delay->setPlayHead (getPlayHead());
            emptyMidi.clear();
            delay->processBlock (buffer, emptyMidi);
        }
        if (apvts.getRawParameterValue (gid::reverbOn)->load() > 0.5f)
        {
            reverb->setPlayHead (getPlayHead());
            emptyMidi.clear();
            reverb->processBlock (buffer, emptyMidi);
        }
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);

    activeVoices = inst.activeVoiceCount();
    const int lat = inst.latencySamples();
    if (lat != lastLatency) { lastLatency = lat; triggerAsyncUpdate(); }
    midi.clear();
}

void VCCloneAudioProcessor::handleAsyncUpdate()
{
    if (lastLatency >= 0 && getLatencySamples() != lastLatency) setLatencySamples (lastLatency);

    int s1, n1, s2, n2;
    const int ready = ccFifo.getNumReady();
    ccFifo.prepareToRead (ready, s1, n1, s2, n2);
    std::array<uint8_t, 256> fullPatch {};
    bool gotFull = false;
    auto handle = [&] (const std::pair<int, int>& e)
    {
        if (e.first >= 1000) { fullPatch[(size_t) (e.first - 1000)] = (uint8_t) e.second; gotFull |= e.first == 1255; return; }
        if (auto* prm = params[(size_t) e.first]) { prm->setValueNotifyingHost (prm->convertTo0to1 ((float) e.second)); lastTouched = e.first; }
        patchRaw[(size_t) e.first] = (uint8_t) e.second;
    };
    for (int i = 0; i < n1; ++i) handle (ccQueue[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) handle (ccQueue[(size_t) (s2 + i)]);
    ccFifo.finishedRead (n1 + n2);
    if (gotFull) { vc::SinglePatch p; p.raw = fullPatch; loadPatch (p); }
}

//==============================================================================
void VCCloneAudioProcessor::applyRawToParameters (const std::array<uint8_t, 256>& raw)
{
    patchRaw = raw;
    for (int o = 0; o < 256; ++o)
        if (auto* prm = params[(size_t) o])
        {
            const auto& d = vc::layout::kParams[o];
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) std::clamp ((int) raw[(size_t) o], d.min, d.max)));
        }
}

void VCCloneAudioProcessor::setGlobalEnvelopes (const vc::AdsrTimes& amp, const vc::AdsrTimes& filter)
{
    auto ms = [] (double s) { return (float) msToEnvKnob (s * 1000.0); };
    setParamValue (gid::ampSync, 0.0f);
    setParamValue (gid::fltSync, 0.0f);
    setParamValue (gid::ampA, ms (amp.attack)); setParamValue (gid::ampD, ms (amp.decay));
    setParamValue (gid::ampS, (float) (amp.sustain * 127.0)); setParamValue (gid::ampR, ms (amp.release));
    setParamValue (gid::fltA, ms (filter.attack)); setParamValue (gid::fltD, ms (filter.decay));
    setParamValue (gid::fltS, (float) (filter.sustain * 127.0)); setParamValue (gid::fltR, ms (filter.release));
}

void VCCloneAudioProcessor::loadPatch (const vc::SinglePatch& p)
{
    applyRawToParameters (p.raw);
    {
        const SpinLock::ScopedLockType lock (patchLock);
        pendingPatch = p.raw;
        patchPending = true;
    }
    // the patch's envelopes become the global Filter / Amp ADSR (measured Virus time laws)
    vc::AdsrTimes amp, flt;
    amp.attack = vc::tables::attackTimeSec (p.value (Amp_Env_Attack));
    amp.decay = vc::tables::decayTimeSec (p.value (Amp_Env_Decay));
    amp.sustain = p.value (Amp_Env_Sustain) / 127.0;
    amp.release = vc::tables::releaseTimeSec (p.value (Amp_Env_Release));
    flt.attack = vc::tables::attackTimeSec (p.value (Filter_Env_Attack));
    flt.decay = vc::tables::decayTimeSec (p.value (Filter_Env_Decay));
    flt.sustain = p.value (Filter_Env_Sustain) / 127.0;
    flt.release = vc::tables::releaseTimeSec (p.value (Filter_Env_Release));
    setGlobalEnvelopes (amp, flt);

    // the Virus arp settings go to the global step sequencer (Virus arp patterns 2..64 are not known: straight steps)
    const int am = p.value (Arp_Mode);
    static constexpr int kModeMap[7] = { 0, 0, 1, 2, 7, 6, 9 };   // Off Up Down Up&Down AsPlayed Random Chord
    const double beats = vc::tables::divisionBeats (std::max (1, p.value (Arp_Clock)));
    int rate = 3, type = 0;
    {
        static const struct { double b; int idx, type; } rates[] = { { 2, 0, 0 }, { 1, 1, 0 }, { 0.5, 2, 0 }, { 0.25, 3, 0 }, { 0.125, 4, 0 },
            { 4.0 / 3, 0, 1 }, { 2.0 / 3, 1, 1 }, { 1.0 / 3, 2, 1 }, { 1.0 / 6, 3, 1 }, { 1.0 / 12, 4, 1 }, { 3, 0, 2 }, { 1.5, 1, 2 }, { 0.75, 2, 2 }, { 0.375, 3, 2 }, { 0.1875, 4, 2 } };
        double best = 1e9;
        for (auto& r : rates) if (std::abs (std::log (r.b / beats)) < best) { best = std::abs (std::log (r.b / beats)); rate = r.idx; type = r.type; }
    }
    stepSeq.loadPlainArp (am != 0, kModeMap[std::clamp (am, 0, 6)], std::clamp (p.value (Arp_Octave_Range) + 1, 1, 4), rate, type);
    setParamValue (gid::arpMode, am != 0 ? 2.0f : 0.0f);

    patchName = p.name();
    ++bankVersion;
}

void VCCloneAudioProcessor::loadAnyPatch (const vc::AnyPatch& a)
{
    switch (a.type)
    {
        case vc::PatchType::Virus:
            setMode (vc::SynthMode::Virus);
            loadPatch (a.virus);
            break;

        case vc::PatchType::Spire:
        {
            auto values = vc::SpireEngine::initParams();
            for (auto& [key, v] : a.spire)
            {
                int idx = key.size() > 1 && key[0] == '#' ? std::atoi (key.c_str() + 1) : vc::spire::findParam (key);
                if (idx >= 0 && idx < vc::spire::kNumParams) values[(size_t) idx] = std::clamp (v, 0.0f, 1.0f);
            }
            for (size_t i = 0; i < values.size(); ++i) spireValues[i].store (values[i]);
            spireDirty = true;
            vc::AdsrTimes amp, flt;
            vc::SpireEngine::extractEnvelopes (values, amp, flt);
            setGlobalEnvelopes (amp, flt);
            const auto st = vc::SpireEngine::extractSteps (values);
            std::vector<vendor::StepSequencer::Step> steps;
            for (auto& s : st.steps) steps.push_back ({ s.transpose, s.velocity, s.tie });
            stepSeq.loadPatchSteps (steps, st.arpOn, st.mode, st.octaves, st.rateIndex, st.rateType,
                                    (float) (st.gate * 100.0), (float) (st.swing * 100.0), st.velocityMode, st.length);
            setParamValue (gid::arpMode, st.arpOn ? 2.0f : 0.0f);
            setMode (vc::SynthMode::Spire);
            patchName = a.name;
            break;
        }

        case vc::PatchType::Sylenth:
        {
            // Sylenth's parameter order is not known yet (needs the probe): keep the raw values,
            // start from the Sylenth init sound.
            sylenthRaw = a.sylenth.values;
            const auto init = vc::SylenthEngine::initParams();
            for (size_t i = 0; i < init.size(); ++i) sylenthValues[i].store (init[i]);
            sylenthDirty = true;
            vc::AdsrTimes amp, flt;
            vc::SylenthEngine::extractEnvelopes (init, amp, flt);
            setGlobalEnvelopes (amp, flt);
            setParamValue (gid::arpMode, 0.0f);
            setMode (vc::SynthMode::Sylenth);
            patchName = a.name;
            break;
        }
    }
    ++bankVersion;
}

StringArray VCCloneAudioProcessor::getPatchNotes() const
{
    StringArray s;
    if (getMode() == vc::SynthMode::Sylenth && ! sylenthRaw.empty())
        s.add ("Sylenth preset: " + String ((int) sylenthRaw.size()) + " values read; their mapping to the Sylenth controls needs "
               "Sylenth's parameter list (run tools/vc_probe once on your PC). The Sylenth init sound plays meanwhile.");
    if (getMode() == vc::SynthMode::Spire)
        s.add ("Spire: modulation targets are decoded only where Spire's target ID is known (Cutoff 1 so far); "
               "the full list comes from tools/vc_probe.");
    return s;
}

void VCCloneAudioProcessor::loadInitPatch()
{
    switch (getMode())
    {
        case vc::SynthMode::Virus: loadPatch (vc::makeInitPatch()); break;
        case vc::SynthMode::Sylenth:
        {
            const auto init = vc::SylenthEngine::initParams();
            for (size_t i = 0; i < init.size(); ++i) sylenthValues[i].store (init[i]);
            sylenthDirty = true; sylenthRaw.clear(); patchName = "Init";
            break;
        }
        case vc::SynthMode::Spire:
        {
            const auto init = vc::SpireEngine::initParams();
            for (size_t i = 0; i < init.size(); ++i) spireValues[i].store (init[i]);
            spireDirty = true; patchName = "Init";
            break;
        }
    }
    ++bankVersion;
}

vc::SinglePatch VCCloneAudioProcessor::currentPatch() const
{
    vc::SinglePatch p;
    p.raw = patchRaw;
    for (int o = 0; o < 256; ++o)
        if (auto* prm = params[(size_t) o])
        {
            const auto& d = vc::layout::kParams[o];
            const int v = roundToInt (prm->convertFrom0to1 (prm->getValue()));
            if (v != std::clamp ((int) patchRaw[(size_t) o], d.min, d.max)) p.raw[(size_t) o] = (uint8_t) v;
        }
    return p;
}

String VCCloneAudioProcessor::loadBankFile (const File& file)
{
    auto res = vc::importAnyFile (file.getFullPathName().toStdString());
    if (res.patches.empty())
        return res.error.empty() ? String ("No patches found in ") + file.getFileName() : String (res.error);
    bank = std::move (res.patches);
    warnings = std::move (res.warnings);
    bankName = file.getFileName();
    bankFormat = res.formatName;
    currentProgram = 0;
    loadAnyPatch (bank.front());
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
    return {};
}

int VCCloneAudioProcessor::getNumPrograms() { return std::max (1, (int) bank.size()); }

void VCCloneAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= (int) bank.size()) return;
    currentProgram = index;
    loadAnyPatch (bank[(size_t) index]);
}

const String VCCloneAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < (int) bank.size()) return bank[(size_t) index].name;
    return "Init";
}

bool VCCloneAudioProcessor::exportCurrent (const File& f) const
{
    auto p = currentPatch();
    const auto bytes = f.hasFileExtension ("mid") ? vc::makeSmf ({ p }, false) : vc::makeSingleDump (p, 0, 0);
    return vc::writeFile (f.getFullPathName().toStdString(), bytes);
}

bool VCCloneAudioProcessor::exportBank (const File& f) const
{
    std::vector<vc::SinglePatch> virus;
    for (auto& a : bank) if (a.type == vc::PatchType::Virus) virus.push_back (a.virus);
    if (virus.empty()) return exportCurrent (f);
    const auto bytes = f.hasFileExtension ("mid") ? vc::makeSmf (virus) : vc::makeSyx (virus);
    return vc::writeFile (f.getFullPathName().toStdString(), bytes);
}

//==============================================================================
namespace
{
    String floatsToBase64 (const float* d, size_t n) { return Base64::toBase64 (d, n * sizeof (float)); }
    bool floatsFromBase64 (const String& s, float* d, size_t n)
    {
        MemoryOutputStream mo;
        if (! Base64::convertFromBase64 (mo, s) || mo.getDataSize() != n * sizeof (float)) return false;
        std::memcpy (d, mo.getData(), n * sizeof (float));
        return true;
    }
    String processorState (AudioProcessor& p) { MemoryBlock mb; p.getStateInformation (mb); return mb.toBase64Encoding(); }
    void restoreProcessor (AudioProcessor& p, const String& s)
    {
        MemoryBlock mb;
        if (s.isNotEmpty() && mb.fromBase64Encoding (s)) p.setStateInformation (mb.getData(), (int) mb.getSize());
    }
}

void VCCloneAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto p = currentPatch();
    XmlElement xml ("VCCloneState");
    xml.setAttribute ("version", 2);
    xml.setAttribute ("patch", Base64::toBase64 (p.raw.data(), p.raw.size()));
    xml.setAttribute ("patchName", patchName);
    xml.setAttribute ("editorWidth", editorWidth.load());
    std::array<float, vc::SylenthEngine::kNumParams> syl {};
    for (size_t i = 0; i < syl.size(); ++i) syl[i] = sylenthValues[i].load();
    xml.setAttribute ("sylenth", floatsToBase64 (syl.data(), syl.size()));
    if (! sylenthRaw.empty()) xml.setAttribute ("sylenthRaw", floatsToBase64 (sylenthRaw.data(), sylenthRaw.size()));
    std::array<float, vc::spire::kNumParams> sp {};
    for (size_t i = 0; i < sp.size(); ++i) sp[i] = spireValues[i].load();
    xml.setAttribute ("spire", floatsToBase64 (sp.data(), sp.size()));
    xml.setAttribute ("delay", processorState (*delay));
    xml.setAttribute ("reverb", processorState (*reverb));
    xml.setAttribute ("melody", processorState (*melody));
    if (auto seqXml = stepSeq.saveState().createXml()) xml.addChildElement (new XmlElement (*seqXml));
    if (auto g = apvts.copyState().createXml()) xml.addChildElement (g.release());
    copyXmlToBinary (xml, dest);
}

void VCCloneAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("VCCloneState")) return;

    editorWidth = xml->getIntAttribute ("editorWidth", 0);
    if (auto* g = xml->getChildByName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*g));
    for (auto* child : xml->getChildIterator())
        if (! child->hasTagName (apvts.state.getType().toString())) stepSeq.loadState (ValueTree::fromXml (*child));

    std::array<float, vc::SylenthEngine::kNumParams> syl {};
    if (floatsFromBase64 (xml->getStringAttribute ("sylenth"), syl.data(), syl.size()))
        for (size_t i = 0; i < syl.size(); ++i) sylenthValues[i].store (syl[i]);
    std::array<float, vc::spire::kNumParams> sp {};
    if (floatsFromBase64 (xml->getStringAttribute ("spire"), sp.data(), sp.size()))
        for (size_t i = 0; i < sp.size(); ++i) spireValues[i].store (sp[i]);
    sylenthDirty = spireDirty = true;
    restoreProcessor (*delay, xml->getStringAttribute ("delay"));
    restoreProcessor (*reverb, xml->getStringAttribute ("reverb"));
    restoreProcessor (*melody, xml->getStringAttribute ("melody"));

    MemoryOutputStream mo;
    if (Base64::convertFromBase64 (mo, xml->getStringAttribute ("patch")) && mo.getDataSize() == 256)
    {
        std::array<uint8_t, 256> raw {};
        std::memcpy (raw.data(), mo.getData(), 256);
        applyRawToParameters (raw);   // only the Virus bytes; the global section was restored above
        const SpinLock::ScopedLockType lock (patchLock);
        pendingPatch = raw;
        patchPending = true;
    }
    patchName = xml->getStringAttribute ("patchName", "Init");
    ++bankVersion;
}

AudioProcessorEditor* VCCloneAudioProcessor::createEditor() { return new VCCloneAudioProcessorEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VCCloneAudioProcessor(); }
