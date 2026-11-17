#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include "../Parameters.h"

namespace as::arp
{
    //==============================================================================
    struct Step
    {
        int transpose = 0;      // -24..+24: semitones, or notes of the scale when the pattern uses scale steps
        int velocity = 100;     // 1..127, used by the Step / Step+Key / Step+Hold velocity modes (0 also rests)
        bool tie = false;       // hold this step's note into the next step without retriggering
        int ratchet = 1;        // 1..kMaxRatchet: the note is retriggered this many times within the step
        bool reverse = false;   // notes started on this step play the sample backwards
        int volume = 127;       // 0..127 loudness of the step in every velocity mode; 0 = silent (rest)
        bool accent = false;    // accented: full velocity, louder, filter opens further (MIDI: velocity 127)

        bool isRest() const noexcept    { return volume <= 0 || velocity <= 0; }

        bool operator== (const Step& o) const noexcept
        {
            return transpose == o.transpose && velocity == o.velocity && tie == o.tie
                && ratchet == o.ratchet && reverse == o.reverse && volume == o.volume && accent == o.accent;
        }
        bool operator!= (const Step& o) const noexcept   { return ! operator== (o); }
    };

    static constexpr int kMaxRatchet = 4;

    //==============================================================================
    /** The step data. Length and End are parameters, so they are passed separately. */
    struct Pattern
    {
        std::array<Step, kMaxSteps> steps {};

        // Scale Lock on: the transposes count notes of the Key's scale (0 = the Key's root,
        // -1 = the scale note below it, ...), so every step is always in key. Off: semitones.
        bool scaleSteps = false;

        // The key and scale the pattern was last converted in (used when switching units).
        int key = 0, scale = 0;

        // The notes of the last imported file (as transposes, in the same units as the steps):
        // how many times each transpose -24..+24 appeared. Randomise can pick from just these.
        static constexpr int kPoolSize = 49;
        std::array<std::uint8_t, kPoolSize> notePool {};
        int poolCount (int transpose) const noexcept
        {
            return transpose >= -24 && transpose <= 24 ? notePool[(size_t) (transpose + 24)] : 0;
        }
        int numPoolNotes() const noexcept        // distinct notes
        {
            int n = 0;
            for (auto c : notePool) n += c > 0 ? 1 : 0;
            return n;
        }

        bool operator== (const Pattern& o) const noexcept
        {
            return steps == o.steps && scaleSteps == o.scaleSteps && key == o.key && scale == o.scale && notePool == o.notePool;
        }
        bool operator!= (const Pattern& o) const noexcept   { return ! operator== (o); }
    };

    /** Number of steps that play: the length (16/32), or the End step if it is set and shorter. */
    inline int effectiveLength (int length, int endStep) noexcept
    {
        return endStep > 0 ? std::min (endStep, length) : length;
    }
}
