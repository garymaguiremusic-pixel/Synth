#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Parameters.h"
#include "mm/ArpProcessor.h"
#include "mm/Library.h"
#include "mm/Melody.h"
#include "mm/Pattern.h"

#include <array>
#include <atomic>
#include <optional>

/** Note events for the MIDI monitor (audio thread -> editor). */
struct MonitorEvent
{
    int note = 0, vel = 0, channel = 1;
    bool on = true;
};

//==============================================================================
class MelodyMagicProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster,   // pattern, groove, library... changed
                             private juce::Timer
{
public:
    MelodyMagicProcessor();
    ~MelodyMagicProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }

    const juce::String getName() const override             { return JucePlugin_Name; }
    bool acceptsMidi() const override                       { return true; }
    bool producesMidi() const override                      { return true; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 0.0; }

    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    // Pattern (message thread). Every change is one undo step.
    const mm::Pattern& getPattern() const noexcept          { return pattern; }
    void setPattern (const mm::Pattern&);
    /** Call once before a series of editor changes (one undo step), then updatePattern() as often as needed. */
    void beginEdit();
    void updatePattern (const mm::Pattern&);

    bool canUndo() const noexcept   { return ! undoStack.empty(); }
    bool canRedo() const noexcept   { return ! redoStack.empty(); }
    void undo();
    void redo();

    // Melody Maker (always in the Key + Scale)
    /** New Melody: new notes, the pattern (rhythm) stays exactly as it is.
        New Pattern: a new rhythm, the notes (in order) stay. Both switch the Arp to Melody mode. */
    void newMelody();
    void newPattern();
    void switchToMelodyMode();
    void randomiseNotes()           { newMelody(); }
    void shuffleFileNotes();        // keeps the rhythm, moves the MIDI file's notes around
    int numFileNotes() const noexcept               { return (int) filePattern.notes.size(); }
    const juce::String& getFileName() const noexcept { return fileName; }
    /** Learn from the imported MIDI file (rhythm, intervals, notes, range). */
    bool useFileStyle = true;
    bool fileStyleActive() const noexcept           { return useFileStyle && ! filePattern.notes.empty(); }
    /** The chords the melody is written over, e.g. "Am  F  C  G". */
    juce::String describeChords() const;

    // My MIDI: the user's own MIDI patterns (Documents/MelodyMagic/My MIDI)
    juce::File getUserMidiFolder() const;
    /** Loads a MIDI file: its notes and rhythm become the pattern, and the Melody Maker learns from it. */
    bool importMidiFile (const juce::File&, juce::String& error);
    /** Copies a MIDI file into My MIDI, then loads it. */
    bool addMidiToLibrary (const juce::File&, juce::String& error);
    /** Saves the pattern into My MIDI as a .mid (played from the key's root). */
    juce::File savePatternToLibrary (const juce::String& name);

    // Groove
    void setGroove (std::optional<mm::Groove>, const juce::String& name);
    const std::optional<mm::Groove>& getGroove() const noexcept     { return groove; }
    const juce::String& getGrooveName() const noexcept              { return grooveName; }

    std::uint16_t getCustomMask() const noexcept    { return customMask.load(); }
    void setCustomMask (std::uint16_t);

    mm::ScaleContext currentScale() const;
    int currentKey() const;
    int currentScaleIndex() const;
    bool scaleLockOn() const;

    /** Renders the arpeggiated pattern to a .mid file in the temp folder (Drag MIDI). */
    juce::File renderMidiFile();

    // Library
    std::shared_ptr<const mm::Library> getLibrary() const;
    juce::File getLibraryFolder() const                 { return libraryFolder; }
    void setLibraryFolder (const juce::File&);
    void rescanLibrary();
    bool isLibraryLoading() const noexcept              { return libraryLoading.load(); }
    juce::String loadedLibraryKey;                      // highlighted in the library

    //==============================================================================
    // Editor state and live info
    std::atomic<int> editorWidth { 0 };
    std::atomic<double> playPosition { -1.0 };   // ticks, -1 when not playing
    std::vector<int> getHeldChord() const;
    int readMonitor (MonitorEvent* dest, int max);

private:
    void timerCallback() override;
    void rebuildPlayPattern();
    void pushUndo();
    void loadGlobalSettings();
    void saveGlobalSettings();
    mm::ArpSettings readSettings() const;
    mm::MelodySettings melodySettings() const;
    mm::MelodyStyle melodyStyle() const;
    void setParam (const char* id, float value);
    int param (const char* id) const;

    // model
    mm::Pattern pattern;
    std::vector<mm::Pattern> undoStack, redoStack;
    std::optional<mm::Groove> groove;
    juce::String grooveName;
    mm::Pattern filePattern;      // the imported MIDI file, as loaded
    juce::String fileName;
    std::atomic<std::uint16_t> customMask { 0x0fff };
    int lastQStart = -1, lastQEnd = -1, lastQLink = -1;

    // handed to the audio thread with std::atomic_load / atomic_store; old copies are freed here
    std::shared_ptr<const mm::PlayPattern> livePattern;
    std::vector<std::shared_ptr<const void>> graveyard;

    // audio thread
    mm::ArpProcessor arp;
    mm::JuceLcg rng;
    std::vector<mm::Event> hostEvents, outEvents;
    juce::MidiBuffer otherMidi;
    double currentSampleRate = 44100.0;
    std::atomic<double> lastBpm { 120.0 };
    std::array<std::atomic<int>, 16> heldChord {};
    std::atomic<int> heldChordCount { 0 };
    std::array<std::atomic<int>, 16> lastChord {};
    std::atomic<int> lastChordCount { 0 };
    juce::AbstractFifo monitorFifo { 1024 };
    std::array<MonitorEvent, 1024> monitorBuffer {};

    // library
    std::shared_ptr<const mm::Library> library;
    juce::File libraryFolder;
    std::atomic<bool> libraryLoading { false };
    juce::ThreadPool loaderPool { 1 };
    std::unique_ptr<juce::PropertiesFile> globalSettings;

    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE (MelodyMagicProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyMagicProcessor)
};
