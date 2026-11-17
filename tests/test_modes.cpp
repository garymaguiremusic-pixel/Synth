#include <catch2/catch_test_macros.hpp>
#include "Instrument.h"
#include "vc/AnyPreset.h"
#include <zlib.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <catch2/catch_approx.hpp>
using Catch::Approx;

using namespace vc;

namespace
{
void put32le (std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back ((uint8_t) (v >> (8 * i))); }
void put32be (std::vector<uint8_t>& b, uint32_t v) { for (int i = 3; i >= 0; --i) b.push_back ((uint8_t) (v >> (8 * i))); }

double renderRms (Instrument& inst, double sr, int note = 60, double seconds = 0.5)
{
    const int n = (int) (sr * seconds);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    inst.noteOn (note, 100);
    for (int pos = 0; pos < n; pos += 256) inst.process (L.data() + pos, R.data() + pos, std::min (256, n - pos));
    double s = 0;
    for (int i = n / 4; i < n; ++i) { REQUIRE (std::isfinite (L[(size_t) i])); s += (double) L[(size_t) i] * L[(size_t) i]; }
    inst.allNotesOff (true);
    return std::sqrt (s / (n - n / 4));
}
}

TEST_CASE ("Sylenth1 bank: chunk container, zlib, records with names")
{
    // payload: "1lys", version, presets, params, 0, then records (params floats + 36-byte name)
    const uint32_t numParams = 4, numPresets = 3;
    std::vector<uint8_t> payload { '1', 'l', 'y', 's' };
    put32le (payload, 3045); put32le (payload, numPresets); put32le (payload, numParams); put32le (payload, 0);
    const char* names[] = { "Lead One", "", "Bass Two" };
    for (uint32_t k = 0; k < numPresets; ++k)
    {
        for (uint32_t i = 0; i < numParams; ++i) { float f = 0.1f * (float) (k + i); uint32_t u; std::memcpy (&u, &f, 4); put32le (payload, u); }
        char name[36] {}; std::strncpy (name, names[k], 35);
        payload.insert (payload.end(), name, name + 36);
    }
    uLongf clen = compressBound (payload.size());
    std::vector<uint8_t> z (clen);
    REQUIRE (compress2 (z.data(), &clen, payload.data(), payload.size(), 6) == Z_OK);
    z.resize (clen);
    std::vector<uint8_t> chunk { '1', 'l', 'y', 's' };
    put32le (chunk, 3045);
    chunk.insert (chunk.end(), z.begin(), z.end());

    std::vector<uint8_t> fxb { 'C', 'c', 'n', 'K' };
    put32be (fxb, 0);
    fxb.insert (fxb.end(), { 'F', 'B', 'C', 'h' });
    put32be (fxb, 1);
    fxb.insert (fxb.end(), { 's', 'y', 'l', '1' });
    put32be (fxb, 1); put32be (fxb, 512);
    fxb.resize (156, 0);
    put32be (fxb, (uint32_t) chunk.size());
    fxb.insert (fxb.end(), chunk.begin(), chunk.end());

    auto res = importAnyData (fxb.data(), fxb.size(), "test.fxb");
    INFO (res.error);
    REQUIRE (res.error.empty());
    REQUIRE (res.patches.size() == 2);           // the unnamed slot is skipped
    REQUIRE (res.patches[0].type == PatchType::Sylenth);
    REQUIRE (res.patches[0].name == "Lead One");
    REQUIRE (res.patches[1].name == "Bass Two");
    REQUIRE (res.patches[1].sylenth.values.size() == 4);
    REQUIRE (std::abs (res.patches[1].sylenth.values[1] - 0.3f) < 1e-6f);
}

TEST_CASE ("Spire JSON preset")
{
    const std::string json = R"({ "author": "Me", "tags": ["Lead", ""], "parameters": { "osc1_type": 0.002002, "flt1_cut": 0.25, "arp_note3": 0.75 } })";
    auto res = importAnyData (reinterpret_cast<const uint8_t*> (json.data()), json.size(), "/x/My Lead.spf2");
    REQUIRE (res.error.empty());
    REQUIRE (res.patches.size() == 1);
    REQUIRE (res.patches[0].type == PatchType::Spire);
    REQUIRE (res.patches[0].name == "My Lead");
    REQUIRE (res.patches[0].category == "Lead");
    REQUIRE (res.patches[0].spire.at ("flt1_cut") == 0.25f);
}

TEST_CASE ("every synth mode makes sound with its init patch")
{
    Instrument inst;
    inst.prepare (48000.0, 256);
    for (auto m : { SynthMode::Virus, SynthMode::Sylenth, SynthMode::Spire })
    {
        inst.setMode (m);
        INFO ("mode " << (int) m);
        REQUIRE (renderRms (inst, 48000.0) > 0.003);
    }
}

