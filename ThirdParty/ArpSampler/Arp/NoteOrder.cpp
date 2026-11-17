#include "NoteOrder.h"

namespace as::arp
{
    int buildCycle (ArpMode mode, int n, int* out) noexcept
    {
        if (n <= 0)
            return 0;

        if (n == 1 || mode == ArpMode::Step || mode == ArpMode::Chord)
        {
            out[0] = 0;
            return 1;
        }

        int len = 0;
        auto up   = [&] (int from, int to) { for (int i = from; i <= to; ++i) out[len++] = i; };
        auto down = [&] (int from, int to) { for (int i = from; i >= to; --i) out[len++] = i; };

        switch (mode)
        {
            case ArpMode::Up:       up (0, n - 1); break;
            case ArpMode::Down:     down (n - 1, 0); break;
            case ArpMode::UpDown:   up (0, n - 1); down (n - 2, 1); break;
            case ArpMode::UpDown2:  up (0, n - 1); down (n - 1, 0); break;
            case ArpMode::DownUp:   down (n - 1, 0); up (1, n - 2); break;
            case ArpMode::DownUp2:  down (n - 1, 0); up (0, n - 1); break;
            case ArpMode::Random:
            case ArpMode::Ordered:  up (0, n - 1); break;
            case ArpMode::Step:
            case ArpMode::Chord:    break;
        }

        return len;
    }

    //==============================================================================
    void NoteSelector::reset (juce::int64 randomSeed) noexcept
    {
        counter = 0;
        lastRandom = -1;
        random.setSeed (randomSeed);
    }

    void NoteSelector::select (ArpMode mode, const HeldNote* held, int numHeld, int octaves, int sequencePass, StepNotes& out) noexcept
    {
        out.count = 0;
        numHeld = juce::jlimit (0, kMaxHeld, numHeld);
        octaves = juce::jlimit (1, 4, octaves);

        if (numHeld == 0)
            return;

        // Notes sorted low to high (insertion sort: at most 32 notes, no allocation)
        HeldNote sorted[kMaxHeld];
        for (int i = 0; i < numHeld; ++i)
        {
            int j = i;
            while (j > 0 && sorted[j - 1].note > held[i].note)
            {
                sorted[j] = sorted[j - 1];
                --j;
            }
            sorted[j] = held[i];
        }

        auto add = [&out] (const HeldNote& h, int octave)
        {
            out.notes[out.count] = h.note + 12 * octave;
            out.velocities[out.count] = h.velocity;
            ++out.count;
        };

        const int seqOctave = (int) ((juce::uint32) juce::jmax (0, sequencePass) % (juce::uint32) octaves);

        if (mode == ArpMode::Step)
        {
            add (held[numHeld - 1], seqOctave);            // most recently pressed key
        }
        else if (mode == ArpMode::Chord)
        {
            for (int i = 0; i < numHeld; ++i)
                add (sorted[i], seqOctave);
        }
        else
        {
            int cycle[2 * kMaxHeld];
            const int cycleLen = buildCycle (mode, numHeld, cycle);
            const juce::uint32 pos = counter % (juce::uint32) (cycleLen * octaves);
            const int octave = (int) (pos / (juce::uint32) cycleLen);

            if (mode == ArpMode::Random)
            {
                int pick = 0;
                if (numHeld > 1)
                {
                    // never the same key twice in a row
                    if (lastRandom >= 0 && lastRandom < numHeld)
                    {
                        pick = random.nextInt (numHeld - 1);
                        if (pick >= lastRandom)
                            ++pick;
                    }
                    else
                    {
                        pick = random.nextInt (numHeld);
                    }
                }
                lastRandom = pick;
                add (sorted[pick], octave);
            }
            else
            {
                const int index = cycle[pos % (juce::uint32) cycleLen];
                add (mode == ArpMode::Ordered ? held[index] : sorted[index], octave);
            }
        }

        ++counter;
    }
}
