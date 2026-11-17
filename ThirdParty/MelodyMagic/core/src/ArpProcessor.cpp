#include "mm/ArpProcessor.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>

namespace mm
{
    PlayPattern makePlayPattern (const Pattern& pattern, const Groove* groove, double startStrength, double endStrength, bool link)
    {
        auto p = pattern;
        p.sort();

        PlayPattern pp;
        pp.clip.length = p.lengthTicks();
        for (int i = 0; i < (int) p.notes.size(); ++i)
        {
            const auto& n = p.notes[(size_t) i];
            PlayNote pn;
            pn.id = i;
            pn.vel = n.velocity;
            pn.start = n.start;
            pn.end = n.end;
            quantizeNote (pn.start, pn.end, pp.clip.length, groove, startStrength, endStrength, link);
            if (pn.start >= pp.clip.length || pn.end <= pn.start)
                continue;
            pp.clip.notes.push_back (pn);
        }
        pp.offset.resize (p.notes.size());
        pp.step.resize (p.notes.size());
        for (int i = 0; i < (int) p.notes.size(); ++i)
        {
            pp.offset[(size_t) i] = p.notes[(size_t) i].offset;
            pp.step[(size_t) i] = i;   // time order
        }
        return pp;
    }

    //==============================================================================
    void OutputTracker::record (const Event& e) noexcept
    {
        if (e.note < 0 || e.note > 127 || e.channel < 1 || e.channel > 16)
            return;
        auto& c = counts[(size_t) e.channel - 1][(size_t) e.note];
        if (e.type == Event::On && c < 255)  ++c;
        if (e.type == Event::Off && c > 0)   --c;
    }

