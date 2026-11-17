#pragma once

#include <juce_core/juce_core.h>
#include "../Parameters.h"

namespace as::arp
{
    static constexpr int kMaxHeld = 32;

    struct HeldNote
    {
        int note = 0;
        int velocity = 0;    // 1..127
    };

    /** The notes chosen for one step (a single note, or a whole chord). */
    struct StepNotes
    {
        int count = 0;
        int notes[kMaxHeld] {};
        int velocities[kMaxHeld] {};    // key velocity of the held note each one came from
    };

    /**
        One cycle of the up/down family as indices into the notes sorted low to high
        (for Ordered and Random, indices into the press order). Returns the length.
          Up       0 1 2 3          Down     3 2 1 0
          Up/Down  0 1 2 3 2 1      Up/Down2 0 1 2 3 3 2 1 0
          Down/Up  3 2 1 0 1 2      Down/Up2 3 2 1 0 0 1 2 3
        A single note always gives a cycle of one.
    */
    int buildCycle (ArpMode mode, int numNotes, int* out) noexcept;

    //==============================================================================
    /**
        Picks the notes for successive arp steps. It keeps a step counter, so
        changing the held notes changes what plays next without restarting the
        pattern. Octaves: each full cycle is repeated one octave higher, up to the
        octave count, then the whole thing starts again from the bottom.

        Step and Chord modes move up an octave after each full pass of the step
        sequence instead, which the caller passes in as 'sequencePass'.
    */
    class NoteSelector
    {
    public:
        void reset (juce::int64 randomSeed = 1) noexcept;

        /** 'held' is in the order the keys were pressed (oldest first). Advances the counter. */
        void select (ArpMode, const HeldNote* held, int numHeld, int octaves, int sequencePass, StepNotes& out) noexcept;

        juce::uint32 getCounter() const noexcept    { return counter; }

    private:
        juce::uint32 counter = 0;
        int lastRandom = -1;
        juce::Random random { 1 };
    };
}
