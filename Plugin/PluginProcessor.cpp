#include "PluginProcessor.h"
#include "SpireLists.h"
#include "PluginEditor.h"
#include "VirusCLayout.h"
#include "Tables/Tables.h"
#include <cstring>

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
        return ms < 10.0 ? String (ms, 1) + " ms" : ms < 1000.0 ? String (roundToInt (ms)) + " ms" : String (ms / 1000.0, 2) + " s";
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
    choiceP (gid::arpMode, "Arp (Step Sequencer)", { "Off", "On" }, 0);
    boolP (gid::delayOn, "Delay On", false);
    boolP (gid::reverbOn, "Reverb On", false);
    intP (gid::volume, "Master Volume", 0, 127, 100);
    auto hzText = [] (int v) { return v == 0 ? String ("Off") : String (v) + " Hz"; };
    intP (gid::monoHz, "Mono (below)", 0, 200, 0, hzText);
    boolP (gid::distOn, "Distortion On", false);
    intP (gid::lockHz, "Phase Lock (below)", 0, 200, 0, hzText);
    for (int o = 0; o < 4; ++o) boolP (gid::oscOn[o], ("Osc " + String (o + 1) + " On").toRawUTF8(), true);
    for (int o = 0; o < 3; ++o) boolP (gid::virusRetrig[o], ("Virus Osc " + String (o + 1) + " Retrig").toRawUTF8(), false);
    for (int o = 0; o < 2; ++o)
        choiceP (gid::virusOscModel[o], ("Virus Osc " + String (o + 1) + " Model").toRawUTF8(),
                 { "Classic", "HyperSaw", "Wavetable", "Wave PWM", "Grain Simple", "Grain Complex", "Formant Simple", "Formant Complex" }, 0);

    // output EQ (after the delay and reverb)
    boolP (gid::eqOn, "EQ On", false);
    {
        auto fText = [] (int v) { const double f = VCCloneAudioProcessor::eqFreqHz (v); return f < 1000.0 ? String (roundToInt (f)) + " Hz" : String (f / 1000.0, f < 10000.0 ? 1 : 0) + " kHz"; };
        auto qText = [] (int v) { return String (VCCloneAudioProcessor::eqQValue (v), 2); };
        auto gText = [] (int v) { return (v > 0 ? "+" : "") + String (v / 10.0, 1) + " dB"; };
        const char* names[3] = { "EQ Low", "EQ Mid", "EQ High" };
        const int fDef[3] = { 34, 64, 101 };    // ~100 Hz, ~1.1 kHz, ~8 kHz
        for (int b = 0; b < 3; ++b)
        {
            intP (gid::eqFreq[b], (String (names[b]) + " Freq").toRawUTF8(), 0, 127, fDef[b], fText);
            intP (gid::eqQ[b], (String (names[b]) + " Q").toRawUTF8(), 0, 100, 35, qText);
            intP (gid::eqGain[b], (String (names[b]) + " Gain").toRawUTF8(), -180, 180, 0, gText);
        }
    }

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
    {
        vendor::DelaySettings d;          // dotted 1/8, clearly audible when switched on
        d.clockBeats = 0.75; d.send = 0.45; d.feedback = 0.45;
        vendor::applyDelay (*delay, d);
        vendor::ReverbSettings r;         // a medium hall
        r.mode = "Hall"; r.decay = 0.55; r.dryWet = 0.3;
        vendor::applyReverb (*reverb, r);
    }
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
    heldNow.reserve (128);
    { const juce::SpinLock::ScopedLockType l (heldLock); lastHeld.reserve (128); }
    sampleRate = sr;
    inst.prepare (sr, block);
    stepSeq.prepare (sr);
    for (auto* p : { delay.get(), reverb.get() })
    {
        p->setRateAndBufferSizeDetails (sr, block);
        p->prepareToPlay (sr, block);
    }
    scratch.setSize (2, block);
    lastLatency = -1;
    eq.valid = false;
    for (auto& band : eq.f) for (auto& fl : band) { fl.coefficients = new dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0); fl.reset(); }
}

