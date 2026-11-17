#include "mm/Scales.h"

#include <algorithm>
#include <initializer_list>

namespace mm
{
    namespace
    {
        constexpr std::uint16_t mask (std::initializer_list<int> degrees)
        {
            std::uint16_t m = 0;
            for (auto d : degrees)
                m = (std::uint16_t) (m | (1u << d));
            return m;
        }

        constexpr std::array<std::uint16_t, kNumScales> masks
        {
            mask ({ 0, 2, 4, 5, 7, 9, 11 }),    // Major (Ionian, Diatonic)
            mask ({ 0, 2, 3, 5, 7, 8, 10 }),    // Natural Minor (Aeolian)
            mask ({ 0, 2, 3, 5, 7, 8, 11 }),    // Harmonic Minor
            mask ({ 0, 2, 3, 5, 7, 9, 11 }),    // Melodic Minor
            mask ({ 0, 2, 3, 5, 7, 9, 10 }),    // Dorian
            mask ({ 0, 1, 3, 5, 7, 8, 10 }),    // Phrygian
            mask ({ 0, 2, 4, 6, 7, 9, 11 }),    // Lydian
            mask ({ 0, 2, 4, 5, 7, 9, 10 }),    // Mixolydian
            mask ({ 0, 1, 3, 5, 6, 8, 10 }),    // Locrian
            mask ({ 0, 2, 4, 7, 9 }),           // Major Pentatonic
            mask ({ 0, 3, 5, 7, 10 }),          // Minor Pentatonic
            mask ({ 0, 3, 5, 6, 7, 10 }),       // Blues
            mask ({ 0, 1, 4, 5, 7, 8, 10 }),    // Phrygian Dominant (Spanish Gypsy)
            mask ({ 0, 2, 3, 6, 7, 8, 11 }),    // Hungarian Minor
            mask ({ 0, 2, 4, 6, 8, 10 }),       // Whole Tone
            mask ({ 0, 1, 5, 7, 10 }),          // Japanese (In Sen)
            mask ({ 0, 1, 4, 5, 6, 8, 11 }),    // Persian
            0x0fff,                             // Chromatic
            0,                                  // Custom (user mask)
        };

        constexpr const char* names[kNumScales] =
        {
            "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian",
            "Lydian", "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic", "Blues",
            "Phrygian Dominant", "Hungarian Minor", "Whole Tone", "Japanese (In Sen)",
            "Persian", "Chromatic", "Custom"
        };

        constexpr const char* keys[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    }

    const char* scaleName (int i) noexcept   { return names[std::clamp (i, 0, (int) kNumScales - 1)]; }
    const char* keyName (int k) noexcept     { return keys[floorMod (k, 12)]; }

    std::string noteName (int note)
    {
        note = std::clamp (note, 0, 127);
        return std::string (keys[note % 12]) + std::to_string (note / 12 - 2);
    }

    std::uint16_t scaleMask (int i) noexcept { return masks[(size_t) std::clamp (i, 0, (int) kNumScales - 1)]; }

    int floorDiv (int a, int b) noexcept     { return a >= 0 ? a / b : -((-a + b - 1) / b); }
    int floorMod (int a, int b) noexcept     { return a - floorDiv (a, b) * b; }

    //==============================================================================
    ScaleContext ScaleContext::make (int scaleIndex, int key, std::uint16_t customMask) noexcept
    {
        if (scaleIndex == Custom)
            return fromAbsoluteMask (customMask);

        const auto rel = scaleMask (scaleIndex);
        std::uint16_t abs = 0;
        for (int pc = 0; pc < 12; ++pc)
            if ((rel >> pc) & 1u)
                abs = (std::uint16_t) (abs | (1u << floorMod (pc + key, 12)));
        return fromAbsoluteMask (abs);
    }

    ScaleContext ScaleContext::fromAbsoluteMask (std::uint16_t absMask) noexcept
    {
        if ((absMask & 0x0fff) == 0)
            absMask = 0x0fff;   // an empty custom scale behaves as chromatic

        ScaleContext s;
        s.notesPerOctave = 0;
        for (int pc = 0; pc < 12; ++pc)
        {
            s.in[(size_t) pc] = ((absMask >> pc) & 1u) != 0;
            if (s.in[(size_t) pc])
                ++s.notesPerOctave;
        }

        for (int pc = 0; pc < 12; ++pc)
        {
            int d = 0;
            while (! s.in[(size_t) floorMod (pc + d, 12)]) ++d;
            s.up[(size_t) pc] = pc + d;
            d = 0;
            while (! s.in[(size_t) floorMod (pc - d, 12)]) ++d;
            s.down[(size_t) pc] = pc - d;
        }

        s.lowestPc = 0;
        while (! s.in[(size_t) s.lowestPc]) ++s.lowestPc;
        s.highestPc = 11;
        while (! s.in[(size_t) s.highestPc]) --s.highestPc;
        return s;
    }

    int ScaleContext::snap (int p, bool upwards) const noexcept
    {
        const int pc = floorMod (p, 12);
        const int r = (upwards ? up : down)[(size_t) pc] + floorDiv (p, 12) * 12;
        if (r < 0)   return lowestPc;
        if (r > 119) return 108 + highestPc;
        return r;
    }

    int ScaleContext::quantise (int note) const noexcept
    {
        for (int d = 0; d <= 6; ++d)
        {
            if (note - d >= 0 && inScale (note - d))   return note - d;
            if (note + d <= 127 && inScale (note + d)) return note + d;
        }
        return std::clamp (note, 0, 127);
    }

    int ScaleContext::stepsAbove (int root, int steps) const noexcept
    {
        int p = root;
        while (! inScale (p)) --p;
        while (steps > 0) { ++p; if (inScale (p)) --steps; }
        while (steps < 0) { --p; if (inScale (p)) ++steps; }
        return p;
    }

}
