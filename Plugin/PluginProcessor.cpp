#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "VirusCLayout.h"

using namespace juce;

//==============================================================================
String VCCloneAudioProcessor::paramID (int offset)
{
    return String (vc::layout::kParams[offset].id);
}

bool VCCloneAudioProcessor::isHostParameter (int o)
{
    const auto& d = vc::layout::kParams[o];
    if (! d.isPublic) return false;
    if (o == vc::layout::off::Version || (o >= 240 && o <= 249)) return false;
    return d.max > d.min;
}

AudioProcessorValueTreeState::ParameterLayout VCCloneAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
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
    for (int o = 0; o < 256; ++o)
        lastPushed[(size_t) o] = -1;
}

VCCloneAudioProcessor::~VCCloneAudioProcessor()
{
    cancelPendingUpdate();
}

bool VCCloneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void VCCloneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latencySamples());
}

void VCCloneAudioProcessor::reset()
{
    // host-requested reset (e.g. before an offline render): clean voices, FX tails and LFO phases
    engine.reset();
}

//==============================================================================
void VCCloneAudioProcessor::handleSysex (const uint8_t* d, int size)
{
    if (size < 8 || d[0] != 0xF0 || d[1] != 0x00 || d[2] != 0x20 || d[3] != 0x33 || d[4] != 0x01)
        return;
    const uint8_t cmd = d[6];
    if (cmd == 0x10 && size >= 9 + 256 + 2)
    {
        std::array<uint8_t, 256> raw {};
        std::copy (d + 9, d + 9 + 256, raw.begin());
        engine.synth.setPatch (raw.data());
        for (int o = 0; o < 256; ++o)
            lastPushed[(size_t) o] = std::clamp ((int) raw[(size_t) o], vc::layout::kParams[o].min, vc::layout::kParams[o].max);
        // hand all bytes to the message thread (value -1 marks "full patch follows")
        int start1, size1, start2, size2;
        ccFifo.prepareToWrite (256, start1, size1, start2, size2);
        if (size1 + size2 == 256)
        {
            for (int i = 0; i < size1; ++i) ccQueue[(size_t) (start1 + i)] = { 1000 + i, raw[(size_t) i] };
            for (int i = 0; i < size2; ++i) ccQueue[(size_t) (start2 + i)] = { 1000 + size1 + i, raw[(size_t) (size1 + i)] };
            ccFifo.finishedWrite (256);
        }
        triggerAsyncUpdate();
    }
    else if ((cmd == 0x70 || cmd == 0x71) && size == 11)
    {
        const int offset = (cmd == 0x71 ? 128 : 0) + d[8];
        engine.synth.setParam (offset, d[9]);
        lastPushed[(size_t) offset] = std::clamp ((int) d[9], vc::layout::kParams[offset].min, vc::layout::kParams[offset].max);
        int s1, n1, s2, n2;
        ccFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 == 1) { ccQueue[(size_t) s1] = { offset, d[9] }; ccFifo.finishedWrite (1); }
        triggerAsyncUpdate();
    }
}

void VCCloneAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    auto& synth = engine.synth;

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

    // host parameters -> engine
    for (int o = 0; o < 256; ++o)
    {
        auto* prm = params[(size_t) o];
        if (prm == nullptr) continue;
        const int v = roundToInt (prm->convertFrom0to1 (prm->getValue()));
        if (v != lastPushed[(size_t) o])
        {
            lastPushed[(size_t) o] = v;
            synth.setParam (o, v);
        }
    }

    // tempo
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                synth.setHostTempo (*bpm);

    const int numSamples = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    HeapBlock<float> tmpR;
    if (R == nullptr) { tmpR.allocate ((size_t) numSamples, true); R = tmpR.get(); }

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        if (end > pos) { engine.process (L + pos, R + pos, end - pos); pos = end; }
    };

    for (const auto meta : midi)
    {
        renderTo (std::clamp (meta.samplePosition, 0, numSamples));
        const auto m = meta.getMessage();
        if (m.isNoteOn())                 synth.noteOn (m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff())           synth.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())        synth.pitchBend (m.getPitchWheelValue());
        else if (m.isChannelPressure())   synth.channelPressure (m.getChannelPressureValue());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff (m.isAllSoundOff());
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), val = m.getControllerValue();
            synth.controller (cc, val);
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
            // getSysExData() excludes F0/F7: rebuild the full message
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
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    activeVoices = synth.activeVoiceCount();
}

