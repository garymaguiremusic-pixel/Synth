#include "mm/Pattern.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace mm
{
    void Pattern::sort()
    {
        std::stable_sort (notes.begin(), notes.end(), [] (const PatternNote& a, const PatternNote& b)
        {
            return a.start != b.start ? a.start < b.start : a.offset < b.offset;
        });
    }

    const char* arpOrderName (ArpOrder o)
    {
        switch (o)
        {
            case ArpOrder::Up:     return "Up";
            case ArpOrder::Down:   return "Down";
            case ArpOrder::UpDown: return "Up & Down";
            case ArpOrder::DownUp: return "Down & Up";
            case ArpOrder::Melody:
            default:               return "Melody";
        }
    }

    std::vector<int> arpSequence (int n, ArpOrder order)
    {
        std::vector<int> seq;
        if (n <= 0)
            return seq;

        std::vector<int> up, down;
        for (int i = 0; i < n; ++i)    up.push_back (i);
        for (int i = n - 1; i >= 0; --i) down.push_back (i);

        switch (order)
        {
            case ArpOrder::Melody: return { 0 };
            case ArpOrder::Up:   return up;
            case ArpOrder::Down: return down;
            case ArpOrder::UpDown:
                seq = up;
                for (int i = n - 2; i >= 1; --i) seq.push_back (i);
                return seq;
            case ArpOrder::DownUp:
            default:
                seq = down;
                for (int i = 1; i <= n - 2; ++i) seq.push_back (i);
                return seq;
        }
    }

    //==============================================================================
    int stepOffset (int offset, int direction, const ScaleContext* scale, int key)
    {
        const int dir = direction >= 0 ? 1 : -1;
        int o = offset + dir;
        if (scale != nullptr)
            while (std::abs (o) <= kMaxOffset && ! scale->inScale (key + o))
                o += dir;
        return std::clamp (o, -kMaxOffset, kMaxOffset);
    }

    int legalBars (int bars)
    {
        if (bars <= 1) return 1;
        if (bars <= 2) return 2;
        if (bars <= 4) return 4;
        return 8;
    }

    void resizePattern (Pattern& p, int bars)
    {
        const int oldBars = std::max (1, p.bars);
        p.bars = legalBars (bars);
        if (p.bars > oldBars)
        {
            const auto source = p.notes;
            for (int b = oldBars; b < p.bars; ++b)
            {
                const int from = (b % oldBars) * kTicksPerBar;
                const int shift = (b - b % oldBars) * kTicksPerBar;
                for (auto n : source)
                {
                    if (n.start < from || n.start >= from + kTicksPerBar)
                        continue;
                    n.start += shift;
                    n.end += shift;
                    p.notes.push_back (n);
                }
            }
        }
        for (auto& n : p.notes)
            n.end = std::min (n.end, p.lengthTicks());
        p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&p] (const PatternNote& n)
        {
            return n.start >= p.lengthTicks() || n.end <= n.start;
        }), p.notes.end());
        p.sort();
    }

    //==============================================================================
    void shuffleNotes (Pattern& p, const std::vector<int>& pool, Rng& rng)
    {
        if (pool.empty() || p.notes.empty())
            return;
        p.sort();

        std::vector<int> bag;
        while (bag.size() < p.notes.size())
            bag.insert (bag.end(), pool.begin(), pool.end());
        bag.resize (p.notes.size());

        std::vector<int> before;
        for (auto& n : p.notes)
            before.push_back (n.offset);

        // Fisher-Yates; try a few times to get a different order than before
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            for (int i = (int) bag.size() - 1; i > 0; --i)
                std::swap (bag[(size_t) i], bag[(size_t) rng.nextInt (i + 1)]);
            if (bag != before)
                break;
        }
        for (size_t i = 0; i < p.notes.size(); ++i)
            p.notes[i].offset = bag[i];
    }

    //==============================================================================
    Pattern patternFromMidi (const MidiClip& clip, int key, const std::string& name, std::vector<int>* fileNotes)
    {
        const auto c = clip.rescaled (kPhrasePpq);
        Pattern p;
        p.name = name;
        if (c.notes.empty())
            return p;

        // start at the bar of the first note
        double first = c.notes.front().start;
        int lowest = 127;
        for (auto& n : c.notes)
        {
            first = std::min (first, n.start);
            lowest = std::min (lowest, n.note);
        }
        const double origin = std::floor (first / kTicksPerBar) * kTicksPerBar;
        const int reference = 60 + std::clamp (key, 0, 11);   // the key's root in octave 3 (C3 = 60)
        // a file far above or below is moved by whole octaves so it fits the offset range
        int octaveShift = 0;
        {
            int lo = 127, hi = 0;
            for (auto& n : c.notes) { lo = std::min (lo, n.note); hi = std::max (hi, n.note); }
            while (hi + octaveShift - reference > kMaxOffset) octaveShift -= 12;
            while (lo + octaveShift - reference < -kMaxOffset) octaveShift += 12;
        }

        double end = 0.0;
        for (auto& n : c.notes)
        {
            const double s = n.start - origin, e = n.end - origin;
            if (s >= kMaxBars * kTicksPerBar)
                continue;
            PatternNote pn;
            pn.start = (int) std::lround (s);
            pn.end = std::max (pn.start + 1, (int) std::lround (std::min (e, (double) kMaxBars * kTicksPerBar)));
            pn.offset = std::clamp (n.note + octaveShift - reference, -kMaxOffset, kMaxOffset);
            pn.velocity = std::clamp (n.vel, 1, 127);
            p.notes.push_back (pn);
            end = std::max (end, (double) pn.end);
        }
        p.bars = legalBars ((int) std::ceil (end / kTicksPerBar - 1.0e-9));
        for (auto& n : p.notes)
            n.end = std::min (n.end, p.lengthTicks());
        p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&p] (const PatternNote& n)
        {
            return n.start >= p.lengthTicks() || n.end <= n.start;
        }), p.notes.end());
        p.sort();

        if (fileNotes != nullptr)
        {
            fileNotes->clear();
            for (auto& n : p.notes)
                fileNotes->push_back (n.offset);
        }
        return p;
    }

    void detectKey (const MidiClip& clip, int& key, int& scaleIndex)
    {
        std::array<double, 12> weight {};
        for (auto& n : clip.notes)
            weight[(size_t) floorMod (n.note, 12)] += std::max (1.0, n.end - n.start);
        int firstPc = clip.notes.empty() ? 0 : floorMod (clip.notes.front().note, 12);
        int lastPc = clip.notes.empty() ? 0 : floorMod (clip.notes.back().note, 12);

        double best = -1.0;
        key = 0;
        scaleIndex = NaturalMinor;
        for (int scale : { (int) NaturalMinor, (int) Major })
            for (int k = 0; k < 12; ++k)
            {
                const auto sc = ScaleContext::make (scale, k);
                double score = 0.0;
                for (int pc = 0; pc < 12; ++pc)
                    score += sc.inScale (pc) ? weight[(size_t) pc] : -2.0 * weight[(size_t) pc];
                // the tonic: weighted, and where melodies often start and end
                score += 0.5 * weight[(size_t) k];
                if (firstPc == k) score += 0.05 * score;
                if (lastPc == k)  score += 0.05 * score;
                if (scale == NaturalMinor) score *= 1.001;   // trance: minor wins a tie
                if (score > best)
                {
                    best = score;
                    key = k;
                    scaleIndex = scale;
                }
            }
    }

    Pattern patternFromPhrase (const Phrase& ph, const ScaleContext* scale, int key)
    {
        Pattern p;
        p.name = ph.name;
        const int length = ph.lengthTicks();
        p.bars = legalBars ((length + kTicksPerBar - 1) / kTicksPerBar);
        for (auto& n : ph.notes)
        {
            if (n.start >= p.lengthTicks())
                continue;
            PatternNote pn;
            pn.start = n.start;
            pn.end = std::min (std::max (n.end, n.start + 1), p.lengthTicks());
            int rowOffset = 0;
            if (scale != nullptr)
            {
                const int root = scale->stepsAbove (60 + key, 0);
                rowOffset = scale->stepsAbove (60 + key, 2 * n.row()) - root;
            }
            pn.offset = std::clamp ((n.lane() - kCentreLane) * 12 + rowOffset, -kMaxOffset, kMaxOffset);
            pn.velocity = std::clamp (n.velocity, 1, 127);
            p.notes.push_back (pn);
        }
        p.sort();
        return p;
    }
}
