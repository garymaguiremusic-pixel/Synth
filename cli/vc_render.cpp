// vc_render: headless, deterministic renderer for VC-Clone.
//
//   vc_render --preset bank.mid --index 5 --notes "60:0.0:2.0:100" --len 4 --sr 44100 --out x.wav
//             [--set "Cutoff=80"] [--set "Filter1 Resonance=100"] [--seed 1] [--block 512]
//             [--bpm 120] [--json patch.json] [--list] [--tables] [--syx probe.syx]
//
// --notes takes "note:start:duration:velocity" items separated by commas; repeatable.
// --cc "time:cc:value" and --bend "time:value14" add controller events.

#include "HostEngine.h"
#include "vc/PresetIO.h"
#include "VirusCLayout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct Event { double time; int type; int a, b; };   // type 0 = on, 1 = off, 2 = cc, 3 = bend

void usage()
{
    std::puts ("usage: vc_render [--preset file.mid|.syx] [--index N] [--notes n:start:dur:vel,...] [--len sec]\n"
               "                 [--sr rate] [--out file.wav] [--set Name=value]... [--seed N] [--block N]\n"
               "                 [--cc t:cc:val]... [--bend t:value14]... [--bpm N] [--json out.json] [--list] [--tables]");
}

bool writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (path, std::ios::binary);
    if (! f) return false;
    const uint32_t frames = (uint32_t) L.size(), dataBytes = frames * 2 * 4;
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (3); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 8); u16 (8); u16 (32);
    f.write ("data", 4); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i) { f.write ((const char*) &L[i], 4); f.write ((const char*) &R[i], 4); }
    return (bool) f;
}

std::vector<std::string> split (const std::string& s, char c)
{
    std::vector<std::string> out;
    std::stringstream ss (s);
    std::string item;
    while (std::getline (ss, item, c)) if (! item.empty()) out.push_back (item);
    return out;
}
} // namespace

