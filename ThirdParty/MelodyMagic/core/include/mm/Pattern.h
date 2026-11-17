#pragma once

#include "MidiFile.h"
#include "Rng.h"
#include "Scales.h"
#include "Types.h"

#include <string>
#include <vector>

namespace mm
{
    /**
        One note of a pattern. 'offset' is in semitones: added to the key the arp is
        playing (with one key held, offset 0 = that key). Ticks at 96 PPQ.
    */
    struct PatternNote
    {
        int start = 0;
        int end = 24;
        int offset = 0;
        int velocity = 100;

        bool operator== (const PatternNote& o) const noexcept
        {
            return start == o.start && end == o.end && offset == o.offset && velocity == o.velocity;
        }
    };

    struct Pattern
    {
        std::string name = "Init";
        int bars = 1;                      // 1, 2, 4 or 8
        std::vector<PatternNote> notes;    // sorted by start, then offset

        int lengthTicks() const noexcept   { return bars * kTicksPerBar; }
        void sort();

        bool operator== (const Pattern& o) const noexcept   { return name == o.name && bars == o.bars && notes == o.notes; }
    };

    constexpr int kMaxOffset = 36;     // offsets stay within -36..+36 semitones
    constexpr int kMaxBars = 8;

    //==============================================================================
    // Arp order
    /** Up / Down / Up & Down / Down & Up arpeggiate the held keys. Melody plays the pattern's
        notes from the Key's root (like ArpSampler's Step mode): the key you hold only starts it
        and picks the octave, so an imported MIDI file plays back at its real pitches. */
    enum class ArpOrder { Up = 0, Down, UpDown, DownUp, Melody };
    constexpr int kNumArpOrders = 5;
    const char* arpOrderName (ArpOrder);

    /** Indices into the held keys (sorted low to high), one cycle of the order. The turn notes
        of Up & Down / Down & Up are not repeated: 4 keys Up & Down = 0 1 2 3 2 1. */
    std::vector<int> arpSequence (int numKeys, ArpOrder);

    //==============================================================================
    // Editing helpers

    /** The offset one semitone up or down. With a scale, the next offset whose note
        (key + offset) is in the scale. Kept within +-kMaxOffset. */
    int stepOffset (int offset, int direction, const ScaleContext* scale, int key);

    /** Rounds bars to 1, 2, 4 or 8. */
    int legalBars (int bars);

    /** Changes the length. Longer: the pattern is copied into the new bars (1 -> 2 bars plays it
        twice, 2 -> 4 repeats both bars). Shorter: notes past the end are dropped or cut. */
    void resizePattern (Pattern&, int bars);

    //==============================================================================
    // Shuffle

    /** Keeps the rhythm and moves the given notes (offsets) to new places: every note of
        'pool' is used, as many times as it appears (repeating the pool if the pattern is longer). */
    void shuffleNotes (Pattern&, const std::vector<int>& pool, Rng&);

    //==============================================================================
    // Import

    /** A MIDI clip (any PPQ) as a pattern, up to kMaxBars from the bar of its first note.
        Offsets are relative to the key's root in octave 3 (C3 = 60), so Melody mode plays the
        file back at its real pitches. 'fileNotes' receives every note's offset. */
    Pattern patternFromMidi (const MidiClip&, int key, const std::string& name, std::vector<int>* fileNotes = nullptr);

    /** The key and scale (Major or Natural Minor) that fit a clip best, by note length. */
    void detectKey (const MidiClip&, int& key, int& scaleIndex);

    /** A library phrase as a pattern: its rhythm and velocities, octave lanes as +-12. With a
        scale, each row becomes a chord note of the key (rows 1, 2, 3 = the scale's 1st, 3rd, 5th),
        so stabs stay chords and arps stay arpeggios when played in Melody mode. */
    Pattern patternFromPhrase (const Phrase&, const ScaleContext* scale = nullptr, int key = 0);
}
