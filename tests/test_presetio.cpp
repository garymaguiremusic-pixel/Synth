#include <catch2/catch_test_macros.hpp>
#include "vc/PresetIO.h"
#include "VirusCLayout.h"

#include <random>

using namespace vc;

namespace
{
std::vector<SinglePatch> makeBank (int n)
{
    std::vector<SinglePatch> bank;
    for (int i = 0; i < n; ++i)
    {
        auto p = makeInitPatch();
        p.setName ("Patch " + std::to_string (i));
        p.raw[layout::off::Cutoff] = (uint8_t) (i % 128);
        p.raw[layout::off::Osc1_Shape] = (uint8_t) ((i * 7) % 128);
        p.bank = 1;
        p.program = i;
        bank.push_back (p);
    }
    return bank;
}

void putVlq (Bytes& b, uint32_t v)
{
    uint8_t tmp[5]; int n = 0;
    tmp[n++] = v & 0x7F;
    while ((v >>= 7) != 0) tmp[n++] = (uint8_t) (0x80 | (v & 0x7F));
    while (n--) b.push_back (tmp[n]);
}

Bytes wrapTrack (const Bytes& track, int format = 0, int tracks = 1)
{
    Bytes smf { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, (uint8_t) format, 0, (uint8_t) tracks, 0x01, 0xE0, 'M', 'T', 'r', 'k' };
    const uint32_t len = (uint32_t) track.size();
    for (int s = 24; s >= 0; s -= 8) smf.push_back ((uint8_t) (len >> s));
    smf.insert (smf.end(), track.begin(), track.end());
    return smf;
}
}

TEST_CASE ("layout is complete and consistent")
{
    for (int i = 0; i < 256; ++i)
    {
        REQUIRE (layout::kParams[i].offset == i);
        REQUIRE (layout::kParams[i].min <= layout::kParams[i].max);
        if (layout::kParams[i].toText[0] != 0)
            REQUIRE (layout::findValueList (layout::kParams[i].toText) != nullptr);
    }
    REQUIRE (layout::off::Cutoff == 40);
    REQUIRE (layout::off::Filter1_Mode == 51);
    REQUIRE (layout::off::Delay_Reverb_Mode == 112);
    REQUIRE (layout::kParams[layout::off::Delay_Reverb_Mode].max == 26);   // filled from JSON
    REQUIRE (layout::findValueList ("modmatrixSource")->count == 28);
    REQUIRE (layout::kNumRegions > 30);
}

TEST_CASE ("checksum maths")
{
    Bytes m { 0xF0, 0x00, 0x20, 0x33, 0x01, 0x10, 0x10, 0x00, 0x00 };
    for (int i = 0; i < 256; ++i) m.push_back ((uint8_t) (i & 0x7F));
    unsigned sum = 0;
    for (size_t i = 5; i < m.size(); ++i) sum += m[i];
    REQUIRE (virusChecksum (m.data(), 5, (int) m.size() - 1) == (sum & 0x7F));

    SinglePatch p = makeInitPatch();
    auto dump = makeSingleDump (p, 0, 0);
    REQUIRE (dump.size() == (size_t) kSingleDumpSize);
    REQUIRE (dump.front() == 0xF0);
    REQUIRE (dump.back() == 0xF7);
    REQUIRE (dump[265] == virusChecksum (dump.data(), 5, 264));
}

TEST_CASE ("SMF with 128 dumps imports 128 patches, names intact")
{
    auto bank = makeBank (128);
    auto smf = makeSmf (bank);
    auto res = importData (smf.data(), smf.size());
    REQUIRE (res.error.empty());
    REQUIRE (res.singles.size() == 128);
    for (int i = 0; i < 128; ++i)
    {
        REQUIRE (res.singles[(size_t) i].name() == "Patch " + std::to_string (i));
        REQUIRE (res.singles[(size_t) i].checksumOk);
        REQUIRE (res.singles[(size_t) i].program == i);
    }
}