TEST_CASE ("global filter, synced envelopes and saturation stay finite")
{
    Instrument inst;
    inst.prepare (44100.0, 256);
    inst.setTempo (128.0);
    inst.params.filterMode = 8;
    inst.params.cutoff = 60;
    inst.params.resonance = 120;
    inst.params.envAmount = 40;
    inst.params.amp.sync = true;
    inst.params.filter.sync = true;
    inst.params.saturationCurve = 4;
    inst.params.saturationDrive = 0.8;
    for (auto m : { SynthMode::Virus, SynthMode::Sylenth, SynthMode::Spire })
    {
        inst.setMode (m);
        REQUIRE (renderRms (inst, 44100.0, 48) > 0.0005);
    }
    REQUIRE (std::abs (Instrument::envTimeSeconds (0, true, 9, 120.0) - 0.5) < 1e-9);    // 1/4 at 120 BPM
}

TEST_CASE ("Spire arp steps go to the global step sequencer")
{
    auto p = SpireEngine::initParams();
    p[(size_t) spire::idx::arp_on] = 1.0f;
    p[(size_t) spire::idx::arp_note1 + 2] = 0.75f;       // +12
    const auto st = SpireEngine::extractSteps (p);
    REQUIRE (st.present);
    REQUIRE (st.arpOn);
    REQUIRE (st.steps.size() == 16);
    REQUIRE (st.steps[2].transpose == 12);
    REQUIRE (st.rateIndex == 3);                          // Spire's default 1/16
}

TEST_CASE ("user preset files (optional: VC_TEST_PRESETS=folder)")
{
    const char* dir = std::getenv ("VC_TEST_PRESETS");
    if (dir == nullptr) { SUCCEED ("no folder given"); return; }
    auto syl = importAnyFile (std::string (dir) + "/Factorybank1.fxb");
    INFO (syl.error);
    REQUIRE (syl.patches.size() > 10);
    REQUIRE (syl.patches[0].name == "ARP 303 Saw");
    REQUIRE (syl.patches[0].sylenth.values.size() == 244);
    auto sp = importAnyFile (std::string (dir) + "/LD_2000.spf2");
    REQUIRE (sp.patches.size() == 1);
    REQUIRE (sp.patches[0].name == "LD_2000");
    REQUIRE (sp.patches[0].spire.size() == 721);
}

TEST_CASE ("Sylenth presets map onto the Sylenth engine", "[modes][sylenth]")
{
    using namespace vc::sylenth;
    // a synthetic preset in Sylenth's VST parameter order (244 values)
    std::vector<float> raw (244, 0.0f);
    raw[0] = 0.0f; raw[1] = 0.3f; raw[2] = 0.2f; raw[3] = 0.8f;          // amp env A: A D R S
    raw[46] = 1.0f / 3.0f;                                                // filter A: lowpass
    raw[47] = 1.0f;                                                       // 12 dB
    raw[42] = 0.6f;
    raw[54] = 0.5f;
    raw[90] = 3.0f / 8.0f;                                                // osc A1: 3 voices
    raw[92] = 1.0f / 7.0f;                                                // saw
    raw[85] = 4.0f / 6.0f;                                                // octave +1
    raw[69] = 1.0f; raw[68] = 0.5f; raw[91] = 1.0f;                       // mix A, main volume, osc A1 volume
    raw[225] = 15.0f / 32.0f; raw[197] = 1.0f;                            // mod env 1 -> Cutoff AB, +10
    raw[207] = 1.0f / 11.0f; raw[233] = 3.0f / 32.0f; raw[205] = 0.75f;   // misc 1A: velocity -> Volume AB
    raw[217] = 1.0f; raw[9] = 8.0f / 9.0f; raw[13] = 8.0f / 16.0f;        // arp on, Step mode, 8 steps
    raw[165] = 36.0f / 48.0f; raw[181] = 0.5f; raw[149] = 1.0f;           // step 1: +12, vel 64, hold
    vc::PatchSteps st;
    const auto a = vc::SylenthEngine::fromSylenthPreset (raw, &st);
    CHECK (vc::SylenthEngine::listValue (a[OscBase + oVoices], 9) == 3);
    CHECK (vc::SylenthEngine::listValue (a[OscBase + oWave], 8) == 1);
    CHECK (vc::SylenthEngine::listValue (a[OscBase + oOctave], 7) == 4);
    CHECK (vc::SylenthEngine::listValue (a[FiltA], 4) == 1);
    CHECK (vc::SylenthEngine::listValue (a[FiltA + 1], 2) == 0);         // 12 dB
    CHECK (a[AmpA + 2] == Approx (0.8f));                                 // sustain moved to engine's S slot
    CHECK (a[AmpA + 3] == Approx (0.2f));
    CHECK (vc::SylenthEngine::listValue (a[Mod1 + 4], DNumDests) == DCutoffAB);
    CHECK (vc::SylenthEngine::listValue (a[Misc], SNumSources) == SVelocity);
    CHECK (vc::SylenthEngine::listValue (a[Misc + 1], DNumDests) == DVolumeAB);
    CHECK (st.arpOn);
    CHECK (st.mode == 8);
    CHECK (st.length == 8);
    REQUIRE (st.steps.size() == 16);
    CHECK (st.steps[0].transpose == 12);
    CHECK (st.steps[0].velocity == 64);
    CHECK (st.steps[0].tie);

    vc::SylenthEngine e;
    e.prepare (44100, 512);
    e.setAllParams (a);
    e.noteOn (57, 100);
    std::vector<float> L (512), R (512);
    double peak = 0;
    for (int b = 0; b < 40; ++b)
    {
        e.process (L.data(), R.data(), 512);
        for (int i = 0; i < 512; ++i) { REQUIRE (std::isfinite (L[(size_t) i])); peak = std::max (peak, (double) std::abs (L[(size_t) i])); }
    }
    CHECK (peak > 0.01);
}