void VCCloneAudioProcessor::processEq (AudioBuffer<float>& buffer)
{
    // low shelf, mid bell, high shelf; coefficients only change when a value moves
    float v[9];
    for (int b = 0; b < 3; ++b)
    {
        v[b * 3] = apvts.getRawParameterValue (gid::eqFreq[b])->load();
        v[b * 3 + 1] = apvts.getRawParameterValue (gid::eqQ[b])->load();
        v[b * 3 + 2] = apvts.getRawParameterValue (gid::eqGain[b])->load();
    }
    if (! eq.valid || std::memcmp (v, eq.last, sizeof (v)) != 0)
    {
        const double sr = sampleRate > 0 ? sampleRate : 44100.0;
        for (int b = 0; b < 3; ++b)
        {
            const double f = jlimit (20.0, sr * 0.45, eqFreqHz (roundToInt (v[b * 3])));
            const double q = eqQValue (roundToInt (v[b * 3 + 1]));
            const float gain = Decibels::decibelsToGain ((float) (v[b * 3 + 2] / 10.0f));
            auto c = b == 0 ? dsp::IIR::Coefficients<float>::makeLowShelf (sr, (float) f, (float) q, gain)
                   : b == 1 ? dsp::IIR::Coefficients<float>::makePeakFilter (sr, (float) f, (float) q, gain)
                            : dsp::IIR::Coefficients<float>::makeHighShelf (sr, (float) f, (float) q, gain);
            for (auto& fl : eq.f[b]) *fl.coefficients = *c;
        }
        if (! eq.valid) for (auto& band : eq.f) for (auto& fl : band) fl.reset();
        std::memcpy (eq.last, v, sizeof (v));
        eq.valid = true;
    }
    const int n = buffer.getNumSamples();
    for (int ch = 0; ch < jmin (2, buffer.getNumChannels()); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int b = 0; b < 3; ++b)
        {
            if (std::abs (v[b * 3 + 2]) < 0.5f) continue;     // 0 dB band: skip
            auto& fl = eq.f[b][ch];
            for (int i = 0; i < n; ++i) d[i] = fl.processSample (d[i]);
        }
    }
}

void VCCloneAudioProcessor::reset()
{
    inst.reset();
    stepSeq.reset();
    for (auto* p : { delay.get(), reverb.get() }) p->reset();
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
    g.saturationCurve = raw (gid::distOn) > 0.5f ? roundToInt (raw (gid::satCurve)) : 0;
    g.saturationDrive = raw (gid::satDrive) / 127.0;
    g.saturationMix = raw (gid::satMix) / 127.0;
    g.volume = std::pow (raw (gid::volume) / 100.0, 2.0);
    g.monoHz = raw (gid::monoHz);
    g.lockHz = raw (gid::lockHz);
    for (int o = 0; o < 4; ++o) g.oscOn[(size_t) o] = raw (gid::oscOn[o]) > 0.5f;
    for (int o = 0; o < 3; ++o) g.virusRetrig[(size_t) o] = raw (gid::virusRetrig[o]) > 0.5f;
    for (int o = 0; o < 2; ++o) g.virusOscModel[(size_t) o] = roundToInt (raw (gid::virusOscModel[o]));
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

    // remember the keys being played (Drag MIDI renders the chord you last held)
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            heldNow.erase (std::remove (heldNow.begin(), heldNow.end(), msg.getNoteNumber()), heldNow.end());
            heldNow.push_back (msg.getNoteNumber());
            const juce::SpinLock::ScopedTryLockType l (heldLock);
            if (l.isLocked()) lastHeld = heldNow;
        }
        else if (msg.isNoteOff())
            heldNow.erase (std::remove (heldNow.begin(), heldNow.end(), msg.getNoteNumber()), heldNow.end());
    }

    // --- arp section ------------------------------------------------------------------
    const int arpMode = roundToInt (apvts.getRawParameterValue (gid::arpMode)->load());
    if (arpMode == 1)
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
    if (apvts.getRawParameterValue (gid::eqOn)->load() > 0.5f) processEq (buffer);
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
    setParamValue (gid::arpMode, am != 0 ? 1.0f : 0.0f);

    patchName = p.name();
    ++bankVersion;
}

