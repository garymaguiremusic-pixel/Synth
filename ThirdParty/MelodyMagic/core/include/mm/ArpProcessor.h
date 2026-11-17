#pragma once

#include "ClipPlayer.h"
#include "Pattern.h"
#include "Rng.h"
#include "Scales.h"

#include <array>
#include <vector>

namespace mm
{
    struct ArpSettings
    {
        ArpOrder order = ArpOrder::Up;
        double speed = 1.0;
        int randomVel = 0;          // +- random velocity, 0..127
        bool scaleLock = false;     // every output note moves to the nearest scale note
        ScaleContext scale = ScaleContext::make (Major, 0);
        int key = 0;                // Melody mode plays from this key's root
        bool autoPlay = false;      // with no key held and the host playing, play as if the key's root (octave 3) was held
        bool bypassed = false;
    };

    struct TransportInfo
    {
        bool playing = false;
        bool hasPosition = false;
        double ppqPosition = 0.0;   // quarter notes
        double bpm = 120.0;
        double sampleRate = 44100.0;
    };

    /** A pattern ready for playback: the clip (after the groove), and per note its offset and
        its arp step (its place in time order, which picks the held key). */
    struct PlayPattern
    {
        PlayClip clip;
        std::vector<int> offset, step;
    };

    PlayPattern makePlayPattern (const Pattern&, const Groove* groove, double startStrength, double endStrength, bool link);

    /** Counts sounding output notes so they can always be switched off. */
    class OutputTracker
    {
    public:
        void record (const Event&) noexcept;
        void flush (int time, std::vector<Event>& out);

    private:
        std::array<std::array<std::uint8_t, 128>, 16> counts {};
    };

    struct HeldNote
    {
        int note = 60, vel = 100, channel = 1;
    };

    /**
        The arpeggiator. Each pattern note plays the next held key in the arp order (Up, Down,
        Up & Down, Down & Up) plus the note's offset; velocity = note velocity x key velocity / 127.

        Timing: while the host plays, the pattern follows the song position. While it is stopped,
        the pattern starts from the first key pressed (after all keys were released) at host tempo;
        stopping the host stops the pattern until the next fresh key press.
        Releasing every key ends the sounding notes.
    */
    class ArpProcessor
    {
    public:
        explicit ArpProcessor (Rng* random = nullptr) : rng (random) {}
        void setRng (Rng* r) noexcept   { rng = r; }

        /** hostIn: note events (time = sample offset, channel 1..16). 'pattern' may be null. */
        void process (const TransportInfo&, int numSamples, const ArpSettings&, const PlayPattern* pattern,
                      const std::vector<Event>& hostIn, std::vector<Event>& out);

        /** All notes off (keeps the held keys). */
        void panic (int time, std::vector<Event>& out);

        /** Pattern position in ticks for the editor's playhead, -1 when not playing. */
        double getPosition() const noexcept                     { return position; }
        const std::vector<HeldNote>& getChord() const noexcept  { return chord; }

        static int velocityOf (int noteVel, int keyVel, int randomVel, Rng*);

    private:
        struct Sounding { int id, pitch, channel; };

        void endAll (int time, std::vector<Event>& out);

        Rng* rng = nullptr;
        ClipPlayer clip;
        OutputTracker tracker;
        std::vector<HeldNote> chord;   // sorted low to high
        std::vector<Sounding> sounding;
        int heldKeys = 0;
        double internalTick = 0.0;
        double position = -1.0;
        bool wasPlaying = false, wasBypassed = false;
        bool internalRunning = false;   // stopped host: runs from a fresh key press
        const PlayPattern* lastPattern = nullptr;
    };

    /** Offline render (Drag MIDI): 'chord' held for the whole length, transport running from 0.
        Returns notes in 96-PPQ ticks. */
    std::vector<TimedNote> renderPattern (const PlayPattern&, const std::vector<int>& chord, int keyVelocity,
                                          const ArpSettings&, double bpm, double lengthTicks, Rng*);
}
