// ArpSampler's arp + step sequencer, hosted without its sampler.
// Same renames as VC_AS_RENAMES in cmake/vendor.cmake (StepGrid uses ArpSampler's own look and feel helpers).
#define CustomLookAndFeel as_CustomLookAndFeel
#define VirusColours as_VirusColours
#define VirusKnob as_VirusKnob
#define KnobAttachment as_KnobAttachment
#define LcdDisplay as_LcdDisplay
#define HelpButton as_HelpButton
#define TempoField as_TempoField
#include "../ThirdParty/ArpSampler/Arp/Randomiser.h"
#include "../ThirdParty/ArpSampler/Export/MidiRenderer.h"
#include "../ThirdParty/ArpSampler/UI/DragMidiHandle.h"
#include "../ThirdParty/ArpSampler/Parameters.h"
#include "../ThirdParty/ArpSampler/Arp/ArpEngine.h"
#include "../ThirdParty/ArpSampler/Arp/PatternModel.h"
#include "../ThirdParty/ArpSampler/UI/StepGrid.h"
#include "../ThirdParty/ArpSampler/Arp/Scales.h"
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

void StepSequencer::randomise()
{
    auto& d = *impl;
    auto v = [&] (const char* id) { return d.raw (id); };
    as::arp::RandomiseSettings r;
    r.density = v (as::ParamIDs::rndDensity) * 0.01f;
    r.range = juce::roundToInt (v (as::ParamIDs::rndRange));
    r.tieProbability = v (as::ParamIDs::rndTieProb) * 0.01f;
    r.velocityMin = juce::roundToInt (v (as::ParamIDs::rndVelMin));
    r.velocityMax = juce::roundToInt (v (as::ParamIDs::rndVelMax));
    r.ratchetProbability = v (as::ParamIDs::rndRatchet) * 0.01f;
    r.accentProbability = v (as::ParamIDs::rndAccent) * 0.01f;
    r.scaleLock = v (as::ParamIDs::scaleLock) > 0.5f;
    r.scaleKey = juce::roundToInt (v (as::ParamIDs::scaleKey));
    r.scaleIndex = juce::roundToInt (v (as::ParamIDs::scale));
    const auto seed = (juce::uint32) juce::Random::getSystemRandom().nextInt (999999999) + 1u;
    d.model.set (as::arp::randomisePattern (d.model.get(), r, seed), true);
}

juce::File StepSequencer::exportMidi (double bpm, const std::vector<int>& heldNotes) const
{
    auto& d = *impl;
    as::exporter::ExportRequest r;
    r.settings = d.settings();
    r.settings.enabled = true;
    r.pattern = d.model.get();
    r.bpm = bpm > 0 ? bpm : 120.0;
    static constexpr int barChoices[] = { 1, 2, 4, 8 };
    r.bars = barChoices[juce::jlimit (0, 3, juce::roundToInt (d.raw (as::ParamIDs::exportBars)))];
    const int root = 60 + r.settings.scaleKey;
    if (r.settings.mode == as::ArpMode::Step)
        r.input = { { heldNotes.empty() ? root : heldNotes.back(), 100 } };
    else if (! heldNotes.empty())
        for (int n : heldNotes) r.input.push_back ({ n, 100 });
    else
        for (int i : as::exporter::chordIntervals (juce::roundToInt (d.raw (as::ParamIDs::exportChord))))
            if (root + i <= 127) r.input.push_back ({ root + i, 100 });
    return as::exporter::writeToTempFile (as::exporter::renderPattern (r), as::exporter::suggestedName (r).replace ("ArpSampler", "VC-Clone"));
}

std::unique_ptr<juce::Component> StepSequencer::createDragHandle (std::function<juce::File()> makeFile)
{
    auto h = std::make_unique<DragMidiHandle>();
    h->createFile = std::move (makeFile);
    h->setTooltip ("Drag the sequence onto a DAW track as a MIDI clip (right-click: save as .mid)");
    return h;
}

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

void StepSequencer::syncScaleUnits()
{
    auto& d = *impl;
    const bool lock = d.raw (as::ParamIDs::scaleLock) > 0.5f;
    const int key = juce::roundToInt (d.raw (as::ParamIDs::scaleKey)), scale = juce::roundToInt (d.raw (as::ParamIDs::scale));
    auto pat = d.model.get();
    if (pat.scaleSteps == lock && (! lock || (pat.key == key && pat.scale == scale))) return;
    for (auto& st : pat.steps)
    {
        if (lock && ! pat.scaleSteps)      st.transpose = juce::jlimit (-24, 24, as::arp::semitonesToScaleSteps (st.transpose, scale));
        else if (! lock && pat.scaleSteps) st.transpose = juce::jlimit (-24, 24, as::arp::scaleStepsToSemitones (st.transpose, pat.scale));
    }
    pat.scaleSteps = lock;
    pat.key = key;
    pat.scale = scale;
    d.model.set (pat, false);
}

