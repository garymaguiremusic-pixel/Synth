#include "mm/MidiFile.h"
#include "mm/ValueTreeCodec.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>

namespace mm
{
    MidiClip MidiClip::rescaled (int newPpq) const
    {
        MidiClip c = *this;
        c.ppq = newPpq;
        const double f = (double) newPpq / (double) std::max (1, ppq);
        for (auto& n : c.notes)
        {
            n.start *= f;
            n.end *= f;
        }
        return c;
    }

    double MidiClip::lengthTicks() const noexcept
    {
        double end = 0.0;
        for (auto& n : notes)
            end = std::max (end, n.end);
        return end;
    }

    namespace
    {
        std::uint32_t be32 (const std::uint8_t* p) { return ((std::uint32_t) p[0] << 24) | ((std::uint32_t) p[1] << 16) | ((std::uint32_t) p[2] << 8) | p[3]; }
        std::uint16_t be16 (const std::uint8_t* p) { return (std::uint16_t) ((p[0] << 8) | p[1]); }
    }

    bool readMidi (const std::vector<std::uint8_t>& b, MidiClip& clip, std::string* error)
    {
        auto fail = [error] (const char* what) { if (error) *error = what; return false; };

        if (b.size() < 14 || std::string ((const char*) b.data(), 4) != "MThd")
            return fail ("not a standard MIDI file");

        const auto headerLen = be32 (&b[4]);
        const int numTracks = be16 (&b[10]);
        const int division = be16 (&b[12]);
        if (division & 0x8000)
            return fail ("SMPTE time division is not supported");

        clip = {};
        clip.ppq = std::max (1, division);
        bool tempoSet = false;
        size_t p = 8 + headerLen;

        for (int t = 0; t < numTracks && p + 8 <= b.size(); ++t)
        {
            if (std::string ((const char*) &b[p], 4) != "MTrk")
            {
                p += 8 + be32 (&b[p + 4]);   // skip unknown chunk
                continue;
            }
            const size_t end = std::min (b.size(), p + 8 + be32 (&b[p + 4]));
            size_t i = p + 8;
            double tick = 0;
            std::uint8_t status = 0;
            std::map<int, std::deque<std::pair<double, int>>> open;   // ch*128+note -> (start, vel)

            auto varLen = [&]() -> std::uint32_t
            {
                std::uint32_t v = 0;
                std::uint8_t c;
                do
                {
                    if (i >= end) return v;
                    c = b[i++];
                    v = (v << 7) | (c & 0x7f);
                } while (c & 0x80);
                return v;
            };

            while (i < end)
            {
                tick += varLen();
                if (i >= end) break;
                std::uint8_t c = b[i];
                if (c & 0x80) { status = c; ++i; }

                if (status == 0xff)
                {
                    if (i >= end) break;
                    const auto type = b[i++];
                    const auto len = varLen();
                    if (type == 0x51 && len >= 3 && i + 3 <= end && ! tempoSet)
                    {
                        const std::uint32_t us = ((std::uint32_t) b[i] << 16) | ((std::uint32_t) b[i + 1] << 8) | b[i + 2];
                        if (us > 0) { clip.bpm = 60000000.0 / us; tempoSet = true; }
                    }
                    i += len;
                    if (type == 0x2f) break;
                    continue;
                }
                if (status == 0xf0 || status == 0xf7)
                {
                    i += varLen();
                    continue;
                }

                const int hi = status & 0xf0, ch = status & 0x0f;
                if (i >= end) break;
                const int d1 = b[i++];
                int d2 = 0;
                if (hi != 0xc0 && hi != 0xd0)
                {
                    if (i >= end) break;
                    d2 = b[i++];
                }

                if (hi == 0x90 && d2 > 0)
                {
                    open[ch * 128 + d1].emplace_back (tick, d2);
                }
                else if (hi == 0x80 || (hi == 0x90 && d2 == 0))
                {
                    auto& q = open[ch * 128 + d1];
                    if (! q.empty())
                    {
                        TimedNote n;
                        n.note = d1;
                        n.channel = ch + 1;
                        n.vel = q.front().second;
                        n.start = q.front().first;
                        n.end = tick;
                        q.pop_front();
                        clip.notes.push_back (n);
                    }
                }
            }
            p = end;
        }

        std::stable_sort (clip.notes.begin(), clip.notes.end(), [] (const TimedNote& a, const TimedNote& c)
        {
            return a.start != c.start ? a.start < c.start : a.note < c.note;
        });
        return true;
    }