TEST_CASE ("names are sanitised")
{
    SinglePatch p = makeInitPatch();
    p.raw[240] = 'A'; p.raw[241] = 0x01; p.raw[242] = 'b'; p.raw[243] = 0x7F;
    for (int i = 244; i < 250; ++i) p.raw[(size_t) i] = ' ';
    REQUIRE (p.name() == "A b");
    p.setName ("TooLongName12345");
    REQUIRE (p.name() == "TooLongNam");
}

TEST_CASE ("byte-exact round trip, including out-of-range bytes")
{
    auto bank = makeBank (8);
    bank[3].raw[layout::off::Filter1_Mode] = 101;     // out of range: must survive export
    bank[4].raw[layout::off::Saturation_Curve] = 50;
    auto smf1 = makeSmf (bank);
    auto r1 = importData (smf1.data(), smf1.size());
    REQUIRE (r1.singles.size() == 8);
    auto smf2 = makeSmf (r1.singles);
    REQUIRE (smf1 == smf2);
    for (size_t i = 0; i < bank.size(); ++i)
        REQUIRE (r1.singles[i].raw == bank[i].raw);

    REQUIRE (r1.singles[3].value (layout::off::Filter1_Mode) == 7);   // clamped for the engine
    REQUIRE (r1.singles[3].raw[layout::off::Filter1_Mode] == 101);    // raw kept
    int clamped = 0;
    for (auto& w : r1.warnings) clamped += w.kind == ImportWarning::Kind::Clamped ? 1 : 0;
    REQUIRE (clamped == 2);

    auto syx = makeSyx (bank);
    auto r2 = importData (syx.data(), syx.size());
    REQUIRE (r2.singles.size() == 8);
    REQUIRE (makeSyx (r2.singles) == syx);
}

TEST_CASE ("checksum mismatch warns but imports")
{
    auto dump = makeSingleDump (makeInitPatch());
    dump[265] ^= 0x15;
    auto res = importData (dump.data(), dump.size());
    REQUIRE (res.singles.size() == 1);
    REQUIRE_FALSE (res.singles[0].checksumOk);
    REQUIRE (res.warnings.size() == 1);
    REQUIRE (res.warnings[0].kind == ImportWarning::Kind::Checksum);
}

TEST_CASE ("TI dump fallback keeps the first 256 bytes")
{
    SinglePatch p = makeInitPatch();
    Bytes m { 0xF0, 0x00, 0x20, 0x33, 0x01, 0x00, 0x10, 0x01, 0x05 };
    for (auto b : p.raw) m.push_back (b);
    for (int i = 0; i < 256; ++i) m.push_back ((uint8_t) (i & 0x7F));
    m.push_back (virusChecksum (m.data(), 5, (int) m.size() - 1));
    m.push_back (0xF7);
    auto res = importData (m.data(), m.size());
    REQUIRE (res.singles.size() == 1);
    REQUIRE (res.singles[0].raw == p.raw);
    REQUIRE (res.singles[0].extra.size() == 256);
    REQUIRE (res.singles[0].checksumOk);
    bool sawTi = false;
    for (auto& w : res.warnings) sawTi |= w.kind == ImportWarning::Kind::TiDump;
    REQUIRE (sawTi);
}

TEST_CASE ("OS update files are rejected")
{
    Bytes track;
    for (int i = 0; i < 40; ++i)
    {
        Bytes msg { 0x00, 0x20, 0x33, 0x01, 0x10, 0x7A, (uint8_t) i };
        for (int k = 0; k < 100; ++k) msg.push_back ((uint8_t) ((k * 13 + i) & 0x7F));
        msg.push_back (0xF7);
        putVlq (track, 10);
        track.push_back (0xF0);
        putVlq (track, (uint32_t) msg.size());
        track.insert (track.end(), msg.begin(), msg.end());
    }
    track.insert (track.end(), { 0x00, 0xFF, 0x2F, 0x00 });
    auto smf = wrapTrack (track);
    auto res = importData (smf.data(), smf.size());
    REQUIRE (res.isOsUpdate);
    REQUIRE (res.singles.empty());
    REQUIRE_FALSE (res.error.empty());
}