int main (int argc, char** argv)
{
    std::string presetPath, outPath, jsonPath, syxOut;
    int index = 0, sr = 44100, block = 512;
    double len = 4.0, bpm = 0.0;
    uint64_t seed = 1;
    bool list = false, showTables = false;
    std::vector<std::string> sets;
    std::vector<Event> events;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string { if (i + 1 >= argc) { usage(); std::exit (2); } return argv[++i]; };
        if (a == "--preset") presetPath = next();
        else if (a == "--index") index = std::atoi (next().c_str());
        else if (a == "--len") len = std::atof (next().c_str());
        else if (a == "--sr") sr = std::atoi (next().c_str());
        else if (a == "--out") outPath = next();
        else if (a == "--json") jsonPath = next();
        else if (a == "--syx") syxOut = next();
        else if (a == "--set") sets.push_back (next());
        else if (a == "--seed") seed = std::strtoull (next().c_str(), nullptr, 10);
        else if (a == "--block") block = std::max (1, std::atoi (next().c_str()));
        else if (a == "--bpm") bpm = std::atof (next().c_str());
        else if (a == "--list") list = true;
        else if (a == "--tables") showTables = true;
        else if (a == "--notes")
        {
            for (auto& item : split (next(), ','))
            {
                auto f = split (item, ':');
                if (f.size() < 3) { std::cerr << "bad note spec: " << item << "\n"; return 2; }
                const int note = std::atoi (f[0].c_str());
                const double start = std::atof (f[1].c_str()), dur = std::atof (f[2].c_str());
                const int vel = f.size() > 3 ? std::atoi (f[3].c_str()) : 100;
                events.push_back ({ start, 0, note, vel });
                events.push_back ({ start + dur, 1, note, 0 });
            }
        }
        else if (a == "--cc")
        {
            auto f = split (next(), ':');
            if (f.size() == 3) events.push_back ({ std::atof (f[0].c_str()), 2, std::atoi (f[1].c_str()), std::atoi (f[2].c_str()) });
        }
        else if (a == "--bend")
        {
            auto f = split (next(), ':');
            if (f.size() == 2) events.push_back ({ std::atof (f[0].c_str()), 3, std::atoi (f[1].c_str()), 0 });
        }
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { std::cerr << "unknown argument " << a << "\n"; usage(); return 2; }
    }

    if (showTables)
    {
        for (auto& t : vc::tables::tableStatus())
            std::printf ("%-18s %-9s %s\n", t.name, t.status == vc::tables::Status::Measured ? "measured" : "GUESSED", t.note);
        if (presetPath.empty() && outPath.empty()) return 0;
    }

    vc::SinglePatch patch = vc::makeInitPatch();
    if (! presetPath.empty())
    {
        auto res = vc::importFile (presetPath);
        for (auto& w : res.warnings)
            std::cerr << "warning: " << (w.patchIndex >= 0 ? "[" + std::to_string (w.patchIndex) + "] " : "") << w.message << "\n";
        if (! res.error.empty() && res.singles.empty()) { std::cerr << "error: " << res.error << "\n"; return 1; }
        if (list)
        {
            for (size_t i = 0; i < res.singles.size(); ++i)
                std::printf ("%3zu  %-10s  %s / %s\n", i, res.singles[i].name().c_str(),
                             vc::valueToText (251, res.singles[i].value (251)).c_str(), vc::valueToText (252, res.singles[i].value (252)).c_str());
            return 0;
        }
        if (index < 0 || index >= (int) res.singles.size()) { std::cerr << "error: index out of range (0.." << res.singles.size() - 1 << ")\n"; return 1; }
        patch = res.singles[(size_t) index];
    }

    for (auto& s : sets)
    {
        const auto eq = s.find ('=');
        if (eq == std::string::npos) { std::cerr << "bad --set " << s << "\n"; return 2; }
        const std::string name = s.substr (0, eq);
        int off = vc::layout::findParam (name);
        if (off < 0 && ! name.empty() && std::isdigit ((unsigned char) name[0])) off = std::atoi (name.c_str());
        if (off < 0 || off > 255) { std::cerr << "unknown parameter " << name << "\n"; return 2; }
        patch.raw[(size_t) off] = (uint8_t) std::clamp (std::atoi (s.c_str() + eq + 1), 0, 127);
    }

    if (! jsonPath.empty())
    {
        std::ofstream (jsonPath) << vc::patchToJson (patch);
        if (outPath.empty()) return 0;
    }
    if (! syxOut.empty())
        vc::writeFile (syxOut, vc::makeSingleDump (patch, 0, 0));
    if (outPath.empty()) return 0;

    vc::HostEngine engine;
    engine.synth.seed (seed);
    engine.prepare (sr, block);
    engine.synth.setPatch (patch.raw.data());
    if (bpm > 0) engine.synth.setHostTempo (bpm);

    std::stable_sort (events.begin(), events.end(), [] (const Event& x, const Event& y) { return x.time < y.time; });
    const long long total = (long long) std::llround (len * sr);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    size_t ev = 0;
    long long pos = 0;
    while (pos < total)
    {
        while (ev < events.size() && (long long) std::llround (events[ev].time * sr) <= pos)
        {
            const auto& e = events[ev++];
            switch (e.type)
            {
                case 0: engine.synth.noteOn (e.a, e.b); break;
                case 1: engine.synth.noteOff (e.a); break;
                case 2: engine.synth.controller (e.a, e.b); break;
                case 3: engine.synth.pitchBend (e.a); break;
            }
        }
        long long end = std::min (total, pos + block);
        if (ev < events.size()) end = std::min (end, std::max (pos + 1, (long long) std::llround (events[ev].time * sr)));
        engine.process (L.data() + pos, R.data() + pos, (int) (end - pos));
        pos = end;
    }

    if (! writeWav (outPath, L, R, sr)) { std::cerr << "could not write " << outPath << "\n"; return 1; }
    float peak = 0.0f;
    for (long long i = 0; i < total; ++i) peak = std::max ({ peak, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]) });
    std::fprintf (stderr, "rendered '%s' %.2fs @ %d Hz, peak %.2f dBFS, latency %d samples\n",
                  patch.name().c_str(), len, sr, 20.0 * std::log10 (std::max (1e-9f, peak)), engine.latencySamples());
    return 0;
}