namespace
{
    /** One cell per step: the transpose in semitones and a bar for its height. */
    class NoteGrid : public juce::Component, private juce::Timer, private juce::ChangeListener
    {
    public:
        NoteGrid (as::arp::PatternModel& m, juce::AudioProcessorValueTreeState& s, std::function<int()> current)
            : model (m), state (s), currentStep (std::move (current))
        {
            model.addChangeListener (this);
            startTimerHz (20);
        }
        ~NoteGrid() override { model.removeChangeListener (this); }

        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            auto b = getLocalBounds().toFloat();
            g.setGradientFill (ColourGradient (Colour (0xff07090b), 0.0f, 0.0f, Colour (0xff0d1013), 0.0f, b.getBottom(), false));
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (Colour (0xff050607));
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            const auto pat = model.get();
            const int n = numSteps(), cur = currentStep ? currentStep() : -1;
            const int scale = juce::roundToInt (raw (as::ParamIDs::scale));
            for (int i = 0; i < n; ++i)
            {
                const auto r = cell (i);
                const auto& st = pat.steps[(size_t) i];
                const int semis = pat.scaleSteps ? as::arp::scaleStepsToSemitones (st.transpose, scale) : st.transpose;
                const bool rest = st.isRest();
                g.setColour (Colour (0xff15181b));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (i == cur ? Colour (0xff3fa9ff) : (i == hot ? Colour (0xff3a5a7a) : Colour (0xff0c0d0f)));
                g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
                // bar: 0 near the bottom, +24 at the top, negatives shorter
                const float frac = jlimit (0.04f, 1.0f, 0.18f + (float) semis / 24.0f * 0.5f);
                auto bar = r.reduced (3.0f).withTrimmedTop (r.getHeight() * 0.38f);
                bar = bar.withTop (bar.getBottom() - bar.getHeight() * frac);
                if (! rest)
                {
                    g.setColour (Colour (0xff1f7fe0).withAlpha (0.35f));
                    g.fillRoundedRectangle (bar.expanded (1.5f), 3.0f);
                    g.setColour (Colour (0xff3fa9ff));
                }
                else g.setColour (Colour (0xff2a3036));
                g.fillRoundedRectangle (bar, 2.0f);
                g.setFont (FontOptions (jmin (11.0f, r.getWidth() * 0.42f), Font::bold));
                g.setColour (rest ? Colour (0xff5d666e) : Colour (0xffeaf5ff));
                g.drawText (semis > 0 ? "+" + String (semis) : String (semis), r.withHeight (r.getHeight() * 0.36f).translated (0.0f, 2.0f),
                            Justification::centred, false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            drag = stepAt (e.position);
            if (drag < 0) return;
            model.pushUndoPoint();
            startT = model.get().steps[(size_t) drag].transpose;
            startY = e.position.y;
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (drag < 0) return;
            const float px = e.mods.isShiftDown() ? 24.0f : 8.0f;      // pixels per step
            setTranspose (drag, startT + juce::roundToInt ((startY - e.position.y) / px), false);
        }
        void mouseUp (const juce::MouseEvent&) override { drag = -1; }
        void mouseMove (const juce::MouseEvent& e) override { const int h = stepAt (e.position); if (h != hot) { hot = h; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override { hot = -1; repaint(); }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int i = stepAt (e.position);
            if (i < 0) return;
            auto pat = model.get();
            auto& st = pat.steps[(size_t) i];
            st.volume = st.isRest() ? 127 : 0;
            if (st.velocity <= 0) st.velocity = 100;
            model.set (pat, true);
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int i = stepAt (e.position);
            if (i < 0 || std::abs (w.deltaY) < 1.0e-6f) return;
            setTranspose (i, model.get().steps[(size_t) i].transpose + (w.deltaY > 0 ? 1 : -1), true);
        }

    private:
        float raw (const char* id) const { return state.getRawParameterValue (id)->load(); }
        int numSteps() const
        {
            const int length = juce::roundToInt (raw (as::ParamIDs::seqLength)) == 0 ? 16 : 32;
            return as::arp::effectiveLength (length, juce::roundToInt (raw (as::ParamIDs::seqEnd)));
        }
        juce::Rectangle<float> cell (int i) const
        {
            const auto b = getLocalBounds().toFloat().reduced (4.0f);
            const int n = juce::jmax (1, numSteps());
            const float w = (b.getWidth() - 3.0f * (float) (n - 1)) / (float) n;
            return { b.getX() + (float) i * (w + 3.0f), b.getY(), w, b.getHeight() };
        }
        int stepAt (juce::Point<float> p) const
        {
            for (int i = 0; i < numSteps(); ++i) if (cell (i).expanded (1.5f, 0.0f).contains (p)) return i;
            return -1;
        }
        void setTranspose (int i, int t, bool undoable)
        {
            auto pat = model.get();
            t = juce::jlimit (-24, 24, t);
            if (pat.steps[(size_t) i].transpose == t) return;
            pat.steps[(size_t) i].transpose = t;
            model.set (pat, undoable);
        }
        void timerCallback() override
        {
            const int cur = currentStep ? currentStep() : -1;
            if (cur != lastCur) { lastCur = cur; repaint(); }
        }
        void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

        as::arp::PatternModel& model;
        juce::AudioProcessorValueTreeState& state;
        std::function<int()> currentStep;
        int drag = -1, startT = 0, hot = -1, lastCur = -2;
        float startY = 0.0f;
    };
}

std::unique_ptr<juce::Component> StepSequencer::createNoteGrid()
{
    return std::make_unique<NoteGrid> (impl->model, impl->apvts, [this] { return getCurrentStep(); });
}
} // namespace vendor
