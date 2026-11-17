// ArpSampler's arp + step sequencer, hosted without its sampler.
// Same renames as VC_AS_RENAMES in cmake/vendor.cmake (StepGrid uses ArpSampler's own look and feel helpers).
#define CustomLookAndFeel as_CustomLookAndFeel
#define VirusColours as_VirusColours
#define VirusKnob as_VirusKnob
#define KnobAttachment as_KnobAttachment
#define LcdDisplay as_LcdDisplay
#define HelpButton as_HelpButton
#define TempoField as_TempoField
#include "../ThirdParty/ArpSampler/Parameters.h"
#include "../ThirdParty/ArpSampler/Arp/ArpEngine.h"
#include "../ThirdParty/ArpSampler/Arp/PatternModel.h"
#include "../ThirdParty/ArpSampler/UI/StepGrid.h"
#include "VendorArp.h"

namespace vendor
{
namespace
{
    /** Minimal processor so ArpSampler's own parameter layout can live in an APVTS. Never processes audio. */
    struct ParamHost : juce::AudioProcessor
    {
        ParamHost() : juce::AudioProcessor (BusesProperties()) {}
        const juce::String getName() const override { return "StepSeq"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };
}

struct StepSequencer::Impl
{
    ParamHost host;
    juce::AudioProcessorValueTreeState apvts { host, nullptr, "StepSeq", as::createParameterLayout() };
    as::arp::PatternExchange exchange;
    as::arp::PatternModel model { exchange };
    as::arp::ArpEngine engine;
    as::arp::EventList events;
    std::array<int, 4096> idToNote {};
    std::atomic<int> currentStep { -1 };
    double sampleRate = 44100.0;
    double freePpq = 0.0;

    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    as::arp::ArpSettings settings() const
    {
        as::arp::ArpSettings s;
        s.enabled = true;     // the global section decides whether the sequencer runs at all
        s.mode = (as::ArpMode) juce::roundToInt (raw (as::ParamIDs::arpMode));
        s.rateIndex = juce::roundToInt (raw (as::ParamIDs::arpRate));
        s.rateType = (as::RateType) juce::roundToInt (raw (as::ParamIDs::arpRateType));
        s.gate = raw (as::ParamIDs::arpGate) * 0.01f;
        s.swing = raw (as::ParamIDs::arpSwing) * 0.01f;
        s.octaves = juce::roundToInt (raw (as::ParamIDs::arpOctave));
        s.velocityMode = (as::VelocityMode) juce::roundToInt (raw (as::ParamIDs::velMode));
        s.retrigger = raw (as::ParamIDs::retrigger) > 0.5f;
        const int length = juce::roundToInt (raw (as::ParamIDs::seqLength)) == 0 ? 16 : 32;
        s.sequenceLength = as::arp::effectiveLength (length, juce::roundToInt (raw (as::ParamIDs::seqEnd)));
        s.scaleKey = juce::roundToInt (raw (as::ParamIDs::scaleKey));
        s.scaleIndex = juce::roundToInt (raw (as::ParamIDs::scale));
        s.followKey = raw (as::ParamIDs::scaleLock) > 0.5f;
        s.quantiseOutput = s.followKey && raw (as::ParamIDs::quantiseOutput) > 0.5f;
        s.autoPlay = raw (as::ParamIDs::autoPlay) > 0.5f;
        s.autoNote = 60 + s.scaleKey;
        return s;
    }

    void set (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
};

StepSequencer::StepSequencer() : impl (std::make_unique<Impl>()) { impl->engine.setRandomSeed (1); }
StepSequencer::~StepSequencer() = default;

void StepSequencer::prepare (double sr) { impl->sampleRate = sr; impl->engine.prepare (sr); impl->engine.setSettings (impl->settings()); }
void StepSequencer::reset() { impl->engine.reset(); impl->freePpq = 0; }
int StepSequencer::getCurrentStep() const { return impl->currentStep.load(); }
juce::AudioProcessorValueTreeState& StepSequencer::state() { return impl->apvts; }

void StepSequencer::process (juce::MidiBuffer& midi, int numSamples, juce::AudioPlayHead* playHead, double fallbackBpm)
{
    auto& d = *impl;
    as::arp::TransportInfo t;
    t.bpm = fallbackBpm;
    if (playHead != nullptr)
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition()) { t.hostPlaying = true; t.ppqAtBlockStart = *ppq; }
            if (pos->getIsLooping())
                if (auto loop = pos->getLoopPoints()) { t.looping = true; t.loopStart = loop->ppqStart; t.loopEnd = loop->ppqEnd; }
        }