    void OutputTracker::flush (int time, std::vector<Event>& out)
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                for (auto& c = counts[(size_t) ch][(size_t) n]; c > 0; --c)
                    out.push_back (Event::off (time, n, ch + 1));
    }

    //==============================================================================
    int ArpProcessor::velocityOf (int noteVel, int keyVel, int randomVel, Rng* r)
    {
        int v = noteVel;
        if (randomVel > 0 && r != nullptr)
        {
            const int amount = (int) ((float) std::min (randomVel, 127) * r->nextFloat());
            v += r->nextBool() ? amount : -amount;
        }
        v = std::clamp (v, 1, 127);
        return std::clamp ((int) std::lround (v * std::clamp (keyVel, 1, 127) / 127.0), 1, 127);
    }

    void ArpProcessor::endAll (int time, std::vector<Event>& out)
    {
        for (auto& s : sounding)
            out.push_back (Event::off (time, s.pitch, s.channel));
        sounding.clear();
    }

    void ArpProcessor::panic (int time, std::vector<Event>& out)
    {
        std::vector<Event> discard;
        clip.stopAll (time, discard);
        sounding.clear();
        tracker.flush (time, out);
    }

    void ArpProcessor::process (const TransportInfo& t, int numSamples, const ArpSettings& s, const PlayPattern* pattern,
                                const std::vector<Event>& hostIn, std::vector<Event>& out)
    {
        const size_t firstOut = out.size();

        // Bypass: host notes pass straight through; nothing the arp started is left hanging.
        if (s.bypassed)
        {
            if (! wasBypassed)
            {
                std::vector<Event> discard;
                clip.stopAll (0, discard);
                sounding.clear();
                chord.clear();
                heldKeys = 0;
                tracker.flush (0, out);
                wasBypassed = true;
            }
            for (auto& e : hostIn)
            {
                out.push_back (e);
                tracker.record (e);
            }
            position = -1.0;
            wasPlaying = t.playing;
            return;
        }
        if (wasBypassed)
        {
            tracker.flush (0, out);
            wasBypassed = false;
            clip.resetPosition();
        }

        // 1. pattern events for this block
        std::vector<Event> clipEvents;
        if (pattern != lastPattern)
        {
            clip.stopAll (0, clipEvents);
            clip.resetPosition();
            lastPattern = pattern;
        }

        const double bpm = t.bpm > 1.0 ? t.bpm : 120.0;
        const double speed = std::max (0.01, s.speed);
        const double tps = bpm / 60.0 * kPhrasePpq * speed / std::max (1.0, t.sampleRate);
        const double length = pattern != nullptr ? pattern->clip.length : 0.0;

        if (t.playing && t.hasPosition)
        {
            if (! wasPlaying)
            {
                clip.stopAll (0, clipEvents);
                clip.resetPosition();
            }
            const double startTick = t.ppqPosition * kPhrasePpq * speed;
            if (pattern != nullptr)
                clip.process (pattern->clip, startTick, tps, numSamples, 0, clipEvents);
            position = length > 0.0 ? std::fmod (std::fmod (startTick, length) + length, length) : -1.0;
        }
        else
        {
            if (wasPlaying)
            {
                clip.stopAll (0, clipEvents);
                clip.resetPosition();
                internalTick = 0.0;
                internalRunning = false;
            }

            // the first key after all keys were released restarts the pattern
            std::vector<int> restarts;
            int held = heldKeys;
            for (auto& e : hostIn)
            {
                if (e.type == Event::On)
                {
                    if (held == 0)
                        restarts.push_back (std::clamp (e.time, 0, numSamples));
                    ++held;
                }
                else if (e.type == Event::Off)
                {
                    held = std::max (0, held - 1);
                }
            }

            int pos = 0;
            for (int r : restarts)
            {
                if (internalRunning && pattern != nullptr && r > pos)
                    clip.process (pattern->clip, internalTick, tps, r - pos, pos, clipEvents);
                clip.stopAll (r, clipEvents);
                clip.resetPosition();
                internalTick = 0.0;
                internalRunning = true;
                pos = r;
            }
            if (internalRunning && pattern != nullptr && numSamples > pos)
                clip.process (pattern->clip, internalTick, tps, numSamples - pos, pos, clipEvents);
            if (internalRunning)
                internalTick += (numSamples - pos) * tps;
            position = internalRunning && held > 0 && length > 0.0 ? std::fmod (internalTick, length) : -1.0;
        }
        wasPlaying = t.playing;

        // 2. merge (host events first at the same time) and play
        std::vector<Event> events;
        events.reserve (hostIn.size() + clipEvents.size());
        for (auto e : hostIn)      { e.fromPhrase = false; events.push_back (e); }
        for (auto& e : clipEvents) events.push_back (e);
        std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });

        for (auto& e : events)
        {
            if (! e.fromPhrase)
            {
                if (e.type == Event::On)
                {
                    HeldNote h { e.note, e.vel, e.channel };
                    auto it = std::upper_bound (chord.begin(), chord.end(), h, [] (const HeldNote& a, const HeldNote& b) { return a.note < b.note; });
                    chord.insert (it, h);
                    ++heldKeys;
                }
                else if (e.type == Event::Off)
                {
                    auto it = std::find_if (chord.begin(), chord.end(), [&] (const HeldNote& h) { return h.note == e.note && h.channel == e.channel; });
                    if (it == chord.end())
                        it = std::find_if (chord.begin(), chord.end(), [&] (const HeldNote& h) { return h.note == e.note; });
                    if (it != chord.end())
                        chord.erase (it);
                    heldKeys = std::max (0, heldKeys - 1);
                    if (chord.empty() && ! (s.autoPlay && t.playing))
                        endAll (e.time, out);
                }
                continue;
            }

            const int id = e.note;
            auto playing = std::find_if (sounding.begin(), sounding.end(), [id] (const Sounding& x) { return x.id == id; });
            if (playing != sounding.end())
            {
                out.push_back (Event::off (e.time, playing->pitch, playing->channel));
                sounding.erase (playing);
            }
            // Auto Play: with nothing held and the host playing, the key's root in octave 3 plays
            const std::vector<HeldNote> ghost { HeldNote { 60 + std::clamp (s.key, 0, 11), 100, 1 } };
            const auto& keys = (chord.empty() && s.autoPlay && t.playing) ? ghost : chord;
            if (e.type != Event::On || keys.empty() || pattern == nullptr || id < 0 || id >= (int) pattern->offset.size())
                continue;

            int pitch, vel;
            const HeldNote* key;
            if (s.order == ArpOrder::Melody)
            {
                // the pattern's own notes, from the Key's root in the octave of the lowest held key
                key = &keys.front();
                pitch = 12 * floorDiv (key->note, 12) + std::clamp (s.key, 0, 11) + pattern->offset[(size_t) id];
                vel = velocityOf (e.vel, 127, s.randomVel, rng);   // the file's / pattern's velocity
            }
            else
            {
                const auto seq = arpSequence ((int) keys.size(), s.order);
                key = &keys[(size_t) seq[(size_t) pattern->step[(size_t) id] % seq.size()]];
                pitch = key->note + pattern->offset[(size_t) id];
                vel = velocityOf (e.vel, key->vel, s.randomVel, rng);
            }
            if (s.scaleLock)
                pitch = s.scale.quantise (pitch);
            if (pitch < 0 || pitch > 127)
                continue;

            out.push_back (Event::on (e.time, pitch, vel, key->channel));
            sounding.push_back ({ id, pitch, key->channel });
        }

        std::stable_sort (out.begin() + (std::ptrdiff_t) firstOut, out.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });
        for (size_t i = firstOut; i < out.size(); ++i)
            tracker.record (out[i]);
    }

    //==============================================================================
    std::vector<TimedNote> renderPattern (const PlayPattern& pattern, const std::vector<int>& chordNotes, int keyVelocity,
                                          const ArpSettings& settings, double bpm, double lengthTicks, Rng* rng)
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;
        bpm = bpm > 1.0 ? bpm : 120.0;
        const double samplesPerTick = sampleRate * 60.0 / (bpm * kPhrasePpq);
        const auto total = (long long) std::llround (lengthTicks * samplesPerTick);

        auto s = settings;
        s.bypassed = false;
        ArpProcessor proc (rng);
        std::vector<std::pair<long long, Event>> produced;
        std::vector<Event> host, out;

        for (long long start = 0; start < total; start += blockSize)
        {
            const int n = (int) std::min<long long> (blockSize, total - start);
            host.clear();
            if (start == 0)
                for (int note : chordNotes)
                    host.push_back (Event::on (0, std::clamp (note, 0, 127), keyVelocity));

            TransportInfo t;
            t.playing = true;
            t.hasPosition = true;
            t.bpm = bpm;
            t.sampleRate = sampleRate;
            t.ppqPosition = (double) start / (samplesPerTick * kPhrasePpq);

            out.clear();
            proc.process (t, n, s, &pattern, host, out);
            for (auto& e : out)
                produced.emplace_back (start + e.time, e);
        }
        std::vector<Event> tail;
        proc.panic (0, tail);
        for (auto& e : tail)
            produced.emplace_back (total, e);

        std::vector<TimedNote> result;
        std::map<int, std::deque<size_t>> open;
        for (auto& [time, e] : produced)
        {
            const int id = e.channel * 128 + e.note;
            const double tick = (double) time / samplesPerTick;
            if (e.type == Event::On)
            {
                TimedNote tn;
                tn.note = e.note;
                tn.vel = e.vel;
                tn.channel = e.channel;
                tn.start = tick;
                tn.end = -1.0;
                open[id].push_back (result.size());
                result.push_back (tn);
            }
            else if (e.type == Event::Off && ! open[id].empty())
            {
                result[open[id].front()].end = tick;
                open[id].pop_front();
            }
        }
        for (auto& n : result)
            if (n.end < 0.0)
                n.end = lengthTicks;
        result.erase (std::remove_if (result.begin(), result.end(), [] (const TimedNote& n) { return n.end <= n.start; }), result.end());
        std::stable_sort (result.begin(), result.end(), [] (const TimedNote& a, const TimedNote& b)
        {
            return a.start != b.start ? a.start < b.start : a.note < b.note;
        });
        return result;
    }
}