void VCCloneAudioProcessor::loadAnyPatch (const vc::AnyPatch& a)
{
    switch (a.type)
    {
        case vc::PatchType::Virus:
        {
            setMode (vc::SynthMode::Virus);
            loadPatch (a.virus);
            // Virus TI: oscillator models from the TI page (Classic, HyperSaw, Wavetable ...); Virus C: Classic
            const bool ti = a.virus.extra.size() >= 257;
            setParamValue (gid::virusOscModel[0], ti ? (float) juce::jlimit (0, 7, (int) a.virus.extra[1 + 30]) : 0.0f);
            setParamValue (gid::virusOscModel[1], ti ? (float) juce::jlimit (0, 7, (int) a.virus.extra[1 + 35]) : 0.0f);
            break;
        }

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
            setParamValue (gid::arpMode, st.arpOn ? 1.0f : 0.0f);
            setMode (vc::SynthMode::Spire);
            patchName = a.name;
            break;
        }

        case vc::PatchType::Sylenth:
        {
            sylenthRaw = a.sylenth.values;
            vc::PatchSteps st;
            const auto values = vc::SylenthEngine::fromSylenthPreset (a.sylenth.values, &st);
            for (size_t i = 0; i < values.size(); ++i) sylenthValues[i].store (values[i]);
            sylenthDirty = true;
            vc::AdsrTimes amp, flt;
            vc::SylenthEngine::extractEnvelopes (values, amp, flt);
            setGlobalEnvelopes (amp, flt);
            if (st.present)
            {
                std::vector<vendor::StepSequencer::Step> steps;
                for (auto& s : st.steps) steps.push_back ({ s.transpose, s.velocity, s.tie });
                stepSeq.loadPatchSteps (steps, st.arpOn, st.mode, st.octaves, st.rateIndex, st.rateType,
                                        (float) (st.gate * 100.0), (float) (st.swing * 100.0), st.velocityMode, st.length);
            }
            setParamValue (gid::arpMode, st.arpOn ? 1.0f : 0.0f);
            setMode (vc::SynthMode::Sylenth);
            patchName = a.name;
            break;
        }
    }
    applyPatchFx (a);
    ++bankVersion;
}

//==============================================================================
// The patch's own delay / reverb, set onto the global Virus Delay and LeadVerb (closest match)
namespace
{
    double parseDivisionBeats (juce::String t)       // "1/8D", "3/16", "1/4T" -> quarter-note beats
    {
        t = t.trim().toUpperCase();
        const bool dotted = t.endsWithChar ('D') || t.endsWithChar ('.'), triplet = t.endsWithChar ('T');
        if (dotted || triplet) t = t.dropLastCharacters (1).trim();
        const double num = t.upToFirstOccurrenceOf ("/", false, false).getDoubleValue();
        const double den = t.fromFirstOccurrenceOf ("/", false, false).getDoubleValue();
        if (num <= 0 || den <= 0) return 0.0;
        return 4.0 * num / den * (dotted ? 1.5 : triplet ? 2.0 / 3.0 : 1.0);
    }
}

