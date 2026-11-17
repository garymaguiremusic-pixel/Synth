#pragma once

#include <array>
#include <cstdint>

namespace as::arp
{
    static constexpr int kNumScales = 16;

    /** Bit n set = pitch class n (semitones above the key) is in the scale. Order matches getScaleNames(). */
    std::uint16_t scaleMask (int scaleIndex) noexcept;

    /** True if 'note' belongs to the scale with root 'key' (0 = C .. 11 = B). */
    bool isInScale (int note, int key, int scaleIndex) noexcept;

    /** Nearest note in the scale (ties go down), kept within 0..127. */
    int quantiseNote (int note, int key, int scaleIndex) noexcept;

    /**
        Quantises a step transpose, treating 0 as the key's root: the result is the
        transpose that lands on the nearest scale note when the triggering note is the key.
    */
    int quantiseTranspose (int transpose, int key, int scaleIndex) noexcept;

    /** Notes per octave in the scale (5 to 7 for the built-in scales). */
    int scaleSize (int scaleIndex) noexcept;

    /** 'steps' notes of the scale above (or below, if negative) the root, in semitones.
        E.g. Major: 1 -> 2, 2 -> 4, -1 -> -1, 7 -> 12. */
    int scaleStepsToSemitones (int steps, int scaleIndex) noexcept;

    /** The other way: the scale step nearest to 'semitones' above the root (ties go down). */
    int semitonesToScaleSteps (int semitones, int scaleIndex) noexcept;

    /** Moves 'note' (snapped into the scale first) by 'steps' notes of the scale. Kept within 0..127. */
    int moveInScale (int note, int steps, int key, int scaleIndex) noexcept;
}

namespace as::arp
{
    /**
        Moves a note from one key/scale to another, keeping the melody's shape:
          - the key moves by the shortest step (-5..+6 semitones), and
          - a note on scale degree n of the old scale lands on degree n of the new one
            (degrees are spread proportionally when the scales have different sizes).
        Notes outside the old scale just move with the key. Result is kept within 0..127.
    */
    int transposeBetweenKeys (int note, int fromKey, int fromScale, int toKey, int toScale) noexcept;
}