TEST_CASE ("The user's Sylenth bank plays", "[modes][sylenth][user]")
{
    const char* dir = std::getenv ("VC_TEST_PRESETS");
    if (! dir) return;
    std::ifstream f (std::string (dir) + "/Factorybank1.fxb", std::ios::binary);
    if (! f) return;
    std::vector<uint8_t> data ((std::istreambuf_iterator<char> (f)), {});
    const auto patches = vc::importAnyData (data.data(), data.size(), "Factorybank1.fxb").patches;
    REQUIRE (patches.size() > 100);
    int sounding = 0, total = 0;
    for (auto& p : patches)
    {
        vc::SylenthEngine e;
        e.prepare (44100, 512);
        e.setAllParams (vc::SylenthEngine::fromSylenthPreset (p.sylenth.values));
        e.noteOn (60, 100);
        std::vector<float> L (512), R (512);
        double peak = 0;
        for (int b = 0; b < 60; ++b)
        {
            if (b == 40) e.noteOff (60);
            e.process (L.data(), R.data(), 512);
            for (int i = 0; i < 512; ++i)
            {
                REQUIRE (std::isfinite (L[(size_t) i]));
                peak = std::max (peak, (double) std::abs (L[(size_t) i]));
            }
        }
        ++total;
        sounding += peak > 1e-3 ? 1 : 0;
        CHECK (peak < 8.0);
    }
    WARN ("Sylenth bank: " << sounding << " of " << total << " presets sound");
    CHECK (sounding > total * 9 / 10);
}

TEST_CASE ("Sylenth and Spire files written by VC-Clone read back", "[modes][export]")
{
    std::vector<float> v (244);
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) i / 300.0f;
    const auto fxp = vc::writeSylenthFxp (v, "My Lead");
    auto res = vc::importAnyData (fxp.data(), fxp.size(), "x.fxp");
    REQUIRE (res.patches.size() == 1);
    CHECK (res.patches[0].type == vc::PatchType::Sylenth);
    CHECK (res.patches[0].name == "My Lead");
    REQUIRE (res.patches[0].sylenth.values.size() == 244);
    CHECK (res.patches[0].sylenth.values[100] == Approx (v[100]));
    const auto fxb = vc::writeSylenthFile ({ v, v, v }, { "A", "B", "C" }, true);
    auto bank = vc::importAnyData (fxb.data(), fxb.size(), "x.fxb");
    CHECK (bank.patches.size() == 3);

    // the engine's parameters survive Sylenth -> VC-Clone -> Sylenth -> VC-Clone
    const auto a = vc::SylenthEngine::fromSylenthPreset (v);
    const auto back = vc::SylenthEngine::fromSylenthPreset (vc::SylenthEngine::toSylenthPreset (a, {}));
    for (size_t i = 0; i < a.size(); ++i) { INFO ("param " << i); CHECK (back[i] == Approx (a[i]).margin (0.002)); }

    const auto text = vc::writeSpirePreset ({ { "cutoff_1", 0.25f }, { "osc1_type", 0.002002f } }, "Pad");
    auto sp = vc::importAnyData (reinterpret_cast<const uint8_t*> (text.data()), text.size(), "Pad.spf2");
    REQUIRE (sp.patches.size() == 1);
    CHECK (sp.patches[0].type == vc::PatchType::Spire);
    CHECK (sp.patches[0].spire.at ("cutoff_1") == Approx (0.25f));
}
