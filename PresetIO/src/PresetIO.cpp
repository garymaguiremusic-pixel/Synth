#include "vc/PresetIO.h"
#include "VirusCLayout.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace vc
{
namespace
{
    constexpr uint8_t kVirusHeader[] = { 0xF0, 0x00, 0x20, 0x33, 0x01 };

    bool isVirusSysex (const Bytes& m)
    {
        return m.size() >= 8 && std::equal (std::begin (kVirusHeader), std::end (kVirusHeader), m.begin());
    }

    struct Reader
    {
        const uint8_t* p;
        size_t size, pos = 0;

        bool has (size_t n) const { return pos + n <= size; }
        bool u8 (uint8_t& v) { if (! has (1)) return false; v = p[pos++]; return true; }
        bool vlq (uint32_t& v)
        {
            v = 0;
            for (int i = 0; i < 4; ++i)
            {
                uint8_t b;
                if (! u8 (b)) return false;
                v = (v << 7) | (b & 0x7F);
                if ((b & 0x80) == 0) return true;
            }
            return false;   // VLQ longer than 4 bytes = corrupt
        }
    };

    uint32_t be32 (const uint8_t* p) { return (uint32_t (p[0]) << 24) | (uint32_t (p[1]) << 16) | (uint32_t (p[2]) << 8) | p[3]; }
    uint16_t be16 (const uint8_t* p) { return uint16_t ((p[0] << 8) | p[1]); }

    void parseTrack (const uint8_t* data, size_t size, std::vector<Bytes>& out)
    {
        Reader r { data, size };
        uint8_t running = 0;
        Bytes pending;            // SysEx being assembled across F7 continuation packets
        bool inSysex = false;

        auto flushSysex = [&]
        {
            if (! pending.empty())
            {
                if (pending.back() != 0xF7) pending.push_back (0xF7);
                out.push_back (std::move (pending));
                pending.clear();
            }
            inSysex = false;
        };

        while (r.pos < r.size)
        {
            uint32_t delta;
            if (! r.vlq (delta)) break;
            uint8_t status;
            if (! r.u8 (status)) break;

            if (status == 0xFF)   // meta
            {
                uint8_t type; uint32_t len;
                if (! r.u8 (type) || ! r.vlq (len) || ! r.has (len)) break;
                r.pos += len;
                if (type == 0x2F) break;     // end of track
                continue;
            }
            if (status == 0xF0 || status == 0xF7)
            {
                uint32_t len;
                if (! r.vlq (len) || ! r.has (len)) break;
                const uint8_t* body = r.p + r.pos;
                r.pos += len;
                running = 0;

                if (status == 0xF0)
                {
                    if (inSysex) flushSysex();
                    pending.assign (1, 0xF0);
                    pending.insert (pending.end(), body, body + len);
                    inSysex = true;
                }
                else if (inSysex)   // continuation packet
                {
                    pending.insert (pending.end(), body, body + len);
                }
                else
                {
                    // "escape" packet: raw bytes. If it carries a whole F0..F7 message, take it.
                    Bytes raw (body, body + len);
                    extractSysexFromRaw (raw.data(), raw.size(), out);
                    continue;
                }
                if (! pending.empty() && pending.back() == 0xF7)
                    flushSysex();
                continue;
            }

            uint8_t first;
            if (status & 0x80)
            {
                if (status >= 0xF0)  // other system messages don't appear in SMF; bail out of this track
                    break;
                running = status;
                if (! r.u8 (first)) break;
            }
            else
            {
                if (running == 0) break;   // data byte without running status = corrupt
                first = status;
                status = running;
            }
            (void) first;
            const uint8_t hi = status & 0xF0;
            const int dataLen = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
            if (dataLen == 2)
            {
                uint8_t second;
                if (! r.u8 (second)) break;
            }
        }
        if (inSysex) flushSysex();
    }
} // namespace

//==============================================================================
std::string SinglePatch::name() const
{
    std::string s;
    for (int i = 240; i < 250; ++i)
    {
        const char c = (char) raw[(size_t) i];
        s += (c >= 32 && c < 127) ? c : ' ';
    }
    while (! s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

void SinglePatch::setName (const std::string& n)
{
    for (int i = 0; i < 10; ++i)
    {
        char c = i < (int) n.size() ? n[(size_t) i] : ' ';
        if (c < 32 || c > 126) c = ' ';
        raw[(size_t) (240 + i)] = (uint8_t) c;
    }
}

int SinglePatch::value (int offset) const
{
    const auto& d = layout::kParams[offset];
    return std::clamp ((int) raw[(size_t) offset], d.min, d.max);
}

bool SinglePatch::isOutOfRange (int offset) const
{
    const auto& d = layout::kParams[offset];
    const int v = raw[(size_t) offset];
    return v < d.min || v > d.max;
}

std::string MultiPatch::name() const
{
    std::string s;
    for (size_t i = 4; i < 14 && i < raw.size(); ++i)
    {
        const char c = (char) raw[i];
        s += (c >= 32 && c < 127) ? c : ' ';
    }
    while (! s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

//==============================================================================
bool extractSysexFromSmf (const uint8_t* data, size_t size, std::vector<Bytes>& out)
{
    if (data == nullptr || size < 14 || std::memcmp (data, "MThd", 4) != 0)
        return false;

    const uint32_t headerLen = be32 (data + 4);
    if (headerLen < 6 || 8 + (size_t) headerLen > size)
        return false;
    const int format = be16 (data + 8);
    const int numTracks = be16 (data + 10);
    if (format > 2)
        return false;

    size_t pos = 8 + headerLen;
    for (int t = 0; t < numTracks && pos + 8 <= size; ++t)
    {
        const uint32_t len = be32 (data + pos + 4);
        const bool isTrack = std::memcmp (data + pos, "MTrk", 4) == 0;
        pos += 8;
        const size_t avail = std::min ((size_t) len, size - pos);
        if (isTrack)
            parseTrack (data + pos, avail, out);
        else
            --t;   // unknown chunk: skip, doesn't count as a track
        pos += avail;
    }
    return true;
}

void extractSysexFromRaw (const uint8_t* data, size_t size, std::vector<Bytes>& out)
{
    if (data == nullptr) return;
    size_t i = 0;
    while (i < size)
    {
        if (data[i] != 0xF0) { ++i; continue; }
        size_t j = i + 1;
        while (j < size && data[j] != 0xF7 && data[j] != 0xF0)
            ++j;
        if (j < size && data[j] == 0xF7)
        {
            out.emplace_back (data + i, data + j + 1);
            i = j + 1;
        }
        else
        {
            i = j;   // unterminated: drop it
        }
    }
}

uint8_t virusChecksum (const uint8_t* msg, int first, int last)
{
    unsigned sum = 0;
    for (int i = first; i <= last; ++i)
        sum += msg[i];
    return (uint8_t) (sum & 0x7F);
}

//==============================================================================
ImportResult parseSysexMessages (const std::vector<Bytes>& messages)
{
    ImportResult res;
    res.sysexCount = (int) messages.size();
    int otherVirus = 0;

    for (const auto& m : messages)
    {
        if (! isVirusSysex (m))
            continue;
        ++res.virusSysexCount;
        const uint8_t cmd = m[6];

        if (cmd == 0x10)
        {
            // F0 00 20 33 01 dev 10 bank prog <data> cs F7
            const int dataLen = (int) m.size() - 11;
            if (dataLen < kSingleSize)
            {
                res.warnings.push_back ({ ImportWarning::Kind::Truncated, -1, -1, dataLen,
                                          "Single dump too short (" + std::to_string (dataLen) + " data bytes), skipped" });
                continue;
            }
            SinglePatch p;
            p.bank = m[7];
            p.program = m[8];
            std::copy (m.begin() + 9, m.begin() + 9 + kSingleSize, p.raw.begin());
            const int idx = (int) res.singles.size();

            if (dataLen > kSingleSize)
            {
                p.extra.assign (m.begin() + 9 + kSingleSize, m.end() - 2);
                res.warnings.push_back ({ ImportWarning::Kind::TiDump, idx, -1, dataLen,
                                          "Dump has " + std::to_string (dataLen) + " data bytes (Virus TI?): using the first 256, rest kept opaque" });
            }

            const int last = (int) m.size() - 3;
            const uint8_t cs = virusChecksum (m.data(), 5, last);
            p.checksumOk = cs == m[m.size() - 2];
            if (! p.checksumOk)
            {
                char buf[96];
                std::snprintf (buf, sizeof buf, "Checksum mismatch (expected %02X, file has %02X), imported anyway", cs, m[m.size() - 2]);
                res.warnings.push_back ({ ImportWarning::Kind::Checksum, idx, -1, m[m.size() - 2], buf });
            }

            for (int o = 0; o < kSingleSize; ++o)
                if (p.isOutOfRange (o))
                {
                    const auto& d = layout::kParams[o];
                    res.warnings.push_back ({ ImportWarning::Kind::Clamped, idx, o, p.raw[(size_t) o],
                                              std::string ("'") + p.name() + "' " + d.name + " = " + std::to_string (p.raw[(size_t) o])
                                              + " outside " + std::to_string (d.min) + ".." + std::to_string (d.max) + ", clamped (raw byte kept)" });
                }
            res.singles.push_back (std::move (p));
        }
        else if (cmd == 0x11)
        {
            if (m.size() < 12) continue;
            MultiPatch mp;
            mp.bank = m[7];
            mp.program = m[8];
            mp.raw.assign (m.begin() + 9, m.end() - 2);
            res.multis.push_back (std::move (mp));
        }
        else if ((cmd == 0x70 || cmd == 0x71) && m.size() == 11)
        {
            res.paramChanges.push_back ({ cmd, m[7], m[8], m[9] });
        }
        else
        {
            ++otherVirus;
        }
    }

    // Access OS updates are .mid files full of Virus SysEx that are neither singles nor multis.
    if (res.singles.empty() && res.multis.empty() && otherVirus >= 8)
    {
        res.isOsUpdate = true;
        res.error = "This looks like an Access OS update (firmware), not a preset bank: "
                    + std::to_string (otherVirus) + " Virus system messages and no single dumps. Not imported.";
        res.paramChanges.clear();
        return res;
    }

    if (res.singles.empty() && res.multis.empty() && res.paramChanges.empty())
        res.error = res.sysexCount == 0 ? "No SysEx found in file" : "No Virus single or multi dumps found";
    return res;
}

ImportResult importData (const uint8_t* data, size_t size)
{
    std::vector<Bytes> msgs;
    if (! extractSysexFromSmf (data, size, msgs))
        extractSysexFromRaw (data, size, msgs);
    return parseSysexMessages (msgs);
}

std::optional<Bytes> readFile (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return std::nullopt;
    return Bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
}

bool writeFile (const std::string& path, const Bytes& b)
{
    std::ofstream f (path, std::ios::binary);
    if (! f) return false;
    f.write ((const char*) b.data(), (std::streamsize) b.size());
    return (bool) f;
}

ImportResult importFile (const std::string& path)
{
    auto data = readFile (path);
    if (! data)
    {
        ImportResult r;
        r.error = "Could not read " + path;
        return r;
    }
    return importData (data->data(), data->size());
}

//==============================================================================
Bytes makeSingleDump (const SinglePatch& p, int bank, int program, uint8_t deviceId)
{
    Bytes m { 0xF0, 0x00, 0x20, 0x33, 0x01, deviceId, 0x10, (uint8_t) (bank & 0x7F), (uint8_t) (program & 0x7F) };
    for (auto b : p.raw) m.push_back (b & 0x7F);
    m.push_back (virusChecksum (m.data(), 5, (int) m.size() - 1));
    m.push_back (0xF7);
    return m;
}

Bytes makeParamChange (int offset, int value, int part, uint8_t deviceId)
{
    return { 0xF0, 0x00, 0x20, 0x33, 0x01, deviceId, (uint8_t) (offset < 128 ? 0x70 : 0x71),
             (uint8_t) (part & 0x7F), (uint8_t) (offset & 0x7F), (uint8_t) (value & 0x7F), 0xF7 };
}

namespace
{
    void putVlq (Bytes& b, uint32_t v)
    {
        uint8_t tmp[5]; int n = 0;
        tmp[n++] = v & 0x7F;
        while ((v >>= 7) != 0) tmp[n++] = (uint8_t) (0x80 | (v & 0x7F));
        while (n--) b.push_back (tmp[n]);
    }
    void put32 (Bytes& b, uint32_t v) { for (int s = 24; s >= 0; s -= 8) b.push_back ((uint8_t) (v >> s)); }
}

Bytes makeSmf (const std::vector<SinglePatch>& singles, bool keepBankProgram)
{
    Bytes track;
    for (size_t i = 0; i < singles.size(); ++i)
    {
        const auto& p = singles[i];
        auto dump = makeSingleDump (p, keepBankProgram ? p.bank : 1, keepBankProgram ? p.program : (int) i);
        putVlq (track, i == 0 ? 0 : 96);
        track.push_back (0xF0);
        putVlq (track, (uint32_t) dump.size() - 1);
        track.insert (track.end(), dump.begin() + 1, dump.end());
    }
    track.insert (track.end(), { 0x00, 0xFF, 0x2F, 0x00 });

    Bytes smf { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0 };
    smf.insert (smf.end(), { 'M', 'T', 'r', 'k' });
    put32 (smf, (uint32_t) track.size());
    smf.insert (smf.end(), track.begin(), track.end());
    return smf;
}

Bytes makeSyx (const std::vector<SinglePatch>& singles, bool keepBankProgram)
{
    Bytes out;
    for (size_t i = 0; i < singles.size(); ++i)
    {
        const auto& p = singles[i];
        auto d = makeSingleDump (p, keepBankProgram ? p.bank : 1, keepBankProgram ? p.program : (int) i);
        out.insert (out.end(), d.begin(), d.end());
    }
    return out;
}

//==============================================================================
SinglePatch makeInitPatch()
{
    using namespace layout::off;
    SinglePatch p;
    for (int i = 0; i < kSingleSize; ++i)
    {
        const auto& d = layout::kParams[i];
        // bipolar parameters rest at their centre
        const bool centred = std::string_view (d.toText) == "signed" || std::string_view (d.toText) == "pan"
                          || std::string_view (d.toText) == "envSustainTime" || std::string_view (d.toText) == "oscShape";
        p.raw[(size_t) i] = (uint8_t) (centred ? std::clamp (64, d.min, d.max) : d.def);
    }
    p.raw[Version] = kVirusCVersion;
    p.raw[Channel_Volume] = 127;
    p.raw[Expression] = 127;
    p.raw[Osc1_Shape] = 64;              // saw
    p.raw[Osc2_Shape] = 64;
    p.raw[Osc1_Pulsewidth] = 0;          // 50 %
    p.raw[Osc2_Pulsewidth] = 0;
    p.raw[Osc1_Keyfollow] = 96;
    p.raw[Osc2_Keyfollow] = 96;
    p.raw[Osc_Balance] = 0;              // osc1 only
    p.raw[Osc_Mainvolume] = 64;
    p.raw[Noise_Color] = 64;
    p.raw[Cutoff] = 127;
    p.raw[Cutoff2] = 127;
    p.raw[Filter1_Keyfollow] = 64;
    p.raw[Filter2_Keyfollow] = 64;
    p.raw[Filter_Balance] = 0;
    p.raw[Filter_Keytrack_Base] = 60;
    p.raw[Filter_Env_Sustain] = 127;
    p.raw[Amp_Env_Attack] = 0;
    p.raw[Amp_Env_Decay] = 0;
    p.raw[Amp_Env_Sustain] = 127;
    p.raw[Amp_Env_Sustain_Time] = 64;    // hold (see Tables.h sustainSlopeSec)
    p.raw[Filter_Env_Sustain_Time] = 64;
    p.raw[Amp_Env_Release] = 0;
    p.raw[Patch_Volume] = 100;
    p.raw[Panorama] = 64;
    p.raw[Lfo1_Rate] = 64;
    p.raw[Lfo2_Rate] = 64;
    p.raw[Lfo3_Rate] = 64;
    p.raw[Clock_Tempo] = 57;             // 120 BPM
    p.raw[Bender_Range_Up] = 66;
    p.raw[Bender_Range_Down] = 62;
    p.raw[Bender_Scale] = 1;
    p.raw[Filter1_Env_Polarity] = 1;
    p.raw[Filter2_Env_Polarity] = 1;
    p.raw[Filter2_Cutoff_Link] = 0;
    p.raw[Unison_Detune] = 48;
    p.raw[Unison_Pan_Spread] = 127;
    p.raw[Delay_Reverb_Mode] = 0;
    p.raw[Effect_Send] = 0;
    p.raw[Delay_Color] = 64;
    p.raw[Chorus_Mix] = 0;
    p.raw[Phaser_Mode] = 0;
    p.raw[Phaser_Mix] = 0;
    p.raw[LowEQ_Gain] = 64;
    p.raw[MidEQ_Gain] = 64;
    p.raw[HighEQ_Gain] = 64;
    p.raw[LowEQ_Frequency] = 43;
    p.raw[HighEQ_Frequency] = 85;
    p.raw[MidEQ_Frequency] = 71;
    p.raw[MidEQ_Q_Factor] = 51;
    p.raw[Bass_Tune] = 32;
    p.raw[Arp_Note_Length] = 64;
    p.raw[Arp_Clock] = 4;
    p.raw[Osc3_Detune] = 0;
    p.raw[Osc2_Detune] = 0;             // neutral (the CSV default is 32)
    p.raw[Osc3_Volume] = 64;
    p.raw[Category1] = 0;
    p.raw[Category2] = 0;
    p.raw[Control_Smooth_Mode] = 1;
    p.setName ("Init");
    return p;
}

//==============================================================================
std::string valueToText (int offset, int value)
{
    if (offset < 0 || offset >= kSingleSize) return std::to_string (value);
    const auto& d = layout::kParams[offset];
    if (d.toText[0] != 0)
        if (auto* vl = layout::findValueList (d.toText))
            if (value >= 0 && value < vl->count)
                return vl->items[value];
    return std::to_string (value);
}

namespace
{
    std::string jsonEscape (const std::string& s)
    {
        std::string o;
        for (char c : s)
        {
            if (c == '"' || c == '\\') { o += '\\'; o += c; }
            else if ((unsigned char) c < 32) { char b[8]; std::snprintf (b, sizeof b, "\\u%04x", c); o += b; }
            else o += c;
        }
        return o;
    }
}

std::string patchToJson (const SinglePatch& p)
{
    std::ostringstream s;
    s << "{\n  \"name\": \"" << jsonEscape (p.name()) << "\",\n"
      << "  \"bank\": " << p.bank << ",\n  \"program\": " << p.program << ",\n"
      << "  \"version\": " << p.version() << ",\n"
      << "  \"category1\": \"" << jsonEscape (valueToText (251, p.value (251))) << "\",\n"
      << "  \"category2\": \"" << jsonEscape (valueToText (252, p.value (252))) << "\",\n"
      << "  \"checksumOk\": " << (p.checksumOk ? "true" : "false") << ",\n"
      << "  \"extraBytes\": " << p.extra.size() << ",\n"
      << "  \"params\": [\n";
    for (int i = 0; i < kSingleSize; ++i)
    {
        const auto& d = layout::kParams[i];
        s << "    { \"offset\": " << i << ", \"name\": \"" << jsonEscape (d.name) << "\", \"raw\": " << (int) p.raw[(size_t) i]
          << ", \"value\": " << p.value (i) << ", \"text\": \"" << jsonEscape (valueToText (i, p.value (i))) << "\""
          << (p.isOutOfRange (i) ? ", \"outOfRange\": true" : "") << " }" << (i < kSingleSize - 1 ? "," : "") << "\n";
    }
    s << "  ]\n}\n";
    return s.str();
}

std::vector<int> ignoredParametersInUse (const SinglePatch& p)
{
    using namespace layout::off;
    std::vector<int> out;
    for (int o : { Input_Mode, Input_Ringmodulator, Vocoder_Mode, Input_Follower_Mode, Second_Output_Balance })
        if (p.raw[(size_t) o] != 0)
            out.push_back (o);
    if (p.raw[Input_Mode] != 0 || p.raw[Osc_FM_Mode] >= 4)
        out.push_back (Input_Select);
    if (p.raw[Osc_FM_Mode] >= 4 && p.raw[Osc_FM_Mode] <= 12)
        out.push_back (Osc_FM_Mode);   // external-input FM modes are no-ops in v1
    std::sort (out.begin(), out.end());
    out.erase (std::unique (out.begin(), out.end()), out.end());
    return out;
}
} // namespace vc