    bool readMidiFile (const std::string& path, MidiClip& clip, std::string* error)
    {
        const auto bytes = vt::readFileBytes (path);
        if (bytes.empty())
        {
            if (error) *error = "cannot read " + path;
            return false;
        }
        return readMidi (bytes, clip, error);
    }

    //==============================================================================
    std::vector<std::uint8_t> writeMidi (const MidiClip& clip, const std::string& trackName)
    {
        struct Ev { long tick; int order; std::uint8_t b0, b1, b2; };
        std::vector<Ev> evs;
        for (auto& n : clip.notes)
        {
            const long s = std::lround (n.start), e = std::max (s + 1, std::lround (n.end));
            const auto ch = (std::uint8_t) (std::clamp (n.channel, 1, 16) - 1);
            evs.push_back ({ s, 1, (std::uint8_t) (0x90 | ch), (std::uint8_t) std::clamp (n.note, 0, 127), (std::uint8_t) std::clamp (n.vel, 1, 127) });
            evs.push_back ({ e, 0, (std::uint8_t) (0x80 | ch), (std::uint8_t) std::clamp (n.note, 0, 127), 0 });
        }
        // offs before ons at the same tick, so repeated notes don't overlap
        std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
        {
            return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
        });

        std::vector<std::uint8_t> trk;
        auto varLen = [&trk] (std::uint32_t v)
        {
            std::uint8_t buf[5];
            int n = 0;
            buf[n++] = (std::uint8_t) (v & 0x7f);
            while ((v >>= 7) > 0)
                buf[n++] = (std::uint8_t) (0x80 | (v & 0x7f));
            while (n > 0)
                trk.push_back (buf[--n]);
        };

        if (! trackName.empty())
        {
            varLen (0);
            trk.insert (trk.end(), { 0xff, 0x03 });
            varLen ((std::uint32_t) trackName.size());
            trk.insert (trk.end(), trackName.begin(), trackName.end());
        }
        const auto us = (std::uint32_t) std::lround (60000000.0 / std::max (1.0, clip.bpm));
        varLen (0);
        trk.insert (trk.end(), { 0xff, 0x51, 0x03, (std::uint8_t) (us >> 16), (std::uint8_t) (us >> 8), (std::uint8_t) us });
        varLen (0);
        trk.insert (trk.end(), { 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08 });

        long last = 0;
        for (auto& e : evs)
        {
            varLen ((std::uint32_t) (e.tick - last));
            last = e.tick;
            trk.insert (trk.end(), { e.b0, e.b1, e.b2 });
        }
        varLen (0);
        trk.insert (trk.end(), { 0xff, 0x2f, 0x00 });

        std::vector<std::uint8_t> out { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1 };
        out.push_back ((std::uint8_t) (clip.ppq >> 8));
        out.push_back ((std::uint8_t) clip.ppq);
        out.insert (out.end(), { 'M', 'T', 'r', 'k' });
        const auto len = (std::uint32_t) trk.size();
        out.insert (out.end(), { (std::uint8_t) (len >> 24), (std::uint8_t) (len >> 16), (std::uint8_t) (len >> 8), (std::uint8_t) len });
        out.insert (out.end(), trk.begin(), trk.end());
        return out;
    }

    bool writeMidiFile (const std::string& path, const MidiClip& clip, const std::string& trackName)
    {
        return vt::writeFileBytes (path, writeMidi (clip, trackName));
    }
}