TEST_CASE ("SMF parsing: running status, meta events, F7 continuation packets, type 1")
{
    auto dump = makeSingleDump (makeInitPatch(), 0, 0);
    Bytes track;
    // tempo meta + note on with running status
    track.insert (track.end(), { 0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20 });
    track.insert (track.end(), { 0x00, 0x90, 60, 100, 0x10, 62, 100, 0x10, 0xC0, 5, 0x00, 0xE0, 0, 64 });
    // split sysex: F0 <first 100 bytes>, F7 <rest>
    putVlq (track, 0);
    track.push_back (0xF0);
    putVlq (track, 100);
    track.insert (track.end(), dump.begin() + 1, dump.begin() + 101);
    putVlq (track, 5);
    track.push_back (0xF7);
    putVlq (track, (uint32_t) (dump.size() - 101));
    track.insert (track.end(), dump.begin() + 101, dump.end());
    track.insert (track.end(), { 0x00, 0x80, 60, 0, 0x00, 0xFF, 0x2F, 0x00 });

    Bytes conductor { 0x00, 0xFF, 0x58, 0x04, 4, 2, 24, 8, 0x00, 0xFF, 0x2F, 0x00 };
    Bytes smf { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x01, 0xE0 };
    auto addTrack = [&] (const Bytes& t)
    {
        smf.insert (smf.end(), { 'M', 'T', 'r', 'k' });
        for (int s = 24; s >= 0; s -= 8) smf.push_back ((uint8_t) (t.size() >> s));
        smf.insert (smf.end(), t.begin(), t.end());
    };
    addTrack (conductor);
    addTrack (track);

    std::vector<Bytes> msgs;
    REQUIRE (extractSysexFromSmf (smf.data(), smf.size(), msgs));
    REQUIRE (msgs.size() == 1);
    REQUIRE (msgs[0] == dump);
}

TEST_CASE ("parameter change messages are parsed")
{
    auto m = makeParamChange (layout::off::Cutoff, 99, 0);
    auto res = importData (m.data(), m.size());
    REQUIRE (res.paramChanges.size() == 1);
    REQUIRE (res.paramChanges[0].offset() == layout::off::Cutoff);
    REQUIRE (res.paramChanges[0].value == 99);
    auto mb = makeParamChange (layout::off::Osc3_Mode, 4, 0);
    auto rb = importData (mb.data(), mb.size());
    REQUIRE (rb.paramChanges[0].offset() == layout::off::Osc3_Mode);
}

TEST_CASE ("fuzzed garbage never crashes")
{
    std::mt19937 rng (1234);
    auto smf = makeSmf (makeBank (4));
    for (int iter = 0; iter < 3000; ++iter)
    {
        Bytes data;
        if (iter % 3 == 0)
        {
            data.resize (rng() % 2048);
            for (auto& b : data) b = (uint8_t) rng();
            if (iter % 6 == 0 && data.size() > 4) { data[0] = 'M'; data[1] = 'T'; data[2] = 'h'; data[3] = 'd'; }
        }
        else
        {
            data = smf;
            const int flips = 1 + (int) (rng() % 20);
            for (int f = 0; f < flips; ++f) data[rng() % data.size()] = (uint8_t) rng();
            if (iter % 2) data.resize (rng() % data.size());
        }
        auto res = importData (data.data(), data.size());
        for (auto& p : res.singles) { (void) p.name(); (void) patchToJson (p); }
    }
    SUCCEED();
}

TEST_CASE ("valueToText uses value lists")
{
    REQUIRE (valueToText (layout::off::Osc1_Shape, 64) == "Saw");
    REQUIRE (valueToText (layout::off::Osc1_Shape, 0) == "Wave");
    REQUIRE (valueToText (layout::off::Filter1_Mode, 7) == "Analog 4P");
    REQUIRE (valueToText (layout::off::Osc1_Semitone, 76) == "+12");
    REQUIRE (valueToText (layout::off::Cutoff, 80) == "80");
}