void VCCloneAudioProcessor::applyPatchFx (const vc::AnyPatch& a)
{
    vendor::DelaySettings d;
    vendor::ReverbSettings r;
    bool delayOn = false, reverbOn = false;

    if (a.type == vc::PatchType::Virus)
    {
        const auto& raw = a.virus.raw;
        const bool ti = a.virus.extra.size() >= 257;
        auto ext = [&] (int i) { return ti ? (int) a.virus.extra[(size_t) (1 + i)] : 0; };   // TI page 6E
        const int mode = raw[112], send = raw[113];
        const int pingPongTI[] = { 2, 3, 4, 5 };
        static const char* ratios[] = { "2:1", "4:3", "4:1", "8:7" };
        static const char* patterns[] = { "1+1", "2+1", "3+1", "4+1", "5+1", "2+3", "2+5", "3+2", "3+3", "3+4", "3+5", "4+3", "4+5", "5+2", "5+3", "5+4", "5+5" };
        (void) pingPongTI;
        juce::String type;
        bool isReverbC = false;
        if (ti)
        {
            if (mode == 1) type = "Simple Delay";
            else if (mode >= 2 && mode <= 5) type = juce::String ("Ping Pong ") + ratios[mode - 2];
            else if (mode >= 6 && mode <= 22) type = juce::String ("Pattern ") + patterns[mode - 6];
        }
        else
        {
            if (mode == 1) type = "Simple Delay";
            else if (mode >= 2 && mode <= 4) isReverbC = true;
            else if (mode >= 5 && mode <= 8) type = juce::String ("Ping Pong ") + ratios[mode - 5];
            else if (mode >= 9 && mode <= 25) type = juce::String ("Pattern ") + patterns[mode - 9];
            else if (mode == 26) type = "Pattern 1+1";
        }
        static const double clockBeats[] = { 0, 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 3.0 / 16, 3.0 / 8, 3.0 / 4, 1.5,
                                             1.0 / 6, 1.0 / 3, 2.0 / 3, 4.0 / 3, 8.0 / 3, 3.0 };
        if (type.isNotEmpty() && send > 0)
        {
            delayOn = true;
            const int dtype = ti ? ext (10) : 0;            // TI: Classic, Tape Clocked, Tape Free, Tape Doppler
            d.kind = dtype == 1 ? vendor::DelaySettings::Kind::TapeClocked : dtype == 2 ? vendor::DelaySettings::Kind::TapeFree
                   : dtype == 3 ? vendor::DelaySettings::Kind::TapeDoppler : vendor::DelaySettings::Kind::Classic;
            d.classicType = type;
            d.send = send / 127.0;
            d.timeMs = juce::jmax (1.0, raw[114] * 5.46);   // Virus: 0 .. ~693 ms
            d.clockBeats = clockBeats[juce::jlimit (0, 16, (int) raw[148])];
            d.feedback = raw[115] / 127.0;
            d.rate = raw[116] / 127.0;
            d.depth = raw[117] / 127.0;
            d.shape = juce::jlimit (0, 4, (int) raw[118]);
            d.color = raw[119] / 127.0;
        }
        if (ti)
        {
            // TI reverb: mode, send, type (Ambience, Small Room, Large Room, Hall), time, damping, colour, predelay
            if (ext (1) > 0 && ext (2) > 0)
            {
                reverbOn = true;
                static const char* types[] = { "Ambience", "Plate1", "Plate2", "Hall" };
                r.mode = types[juce::jlimit (0, 3, ext (3))];
                r.dryWet = ext (2) / 127.0;
                r.decay = ext (4) / 127.0;
                r.damp = ext (5) / 127.0;
                r.color = ext (6) / 127.0;
                r.predelayMs = ext (7) * 500.0 / 92.0;
            }
        }
        else if (isReverbC && send > 0)
        {
            // Virus C: the delay/reverb section in reverb mode (decay, damping, colour, predelay = delay time)
            reverbOn = true;
            r.mode = raw[116] < 40 ? "Plate1" : raw[116] < 90 ? "Plate2" : "Hall";
            r.dryWet = send / 127.0;
            r.decay = raw[116] / 127.0;
            r.damp = raw[118] / 127.0;
            r.color = raw[119] / 127.0;
            r.predelayMs = raw[114] * 500.0 / 127.0;
        }
    }
    else if (a.type == vc::PatchType::Sylenth)
    {
        const auto& v = a.sylenth.values;
        auto sv = [&] (int i) { return i < (int) v.size() ? (double) v[(size_t) i] : 0.0; };
        static const char* times[33] = { "1/64", "1/32T", "1/32T", "1/64D", "1/64D", "1/32", "1/32", "1/16T", "1/16T", "1/16T", "1/32D",
                                         "1/32D", "1/16", "1/16", "1/8T", "1/8T", "1/16D", "1/16D", "1/16D", "1/8", "1/8", "1/4T", "1/4T",
                                         "1/8D", "1/8D", "1/4", "1/4", "1/2T", "1/2T", "1/2T", "1/4D", "1/4D", "1/2" };
        auto beats = [&] (double x) { return parseDivisionBeats (times[juce::jlimit (0, 32, (int) std::lround (x * 32.0))]); };
        if (sv (220) > 0.5 && sv (25) > 0.0)
        {
            delayOn = true;
            d.kind = vendor::DelaySettings::Kind::Spire;
            d.spireSync = true;
            d.leftBeats = beats (sv (32));
            d.rightBeats = beats (sv (33));
            d.feedback = sv (26);
            d.pingPong = sv (29) > 0.5;
            d.wide = sv (34);
            d.dryWet = sv (25);
            d.color = 0.5 + 0.5 * (sv (28) - sv (27)) * 0.5;   // low / high cut as a tilt
        }
        if (sv (224) > 0.5 && sv (143) > 0.0)
        {
            reverbOn = true;
            r.mode = sv (145) > 0.7 ? "Hall" : sv (145) > 0.35 ? "Plate2" : "Plate1";
            r.decay = sv (145);
            r.damp = sv (142);
            r.predelayMs = sv (144) * 200.0;
            r.wide = sv (146) * 0.5;
            r.dryWet = sv (143);
        }
    }
    else if (a.type == vc::PatchType::Spire)
    {
        auto sp = [&] (const char* key, double def) { auto it = a.spire.find (key); return it != a.spire.end() ? (double) it->second : def; };
        auto flag = [] (double x) { return (int) (x * 1.99) != 0; };
        auto listIdx = [] (double x) { return (int) (x * 999.99); };
        if (! flag (sp ("del_mute", 1.0)) && sp ("del_wet", 0.0) > 0.0)
        {
            delayOn = true;
            d.kind = vendor::DelaySettings::Kind::Spire;
            d.spireSync = flag (sp ("del_sync", 1.0));
            const auto& names = vc::spire::lists::arpSpeeds;
            auto divBeats = [&] (double x) { return parseDivisionBeats (names[juce::jlimit (0, vc::spire::lists::arpSpeedsCount - 1, listIdx (x))]); };
            d.leftBeats = divBeats (sp ("del_stimeL", 0.010));
            d.rightBeats = divBeats (sp ("del_stimeR", 0.010));
            d.leftMs = std::pow (2000.0, sp ("del_timeL", 0.5));
            d.rightMs = std::pow (2000.0, sp ("del_timeR", 0.5));
            d.feedback = sp ("del_fback", 0.4);
            d.pingPong = flag (sp ("del_pingpong", 0.0));
            d.wide = sp ("del_wide", 1.0);
            d.color = sp ("del_color", 0.5);
            d.modulate = sp ("del_modulate", 0.0);
            d.dryWet = sp ("del_wet", 0.3);
        }
        if (! flag (sp ("rev_mute", 1.0)) && sp ("rev_wet", 0.0) > 0.0)
        {
            reverbOn = true;
            static const char* modes[] = { "Plate1", "Plate2", "Hall", "Sparkles", "Ambience", "Dmension", "Aura" };
            r.mode = modes[juce::jlimit (0, 6, listIdx (sp ("rev_mode", 0.0)))];
            r.decay = sp ("rev_decay", 0.6357);
            r.damp = sp ("rev_damp", 0.15);
            r.color = sp ("rev_color", 0.65);
            r.wide = sp ("rev_wide", 0.5);
            r.dryWet = sp ("rev_wet", 0.3);
            r.predelayKnob = sp ("rev_pretime", 0.2) * 1000.0;
        }
    }

    if (delayOn) vendor::applyDelay (*delay, d);
    if (reverbOn) vendor::applyReverb (*reverb, r);
    setParamValue (gid::delayOn, delayOn ? 1.0f : 0.0f);
    setParamValue (gid::reverbOn, reverbOn ? 1.0f : 0.0f);
}

