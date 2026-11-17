#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include "NoteOrder.h"
#include "Pattern.h"

namespace as::arp
{
    //==============================================================================
    /** A note command from the arp, at a sample offset inside the current block. */
    struct NoteEvent
    {
        enum class Type : std::uint8_t
        {
            Start,         // new note (a new voice, or a legato slot that changes pitch without restarting)
            Change,        // tied step: the sounding note moves to a new pitch/velocity, gliding
            Stop,          // end the note 'id' (legato stops only apply if the slot still plays 'id')
            LegatoReset    // keys released: legato slots forget their running playheads
        };

        int offset = 0;
        Type type = Type::Start;
        bool legato = false;   // Retrigger Off: the note lives in a legato slot of the sampler
        int id = 0;            // unique per started note
        int slot = 0;          // position in the chord (0 for single-note modes)
        int note = 60;
        int velocity = 100;
        bool reverse = false;  // play the sample backwards (from the step's Reverse flag)
        int volume = 127;      // the step's Volume (scales loudness on top of velocity)
        int stepVelocity = 100; // the step's Velocity lane (drives the filter's velocity link)
        bool accent = false;    // the step's Accent: full velocity, filter opens further
    };

    /** Fixed-capacity, time-sorted event list (no allocation on the audio thread). */
    class EventList
    {
    public:
        static constexpr int kCapacity = 1024;

        void clear() noexcept                          { count = 0; }
        int size() const noexcept                      { return count; }
        const NoteEvent& operator[] (int i) const noexcept { return events[(size_t) i]; }
        const NoteEvent* begin() const noexcept        { return events.data(); }
        const NoteEvent* end() const noexcept          { return events.data() + count; }

        /** Inserts in time order. At equal offsets, stops come before starts. */
        bool add (const NoteEvent&) noexcept;

        /** Removes Start/Change events later than 'offset' (used when everything is stopped mid-block). */
        void removeStartsAfter (int offset) noexcept;

    private:
        std::array<NoteEvent, kCapacity> events {};
        int count = 0;
    };

    //==============================================================================
    struct ArpSettings
    {
        bool enabled = true;
        ArpMode mode = ArpMode::Up;
        int rateIndex = 3;                    // 1/2, 1/4, 1/8, 1/16, 1/32
        RateType rateType = RateType::Straight;
        float gate = 0.75f;                   // 0.01 .. 2.0 of a step
        float swing = 0.0f;                   // -1 .. 1
        int octaves = 1;                      // 1 .. 4
        VelocityMode velocityMode = VelocityMode::Key;
        bool retrigger = true;
        int sequenceLength = 16;              // already includes End
        bool followKey = false;               // Scale Lock: move notes from the pattern's key/scale to the current one;
                                              // in Step mode the sequence plays from the Key's root
        bool quantiseOutput = false;          // snap every note to the scale
        int scaleKey = 0, scaleIndex = 0;
        bool autoPlay = false;                // play while the host plays, as if 'autoNote' were held
        int autoNote = 60;
    };

    struct TransportInfo
    {
        bool hostPlaying = false;             // host transport running with a valid position
        double ppqAtBlockStart = 0.0;         // only used when hostPlaying
        double bpm = 120.0;                   // host tempo, or the internal BPM
        bool looping = false;                 // host loop/cycle on, with these points (quarter notes)
        double loopStart = 0.0, loopEnd = 0.0;
    };

    /** Step length in quarter notes. */
    double stepLengthInQuarters (int rateIndex, RateType) noexcept;

    /** Start of step k in quarter notes, with swing moving every odd step by up to half a step. */
    double stepTimeInQuarters (juce::int64 k, double stepLength, float swing) noexcept;

    /** The velocity a step plays at. 0 means the step is silent. */
    int computeVelocity (VelocityMode, int stepVelocity, int keyVelocity, int holdVelocity) noexcept;

    //==============================================================================
    /**
        Turns held keys into timed note events.

        Clock: while the host plays, step k starts at k * stepLength quarter notes
        of the song position (plus swing), so the grid and the step sequencer stay
        locked to the bar. When the host is stopped, a free-running clock starts
        at the first key press and keeps going until every key is released.
        Pressing or releasing keys mid-pattern never moves the grid.

        Every arp step is also a step-sequencer step: its transpose, velocity
        (0 = rest) and tie apply on top of every mode.
    */
    class ArpEngine
    {
    public:
        ArpEngine();

