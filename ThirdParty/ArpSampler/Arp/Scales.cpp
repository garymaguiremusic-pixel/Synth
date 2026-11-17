#include "Scales.h"
#include <algorithm>
#include <cmath>

namespace as::arp
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
            mask ({ 0, 2, 4, 5, 7, 9, 11 }),    // Major (Ionian)
            mask ({ 0, 2, 3, 5, 7, 8, 10 }),    // Natural Minor (Aeolian)
            mask ({ 0, 2, 3, 5, 7, 8, 11 }),    // Harmonic Minor
            mask ({ 0, 2, 3, 5, 7, 9, 11 }),    // Melodic Minor (ascending)
            mask ({ 0, 2, 3, 5, 7, 9, 10 }),    // Dorian
            mask ({ 0, 1, 3, 5, 7, 8, 10 }),    // Phrygian
            mask ({ 0, 2, 4, 6, 7, 9, 11 }),    // Lydian
            mask ({ 0, 2, 4, 5, 7, 9, 10 }),    // Mixolydian
            mask ({ 0, 1, 3, 5, 6, 8, 10 }),    // Locrian
            mask ({ 0, 2, 4, 7, 9 }),           // Major Pentatonic
            mask ({ 0, 3, 5, 7, 10 }),          // Minor Pentatonic
            mask ({ 0, 3, 5, 6, 7, 10 }),       // Blues
            mask ({ 0, 1, 4, 5, 7, 8, 10 }),    // Phrygian Dominant
            mask ({ 0, 2, 3, 6, 7, 8, 11 }),    // Hungarian Minor
            mask ({ 0, 2, 4, 6, 8, 10 }),       // Whole Tone
            mask ({ 0, 1, 5, 7, 10 }),          // Japanese (In Sen)
        };
    }

    std::uint16_t scaleMask (int scaleIndex) noexcept
    {
        return masks[(size_t) std::clamp (scaleIndex, 0, kNumScales - 1)];
    }

    bool isInScale (int note, int key, int scaleIndex) noexcept
    {
        const int pc = ((note - key) % 12 + 12) % 12;
        return (scaleMask (scaleIndex) >> pc) & 1u;
    }

    int quantiseNote (int note, int key, int scaleIndex) noexcept
    {
        for (int d = 0; d <= 6; ++d)
        {
            if (note - d >= 0 && isInScale (note - d, key, scaleIndex))
                return note - d;
            if (note + d <= 127 && isInScale (note + d, key, scaleIndex))
                return note + d;
        }
        return std::clamp (note, 0, 127);
    }

    namespace
    {
        int floorDiv (int a, int b) noexcept      { return a >= 0 ? a / b : -((-a + b - 1) / b); }

        int degreesOf (int scaleIndex, int* out) noexcept
        {
            int n = 0;
            const auto mask = scaleMask (scaleIndex);
            for (int pc = 0; pc < 12; ++pc)
                if ((mask >> pc) & 1u)
                    out[n++] = pc;
            if (n == 0)
                out[n++] = 0;
            return n;
        }
    }

    int scaleSize (int scaleIndex) noexcept
    {
        int d[12];
        return degreesOf (scaleIndex, d);
    }

    int scaleStepsToSemitones (int steps, int scaleIndex) noexcept
    {
        int d[12];
        const int n = degreesOf (scaleIndex, d);
        const int octave = floorDiv (steps, n);
        return octave * 12 + d[steps - octave * n];
    }

    int semitonesToScaleSteps (int semitones, int scaleIndex) noexcept
    {
        int d[12];
        const int n = degreesOf (scaleIndex, d);
        const auto mask = scaleMask (scaleIndex);
        auto inScale = [mask] (int x) { return ((mask >> (((x % 12) + 12) % 12)) & 1u) != 0; };

        // nearest scale note (ties go down), without any 0..127 limit
        int snapped = semitones;
        for (int dist = 0; dist <= 6; ++dist)
        {
            if (inScale (semitones - dist)) { snapped = semitones - dist; break; }
            if (inScale (semitones + dist)) { snapped = semitones + dist; break; }
        }

        const int octave = floorDiv (snapped, 12);
        const int pc = snapped - octave * 12;
        for (int i = 0; i < n; ++i)
            if (d[i] == pc)
                return octave * n + i;
        return octave * n;
    }

    int moveInScale (int note, int steps, int key, int scaleIndex) noexcept
    {
        key = std::clamp (key, 0, 11);
        const int total = semitonesToScaleSteps (note - key, scaleIndex) + steps;
        return std::clamp (key + scaleStepsToSemitones (total, scaleIndex), 0, 127);
    }

    int quantiseTranspose (int transpose, int key, int scaleIndex) noexcept
    {
        const int root = 60 + key;
        return quantiseNote (root + transpose, key, scaleIndex) - root;
    }
}

namespace as::arp
{
    int transposeBetweenKeys (int note, int fromKey, int fromScale, int toKey, int toScale) noexcept
    {
        fromKey = std::clamp (fromKey, 0, 11);
        toKey = std::clamp (toKey, 0, 11);
        if (fromKey == toKey && fromScale == toScale)
            return note;

        int shift = ((toKey - fromKey) % 12 + 12) % 12;
        if (shift > 6)
            shift -= 12;

        auto degreesOf = [] (int scale, int* out)
        {
            int n = 0;
            const auto mask = scaleMask (scale);
            for (int pc = 0; pc < 12; ++pc)
                if ((mask >> pc) & 1u)
                    out[n++] = pc;
            return n;
        };

        int from[12], to[12];
        const int nFrom = degreesOf (fromScale, from);
        const int nTo = degreesOf (toScale, to);

        const int pc = ((note - fromKey) % 12 + 12) % 12;
        int newPc = pc;
        for (int i = 0; i < nFrom; ++i)
        {
            if (from[i] == pc)
            {
                const int j = nFrom == nTo ? i : std::clamp ((int) std::lround ((double) i * nTo / nFrom), 0, nTo - 1);
                newPc = to[j];
                break;
            }
        }

        const int result = note - pc + newPc + shift;
        return result < 0 ? result + 12 * ((11 - result) / 12) : (result > 127 ? result - 12 * ((result - 116) / 12) : result);
    }
}
