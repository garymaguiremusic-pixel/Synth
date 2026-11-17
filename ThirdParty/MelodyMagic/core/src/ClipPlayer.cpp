#include "mm/ClipPlayer.h"

#include <algorithm>
#include <cmath>

namespace mm
{
    double nearestGridTick (double t, const Groove& g)
    {
        if (g.grid.empty() || g.totalPpq <= 0 || g.globalPpq <= 0)
            return t;

        const double scale = (double) kPhrasePpq / (double) g.globalPpq;
        const double period = g.totalPpq * scale;
        const double base = std::floor (t / period) * period;
        const double local = t - base;

        double best = (g.grid.back() * scale) - period;   // last line of the previous period
        double bestDist = std::abs (local - best);
        auto consider = [&] (double line)
        {
            const double d = std::abs (local - line);
            if (d < bestDist) { bestDist = d; best = line; }
        };
        for (int v : g.grid)
            consider (v * scale);
        consider (g.grid.front() * scale + period);         // first line of the next period
        return base + best;
    }

    double quantizeTick (double t, double strength, const Groove& g)
    {
        if (strength <= 0.0)
            return t;
        return t + std::min (1.0, strength) * (nearestGridTick (t, g) - t);
    }

    void quantizeNote (double& start, double& end, double length, const Groove* g, double startStrength, double endStrength, bool link)
    {
        if (link)
            endStrength = startStrength;
        if (g != nullptr)
        {
            start = quantizeTick (start, startStrength, *g);
            end = quantizeTick (end, endStrength, *g);
        }
        start = std::clamp (start, 0.0, length);
        end = std::min (length, std::max (end, start + 1.0));
    }

    //==============================================================================
    void ClipPlayer::stopAll (int time, std::vector<Event>& out)
    {
        for (auto& s : sounding)
        {
            Event e = Event::off (time, s.id);
            e.fromPhrase = true;
            out.push_back (e);
        }
        sounding.clear();
    }

    void ClipPlayer::process (const PlayClip& pp, double startTick, double tps, int numSamples,
                              int timeOffset, std::vector<Event>& out)
    {
        if (numSamples <= 0 || tps <= 0.0)
            return;

        if (hasExpected && std::abs (startTick - expected) > 1.0)
            stopAll (timeOffset, out);
        expected = startTick + numSamples * tps;
        hasExpected = true;

        const double L = pp.length;
        if (L < 1.0)
        {
            stopAll (timeOffset, out);   // the clip has stopped
            return;
        }

        constexpr double eps = 5.0e-7;
        auto toSample = [&] (double ticksFromBlockStart)
        {
            const int s = (int) std::floor (ticksFromBlockStart / tps + eps);
            return timeOffset + std::clamp (s, 0, numSamples - 1);
        };

        double pos = std::fmod (startTick, L);
        if (pos < 0.0) pos += L;
        if (pos >= L - eps) pos = 0.0;

        double remaining = numSamples * tps, consumed = 0.0;
        struct Pending { double tick; int kind; int index; };   // kind 0 = off, 1 = on
        std::vector<Pending> pending;

        while (remaining > eps)
        {
            const double segLen = std::min (remaining, L - pos);
            const double segEnd = pos + segLen;
            pending.clear();

            for (int i = 0; i < (int) pp.notes.size(); ++i)
            {
                const auto& n = pp.notes[(size_t) i];
                if (n.start >= pos - eps && n.start < segEnd - eps)
                    pending.push_back ({ n.start, 1, i });
                if (n.end < L - eps && n.end >= pos - eps && n.end < segEnd - eps)
                    pending.push_back ({ n.end, 0, i });
            }
            std::stable_sort (pending.begin(), pending.end(), [] (const Pending& a, const Pending& b)
            {
                return a.tick != b.tick ? a.tick < b.tick : a.kind < b.kind;
            });

            for (auto& ev : pending)
            {
                const auto& n = pp.notes[(size_t) ev.index];
                const int time = toSample (consumed + (ev.tick - pos));
                auto it = std::find_if (sounding.begin(), sounding.end(), [&] (const Sounding& s) { return s.index == ev.index; });

                if (ev.kind == 0)
                {
                    if (it != sounding.end())
                    {
                        Event e = Event::off (time, n.id);
                        e.fromPhrase = true;
                        out.push_back (e);
                        sounding.erase (it);
                    }
                }
                else
                {
                    if (it != sounding.end())
                    {
                        Event e = Event::off (time, n.id);
                        e.fromPhrase = true;
                        out.push_back (e);
                        sounding.erase (it);
                    }
                    Event e = Event::on (time, n.id, n.vel);
                    e.fromPhrase = true;
                    out.push_back (e);
                    sounding.push_back ({ ev.index, n.id });
                }
            }

            consumed += segLen;
            remaining -= segLen;

            if (segEnd >= L - eps)
            {
                // loop boundary: anything still sounding ends here (notes ending at the phrase end)
                stopAll (toSample (consumed), out);
                pos = 0.0;
            }
            else
            {
                pos = segEnd;
            }
        }
    }
}
