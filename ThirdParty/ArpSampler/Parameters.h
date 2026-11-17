#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** Parameter IDs, ranges, choice lists and value formatting. */
namespace as
{
    inline constexpr const char* kPluginName = "ArpSampler";
    inline constexpr int kParamVersion = 1;

    namespace ParamIDs
    {
        // Sampler
        inline constexpr const char* rootNote    = "rootNote";
        inline constexpr const char* coarse      = "coarse";
        inline constexpr const char* fine        = "fine";
        inline constexpr const char* retrigger   = "retrigger";
        inline constexpr const char* sampleStart = "sampleStart";
        inline constexpr const char* sampleEnd   = "sampleEnd";
        inline constexpr const char* loop        = "loop";
        inline constexpr const char* attack      = "attack";
        inline constexpr const char* decay       = "decay";
        inline constexpr const char* sustain     = "sustain";
        inline constexpr const char* release     = "release";
        inline constexpr const char* glide       = "glide";
        inline constexpr const char* volume      = "volume";

        // Output
        inline constexpr const char* samplerOn   = "samplerOn";
        inline constexpr const char* midiOut     = "midiOut";

        // Filter
        inline constexpr const char* filterOn        = "filterOn";
        inline constexpr const char* filterType      = "filterType";
        inline constexpr const char* filterSlope     = "filterSlope";
        inline constexpr const char* filterCutoff    = "filterCutoff";
        inline constexpr const char* filterReso      = "filterReso";
        inline constexpr const char* filterVelLink   = "filterVelLink";
        inline constexpr const char* filterVelAmount = "filterVelAmount";

        // Arpeggiator
        inline constexpr const char* arpOn       = "arpOn";
        inline constexpr const char* arpMode     = "arpMode";
        inline constexpr const char* arpRate     = "arpRate";
        inline constexpr const char* arpRateType = "arpRateType";
        inline constexpr const char* arpGate     = "arpGate";
        inline constexpr const char* arpSwing    = "arpSwing";
        inline constexpr const char* arpOctave   = "arpOctave";
        inline constexpr const char* velMode     = "velMode";
        inline constexpr const char* bpm         = "bpm";
        inline constexpr const char* autoPlay    = "autoPlay";

        // Step sequencer
        inline constexpr const char* seqLength   = "seqLength";
        inline constexpr const char* seqEnd      = "seqEnd";

        // Scale
        inline constexpr const char* scaleKey       = "scaleKey";
        inline constexpr const char* scale          = "scale";
        inline constexpr const char* scaleLock      = "scaleLock";
        inline constexpr const char* quantiseEdits  = "quantiseEdits";
        inline constexpr const char* quantiseOutput = "quantiseOutput";
        inline constexpr const char* keyLock        = "keyLock";

        // Randomiser (not automatable)
        inline constexpr const char* rndDensity   = "rndDensity";
        inline constexpr const char* rndRange     = "rndRange";
        inline constexpr const char* rndTieProb   = "rndTieProb";
        inline constexpr const char* rndVelMin    = "rndVelMin";
        inline constexpr const char* rndVelMax    = "rndVelMax";
        inline constexpr const char* rndLockPitch = "rndLockPitch";
        inline constexpr const char* rndLockVel   = "rndLockVel";
        inline constexpr const char* rndLockRest  = "rndLockRest";
        inline constexpr const char* rndLockTie   = "rndLockTie";
        inline constexpr const char* rndRatchet   = "rndRatchet";
        inline constexpr const char* rndLockRatchet = "rndLockRatchet";
        inline constexpr const char* rndReverse   = "rndReverse";
        inline constexpr const char* rndLockReverse = "rndLockReverse";
        inline constexpr const char* rndAccent    = "rndAccent";
        inline constexpr const char* rndLockAccent = "rndLockAccent";
        inline constexpr const char* rndFileNotes = "rndFileNotes";

        // MIDI export (not automatable)
        inline constexpr const char* exportBars  = "exportBars";
        inline constexpr const char* exportRoot  = "exportRoot";
        inline constexpr const char* exportChord = "exportChord";
    }