        void prepare (double sampleRate) noexcept;
        /** Forget held keys, sounding notes and clocks (no events are produced). */
        void reset() noexcept;

        void setSettings (const ArpSettings&) noexcept;
        void setPattern (const Pattern& p) noexcept    { pattern = p; }
        const Pattern& getPattern() const noexcept     { return pattern; }

        /** Seed for Random mode, so offline renders are repeatable. */
        void setRandomSeed (juce::int64 seed) noexcept { randomSeed = seed; selector.reset (seed); }

        /**
            Processes one block: reads note/sustain/panic messages from 'midi' and
            appends the resulting note events to 'out'. With the arp disabled it
            only tracks the keys (and stops any notes it was playing).
        */
        void process (const juce::MidiBuffer& midi, int numSamples, const TransportInfo&, EventList& out) noexcept;

        /** Stops every sounding arp note at 'offset' (transport stop, arp switched off, panic). */
        void stopAll (int offset, EventList& out) noexcept;

        // State for the UI (read from the audio thread only; the processor publishes copies).
        int getCurrentStep() const noexcept            { return currentStep; }
        int getNumHeld() const noexcept                { return numHeld; }
        const HeldNote* getHeldNotes() const noexcept  { return held.data(); }
        bool isRunning() const noexcept                { return numHeld > 0 && settings.enabled; }

    private:
        struct SlotState
        {
            bool sounding = false;   // a note started on this slot and hasn't been stopped
            bool awaitingTie = false;
            bool legato = false;
            int id = 0;
            int note = 0;
        };

        struct PendingEvent
        {
            juce::int64 samplesLeft = 0;
            NoteEvent event;
        };

        void handleMidi (const juce::MidiMessage&, int offset, EventList& out) noexcept;
        void keyDown (int note, int velocity, int offset) noexcept;
        void keyUp (int note) noexcept;
        void releaseAllKeys() noexcept;
        void runSteps (int fromSample, int toSample, EventList& out) noexcept;
        void playStep (juce::int64 k, int offset, EventList& out) noexcept;
        void scheduleStop (const NoteEvent& stop, int offset, EventList& out) noexcept;
        /** Emits the event now, or keeps it for a later block. 'at' is a sample offset from this block's start. */
        void scheduleEvent (NoteEvent, juce::int64 at, EventList& out) noexcept;
        void stopSlot (int slot, int offset, EventList& out) noexcept;
        void resyncGrid (double ppq) noexcept;
        /** The host jumped (loop restart, locate): end what's playing and start again as if from Play. */
        void restartAt (int offset, double ppq, EventList& out) noexcept;
        juce::int64 sampleOfQuarter (double quarters) const noexcept;

        ArpSettings settings;
        Pattern pattern;
        NoteSelector selector;
        juce::int64 randomSeed = 1;
        double sampleRate = 44100.0;

        // held keys, oldest first
        std::array<HeldNote, kMaxHeld> held {};
        int numHeld = 0;
        bool sustainPedal = false;
        std::array<bool, 128> sustained {};
        int holdVelocity = 100;
        bool autoHeld = false;               // Auto Play's virtual key is in 'held'
        void updateAutoPlay (bool wanted, int offset) noexcept;

        // clock
        double stepLength = 0.25;            // quarter notes
        double blockStartPpq = 0.0;          // ppq at sample 0 of the current block
        double quartersPerSample = 0.0;
        double expectedPpq = 0.0;
        bool wasHostPlaying = false;
        bool haveExpectedPpq = false;
        juce::int64 lastStep = -1;           // last step index that has been played (or skipped)
        bool wasEnabled = false;
        bool freeRunning = true;
        int blockSize = 0;

        // notes
        std::array<SlotState, kMaxHeld> slots {};
        static constexpr int kMaxPending = 1024;   // stops and future ratchet hits
        std::array<PendingEvent, kMaxPending> pending {};
        int numPending = 0;
        int nextId = 1;
        juce::int64 lastGateSamples = 1;
        bool legatoResetPending = false;
        int currentStep = -1;
    };
}
