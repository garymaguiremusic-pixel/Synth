#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "HostEngine.h"
#include "vc/PresetIO.h"
#include "VirusCLayout.h"

#include <cmath>

using namespace vc;
using namespace vc::layout::off;

namespace
{
struct Render { std::vector<float> L, R; };

Render render (const SinglePatch& p, double sr, double seconds, int note = 69, int vel = 100, double noteLen = -1.0, uint64_t seed = 1)
{
    HostEngine e;
    e.synth.seed (seed);
    e.prepare (sr, 256);
    e.synth.setPatch (p.raw.data());
    const int total = (int) (seconds * sr);
    const int offAt = noteLen < 0 ? total + 1 : (int) (noteLen * sr);
    Render r { std::vector<float> ((size_t) total), std::vector<float> ((size_t) total) };
    e.synth.noteOn (note, vel);
    for (int pos = 0; pos < total; pos += 256)
    {
        if (pos <= offAt && offAt < pos + 256) e.synth.noteOff (note);
        const int n = std::min (256, total - pos);
        e.process (r.L.data() + pos, r.R.data() + pos, n);
    }
    return r;
}

// frequency estimate by autocorrelation with parabolic interpolation
double estimateHz (const std::vector<float>& x, double sr, int start, int len)
{
    const int minLag = (int) (sr / 2000.0), maxLag = (int) (sr / 40.0);
    double best = -1e30; int bestLag = minLag;
    std::vector<double> ac ((size_t) maxLag + 2);
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double s = 0;
        for (int i = 0; i < len; ++i) s += (double) x[(size_t) (start + i)] * x[(size_t) (start + i + lag)];
        ac[(size_t) lag] = s;
    }
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] > best && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1]) { best = ac[(size_t) lag]; bestLag = lag; }
    // take the first peak that's close to the best (avoid octave errors)
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1] && ac[(size_t) lag] > 0.9 * best) { bestLag = lag; break; }
    const double a = ac[(size_t) bestLag - 1], b = ac[(size_t) bestLag], c = ac[(size_t) bestLag + 1];
    const double d = 0.5 * (a - c) / (a - 2 * b + c);
    return sr / (bestLag + d);
}

double rms (const std::vector<float>& x, int start, int len)
{
    double s = 0;
    for (int i = 0; i < len; ++i) s += (double) x[(size_t) (start + i)] * x[(size_t) (start + i)];
    return std::sqrt (s / len);
}

bool allFinite (const Render& r, float limit = 64.0f)
{
    for (size_t i = 0; i < r.L.size(); ++i)
        if (! std::isfinite (r.L[i]) || ! std::isfinite (r.R[i]) || std::abs (r.L[i]) > limit || std::abs (r.R[i]) > limit) return false;
    return true;
}

double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
}

TEST_CASE ("init patch: A4 saw is in tune at 44.1k and 48k")
{
    auto p = makeInitPatch();
    p.raw[Osc_Init_Phase] = 1;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        auto r = render (p, sr, 0.5);
        REQUIRE (allFinite (r));
        REQUIRE (rms (r.L, (int) (0.1 * sr), (int) (0.2 * sr)) > 0.01);
        const double hz = estimateHz (r.L, sr, (int) (0.1 * sr), (int) (0.2 * sr));
        INFO ("sr " << sr << " -> " << hz << " Hz");
        REQUIRE (std::abs (cents (hz, 440.0)) < 1.0);
    }
}

TEST_CASE ("semitone and keyfollow")
{
    auto p = makeInitPatch();
    p.raw[Osc1_Semitone] = 64 + 12;
    auto r = render (p, 44100.0, 0.4, 57);
    REQUIRE (std::abs (cents (estimateHz (r.L, 44100.0, 4410, 8820), 440.0)) < 1.0);

    p = makeInitPatch();
    p.raw[Osc1_Keyfollow] = 64;     // 0 % keyfollow: pitch fixed at the base note (C3 = 60)
    auto r2 = render (p, 44100.0, 0.4, 81);
    REQUIRE (std::abs (cents (estimateHz (r2.L, 44100.0, 4410, 8820), 261.6256)) < 1.0);
}

TEST_CASE ("renders are deterministic for a fixed seed")
{
    auto p = makeInitPatch();
    p.raw[Noise_Volume] = 80;
    p.raw[Osc_Init_Phase] = 0;
    auto a = render (p, 48000.0, 0.3, 60, 100, -1, 7);
    auto b = render (p, 48000.0, 0.3, 60, 100, -1, 7);
    REQUIRE (a.L == b.L);
    REQUIRE (a.R == b.R);
}