    inline constexpr float kDefaultBpm = 120.0f;
    inline constexpr int kMaxSteps = 32;

    enum class ArpMode { Up, Down, UpDown, UpDown2, DownUp, DownUp2, Random, Ordered, Step, Chord };
    enum class RateType { Straight, Triplet, Dotted };
    enum class VelocityMode { Key, Hold, Step, StepKey, StepHold };

    inline juce::StringArray getArpModeNames()   { return { "Up", "Down", "Up/Down", "Up/Down2", "Down/Up", "Down/Up2", "Random", "Ordered", "Step", "Chord" }; }
    inline juce::StringArray getRateNames()      { return { "1/2", "1/4", "1/8", "1/16", "1/32" }; }
    inline juce::StringArray getRateTypeNames()  { return { "Straight", "Triplet", "Dotted" }; }
    inline juce::StringArray getVelModeNames()   { return { "Key", "Hold", "Step", "Step+Key", "Step+Hold" }; }
    inline juce::StringArray getFilterTypeNames()  { return { "Low Pass", "High Pass", "Band Pass" }; }
    inline juce::StringArray getFilterSlopeNames() { return { "6 dB", "12 dB", "18 dB", "24 dB", "36 dB", "48 dB" }; }
    inline juce::StringArray getSeqLengthNames() { return { "16", "32" }; }
    inline juce::StringArray getKeyNames()       { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    inline juce::StringArray getExportBarNames() { return { "1 bar", "2 bars", "4 bars", "8 bars" }; }
    inline juce::StringArray getExportChordNames() { return { "Major", "Minor", "Maj7", "Min7", "Dom7", "Sus4", "Octave", "Single" }; }

    inline juce::StringArray getSeqEndNames()
    {
        juce::StringArray s { "No" };
        for (int i = 1; i <= kMaxSteps; ++i)
            s.add (juce::String (i));
        return s;
    }

    inline juce::StringArray getScaleNames()
    {
        return { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian",
                 "Lydian", "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic", "Blues",
                 "Phrygian Dominant", "Hungarian Minor", "Whole Tone", "Japanese (In Sen)" };
    }

    /** MIDI note name with C3 = 60. */
    inline juce::String noteName (int note)
    {
        note = juce::jlimit (0, 127, note);
        return getKeyNames()[note % 12] + juce::String (note / 12 - 2);
    }

    // Value formatting ----------------------------------------------------------------
    inline juce::String formatMs (float ms)
    {
        return ms < 1000.0f ? juce::String (ms, ms < 10.0f ? 1 : 0) + " ms"
                            : juce::String (ms / 1000.0f, 2) + " s";
    }
    inline juce::String formatPercent (float v)  { return juce::String (juce::roundToInt (v)) + " %"; }
    inline juce::String formatDb (float db)      { return db <= -59.9f ? juce::String ("-inf dB") : juce::String (db, 1) + " dB"; }
    inline juce::String formatSemis (float v)    { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " st"; }
    inline juce::String formatHz (float hz)      { return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0f, hz < 10000.0f ? 2 : 1) + " kHz"; }
    inline juce::String formatCents (float v)    { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " ct"; }

    //==============================================================================
    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto id = [] (const char* s) { return ParameterID { s, kParamVersion }; };

        auto msRange = [] (float lo, float hi, float centre)
        {
            NormalisableRange<float> r (lo, hi, 0.0f);
            r.setSkewForCentre (centre);
            return r;
        };

        auto floatParam = [&] (const char* pid, const char* name, NormalisableRange<float> range, float def,
                               std::function<String (float)> fmt, bool automatable = true)
        {
            layout.add (std::make_unique<AudioParameterFloat> (
                id (pid), name, range, def,
                AudioParameterFloatAttributes().withStringFromValueFunction ([fmt] (float v, int) { return fmt (v); })
                                               .withAutomatable (automatable)));
        };

        auto intParam = [&] (const char* pid, const char* name, int lo, int hi, int def,
                             std::function<String (int)> fmt = nullptr, bool automatable = true)
        {
            auto attrs = AudioParameterIntAttributes().withAutomatable (automatable);
            if (fmt)
                attrs = attrs.withStringFromValueFunction ([fmt] (int v, int) { return fmt (v); });
            layout.add (std::make_unique<AudioParameterInt> (id (pid), name, lo, hi, def, attrs));
        };

