#include "FactoryPatterns.h"

#include <initializer_list>

namespace mmp
{
    namespace
    {
        constexpr int S = mm::kPhrasePpq / 4;   // one 1/16 step

        struct N { int step, len, offset, vel; };

        mm::Pattern make (const char* name, int bars, std::initializer_list<N> notes)
        {
            mm::Pattern p;
            p.name = name;
            p.bars = bars;
            for (auto& n : notes)
                p.notes.push_back ({ n.step * S, n.step * S + n.len, n.offset, n.vel });
            p.sort();
            return p;
        }

        /** Every 1/16 (or 'stepTicks') with the given offsets repeating. */
        mm::Pattern cycle (const char* name, std::initializer_list<int> offsets, int gate, int stepTicks = S, int bars = 1)
        {
            mm::Pattern p;
            p.name = name;
            p.bars = bars;
            const std::vector<int> seq (offsets);
            const int steps = bars * mm::kTicksPerBar / stepTicks;
            for (int i = 0; i < steps; ++i)
            {
                const int vel = (i % 4 == 0) ? 112 : (i % 2 == 0 ? 96 : 84);
                p.notes.push_back ({ i * stepTicks, i * stepTicks + gate, seq[(size_t) i % seq.size()], vel });
            }
            return p;
        }
    }

    const std::map<std::string, mm::Pattern>& factoryPatterns()
    {
        static const std::map<std::string, mm::Pattern> patterns = []
        {
            std::map<std::string, mm::Pattern> m;
            auto add = [&m] (const char* folder, mm::Pattern p) { m[std::string ("Factory/") + folder + "/" + p.name] = std::move (p); };

            // Arps: play the held keys in the Arp Mode order
            add ("Arps", cycle ("16ths",          { 0 }, 18));
            add ("Arps", cycle ("8ths",           { 0 }, 36, 2 * S));
            add ("Arps", cycle ("Triplets",       { 0 }, 24, 32));
            add ("Arps", cycle ("Octave Jump",    { 0, 12 }, 16));
            add ("Arps", cycle ("Two Octaves",    { 0, 0, 0, 12, 12, 12 }, 16));
            add ("Arps", make ("Gallop", 1,
                { { 0, 20, 0, 116 }, { 2, 10, 0, 92 }, { 3, 10, 0, 96 }, { 4, 20, 0, 112 }, { 6, 10, 0, 92 }, { 7, 10, 0, 96 },
                  { 8, 20, 0, 116 }, { 10, 10, 0, 92 }, { 11, 10, 0, 96 }, { 12, 20, 0, 112 }, { 14, 10, 0, 92 }, { 15, 10, 0, 96 } }));
            add ("Arps", make ("Offbeat 8ths", 1, { { 2, 20, 0, 110 }, { 6, 20, 0, 104 }, { 10, 20, 0, 110 }, { 14, 20, 0, 104 } }));
            add ("Arps", make ("Syncopated", 1,
                { { 0, 16, 0, 116 }, { 3, 16, 0, 96 }, { 6, 16, 0, 104 }, { 8, 16, 12, 100 }, { 11, 16, 0, 96 }, { 14, 16, 0, 100 } }));

            // Melodies: hold one key (the key of the song) and these play as lines
            add ("Melodies", make ("Pentatonic Run", 1,
                { { 0, 20, 0, 112 }, { 2, 20, 2, 96 }, { 4, 20, 4, 100 }, { 6, 20, 7, 96 },
                  { 8, 20, 9, 108 }, { 10, 20, 7, 96 }, { 12, 20, 4, 100 }, { 14, 20, 2, 96 } }));
            add ("Melodies", make ("Minor Hook", 2,
                { { 0, 20, 0, 112 }, { 2, 20, 3, 96 }, { 3, 40, 7, 104 }, { 8, 20, 10, 100 }, { 10, 40, 7, 96 },
                  { 16, 20, 5, 108 }, { 18, 20, 3, 96 }, { 19, 40, 0, 100 }, { 24, 20, -2, 96 }, { 26, 60, 0, 104 } }));
            add ("Melodies", make ("Trance Lead", 1,
                { { 0, 16, 0, 116 }, { 2, 16, 12, 96 }, { 3, 16, 7, 100 }, { 5, 16, 12, 96 }, { 6, 16, 3, 104 },
                  { 8, 16, 0, 112 }, { 10, 16, 12, 96 }, { 11, 16, 7, 100 }, { 13, 16, 5, 96 }, { 14, 16, 3, 104 } }));

            // Bass
            add ("Bass", make ("Offbeat Bass", 1, { { 2, 18, -12, 110 }, { 6, 18, -12, 104 }, { 10, 18, -12, 110 }, { 14, 18, -12, 104 } }));
            add ("Bass", make ("Rolling Bass", 1,
                { { 1, 16, -12, 100 }, { 2, 16, -12, 92 }, { 3, 16, -12, 96 }, { 5, 16, -12, 100 }, { 6, 16, -12, 92 }, { 7, 16, -12, 96 },
                  { 9, 16, -12, 100 }, { 10, 16, -12, 92 }, { 11, 16, -12, 96 }, { 13, 16, -12, 100 }, { 14, 16, -12, 92 }, { 15, 16, -5, 96 } }));
            add ("Bass", make ("Octave Pump", 1,
                { { 0, 20, -12, 112 }, { 2, 12, 0, 90 }, { 4, 20, -12, 112 }, { 6, 12, 0, 90 },
                  { 8, 20, -12, 112 }, { 10, 12, 0, 90 }, { 12, 20, -12, 112 }, { 14, 12, 0, 90 } }));

            // Chords: stacked notes from one held key (major / minor shapes)
            add ("Chords", make ("Major Stabs", 1,
                { { 2, 14, 0, 104 }, { 2, 14, 4, 104 }, { 2, 14, 7, 104 }, { 6, 14, 0, 100 }, { 6, 14, 4, 100 }, { 6, 14, 7, 100 },
                  { 10, 14, 0, 104 }, { 10, 14, 4, 104 }, { 10, 14, 7, 104 }, { 14, 14, 0, 100 }, { 14, 14, 4, 100 }, { 14, 14, 7, 100 } }));
            add ("Chords", make ("Minor Stabs", 1,
                { { 2, 14, 0, 104 }, { 2, 14, 3, 104 }, { 2, 14, 7, 104 }, { 6, 14, 0, 100 }, { 6, 14, 3, 100 }, { 6, 14, 7, 100 },
                  { 10, 14, 0, 104 }, { 10, 14, 3, 104 }, { 10, 14, 7, 104 }, { 14, 14, 0, 100 }, { 14, 14, 3, 100 }, { 14, 14, 7, 100 } }));
            return m;
        }();
        return patterns;
    }

    mm::Pattern defaultPattern()
    {
        return factoryPatterns().at ("Factory/Arps/16ths");
    }
}