void VCCloneAudioProcessor::handleAsyncUpdate()
{
    int s1, n1, s2, n2;
    const int ready = ccFifo.getNumReady();
    ccFifo.prepareToRead (ready, s1, n1, s2, n2);
    std::array<uint8_t, 256> fullPatch {};
    bool gotFull = false;
    auto handle = [&] (const std::pair<int, int>& e)
    {
        if (e.first >= 1000)
        {
            fullPatch[(size_t) (e.first - 1000)] = (uint8_t) e.second;
            gotFull |= e.first == 1255;
            return;
        }
        if (auto* prm = params[(size_t) e.first])
        {
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) e.second));
            lastTouched = e.first;
        }
        patchRaw[(size_t) e.first] = (uint8_t) e.second;
    };
    for (int i = 0; i < n1; ++i) handle (ccQueue[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) handle (ccQueue[(size_t) (s2 + i)]);
    ccFifo.finishedRead (n1 + n2);
    if (gotFull)
    {
        vc::SinglePatch p;
        p.raw = fullPatch;
        loadPatch (p);
    }
}

//==============================================================================
void VCCloneAudioProcessor::applyRawToParameters (const std::array<uint8_t, 256>& raw)
{
    patchRaw = raw;
    for (int o = 0; o < 256; ++o)
        if (auto* prm = params[(size_t) o])
        {
            const auto& d = vc::layout::kParams[o];
            const int v = std::clamp ((int) raw[(size_t) o], d.min, d.max);
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
        }
}

void VCCloneAudioProcessor::loadPatch (const vc::SinglePatch& p)
{
    // parameters first, then the full patch: the audio thread ends up with exactly p.raw
    applyRawToParameters (p.raw);
    {
        const SpinLock::ScopedLockType lock (patchLock);
        pendingPatch = p.raw;
        patchPending = true;
    }
    ++bankVersion;
}

void VCCloneAudioProcessor::loadInitPatch() { loadPatch (vc::makeInitPatch()); }

vc::SinglePatch VCCloneAudioProcessor::currentPatch() const
{
    vc::SinglePatch p;
    p.raw = patchRaw;
    for (int o = 0; o < 256; ++o)
        if (auto* prm = params[(size_t) o])
        {
            const auto& d = vc::layout::kParams[o];
            const int v = roundToInt (prm->convertFrom0to1 (prm->getValue()));
            if (v != std::clamp ((int) patchRaw[(size_t) o], d.min, d.max))   // keep raw out-of-range bytes if untouched
                p.raw[(size_t) o] = (uint8_t) v;
        }
    return p;
}

String VCCloneAudioProcessor::loadBankFile (const File& file)
{
    auto res = vc::importFile (file.getFullPathName().toStdString());
    if (res.singles.empty())
        return res.error.empty() ? String ("No Virus singles found in ") + file.getFileName() : String (res.error);

    bank = std::move (res.singles);
    warnings = std::move (res.warnings);
    bankName = file.getFileName();
    currentProgram = 0;
    loadPatch (bank.front());
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
    return {};
}

int VCCloneAudioProcessor::getNumPrograms() { return std::max (1, (int) bank.size()); }

void VCCloneAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= (int) bank.size()) return;
    currentProgram = index;
    loadPatch (bank[(size_t) index]);
}

const String VCCloneAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < (int) bank.size()) return bank[(size_t) index].name();
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
    if (bank.empty()) return exportCurrent (f);
    const auto bytes = f.hasFileExtension ("mid") ? vc::makeSmf (bank) : vc::makeSyx (bank);
    return vc::writeFile (f.getFullPathName().toStdString(), bytes);
}

//==============================================================================
void VCCloneAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto p = currentPatch();
    XmlElement xml ("VCCloneState");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("patch", Base64::toBase64 (p.raw.data(), p.raw.size()));
    xml.setAttribute ("program", currentProgram);
    xml.setAttribute ("bankName", bankName);
    xml.setAttribute ("editorWidth", editorWidth.load());
    if (! bank.empty())
    {
        const auto syx = vc::makeSyx (bank);
        xml.setAttribute ("bank", Base64::toBase64 (syx.data(), syx.size()));
    }
    copyXmlToBinary (xml, dest);
}

void VCCloneAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("VCCloneState")) return;

    editorWidth = xml->getIntAttribute ("editorWidth", 0);
    bankName = xml->getStringAttribute ("bankName");
    if (xml->hasAttribute ("bank"))
    {
        MemoryOutputStream mo;
        if (Base64::convertFromBase64 (mo, xml->getStringAttribute ("bank")))
        {
            auto res = vc::importData ((const uint8_t*) mo.getData(), mo.getDataSize());
            bank = std::move (res.singles);
            warnings = std::move (res.warnings);
        }
    }
    currentProgram = xml->getIntAttribute ("program", 0);

    MemoryOutputStream mo;
    if (Base64::convertFromBase64 (mo, xml->getStringAttribute ("patch")) && mo.getDataSize() == 256)
    {
        vc::SinglePatch p;
        std::memcpy (p.raw.data(), mo.getData(), 256);
        loadPatch (p);
    }
    ++bankVersion;
}

AudioProcessorEditor* VCCloneAudioProcessor::createEditor() { return new VCCloneAudioProcessorEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VCCloneAudioProcessor(); }