        auto choiceParam = [&] (const char* pid, const char* name, const StringArray& items, int def, bool automatable = true)
        {
            layout.add (std::make_unique<AudioParameterChoice> (id (pid), name, items, def,
                                                                AudioParameterChoiceAttributes().withAutomatable (automatable)));
        };

        auto boolParam = [&] (const char* pid, const char* name, bool def, bool automatable = true)
        {
            layout.add (std::make_unique<AudioParameterBool> (id (pid), name, def,
                                                              AudioParameterBoolAttributes().withAutomatable (automatable)));
        };

        // Sampler -------------------------------------------------------------------
        intParam   (ParamIDs::rootNote, "Root Note", 0, 127, 60, [] (int v) { return noteName (v); });
        intParam   (ParamIDs::coarse, "Transpose", -24, 24, 0, [] (int v) { return formatSemis ((float) v); });
        intParam   (ParamIDs::fine, "Fine Tune", -100, 100, 0, [] (int v) { return formatCents ((float) v); });
        boolParam  (ParamIDs::retrigger, "Retrigger", true);
        floatParam (ParamIDs::sampleStart, "Sample Start", { 0.0f, 1.0f }, 0.0f, [] (float v) { return formatPercent (v * 100.0f); });
        floatParam (ParamIDs::sampleEnd, "Sample End", { 0.0f, 1.0f }, 1.0f, [] (float v) { return formatPercent (v * 100.0f); });
        boolParam  (ParamIDs::loop, "Loop", false);
        floatParam (ParamIDs::attack, "Attack", msRange (0.0f, 5000.0f, 300.0f), 1.0f, formatMs);
        floatParam (ParamIDs::decay, "Decay", msRange (1.0f, 5000.0f, 400.0f), 300.0f, formatMs);
        floatParam (ParamIDs::sustain, "Sustain", { 0.0f, 100.0f }, 100.0f, formatPercent);
        floatParam (ParamIDs::release, "Release", msRange (3.0f, 5000.0f, 400.0f), 60.0f, formatMs);
        floatParam (ParamIDs::glide, "Glide", msRange (0.0f, 1000.0f, 150.0f), 60.0f, formatMs);
        floatParam (ParamIDs::volume, "Volume", msRange (-60.0f, 6.0f, -12.0f), -6.0f, formatDb);

        // Output ----------------------------------------------------------------------
        boolParam   (ParamIDs::samplerOn, "Sampler On", true);
        boolParam   (ParamIDs::midiOut, "MIDI Out", false);

        // Filter --------------------------------------------------------------------
        boolParam   (ParamIDs::filterOn, "Filter On", false);
        choiceParam (ParamIDs::filterType, "Filter Type", getFilterTypeNames(), 0);
        choiceParam (ParamIDs::filterSlope, "Filter Slope", getFilterSlopeNames(), 3);
        floatParam  (ParamIDs::filterCutoff, "Filter Cutoff", msRange (20.0f, 20000.0f, 1000.0f), 20000.0f, formatHz);
        floatParam  (ParamIDs::filterReso, "Filter Resonance", { 0.0f, 100.0f }, 0.0f, formatPercent);
        boolParam   (ParamIDs::filterVelLink, "Filter Velocity Link", false);
        floatParam  (ParamIDs::filterVelAmount, "Filter Velocity Amount", { 0.0f, 100.0f }, 50.0f, formatPercent);

