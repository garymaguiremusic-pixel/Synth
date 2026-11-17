#include "mm/Melody.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace mm
{
    //==============================================================================
    // Scale degrees
    //==============================================================================
    int degreeToOffset (int degree, const ScaleContext& sc, int key)
    {
        return sc.stepsAbove (key, degree) - sc.stepsAbove (key, 0);
    }

    int offsetToDegree (int offset, const ScaleContext& sc, int key)
    {
        const int root = sc.stepsAbove (key, 0);
        int p = key + offset;
        for (int d = 0; d <= 6; ++d)   // nearest scale note, ties down
        {
            if (sc.inScale (p - d)) { p -= d; break; }
            if (sc.inScale (p + d)) { p += d; break; }
        }
        int deg = 0;
        for (int x = root; x < p; ++x)  if (sc.inScale (x + 1)) ++deg;
        for (int x = root; x > p; --x)  if (sc.inScale (x - 1)) --deg;
        return deg;
    }

    namespace
    {
        int scaleSize (const ScaleContext& sc)   { return std::max (1, sc.notesPerOctave); }

        bool isMinor (const ScaleContext& sc, int key)
        {
            return sc.inScale (key + 3) && ! sc.inScale (key + 4);
        }

        /** Chord tones of the triad on 'root' (degrees root, root+2, root+4). */
        bool isChordTone (int degree, int root, int n)
        {
            const int r = floorMod (degree - root, n);
            return r == 0 || r == 2 || (r == 4 && n > 4);
        }

        int nearestChordTone (int degree, int root, int n, int lo, int hi)
        {
            degree = std::clamp (degree, lo, hi);
            for (int d = 0; d <= n; ++d)
            {
                if (degree - d >= lo && isChordTone (degree - d, root, n)) return degree - d;
                if (degree + d <= hi && isChordTone (degree + d, root, n)) return degree + d;
            }
            return degree;
        }

        /** The note that carries the harmony in its beat: the first note in each beat. */
        bool isAnchor (const std::vector<PatternNote>& rhythm, size_t i)
        {
            const int step = (rhythm[i].start / (kPhrasePpq / 4)) % 16;
            if (step % 4 == 0 || i == 0)
                return true;
            const int prevStep = (rhythm[i - 1].start / (kPhrasePpq / 4)) % 16;
            return prevStep / 4 != step / 4;
        }

        template <typename T>
        T pickWeighted (const std::vector<std::pair<T, double>>& options, Rng& rng)
        {
            double total = 0.0;
            for (auto& o : options) total += std::max (0.0, o.second);
            if (total <= 0.0)
                return options.front().first;
            double r = rng.nextDouble() * total;
            for (auto& o : options)
            {
                r -= std::max (0.0, o.second);
                if (r <= 0.0)
                    return o.first;
            }
            return options.back().first;
        }

        //==============================================================================
        // Classic trance lead rhythms: (step, length) on a 1/16 grid, one bar
        struct Hit { int step, len; };
        const std::vector<std::vector<Hit>>& rhythmTemplates()
        {
            static const std::vector<std::vector<Hit>> t
            {
                { { 2, 2 }, { 6, 2 }, { 10, 2 }, { 14, 2 } },                                                       // offbeat 8ths
                { { 0, 3 }, { 3, 3 }, { 6, 4 }, { 10, 2 }, { 12, 4 } },                                             // long-short
                { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 3 }, { 11, 3 }, { 14, 2 } },                                   // 3-3-2 (tresillo)
                { { 0, 4 }, { 4, 2 }, { 6, 2 }, { 8, 4 }, { 12, 2 }, { 14, 2 } },                                   // anthem
                { { 0, 2 }, { 3, 2 }, { 5, 1 }, { 6, 2 }, { 8, 2 }, { 11, 2 }, { 13, 1 }, { 14, 2 } },              // syncopated
                { { 0, 2 }, { 2, 1 }, { 3, 2 }, { 5, 1 }, { 6, 2 }, { 8, 2 }, { 10, 1 }, { 11, 2 }, { 13, 1 }, { 14, 2 } },   // rolling
                { { 0, 2 }, { 2, 2 }, { 4, 1 }, { 5, 1 }, { 6, 2 }, { 8, 2 }, { 10, 2 }, { 12, 1 }, { 13, 1 }, { 14, 2 } },   // uplifting
                { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 8, 1 }, { 9, 1 }, { 10, 1 }, { 12, 1 }, { 13, 1 }, { 14, 1 } },  // driving 16ths
                { { 0, 2 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 6, 1 }, { 7, 1 }, { 8, 2 }, { 10, 1 }, { 11, 1 }, { 12, 2 }, { 14, 1 }, { 15, 1 } }, // gallop
            };
            return t;
        }

        constexpr int kStep = kPhrasePpq / 4;

        std::vector<PatternNote> templateBar (int density, int gate, Rng& rng)
        {
            const auto& t = rhythmTemplates();
            const int target = 4 + (int) std::lround (std::clamp (density, 0, 100) / 100.0 * 8.0);
            std::vector<std::pair<int, double>> options;
            for (int i = 0; i < (int) t.size(); ++i)
            {
                const int diff = std::abs ((int) t[(size_t) i].size() - target);
                options.push_back ({ i, diff == 0 ? 4.0 : (diff == 1 ? 2.0 : (diff == 2 ? 0.6 : 0.05)) });
            }
            const auto& chosen = t[(size_t) pickWeighted (options, rng)];

            std::vector<PatternNote> bar;
            for (auto& h : chosen)
            {
                PatternNote n;
                n.start = h.step * kStep;
                n.end = n.start + std::max (kStep / 2, (int) std::lround (h.len * kStep * std::clamp (gate, 10, 100) / 100.0));
                n.velocity = h.step % 4 == 0 ? 116 : (h.step % 2 == 0 ? 104 : 94);
                bar.push_back (n);
            }
            return bar;
        }

        /** The answer bar: the hook's first half, then a new second half. */
        std::vector<PatternNote> answerBar (const std::vector<PatternNote>& a, int density, int gate, bool ending, Rng& rng)
        {
            std::vector<PatternNote> bar;
            for (auto& n : a)
                if (n.start < 8 * kStep)
                    bar.push_back (n);
            for (auto& n : bar)
                n.end = std::min (n.end, 8 * kStep);

            if (ending)
            {
                // phrase end: a long held note on beat 3
                bar.push_back ({ 8 * kStep, 8 * kStep + (int) std::lround (6 * kStep * std::clamp (gate, 30, 100) / 100.0), 0, 112 });
            }
            else if (density >= 55 && rng.chance (0.6))
            {
                // a climbing run back into the hook
                for (int s = 8; s < 16; ++s)
                    bar.push_back ({ s * kStep, s * kStep + std::max (kStep / 2, kStep * std::clamp (gate, 10, 100) / 100), 0, s % 2 == 0 ? 108 : 96 });
            }
            else
            {
                const auto other = templateBar (density, gate, rng);
                for (auto& n : other)
                    if (n.start >= 8 * kStep)
                        bar.push_back (n);
            }
            std::stable_sort (bar.begin(), bar.end(), [] (const PatternNote& x, const PatternNote& y) { return x.start < y.start; });
            return bar;
        }

        //==============================================================================
        /** Writes the notes of one bar (given its rhythm) over a chord. */
        struct Writer
        {
            const ScaleContext& sc;
            const MelodyStyle& style;
            int n;
            int lo, hi;
            Rng& rng;

            double degreeWeight (int d) const
            {
                if (style.degreeWeight.empty())
                    return 1.0;
                return style.degreeWeight[(size_t) floorMod (d, (int) style.degreeWeight.size())];
            }

            double stepWeight (int step) const
            {
                const int i = std::clamp (step, -7, 7) + 7;
                return (size_t) i < style.stepWeight.size() ? style.stepWeight[(size_t) i] : 0.1;
            }

            /** 'target' shapes the contour: the line drifts towards it. */
            int next (int prev, int root, bool strong, int target)
            {
                std::vector<std::pair<int, double>> options;
                for (int d = prev - 7; d <= prev + 7; ++d)
                {
                    if (d < lo || d > hi)
                        continue;
                    const int step = d - prev;
                    double w = stepWeight (step) * degreeWeight (d);
                    if (strong)
                        w *= isChordTone (d, root, n) ? 3.0 : 0.08;   // beats sit on chord tones
                    else if (isChordTone (d, root, n))
                        w *= 1.4;
                    if ((target - prev) * step > 0)
                        w *= 1.7;                                      // heading the right way
                    if (std::abs (step) > 2 && std::abs (d - target) > std::abs (prev - target))
                        w *= 0.3;                                      // no big leaps away from the goal
                    options.push_back ({ d, w });
                }
                if (strong)
                {
                    // notes on the beat are always chord tones
                    std::vector<std::pair<int, double>> tones;
                    for (auto& o : options)
                        if (isChordTone (o.first, root, n))
                            tones.push_back (o);
                    if (! tones.empty())
                        return pickWeighted (tones, rng);
                    return nearestChordTone (prev, root, n, lo, hi);
                }
                if (options.empty())
                    return std::clamp (prev, lo, hi);
                return pickWeighted (options, rng);
            }

            std::vector<int> line (const std::vector<PatternNote>& rhythm, int root, int start, int target, bool pedal)
            {
                std::vector<int> out;
                int prev = start;
                const int pedalDegree = nearestChordTone (root + n, root, n, lo, hi);
                for (size_t i = 0; i < rhythm.size(); ++i)
                {
                    const int pos = (rhythm[i].start / kStep) % 16;
                    const bool strong = isAnchor (rhythm, i);
                    int d;
                    if (i == 0)
                        d = start;
                    else if (pedal && i % 2 == 1)
                        d = pedalDegree;   // trance pedal: every other note returns to a high chord tone
                    else
                    {
                        // contour: rise to a peak in the middle of the bar, then fall to the target
                        const int peak = std::min (hi, start + n / 2 + 1);
                        const int goal = pos < 8 ? peak : target;
                        d = next (prev, root, strong, goal);
                    }
                    out.push_back (d);
                    if (! (pedal && i % 2 == 1))
                        prev = d;
                }
                return out;
            }
        };

        /** Moves a hook written over chord 'fromRoot' onto chord 'toRoot'. */
        std::vector<int> fitHook (const std::vector<int>& hook, const std::vector<PatternNote>& rhythm, int fromRoot, int toRoot,
                                  bool sequence, int n, int lo, int hi)
        {
            int shift = 0;
            if (sequence)
            {
                shift = floorMod (toRoot - fromRoot, n);
                if (shift > n / 2) shift -= n;   // the smaller move
            }
            std::vector<int> out;
            for (size_t i = 0; i < hook.size(); ++i)
            {
                int d = hook[i] + shift;
                while (d > hi) d -= n;
                while (d < lo) d += n;
                d = std::clamp (d, lo, hi);   // ranges narrower than an octave
                if (isAnchor (rhythm, i) && ! isChordTone (d, toRoot, n))
                    d = nearestChordTone (d, toRoot, n, lo, hi);
                out.push_back (d);
            }
            return out;
        }
    }

    //==============================================================================
    // Styles
    //==============================================================================
    MelodyStyle defaultMelodyStyle (const ScaleContext& sc)
    {
        MelodyStyle s;
        const int n = scaleSize (sc);
        s.degreeWeight.assign ((size_t) n, 1.0);
        s.degreeWeight[0] = 1.6;                     // the root
        if (n > 2) s.degreeWeight[2] = 1.3;          // the third
        if (n > 4) s.degreeWeight[4] = 1.4;          // the fifth
        //            -7    -6    -5    -4    -3    -2    -1    0     +1    +2    +3    +4    +5    +6    +7
        s.stepWeight = { 0.35, 0.02, 0.05, 0.25, 0.45, 1.2, 3.0, 0.9, 3.0, 1.3, 0.5, 0.3, 0.05, 0.02, 0.4 };
        s.lowDegree = -2;
        s.highDegree = n + 3;
        return s;
    }

    MelodyStyle styleFromPattern (const Pattern& p, const ScaleContext& sc, int key)
    {
        auto s = defaultMelodyStyle (sc);
        if (p.notes.empty())
            return s;
        s.fromFile = true;
        const int n = scaleSize (sc);

        // the top note of each onset is the melody
        std::map<int, int> topAt;
        for (auto& note : p.notes)
        {
            const int d = offsetToDegree (note.offset, sc, key);
            auto it = topAt.find (note.start);
            if (it == topAt.end() || d > it->second)
                topAt[note.start] = d;
        }

        std::vector<double> degreeCounts ((size_t) n, 0.5);
        std::vector<double> stepCounts (15, 0.0);
        int lo = 1000, hi = -1000, prev = 0;
        bool first = true;
        for (auto& [start, d] : topAt)
        {
            degreeCounts[(size_t) floorMod (d, n)] += 1.0;
            lo = std::min (lo, d);
            hi = std::max (hi, d);
            if (! first)
                stepCounts[(size_t) (std::clamp (d - prev, -7, 7) + 7)] += 1.0;
            prev = d;
            first = false;
        }
        s.degreeWeight = degreeCounts;
        // blend the file's intervals with the default ones, so short files still move
        double total = 0.0;
        for (double c : stepCounts) total += c;
        if (total > 0.0)
            for (size_t i = 0; i < stepCounts.size(); ++i)
                s.stepWeight[i] = 0.35 * s.stepWeight[i] + 0.65 * (stepCounts[i] / total) * 12.0;

        if (hi - lo < 4)
        {
            const int grow = (4 - (hi - lo) + 1) / 2;
            lo -= grow;
            hi += grow;
        }
        s.lowDegree = lo;
        s.highDegree = hi;

        // rhythm: the file's first bar, and its last bar as the answer
        const int bars = std::max (1, p.bars);
        for (auto& note : p.notes)
        {
            if (topAt.count (note.start) == 0 || offsetToDegree (note.offset, sc, key) != topAt[note.start])
                continue;
            auto r = note;
            const int bar = note.start / kTicksPerBar;
            r.start -= bar * kTicksPerBar;
            r.end = std::min (r.end - bar * kTicksPerBar, kTicksPerBar);
            if (r.end <= r.start)
                continue;
            if (bar == 0)
                s.rhythmA.push_back (r);
            else if (bars > 1 && bar == bars - 1)
                s.rhythmB.push_back (r);
        }
        return s;
    }

    //==============================================================================
    // Progressions
    //==============================================================================
    const std::vector<std::string>& progressionNames()
    {
        static const std::vector<std::string> names
        {
            "Auto", "1 - 6 - 3 - 7", "1 - 6 - 7 - 1", "6 - 7 - 1 - 1", "1 - 4 - 6 - 7",
            "1 - 7 - 6 - 7", "1 - 5 - 6 - 4", "6 - 4 - 1 - 5", "1 - 4 - 1 - 5", "1 (no change)"
        };
        return names;
    }

    std::vector<int> progressionDegrees (int index, const ScaleContext& sc, int key)
    {
        switch (index)
        {
            case 1: return { 0, 5, 2, 6 };
            case 2: return { 0, 5, 6, 0 };
            case 3: return { 5, 6, 0, 0 };
            case 4: return { 0, 3, 5, 6 };
            case 5: return { 0, 6, 5, 6 };
            case 6: return { 0, 4, 5, 3 };
            case 7: return { 5, 3, 0, 4 };
            case 8: return { 0, 3, 0, 4 };
            case 9: return { 0 };
            case 0:
            default:
                // the classic trance progressions: Am F C G in minor, F G Am Am in major keys
                return isMinor (sc, key) ? std::vector<int> { 0, 5, 2, 6 } : std::vector<int> { 5, 3, 0, 4 };
        }
    }

    std::string chordName (int degree, const ScaleContext& sc, int key)
    {
        const int root = key + degreeToOffset (degree, sc, key);
        const int third = key + degreeToOffset (degree + 2, sc, key) - root;
        const int fifth = key + degreeToOffset (degree + 4, sc, key) - root;
        std::string name = keyName (floorMod (root, 12));
        if (third == 3 && fifth == 6)      name += "dim";
        else if (third == 4 && fifth == 8) name += "aug";
        else if (third == 3)               name += "m";
        else if (third == 2 || third == 5) name += "sus";
        return name;
    }

    //==============================================================================
    // Melody Maker
    //==============================================================================
    namespace
    {
        std::vector<int> formFor (int bars)
        {
            switch (bars)
            {
                case 1:  return { 0 };
                case 2:  return { 0, 1 };
                case 4:  return { 0, 0, 0, 1 };
                default: return { 0, 0, 0, 1, 0, 0, 0, 2 };   // A A A B A A A C
            }
        }

        struct Ranges { int lo, hi; };

        Ranges rangeFor (const MelodySettings& m, const MelodyStyle& style, int n)
        {
            if (style.fromFile)
                return { style.lowDegree, std::max (style.lowDegree + 4, style.highDegree) };
            const int span = std::max (4, (int) std::lround (std::clamp (m.range, 5, 36) * n / 12.0));
            const int lo = -span / 4;
            return { lo, lo + span };
        }

        /** Writes the notes for one bar; bar rhythms are relative to the bar start. */
        void writeBars (const std::vector<std::vector<PatternNote>>& rhythms, const std::vector<int>& form,
                        const std::vector<int>& chords, const MelodySettings& m, const MelodyStyle& style,
                        const ScaleContext& sc, Rng& rng, std::vector<std::vector<int>>& degreesOut,
                        const std::vector<int>* fixedHook = nullptr, const Ranges* rangeOverride = nullptr)
        {
            const int n = scaleSize (sc);
            auto r = rangeOverride != nullptr ? *rangeOverride : rangeFor (m, style, n);
            const bool useFixed = fixedHook != nullptr && ! fixedHook->empty() && fixedHook->size() == rhythms[0].size();
            if (useFixed)
                for (int d : *fixedHook)
                {
                    r.lo = std::min (r.lo, d);
                    r.hi = std::max (r.hi, d);
                }
            Writer w { sc, style, n, r.lo, r.hi, rng };

            const int bars = (int) form.size();
            const bool sequence = rng.chance (0.5);
            const bool pedal = ! style.fromFile && ! rhythms[0].empty() && rhythms[0].size() >= 10 && rng.chance (0.3);
            const int root0 = chords[0];

            // the hook over the first chord: start on a chord tone in the middle of the range
            std::vector<std::pair<int, double>> starts;
            for (int d = r.lo; d <= r.hi; ++d)
                if (isChordTone (d, root0, n))
                    starts.push_back ({ d, (d >= 0 && d <= n) ? 3.0 * w.degreeWeight (d) : 0.6 });
            const int hookStart = useFixed ? fixedHook->front() : (starts.empty() ? 0 : pickWeighted (starts, rng));
            const auto hook = useFixed ? *fixedHook : w.line (rhythms[0], root0, hookStart, hookStart, pedal);

            degreesOut.assign ((size_t) bars, {});
            for (int b = 0; b < bars; ++b)
            {
                const int chord = chords[(size_t) b % chords.size()];
                const auto& rhythm = rhythms[(size_t) b];
                if (form[(size_t) b] == 0 && rhythm.size() == hook.size())
                {
                    degreesOut[(size_t) b] = fitHook (hook, rhythm, root0, chord, sequence, n, r.lo, r.hi);
                    continue;
                }

                // answer bar: keep the hook where the rhythm matches, then write towards the
                // hook's first note (so the loop flows), or home to the root at the phrase end
                const bool ending = form[(size_t) b] == 2 || (b == bars - 1 && bars >= 4 && rng.chance (0.35));
                const int home = ending ? nearestChordTone (0, chords[0], n, r.lo, r.hi) : hook.front();
                const auto fitted = fitHook (hook, rhythms[0], root0, chord, sequence, n, r.lo, r.hi);

                std::vector<int> out;
                int prev = fitted.empty() ? hookStart : fitted.front();
                for (size_t i = 0; i < rhythm.size(); ++i)
                {
                    const int pos = (rhythm[i].start / kStep) % 16;
                    int d = -1000;
                    for (size_t k = 0; k < rhythms[0].size(); ++k)
                        if (rhythms[0][k].start == rhythm[i].start && pos < 8)
                            d = fitted[k];
                    if (d == -1000)
                    {
                        const bool last = i + 1 == rhythm.size();
                        const bool anchor = isAnchor (rhythm, i);
                        d = last ? nearestChordTone (home, chord, n, r.lo, r.hi) : w.next (prev, chord, anchor, home);
                        if (last && ! ending && ! anchor && std::abs (d - home) > 2)
                            d = std::clamp (home + (d < home ? -1 : 1), r.lo, r.hi);   // lead into the hook by step
                    }
                    out.push_back (d);
                    prev = d;
                }
                degreesOut[(size_t) b] = out;
            }
        }
    }

    Pattern makeMelody (int bars, const MelodySettings& m, const MelodyStyle& style, const ScaleContext& sc, int key, Rng& rng)
    {
        bars = legalBars (bars);
        for (int i = 0; i < 3; ++i)
            rng.nextInt();   // small seeds give tiny first values in this generator
        const auto form = formFor (bars);
        const auto chords = progressionDegrees (m.progression, sc, key);

        // rhythms per bar
        std::vector<PatternNote> a = style.rhythmA.empty() ? templateBar (m.density, m.gate, rng) : style.rhythmA;
        if (a.empty())
            a.push_back ({ 0, 2 * kStep, 0, 112 });
        std::vector<std::vector<PatternNote>> rhythms;
        for (int b = 0; b < bars; ++b)
        {
            if (form[(size_t) b] == 0)
                rhythms.push_back (a);
            else if (style.fromFile && ! style.rhythmB.empty())
                rhythms.push_back (style.rhythmB);
            else
                rhythms.push_back (answerBar (a, m.density, m.gate, form[(size_t) b] == 2, rng));
        }

        std::vector<std::vector<int>> degrees;
        writeBars (rhythms, form, chords, m, style, sc, rng, degrees);

        Pattern p;
        p.name = style.fromFile ? "File Melody" : "Trance Melody";
        p.bars = bars;
        for (int b = 0; b < bars; ++b)
            for (size_t i = 0; i < rhythms[(size_t) b].size(); ++i)
            {
                auto note = rhythms[(size_t) b][i];
                note.start += b * kTicksPerBar;
                note.end = std::min (note.end + b * kTicksPerBar, (b + 1) * kTicksPerBar);
                note.offset = std::clamp (degreeToOffset (degrees[(size_t) b][i], sc, key), -kMaxOffset, kMaxOffset);
                if (note.end > note.start)
                    p.notes.push_back (note);
            }
        p.sort();
        return p;
    }

    void melodiseNotes (Pattern& p, const MelodySettings& m, const MelodyStyle& style, const ScaleContext& sc, int key, Rng& rng,
                        bool keepFirstBar, bool topOnly)
    {
        if (p.notes.empty())
            return;
        for (int i = 0; i < 3; ++i)
            rng.nextInt();
        p.sort();
        const int bars = std::max (1, p.bars);
        const int n = scaleSize (sc);
        const auto chords = progressionDegrees (m.progression, sc, key);

        // one melody note per onset; stacked notes become chord tones below it
        std::vector<std::vector<PatternNote>> rhythms ((size_t) bars);
        std::vector<std::vector<int>> onsetsOfBar ((size_t) bars);
        for (auto& note : p.notes)
        {
            const int bar = std::min (bars - 1, note.start / kTicksPerBar);
            auto& on = onsetsOfBar[(size_t) bar];
            if (on.empty() || on.back() != note.start)
            {
                on.push_back (note.start);
                auto r = note;
                r.start -= bar * kTicksPerBar;
                r.end -= bar * kTicksPerBar;
                rhythms[(size_t) bar].push_back (r);
            }
        }

        // bars with the same rhythm as bar 1 repeat the hook; the last bar answers
        std::vector<int> form ((size_t) bars, 1);
        auto positions = [] (const std::vector<PatternNote>& r)
        {
            std::vector<int> v;
            for (auto& x : r) v.push_back (x.start);
            return v;
        };
        for (int b = 0; b < bars; ++b)
            form[(size_t) b] = (b == 0 || positions (rhythms[(size_t) b]) == positions (rhythms[0])) && ! (bars >= 4 && b == bars - 1) ? 0 : 1;
        if (bars == 8)
            form[7] = 2;
        if (rhythms[0].empty())
            rhythms[0].push_back ({ 0, kStep, 0, 100 });

        // Variation: bar 1 stays exactly as it is and becomes the hook for the other bars
        keepFirstBar = keepFirstBar && bars > 1;
        std::vector<int> hook;
        if (keepFirstBar)
        {
            std::map<int, int> top;
            for (auto& note : p.notes)
                if (note.start < kTicksPerBar)
                {
                    const int d = offsetToDegree (note.offset, sc, key);
                    auto it = top.find (note.start);
                    if (it == top.end() || d > it->second)
                        top[note.start] = d;
                }
            for (auto& r : rhythms[0])
                hook.push_back (top.count (r.start) ? top[r.start] : 0);
        }

        // Stay in the pattern's register: the melody starts at the pattern's lowest note and goes up
        // as far as Range allows (or as high as the pattern already goes).
        const auto original = p.notes;
        int lowDeg = 1000, highDeg = -1000, lowOffset = 1000;
        for (auto& note : p.notes)
        {
            const int d = offsetToDegree (note.offset, sc, key);
            lowDeg = std::min (lowDeg, d);
            highDeg = std::max (highDeg, d);
            lowOffset = std::min (lowOffset, note.offset);
        }

        // Top notes only: the bottom notes (the pattern's lowest note, and the lower notes of
        // stacked chords) stay; only the notes above them get the new melody.
        std::vector<bool> keep (p.notes.size(), false);
        if (topOnly)
        {
            std::map<int, int> topIndex;   // start -> index of the highest note there
            for (int i = 0; i < (int) p.notes.size(); ++i)
            {
                auto it = topIndex.find (p.notes[(size_t) i].start);
                if (it == topIndex.end() || p.notes[(size_t) i].offset >= p.notes[(size_t) it->second].offset)
                    topIndex[p.notes[(size_t) i].start] = i;
            }
            bool anyFree = false;
            for (int i = 0; i < (int) p.notes.size(); ++i)
            {
                const bool isTop = topIndex[p.notes[(size_t) i].start] == i;
                keep[(size_t) i] = ! isTop || p.notes[(size_t) i].offset == lowOffset;
                anyFree = anyFree || ! keep[(size_t) i];
            }
            if (! anyFree)
            {
                // every note is on the bottom note (e.g. a 16ths arp on the root): the notes on
                // the beat stay as a pedal, the ones in between become the melody
                for (int i = 0; i < (int) p.notes.size(); ++i)
                    keep[(size_t) i] = p.notes[(size_t) i].start % kPhrasePpq == 0;
                bool free = false;
                for (bool k : keep) free = free || ! k;
                if (! free)
                    for (int i = 0; i < (int) p.notes.size(); ++i)
                        keep[(size_t) i] = p.notes[(size_t) i].start % kTicksPerBar == 0;
            }
        }

        const auto def = rangeFor (m, style, n);
        Ranges range;
        range.lo = topOnly ? lowDeg + 1 : lowDeg;
        range.hi = std::max ({ highDeg, range.lo + (def.hi - def.lo), range.lo + 4 });

        std::vector<std::vector<int>> degrees;
        writeBars (rhythms, form, chords, m, style, sc, rng, degrees, keepFirstBar ? &hook : nullptr, &range);

        // write back: the top note of each onset gets the melody, the others stack below it
        std::map<int, int> melodyAt;
        for (int b = keepFirstBar ? 1 : 0; b < bars; ++b)
            for (size_t i = 0; i < rhythms[(size_t) b].size() && i < degrees[(size_t) b].size(); ++i)
                melodyAt[rhythms[(size_t) b][i].start + b * kTicksPerBar] = degrees[(size_t) b][i];

        std::map<int, int> stackCount;
        for (int i = (int) p.notes.size() - 1; i >= 0; --i)
        {
            auto& note = p.notes[(size_t) i];
            auto it = melodyAt.find (note.start);
            if (it == melodyAt.end())
                continue;
            const int below = stackCount[note.start]++;
            const int bar = std::min (bars - 1, note.start / kTicksPerBar);
            const int chord = chords[(size_t) bar % chords.size()];
            int d = it->second;
            for (int k = 0; k < below; ++k)
            {
                --d;
                while (! isChordTone (d, chord, n)) --d;
            }
            while (d < range.lo)   // stacked notes never go below the pattern's bottom note
                d += n;
            note.offset = std::clamp (degreeToOffset (d, sc, key), -kMaxOffset, kMaxOffset);
        }

        // the kept notes go back exactly as they were (p.notes is still in its original order)
        for (size_t i = 0; i < p.notes.size(); ++i)
            if (keep[i])
                p.notes[i].offset = original[i].offset;

        // a new top note always sits above the notes it is stacked on
        if (topOnly)
        {
            std::map<int, int> highestKept;
            for (size_t i = 0; i < p.notes.size(); ++i)
                if (keep[i])
                {
                    auto it = highestKept.find (p.notes[i].start);
                    if (it == highestKept.end() || p.notes[i].offset > it->second)
                        highestKept[p.notes[i].start] = p.notes[i].offset;
                }
            for (size_t i = 0; i < p.notes.size(); ++i)
            {
                auto it = highestKept.find (p.notes[i].start);
                if (keep[i] || it == highestKept.end())
                    continue;
                while (p.notes[i].offset <= it->second && p.notes[i].offset + 12 <= kMaxOffset)
                    p.notes[i].offset += 12;
            }
        }
        p.sort();
    }

    void rhythmiseMelody (Pattern& p, const MelodySettings& m, const MelodyStyle& style, Rng& rng)
    {
        if (p.notes.empty())
            return;
        for (int i = 0; i < 3; ++i)
            rng.nextInt();
        p.sort();
        const int bars = std::max (1, p.bars);

        // the melody, in order: the notes of each onset (a chord stays a chord)
        struct Onset { int start; std::vector<int> offsets; };
        std::vector<Onset> melody;
        for (auto& note : p.notes)
        {
            if (melody.empty() || melody.back().start != note.start)
                melody.push_back ({ note.start, {} });
            melody.back().offsets.push_back (note.offset);
        }

        // a new rhythm: hook, repeats and answer, like New Melody
        const auto form = formFor (bars);
        std::vector<PatternNote> a = style.rhythmA.empty() ? templateBar (m.density, m.gate, rng) : style.rhythmA;
        if (a.empty())
            a.push_back ({ 0, 2 * kStep, 0, 112 });
        std::vector<std::vector<PatternNote>> rhythms;
        for (int b = 0; b < bars; ++b)
        {
            if (form[(size_t) b] == 0)
                rhythms.push_back (a);
            else if (style.fromFile && ! style.rhythmB.empty())
                rhythms.push_back (style.rhythmB);
            else
                rhythms.push_back (answerBar (a, m.density, m.gate, form[(size_t) b] == 2, rng));
        }

        // each bar's notes go to that bar's new rhythm in the same order, spread evenly
        Pattern out;
        out.name = p.name;
        out.bars = bars;
        for (int b = 0; b < bars; ++b)
        {
            std::vector<const Onset*> old;
            for (auto& o : melody)
                if (o.start / kTicksPerBar == b)
                    old.push_back (&o);
            if (old.empty())
                for (auto& o : melody)
                    old.push_back (&o);

            const auto& rhythm = rhythms[(size_t) b];
            for (size_t i = 0; i < rhythm.size(); ++i)
            {
                const auto* src = old[std::min (old.size() - 1, i * old.size() / std::max<size_t> (1, rhythm.size()))];
                for (int offset : src->offsets)
                {
                    auto note = rhythm[i];
                    note.start += b * kTicksPerBar;
                    note.end = std::min (note.end + b * kTicksPerBar, (b + 1) * kTicksPerBar);
                    note.offset = offset;
                    if (note.end > note.start)
                        out.notes.push_back (note);
                }
            }
        }
        out.sort();
        p = out;
    }
}