TEST_CASE ("amp envelope release reaches silence")
{
    auto p = makeInitPatch();
    p.raw[Amp_Env_Release] = 40;
    auto r = render (p, 44100.0, 1.5, 60, 100, 0.2);
    REQUIRE (rms (r.L, 44100, 4410) < 1e-4);
}

TEST_CASE ("every filter mode, routing, saturation and FX mode stays finite")
{
    for (int f1 = 0; f1 < 8; ++f1)
        for (int routing = 0; routing < 4; ++routing)
        {
            auto p = makeInitPatch();
            p.raw[Filter1_Mode] = (uint8_t) f1;
            p.raw[Filter2_Mode] = (uint8_t) (f1 % 4);
            p.raw[Filter_Routing] = (uint8_t) routing;
            p.raw[Cutoff] = 60;
            p.raw[Cutoff2] = 70;
            p.raw[Filter1_Resonance] = 127;
            p.raw[Filter2_Resonance] = 127;
            p.raw[Filter1_Env_Amt] = 100;
            p.raw[Filter_Env_Decay] = 50;
            p.raw[Filter_Env_Sustain] = 20;
            p.raw[Saturation_Curve] = (uint8_t) ((f1 * 4 + routing) % 15);
            p.raw[Osc_Mainvolume] = 127;
            auto r = render (p, 44100.0, 0.3, 48);
            INFO ("f1 " << f1 << " routing " << routing);
            REQUIRE (allFinite (r));
        }
    for (int mode = 0; mode <= 26; ++mode)
    {
        auto p = makeInitPatch();
        p.raw[Delay_Reverb_Mode] = (uint8_t) mode;
        p.raw[Effect_Send] = 100;
        p.raw[Delay_Feedback] = 120;
        p.raw[Chorus_Mix] = 80;
        p.raw[Phaser_Mix] = 80;
        p.raw[Phaser_Mode] = (uint8_t) (mode % 7);
        p.raw[Distortion_Curve] = (uint8_t) (mode % 12);
        p.raw[Distortion_Intensity] = 100;
        p.raw[Bass_Intensity] = 80;
        p.raw[LowEQ_Gain] = 100;
        auto r = render (p, 44100.0, 0.5, 60, 100, 0.2);
        INFO ("delay mode " << mode);
        REQUIRE (allFinite (r));
    }
}

TEST_CASE ("unison, mono modes and arpeggiator produce sound")
{
    auto p = makeInitPatch();
    p.raw[Unison_Mode] = 5;
    p.raw[Unison_Detune] = 80;
    REQUIRE (rms (render (p, 44100.0, 0.3).L, 4410, 4410) > 0.01);

    for (int km = 1; km <= 5; ++km)
    {
        p = makeInitPatch();
        p.raw[Key_Mode] = (uint8_t) km;
        p.raw[Portamento_Time] = 40;
        REQUIRE (rms (render (p, 44100.0, 0.3).L, 4410, 4410) > 0.01);
    }

    p = makeInitPatch();
    p.raw[Arp_Mode] = 1;
    p.raw[Arp_Clock] = 3;
    auto r = render (p, 44100.0, 1.0);
    REQUIRE (allFinite (r));
    REQUIRE (rms (r.L, 0, 44100) > 0.005);
}

TEST_CASE ("all oscillator shapes, waves and osc3 modes are finite")
{
    for (int w = 0; w < 64; w += 3)
    {
        auto p = makeInitPatch();
        p.raw[Osc1_Shape] = (uint8_t) ((w * 2) % 128);
        p.raw[Osc1_Wave_Select] = (uint8_t) w;
        p.raw[Osc2_Wave_Select] = (uint8_t) (63 - w);
        p.raw[Osc_Balance] = 64;
        p.raw[Osc3_Mode] = (uint8_t) (w + 4 > 67 ? 67 : w + 4);
        p.raw[Osc3_Volume] = 100;
        p.raw[Osc2_FM_Amount] = (uint8_t) (w * 2);
        p.raw[Osc_FM_Mode] = (uint8_t) (w % 4);
        p.raw[Osc2_Sync] = (uint8_t) (w & 1);
        p.raw[Suboscillator_Volume] = 60;
        p.raw[Ringmodulator_Volume] = 40;
        p.raw[Noise_Volume] = 30;
        auto r = render (p, 44100.0, 0.2, 40 + w);
        INFO ("wave " << w);
        REQUIRE (allFinite (r));
    }
}