        // Arpeggiator ---------------------------------------------------------------
        boolParam   (ParamIDs::arpOn, "Arp On", true);
        choiceParam (ParamIDs::arpMode, "Arp Mode", getArpModeNames(), 0);
        choiceParam (ParamIDs::arpRate, "Arp Rate", getRateNames(), 3);
        choiceParam (ParamIDs::arpRateType, "Arp Rate Type", getRateTypeNames(), 0);
        floatParam  (ParamIDs::arpGate, "Arp Gate", { 1.0f, 200.0f, 0.0f }, 75.0f, formatPercent);
        floatParam  (ParamIDs::arpSwing, "Arp Swing", { -100.0f, 100.0f, 0.0f }, 0.0f, formatPercent);
        intParam    (ParamIDs::arpOctave, "Arp Octave", 1, 4, 1);
        choiceParam (ParamIDs::velMode, "Velocity Mode", getVelModeNames(), 0);
        boolParam   (ParamIDs::autoPlay, "Auto Play", false);
        floatParam  (ParamIDs::bpm, "Internal BPM", { 20.0f, 300.0f, 0.0f }, kDefaultBpm,
                     [] (float v) { return String (v, 1) + " BPM"; });

        // Step sequencer -------------------------------------------------------------
        choiceParam (ParamIDs::seqLength, "Seq Length", getSeqLengthNames(), 0);
        choiceParam (ParamIDs::seqEnd, "Seq End", getSeqEndNames(), 0);

        // Scale ------------------------------------------------------------------------
        choiceParam (ParamIDs::scaleKey, "Scale Key", getKeyNames(), 0);
        choiceParam (ParamIDs::scale, "Scale", getScaleNames(), 0);
        boolParam   (ParamIDs::scaleLock, "Scale Lock", false);
        boolParam   (ParamIDs::quantiseEdits, "Quantise Edits", false);
        boolParam   (ParamIDs::quantiseOutput, "Quantise Output", false);
        boolParam   (ParamIDs::keyLock, "Key Lock", true, false);

        // Randomiser (UI settings, saved but not automatable) ---------------------------
        floatParam (ParamIDs::rndDensity, "Rnd Density", { 0.0f, 100.0f }, 75.0f, formatPercent, false);
        intParam   (ParamIDs::rndRange, "Rnd Pitch Range", 0, 24, 12,
                    [] (int v) { return v % 12 == 0 && v > 0 ? "+/-" + String (v / 12) + " oct" : "+/-" + String (v) + " st"; }, false);
        floatParam (ParamIDs::rndTieProb, "Rnd Tie Prob", { 0.0f, 100.0f }, 15.0f, formatPercent, false);
        intParam   (ParamIDs::rndVelMin, "Rnd Vel Min", 1, 127, 60, nullptr, false);
        intParam   (ParamIDs::rndVelMax, "Rnd Vel Max", 1, 127, 127, nullptr, false);
        boolParam  (ParamIDs::rndLockPitch, "Rnd Lock Pitch", false, false);
        boolParam  (ParamIDs::rndLockVel, "Rnd Lock Velocity", false, false);
        boolParam  (ParamIDs::rndLockRest, "Rnd Lock Rests", false, false);
        boolParam  (ParamIDs::rndLockTie, "Rnd Lock Ties", false, false);
        floatParam (ParamIDs::rndRatchet, "Rnd Ratchet Prob", { 0.0f, 100.0f }, 10.0f, formatPercent, false);
        boolParam  (ParamIDs::rndLockRatchet, "Rnd Lock Ratchets", false, false);
        floatParam (ParamIDs::rndReverse, "Rnd Reverse Prob", { 0.0f, 100.0f }, 0.0f, formatPercent, false);
        boolParam  (ParamIDs::rndLockReverse, "Rnd Lock Reverse", false, false);
        floatParam (ParamIDs::rndAccent, "Rnd Accent Prob", { 0.0f, 100.0f }, 25.0f, formatPercent, false);
        boolParam  (ParamIDs::rndLockAccent, "Rnd Lock Accents", false, false);
        boolParam  (ParamIDs::rndFileNotes, "Rnd File Notes", false, false);

        // MIDI export --------------------------------------------------------------------
        choiceParam (ParamIDs::exportBars, "Export Length", getExportBarNames(), 1, false);
        intParam    (ParamIDs::exportRoot, "Export Root", -1, 127, -1,
                     [] (int v) { return v < 0 ? String ("Auto") : noteName (v); }, false);
        choiceParam (ParamIDs::exportChord, "Export Chord", getExportChordNames(), 0, false);

        return layout;
    }
}