StringArray VCCloneAudioProcessor::getPatchNotes() const
{
    StringArray s;
    if (getMode() == vc::SynthMode::Sylenth && ! sylenthRaw.empty())
        s.add ("Sylenth preset: oscillators, filters, envelopes, LFOs, mod slots and arp steps are mapped. Sylenth's own "
               "effects are replaced by the global FX; the envelope/cutoff curves are not yet measured against Sylenth.");
    if (getMode() == vc::SynthMode::Spire)
        s.add ("Spire: all 230 modulation targets and 33 sources are named (from Spire via vc_probe). Oscillator, filter, "
               "LFO and volume targets are applied; envelope-time, FX and arp targets are not modelled yet.");
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
    bankFile = file;
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
    // the format of the synth mode that is loaded
    switch (getMode())
    {
        case vc::SynthMode::Sylenth:
        {
            std::array<float, vc::SylenthEngine::kNumParams> a {};
            for (size_t i = 0; i < a.size(); ++i) a[i] = sylenthValues[i].load();
            const auto bytes = vc::writeSylenthFxp (vc::SylenthEngine::toSylenthPreset (a, sylenthRaw), patchName.toStdString());
            return ! bytes.empty() && vc::writeFile (f.getFullPathName().toStdString(), bytes);
        }
        case vc::SynthMode::Spire:
        {
            std::vector<std::pair<std::string, float>> values;
            for (int i = 0; i < vc::spire::kNumParams; ++i)
                values.push_back ({ String (vc::spire::kParams[i].key).replace ("(inferred)", "").toStdString(), spireValues[(size_t) i].load() });
            return f.replaceWithText (String::fromUTF8 (vc::writeSpirePreset (values, patchName.toStdString()).c_str()));
        }
        case vc::SynthMode::Virus:
        default:
        {
            auto p = currentPatch();
            const auto bytes = f.hasFileExtension ("mid") ? vc::makeSmf ({ p }, false) : vc::makeSingleDump (p, 0, 0);
            return vc::writeFile (f.getFullPathName().toStdString(), bytes);
        }
    }
}

bool VCCloneAudioProcessor::exportBank (const File& f) const
{
    if (getMode() == vc::SynthMode::Sylenth)
    {
        std::vector<std::vector<float>> presets;
        std::vector<std::string> names;
        for (auto& a : bank)
            if (a.type == vc::PatchType::Sylenth) { presets.push_back (a.sylenth.values); names.push_back (a.name); }
        if (presets.empty()) return exportCurrent (f.withFileExtension ("fxp"));
        const auto bytes = vc::writeSylenthFile (presets, names, true);
        return ! bytes.empty() && vc::writeFile (f.getFullPathName().toStdString(), bytes);
    }
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