    d.engine.setSettings (d.settings());
    d.engine.setPattern (d.exchange.read());
    d.events.clear();
    d.engine.process (midi, numSamples, t, d.events);
    d.currentStep.store (d.engine.getCurrentStep());

    // note events -> MIDI for the synth (keys are consumed; controllers, bend etc. pass through)
    juce::MidiBuffer out;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOnOrOff()) out.addEvent (m, meta.samplePosition);
    }
    for (const auto& e : d.events)
    {
        const int off = juce::jlimit (0, juce::jmax (0, numSamples - 1), e.offset);
        auto& slotNote = d.idToNote[(size_t) (e.id & 4095)];
        switch (e.type)
        {
            case as::arp::NoteEvent::Type::Start:
            {
                int vel = e.accent ? 127 : juce::jlimit (1, 127, e.velocity * e.volume / 127);
                out.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) vel), off);
                slotNote = e.note;
                break;
            }
            case as::arp::NoteEvent::Type::Change:   // tie: the new pitch starts before the old one ends (legato)
                out.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) juce::jlimit (1, 127, e.velocity)), off);
                if (slotNote != e.note) out.addEvent (juce::MidiMessage::noteOff (1, slotNote), off);
                slotNote = e.note;
                break;
            case as::arp::NoteEvent::Type::Stop:
                out.addEvent (juce::MidiMessage::noteOff (1, slotNote > 0 ? slotNote : e.note), off);
                break;
            case as::arp::NoteEvent::Type::LegatoReset:
                break;
        }
    }
    midi.swapWith (out);
}

void StepSequencer::loadPatchSteps (const std::vector<Step>& steps, bool arpOn, int mode, int octaves, int rateIndex,
                                    int rateType, float gatePercent, float swingPercent, int velocityMode, int length)
{
    auto& d = *impl;
    as::arp::Pattern pat = d.model.get();
    for (size_t k = 0; k < pat.steps.size(); ++k)
    {
        auto& s = pat.steps[k];
        s = {};
        if (k < steps.size())
        {
            s.transpose = juce::jlimit (-24, 24, steps[k].transpose);
            s.velocity = juce::jlimit (0, 127, steps[k].velocity);
            s.tie = steps[k].tie;
        }
    }
    pat.scaleSteps = false;
    d.model.set (pat, true);
    d.set (as::ParamIDs::arpOn, arpOn ? 1.0f : 0.0f);
    d.set (as::ParamIDs::arpMode, (float) mode);
    d.set (as::ParamIDs::arpOctave, (float) octaves);
    d.set (as::ParamIDs::arpRate, (float) rateIndex);
    d.set (as::ParamIDs::arpRateType, (float) rateType);
    d.set (as::ParamIDs::arpGate, gatePercent);
    d.set (as::ParamIDs::arpSwing, swingPercent);
    d.set (as::ParamIDs::velMode, (float) velocityMode);
    d.set (as::ParamIDs::seqLength, length > 16 ? 1.0f : 0.0f);
    d.set (as::ParamIDs::seqEnd, length > 0 && length < 16 ? (float) length : 0.0f);
}

void StepSequencer::loadPlainArp (bool arpOn, int mode, int octaves, int rateIndex, int rateType)
{
    loadPatchSteps ({}, arpOn, mode, octaves, rateIndex, rateType, 75.0f, 0.0f, 0, 16);
}

juce::ValueTree StepSequencer::saveState() const
{
    auto tree = impl->apvts.copyState();
    tree.appendChild (impl->model.toValueTree(), nullptr);
    return tree;
}

void StepSequencer::loadState (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    auto copy = t.createCopy();
    auto pat = copy.getChildWithName (as::arp::PatternModel::treeType);
    if (pat.isValid()) { impl->model.fromValueTree (pat); copy.removeChild (pat, nullptr); }
    impl->apvts.replaceState (copy);
}

std::unique_ptr<juce::Component> StepSequencer::createPanel()
{
    return std::make_unique<StepGrid> (impl->model, *impl->apvts.getParameter (as::ParamIDs::seqLength),
                                       *impl->apvts.getParameter (as::ParamIDs::seqEnd));
}
} // namespace vendor
